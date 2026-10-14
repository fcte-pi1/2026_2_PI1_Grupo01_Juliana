# Registro de decisões de arquitetura (ARQ-03)

Decisões que alinham **TAP** (baseline V1), **EAP**, **4.3**, **4.4** e **Requisitos** à arquitetura V2 e às regras operacionais do TAP (execução lógica, tentativas, telemetria).

| ADR | Título | AP7 / áreas |
| --- | --- | --- |
| [001](001-motores-n20-dc-encoders.md) | Motores N20 DC com encoders | Eletrônica, Estrutura, Firmware |
| [002](002-sensores-tof-quatro.md) | Quatro sensores ToF (I²C) | Eletrônica, Firmware |
| [003](003-bluetooth-spp-esp32.md) | Bluetooth clássico SPP na ESP32 | Eletrônica, Software, Firmware |
| [004](004-ponte-h-acionamento.md) | Ponte H dupla (TB6612) | Eletrônica, Hardware |
| [005](005-odometria-malha-fechada.md) | Odometria em malha fechada (PID) | Firmware, Software |
| [006](006-downlink-apenas-interrupcao.md) | Downlink: só interrupção manual | Backend, Front, Firmware |
| [007](007-falha-componente-consome-tentativa.md) | Falha de componente encerra tentativa | Firmware, Backend, Testes |
| [008](008-numeracao-uc01-uc34.md) | Numeração única UC01–UC34 | Software, Requisitos |
| [009](009-health-check-nove-itens.md) | Health-check com 9 componentes fixos | Firmware, Backend, Eletrônica |
| [010](010-reconexao-deduplicacao-seq.md) | Reconexão ≤10 s e deduplicação por `seq` | Backend, Firmware, Ponte |
| [011](011-execucao-logica-tres-tentativas.md) | Execução lógica, 3 tentativas e 10 min totais | Backend, Front, Testes |
| [012](012-retomada-celula-falha.md) | Retomada na célula da falha (sem checkpoint anterior) | Front, Backend, Firmware, Testes |
| [013](013-status-execucao-e-tentativa.md) | Dois status no contrato: execução e tentativa | Backend, Front |

**Oráculo de testes (backend):** [transicoes-execucao.md](transicoes-execucao.md) — tabelas de transição da **execução lógica** e da **tentativa**.

**Firmware (laço de navegação):** Tabela B em [4.4 — Diagrama de estados](../4.4%20-%20Projeto%20conceitual%20de%20software.md#diagrama-de-estados).

Formato de cada ADR: **Contexto** · **Decisão** · **Consequências**.
