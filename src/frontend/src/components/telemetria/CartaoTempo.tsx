import { formatarTempo, LIMITE_EXECUCAO_S } from '../../utils/formatacao'

interface CartaoTempoProps {
  rotulo: string
  /** Tempo da execução lógica em segundos (RF13). */
  tempoS: number
  detalhe: string
}

/** Cronômetro com a barra do limite de 10 min por labirinto (RF32). */
export function CartaoTempo({ rotulo, tempoS, detalhe }: CartaoTempoProps) {
  const usado = Math.min(tempoS / LIMITE_EXECUCAO_S, 1)
  return (
    <section className="cartao tempo">
      <div>
        <span className="rotulo">{rotulo}</span>
        <p className="tempo__cronometro mono">{formatarTempo(tempoS)}</p>
        <p className="texto-suave">{detalhe}</p>
      </div>
      <div className="tempo__limite">
        <div className="tempo__linha">
          <span className="rotulo">Limite de 10 min</span>
          <span className="texto-suave">
            restam <strong className="mono">{formatarTempo(Math.max(LIMITE_EXECUCAO_S - tempoS, 0))}</strong>
          </span>
        </div>
        <div className="barra">
          <div className="barra__preenchida" style={{ width: `${usado * 100}%` }} />
        </div>
        <p className="texto-suave">Passou de 10 min: falha por time_exceeded.</p>
      </div>
    </section>
  )
}
