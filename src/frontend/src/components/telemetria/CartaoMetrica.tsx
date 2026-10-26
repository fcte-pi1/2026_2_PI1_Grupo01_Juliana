import type { ReactNode } from 'react'

interface CartaoMetricaProps {
  rotulo: string
  valor: string
  unidade?: string
  detalhe?: ReactNode
  /** Percentual de 0 a 100 recebido do backend (ex.: bateria). */
  progresso?: number
}

export function CartaoMetrica({ rotulo, valor, unidade, detalhe, progresso }: CartaoMetricaProps) {
  return (
    <section className="cartao metrica">
      <span className="rotulo">{rotulo}</span>
      <p className="metrica__valor mono">
        {valor}
        {unidade && <span className="metrica__unidade">{unidade}</span>}
      </p>
      {progresso !== undefined && (
        <div
          className="barra"
          role="progressbar"
          aria-label={rotulo}
          aria-valuenow={progresso}
          aria-valuemin={0}
          aria-valuemax={100}
        >
          <div className="barra__preenchida barra__preenchida--sucesso" style={{ width: `${progresso}%` }} />
        </div>
      )}
      {detalhe && <p className="texto-suave">{detalhe}</p>}
    </section>
  )
}
