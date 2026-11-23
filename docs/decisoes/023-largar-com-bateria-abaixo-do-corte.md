# ADR-023 — O operador pode largar com a bateria abaixo do corte, só quando ela é o único item reprovado

**Data:** 21/11/2026 · **Assinou:** Amanda · **Complementa:** [ADR-017](017-downlink-tres-comandos.md) e [ADR-022](022-health-check-vale-10-minutos.md)

## Contexto

O health-check reprova a bateria abaixo de 7000 mV (3,5 V por célula na LiPo 2S, `BATERIA_CORTE_MV`) e, com ele reprovado,
o back-end recusa a largada (`health_check_reprovado`) e o firmware ignora o `largar`. Nos ensaios a bateria de 650 mAh
desceu a 6861 mV depois de várias corridas curtas, o que bloqueava todo o fluxo mesmo com os outros 7 itens aprovados.
A tela precisava deixar o operador decidir, sem desabilitar o botão **Nova execução**.

## Decisão

- **Tela:** o botão **Nova execução** (e a confirmação do modal e da retomada) continua ativo quando só a bateria
  reprovou. Antes de largar abre o aviso **Bateria abaixo do padrão seguro**, com a leitura em mV e o corte, e o
  operador escolhe **Cancelar** ou **Continuar mesmo assim**.
- **Back-end:** `ignorar_bateria` em `POST /execucoes` e em `POST /execucoes/{id}/retomar`. Só vale se os 8 itens
  chegaram, a bateria reprovou e todos os outros aprovaram, dentro dos 10 minutos e com o DIP do labirinto. Fora disso,
  a recusa é a de sempre.
- **Contrato e firmware:** o `largar` leva `"forcar_bateria":true` nesse caso. O firmware parte a partir de um
  health-check concluído e ainda válido, aprovado ou não (`MAQ_EV_CMD_LARGAR_FORCADO`); sem health-check concluído,
  vencido ou com a corrida em andamento, ignora.

## Alternativas descartadas

- **Baixar o limite de 7000 mV.** Ele protege a LiPo contra descarga profunda; a exceção é uma escolha do operador, não
  um novo padrão.
- **Liberar qualquer item reprovado.** Motores, encoders e ToF reprovados indicam defeito, não bateria baixa.
- **Só o front.** O firmware ignorava o `largar` com o health-check reprovado e a execução abriria com o robô parado.

## Consequências

- O firmware não confere qual item reprovou: quem garante que foi só a bateria é o back-end. Um `largar` forçado por
  outro caminho passaria pelo firmware.
- Durante a corrida o firmware não corta por bateria baixa (`BATERIA_BAIXA` só entra no health-check), então a execução
  anda até o fim ou até falhar por outro motivo.
- A confirmação não fica gravada na tentativa; o back-end só registra no log. O resultado dessas execuções não vale como
  medida com a bateria no padrão.
- Exige regravar o robô (`esp32_real_bt`).
