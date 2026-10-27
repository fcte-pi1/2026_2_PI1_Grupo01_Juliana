"""Configuração da ponte, lida do ambiente e do `.env`."""

from pathlib import Path

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class ConfigPonte(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore", populate_by_name=True)

    api_url: str = Field("http://localhost:8000", validation_alias="API_URL")
    porta_serial: str = Field("/dev/rfcomm0", validation_alias="PONTE_PORTA_SERIAL")
    fila_maxima: int = Field(100, validation_alias="PONTE_FILA_MAXIMA")
    log_dir: Path = Field(Path("logs/ponte"), validation_alias="PONTE_LOG_DIR")
