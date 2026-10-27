import { screen, within } from '@testing-library/react'
import { http, HttpResponse } from 'msw'
import { describe, expect, it } from 'vitest'
import type { ComponenteHealthCheck, Execucao, MotivoFalha, PassoTrajeto } from '../api/tipos'
import { detalheAPI, execucao, falha, tentativa } from '../mocks/dados'
import { servidor } from '../mocks/servidor'
import { nomeCelula, ROTULO_COMPONENTE, ROTULO_MOTIVO_FALHA } from '../utils/formatacao'
import { renderizarRota } from './renderizar'

/**
 * Caso 4.10 do 7.4 (US06, US11, US17, US29): o labirinto do detalhe precisa mostrar o
 * mesmo trajeto, as mesmas paredes e a mesma célula de falha dos roteiros do simulador
 * de telemetria (src/simulador-telemetria/roteiros). O roteiro vira a resposta da API do
 * jeito que o backend grava: um passo por célula, o primeiro da retomada marcado, e a
 * falha na última célula da tentativa.
 */

interface CelulaRoteiro {
  x: number
  y: number
  paredes: number
}

interface Roteiro {
  nome: string
  labirinto: '4x4'
  tentativas: Array<{
    inicio: 'nova' | 'retomada'
    celulas: CelulaRoteiro[]
    fim: { tipo: 'sucesso' } | { tipo: 'falha'; motivo: MotivoFalha; componente?: ComponenteHealthCheck }
  }>
}

const ROTEIROS = import.meta.glob<Roteiro>('../../../simulador-telemetria/roteiros/{sucesso-4x4,colisao-retomada,falha-componente}.json', {
  eager: true,
  import: 'default',
})

function roteiro(nome: string): Roteiro {
  const achado = Object.entries(ROTEIROS).find(([caminho]) => caminho.endsWith(`/${nome}.json`))
  if (!achado) throw new Error(`roteiro ${nome} não encontrado`)
  return achado[1]
}

const INICIO = Date.parse('2026-10-24T12:00:00.000Z')

function execucaoDoRoteiro(r: Roteiro, id: string): Execucao {
  const trajeto: PassoTrajeto[] = []
  const tentativas = r.tentativas.map((t, indice) => {
    const inicio = INICIO + indice * 60_000
    t.celulas.forEach((c, ordem) => {
      trajeto.push({
        seq: trajeto.length + 1,
        x: c.x,
        y: c.y,
        paredes_mask: c.paredes,
        retomada: t.inicio === 'retomada' && ordem === 0,
        entrou_em: new Date(inicio + 1000 + ordem * 800).toISOString(),
      })
    })
    const ultima = t.celulas.at(-1)!
    return tentativa(inicio, {
      tentativa_id: `${id}-t${indice + 1}`,
      attempt_index: indice + 1,
      status: t.fim.tipo === 'sucesso' ? 'success' : 'failed',
      tipo_inicio: t.inicio,
      iniciada_em: new Date(inicio).toISOString(),
      encerrada_em: new Date(inicio + 30_000).toISOString(),
      falha: t.fim.tipo === 'falha'
        ? falha(inicio, {
          motivo: t.fim.motivo,
          origem: 'automatica',
          celula_x: ultima.x,
          celula_y: ultima.y,
          componente: t.fim.componente ?? null,
          momento_falha: new Date(inicio + 30_000).toISOString(),
        })
        : null,
    })
  })
  const sucesso = r.tentativas.at(-1)!.fim.tipo === 'sucesso'
  return execucao(INICIO, {
    execucao_id: id,
    numero: 410,
    labirinto: r.labirinto,
    status: sucesso ? 'concluida' : 'em_andamento',
    tentativas_usadas: tentativas.length,
    tentativas,
    trajeto,
    celula_atual: trajeto.length ? { x: trajeto.at(-1)!.x, y: trajeto.at(-1)!.y } : null,
  })
}

/** Geometria do VisualizacaoLabirinto num 4x4: célula de 100, margem 20 + eixo 36 à esquerda. */
const LADO = 100
const ESQ = 56
const TOPO = 20
const LINHAS = 4

const celulaDoPonto = (cx: number, cy: number) => ({ x: (cx - ESQ - LADO / 2) / LADO, y: LINHAS - 1 - (cy - TOPO - LADO / 2) / LADO })

function trechosDesenhados() {
  return Array.from(document.querySelectorAll('polyline.labirinto__trajeto'), (linha) =>
    (linha.getAttribute('points') ?? '').trim().split(/\s+/).map((par) => {
      const [cx, cy] = par.split(',').map(Number)
      return celulaDoPonto(cx, cy)
    }))
}

async function abrirDetalhe(nome: string) {
  const r = roteiro(nome)
  const id = `e-410-${nome}`
  const dados = execucaoDoRoteiro(r, id)
  servidor.use(http.get(`*/api/execucoes/${id}`, () => HttpResponse.json(detalheAPI(dados))))
  renderizarRota(`/execucoes/${id}`)
  await screen.findByRole('heading', { name: 'Execução #0410' })
  return { r, dados }
}

describe('4.10 Labirinto e trajeto contra os roteiros do simulador', () => {
  it.each(['sucesso-4x4', 'colisao-retomada', 'falha-componente'])('US06/US11: %s desenha as células na ordem do roteiro', async (nome) => {
    const { r } = await abrirDetalhe(nome)
    expect(trechosDesenhados()).toEqual(r.tentativas.map((t) => t.celulas.map(({ x, y }) => ({ x, y }))))
  })

  it.each(['sucesso-4x4', 'colisao-retomada', 'falha-componente'])('US29: %s mostra as paredes de cada célula', async (nome) => {
    const { r } = await abrirDetalhe(nome)
    // a célula repetida na retomada mostra as paredes do último passo, iguais no roteiro
    const ultimas = new Map(r.tentativas.flatMap((t) => t.celulas).map((c) => [nomeCelula(c), c]))
    for (const [celula, dados] of ultimas) {
      expect(contarParedes(celula), `paredes de ${celula}`).toBe(popcount(dados.paredes))
      expect(ladosDesenhados(celula, dados), `lados de ${celula}`).toBe(dados.paredes)
    }
  })

  it('US17: colisão destaca a célula da falha da T1 e a retomada chega ao objetivo', async () => {
    const { r } = await abrirDetalhe('colisao-retomada')
    const celulaFalha = nomeCelula(r.tentativas[0].celulas.at(-1)!)
    expect(screen.getByText('FALHA T1')).toBeInTheDocument()
    expect(document.querySelectorAll('.labirinto__destaque--falha')).toHaveLength(1)
    const cartao = screen.getByRole('heading', { name: 'Registro da falha · tentativa 1' }).closest('section')!
    expect(within(cartao).getByText(celulaFalha)).toBeInTheDocument()
    expect(within(cartao).getByText(ROTULO_MOTIVO_FALHA.collision)).toBeInTheDocument()
    expect(document.querySelector('.labirinto__robo title')?.textContent).toBe(`Robô em ${nomeCelula(r.tentativas[1].celulas.at(-1)!)}`)
    expect(document.querySelector('.labirinto__trajeto--retomada')).not.toBeNull()
  })

  it('US17: falha de componente destaca a célula, pinta o robô e mostra o componente', async () => {
    const { r } = await abrirDetalhe('falha-componente')
    const celulaFalha = nomeCelula(r.tentativas[0].celulas.at(-1)!)
    expect(screen.getByText('FALHA T1')).toBeInTheDocument()
    expect(document.querySelector('.labirinto__robo title')?.textContent).toBe(`Robô em ${celulaFalha} · falhou`)
    const cartao = screen.getByRole('heading', { name: 'Registro da falha · tentativa 1' }).closest('section')!
    expect(within(cartao).getByText(celulaFalha)).toBeInTheDocument()
    expect(within(cartao).getByText(ROTULO_MOTIVO_FALHA.falha_componente)).toBeInTheDocument()
    expect(within(cartao).getByText(new RegExp(`Componente: ${ROTULO_COMPONENTE.motor_direito}`))).toBeInTheDocument()
  })

  it('US06: sem falha, nenhuma célula fica vermelha e o robô termina no objetivo', async () => {
    await abrirDetalhe('sucesso-4x4')
    expect(document.querySelectorAll('.labirinto__destaque--falha')).toHaveLength(0)
    expect(document.querySelector('.labirinto__robo--falha')).toBeNull()
    expect(screen.queryByRole('heading', { name: /Registro da falha/ })).not.toBeInTheDocument()
  })
})

function popcount(n: number) {
  let total = 0
  for (let v = n; v; v &= v - 1) total++
  return total
}

function linhasDaCelula(nome: string) {
  const grupo = Array.from(document.querySelectorAll('svg.labirinto__svg g')).find((g) =>
    g.querySelector(':scope > title')?.textContent?.startsWith(`${nome} ·`))
  if (!grupo) throw new Error(`célula ${nome} não desenhada`)
  return Array.from(grupo.querySelectorAll('.labirinto__parede line'), (l) =>
    ['x1', 'y1', 'x2', 'y2'].map((a) => Number(l.getAttribute(a))))
}

function contarParedes(nome: string) {
  return linhasDaCelula(nome).length
}

/** Compara cada linha com os quatro lados da célula, calculados pela posição no 4x4. */
function ladosDesenhados(nome: string, c: CelulaRoteiro) {
  const esquerda = ESQ + c.x * LADO
  const topo = TOPO + (LINHAS - 1 - c.y) * LADO
  let mascara = 0
  for (const [x1, y1, x2, y2] of linhasDaCelula(nome)) {
    if (y1 === y2) mascara |= y1 === topo ? 1 : y1 === topo + LADO ? 2 : 0
    else if (x1 === x2) mascara |= x1 === esquerda + LADO ? 4 : x1 === esquerda ? 8 : 0
  }
  return mascara
}
