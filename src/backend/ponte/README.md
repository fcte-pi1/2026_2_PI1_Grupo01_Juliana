# Ponte serial Bluetooth (BACK-08)

Processo separado da API. Lê linhas da serial pareada com a ESP32 (ou de um arquivo) e envia cada uma, crua, para `POST /telemetria`. Os `comandos` da resposta — também os de um 422 — voltam para a mesma serial, um JSON por linha. Com `--arquivo` ou pelo stdin, esses comandos saem no terminal. Linhas em branco e as que começam com `#` ficam de fora.

Se o POST não chega ou a API responde 5xx, a linha fica numa fila em memória (no máximo 100) e é reenviada em ordem quando a API volta. Acima de 100, a mais antiga é descartada. O `recebido_em` continua sendo o instante da leitura.

```bash
cd src/backend
python -m ponte --url http://127.0.0.1:8000 --porta /dev/ttyUSB0
python -m ponte --url http://127.0.0.1:8000 --arquivo captura.jsonl
```

`--porta` precisa do `pyserial`. Sem origem, lê o stdin.

`scripts/simular_ponte_telemetria.py` não é esta ponte: ele inventa um trajeto e publica na API quando o firmware não está ligado.
