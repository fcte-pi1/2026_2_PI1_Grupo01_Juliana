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

/**
 * Num 8x4 ou 12x4, o robô só passa de y = 3 se o lado longo estiver em y, e só passa
 * de x = 3 se estiver em x (4.4, "Orientação"). Enquanto não passou de nenhum, devolve null.
 */
export function deduzirEixo(trajeto: PassoTrajeto[], ladoCurto: number): EixoLongo | null {
  if (trajeto.some((p) => p.y >= ladoCurto)) return 'y'
  if (trajeto.some((p) => p.x >= ladoCurto)) return 'x'
  return null
}

/**
 * Colunas e linhas da grade, com o lado longo no eixo informado. Sem eixo, mostra só o
 * quadrado do lado curto: o tamanho completo espera a orientação ser conhecida.
 */
export function dimensoes(largura: number, altura: number, eixo: EixoLongo | null): { colunas: number; linhas: number } {
  const longo = Math.max(largura, altura)
  const curto = Math.min(largura, altura)
  if (eixo === null) return { colunas: curto, linhas: curto }
  return eixo === 'y' ? { colunas: curto, linhas: longo } : { colunas: longo, linhas: curto }
}

/** Onde marcar cada retomada: na célula da falha da tentativa anterior, de onde o robô reparte. */
export function celulasDeRetomada(trechos: PassoTrajeto[][], falhas: FalhaNoMapa[]): Array<Celula & { tentativa: number }> {
  return trechos.slice(1).map((trecho, i) => {
    const falha = falhas.find((f) => f.tentativa === i + 1)
    return { ...(falha ?? trecho[0]), tentativa: i + 2 }
  })
}
