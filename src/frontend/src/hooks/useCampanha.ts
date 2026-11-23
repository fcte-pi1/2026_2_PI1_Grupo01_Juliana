import { useCallback } from 'react'
import { buscarCampanha } from '../api/endpoints'
import type { TipoLabirinto } from '../api/tipos'
import { useRequisicao } from './useRequisicao'

/** Resumo da campanha para o filtro do histórico. */
export function useCampanha(labirinto: TipoLabirinto | null, configuracao: string | null = null) {
  const chave = `${labirinto ?? 'todos'}|${configuracao ?? ''}`
  const buscar = useCallback((sinal: AbortSignal) => buscarCampanha(labirinto, sinal, configuracao), [labirinto, configuracao])
  return useRequisicao(buscar, chave)
}
