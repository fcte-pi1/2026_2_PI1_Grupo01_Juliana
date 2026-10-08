import { listarExecucoes } from '../api/endpoints'
import type { TipoLabirinto } from '../api/tipos'
import { useRequisicao } from './useRequisicao'

export function useExecucoes(labirinto: TipoLabirinto | null) {
  return useRequisicao((sinal) => listarExecucoes(labirinto, sinal), labirinto ?? 'todos')
}
