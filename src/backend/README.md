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

---

## Migrações

```bash
alembic revision --autogenerate -m "descricao"
alembic upgrade head
```

Requer `DATABASE_URL` configurado (local ou `.env`).

---

## Ingestão de telemetria (BACK-02)

A ponte serial envia cada linha recebida do robô, sem alterar, para `POST /telemetria`:

```json
{"linha": "{\"v\":1,\"boot\":7,\"seq\":42,\"t_ms\":11250,\"tipo\":\"tel\",...}", "recebido_em": "2026-10-08T12:00:00+00:00"}
```

A resposta traz os comandos para o robô: `{"comandos": []}`. O formato das mensagens é o contrato v1 (`contrato/telemetria.py`, exemplos em `src/contrato/exemplos.jsonl`).

- **422**: linha inválida (mais de 256 B, não é JSON, `v` ≠ 1, `tipo` desconhecido, campo faltando ou fora da faixa, `falha` incoerente ou célula fora do labirinto da execução). Nada é gravado e o motivo vai para o log (WARNING, logger `app.telemetria`). **A ponte não deve reenviar respostas 4xx**: a mesma linha vai falhar de novo.
- **200**: mensagem aceita e gravada na tentativa aberta (`health-check` ou `running`). Também devolve 200, sem gravar e com INFO no log, a mensagem sem tentativa aberta, o evento repetido e o `hc_item` fora do health-check.
- **Deduplicação**: eventos (`hc_item`, `hc_resultado`, `passo`, `falha`, `sucesso`) com `seq` menor ou igual ao último aceito no mesmo `boot` são repetidos (o firmware reenvia o buffer ao reconectar). O `seq` só é marcado como visto depois do commit; se a gravação falhar, o reenvio é aceito. A `tel` nunca é repetida.

### Teste de carga (RNF-B11)

O pytest `tests/integracao/test_carga_telemetria.py` envia 600 mensagens sem espera e verifica p95 < 100 ms sem crescimento. Para medir a API no ar, use o script, que manda `tel` no ritmo pedido e sai com código 1 se p95 ≥ 100 ms ou se o atraso acumulado passar de 1 s:

```bash
cd src/backend
python scripts/carga_telemetria.py --url http://localhost:8000 --taxa 10 --duracao 60
```

O script precisa de uma tentativa aberta; sem ela, as mensagens são descartadas. Para criar uma no banco de desenvolvimento:

```bash
docker compose exec db psql -U micromouse -d micromouse -c "
INSERT INTO execucao_logica (execucao_id, labirinto_id, status, tentativas_usadas, iniciada_em)
VALUES ('00000000-0000-0000-0000-00000000c4a6', 1, 'em_andamento', 1, now());
INSERT INTO tentativa (tentativa_id, execucao_id, attempt_index, status, tipo_inicio, iniciada_em)
VALUES (gen_random_uuid(), '00000000-0000-0000-0000-00000000c4a6', 1, 'running', 'nova', now());"
```

Depois da medição, encerre a tentativa (`UPDATE tentativa SET status = 'failed' WHERE execucao_id = '00000000-0000-0000-0000-00000000c4a6';`) para ela não receber a telemetria real.

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
| POST /telemetria (4.4.1) | `contrato/telemetria.py` (`EntradaPonte`, `RespostaPonte`) |
| Schemas REST / SSE (ARQ-02) | `app/schemas/api.py` |
