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

// Rotas provisórias até o OpenAPI do ARQ-02.

export const buscarExecucaoAtual = (sinal?: AbortSignal) =>
  api.get<ExecucaoAtualResposta>('/execucoes/atual', sinal)

export const listarLabirintos = (sinal?: AbortSignal) => api.get<Labirinto[]>('/labirintos', sinal)

export const listarExecucoes = (labirinto: TipoLabirinto | null, sinal?: AbortSignal) =>
  api.get<ResumoExecucao[]>(labirinto ? `/execucoes?labirinto=${labirinto}` : '/execucoes', sinal)

export const buscarExecucao = (id: string, sinal?: AbortSignal) => api.get<Execucao>(`/execucoes/${id}`, sinal)

export const criarExecucao = (pedido: NovaExecucaoPedido) => api.post<Execucao>('/execucoes', pedido)

export const encerrarTentativa = (id: string, pedido: EncerrarPedido) =>
  api.post<Execucao>(`/execucoes/${id}/encerrar`, pedido)

export const retomarTentativa = (id: string) => api.post<Execucao>(`/execucoes/${id}/retomar`)
