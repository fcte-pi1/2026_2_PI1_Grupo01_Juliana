"""Roteiro do simulador: o cenário de alto nível que vira mensagens do contrato.

O formato está na seção 6.1 de `tasks/prd-simulador-telemetria.md`. As paredes
seguem a máscara do contrato (N = 1, S = 2, L = 4, O = 8).
"""

from pathlib import Path
from typing import Annotated, Literal

from contrato.telemetria import Componente, Coordenada
from pydantic import BaseModel, ConfigDict, Field, ValidationError, model_validator

MotivoAutomatico = Literal["collision", "stuck", "falha_componente", "low_battery"]


class RoteiroInvalido(ValueError):
    """Roteiro com erro; a mensagem traz o caminho de cada campo errado."""


class _Modelo(BaseModel):
    model_config = ConfigDict(extra="forbid", frozen=True)


class Celula(_Modelo):
    x: Coordenada
    y: Coordenada
    paredes: Annotated[int, Field(ge=0, le=15)]


class _Evento(_Modelo):
    na_celula: Annotated[int, Field(ge=0)]  # índice em celulas


class PerdaLink(_Evento):
    tipo: Literal["perda_link"]
    segundos: Annotated[float, Field(gt=0)]


class LinhaCrua(_Evento):
    tipo: Literal["linha_crua"]
    linha: str  # sai exatamente assim, mesmo que não seja válida


class ParadaBoot(_Evento):
    tipo: Literal["parada_boot"]


Evento = Annotated[PerdaLink | LinhaCrua | ParadaBoot, Field(discriminator="tipo")]


class FimSucesso(_Modelo):
    tipo: Literal["sucesso"]


class FimFalha(_Modelo):
    tipo: Literal["falha"]
    motivo: MotivoAutomatico
    componente: Componente | None = None

    @model_validator(mode="after")
    def _componente_so_com_falha_componente(self) -> "FimFalha":
        if (self.motivo == "falha_componente") != (self.componente is not None):
            raise ValueError("componente vai só com falha_componente")
        return self


class FimNenhum(_Modelo):
    tipo: Literal["nenhum"]


Fim = Annotated[FimSucesso | FimFalha | FimNenhum, Field(discriminator="tipo")]


class Tentativa(_Modelo):
    inicio: Literal["nova", "retomada"] | None = None  # None: o gerador decide
    pausa_s: Annotated[float, Field(ge=0)] = 0
    hc_reprovados: list[Componente] = []
    celulas: Annotated[list[Celula], Field(min_length=1)]
    eventos: list[Evento] = []
    fim: Fim

    @model_validator(mode="after")
    def _caminho_coerente(self) -> "Tentativa":
        for i, (anterior, atual) in enumerate(zip(self.celulas, self.celulas[1:]), start=1):
            if abs(atual.x - anterior.x) + abs(atual.y - anterior.y) != 1:
                raise ValueError(
                    f"celulas[{i}] ({atual.x}, {atual.y}) não é vizinha de "
                    f"celulas[{i - 1}] ({anterior.x}, {anterior.y})"
                )
        for i, evento in enumerate(self.eventos):
            if evento.na_celula >= len(self.celulas):
                raise ValueError(
                    f"eventos[{i}].na_celula = {evento.na_celula} fora da lista de "
                    f"{len(self.celulas)} células"
                )
        return self


class Roteiro(_Modelo):
    nome: str
    labirinto: Literal["4x4", "8x4", "12x4", "invalido"]  # vira o tipo_dip
    bateria_inicial_mv: Annotated[int, Field(ge=0, le=10_000)]
    s_por_celula: Annotated[float, Field(gt=0)]
    tentativas: Annotated[list[Tentativa], Field(min_length=1)]


# Rótulos das uniões discriminadas que o Pydantic põe no caminho do erro.
_ROTULOS = {"perda_link", "linha_crua", "parada_boot", "sucesso", "falha", "nenhum"}


def _caminho(loc: tuple[int | str, ...]) -> str:
    caminho = ""
    for parte in loc:
        if isinstance(parte, int):
            caminho += f"[{parte}]"
        elif parte not in _ROTULOS:
            caminho += f".{parte}" if caminho else parte
    return caminho or "(raiz)"


# Tradução das mensagens mais comuns do Pydantic; as outras saem como vêm.
_MENSAGENS = {
    "missing": "campo obrigatório",
    "extra_forbidden": "campo desconhecido",
    "greater_than": "deve ser maior que {gt}",
    "greater_than_equal": "deve ser no mínimo {ge}",
    "less_than_equal": "deve ser no máximo {le}",
    "literal_error": "deve ser {expected}",
    "union_tag_invalid": "tipo {tag} desconhecido; use {expected_tags}",
    "too_short": "precisa de pelo menos {min_length} item(ns)",
}


def _mensagem(erro: dict) -> str:
    modelo = _MENSAGENS.get(erro["type"])
    if modelo is None:
        return erro["msg"].removeprefix("Value error, ")
    return modelo.format(**erro.get("ctx", {})).replace("' or '", "' ou '")


def validar(texto: str | bytes, origem: str = "roteiro") -> Roteiro:
    """Valida o JSON de um roteiro; levanta RoteiroInvalido com o caminho dos campos."""
    try:
        return Roteiro.model_validate_json(texto)
    except ValidationError as erro:
        linhas = [f"{_caminho(e['loc'])}: {_mensagem(e)}" for e in erro.errors()]
        raise RoteiroInvalido(f"{origem}: roteiro inválido\n  " + "\n  ".join(linhas)) from erro


def carregar(caminho: str | Path) -> Roteiro:
    """Lê e valida o roteiro do arquivo."""
    try:
        texto = Path(caminho).read_bytes()
    except OSError as erro:
        raise RoteiroInvalido(f"{caminho}: não foi possível ler ({erro.strerror})") from erro
    return validar(texto, origem=str(caminho))
