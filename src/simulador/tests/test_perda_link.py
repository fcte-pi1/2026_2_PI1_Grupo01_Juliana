"""Testes da perda de link: silêncio na queda, buffer de 64 e reenvio na volta."""

from contrato.telemetria import Deduplicador, Falha, Tel, ler_linha

from simulador.emissor import emitir
from simulador.gerador import (
    DURACAO_HC_MS,
    INTERVALO_REENVIO_MS,
    TAMANHO_BUFFER,
    LinhaCrua,
    gerar,
)
from simulador.interrupcao import Interrupcao
from simulador.roteiro import Roteiro

S_POR_CELULA = 0.8
CELULA_MS = round(S_POR_CELULA * 1000)
INTERROMPER = '{"v":1,"cmd":"interromper"}'


def _roteiro(celulas: int = 8, quedas=(), fim="sucesso") -> Roteiro:
    """Vai e volta entre (0, 0) e (0, 1); cada queda é (na_celula, segundos)."""
    ida_e_volta = [{"x": 0, "y": 0, "paredes": 14}, {"x": 0, "y": 1, "paredes": 13}]
    tentativa = {
        "inicio": "nova",
        "celulas": [ida_e_volta[i % 2] for i in range(celulas)],
        "eventos": [
            {"na_celula": celula, "tipo": "perda_link", "segundos": segundos}
            for celula, segundos in quedas
        ],
        "fim": {"tipo": fim},
    }
    return Roteiro.model_validate(
        {
            "nome": "teste",
            "labirinto": "4x4",
            "bateria_inicial_mv": 7900,
            "s_por_celula": S_POR_CELULA,
            "tentativas": [tentativa],
        }
    )


def _t_celula(i: int) -> int:
    return DURACAO_HC_MS + i * CELULA_MS


def _eventos(itens) -> list:
    return [item for _, item in itens if not isinstance(item, Tel | LinhaCrua)]


def test_sem_queda_nada_muda():
    itens = list(gerar(_roteiro()))

    assert [t for t, _ in itens] == sorted(t for t, _ in itens)
    assert [m.seq for m in _eventos(itens)] == sorted(m.seq for m in _eventos(itens))


def test_nada_sai_durante_a_queda_e_a_tel_nao_e_reenviada():
    inicio, fim = _t_celula(2), _t_celula(2) + 3000
    itens = list(gerar(_roteiro(quedas=[(2, 3)])))
    tels = [m for _, m in itens if isinstance(m, Tel)]

    assert not [t for t, _ in itens if inicio <= t < fim]
    assert not [m for m in tels if inicio <= m.t_ms < fim]
    assert len({m.seq for m in tels}) == len(tels)  # nenhuma tel repetida
    # a tel volta na hora, junto com o reenvio
    assert min(m.t_ms for m in tels if m.t_ms >= fim) == fim


def test_a_corrida_continua_durante_a_queda():
    sem_queda = _eventos(gerar(_roteiro()))
    com_queda = _eventos(gerar(_roteiro(quedas=[(2, 3)])))

    # os passos da queda foram gerados (com o mesmo seq) e chegam pelo reenvio
    assert {m.seq for m in com_queda} == {m.seq for m in sem_queda}
    assert com_queda[-1].tipo == "sucesso"


def test_reenvia_o_buffer_inteiro_em_ordem_a_20_por_segundo():
    fim = _t_celula(2) + 3000
    itens = list(gerar(_roteiro(quedas=[(2, 3)])))
    antes = _eventos((t, m) for t, m in itens if t < _t_celula(2))
    reenvio = [(t, m) for t, m in itens if t >= fim and not isinstance(m, Tel)]
    gerados = len(antes) + 4  # passos das células 2 a 5, gerados até a volta (5 = fim)

    eventos = [m for _, m in reenvio[:gerados]]
    assert [m.seq for m in eventos] == sorted(m.seq for m in eventos)
    assert eventos[: len(antes)] == antes  # do mais antigo, inclusive os já enviados
    tempos = [t for t, _ in reenvio[:gerados]]
    assert tempos[0] == fim
    assert all(b - a == INTERVALO_REENVIO_MS for a, b in zip(tempos, tempos[1:]))
    # o que foi gerado durante o reenvio sai depois dele, sem buraco de seq
    novos = [m for _, m in reenvio[gerados:]]
    assert novos and novos[0].seq > eventos[-1].seq


def test_buffer_cheio_descarta_o_mais_antigo():
    celulas = TAMANHO_BUFFER + 20
    itens = list(gerar(_roteiro(celulas=celulas, quedas=[(1, celulas * S_POR_CELULA)])))
    fim = _t_celula(1) + round(celulas * S_POR_CELULA * 1000)
    reenvio = _eventos((t, m) for t, m in itens if t >= fim)
    todos = _eventos(gerar(_roteiro(celulas=celulas)))

    assert len(reenvio) == TAMANHO_BUFFER
    assert reenvio == todos[-TAMANHO_BUFFER:]


def test_deduplicador_aceita_cada_evento_uma_vez_e_em_ordem():
    roteiro = _roteiro(celulas=12, quedas=[(2, 3), (7, 2)])
    dedup = Deduplicador()
    aceitos = []
    for _, item in gerar(roteiro):
        mensagem = ler_linha(item.model_dump_json() + "\n")
        if dedup.eh_nova(mensagem) and not isinstance(mensagem, Tel):
            aceitos.append(mensagem)
    repetidos = len(_eventos(gerar(roteiro))) - len(aceitos)

    assert [m.seq for m in aceitos] == [m.seq for m in _eventos(gerar(_roteiro(celulas=12)))]
    assert repetidos > 0  # houve reenvio de verdade


def test_queda_que_passa_do_fim_reenvia_depois_e_atrasa_o_proximo_boot():
    # queda de 10 s começando na penúltima célula: o fim e as tel finais caem nela
    itens = list(gerar(_roteiro(quedas=[(6, 10)])))
    volta = _t_celula(6) + 10_000

    assert _eventos(itens)[-1].tipo == "sucesso"
    assert [t for t, m in itens if not isinstance(m, Tel)][-1] >= volta
    assert all(t < _t_celula(6) or t >= volta for t, _ in itens)


def test_interromper_depois_da_queda_numera_a_falha_depois_de_tudo():
    interrupcao = Interrupcao()
    saida = []
    volta = _t_celula(2) + 3000

    def escrever(linha: str) -> None:
        mensagem = ler_linha(linha)
        saida.append(mensagem)
        if isinstance(mensagem, Tel) and mensagem.t_ms == volta:
            interrupcao.ao_receber(INTERROMPER)  # chega no meio do reenvio

    emitir(gerar(_roteiro(quedas=[(2, 3)])), escrever, sem_espera=True, interrupcao=interrupcao)
    falha = next(m for m in saida if isinstance(m, Falha))
    tel = next(m for m in saida if isinstance(m, Tel) and m.t_ms == volta)

    assert falha.origem == "web"
    assert falha.seq == max(m.seq for m in saida if m is not falha and m.t_ms <= falha.t_ms) + 1
    assert (falha.x, falha.y) == (tel.x, tel.y)
    assert falha.t_ms == volta + INTERVALO_REENVIO_MS
