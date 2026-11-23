"""Resumo da campanha e exportação das execuções (coleta de dados pelo front)."""

import csv
import io
from decimal import Decimal

from app import models
from app.schemas import api

LIMITE_TEMPO_S = 600  # todo percurso deve terminar em até 10 min (tempo_maximo_execucao_s)

COLUNAS_EXPORTACAO = (
    "numero",
    "execucao_id",
    "tentativa_id",
    "attempt_index",
    "tipo_labirinto",
    "configuracao",
    "observacao",
    "status_execucao",
    "status_tentativa",
    "tipo_inicio",
    "tipo_dip",
    "iniciada_em",
    "encerrada_em",
    "tempo_s",
    "tempo_total_s",
    "velocidade_media",
    "bateria_inicial",
    "bateria_final",
    "consumo_bateria_pct",
    "motivo_falha",
    "origem_falha",
    "celula_x",
    "celula_y",
    "componente",
    "observacao_falha",
    "passos",
)


def _numero(valor: Decimal | float | None) -> float | None:
    return None if valor is None else float(valor)


def _taxa(concluidas: int, encerradas: int) -> float | None:
    return round(100 * concluidas / encerradas, 1) if encerradas else None


def _media(valores: list[float]) -> float | None:
    return round(sum(valores) / len(valores), 2) if valores else None


def _grupo(
    execucoes: list[models.ExecucaoLogica], configuracao: str | None
) -> api.ResumoConfiguracao:
    concluidas = [e for e in execucoes if e.status == "concluida"]
    encerradas = [e for e in execucoes if e.status != "em_andamento"]
    tempos = [float(e.tempo_total_s) for e in concluidas if e.tempo_total_s is not None]
    return api.ResumoConfiguracao(
        configuracao=configuracao,
        total=len(execucoes),
        concluidas=len(concluidas),
        taxa_sucesso_pct=_taxa(len(concluidas), len(encerradas)),
        tempo_medio_s=_media(tempos),
        tempo_maximo_s=max(tempos) if tempos else None,
    )


def resumir(execucoes: list[models.ExecucaoLogica]) -> api.ResumoCampanha:
    """Taxa de sucesso e tempos. Sucesso é a execução concluída; a taxa conta as encerradas."""
    concluidas = [e for e in execucoes if e.status == "concluida"]
    canceladas = [e for e in execucoes if e.status == "cancelada"]
    em_andamento = [e for e in execucoes if e.status == "em_andamento"]
    tempos = [float(e.tempo_total_s) for e in concluidas if e.tempo_total_s is not None]

    por_configuracao: dict[str | None, list[models.ExecucaoLogica]] = {}
    for execucao in execucoes:
        por_configuracao.setdefault(execucao.configuracao, []).append(execucao)
    grupos = [
        _grupo(lista, configuracao)
        for configuracao, lista in sorted(
            por_configuracao.items(), key=lambda par: (par[0] is None, par[0] or "")
        )
    ]

    return api.ResumoCampanha(
        total=len(execucoes),
        concluidas=len(concluidas),
        canceladas=len(canceladas),
        em_andamento=len(em_andamento),
        taxa_sucesso_pct=_taxa(len(concluidas), len(concluidas) + len(canceladas)),
        tempo_medio_s=_media(tempos),
        tempo_maximo_s=max(tempos) if tempos else None,
        concluidas_abaixo_de_10_min=sum(1 for t in tempos if t < LIMITE_TEMPO_S),
        por_configuracao=grupos,
    )


def linhas_exportacao(execucoes: list[models.ExecucaoLogica]) -> list[dict[str, object]]:
    """Uma linha por tentativa, com os dados da execução repetidos, no formato da planilha."""
    linhas: list[dict[str, object]] = []
    for execucao in execucoes:
        passos = {}
        for passo in execucao.passos_trajeto:
            passos[passo.tentativa_id] = passos.get(passo.tentativa_id, 0) + 1
        for tentativa in execucao.tentativas:
            falha = tentativa.falha
            linhas.append(
                {
                    "numero": execucao.numero,
                    "execucao_id": str(execucao.execucao_id),
                    "tentativa_id": str(tentativa.tentativa_id),
                    "attempt_index": tentativa.attempt_index,
                    "tipo_labirinto": execucao.labirinto.tipo,
                    "configuracao": execucao.configuracao,
                    "observacao": execucao.observacao,
                    "status_execucao": execucao.status,
                    "status_tentativa": tentativa.status,
                    "tipo_inicio": tentativa.tipo_inicio,
                    "tipo_dip": tentativa.tipo_dip,
                    "iniciada_em": tentativa.iniciada_em.isoformat(),
                    "encerrada_em": tentativa.encerrada_em.isoformat()
                    if tentativa.encerrada_em
                    else None,
                    "tempo_s": _numero(tentativa.tempo_s),
                    "tempo_total_s": _numero(execucao.tempo_total_s),
                    "velocidade_media": _numero(tentativa.velocidade_media),
                    "bateria_inicial": _numero(tentativa.bateria_inicial),
                    "bateria_final": _numero(tentativa.bateria_final),
                    "consumo_bateria_pct": _numero(tentativa.consumo_bateria_pct),
                    "motivo_falha": falha.motivo if falha else None,
                    "origem_falha": falha.origem if falha else None,
                    "celula_x": falha.celula_x if falha else None,
                    "celula_y": falha.celula_y if falha else None,
                    "componente": falha.componente if falha else None,
                    "observacao_falha": falha.observacao if falha else None,
                    "passos": passos.get(tentativa.tentativa_id, 0),
                }
            )
    return linhas


def para_csv(linhas: list[dict[str, object]]) -> str:
    saida = io.StringIO()
    escritor = csv.DictWriter(saida, fieldnames=COLUNAS_EXPORTACAO, lineterminator="\n")
    escritor.writeheader()
    for linha in linhas:
        escritor.writerow({k: "" if v is None else v for k, v in linha.items()})
    return saida.getvalue()
