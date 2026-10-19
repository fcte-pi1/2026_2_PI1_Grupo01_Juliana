import { render, screen, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { describe, expect, it } from 'vitest'
import { aplicarEvento } from '../api/eventosExecucao'
import type { Execucao } from '../api/tipos'
import { buscarCenario } from '../mocks/cenarios'
import { detalheAPI, LABIRINTOS } from '../mocks/dados'
import { servidor } from '../mocks/servidor'
import { PainelEncerrada } from '../pages/inicio/PainelEncerrada'
import { renderizarRota } from './renderizar'

function responder(execucao: Execucao) {
  servidor.use(
    http.get('*/api/execucoes/em-andamento', () => HttpResponse.json(detalheAPI(execucao))),
    http.get('*/api/execucoes/:id', () => HttpResponse.json(detalheAPI(execucao))),
  )
}

describe('FRONT-03 integrado ao labirinto do FRONT-04', () => {
  it.each([
    ['8x4', 'x', 'H4', 32], ['8x4', 'y', 'D8', 32],
    ['12x4', 'x', 'L4', 48], ['12x4', 'y', 'D12', 48],
  ] as const)('mantém resumo e mapa alinhados no %s com eixo %s', async (labirinto, eixo, objetivo, celulas) => {
    const execucao = buscarCenario('03-em-execucao').atual(Date.now()).execucao!
    Object.assign(execucao, { labirinto, eixo_longo: eixo })
    responder(execucao)
    renderizarRota('/')
    expect(await screen.findByText(`A1 → ${objetivo}`)).toBeInTheDocument()
    expect(screen.getByRole('img', { name: new RegExp(`Labirinto ${labirinto}`) }).querySelectorAll('.labirinto__celula')).toHaveLength(celulas)
  })

  it.each([
    ['8x4', 'H4', 32],
    ['12x4', 'L4', 48],
  ] as const)('desenha o %s inteiro antes de o robô informar a orientação', async (labirinto, objetivo, celulas) => {
    const execucao = buscarCenario('03-em-execucao').atual(Date.now()).execucao!
    Object.assign(execucao, { labirinto, eixo_longo: null })
    responder(execucao)
    renderizarRota('/')
    expect(await screen.findByText(`A1 → ${objetivo}`)).toBeInTheDocument()
    expect(screen.getByRole('img', { name: new RegExp(`Labirinto ${labirinto}`) }).querySelectorAll('.labirinto__celula')).toHaveLength(celulas)
  })

  it('usa a posição persistida mais recente no cartão, no mapa e no modal de encerramento', async () => {
    const agora = Date.now()
    const execucao = buscarCenario('03-em-execucao').atual(agora).execucao!
    execucao.leituras = [{ ordem: 1, x: 0, y: 0, bateria: 7810, velocidade: 210,
      enviado_em: new Date(agora - 2_000).toISOString() }]
    execucao.trajeto = [
      { passo_id: 1, seq: 1, x: 0, y: 0, paredes_mask: 8, retomada: false },
      { passo_id: 2, seq: 2, x: 0, y: 1, paredes_mask: 8, retomada: false, entrou_em: new Date(agora - 1_000).toISOString() },
    ]
    responder(execucao)
    renderizarRota('/')
    await screen.findByRole('heading', { name: 'Execução ativa #0042' })
    const cartao = screen.getByText('Célula atual', { selector: '.rotulo' }).closest('section')!
    expect(within(cartao).getByText('A2')).toBeInTheDocument()
    expect(screen.getByText('Robô em A2')).toBeInTheDocument()
    await userEvent.click(screen.getByRole('button', { name: 'Encerrar tentativa' }))
    expect(within(screen.getByRole('dialog')).getByText('Última célula A2')).toBeInTheDocument()
  })

  it('restaura no mapa a célula da falha mesmo quando o último passo é anterior a ela', async () => {
    const execucao = buscarCenario('08-encerrada-como-falha').atual(Date.now()).execucao!
    execucao.trajeto = [{ passo_id: 1, seq: 1, x: 0, y: 1, paredes_mask: 8, retomada: false }]
    responder(execucao)
    const primeira = renderizarRota('/')
    expect(await screen.findByText('Robô em B2 · falhou')).toBeInTheDocument()
    expect(screen.getByText('FALHA T1')).toBeInTheDocument()
    primeira.unmount()
    renderizarRota('/')
    expect(await screen.findByText('Robô em B2 · falhou')).toBeInTheDocument()
    expect(screen.getByText('FALHA T1')).toBeInTheDocument()
  })

  it('atualiza paredes de um passo republicado sem duplicar o percurso nem apagar a falha', () => {
    const agora = Date.now()
    const execucao = buscarCenario('08-encerrada-como-falha').atual(agora).execucao!
    execucao.trajeto = [{ passo_id: 1, seq: 1, x: 1, y: 1, paredes_mask: 8, retomada: false }]
    const props = { tentativa: execucao.tentativas[0], labirinto: LABIRINTOS[0], agora }
    const { container, rerender } = render(<PainelEncerrada execucao={execucao} {...props} />)
    expect(container.querySelectorAll('.labirinto__parede line')).toHaveLength(1)
    const atualizada = aplicarEvento(execucao, { tipo: 'passo', ...execucao.trajeto[0], passo_id: 1, paredes_mask: 9 })
    rerender(<PainelEncerrada execucao={atualizada} {...props} />)
    expect(container.querySelectorAll('.labirinto__parede line')).toHaveLength(2)
    expect(atualizada.trajeto).toHaveLength(1)
    expect(screen.getByText('FALHA T1')).toBeInTheDocument()
  })
})
