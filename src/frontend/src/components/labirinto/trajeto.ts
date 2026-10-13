import type { Celula, PassoTrajeto, Tentativa } from '../../api/tipos'

export type EixoLongo = 'x' | 'y'

/** Célula onde uma tentativa falhou, com o número da tentativa para o rótulo. */
export interface FalhaNoMapa extends Celula {
  tentativa: number
}

/** Falhas de todas as tentativas, para o mapa mostrar também as anteriores à retomada (RF15). */
export function falhasDasTentativas(tentativas: Tentativa[]): FalhaNoMapa[] {
  return tentativas.flatMap((t) => (t.falha ? [{ x: t.falha.celula_x, y: t.falha.celula_y, tentativa: t.attempt_index }] : []))
}

/** Separa o trajeto em trechos: cada passo marcado como retomada começa um trecho novo. */
export function dividirEmTrechos(trajeto: PassoTrajeto[]): PassoTrajeto[][] {
  const trechos: PassoTrajeto[][] = []
  for (const passo of trajeto) {
    if (passo.retomada || trechos.length === 0) trechos.push([])
    trechos[trechos.length - 1].push(passo)
  }
  return trechos
}

/** Num 8x4 ou 12x4, o robô só passa de y = 3 se o lado longo estiver em y (4.4, "Orientação"). */
export function deduzirEixo(trajeto: PassoTrajeto[], ladoCurto: number): EixoLongo {
  return trajeto.some((p) => p.y >= ladoCurto) ? 'y' : 'x'
}

/** Colunas e linhas da grade, com o lado longo no eixo informado. */
export function dimensoes(largura: number, altura: number, eixo: EixoLongo): { colunas: number; linhas: number } {
  const longo = Math.max(largura, altura)
  const curto = Math.min(largura, altura)
  return eixo === 'y' ? { colunas: curto, linhas: longo } : { colunas: longo, linhas: curto }
}
