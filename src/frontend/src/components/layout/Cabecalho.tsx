import type { ReactNode } from 'react'

interface CabecalhoProps {
  trilha: string
  titulo: string
  /** Pílulas de status e botões à direita. */
  acoes?: ReactNode
}

export function Cabecalho({ trilha, titulo, acoes }: CabecalhoProps) {
  return (
    <header className="cabecalho">
      <div>
        <span className="rotulo">{trilha}</span>
        <h1 className="cabecalho__titulo">{titulo}</h1>
      </div>
      {acoes && <div className="cabecalho__acoes">{acoes}</div>}
    </header>
  )
}
