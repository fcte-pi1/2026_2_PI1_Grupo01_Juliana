import { useCallback } from 'react'
import { listarExecucoes } from '../api/endpoints'
import type { TipoLabirinto } from '../api/tipos'
import { useRequisicao } from './useRequisicao'

export function useExecucoes(labirinto: TipoLabirinto | null) {
  const chave = labirinto ?? 'todos'
  const buscar = useCallback((sinal: AbortSignal) => listarExecucoes(labirinto, sinal), [labirinto])
  return useRequisicao(buscar, chave)
}
