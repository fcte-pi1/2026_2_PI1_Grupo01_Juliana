"""Histórico e detalhe de execuções no formato do contrato (BACK-06)."""

from decimal import Decimal
from uuid import UUID

from app import models
from app.repositories import ExecucaoRepository, FiltroExecucoes
from app.schemas import api

# Bits do paredes_mask, iguais ao openapi.yaml.
NORTE, SUL, LESTE, OESTE = 1, 2, 4, 8
TAMANHO_CANTO = 4
STATUS_ENCERRADOS = ("success", "failed")


def listar_historico(
    repo: ExecucaoRepository,
    filtro: FiltroExecucoes,
    *,
    limite: int,
    offset: int,
) -> tuple[list[api.ResumoExecucao], int]:
    execucoes, total = repo.listar_pagina(filtro, limite=limite, offset=offset)
    return [_resumo(execucao) for execucao in execucoes], total


def buscar_detalhe(repo: ExecucaoRepository, execucao_id: UUID) -> api.ExecucaoDetalhe | None:
    execucao = repo.buscar_detalhe(execucao_id)
    if execucao is None:
        return None

    tentativas = execucao.tentativas
    ultima = tentativas[-1] if tentativas else None
    leituras = repo.listar_leituras(ultima.tentativa_id) if ultima else []
    paredes = {(p.x, p.y): _mascara(p) for p in execucao.paredes}

    return api.ExecucaoDetalhe(
        execucao_id=execucao.execucao_id,
        tipo_labirinto=execucao.labirinto.tipo,
        status=execucao.status,
        tentativas_usadas=execucao.tentativas_usadas,
        iniciada_em=execucao.iniciada_em,
        encerrada_em=execucao.encerrada_em,
        tempo_total_s=execucao.tempo_total_s,
        variacao_melhor_tempo_pct=_variacao_melhor_tempo(repo, execucao),
        eixo_longo=_eixo_longo(execucao),
        ultima_mensagem_em=repo.ultima_mensagem_em(execucao.execucao_id),
        tentativas=[_tentativa(t, execucao.passos_trajeto) for t in tentativas],
        trajeto=[_passo(p, paredes) for p in execucao.passos_trajeto],
        leituras=[_leitura(lt) for lt in leituras if _leitura_completa(lt)],
    )


def _resumo(execucao: models.ExecucaoLogica) -> api.ResumoExecucao:
    tentativas = sorted(execucao.tentativas, key=lambda t: t.attempt_index)
    ultima = tentativas[-1] if tentativas else None
    encerradas = [t for t in tentativas if t.status in STATUS_ENCERRADOS]
    return api.ResumoExecucao(
        execucao_id=execucao.execucao_id,
        tipo_labirinto=execucao.labirinto.tipo,
        status=execucao.status,
        tentativas_usadas=execucao.tentativas_usadas,
        iniciada_em=execucao.iniciada_em,
        encerrada_em=execucao.encerrada_em,
        tempo_total_s=execucao.tempo_total_s,
        velocidade_media=ultima.velocidade_media if ultima else None,
        consumo_bateria=_consumo(encerradas[-1]) if encerradas else None,
    )


def _consumo(tentativa: models.Tentativa) -> Decimal | None:
    if tentativa.consumo_bateria is not None:
        return tentativa.consumo_bateria
    if tentativa.bateria_inicial is None or tentativa.bateria_final is None:
        return None
    return tentativa.bateria_inicial - tentativa.bateria_final


def _variacao_melhor_tempo(
    repo: ExecucaoRepository, execucao: models.ExecucaoLogica
) -> float | None:
    tempos = [
        t.tempo_s for t in execucao.tentativas if t.status == "success" and t.tempo_s is not None
    ]
    if not tempos:
        return None
    melhor = repo.melhor_tempo_anterior(execucao)
    if not melhor:
        return None
    return round(float((min(tempos) - melhor) / melhor * 100), 1)


def _eixo_longo(execucao: models.ExecucaoLogica) -> api.EixoLongo | None:
    if execucao.labirinto.tipo == "4x4":
        return None
    for passo in execucao.passos_trajeto:
        if passo.x >= TAMANHO_CANTO:
            return "x"
        if passo.y >= TAMANHO_CANTO:
            return "y"
    return None


def _mascara(parede: models.ParedeCelula) -> int:
    return (
        (NORTE if parede.norte else 0)
        | (SUL if parede.sul else 0)
        | (LESTE if parede.leste else 0)
        | (OESTE if parede.oeste else 0)
    )


def _passo(passo: models.PassoTrajeto, paredes: dict[tuple[int, int], int]) -> api.PassoTrajeto:
    mascara = passo.paredes_mask
    if mascara is None:
        mascara = paredes.get((passo.x, passo.y), 0)
    return api.PassoTrajeto(
        passo_id=passo.passo_id,
        seq=passo.seq,
        x=passo.x,
        y=passo.y,
        paredes_mask=mascara,
        retomada=passo.retomada,
        entrou_em=passo.entrou_em,
    )


def _tentativa(tentativa: models.Tentativa, passos: list[models.PassoTrajeto]) -> api.Tentativa:
    return api.Tentativa(
        tentativa_id=tentativa.tentativa_id,
        attempt_index=tentativa.attempt_index,
        status=tentativa.status,
        tipo_inicio=tentativa.tipo_inicio,
        tipo_dip=tentativa.tipo_dip,
        iniciada_em=tentativa.iniciada_em,
        encerrada_em=tentativa.encerrada_em,
        tempo_s=tentativa.tempo_s,
        velocidade_media=tentativa.velocidade_media,
        bateria_inicial=tentativa.bateria_inicial,
        bateria_final=tentativa.bateria_final,
        consumo_bateria=_consumo(tentativa),
        health_check=[
            api.ItemHealthCheck(
                componente=item.componente,
                aprovado=item.aprovado,
                valor_lido=item.valor_lido,
            )
            for item in sorted(tentativa.health_check_itens, key=lambda i: i.verificado_em)
        ],
        falha=_falha(tentativa, passos) if tentativa.falha else None,
    )


def _falha(tentativa: models.Tentativa, passos: list[models.PassoTrajeto]) -> api.Falha:
    falha = tentativa.falha
    x, y = falha.celula_x, falha.celula_y
    if x is None or y is None:
        # Sem célula gravada (ex.: health-check reprovado): usa a última célula
        # da tentativa ou a largada.
        da_tentativa = [p for p in passos if p.tentativa_id == tentativa.tentativa_id]
        ultimo = da_tentativa[-1] if da_tentativa else None
        x, y = (ultimo.x, ultimo.y) if ultimo else (0, 0)
    return api.Falha(
        motivo=falha.motivo,
        origem=falha.origem,
        celula_x=x,
        celula_y=y,
        componente=falha.componente,
        observacao=falha.observacao,
        momento_falha=falha.momento_falha,
    )


def _leitura_completa(leitura: models.LeituraTelemetria) -> bool:
    return leitura.x is not None and leitura.y is not None and leitura.bateria is not None


def _leitura(leitura: models.LeituraTelemetria) -> api.LeituraTelemetria:
    return api.LeituraTelemetria(
        ordem=leitura.ordem,
        x=leitura.x,
        y=leitura.y,
        bateria=leitura.bateria,
        velocidade=leitura.velocidade,
        rumo=None,
        enviado_em=leitura.enviado_em or leitura.recebido_em,
    )
