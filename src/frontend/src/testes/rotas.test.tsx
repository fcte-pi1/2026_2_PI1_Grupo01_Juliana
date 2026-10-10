import { screen, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { describe, expect, it } from 'vitest'
import { CENARIOS } from '../mocks/cenarios'
import { renderizarRota } from './renderizar'

/** Texto que identifica cada um dos 12 estados do protótipo na rota `/`. */
const MARCA_DO_CENARIO: Record<string, RegExp> = {
  '01-inicio': /Nenhuma execução ativa/,
  '02-health-check': /6 de 8 OK/,
  '03-em-execucao': /Tempo da execução/,
  '04-concluida': /concluída na tentativa 1/,
  '05-execucao-recusada': /Tentativa recusada/,
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
    expect((await screen.findAllByText(MARCA_DO_CENARIO[id])).length).toBeGreaterThan(0)
  })

  it('Nova execução abre o health-check', async () => {
    renderizarRota('/', '01-inicio')
    await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
    expect(await screen.findByText(/6 de 8 OK/)).toBeInTheDocument()
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
  it('lista as execuções e filtra por labirinto', async () => {
    renderizarRota('/execucoes')
    expect(await screen.findByRole('link', { name: '#0041' })).toBeInTheDocument()
    expect(screen.getByRole('link', { name: '#0032' })).toBeInTheDocument()

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
