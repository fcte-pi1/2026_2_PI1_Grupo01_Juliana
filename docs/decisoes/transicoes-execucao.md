# Tabelas de transição — execução lógica e tentativa

Oráculo único para **backend** (`GerenciadorExecucoes`), **front** (habilitação de botões), **firmware** (telemetria de status) e **testes** (7.4 caso 4.2, BACK-03). Decisão de produto: [ADR-011](011-execucao-logica-tres-tentativas.md).

**Convenções** 

- **Execução lógica:** `em_andamento` | `concluida` | `cancelada`.
- **Tentativa:** `health-check` | `running` | `success` | `failed` | **∅** (nenhuma tentativa aberta na execução).
- **Aberta:** `health-check` ou `running`.
- Contadores: `attempt_index` ∈ {1,2,3}; relógio da execução desde **Nova execução** (RF32).
- Ações recusadas registram **tentativa rejeitada** (RF19) sem alterar estados válidos.

`success` é o único status de tentativa que conclui a execução. `failed` na tentativa 1 ou 2, inclusive **Encerrar tentativa**, mantém a execução `em_andamento`. Na tentativa 3, **Encerrar tentativa** também deixa a tentativa `failed` e a execução vai a `cancelada`, porque não cabe retomada. O mesmo vale para as outras falhas da terceira tentativa, para a perda de link, para o estouro de tempo e para uma nova execução que cancela a anterior.

| Tentativa | Execução depois | Linhas |
| --- | --- | --- |
| `health-check` ou `running` | `em_andamento` | T01, T03 |
| `success` | `concluida` | T05, T11 |
| `failed` na tentativa 1 ou 2, inclusive **Encerrar tentativa** | `em_andamento` | T04, T06, T07 |
| `failed` na tentativa 3, inclusive **Encerrar tentativa** | `cancelada` | T07, T12 |
| `failed` por link (10 s) ou tempo (10 min) | `cancelada` | T08, T09 |
| execução anterior ainda aberta quando nasce outra | a anterior fica `cancelada` | T13 |

---



## 1. Tentativa (dentro de execução `em_andamento`)


| ID  | Estado tentativa (antes)    | Evento                                    | Estado tentativa (depois) | Estado execução (depois) | Efeito esperado                                                               |
| --- | --------------------------- | ----------------------------------------- | ------------------------- | ------------------------ | ----------------------------------------------------------------------------- |
| T01 | ∅                           | Abertura com **Nova execução** (UC01)     | `health-check`            | `em_andamento`           | Nova `execucao_id`, `tentativa_id`, `attempt_index`=1, `iniciada_em` execução |
| T02 | ∅                           | **Retomar tentativa** válida (UC31)       | `health-check`            | `em_andamento`           | Nova `tentativa_id`, `attempt_index`+1, `tipo_inicio`=retomada                |
| T03 | `health-check`              | Telemetria: health-check OK → `running`   | `running`                 | `em_andamento`           | Transição só por telemetria (RF10), sem downlink                              |
| T04 | `health-check`              | Telemetria: falha no health-check         | `failed`                  | `em_andamento`*          | Motivo `health_check_failed` ou `falha_componente` (RF15)                     |
| T05 | `running`                   | Telemetria: `success`                     | `success`                 | `concluida`              | Métricas; execução encerrada com sucesso (RF26, RF33)                         |
| T06 | `running`                   | Telemetria: `failed` (trajeto/componente) | `failed`                  | `em_andamento`†          | Motivo conforme RF15; célula/componente gravados                              |
| T07 | `health-check` ou `running` | **Encerrar tentativa** (UC04)             | `failed`                  | `em_andamento`†          | Motivo escolhido + `encerrado_operador`; `interrupcao_pendente`=true (RF39). Na tentativa 3, a execução segue para T12 e fica `cancelada` |
| T08 | `health-check` ou `running` | **Perda de link** 10 s (UC12.2)           | `failed`                  | `cancelada`              | Motivo `link_lost`; sem downlink                                              |
| T09 | `health-check` ou `running` | **Tempo execução** ≥600 s (UC12.1)        | `failed`                  | `cancelada`              | Motivo `time_exceeded`; sem downlink                                          |
| T10 | `failed`                    | —                                         | `failed`                  | †                        | Terminal; só nova tentativa via T02 ou fim de execução                        |
| T11 | `success`                   | —                                         | `success`                 | `concluida`              | Terminal                                                                      |


\* e † Com `attempt_index` 1 ou 2, a execução permanece `em_andamento` e cabe **Retomar tentativa**. Com `attempt_index`=3, a falha segue para **T12** e a execução fica `cancelada`. Isso inclui **Encerrar tentativa** (T07) na terceira tentativa: a tentativa fica `failed` e a execução fica `cancelada`.


| ID  | Estado tentativa (antes) | Evento                                             | Estado tentativa (depois)               | Estado execução (depois) | Efeito esperado                           |
| --- | ------------------------ | -------------------------------------------------- | --------------------------------------- | ------------------------ | ----------------------------------------- |
| T12 | `failed`                 | `attempt_index`=3 e sem retomada pendente          | `failed`                                | `cancelada`              | Três tentativas consumidas sem sucesso, inclusive **Encerrar tentativa** na terceira |
| T13 | Aberta                   | **Nova execução** (UC01) cancela execução anterior | *(exec anterior)* `failed` ou encerrada | anterior `cancelada`     | Tentativa anterior fechada; nova exec T01 |


---



## 2. Execução lógica (visão agregada)

Estado derivado das tentativas e dos monitores de tempo/link. Use com a tabela 1 para testes de integração.


| ID  | Estado exec (antes)        | Evento                                                     | Estado exec (depois)  | Tentativa / efeito                                 |
| --- | -------------------------- | ---------------------------------------------------------- | --------------------- | -------------------------------------------------- |
| E01 | *(nenhuma ou terminal)*    | **Nova execução** sem outra `em_andamento`                 | `em_andamento`        | T01                                                |
| E02 | `em_andamento`             | **Nova execução** (outra labirinto/sessão)                 | `em_andamento` (nova) | Execução anterior → `cancelada` (T13); T01 na nova |
| E03 | `em_andamento`             | Tentativa → `success`                                      | `concluida`           | T05                                                |
| E04 | `em_andamento`             | Tentativa → `failed`, restam retomadas (`attempt_index`<3) | `em_andamento`        | T06/T07; tentativa ∅ até retomada                  |
| E05 | `em_andamento`             | Tentativa → `failed`, `attempt_index`=3                    | `cancelada`           | T12                                                |
| E06 | `em_andamento`             | UC12.2 link                                                | `cancelada`           | T08                                                |
| E07 | `em_andamento`             | UC12.1 tempo                                               | `cancelada`           | T09                                                |
| E08 | `concluida` ou `cancelada` | **Nova execução**                                          | `em_andamento`        | Nova execução (E01/T01)                            |


---



## 3. Recusas (sem transição de tentativa/execução válida)


| ID  | Condição                                                     | Ação do operador                                             | Resultado                                 |
| --- | ------------------------------------------------------------ | ------------------------------------------------------------ | ----------------------------------------- |
| R01 | Existe tentativa **aberta**                                  | **Retomar tentativa** ou abrir outra tentativa na mesma exec | Recusa RF19; registro tentativa rejeitada |
| R02 | Execução não `em_andamento` ou última tentativa não `failed` | **Retomar tentativa**                                        | Recusa RF19                               |
| R03 | `attempt_index`=3 já consumido                               | **Retomar tentativa**                                        | Recusa RF19                               |
| R04 | Execução `cancelada` ou `concluida`                          | **Encerrar tentativa**                                       | HTTP erro / ação ignorada; sem mudança    |
| R05 | Nenhuma tentativa aberta                                     | **Encerrar tentativa**                                       | HTTP erro / ação ignorada                 |


**Nova execução** com execução anterior `em_andamento` **não** é recusa: cancela a anterior (E02), conforme US12.

---



## 4. Firmware (referência cruzada)

Transições do laço embarcado (health-check → lendo sensores → …): **Tabela B** em [4.4 — Diagrama de estados](../4.4%20-%20Projeto%20conceitual%20de%20software.md#eventos-e-transições). Status `health-check` / `running` / `success` / `failed` na telemetria devem ser **consistentes** com T03–T07 deste documento.

---



## 5. Testes

- **Backend:** um teste automatizado por linha **T01–T13** e **E01–E08** (mínimo); recusas **R01–R05**.
- **7.4 / 4.2:** suite “Regras de tentativas e recusa” cobre tabela inteira.
- **Front:** após T06/T07 com retomada possível, exibir **Retomar (*n*/3)**; ocultar após T12/E05.

