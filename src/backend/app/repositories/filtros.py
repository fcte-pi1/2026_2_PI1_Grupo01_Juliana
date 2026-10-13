from dataclasses import dataclass

from app.schemas.api import TipoLabirinto


@dataclass(frozen=True, slots=True)
class FiltroExecucoes:
    tipo_labirinto: TipoLabirinto | None = None
