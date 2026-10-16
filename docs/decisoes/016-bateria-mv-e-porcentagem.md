# ADR-016 — Bateria em mV no contrato, porcentagem calculada pelo backend

**Status:** Aceito  
**Data:** 2026-10-15

## Contexto

O robô envia a tensão da bateria em `bat_mv`, um inteiro em milivolts ([Contrato de telemetria](../4.4.1%20-%20Contrato%20de%20telemetria.md)), e o backend grava esse valor como recebido: a Tabela de destino do contrato diz `bat_mv` → `bateria` (em mV). O mesmo vale para `vel_mm_s`, em mm/s.

O `src/backend/openapi.yaml`, porém, descrevia o campo `bateria` como "Tensão em volts, convertida de bat_mv", e os exemplos do stream mostravam `"bateria":7.81` e `"velocidade":0.21`. O `GET /execucoes/{id}` sempre devolveu o valor cru, em mV, então o contrato divergia da implementação desde antes do BACK-07 — e era o contrato que estava errado.

Duas razões sustentam o mV:

- **Firmware.** Medir em mV evita ponto flutuante no microcontrolador, que deixa o processamento mais lento e pesado. É também a unidade com que a Eletrônica trabalha na proteção do circuito: o corte por subtensão e o alerta estão fixados em mV por célula ([4.2](../4.2%20-%20Projeto%20conceitual%20de%20energia.md)).
- **O projeto já tinha decidido quem converte.** O contrato (linha de `bat_mv`) e o RF11 atribuem ao backend a conversão para **percentual de carga**, pela curva de descarga da fonte ativa, e não a conversão para volts.

Faltava só dizer isso em um lugar só, porque a tela precisa de um número que signifique "quanto resta", e tensão crua não serve: 7,4 V é 15 % numa LiPo 2S e seria carga cheia num pacote de pilhas AA.

## Decisão

- **mV é a unidade canônica** do contrato serial ao SSE, e mm/s para a velocidade. Nenhuma camada divide por mil.
- **A porcentagem é responsabilidade do backend**, em `nivel_bateria_pct` (`app/services/calculo_metricas.py`), por interpolação linear na curva de descarga da fonte ativa. O evento `telemetria` do SSE leva os dois campos: `bateria`, em mV, e `bateria_pct`.
- **A fonte ativa é deduzida da própria tensão, sem configuração.** As faixas não se sobrepõem: a LiPo 2S vai de 7000 mV (corte por subtensão) a 8400 mV, e o pacote de quatro pilhas AA termina em 6400 mV. Acima de 7000 mV só pode ser a LiPo; abaixo, só as pilhas. Se algum dia as faixas se cruzarem, aí sim entra uma configuração declarando a fonte.
- **As curvas espelham `BATERIA_CURVA_LIPO` e `BATERIA_CURVA_AA`** de `src/firmware/include/config/robo.h`, incluindo o aviso de que são **provisórias**: são curvas típicas sob a corrente média de 0,43 A e serão trocadas pela curva medida nos [testes de energia](../7.2%20-%20Testes%20de%20energia.md).
- **O openapi foi corrigido:** a descrição do campo `bateria`, os exemplos do stream (`7810`, `210`, `valor_lido: 7840`) e o novo `bateria_pct`.

Na LiPo, 0 % é o corte de 7,0 V, e não a descarga total: é a carga que o robô ainda pode usar.

## Consequências

- O front-end exibe mV e porcentagem; se quiser volts na tela, a divisão é de apresentação, não de contrato.
- `valor_lido` do `health_check` segue a unidade do componente medido: mV para a bateria, mm para os ToF.
- Os exemplos REST do `openapi.yaml` (as telas `Tela0X`) e as colunas `bateria_inicial`, `bateria_final` e `consumo_bateria` de `TENTATIVA` ainda trazem valores em volts. Elas são métricas do **BACK-04**, que ainda não as calcula; quando calcular, devem seguir esta decisão. Fica como pendência registrada, fora do BACK-07.
- A curva vive no backend e no firmware. Enquanto for provisória, as duas cópias mudam juntas; depois dos testes 7.2, a curva medida substitui as duas.
