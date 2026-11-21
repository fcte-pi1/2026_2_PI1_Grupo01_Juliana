#!/usr/bin/env python3
"""
Monitor serial com comandos por tecla, para testar a placa sem digitar JSON.

Uso: python tools/monitor_comandos.py [porta]      (porta padrão: /dev/ttyACM0)

Teclas: l = largar, i = interromper, h = health_check, t = mostrar/ocultar toda a tel, q = sair.
Por padrão a tel só aparece quando o estado muda; as demais linhas saem sempre.
"""

import re
import select
import sys
import termios
import tty

import serial

COMANDOS = {
    "l": b'{"v":1,"cmd":"largar"}\n',
    "i": b'{"v":1,"cmd":"interromper"}\n',
    "h": b'{"v":1,"cmd":"health_check"}\n',
}
ESTADO = re.compile(rb'"tipo":"tel".*?"estado":"([a-z-]+)"')


def main():
    porta = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
    enlace = serial.Serial()
    enlace.port = porta
    enlace.baudrate = 115200
    enlace.timeout = 0
    if "rfcomm" not in porta:
        # No USB, DTR e RTS em baixa não reiniciam a placa ao abrir; no rfcomm o DTR baixo derruba a conexão.
        enlace.dtr = False
        enlace.rts = False
    enlace.open()

    antigos = termios.tcgetattr(sys.stdin)
    tty.setcbreak(sys.stdin.fileno())
    tudo = False
    ultimo_estado = None
    pendente = b""
    print(f"# {porta}: l=largar i=interromper h=health_check t=tel q=sair", flush=True)
    try:
        while True:
            pronto, _, _ = select.select([sys.stdin, enlace], [], [], 0.2)
            if sys.stdin in pronto:
                tecla = sys.stdin.read(1).lower()
                if tecla == "q":
                    break
                if tecla == "t":
                    tudo = not tudo
                    print(f"# tel completa: {'sim' if tudo else 'nao'}", flush=True)
                elif tecla in COMANDOS:
                    enlace.write(COMANDOS[tecla])
                    print(f"# enviado: {COMANDOS[tecla].decode().strip()}", flush=True)
            if enlace in pronto:
                pendente += enlace.read(4096)
                *linhas, pendente = pendente.split(b"\n")
                for linha in linhas:
                    linha = linha.rstrip(b"\r")
                    achou = ESTADO.search(linha)
                    if achou and not tudo:
                        if achou.group(1) == ultimo_estado:
                            continue
                        ultimo_estado = achou.group(1)
                    print(linha.decode(errors="replace"), flush=True)
    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, antigos)
        enlace.close()


if __name__ == "__main__":
    main()
