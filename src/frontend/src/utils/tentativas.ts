import type { Execucao, Tentativa } from '../api/tipos'
import { LIMITE_TENTATIVAS } from './formatacao'

export function tentativaAtual(execucao: Execucao): Tentativa | null {
  return execucao.tentativas.reduce<Tentativa | null>(
    (maior, tentativa) => maior === null || tentativa.attempt_index > maior.attempt_index ? tentativa : maior,
    null,
  )
}

export function tentativaAberta(execucao: Execucao): Tentativa | null {
  if (execucao.status !== 'em_andamento') return null
  return execucao.tentativas.find((tentativa) => tentativa.status === 'running') ?? null
}

/** US07: retomar somente após falha, sem tentativa aberta e com chances disponíveis. */
export function podeRetomar(execucao: Execucao): boolean {
  const tentativa = tentativaAtual(execucao)
  return execucao.status === 'em_andamento' && tentativa?.status === 'failed'
    && tentativa.attempt_index < LIMITE_TENTATIVAS && execucao.tentativas_usadas < LIMITE_TENTATIVAS
    && tentativaAberta(execucao) === null
}
