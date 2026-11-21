# ADR-021 — O health-check só roda por comando, não ao ligar

**Data:** 21/11/2026 · **Assinou:** Amanda · **Issues:** #141 (FIRM-10), #139 (FIRM-11)

## Contexto

Até aqui o robô rodava o health-check ao ligar. O autoteste gira cada motor, então ligar o interruptor da bateria
já movia as rodas sem ninguém pedir, e na bancada e na pista isso atrapalhava (rodas girando ao ligar, bateria
sob carga antes de o enlace subir).

## Decisão

O robô liga **ocioso** (`MAQ_OCIOSO`): só manda a `tel`, com o estado `health-check` do contrato, e espera. O health-check
roda apenas quando o comando `health_check` chega pela telemetria. O `largar` continua exigindo o último health-check
aprovado, então no estado ocioso ele é ignorado. Os toques do BOOT (curto e longo) valem no estado ocioso.
O LED dá uma batida breve de 100 ms a cada 3 s nesse estado.

O health-check aprovado **vale por 10 minutos** (`MAQ_HC_VALIDADE_MS`): passado o prazo a largada é ignorada até um novo
`health_check` aprovado, e o robô que esperava a largada volta ao estado ocioso. Na corrida o prazo não corta; vale para largar de novo
(depois de `success` ou `failed` também). O mesmo prazo vale no contrato e no back-end ([ADR-022](022-health-check-vale-10-minutos.md)).

## Alternativas descartadas

- **Manter o health-check no boot e só esconder o autoteste dos motores.** O boot ficaria diferente do comando e o
  health-check do boot não seria o mesmo que a web pede.
- **Um interruptor ou DIP para escolher.** Mais um estado de hardware para configurar, sem ganho: o comando já resolve.

## Consequências

- Quem liga o robô precisa pedir o `health_check` antes do `largar` (`monitor_comandos.py`, a ponte ou `verificar_serial.py --health-check`).
- O `hc_resultado` do boot deixa de existir; o `inicio` (`nova` ou `retomada`) passa a sair no `hc_resultado` do comando.
- Quem espera mais de 10 min entre o health-check e a largada precisa pedir o health-check de novo.
- O estado ocioso não aparece no contrato: ele sai como `health-check`, como o aguardando e o reprovado.
