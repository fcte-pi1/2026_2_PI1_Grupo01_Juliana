# ADR-022 — O health-check vale por 10 minutos, no contrato, no back-end e no firmware

**Data:** 21/11/2026 · **Assinou:** Amanda · **Substitui em parte:** [ADR-017](017-downlink-tres-comandos.md) (validade de 7 minutos)

## Contexto

O ADR-017 fixou o health-check como pré-requisito da tentativa, com validade de 7 minutos, e o back-end recusa
**Nova execução** e **Retomar** com `health_check_expirado` depois disso. O firmware (ADR-021) passou a aplicar o mesmo
tipo de prazo: o `largar` é ignorado quando o último health-check aprovado é velho demais. Os dois lados precisam do
mesmo número, senão o robô aceita uma largada que o back-end recusa, ou o contrário.

Todo percurso precisa ser resolvido em, no máximo, 10 minutos (`tempo_maximo_execucao_s` de 600 s). Um prazo de 7 minutos
para o health-check ficava menor que o percurso, e o operador tinha que refazer o health-check no meio de um trabalho
que ainda cabia na regra de 10 minutos.

## Decisão

A validade do health-check passa de 7 para **10 minutos**, igual nos três lugares:

- **Contrato e back-end:** `validade_health_check_s` de 420 para 600; `valido_ate` é `concluido_em` mais 600 s;
  o motivo `health_check_expirado` vale acima de 10 minutos.
- **Firmware:** `MAQ_HC_VALIDADE_MS` de 600000 (ADR-021), contado do health-check aprovado.
- **Documentos:** requisitos (RF10), projeto conceitual de software (4.4), contrato (4.4.1), OpenAPI, glossário e a
  tabela de transições (R06).

## Alternativas descartadas

- **Manter 7 minutos no back-end e 10 no firmware.** Dois prazos diferentes para a mesma regra.
- **Não ter prazo.** O health-check de ontem não diz nada sobre o robô de hoje.

## Consequências

- O operador tem 10 minutos entre o health-check e a largada, o mesmo teto de um percurso.
- O `docs/7.4` (testes de software) segue citando os 7 minutos nos casos de teste antigos: ele não foi alterado nesta
  decisão e precisa ser conferido com o time antes de os testes do back-end serem refeitos.
