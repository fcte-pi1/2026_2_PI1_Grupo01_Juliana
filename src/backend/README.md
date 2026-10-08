# Backend — Sistema de Telemetria Micromouse

API **FastAPI** + **PostgreSQL** (MVC: rotas → serviços → repositórios/modelos).

## Pré-requisitos

- **Python 3.11+** (3.12 recomendado; igual ao CI)
- **Docker** e **Docker Compose** (para Postgres e/ou stack completa)
- Porta **5432** livre para o Postgres; porta **8000** livre para a API (se outro serviço, ex. MkDocs, usar 8000, altere o mapeamento no `docker-compose.yml`)

---

## Rodar local (API no host, Postgres no Docker)

Fluxo usual para desenvolver com hot reload, sem rebuild de imagem a cada mudança.

### 1. Entrar na pasta e criar o ambiente Python

```bash
cd src/backend
python3 -m venv .venv
source .venv/bin/activate   # Windows: .venv\Scripts\activate
pip install -e ".[dev]"
```

### 2. Subir só o banco

```bash
docker compose up -d db
```

Aguarde o healthcheck (`docker compose ps` deve mostrar `healthy`).

### 3. Variáveis de ambiente

```bash
cp .env.example .env
```

No `.env` (ou export no shell), use host **`localhost`** — não `db`, que só existe dentro da rede Docker:

```bash
export DATABASE_URL=postgresql+psycopg://micromouse:micromouse@localhost:5432/micromouse
```

Opcional: `TEMPO_MAXIMO_EXECUCAO_S` e `LIMITE_SEM_SINAL_S` (valores padrão em `.env.example`).

### 4. Aplicar migrações

Com o venv ativo e `DATABASE_URL` apontando para `micromouse`:

```bash
alembic upgrade head
```

### 5. Subir a API

```bash
uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
```

- Documentação interativa: http://localhost:8000/docs  
- Health (API + banco): http://localhost:8000/back-health  

Para parar o Postgres: `docker compose stop db` (dados persistem no volume `pgdata`).

---

## Testes e lint (igual ao CI)

O **pytest** recria o schema do banco de testes (`DROP SCHEMA public CASCADE`). Use um banco **separado** do de desenvolvimento.

### Banco de testes

Com o serviço `db` rodando, crie `micromouse_test` uma vez:

```bash
docker compose exec db psql -U micromouse -d micromouse -c "CREATE DATABASE micromouse_test;"
```

(Se o banco já existir, ignore o erro.)

### Comandos

```bash
cd src/backend
source .venv/bin/activate
export TEST_DATABASE_URL=postgresql+psycopg://micromouse:micromouse@localhost:5432/micromouse_test

ruff check .
pytest -q          # resumo; use pytest -v para mais detalhe
```

---

## Rodar tudo no Docker (API + Postgres)

Útil para validar o `Dockerfile` e o fluxo `alembic upgrade` na subida do container.

```bash
cd src/backend
cp .env.example .env   # opcional; o compose já define variáveis na API
docker compose up --build
```

- API: http://localhost:8000/docs  
- Back-health: http://localhost:8000/back-health  

Encerrar: `docker compose down` — apagar volume do Postgres: `docker compose down -v`.

### Persistência após restart (RNF-B05 · BACK-01)

Critério de aceite: execuções gravadas continuam consultáveis depois de reiniciar o servidor/banco. No compose, os dados ficam no volume nomeado **`pgdata`** (`docker-compose.yml`).

Verificação automatizada (Postgres no Docker + migrações aplicadas):

```bash
cd src/backend
chmod +x scripts/verificar_rnf_b05_persistencia.sh   # uma vez
./scripts/verificar_rnf_b05_persistencia.sh
```

O script: sobe `db`, aplica `alembic upgrade head` (se existir `.venv`), insere uma `execucao_logica` marcador, executa `docker compose restart db` e confere que o registro e os 3 labirintos do seed ainda existem.

**Evidência no PR:** colar a linha final `OK: RNF-B05 — dados persistiram...` ou anexar screenshot do terminal.

Reiniciar a **API** (`docker compose restart api`) não apaga dados — só o Postgres persiste execuções; reiniciar `db` é o teste que valida o volume.

---

## Migrações

```bash
alembic revision --autogenerate -m "descricao"
alembic upgrade head
```

Requer `DATABASE_URL` configurado (local ou `.env`).

---

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
