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

export const PAREDE_N = 1
export const PAREDE_S = 2
export const PAREDE_L = 4
export const PAREDE_O = 8

/** Chave da célula no mapa de paredes, igual à do desenho. */
function chaveCelula(c: Celula): string {
  return `${c.x},${c.y}`
}

/** Lado de `a` por onde se chega à vizinha `b` e o lado oposto, de `b`. Sem vizinhança, null. */
function ladosEntre(a: Celula, b: Celula): [number, number] | null {
  const dx = b.x - a.x
  const dy = b.y - a.y
  if (dx === 0 && dy === 1) return [PAREDE_N, PAREDE_S]
  if (dx === 0 && dy === -1) return [PAREDE_S, PAREDE_N]
  if (dx === 1 && dy === 0) return [PAREDE_L, PAREDE_O]
  if (dx === -1 && dy === 0) return [PAREDE_O, PAREDE_L]
  return null
}

/**
 * Paredes de cada célula visitada, somando todas as passagens por ela. Cada passo traz só os três
 * lados que os sensores veem naquele rumo (frente, esquerda, direita), nunca o de trás: ficar com o
 * último passo apagava, na volta, a parede vista na ida. Por onde o robô passou de uma célula para a
 * vizinha não há parede, mesmo que alguma leitura a tenha marcado.
 */
export function paredesDasCelulas(trajeto: PassoTrajeto[]): Map<string, number> {
  const paredes = new Map<string, number>()
  for (const passo of trajeto) {
    const chave = chaveCelula(passo)
    paredes.set(chave, (paredes.get(chave) ?? 0) | passo.paredes_mask)
  }
  for (const trecho of dividirEmTrechos(trajeto)) {
    for (let i = 1; i < trecho.length; i++) {
      const lados = ladosEntre(trecho[i - 1], trecho[i])
      if (!lados) continue
      const origem = chaveCelula(trecho[i - 1])
      const destino = chaveCelula(trecho[i])
      paredes.set(origem, paredes.get(origem)! & ~lados[0])
      paredes.set(destino, paredes.get(destino)! & ~lados[1])
    }
  }
  return paredes
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
 * Passos de uma tentativa, para o detalhe mostrar cada uma em separado. A retomada reparte
 * da célula onde a anterior falhou (US31, ADR-012), não de A1. O passo pertence à última
 * tentativa iniciada antes dele; sem `entrou_em`, vale a ordem dos trechos.
 */
export function passosDaTentativa(trajeto: PassoTrajeto[], tentativas: Tentativa[], attemptIndex: number): PassoTrajeto[] {
  if (trajeto.every((p) => p.entrou_em)) {
    const inicios = tentativas.map((t) => ({ indice: t.attempt_index, inicio: Date.parse(t.iniciada_em) }))
      .sort((a, b) => a.inicio - b.inicio)
    return trajeto.filter((p) => {
      const entrada = Date.parse(p.entrou_em!)
      return inicios.filter((t) => t.inicio <= entrada).at(-1)?.indice === attemptIndex
    })
  }
  return dividirEmTrechos(trajeto)[attemptIndex - 1] ?? []
}

/**
 * Célula de onde a tentativa partiu: A1 na primeira; na retomada, a falha da anterior
 * (US31, ADR-012) ou, sem ela registrada, o primeiro passo da tentativa.
 */
export function inicioDaTentativa(passos: PassoTrajeto[], tentativas: Tentativa[], attemptIndex: number): Celula | null {
  if (attemptIndex === 1) return { x: 0, y: 0 }
  const falha = tentativas.find((t) => t.attempt_index === attemptIndex - 1)?.falha
  if (falha) return { x: falha.celula_x, y: falha.celula_y }
  return passos[0] ? { x: passos[0].x, y: passos[0].y } : null
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
 * Eixo usado no desenho. O tipo escolhido (8x4, 12x4) já tem tamanho: sem orientação
 * confirmada, o lado longo fica na horizontal. O eixo `y` só entra quando o robô o informa.
 */
export function eixoParaGrade(largura: number, altura: number, eixo: EixoLongo | null): EixoLongo | null {
  if (eixo) return eixo
  return largura === altura ? null : 'x'
}

/**
 * Colunas e linhas da grade. Sem eixo, o labirinto é o quadrado do lado curto.
 * Com eixo, o lado longo fica na horizontal (`x`) ou na vertical (`y`).
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
