import { api } from './clienteApi'
import type {
  EncerrarPedido,
  Execucao,
  ExecucaoAtualResposta,
  Labirinto,
  NovaExecucaoPedido,
  ResumoExecucao,
  TipoLabirinto,
} from './tipos'

// Rotas provisórias — alinhar ao openapi.yaml na tarefa FRONT-02 (ver src/frontend/README.md).

export const buscarExecucaoAtual = (sinal?: AbortSignal) =>
  api.get<ExecucaoAtualResposta>('/execucoes/atual', sinal)

export const listarLabirintos = (sinal?: AbortSignal) => api.get<Labirinto[]>('/labirintos', sinal)

export const listarExecucoes = (labirinto: TipoLabirinto | null, sinal?: AbortSignal) =>
  api.get<ResumoExecucao[]>(
    labirinto ? `/execucoes?tipo_labirinto=${labirinto}` : '/execucoes',
    sinal,
  )

export const buscarExecucao = (id: string, sinal?: AbortSignal) => api.get<Execucao>(`/execucoes/${id}`, sinal)

export const criarExecucao = (pedido: NovaExecucaoPedido) => api.post<Execucao>('/execucoes', pedido)

export const encerrarTentativa = (id: string, pedido: EncerrarPedido) =>
  api.post<Execucao>(`/execucoes/${id}/encerrar`, pedido)

export const retomarTentativa = (id: string) => api.post<Execucao>(`/execucoes/${id}/retomar`)
