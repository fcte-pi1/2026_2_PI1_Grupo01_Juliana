import type { Celula, PassoTrajeto } from '../../api/tipos'
import { nomeCelula } from '../../utils/formatacao'
import './labirinto.css'

const PAREDE_N = 1
const PAREDE_S = 2
const PAREDE_L = 4
const PAREDE_O = 8

interface VisualizacaoLabirintoProps {
  largura: number
  altura: number
  /** Passos na ordem de visita (RF09). */
  trajeto: PassoTrajeto[]
  celulaAtual?: Celula | null
  /** Célula da falha, destacada em vermelho (RF15). */
  celulaFalha?: Celula | null
  /** Esconde eixos e legenda (miniatura). */
  compacto?: boolean
}

/** Grade do labirinto com trajeto, paredes lidas e célula de falha. A1 fica embaixo à esquerda. */
export function VisualizacaoLabirinto({ largura, altura, trajeto, celulaAtual, celulaFalha, compacto }: VisualizacaoLabirintoProps) {
  const ultimoPasso = new Map<string, PassoTrajeto>()
  for (const passo of trajeto) ultimoPasso.set(chave(passo), passo)

  const linhas = Array.from({ length: altura }, (_, i) => altura - 1 - i)
  const colunas = Array.from({ length: largura }, (_, x) => x)
  const objetivo = { x: largura - 1, y: altura - 1 }

  return (
    <figure className={`labirinto ${compacto ? 'labirinto--compacto' : ''}`}>
      <div
        className="labirinto__grade"
        style={{ gridTemplateColumns: `repeat(${largura}, 1fr)`, aspectRatio: `${largura} / ${altura}` }}
        role="img"
        aria-label={`Labirinto ${largura}x${altura} com ${ultimoPasso.size} células visitadas`}
      >
        {linhas.map((y) =>
          colunas.map((x) => {
            const celula = { x, y }
            const passo = ultimoPasso.get(chave(celula))
            const classes = ['labirinto__celula']
            if (passo) classes.push('labirinto__celula--visitada')
            if (passo?.retomada) classes.push('labirinto__celula--retomada')
            if (x === 0 && y === 0) classes.push('labirinto__celula--largada')
            if (igual(celula, objetivo)) classes.push('labirinto__celula--objetivo')
            if (celulaAtual && igual(celula, celulaAtual)) classes.push('labirinto__celula--atual')
            if (celulaFalha && igual(celula, celulaFalha)) classes.push('labirinto__celula--falha')
            return (
              <div key={chave(celula)} className={classes.join(' ')} style={paredes(passo?.paredes_mask ?? 0)} title={nomeCelula(celula)}>
                {!compacto && passo && <span className="labirinto__seq">{passo.seq}</span>}
              </div>
            )
          }),
        )}
      </div>
      {!compacto && (
        <figcaption className="labirinto__legenda">
          <span className="legenda legenda--largada">Largada A1</span>
          <span className="legenda legenda--objetivo">Objetivo {nomeCelula(objetivo)}</span>
          <span className="legenda legenda--visitada">Trajeto</span>
          <span className="legenda legenda--retomada">Retomada</span>
          <span className="legenda legenda--falha">Falha</span>
        </figcaption>
      )}
    </figure>
  )
}

function chave({ x, y }: Celula): string {
  return `${x},${y}`
}

function igual(a: Celula, b: Celula): boolean {
  return a.x === b.x && a.y === b.y
}

function paredes(mascara: number) {
  const borda = (bit: number) => (mascara & bit ? 'var(--cor-labirinto-parede)' : 'transparent')
  return {
    borderTopColor: borda(PAREDE_N),
    borderBottomColor: borda(PAREDE_S),
    borderRightColor: borda(PAREDE_L),
    borderLeftColor: borda(PAREDE_O),
  }
}
