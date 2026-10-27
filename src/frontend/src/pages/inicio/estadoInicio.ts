import type { Execucao } from '../../api/tipos'
import { ALERTA_SEM_DADOS_S } from '../../utils/formatacao'
import { podeRetomar, tentativaAtual } from '../../utils/tentativas'

export { podeRetomar, tentativaAtual } from '../../utils/tentativas'

/**
 * Estado da tela inicial. Junto com o modal aberto e a recusa, cobre os 12
 * estados do protótipo:
 *
 * - sem-execucao: 01, 02 (health-check anterior à tentativa)
 * - aguardando-tentativa: detalhe da execução ainda sem tentativa
 * - em-execucao: 03, 05 (com recusa), 07 (modal encerrar), 10 (tentativa 2)
 * - sem-comunicacao: 06
 * - concluida: 04, 11 (após retomada)
 * - falha-retomavel: 08, 09 (modal retomar)
 * - falha-sem-retomada: 12
 */
export type EstadoInicio =
  | 'sem-execucao'
  | 'aguardando-tentativa'
  | 'em-execucao'
  | 'sem-comunicacao'
  | 'concluida'
  | 'falha-retomavel'
  | 'falha-sem-retomada'

export function segundosSemDados(execucao: Execucao, agora: number): number | null {
  const ultima = execucao.ultima_atualizacao_em ?? (execucao.ultima_mensagem_em ? Date.parse(execucao.ultima_mensagem_em) : null)
  if (ultima === null || !Number.isFinite(ultima)) return null
  return Math.max((agora - ultima) / 1000, 0)
}

export function derivarEstadoInicio(execucao: Execucao | null, agora: number): EstadoInicio {
  if (!execucao) return 'sem-execucao'
  if (execucao.status === 'cancelada') return 'falha-sem-retomada'
  if (execucao.status === 'concluida') return 'concluida'
  const tentativa = tentativaAtual(execucao)
  switch (tentativa?.status) {
    case 'running':
      return (segundosSemDados(execucao, agora) ?? 0) >= ALERTA_SEM_DADOS_S ? 'sem-comunicacao' : 'em-execucao'
    case 'success':
      return 'em-execucao'
    default:
      if (!tentativa) return 'aguardando-tentativa'
      return podeRetomar(execucao) ? 'falha-retomavel' : 'falha-sem-retomada'
  }
}
