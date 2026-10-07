# Software — código e decisões de implementação

Esta pasta concentra o **código** do Sistema de Telemetria e do firmware do Micromouse. A especificação formal (requisitos, arquitetura, diagramas) fica em [`docs/`](../docs/), em especial:

- [`docs/2 - Requisitos.md`](../docs/2%20-%20Requisitos.md)
- [`docs/4.4 - Projeto conceitual de software.md`](../docs/4.4%20-%20Projeto%20conceitual%20de%20software.md)

Este arquivo registra **decisões de software tomadas durante o desenvolvimento** — o que foi escolhido na prática, por quê, e onde está no código ou na doc. As **12 decisões de arquitetura (ARQ-03)** e o oráculo de transições estão em [`docs/decisoes/`](../docs/decisoes/README.md). Quando uma decisão mudar a especificação, atualize também os documentos em `docs/`.

## Estrutura

| Pasta | Conteúdo |
| --- | --- |
| [`backend/`](backend/) | API (FastAPI), persistência, regras TAP, ponte serial |
| [`frontend/`](frontend/) | SPA React + TypeScript, tempo real (SSE), protótipo operacional |
| [`firmware/`](firmware/) | ESP32: navegação, sensores, motores, telemetria Bluetooth |

> [!WARNING]
> **Não versione segredos** (`.env` com valores reais, tokens, senhas). Use `.env.example` nos subprojetos.

---

## Registro de decisões

Formato sugerido para novas entradas:

```text
### YYYY-MM-DD — Título curto
- **Contexto:** …
- **Decisão:** …
- **Consequência:** …
- **Doc/código:** RF…, US…, caminho ou PR #
```

---

### 2026-09 — Referência de casos de uso (UC01–UC34)

- **Contexto:** IDs de UC divergiam entre documentos antigos e o diagrama do 4.4.
- **Decisão:** Numeração única do [Diagrama de Casos de Uso](../docs/4.4%20-%20Projeto%20conceitual%20de%20software.md#diagrama-de-casos-de-uso) (ex.: **UC01** Nova execução, **UC20** health-check, **UC04** encerrar tentativa, **UC31** retomar, **UC12.1** / **UC12.2** timeout e link).
- **Consequência:** Código e testes citam os mesmos UC que RF/US no backlog.
- **Doc/código:** `docs/2 - Requisitos.md`, `docs/4.4 - Projeto conceitual de software.md`.

### 2026-09 — Autonomia do firmware (downlink)

- **Contexto:** TAP e RNF06 limitam o que a web pode enviar ao robô.
- **Decisão:** Sem comando de **iniciar** ou **parar** genérico; único downlink = **interrupção** no encerramento manual (RF30 / RF39). Partida após health-check aprovado, só pelo firmware.
- **Consequência:** Backend abre execução/tentativa; transição para `running` vem da telemetria, não de write na serial.
- **Doc/código:** RF10, RF39, RNF06; canal serial em `backend/` (a implementar).

### 2026-09 — Falha de componente e tentativas (TAP)

- **Contexto:** Regra das 3 tentativas por execução lógica (10 min totais).
- **Decisão:** Falha de componente encerra a **tentativa** (`failed`); retomada só com **Retomar tentativa (*n*/3)**, nova tentativa, health-check (**UC20**), mapa na NVS (**RF38**, **RF42**).
- **Consequência:** Não manter estado “consertou na mesma tentativa” no firmware; backend incrementa `attempt_index`.
- **Doc/código:** diagrama de estados e BPMN no 4.4; `firmware/` (a implementar).

### 2026-09 — Stack embarcada e telemetria

- **Contexto:** Alinhamento com Estrutura, Eletrônica e Energia.
- **Decisão:** Motores **DC N20** + encoder, **PID** de velocidade, sensores **ToF** (I²C); detecção de **`stuck`** por encoders parados (**US45**); Bluetooth **SPP** da ESP32 (`BluetoothSerial`); health-check com **9 componentes** fixos; reconexão ≤10 s com reenvio e deduplicação por **`seq`** (RNF08).
- **Consequência:** Métricas no backend: distância base células × 18 cm (RF12); bateria V/% pela curva do doc 4.2 (RF11).
- **Doc/código:** `docs/3 - EAP.md`, RF20–RF22, RF29; DER `HEALTH_CHECK_ITEM` no 4.4.

### 2026-09 — Organização deste repositório

- **Contexto:** Equipe precisa de histórico leve sem duplicar o 4.4 inteiro.
- **Decisão:** ADRs formais em `docs/decisoes/`; este `src/README.md` resume decisões de **implementação** no código.
- **Consequência:** PRs de código referenciam ADR ou entrada aqui quando fecharem pendência (ex.: formato JSON da telemetria → ARQ-01).

---

## Pendências abertas (software)

Itens ainda não fechados na implementação — detalhes no 4.4 (pendências / diagrama de componentes):

- Formato das mensagens de telemetria (JSON, campos, `seq`, fase `health-check` / `running`).
- Limiar de parede ToF e parâmetros PID / `stuck` (calibração em testes).
- Uso concreto do DIP switch (modos que não revelam o labirinto).

Quando uma pendência for resolvida, **adicione uma entrada datada** na seção acima e remova ou atualize a linha correspondente aqui.
