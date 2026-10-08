import { screen, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { describe, expect, it } from 'vitest'
import { CENARIOS } from '../mocks/cenarios'
import { HISTORICO, resumoAPI, ULTIMA_EXECUCAO } from '../mocks/dados'
import { servidor } from '../mocks/servidor'
import { renderizarRota } from './renderizar'

/** Texto que identifica cada um dos 12 estados do protótipo na rota `/`. */
const MARCA_DO_CENARIO: Record<string, RegExp> = {
  '01-inicio': /Nenhuma execução ativa/,
  '02-health-check': /6 de 9 OK/,
  '03-em-execucao': /Tempo da execução/,
  '04-concluida': /concluída na tentativa 1/,
  '05-execucao-recusada': /Nova execução recusada/,
  '06-sem-comunicacao': /Sem dados do robô há/,
  '07-encerrar-execucao': /Encerrar a tentativa 1 da execução #0042\?/,
  '08-encerrada-como-falha': /Retomar tentativa \(2\/3\)/,
  '09-retomar-tentativa': /Retomar a execução #0042\?/,
  '10-retomada-tentativa-2': /Tentativa 2\/3/,
  '11-concluida-apos-retomada': /concluída na tentativa 2/,
  '12-falha-sem-retomada': /Sem retomada/,
}

describe('rota /', () => {
  it('cobre os 12 estados do protótipo', () => {
    expect(CENARIOS.map((c) => c.id)).toEqual(Object.keys(MARCA_DO_CENARIO))
  })

  it.each(CENARIOS.map((c) => [c.id, c.modal] as const))('exibe o cenário %s', async (id, modal) => {
    renderizarRota(modal ? `/?modal=${modal}` : '/', id)
    if (id === '05-execucao-recusada') {
      await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
      const dialogo = await screen.findByRole('dialog')
      await userEvent.click(within(dialogo).getByRole('button', { name: 'Confirmar nova execução' }))
    }
    expect((await screen.findAllByText(MARCA_DO_CENARIO[id])).length).toBeGreaterThan(0)
  })

  it('Nova execução abre o health-check', async () => {
    renderizarRota('/', '01-inicio')
    await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
    expect(await screen.findByRole('heading', { name: 'Health-check' })).toBeInTheDocument()
  })

  it('mostra o estado da última execução sem usar o resultado da tentativa', async () => {
    renderizarRota('/')
    const cartao = (await screen.findByText('Última execução · 4x4')).closest('section')!
    expect(within(cartao).getByText('Concluída')).toBeInTheDocument()
    expect(within(cartao).queryByText('Sucesso')).not.toBeInTheDocument()
  })

  it.each([null, undefined])('usa o status da última execução quando resultado é %s', async (resultado) => {
    servidor.use(http.get('*/api/execucoes', () => HttpResponse.json([
      { ...resumoAPI(ULTIMA_EXECUCAO), resultado },
    ])))
    renderizarRota('/')
    const cartao = (await screen.findByText('Última execução · 4x4')).closest('section')!
    expect(within(cartao).getByText('Concluída')).toBeInTheDocument()
    expect(within(cartao).queryByText('—')).not.toBeInTheDocument()
  })

  it('Encerrar tentativa registra a falha e oferece a retomada', async () => {
    renderizarRota('/', '03-em-execucao')
    await userEvent.click(await screen.findByRole('button', { name: 'Encerrar tentativa' }))
    const modal = await screen.findByRole('dialog')
    await userEvent.click(within(modal).getByLabelText(/Colisão com parede/))
    await userEvent.click(within(modal).getByRole('button', { name: 'Encerrar como falha' }))
    expect(await screen.findByRole('button', { name: /Retomar tentativa \(2\/3\)/ })).toBeInTheDocument()
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument()
  })

  it('Retomar parte da célula da falha e só libera depois do checklist', async () => {
    renderizarRota('/?modal=retomar', '08-encerrada-como-falha')
    const modal = await screen.findByRole('dialog')
    expect(within(modal).getByText('Robô posicionado em B2')).toBeInTheDocument()
    const confirmar = within(modal).getByRole('button', { name: 'Retomar da célula B2' })
    expect(confirmar).toBeDisabled()
    for (const caixa of within(modal).getAllByRole('checkbox')) await userEvent.click(caixa)
    expect(confirmar).toBeEnabled()
  })
})

describe('rota /execucoes', () => {
  it.each([null, undefined])('usa o status no histórico quando resultado é %s', async (resultado) => {
    servidor.use(http.get('*/api/execucoes', () => HttpResponse.json(
      HISTORICO.map((execucao) => ({ ...resumoAPI(execucao), resultado })),
    )))
    renderizarRota('/execucoes')
    expect(await screen.findByRole('link', { name: '#0041' })).toBeInTheDocument()
    const tabela = within(screen.getByRole('table'))
    expect(tabela.getAllByText('Concluída')).toHaveLength(3)
    expect(tabela.getByText('Cancelada')).toBeInTheDocument()
    expect(tabela.queryByText('—')).not.toBeInTheDocument()
  })

  it('lista as execuções e filtra por labirinto', async () => {
    renderizarRota('/execucoes')
    expect(await screen.findByRole('link', { name: '#0041' })).toBeInTheDocument()
    expect(screen.getByRole('link', { name: '#0032' })).toBeInTheDocument()

    const tabela = within(screen.getByRole('table'))
    expect(tabela.getAllByText('Concluída')).toHaveLength(3)
    expect(tabela.getByText('Cancelada')).toBeInTheDocument()
    expect(tabela.queryByText('Sucesso')).not.toBeInTheDocument()
    expect(tabela.queryByText('Falha')).not.toBeInTheDocument()

    await userEvent.click(screen.getByRole('button', { name: '8x4' }))
    expect(await screen.findByRole('link', { name: '#0040' })).toBeInTheDocument()
    expect(screen.queryByRole('link', { name: '#0041' })).not.toBeInTheDocument()
  })
})

describe('rota /execucoes/:id', () => {
  it('mostra o detalhe com trajeto e tentativas', async () => {
    renderizarRota('/execucoes/e-0036')
    expect(await screen.findByRole('heading', { name: 'Execução #0036' })).toBeInTheDocument()
    expect(screen.getByRole('img', { name: /Labirinto 4x4 com 11 células visitadas/ })).toBeInTheDocument()
    expect(screen.getByText('Tentativas em ordem cronológica')).toBeInTheDocument()
    expect(screen.getByText('Concluída')).toBeInTheDocument()
    expect(within(screen.getByRole('table')).getByText('Sucesso')).toBeInTheDocument()
  })

  it('mantém a execução em andamento quando a última tentativa falhou', async () => {
    renderizarRota('/execucoes/e-0042', '08-encerrada-como-falha')
    expect(await screen.findByRole('heading', { name: 'Execução #0042' })).toBeInTheDocument()
    expect(screen.getByText('Em andamento')).toBeInTheDocument()
    expect(within(screen.getByRole('table')).getByText('Falha')).toBeInTheDocument()
    expect(screen.queryByText('Cancelada')).not.toBeInTheDocument()
  })

  it('avisa quando a execução não existe', async () => {
    renderizarRota('/execucoes/nao-existe')
    expect(await screen.findByRole('heading', { name: 'Execução não encontrada' })).toBeInTheDocument()
  })
})

it('rota desconhecida mostra a página 404', () => {
  renderizarRota('/qualquer-coisa')
  expect(screen.getByRole('heading', { name: 'Página não encontrada' })).toBeInTheDocument()
})
