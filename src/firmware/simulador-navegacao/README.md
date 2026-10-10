# Simulador de navegação no PC

Roda a navegação do firmware em labirintos desenhados em texto, sem placa e sem física, e diz se o robô chegou ao objetivo. É o único lugar onde os labirintos 8x4 e 12x4 são testados antes da AP18, já que as pistas físicas desses tamanhos estão fora do escopo do TAP (FIRM-02, [#114](https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/issues/114)).

O código da navegação é o mesmo da ESP32 (`lib/navegacao/nav.c`). A cada célula, o simulador:

1. lê as paredes da frente, da esquerda e da direita no labirinto (com ruído, se pedido);
2. chama `nav_passo()` com essas leituras;
3. gira e anda uma célula conforme a resposta, ou termina a corrida.

## Uso

Dentro de `src/firmware/simulador-navegacao`:

```bash
make                        # compila e roda todos os labirintos de labirintos/
./build/simulador --ruido 5 --semente 3 labirintos/8x4-*.txt
./build/simulador --telemetria build/telemetria labirintos/4x4-01-dir.txt
```

| Opção | O que faz |
|---|---|
| `--ruido PCT` | Chance (0 a 100) de cada leitura de parede vir trocada: parede vira passagem e vice-versa |
| `--semente N` | Semente do sorteio do ruído. Mesma semente, mesma corrida |
| `--limite N` | Células percorridas antes de desistir (padrão 1000) |
| `--telemetria PASTA` | Grava `PASTA/<labirinto>.jsonl`, o roteiro de telemetria da corrida |
| `--boot N` | Valor do campo `boot` no roteiro (padrão 1) |
| `--exigir-sucesso` | Sai com código 1 se algum labirinto não terminar em sucesso |

## Relatório

Uma linha por labirinto e um total no fim:

```text
8x4-frente-01-dir        8x4       sucesso       celulas=26   distintas=26  giros90=15   giros180=1   leituras_erradas=0
```

| Campo | Significado |
|---|---|
| `celulas` | Células percorridas, contando as repetidas |
| `distintas` | Células diferentes visitadas, contando a largada |
| `giros90`, `giros180` | Giros de 90° (direita ou esquerda) e de 180° |
| `leituras_erradas` | Leituras que o ruído trocou |

Resultados possíveis:

| Resultado | Quando |
|---|---|
| `sucesso` | A navegação declarou sucesso na célula de objetivo |
| `sem_caminho` | A navegação devolveu `NAV_ACAO_SEM_PASSO` |
| `colisao` | A navegação mandou andar para uma parede |
| `sucesso_falso` | A navegação declarou sucesso fora do objetivo |
| `limite` | Passou do `--limite` sem terminar |
| `INVALIDO` | O arquivo não é um labirinto válido (o código de saída é 2) |

> [!NOTE]
> Enquanto o flood fill (FIRM-01, [#117](https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/issues/117)) não entra, `nav_passo()` é um esqueleto que não sai do lugar, e todos os labirintos terminam em `sem_caminho`. O CI só falha hoje se algum labirinto for inválido. Quando o FIRM-01 entrar, basta acrescentar `--exigir-sucesso` no passo do CI (`.github/workflows/firmware.yml`).
>
> O tipo do labirinto ainda não é passado para a navegação: o `nav.h` atual não tem onde recebê-lo. Quando o FIRM-01 definir essa entrada (o tipo lido no DIP switch), o simulador passa o tipo do arquivo por ela.

## Labirintos

Os 24 arquivos de `labirintos/` são válidos e cobrem os três tamanhos nas duas orientações e nos dois lados:

| Arquivos | Tamanho | Lado longo |
|---|---|---|
| `4x4-01` a `4x4-04` | 4x4 | — |
| `8x4-lateral-01`, `8x4-lateral-02` | 8x4 | ao lado da largada (objetivo em (7, 3) para o robô) |
| `8x4-frente-01`, `8x4-frente-02` | 8x4 | à frente da largada (objetivo em (3, 7)) |
| `12x4-lateral-01`, `12x4-lateral-02` | 12x4 | ao lado (objetivo em (11, 3)) |
| `12x4-frente-01`, `12x4-frente-02` | 12x4 | à frente (objetivo em (3, 11)) |

Cada um existe em duas versões espelhadas: `-dir`, em que o labirinto se estende à direita do robô na largada, e `-esq`, à esquerda. O `4x4-01` é o mesmo labirinto do `src/app/labirinto_demo.h`.

O formato está em `lib/simulacao/sim_labirinto.h`. A largada é marcada com `L`; sem a marca, fica no canto inferior esquerdo. O simulador recusa o labirinto (`INVALIDO`) se:

- o tamanho não for 4x4, 8x4 ou 12x4 (em qualquer orientação);
- a borda tiver alguma abertura;
- a largada não ficar num canto ou não for um beco (três paredes e uma saída);
- o objetivo, no canto oposto à largada, não for alcançável.

Para acrescentar um labirinto, basta desenhar o arquivo em `labirintos/` com a extensão `.txt`; o CI passa a simulá-lo.

## Roteiro de telemetria

Com `--telemetria`, cada corrida vira um arquivo `.jsonl` no formato do contrato de telemetria v1 (ARQ-01, [#107](https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/issues/107)), para o [simulador de telemetria](../../simulador-telemetria/README.md) (ARQ-07) reproduzir para o backend:

1. health-check aprovado: um `hc_item` por componente e o `hc_resultado` com o tipo do labirinto;
2. um `passo` a cada célula, com as paredes que o robô leu;
3. `tel` a 5 Hz enquanto o robô anda;
4. `sucesso` ou `falha` (`collision` ou `stuck`), seguido de uma `tel` com o estado final.

As coordenadas estão no referencial do robô (largada em (0, 0), y para a saída do beco, x para o lado em que o labirinto se estende), então as versões `-dir` e `-esq` do mesmo labirinto aparecem iguais na web. Os tempos vêm de um modelo simples (200 mm/s, giro de 90° em 0,5 s) e os valores do health-check são fictícios.

O CI guarda os roteiros de todos os labirintos como artefato da execução (`roteiros-telemetria`).

Para mandar uma corrida para o backend, o simulador de telemetria reproduz o arquivo pelas saídas dele (`stdout`, `pty` ou `http`), trocando o `boot` a cada reprodução. Dentro de `src/simulador-telemetria`:

```bash
uv run python -m simulador ../firmware/simulador-navegacao/build/telemetria/8x4-frente-01-dir.jsonl --saida http --url http://localhost:8000
```

Detalhes em [Reproduzir uma gravação](../../simulador-telemetria/README.md#reproduzir-uma-gravação-jsonl).
