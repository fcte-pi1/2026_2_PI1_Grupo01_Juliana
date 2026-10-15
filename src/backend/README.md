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

### Validação BACK-03 (GerenciadorExecucoes)

Checklist da task (transições T/E/R, critérios de aceite, rotas de execução):

```bash
cd src/backend
source .venv/bin/activate
export TEST_DATABASE_URL=postgresql+psycopg://micromouse:micromouse@localhost:5432/micromouse_test

python -m scripts.validar_back03        # escopo completo
python -m scripts.validar_back03 -v     # saída detalhada
python -m scripts.validar_back03 --apenas-criterios
python -m scripts.validar_back03 --mostrar-mapa   # T01–R05 → nome do teste
```

Sai com código **0** se tudo passar, **1** se algum teste falhar.

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

- **422**: linha inválida (mais de 256 B, não é JSON, `v` diferente de 1, `tipo` desconhecido, campo faltando ou fora da faixa, `falha` incoerente ou célula fora do labirinto da execução). Nada é gravado e o motivo vai para o log (WARNING, logger `app.telemetria`). A resposta é `{"detail": "<motivo>", "comandos": [...]}`: **a ponte escreve os `comandos` na serial como no 200**, para não perder uma interrupção pendente. Se o próprio corpo `EntradaPonte` vier malformado, o 422 é o padrão do FastAPI (`detail` em lista, sem `comandos`). **A ponte não deve reenviar respostas 4xx**: a mesma linha vai falhar de novo.
- **200**: mensagem aceita e gravada na tentativa aberta (`health-check` ou `running`). Também devolve 200, sem gravar e com INFO no log, a mensagem sem tentativa aberta, o evento repetido e o `hc_item` fora do health-check.
- **Deduplicação**: eventos (`hc_item`, `hc_resultado`, `passo`, `falha`, `sucesso`) com `seq` menor ou igual ao último aceito no mesmo `boot` são repetidos (o firmware reenvia o buffer ao reconectar). O `seq` só é marcado como visto depois do commit; se a gravação falhar, o reenvio é aceito. A `tel` nunca é repetida.

### Teste de carga (RNF-B11)

O pytest `tests/integracao/test_carga_telemetria.py` envia 600 mensagens sem espera e verifica p95 < 100 ms sem crescimento. Para medir a API no ar, use o script, que manda `tel` no ritmo pedido e sai com código 1 se p95 ≥ 100 ms ou se o atraso acumulado passar de 1 s:

```bash
cd src/backend
python -m scripts.carga_telemetria --url http://localhost:8000 --taxa 10 --duracao 60
```

### Simular ponte (métricas / bateria)

Com a API rodando, o script abre uma execução, envia `hc_resultado`, depois `tel` a cada 1 s (bateria caindo, posição e velocidade), `passo` periódico e imprime consumo (mV e %), tempo e velocidade após cada POST — útil para validar BACK-04 como no hardware:

```bash
cd src/backend
python -m scripts.simular_ponte_telemetria
python -m scripts.simular_ponte_telemetria --ticks 30 --queda-mv 15 --passo-a-cada 2 --encerrar sucesso
python -m scripts.simular_ponte_telemetria --ticks 5 --encerrar cancelada
python -m scripts.simular_ponte_telemetria --ticks 8 --encerrar interrompida --confirmar-firmware
python -m scripts.simular_ponte_telemetria --health-check item-falha --componente-falha tof_direito
```

O `carga_telemetria` lê o `DATABASE_URL` (o mesmo da API) e sai com código 1 se não houver tentativa aberta, porque sem ela as mensagens são descartadas e a carga não mede a gravação. Para criar uma no banco de desenvolvimento:

```bash
docker compose exec db psql -U micromouse -d micromouse -c "
INSERT INTO execucao_logica (execucao_id, labirinto_id, status, tentativas_usadas, iniciada_em)
VALUES ('00000000-0000-0000-0000-00000000c4a6', 1, 'em_andamento', 1, now());
INSERT INTO tentativa (tentativa_id, execucao_id, attempt_index, status, tipo_inicio, iniciada_em)
VALUES (gen_random_uuid(), '00000000-0000-0000-0000-00000000c4a6', 1, 'running', 'nova', now());"
```

Depois da medição, encerre a tentativa (`UPDATE tentativa SET status = 'failed' WHERE execucao_id = '00000000-0000-0000-0000-00000000c4a6';`) para ela não receber a telemetria real.

---

## Stream SSE (BACK-07)

`GET /execucoes/{execucao_id}/stream` devolve `text/event-stream` com os eventos da execução. A linha `event:` repete o campo `tipo` do JSON em `data:`, e os seis tipos estão no `openapi.yaml`: `telemetria`, `passo`, `status`, `health_check`, `falha` e `sucesso`.

```bash
curl -N http://localhost:8000/execucoes/<execucao_id>/stream
curl -N -H "Last-Event-ID: 40" http://localhost:8000/execucoes/<execucao_id>/stream
```

- **Reenvio.** O fluxo começa reenviando os passos com `passo_id` maior que o `Last-Event-ID`. Sem o cabeçalho (ou com valor vazio, negativo ou não numérico), reenvia o trajeto inteiro da execução — é o que a tela recebe ao abrir no meio da corrida. Só o evento `passo` tem linha `id:`; a amostra `telemetria` não entra no reenvio.
- **Keep-alive.** Sem evento novo, sai uma linha de comentário a cada 15 s.
- **Fim do fluxo.** O servidor fecha em dois casos: execução já encerrada, depois do reenvio, e fila do cliente estourada (500 eventos). Nos dois, reconectar com `Last-Event-ID` recompõe o trajeto. No resto, quem fecha é o navegador.
- **Publicação.** Acontece **depois** do COMMIT da ingestão, na mesma chamada: o evento sai em milissegundos (RNF-B04). Uma falha na publicação é registrada no log e não desfaz a gravação nem falha o `POST /telemetria` (RNF-B07).
- **Um worker só.** A fila de cada cliente é memória do processo. Com `uvicorn --workers 2`, um `POST /telemetria` que caia no worker 2 não alcança o stream preso no worker 1. O mesmo já valia para o deduplicador e o relógio do robô.
- **404** quando não existe execução com aquele `execucao_id`.

Para ver funcionando sem robô, abra o `curl` acima e mande passos pelo `POST /telemetria` (seção anterior) ou rode o `src/simulador-telemetria`. Cada rodada precisa de um `boot` novo: o deduplicador guarda `(boot, seq)` em memória e descarta o que já viu.

---

## Onde implementar cada tarefa (BACK-01 … BACK-08)

| Tarefa | Pasta / arquivo |
|--------|------------------|
| BACK-01 Persistência / consultas | `app/repositories/`, `app/models/` |
| BACK-03 Máquina de estados | `app/services/gerenciador_execucoes.py` |
| BACK-04 Métricas | `app/services/calculo_metricas.py`, `app/services/atualizacao_metricas.py` · `python -m scripts.validar_back04` |
| BACK-05 Link / tempo | `app/services/monitor_conexao.py` (limites em `app/config.py`) |
| BACK-06 SQL específico | `app/repositories/` |
| BACK-07 SSE | `app/services/publicador_sse.py`, `app/services/fabrica_eventos.py`, `app/services/fluxo_sse.py`, `app/routers/stream.py` |
| BACK-08 Ponte serial | `ponte/` (processo separado) |
| HTTP / contrato ARQ-02 | `app/routers/` (501 até implementar) |
| POST /telemetria (4.4.1) | `contrato/telemetria.py` (`EntradaPonte`, `RespostaPonte`) |
| Schemas REST / SSE (ARQ-02) | `app/schemas/api.py` |
