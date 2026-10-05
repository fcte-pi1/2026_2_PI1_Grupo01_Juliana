# Backend — Sistema de Telemetria Micromouse

API **FastAPI** + **PostgreSQL** (MVC: rotas → serviços → repositórios/modelos).

## Subir o ambiente

```bash
cd src/backend
cp .env.example .env   # opcional; docker-compose já define variáveis
docker compose up --build
```

- API: http://localhost:8000/docs  
- Back-health (API + banco): http://localhost:8000/back-health  

## Onde implementar cada tarefa (BACK-01 … BACK-08)

| Tarefa | Pasta / arquivo |
|--------|------------------|
| BACK-01 Persistência / consultas | `app/repositories/`, `app/models/` |
| BACK-03 Máquina de estados | `app/services/gerenciador_execucoes.py` |
| BACK-04 Métricas | `app/services/calculo_metricas.py` |
| BACK-05 Link / tempo | `app/services/monitor_conexao.py` (limites em `app/config.py`) |
| BACK-06 SQL específico | `app/repositories/` |
| BACK-07 SSE | `app/services/publicador_sse.py`, `app/routers/stream.py` |
| BACK-08 Ponte serial | `ponte/` (processo separado) |
| HTTP / contrato ARQ-02 | `app/routers/` (501 até implementar) |
| Schemas ARQ-01 / ARQ-02 | `app/schemas/telemetria.py`, `app/schemas/api.py` |

## Desenvolvimento local

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -e ".[dev]"
export DATABASE_URL=postgresql+psycopg://micromouse:micromouse@localhost:5432/micromouse
alembic upgrade head
uvicorn app.main:app --reload
pytest
ruff check .
```

## Migrações

```bash
alembic revision --autogenerate -m "descricao"
alembic upgrade head
```
