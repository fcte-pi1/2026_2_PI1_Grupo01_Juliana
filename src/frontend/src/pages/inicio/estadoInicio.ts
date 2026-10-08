import type { Execucao, Tentativa } from '../../api/tipos'
import { ALERTA_SEM_DADOS_S, LIMITE_TENTATIVAS } from '../../utils/formatacao'

/**
 * Estado da tela inicial. Junto com o modal aberto e a recusa, cobre os 12
 * estados do protótipo:
 *
 * - sem-execucao: 01
 * - health-check: 02
 * - em-execucao: 03, 05 (com recusa), 07 (modal encerrar), 10 (tentativa 2)
 * - sem-comunicacao: 06
 * - concluida: 04, 11 (após retomada)
 * - falha-retomavel: 08, 09 (modal retomar)
 * - falha-sem-retomada: 12
 */
export type EstadoInicio =
  | 'sem-execucao'
  | 'health-check'
  | 'em-execucao'
  | 'sem-comunicacao'
  | 'concluida'
  | 'falha-retomavel'
  | 'falha-sem-retomada'

export function tentativaAtual(execucao: Execucao): Tentativa | null {
  return execucao.tentativas.reduce<Tentativa | null>(
    (maior, t) => (maior === null || t.attempt_index > maior.attempt_index ? t : maior),
    null,
  )
}

export function segundosSemDados(execucao: Execucao, agora: number): number | null {
  if (!execucao.ultima_mensagem_em) return null
  return Math.max((agora - new Date(execucao.ultima_mensagem_em).getTime()) / 1000, 0)
}

export function podeRetomar(execucao: Execucao): boolean {
  const tentativa = tentativaAtual(execucao)
  return (
    tentativa?.status === 'failed' && execucao.status === 'em_andamento' && execucao.tentativas_usadas < LIMITE_TENTATIVAS
  )
}

export function derivarEstadoInicio(execucao: Execucao | null, agora: number): EstadoInicio {
  if (!execucao) return 'sem-execucao'
  if (execucao.status === 'cancelada') return 'falha-sem-retomada'
  if (execucao.status === 'concluida') return 'concluida'
  const tentativa = tentativaAtual(execucao)
  switch (tentativa?.status ?? 'health-check') {
    case 'health-check':
      return 'health-check'
    case 'running':
      return (segundosSemDados(execucao, agora) ?? 0) > ALERTA_SEM_DADOS_S ? 'sem-comunicacao' : 'em-execucao'
    case 'success':
      return 'em-execucao'
    default:
      return podeRetomar(execucao) ? 'falha-retomavel' : 'falha-sem-retomada'
  }
}
