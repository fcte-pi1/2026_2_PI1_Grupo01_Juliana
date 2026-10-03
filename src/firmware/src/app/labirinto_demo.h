#ifndef APP_LABIRINTO_DEMO_H
#define APP_LABIRINTO_DEMO_H

/*
 * Labirinto 4x4 usado pela HAL simulada na ESP32 (lá não há arquivos para ler).
 * Largada em (0, 0), canto inferior esquerdo, com saída só para o norte.
 */
static const char LABIRINTO_DEMO[] =
    "+---+---+---+---+\n"
    "|           |   |\n"
    "+   +---+   +   +\n"
    "|   |       |   |\n"
    "+   +   +---+   +\n"
    "|   |   |       |\n"
    "+   +---+   +---+\n"
    "|   |           |\n"
    "+---+---+---+---+\n";

#endif /* APP_LABIRINTO_DEMO_H */
