import { memo, useMemo } from 'react'
import type { Celula, PassoTrajeto } from '../../api/tipos'
import { nomeCelula } from '../../utils/formatacao'
import { celulasDeRetomada, deduzirEixo, dimensoes, dividirEmTrechos, eixoParaGrade, type EixoLongo, type FalhaNoMapa } from './trajeto'
import './labirinto.css'

const PAREDE_N = 1
const PAREDE_S = 2
const PAREDE_L = 4
const PAREDE_O = 8

/** Lado da célula em unidades do viewBox. */
const LADO = 100
const FOLGA = 6
const MARGEM = 20
const MARGEM_EIXO = 36

interface VisualizacaoLabirintoProps {
  largura: number
  altura: number
  /**
   * Lado em que o labirinto se estende (4.4, "Orientação"), vindo do `eixo_longo` do backend.
   * Sem ele, o 8x4 e o 12x4 usam o lado longo na horizontal, no tamanho escolhido, até o trajeto indicar o contrário.
   */
  eixoLongo?: EixoLongo | null
  /** Passos na ordem de visita (RF09). A cada evento `passo`, o trajeto cresce um ponto. */
  trajeto: PassoTrajeto[]
  /** Posição do robô antes do primeiro passo; depois, o marcador segue o último passo. */
  celulaAtual?: Celula | null
  /** Posição reconciliada pelo REST/SSE, inclusive uma falha após o último passo gravado. */
  posicaoAtual?: Celula | null
  /** Células de falha de cada tentativa, destacadas em vermelho (RF15). */
  falhas?: FalhaNoMapa[]
  /** Esconde eixos, rótulos e legenda (miniatura). */
  compacto?: boolean
  /** Trajeto de uma única retomada (detalhe por tentativa): desenha tudo como retomada. */
  soRetomada?: boolean
  /** Onde começou a retomada mostrada sozinha (detalhe por tentativa). Sem ele, nada muda. */
  inicioRetomada?: Celula | null
  /** Se o robô terminou em falha, quando quem chama já sabe (detalhe). Sem ele, deduz pelas falhas e trechos. */
  roboFalhou?: boolean
}

/** Labirinto em SVG com trajeto, retomadas e falha. A1 fica embaixo à esquerda. Usado ao vivo e no detalhe. */
export function VisualizacaoLabirinto({
  largura,
  altura,
  eixoLongo,
  trajeto,
  celulaAtual,
  posicaoAtual,
  falhas = [],
  compacto = false,
  soRetomada = false,
  inicioRetomada = null,
  roboFalhou: falhouInformado,
}: VisualizacaoLabirintoProps) {
  const eixo = eixoParaGrade(largura, altura, eixoLongo ?? deduzirEixo(trajeto, Math.min(largura, altura)))
  const { colunas, linhas } = dimensoes(largura, altura, eixo)
  const objetivo = { x: colunas - 1, y: linhas - 1 }

  const ultimoPasso = useMemo(() => {
    const mapa = new Map<string, PassoTrajeto>()
    for (const passo of trajeto) mapa.set(chave(passo), passo)
    return mapa
  }, [trajeto])
  const trechos = useMemo(() => dividirEmTrechos(trajeto), [trajeto])
  const retomadas = useMemo(() => celulasDeRetomada(trechos, falhas), [trechos, falhas])

  const margemEsq = compacto ? MARGEM : MARGEM + MARGEM_EIXO
  const margemBaixo = compacto ? MARGEM : MARGEM + MARGEM_EIXO
  const larguraSvg = margemEsq + colunas * LADO + MARGEM
  const alturaSvg = MARGEM + linhas * LADO + margemBaixo
  const centro = (c: Celula) => ({ cx: margemEsq + c.x * LADO + LADO / 2, cy: MARGEM + (linhas - 1 - c.y) * LADO + LADO / 2 })
  const canto = (c: Celula) => ({ x: margemEsq + c.x * LADO, y: MARGEM + (linhas - 1 - c.y) * LADO })

  const destaques: Destaque[] = [
    { chave: 'largada', classe: 'largada', celula: { x: 0, y: 0 }, rotulo: 'LARGADA' },
    ...(objetivo ? [{ chave: 'objetivo', classe: 'objetivo' as const, celula: objetivo, rotulo: 'OBJETIVO' }] : []),
    ...falhas.map((f) => ({ chave: `f${f.tentativa}`, classe: 'falha' as const, celula: f, rotulo: `FALHA T${f.tentativa}` })),
    ...retomadas.map((r) => ({ chave: `r${r.tentativa}`, classe: 'retomada' as const, celula: r, rotulo: 'RETOMADA' })),
    ...(inicioRetomada ? [{ chave: 'inicio', classe: 'retomada' as const, celula: inicioRetomada, rotulo: 'RETOMADA' }] : []),
  ]

  const robo = posicaoAtual ?? trajeto.at(-1) ?? celulaAtual ?? null
  // cada falha fecha um trecho; sem retomada depois dela, o robô parou ali
  const roboFalhou = falhouInformado ?? (trechos.length > 0 && falhas.length >= trechos.length)
  const novo = trajeto.length >= 2 && !trajeto.at(-1)!.retomada ? trajeto.slice(-2) : null

  return (
    <figure className={`labirinto ${compacto ? 'labirinto--compacto' : ''}`}>
      <svg
        className="labirinto__svg"
        viewBox={`0 0 ${larguraSvg} ${alturaSvg}`}
        role="img"
        aria-label={`Labirinto ${largura}x${altura} com ${ultimoPasso.size} células visitadas`}
      >
        <rect className="labirinto__fundo" width={larguraSvg} height={alturaSvg} rx={18} />

        <Grade colunas={colunas} linhas={linhas} margemEsq={margemEsq} compacto={compacto} />

        {destaques.map((d) => (
          <CelulaDestaque key={d.chave} {...d} canto={canto} />
        ))}

        {!compacto &&
          Array.from(ultimoPasso.values(), (p) => {
            const { x, y } = canto(p)
            return (
              <g key={chave(p)}>
                <title>{`${nomeCelula(p)} · passo ${p.seq}`}</title>
                <Paredes x={x} y={y} mascara={p.paredes_mask} />
              </g>
            )
          })}

        {trechos.map((trecho, i) => (
          <polyline
            key={i}
            className={`labirinto__trajeto ${i > 0 || soRetomada ? 'labirinto__trajeto--retomada' : ''}`}
            points={trecho.map((p) => `${centro(p).cx},${centro(p).cy}`).join(' ')}
          />
        ))}
        {novo && (
          <line
            key={novo[1].seq}
            className={`labirinto__trajeto labirinto__trajeto--novo ${trechos.length > 1 || soRetomada ? 'labirinto__trajeto--retomada' : ''}`}
            x1={centro(novo[0]).cx}
            y1={centro(novo[0]).cy}
            x2={centro(novo[1]).cx}
            y2={centro(novo[1]).cy}
          />
        )}
        {!compacto &&
          trechos.map((trecho, i) =>
            trecho.map((p) => (
              <circle key={`${i}-${p.seq}`} className="labirinto__no" {...centro(p)} r={7} />
            )),
          )}

        {!compacto && destaques.map((d) => <Rotulo key={d.chave} {...d} canto={canto} />)}

        {robo && (
          <circle
            className={`labirinto__robo ${roboFalhou ? 'labirinto__robo--falha' : ''}`}
            r={compacto ? 22 : 16}
            style={{ transform: `translate(${centro(robo).cx}px, ${centro(robo).cy}px)` }}
          >
            <title>{`Robô em ${nomeCelula(robo)}${roboFalhou ? ' · falhou' : ''}`}</title>
          </circle>
        )}
      </svg>
      {!compacto && (
        <figcaption className="labirinto__legenda">
          {!soRetomada && <span className="legenda legenda--trajeto">Tentativa 1</span>}
          <span className="legenda legenda--retomada">{inicioRetomada ? `Retomada em ${nomeCelula(inicioRetomada)}` : 'Retomada'}</span>
          <span className="legenda legenda--largada">Largada A1</span>
          <span className="legenda legenda--objetivo">Objetivo {nomeCelula(objetivo)}</span>
          <span className="legenda legenda--falha">Falha</span>
        </figcaption>
      )}
    </figure>
  )
}

interface GradeProps {
  colunas: number
  linhas: number
  margemEsq: number
  compacto: boolean
}

/** Células vazias e eixos. Só muda com o tamanho, então não redesenha a cada passo. */
const Grade = memo(function Grade({ colunas, linhas, margemEsq, compacto }: GradeProps) {
  const celulas = []
  for (let y = 0; y < linhas; y++) {
    for (let x = 0; x < colunas; x++) {
      celulas.push(
        <rect
          key={`${x},${y}`}
          className="labirinto__celula"
          x={margemEsq + x * LADO + FOLGA}
          y={MARGEM + (linhas - 1 - y) * LADO + FOLGA}
          width={LADO - 2 * FOLGA}
          height={LADO - 2 * FOLGA}
          rx={8}
        />,
      )
    }
  }
  return (
    <g>
      {celulas}
      {!compacto && (
        <g className="labirinto__eixo">
          {Array.from({ length: colunas }, (_, x) => (
            <text key={`c${x}`} x={margemEsq + x * LADO + LADO / 2} y={MARGEM + linhas * LADO + MARGEM_EIXO - 6}>
              {String.fromCharCode(65 + x)}
            </text>
          ))}
          {Array.from({ length: linhas }, (_, y) => (
            <text key={`l${y}`} x={MARGEM + MARGEM_EIXO / 2} y={MARGEM + (linhas - 1 - y) * LADO + LADO / 2 + 8}>
              {y + 1}
            </text>
          ))}
        </g>
      )}
    </g>
  )
})

interface Destaque {
  chave: string
  classe: 'largada' | 'objetivo' | 'retomada' | 'falha'
  celula: Celula
  rotulo: string
}

type Canto = (c: Celula) => { x: number; y: number }

function CelulaDestaque({ classe, celula, canto }: Destaque & { canto: Canto }) {
  const { x, y } = canto(celula)
  return (
    <rect
      className={`labirinto__destaque labirinto__destaque--${classe}`}
      x={x + FOLGA}
      y={y + FOLGA}
      width={LADO - 2 * FOLGA}
      height={LADO - 2 * FOLGA}
      rx={8}
    />
  )
}

/** Como no protótipo: objetivo e falha no alto da célula, largada e retomada embaixo, sem se cobrirem. */
const ROTULO_EM_CIMA = new Set(['objetivo', 'falha'])
const LARGURA_LETRA = 8.6

/** Etiqueta com fundo, desenhada por cima do trajeto para a linha não passar sobre o texto. */
function Rotulo({ classe, celula, rotulo, canto }: Destaque & { canto: Canto }) {
  const { x, y } = canto(celula)
  const largura = rotulo.length * LARGURA_LETRA + 12
  const topo = ROTULO_EM_CIMA.has(classe) ? y + FOLGA + 4 : y + LADO - FOLGA - 26
  return (
    <g className={`labirinto__rotulo labirinto__rotulo--${classe}`}>
      <rect x={x + (LADO - largura) / 2} y={topo} width={largura} height={22} rx={5} />
      <text x={x + LADO / 2} y={topo + 16}>
        {rotulo}
      </text>
    </g>
  )
}

function Paredes({ x, y, mascara }: { x: number; y: number; mascara: number }) {
  const fim = LADO
  return (
    <g className="labirinto__parede">
      {mascara & PAREDE_N ? <line x1={x} y1={y} x2={x + fim} y2={y} /> : null}
      {mascara & PAREDE_S ? <line x1={x} y1={y + fim} x2={x + fim} y2={y + fim} /> : null}
      {mascara & PAREDE_L ? <line x1={x + fim} y1={y} x2={x + fim} y2={y + fim} /> : null}
      {mascara & PAREDE_O ? <line x1={x} y1={y} x2={x} y2={y + fim} /> : null}
    </g>
  )
}

function chave({ x, y }: Celula): string {
  return `${x},${y}`
}
