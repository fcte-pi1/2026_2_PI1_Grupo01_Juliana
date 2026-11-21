# ADR-018 — Firmware com a HAL real (`esp32_real`), com o ToF mockado em um ambiente à parte

**Status:** Aceito  
**Data:** 2026-11-20

> **Nota de 21/11/2026:** o `esp32_real` descrito aqui (só o health-check) passou a se chamar `esp32_real_hc`, e o `esp32_real_hc_tof_mock` é o seu par com o ToF mockado. O nome `esp32_real` é agora o firmware completo no robô (navegação e movimento, FIRM-10), com as variantes `esp32_real_tof_mock`, `esp32_real_bt` e `esp32_real_bt_tof_mock`.

## Contexto

Até aqui o firmware completo só rodava na ESP32 com a **HAL simulada** (`esp32_sim`): ToF, motores, encoders, DIP e bateria saem do mundo simulado. Os drivers reais existem em `lib/hal/esp32/`, mas só eram exercitados por programas de bancada (`esp32_drivers` e `esp32_hc_bancada`), que são interativos e não enviam a telemetria do contrato.

Os testes de integração de 20/11/2026 ([7.5](../7.5%20-%20Testes%20de%20integra%C3%A7%C3%A3o.md), testes 5.1 a 5.5) mostraram que o DIP, a bateria, os motores e os encoders funcionam com os drivers reais, mas que **dois dos três ToF não iniciam** (teste 5.2): um lateral falha na calibração e o outro nem responde. Os módulos estão soldados, então o hardware não pôde ser corrigido na sessão. Com isso o health-check real reprova sempre, e o back-end recusa abrir a tentativa (R06), o que impede os testes de ponta a ponta no cenário ideal.

## Decisão

- **`esp32_real`:** firmware com a HAL real inteira, ToF real incluído. Por enquanto roda **só o health-check**: executa ao ligar e a cada comando `health_check` da ponte, e envia os `hc_item`, o `hc_resultado` e a `tel` (estado `health-check`, a 1 Hz) pelo contrato v1. Não anda; os motores só giram no autoteste, com as rodas no ar. Enquanto os laterais não funcionarem, ele reprova, e isso é o comportamento correto.
- **`esp32_real_tof_mock`:** o mesmo firmware compilado com `-D TOF_MOCK`. Só o `hal_tof` é trocado por leituras fixas (frontal 300 mm, laterais 120 mm); todo o resto continua real. O health-check aprova, e o ensaio de ponta a ponta roda com hardware real, exceto o ToF.
- **Enlace pela serial USB** nos dois ambientes (`hal_bt_serial_esp32.cpp`), porque o Bluetooth real é da FIRM-11. A ponte do notebook abre `/dev/ttyACM0`. A HAL e a telemetria não mudam quando o Bluetooth entrar.
- **`hc_montar_eventos`** (em `lib/health_check`) monta os eventos do contrato e é compartilhado com a navegação do `esp32_sim`, para o contrato sair igual nos três ambientes.

Os ambientes de teste que já existiam continuam: `esp32_sim` (tudo simulado), `esp32_drivers` e `esp32_hc_bancada` (bancada).

## Alternativas descartadas

- **Usar só o `esp32_sim` para o ponta a ponta.** Não exercita nenhum driver real; o DIP, os motores, os encoders e a bateria lidos pelo back-end seriam todos simulados.
- **Mockar o ToF por padrão no `esp32_real`.** Esconderia a falha dos laterais e poderia chegar ao robô final sem ser notado. O mock só existe em um ambiente com nome explícito.
- **Esperar o conserto dos ToF laterais.** Os módulos estão soldados e a Eletrônica não tem data; o ponta a ponta ficaria parado.

## Consequências

- O ponta a ponta com o back-end e a web pode rodar com o health-check aprovado usando `esp32_real_tof_mock`, e com o resultado real (reprovado) usando `esp32_real`.
- O `TOF_MOCK` **nunca** entra no firmware do robô final. Quando os ToF laterais forem corrigidos, o `esp32_real` passa a aprovar e o ambiente com mock deixa de ser necessário.
- O `esp32_real` é compilado no CI (`firmware.yml`), junto com o `esp32_real_tof_mock`. A validação na placa ainda está pendente.
- O firmware completo com navegação e a máquina de estados (espera do `largar`) continua em `esp32_sim` e depende das próximas tarefas da FIRM-09.
