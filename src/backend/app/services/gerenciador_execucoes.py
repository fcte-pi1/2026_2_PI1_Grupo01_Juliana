"""Máquina de estados da execução lógica e das tentativas (BACK-03)."""


class GerenciadorExecucoes:
    limite_tentativas: int = 3

    def __init__(self, tempo_maximo_execucao_s: int) -> None:
        self.tempo_maximo_execucao_s = tempo_maximo_execucao_s
