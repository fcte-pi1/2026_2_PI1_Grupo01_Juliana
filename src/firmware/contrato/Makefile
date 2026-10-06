# Teste do contrato de telemetria no PC (não compila para a ESP32).
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
EXEMPLOS := ../../contrato/exemplos.jsonl

.PHONY: teste limpar

teste: build/teste_telemetria
	./build/teste_telemetria $(EXEMPLOS)

build/teste_telemetria: telemetria.c telemetria.h teste/teste_telemetria.c
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ telemetria.c teste/teste_telemetria.c

limpar:
	rm -rf build
