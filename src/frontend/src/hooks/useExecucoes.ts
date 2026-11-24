import { useCallback } from 'react'
import { listarTodasExecucoes } from '../api/endpoints'
import type { TipoLabirinto } from '../api/tipos'
import { useRequisicao } from './useRequisicao'

export function useExecucoes(labirinto: TipoLabirinto | null, configuracao: string | null = null) {
  const chave = `${labirinto ?? 'todos'}|${configuracao ?? ''}`
  const buscar = useCallback((sinal: AbortSignal) => listarTodasExecucoes(labirinto, sinal, configuracao), [labirinto, configuracao])
  return useRequisicao(buscar, chave)
}
