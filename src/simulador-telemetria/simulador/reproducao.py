"""Reprodução de uma gravação: um .jsonl com as mensagens do contrato já prontas.

É o que o simulador de navegação do firmware (FIRM-02) grava com `--telemetria`. Ao
contrário do roteiro, não há cenário a gerar: cada linha sai como está, só com o `boot`
trocado pelo da execução, para o Deduplicador do backend não descartar uma segunda
reprodução do mesmo arquivo.
"""

from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path

from contrato.telemetria import LinhaInvalida, Mensagem, ler_linha

from simulador.roteiro import RoteiroInvalido


@dataclass(frozen=True)
class Gravacao:
    """Mensagens de um .jsonl, na ordem do arquivo."""

    nome: str
    mensagens: list[Mensagem]

    @property
    def boots(self) -> int:
        """Quantos boots diferentes o arquivo tem; cada um vira um boot novo."""
        return len({mensagem.boot for mensagem in self.mensagens})


def carregar_gravacao(caminho: str | Path) -> Gravacao:
    """Lê o .jsonl e valida cada linha com o contrato; linhas em branco são ignoradas."""
    caminho = Path(caminho)
    try:
        texto = caminho.read_text(encoding="utf-8")
    except OSError as erro:
        raise RoteiroInvalido(f"{caminho}: não foi possível ler ({erro.strerror})") from erro

    mensagens = []
    for numero, linha in enumerate(texto.splitlines(), start=1):
        if not linha.strip():
            continue
        try:
            mensagens.append(ler_linha(linha))
        except LinhaInvalida as erro:
            raise RoteiroInvalido(f"{caminho}: linha {numero} fora do contrato\n  {erro}") from erro
    if not mensagens:
        raise RoteiroInvalido(f"{caminho}: nenhuma mensagem")
    return Gravacao(nome=caminho.stem, mensagens=mensagens)


def reproduzir(gravacao: Gravacao, primeiro_boot: int) -> Iterator[tuple[int, Mensagem]]:
    """Linha do tempo para o emissor: (t simulado, mensagem com o boot da execução).

    O instante vem do `t_ms`. Num boot novo o `t_ms` recomeça, então o instante continua de
    onde o boot anterior parou. Nunca volta: um evento reenviado depois de uma queda, com
    `t_ms` antigo, sai logo depois do anterior.
    """
    boots: dict[int, int] = {}
    inicio_ms = agora_ms = 0
    for mensagem in gravacao.mensagens:
        if mensagem.boot not in boots:
            boots[mensagem.boot] = primeiro_boot + len(boots)
            inicio_ms = agora_ms
        agora_ms = max(agora_ms, inicio_ms + mensagem.t_ms)
        yield agora_ms, mensagem.model_copy(update={"boot": boots[mensagem.boot]})
