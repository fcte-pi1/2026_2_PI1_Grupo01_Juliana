from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str = "postgresql+psycopg://micromouse:micromouse@db:5432/micromouse"
    tempo_maximo_execucao_s: int = 600
    limite_sem_sinal_s: int = 10


settings = Settings()
