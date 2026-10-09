# Simulador de telemetria

Faz o papel do Micromouse: lê um roteiro JSON e emite as mensagens do contrato de
telemetria v1 (`docs/4.4.1 - Contrato de telemetria.md`), uma linha JSON por mensagem.
Serve para testar a ponte, o backend e o front sem o robô. As mensagens vêm de
`contrato.telemetria`, do backend, então o simulador nunca sai do contrato.

## Instalação

Precisa do [uv](https://docs.astral.sh/uv/) e do Python 3.12 ou mais novo.

```sh
cd src/simulador-telemetria
uv sync
```

O `uv sync` instala também o backend (`../backend`) em modo editável, de onde vem o contrato.

## Uso

```sh
uv run python -m simulador <roteiro> [opções]
```

As linhas da telemetria vão para a saída escolhida; o log (incluindo cada linha enviada) vai
para o stderr.

### Saídas

| `--saida` | O que faz |
| --- | --- |
| `stdout` (padrão) | Escreve cada linha no terminal. Bom para ver o cenário ou gerar um `.jsonl`. |
| `pty` | Abre uma porta serial virtual, imprime o caminho (ex.: `/dev/pts/5`) no stderr e só começa quando alguém abre a porta, como o SPP do robô faz com a ponte. Linux e WSL; no Windows nativo termina com código 1. |
| `http` | Envia cada linha para `POST /telemetria` da API (`--url` ou `API_URL`), sem a ponte. Sem URL, termina com código 1. |

### Opções

| Opção | Padrão | Efeito |
| --- | --- | --- |
| `roteiro` | | Arquivo JSON do cenário (formato na seção 6.1 de `tasks/prd-simulador-telemetria.md`). |
| `--saida stdout\|pty\|http` | `stdout` | Para onde as linhas vão. |
| `--url URL` | | URL da API, para `--saida http`. |
| `--acelerar N` | `1` | Divide as esperas e o `t_ms` por N. |
| `--sem-espera` | | Emite tudo de uma vez, sem dormir. |
| `--taxa-tel HZ` | `5` | Taxa da `tel` em movimento, de 1 a 20 Hz; fora disso, código 1. |
| `--boot N` | salvo em `.simulador/boot` | Número do boot da primeira tentativa. |

Mesmo acelerado, o simulador nunca passa de 20 linhas por segundo de tempo real.

### Boot

Como o robô, o simulador guarda o próximo boot em `.simulador/boot` (relativo à pasta em que
roda; o arquivo não é versionado). Cada execução começa no número salvo (`0` se o arquivo não
existe) e grava o primeiro boot mais o número de tentativas do roteiro, então duas execuções
seguidas nunca repetem boot. `--boot N` força o primeiro boot (útil para comparar saídas) e
também atualiza o arquivo.

## Comportamento

### Interromper

Com `--saida pty` ou `--saida http`, o simulador ouve o comando que o Sistema de Telemetria
manda ao robô: `{"v":1,"cmd":"interromper"}` (no pty, uma linha na porta; no http, a resposta
do `POST /telemetria`). Se o robô está andando, o resto do roteiro é descartado e saem a
`falha` com `origem` `web` e `motivo` nulo (quem preenche é o backend) na posição atual e três `tel`
`failed` a 1 Hz. Um novo comando nesses 3 s reenvia a mesma `falha` (mesmo `seq`). Comando
com o robô já parado, ou qualquer outra linha, só vai para o log.

### `perda_link`

O link cai na célula `na_celula` por `segundos` de tempo simulado. Na queda nada sai, mas o
robô continua andando e cada evento (tudo menos `tel`) entra no buffer de 64. Na volta, o
buffer inteiro é reenviado, do mais antigo ao mais novo, um a cada 50 ms, inclusive eventos
que já tinham saído antes da queda; o backend descarta os repetidos pelo `seq`. A `tel` volta
a sair na hora.

```json
"eventos": [{"na_celula": 4, "tipo": "perda_link", "segundos": 7}]
```

No pty, a queda é só silêncio na porta; no http, nenhum `POST` é feito durante ela.

### `parada_boot`

O operador aperta o BOOT na célula `na_celula`: a corrida para ali, sai a `falha` com
`origem` `boot` e `motivo` `encerrado_operador` e três `tel` `failed` a 1 Hz. O `fim` da
tentativa e os eventos de células posteriores são ignorados; a próxima tentativa segue
normalmente.

```json
"eventos": [{"na_celula": 2, "tipo": "parada_boot"}]
```

## Configuração

O simulador usa o mesmo `.env` do backend. Copie o exemplo e ajuste o que precisar:

```sh
cp src/backend/.env.example src/backend/.env
```

Para carregar o arquivo, use o `--env-file` do uv (não há python-dotenv):

```sh
cd src/simulador-telemetria
uv run --env-file ../backend/.env python -m simulador <roteiro> [opções]
```

Cada variável só vale quando a opção correspondente não foi passada; a opção sempre vence.
Valor inválido no ambiente termina com código 1, como a opção inválida.

| Variável | Opção | Padrão | Efeito |
| --- | --- | --- | --- |
| `API_URL` | `--url` | | URL da API, para `--saida http`. Também usada pela ponte. |
| `SIMULADOR_SAIDA` | `--saida` | `stdout` | `stdout`, `pty` ou `http`. |
| `SIMULADOR_ACELERAR` | `--acelerar` | `1` | Divide as esperas e o `t_ms`; maior que 0. |
| `SIMULADOR_TAXA_TEL_HZ` | `--taxa-tel` | `5` | Taxa da `tel` em movimento, de 1 a 20 Hz. |

## Roteiros prontos

Ficam em `roteiros/`.

### `sucesso-4x4.json`

Health-check aprovado, `inicio` nova, corrida de (0, 0) até o centro (2, 2) e `sucesso`.

```sh
uv run python -m simulador roteiros/sucesso-4x4.json
```

### `falha-componente.json`

A corrida para em (0, 3), no meio do labirinto, com `falha` automática `falha_componente` e
`componente` `motor_direito`.

```sh
uv run python -m simulador roteiros/falha-componente.json --saida pty --acelerar 4
```

Em outro terminal, leia a porta impressa pelo simulador (ex.: `cat /dev/pts/5`).

### `malformadas.json`

Uma corrida válida com linhas inválidas no meio, que o backend tem que descartar: texto que
não é JSON, `v` = 2, `x` = 12, linha com mais de 256 bytes, `falha` web com motivo e um
`tipo` desconhecido.

```sh
uv run python -m simulador roteiros/malformadas.json --sem-espera > malformadas.jsonl
```

### `colisao-retomada.json`

A tentativa 1 (`inicio` nova) bate em (1, 3) com `falha` `collision`. Depois de 5 s, a
tentativa 2 (boot seguinte, `inicio` retomada) sai de novo de (0, 0) e chega ao centro com
`sucesso`.

```sh
uv run python -m simulador roteiros/colisao-retomada.json --saida http --url http://localhost:8000
```

### `perda-link.json`

Corrida de exploração até o centro com uma queda de 12 s no meio, a partir da célula 4. Passa
do `LIMITE_SEM_SINAL_S` (10 s), então o backend deve marcar `link_lost`. Depois da queda o
buffer é reenviado e o robô termina a corrida.

```sh
uv run python -m simulador roteiros/perda-link.json --saida http --url http://localhost:8000
```

### `reconexao.json`

A mesma corrida com uma queda de 7 s, abaixo do limite: o link volta, o buffer é reenviado e
a corrida chega ao `sucesso`.

```sh
uv run python -m simulador roteiros/reconexao.json --saida pty
```

### `estouro-10min.json`

O robô dá 64 voltas no anel externo do 4x4 (o centro é fechado), mais de 600 s simulados
andando, sem `sucesso` nem `falha` (`fim` `nenhum`). Quem encerra a execução por tempo é o
backend.

Para não esperar 10 min, rode acelerado e reduza o limite do backend na mesma proporção. No
`src/backend/.env`:

```dotenv
TEMPO_MAXIMO_EXECUCAO_S=60
```

E o simulador com `--acelerar 10`, que divide também o `t_ms` por 10:

```sh
uv run python -m simulador roteiros/estouro-10min.json --saida http --url http://localhost:8000 --acelerar 10 --taxa-tel 1
```

O `--taxa-tel 1` é recomendado: com a `tel` a 5 Hz, a versão acelerada passaria de 20 linhas
por segundo e o limitador esticaria a execução para uns 3 min. A 1 Hz são cerca de 1400
linhas, uns 70 s de tempo real, ainda acima dos 60 s do limite.

## Desenvolvimento

```sh
uv run ruff check .
uv run pytest -q
```
