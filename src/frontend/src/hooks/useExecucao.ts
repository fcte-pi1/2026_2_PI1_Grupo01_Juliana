import { useCallback } from 'react'
import { buscarExecucao } from '../api/endpoints'
import { useRequisicao } from './useRequisicao'

export function useExecucao(id: string) {
  const buscar = useCallback((sinal: AbortSignal) => buscarExecucao(id, sinal), [id])
  return useRequisicao(buscar, id)
}
