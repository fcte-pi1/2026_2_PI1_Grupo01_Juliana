import { listarLabirintos } from '../api/endpoints'
import { useRequisicao } from './useRequisicao'

export function useLabirintos() {
  return useRequisicao(listarLabirintos, 'labirintos')
}
