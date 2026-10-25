import { useCallback } from 'react'
import { listarTodasExecucoes } from '../api/endpoints'
import type { TipoLabirinto } from '../api/tipos'
import { useRequisicao } from './useRequisicao'

export function useExecucoes(labirinto: TipoLabirinto | null) {
  const chave = labirinto ?? 'todos'
  const buscar = useCallback((sinal: AbortSignal) => listarTodasExecucoes(labirinto, sinal), [labirinto])
  return useRequisicao(buscar, chave)
}
