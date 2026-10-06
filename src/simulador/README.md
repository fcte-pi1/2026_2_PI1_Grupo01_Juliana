# Simulador de telemetria

Faz o papel do Micromouse: lê um roteiro JSON e emite as mensagens do contrato de
telemetria v1 (`docs/4.4.1 - Contrato de telemetria.md`), uma linha JSON por mensagem.
Serve para testar a ponte, o backend e o front sem o robô. As mensagens vêm de
`contrato.telemetria`, do backend, então o simulador nunca sai do contrato.

## Instalação

Precisa do [uv](https://docs.astral.sh/uv/) e do Python 3.12 ou mais novo.

```sh
cd src/simulador
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
| `http` | Em breve: envia cada linha para `POST /telemetria` da API, sem a ponte. Hoje termina com código 1. |

### Opções

| Opção | Padrão | Efeito |
| --- | --- | --- |
| `roteiro` | | Arquivo JSON do cenário (formato na seção 6.1 de `tasks/prd-simulador-telemetria.md`). |
| `--saida stdout\|pty\|http` | `stdout` | Para onde as linhas vão. |
| `--url URL` | | URL da API, para `--saida http`. |
| `--acelerar N` | `1` | Divide as esperas e o `t_ms` por N. |
| `--sem-espera` | | Emite tudo de uma vez, sem dormir. |
| `--taxa-tel HZ` | `5` | Taxa da `tel` em movimento, de 1 a 20 Hz; fora disso, código 1. |
| `--boot N` | `0` | Número do boot da primeira tentativa. |

Mesmo acelerado, o simulador nunca passa de 20 linhas por segundo de tempo real.

## Configuração

O simulador usa o mesmo `.env` do backend. Copie o exemplo e ajuste o que precisar:

```sh
cp src/backend/.env.example src/backend/.env
```

Para carregar o arquivo, use o `--env-file` do uv (não há python-dotenv):

```sh
cd src/simulador
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

## Desenvolvimento

```sh
uv run ruff check .
uv run pytest -q
```
