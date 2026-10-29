# Ponte serial Bluetooth (BACK-08)

Processo que roda no notebook, fora do Docker, e liga o Micromouse ao Sistema de Telemetria. O contrato está em `docs/4.4.1 - Contrato de telemetria.md`, seções "Comando da web para o robô" e "Ponte e API".

## O que faz

- Abre a porta serial do SPP do robô (`/dev/rfcomm0` no Linux, `COMx` no Windows), lê linha a linha e anota em `recebido_em` a hora local, com fuso, em que o `\n` chegou.
- Descarta a primeira linha depois de abrir a porta (pode vir pela metade).
- Envia cada linha não vazia, sem alteração e em ordem de leitura, para `POST /telemetria` (`EntradaPonte`).
- Escreve na serial os itens de `comandos` da resposta (`RespostaPonte`, também presentes no 422 de linha inválida), um por linha, em JSON compacto terminado em `\n`. Só escreve o que valida como `Comando` do contrato; o resto vai para o log como recusado.
- Reabre a porta e reenvia para a API sozinha, sem reiniciar, com esperas de 0,5 s, 1 s, 2 s e depois fixas em 2 s, que voltam a 0,5 s no primeiro sucesso.
- Guarda as linhas numa fila em memória enquanto a API está fora do ar.
- Registra num arquivo de log tudo que recebe, escreve e cada falha (RNF-B06).

## O que não faz

- Não interpreta nem valida a telemetria: toda a validação fica no backend. A única exceção é olhar o `tipo` da linha para escolher o que descartar com a fila cheia.
- Não pareia o Bluetooth (isso fica com o sistema operacional, ver abaixo).
- Não guarda a fila em disco: no Ctrl+C ou num reinício, as linhas pendentes se perdem (o número fica no log).
- Não roda dentro do Docker.

## Instalação

Na pasta `src/backend`, num ambiente virtual:

```sh
pip install -e ".[ponte]"
```

O extra `ponte` traz `pyserial` e `httpx`; a imagem Docker da API não muda.

## Configuração

Lida do ambiente e do `.env` da pasta em que o processo roda. A opção da linha de comando vence a variável.

| Variável | Opção | Padrão | Descrição |
| --- | --- | --- | --- |
| `API_URL` | `--url` | `http://localhost:8000` | URL base da API. |
| `PONTE_PORTA_SERIAL` | `--porta` | `/dev/rfcomm0` | Porta serial do SPP (`COM5`, por exemplo, no Windows). |
| `PONTE_FILA_MAXIMA` | `--fila` | `100` | Máximo de linhas na fila. Menor que 1 encerra com código 1. |
| `PONTE_LOG_DIR` | `--log-dir` | `logs/ponte` | Pasta do log de auditoria (criada se não existir). |

```sh
python -m ponte
python -m ponte --porta /dev/pts/5 --url http://localhost:8000
```

Ao iniciar, a ponte imprime no stderr a porta, a URL e o caminho do log. Ctrl+C encerra na hora, com código 0.

## Porta serial do robô

**Linux.** Pareie o robô pelo `bluetoothctl` (`pair <MAC>` e `trust <MAC>`) e associe a porta:

```sh
sudo rfcomm bind 0 <MAC>
```

A porta fica em `/dev/rfcomm0`, o padrão da ponte. Para desfazer: `sudo rfcomm release 0`.

**Windows.** Pareie o robô nas configurações de Bluetooth. Em "Mais opções de Bluetooth", aba "Portas COM", fica a porta de saída do SPP (por exemplo `COM5`); use-a em `--porta` ou `PONTE_PORTA_SERIAL`.

## Fila e descarte

A fila guarda no máximo `PONTE_FILA_MAXIMA` linhas (100 são 10 s a 10 mensagens por segundo). A linha só sai da fila depois que a API responde 2xx ou 4xx; erro de conexão, timeout ou 5xx deixam a linha no início da fila, e ela é reenviada até passar. O envio é sempre em ordem de chegada.

Com a fila cheia, para caber a linha nova, a ponte descarta, nesta ordem:

1. a linha não classificável mais antiga (não é um objeto JSON com `tipo` texto);
2. a `tel` mais antiga;
3. o evento mais antigo (qualquer outro `tipo`).

Cada descarte vai para o log como `FILA`. A linha nunca é alterada.

## Log de auditoria

Um arquivo por execução: `PONTE_LOG_DIR/ponte-AAAAMMDD-HHMMSS.log`, com a hora local de início. Qualquer pasta `logs/ponte/` é ignorada pelo git. Cada linha tem a hora ISO com fuso e milissegundos, o tipo e o conteúdo cru:

```text
2026-10-21T09:05:07.123-03:00 CONEXAO aberta /dev/rfcomm0
2026-10-21T09:05:07.456-03:00 RX {"v":1,"boot":7,"seq":42,"t_ms":11250,"tipo":"tel","estado":"running",...}
2026-10-21T09:05:07.470-03:00 API 200 comandos=0
2026-10-21T09:05:09.002-03:00 API 200 comandos=1
2026-10-21T09:05:09.003-03:00 TX {"v":1,"cmd":"interromper"}
```

| Tipo | Quando |
| --- | --- |
| `RX` | Linha lida da serial. |
| `TX` | Comando escrito na serial. |
| `API` | Resposta da API (`<status> comandos=<n>`), falha (`falhou ...`), comando recusado (`comando recusado ...`) ou erro inesperado na enviadora (`erro inesperado ...`), que espera e tenta de novo. |
| `CONEXAO` | Porta aberta, porta caída (inclusive por erro inesperado na leitora, que reabre a porta), comando perdido sem porta ou erro na escrita, e o encerramento com o número de linhas descartadas da fila. |
| `FILA` | Linha descartada com a fila cheia (`descartada <classe> <linha>`). |

## Ensaio de 10 min com o simulador

Roteiro do caso 4.16 para conferir a cadeia simulador, ponte, API e banco sem o robô (Linux, macOS ou WSL). Os comandos do `ensaio_serial` rodam em `src/backend`, com o extra `dev` (traz o `httpx`).

1. Suba a API e o banco (ver `src/backend/README.md`).
2. Em `src/backend`, deixe rodando em outro terminal o comando que abre a execução, como o operador faria clicando em **Nova execução**:

   ```sh
   python -m scripts.ensaio_serial abrir --url http://localhost:8000 --labirinto 4x4
   ```

   Ele espera um health-check concluído depois do seu início (até `--timeout`, 60 s por padrão) e então faz `POST /execucoes`. Imprime `execucao_id`, `tentativa_id` e `aberta_em` e sai com 0; com 409 ou sem health-check novo, sai com 1. Rode-o antes do simulador, porque o simulador corre logo depois do `hc_resultado`, sem esperar o `largar`.
3. Em `src/simulador-telemetria`, rode o roteiro longo em pty, com o stderr salvo em arquivo:

   ```sh
   mkdir -p /tmp/ensaio
   uv run python -m simulador roteiros/estouro-10min.json --saida pty 2> /tmp/ensaio/simulador.log
   ```

   O caminho da porta (por exemplo `/dev/pts/5`) fica no próprio arquivo (`grep "porta serial virtual" /tmp/ensaio/simulador.log`), e o simulador espera alguém abrir. O arquivo também guarda uma linha `simulador: enviada: <linha JSON>` por mensagem, que o `conferir` usa.
4. Em `src/backend`, rode a ponte nessa porta, com o log numa pasta só do ensaio:

   ```sh
   python -m ponte --porta /dev/pts/5 --url http://localhost:8000 --log-dir /tmp/ensaio/ponte
   ```

5. Espere o simulador terminar (cerca de 10 min 15 s). A tentativa é encerrada pelo backend com `time_exceeded` 600 s depois da abertura.
6. Encerre a ponte com Ctrl+C.
7. Em `src/backend`, confira o ensaio, com `DATABASE_URL` apontando para o banco da API (fora do Docker, use `localhost` e a porta publicada do banco, não o host `db` do `.env`):

   ```sh
   python -m scripts.ensaio_serial conferir \
     --log-simulador /tmp/ensaio/simulador.log \
     --log-ponte /tmp/ensaio/ponte/ponte-AAAAMMDD-HHMMSS.log \
     --csv /tmp/ensaio/janelas.csv
   ```

   O comando imprime as contagens e termina em `APROVADO` (código 0) ou `REPROVADO` (código 1). Ele confere:

   - na ponte: enviadas igual a RX mais 1, porque a ponte descarta a primeira linha depois de abrir a porta e não registra o descarte; as demais enviadas iguais aos RX, no mesmo texto e na mesma ordem; uma resposta `API` por RX, todas 200; nenhum `API falhou` nem `FILA`; todo `TX` igual a um comando do contrato (`interromper`, `largar` ou `health_check`);
   - no banco: a tentativa do health-check com o `boot` do ensaio, já encerrada; dentro da janela da tentativa (da abertura ao encerramento), todo `passo` gravado em `passo_trajeto` e uma `leitura_telemetria` por `tel`, e o health-check com 8 itens. As mensagens antes da abertura e depois do encerramento recebem 200 e não são gravadas; aparecem no relatório, mas não contam como perda. RX a menos de 50 ms das bordas da janela ficam à parte, porque o relógio da ponte e o do backend não são o mesmo;
   - a duração do ensaio, do primeiro ao último RX, de pelo menos 600 s.

   Com `--csv`, grava as contagens por janela de 10 s (`janela_inicio_s,enviadas,rx,gravadas`). O gráfico sai com:

   ```sh
   uv run --extra graficos python -m scripts.grafico_ensaio_serial /tmp/ensaio/janelas.csv /tmp/ensaio/serial.png
   ```

Para um ensaio mais curto, troque o roteiro por `roteiros/sucesso-4x4.json` (o `conferir` reprova pela duração, mas as demais conferências valem).

### Queda da API

Roteiro à parte, manual. Rode de novo o simulador (passo 3, sem redirecionar o stderr se quiser ver a porta no terminal) e a ponte (passo 4), **sem** rodar o `abrir` nem clicar em **Nova execução**. Derrube a API por cerca de 5 s e suba de novo. No log, confira que nenhuma linha se perdeu: toda linha `RX` tem uma linha `API <status> comandos=<n>` correspondente, sem `FILA descartada`. Sem tentativa aberta, o backend responde 200 e descarta as linhas só no log dele, o que basta para conferir a ponte. Não derrube a API com uma tentativa aberta: reiniciar a API no meio dela a encerra com `link_lost` (issue #229).
