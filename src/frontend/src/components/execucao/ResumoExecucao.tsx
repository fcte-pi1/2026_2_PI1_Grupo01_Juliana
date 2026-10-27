import type { TipoLabirinto } from '../../api/tipos'
import { formatarDataHora } from '../../utils/formatacao'
import { ContadorTentativas } from '../tentativas/ContadorTentativas'

interface ResumoExecucaoProps {
  numero: string
  labirinto: TipoLabirinto
  attemptIndex: number
  criadaEm: string
  largada: string
  objetivo: string
  tempoHealthCheck?: string
}

/** Faixa de chips no topo da execução: número, labirinto, tentativa n/3, horários. */
export function ResumoExecucao({ numero, labirinto, attemptIndex, criadaEm, largada, objetivo, tempoHealthCheck }: ResumoExecucaoProps) {
  return (
    <section className="cartao resumo-execucao">
      <strong className="resumo-execucao__numero mono">{numero}</strong>
      <span className="chip">Labirinto {labirinto}</span>
      <ContadorTentativas atual={attemptIndex} />
      <span className="chip">Criada {formatarDataHora(criadaEm)}</span>
      <span className="chip">
        {largada} → {objetivo}
      </span>
      {tempoHealthCheck && <span className="resumo-execucao__tempo"><span className="rotulo">Em health-check há </span><strong className="mono">{tempoHealthCheck}</strong></span>}
    </section>
  )
}
