# Simulador de navegação no PC (FIRM-02). Não compila para a ESP32.
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
LIB := ../lib
INCLUDES := -I../include -I$(LIB)/navegacao -I$(LIB)/simulacao -I$(LIB)/simulador
FONTES := main.c $(LIB)/navegacao/nav.c $(LIB)/simulacao/sim_labirinto.c \
          $(LIB)/simulador/sim_corrida.c $(LIB)/simulador/sim_roteiro.c
CABECALHOS := $(wildcard ../include/config/*.h $(LIB)/navegacao/*.h $(LIB)/simulacao/sim_labirinto.h $(LIB)/simulador/*.h)

.PHONY: relatorio limpar

relatorio: build/simulador
	./build/simulador labirintos/*.txt

build/simulador: $(FONTES) $(CABECALHOS)
	mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(FONTES)

limpar:
	rm -rf build
