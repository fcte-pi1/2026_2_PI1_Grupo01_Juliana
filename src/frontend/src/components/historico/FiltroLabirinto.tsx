import type { TipoLabirinto } from '../../api/tipos'

const OPCOES: Array<{ valor: TipoLabirinto | null; rotulo: string }> = [
  { valor: null, rotulo: 'Todos' },
  { valor: '4x4', rotulo: '4x4' },
  { valor: '8x4', rotulo: '8x4' },
  { valor: '12x4', rotulo: '12x4' },
]

interface FiltroLabirintoProps {
  valor: TipoLabirinto | null
  aoMudar: (valor: TipoLabirinto | null) => void
}

export function FiltroLabirinto({ valor, aoMudar }: FiltroLabirintoProps) {
  return (
    <div className="filtro" role="group" aria-label="Filtrar por labirinto">
      {OPCOES.map((opcao) => (
        <button
          key={opcao.rotulo}
          type="button"
          className={`filtro__opcao ${valor === opcao.valor ? 'filtro__opcao--ativa' : ''}`}
          aria-pressed={valor === opcao.valor}
          onClick={() => aoMudar(opcao.valor)}
        >
          {opcao.rotulo}
        </button>
      ))}
    </div>
  )
}
