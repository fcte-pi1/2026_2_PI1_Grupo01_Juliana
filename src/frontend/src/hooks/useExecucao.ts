import { buscarExecucao } from '../api/endpoints'
import { useRequisicao } from './useRequisicao'

export function useExecucao(id: string) {
  return useRequisicao((sinal) => buscarExecucao(id, sinal), id)
}
