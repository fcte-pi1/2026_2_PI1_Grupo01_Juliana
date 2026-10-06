"""Contrato de telemetria serial v1.

Implementa no backend o formato descrito em
`docs/4.4.1 - Contrato de telemetria.md`. Os exemplos de
`src/contrato/exemplos.jsonl` são a fonte única: os testes conferem que
este schema os aceita e os gera de volta byte a byte.
"""

from datetime import datetime, timedelta
from typing import Annotated, Literal, Union

from pydantic import (
    AwareDatetime,
    BaseModel,
    ConfigDict,
    Field,
    TypeAdapter,
    ValidationError,
    model_validator,
)

VERSAO = 1
TAMANHO_MAXIMO = 256  # bytes por linha, contando o "\n"

UInt32 = Annotated[int, Field(ge=0, le=2**32 - 1)]
Coordenada = Annotated[int, Field(ge=0, le=11)]  # referencial do robô, matriz 12 x 12

Componente = Literal[
    "bateria",
    "tof_frontal_esq",
    "tof_frontal_dir",
    "tof_esquerdo",
    "tof_direito",
    "motor_esquerdo",
    "motor_direito",
    "encoder_esquerdo",
    "encoder_direito",
]

MotivoFalha = Literal[
    "collision",
    "stuck",
    "falha_componente",
    "low_battery",
    "encerrado_operador",
]

MOTIVOS_AUTOMATICOS = {"collision", "stuck", "falha_componente", "low_battery"}


class LinhaInvalida(ValueError):
    """Linha descartada pela validação (RNF-B09)."""


class _Mensagem(BaseModel):
    """Campos comuns a todas as mensagens do robô."""

    model_config = ConfigDict(strict=True, frozen=True, extra="ignore")

    v: Literal[1]
    boot: UInt32
    seq: UInt32
    t_ms: UInt32


class Tel(_Mensagem):
    tipo: Literal["tel"]
    estado: Literal["health-check", "running", "success", "failed"]
    x: Coordenada
    y: Coordenada
    rumo: Literal["N", "S", "L", "O"]
    bat_mv: Annotated[int, Field(ge=0, le=10_000)]
    vel_mm_s: Annotated[int, Field(ge=0, le=2_000)]
    eixo_longo: Literal["x", "y"] | None


class HcItem(_Mensagem):
    tipo: Literal["hc_item"]
    componente: Componente
    aprovado: bool
    valor: int | None


class HcResultado(_Mensagem):
    tipo: Literal["hc_resultado"]
    aprovado: bool
    tipo_dip: Literal["4x4", "8x4", "12x4", "invalido"]
    inicio: Literal["nova", "retomada"]

    @model_validator(mode="after")
    def _dip_invalido_reprova(self) -> "HcResultado":
        if self.tipo_dip == "invalido" and self.aprovado:
            raise ValueError("DIP switch inválido não pode aprovar o health-check")
        return self


class Passo(_Mensagem):
    tipo: Literal["passo"]
    x: Coordenada
    y: Coordenada
    paredes: Annotated[int, Field(ge=0, le=15)]  # N = 1, S = 2, L = 4, O = 8


class Falha(_Mensagem):
    tipo: Literal["falha"]
    motivo: MotivoFalha | None
    origem: Literal["automatica", "web", "boot"]
    x: Coordenada
    y: Coordenada
    componente: Componente | None

    @model_validator(mode="after")
    def _motivo_combina_com_origem(self) -> "Falha":
        if self.origem == "web" and self.motivo is not None:
            raise ValueError("a confirmação da interrupção não traz motivo")
        if self.origem == "boot" and self.motivo != "encerrado_operador":
            raise ValueError("a parada pelo BOOT tem motivo encerrado_operador")
        if self.origem == "automatica" and self.motivo not in MOTIVOS_AUTOMATICOS:
            raise ValueError("motivo inválido para falha automática")
        if (self.motivo == "falha_componente") != (self.componente is not None):
            raise ValueError("componente vai só com falha_componente")
        return self


class Sucesso(_Mensagem):
    tipo: Literal["sucesso"]
    x: Coordenada
    y: Coordenada


Mensagem = Annotated[
    Union[Tel, HcItem, HcResultado, Passo, Falha, Sucesso],
    Field(discriminator="tipo"),
]

_leitor = TypeAdapter(Mensagem)


def ler_linha(linha: str | bytes) -> Mensagem:
    """Valida uma linha da serial e devolve a mensagem tipada.

    Levanta `LinhaInvalida` se a linha passar de 256 bytes, não for JSON,
    tiver outra versão ou violar qualquer campo do contrato.
    """
    bruta = linha.encode("utf-8") if isinstance(linha, str) else linha
    if len(bruta) > TAMANHO_MAXIMO:
        raise LinhaInvalida(f"linha com {len(bruta)} bytes (máximo {TAMANHO_MAXIMO})")
    try:
        return _leitor.validate_json(bruta.rstrip(b"\r\n"))
    except ValidationError as erro:
        raise LinhaInvalida(str(erro)) from erro


def escrever_linha(mensagem: Mensagem) -> str:
    """Gera a linha exatamente como o firmware a envia, com o "\\n"."""
    return mensagem.model_dump_json() + "\n"


class Deduplicador:
    """Descarta eventos reenviados depois de uma reconexão (RNF-B08).

    O firmware envia os eventos sempre em ordem de `seq`; ao reconectar,
    reenvia o buffer inteiro. Basta guardar, por `boot`, o maior `seq` de
    evento já aceito. A `tel` nunca é reenviada e passa direto.
    """

    def __init__(self) -> None:
        self._ultimo_evento: dict[int, int] = {}

    def eh_nova(self, mensagem: Mensagem) -> bool:
        if isinstance(mensagem, Tel):
            return True
        ultimo = self._ultimo_evento.get(mensagem.boot)
        if ultimo is not None and mensagem.seq <= ultimo:
            return False
        self._ultimo_evento[mensagem.boot] = mensagem.seq
        return True


class RelogioDoRobo:
    """Converte o `t_ms` do robô em hora real para o `enviado_em` (RF08).

    Na primeira mensagem de cada `boot`, fixa a âncora = recebido_em - t_ms.
    Depois, enviado_em = âncora + t_ms: o erro é a latência da primeira
    mensagem e os intervalos dentro do boot saem exatos.
    """

    def __init__(self) -> None:
        self._ancoras: dict[int, datetime] = {}

    def enviado_em(self, mensagem: Mensagem, recebido_em: datetime) -> datetime:
        ancora = self._ancoras.setdefault(
            mensagem.boot, recebido_em - timedelta(milliseconds=mensagem.t_ms)
        )
        return ancora + timedelta(milliseconds=mensagem.t_ms)


# --- Ponte <-> API (POST /telemetria) ---------------------------------------


class Comando(BaseModel):
    """Único comando da web para o robô (RF39, RNF06)."""

    model_config = ConfigDict(frozen=True)

    v: Literal[1] = VERSAO
    cmd: Literal["interromper"] = "interromper"


class EntradaPonte(BaseModel):
    """Corpo do POST /telemetria: a linha crua e o instante em que a ponte a leu."""

    linha: str
    recebido_em: AwareDatetime


class RespostaPonte(BaseModel):
    """Resposta do POST /telemetria: comandos para a ponte escrever na serial."""

    comandos: list[Comando] = []


__all__ = [
    "Comando",
    "Deduplicador",
    "EntradaPonte",
    "Falha",
    "HcItem",
    "HcResultado",
    "LinhaInvalida",
    "Mensagem",
    "Passo",
    "RelogioDoRobo",
    "RespostaPonte",
    "Sucesso",
    "TAMANHO_MAXIMO",
    "Tel",
    "VERSAO",
    "escrever_linha",
    "ler_linha",
]
