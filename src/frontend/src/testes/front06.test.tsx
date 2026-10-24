import { screen, waitFor, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { describe, expect, it } from 'vitest'
import { detalheAPI } from '../mocks/dados'
import { servidor } from '../mocks/servidor'
import { execucaoUs28 } from './execucaoUs28'
import { renderizarRota } from './renderizar'

function titulosDoMapa(): string[] {
  return Array.from(document.querySelectorAll('svg.labirinto__svg title'), (el) => el.textContent ?? '')
}

/** O segmento entre dois centros de célula não pode coincidir com uma parede desenhada. */
function caminhoNaoAtravessaParede() {
  const pontos = Array.from(document.querySelectorAll('polyline.labirinto__trajeto'))
    .flatMap((linha) => (linha.getAttribute('points') ?? '').trim().split(/\s+/).filter(Boolean))
    .map((par) => par.split(',').map(Number))
  const paredes = Array.from(document.querySelectorAll('.labirinto__parede line')).map((linha) => ({
    x1: Number(linha.getAttribute('x1')),
    y1: Number(linha.getAttribute('y1')),
    x2: Number(linha.getAttribute('x2')),
    y2: Number(linha.getAttribute('y2')),
  }))
  expect(paredes.length).toBeGreaterThan(0)
  for (let i = 1; i < pontos.length; i++) {
    const [x1, y1] = pontos[i - 1]
    const [x2, y2] = pontos[i]
    if (x1 === x2 && y1 === y2) continue
    const mx = (x1 + x2) / 2
    const my = (y1 + y2) / 2
    const bloqueia = paredes.some((parede) => {
      if (y1 !== y2 && parede.y1 === parede.y2 && parede.y1 === my) {
        const [menor, maior] = [Math.min(parede.x1, parede.x2), Math.max(parede.x1, parede.x2)]
        return mx > menor && mx < maior
      }
      if (x1 !== x2 && parede.x1 === parede.x2 && parede.x1 === mx) {
        const [menor, maior] = [Math.min(parede.y1, parede.y2), Math.max(parede.y1, parede.y2)]
        return my > menor && my < maior
      }
      return false
    })
    expect(bloqueia, `trajeto cruza parede entre ${x1},${y1} e ${x2},${y2}`).toBe(false)
  }
}

describe('FRONT-06 histórico e detalhe', () => {
  it('lista em ordem decrescente e filtra por labirinto, inclusive Todos', async () => {
    renderizarRota('/execucoes')
    expect(await screen.findByRole('link', { name: '#0041' })).toBeInTheDocument()
    const links = () => within(screen.getByRole('table')).getAllByRole('link').map((link) => link.textContent)
    expect(links()).toEqual(['#0041', '#0040', '#0036', '#0034', '#0032'])

    const filtro = within(screen.getByRole('group', { name: 'Filtrar por labirinto' }))
    await userEvent.click(filtro.getByRole('button', { name: '4x4' }))
    await waitFor(() => expect(links()).toEqual(['#0041', '#0036']))

    await userEvent.click(filtro.getByRole('button', { name: '8x4' }))
    await waitFor(() => expect(links()).toEqual(['#0040', '#0032']))

    await userEvent.click(filtro.getByRole('button', { name: '12x4' }))
    await waitFor(() => expect(links()).toEqual(['#0034']))

    await userEvent.click(filtro.getByRole('button', { name: 'Todos' }))
    await waitFor(() => expect(links()[0]).toBe('#0041'))
    expect(links()).toHaveLength(5)
  })

  it('reexibe as três tentativas, as métricas, a variação e o trajeto completo', async () => {
    servidor.use(http.get('*/api/execucoes/e-0043', () => HttpResponse.json(detalheAPI(execucaoUs28()))))
    renderizarRota('/execucoes/e-0043')
    expect(await screen.findByRole('heading', { name: 'Execução #0043' })).toBeInTheDocument()
    expect(screen.getByText('3/3 tentativas')).toBeInTheDocument()
    expect(screen.getByText('Variação contra o melhor tempo')).toBeInTheDocument()
    expect(screen.getByText('12,5')).toBeInTheDocument()

    const tabela = within(screen.getByRole('table'))
    const linhas = tabela.getAllByRole('row').slice(1).map((linha) => within(linha).getAllByRole('cell').map((celula) => celula.textContent))
    expect(linhas).toEqual([
      ['1/3', 'Falha', '00:06,2', '0,12 m/s', '100 mV', '—'],
      ['2/3', 'Falha', '00:04,8', '0,15 m/s', '100 mV', '—'],
      ['3/3', 'Sucesso', '00:09,5', '0,18 m/s', '200 mV', '—'],
    ])

    const cartao = (tentativa: number) => screen.getByRole('heading', { name: `Registro da falha · tentativa ${tentativa}` }).closest('section')!
    expect(within(cartao(1)).getByText('A3')).toBeInTheDocument()
    expect(within(cartao(1)).getByText('Colisão com parede')).toBeInTheDocument()
    expect(within(cartao(1)).getByText(/Encerrada pelo operador/)).toBeInTheDocument()
    expect(within(cartao(1)).queryByText(/Componente/)).not.toBeInTheDocument()
    expect(within(cartao(2)).getByText('B4')).toBeInTheDocument()
    expect(within(cartao(2)).getByText('Robô travado ou parado')).toBeInTheDocument()
    expect(within(cartao(2)).queryByText(/Componente/)).not.toBeInTheDocument()
    expect(screen.queryByRole('heading', { name: 'Registro da falha · tentativa 3' })).not.toBeInTheDocument()

    expect(screen.getByRole('img', { name: 'Labirinto 4x4 com 7 células visitadas' })).toBeInTheDocument()
    expect(titulosDoMapa()).toEqual([
      'A1 · passo 1', 'A2 · passo 2', 'A3 · passo 4', 'A4 · passo 5',
      'B4 · passo 7', 'C4 · passo 8', 'D4 · passo 9', 'Robô em D4',
    ])
    const pontos = Array.from(document.querySelectorAll('polyline.labirinto__trajeto'),
      (linha) => (linha.getAttribute('points') ?? '').trim().split(/\s+/).length)
    expect(pontos).toEqual([3, 3, 3])
    expect(document.querySelector('.labirinto__robo title')?.textContent).toBe('Robô em D4')
    caminhoNaoAtravessaParede()

    const grupo = within(screen.getByRole('group', { name: 'Trajeto por tentativa' }))
    await userEvent.click(grupo.getByRole('button', { name: 'T1' }))
    expect(titulosDoMapa()).toEqual(expect.arrayContaining(['A1 · passo 1', 'A2 · passo 2', 'A3 · passo 3']))
    expect(titulosDoMapa().some((titulo) => titulo.startsWith('D4'))).toBe(false)
    expect(screen.getByText('FALHA T1')).toBeInTheDocument()
    expect(screen.getByText('FALHA T2')).toBeInTheDocument()
    expect(document.querySelector('.labirinto__robo title')?.textContent).toBe('Robô em A3 · falhou')
    expect(screen.getByText('Colisão com parede')).toBeInTheDocument()

    await userEvent.click(grupo.getByRole('button', { name: 'T2' }))
    expect(screen.getByText('Retomada em A3', { selector: '.legenda' })).toBeInTheDocument()
    expect(titulosDoMapa()).toEqual(expect.arrayContaining(['A3 · passo 4', 'A4 · passo 5', 'B4 · passo 6']))
    expect(document.querySelector('.labirinto__robo title')?.textContent).toBe('Robô em B4 · falhou')

    await userEvent.click(grupo.getByRole('button', { name: 'T3' }))
    expect(screen.getByText('Retomada em B4', { selector: '.legenda' })).toBeInTheDocument()
    expect(titulosDoMapa()).toEqual(expect.arrayContaining(['B4 · passo 7', 'C4 · passo 8', 'D4 · passo 9']))
    expect(document.querySelector('.labirinto__robo title')?.textContent).toBe('Robô em D4')
    expect(screen.getByText('OBJETIVO')).toBeInTheDocument()
    expect(screen.getByText('Tentativas em ordem cronológica')).toBeInTheDocument()
    caminhoNaoAtravessaParede()
  })
})
