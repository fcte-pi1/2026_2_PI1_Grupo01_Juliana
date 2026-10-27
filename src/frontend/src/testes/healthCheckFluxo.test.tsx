import { screen, waitFor, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { describe, expect, it } from 'vitest'
import type { UltimoHealthCheck } from '../api/tipos'
import { healthCheck } from '../mocks/dados'
import { servidor } from '../mocks/servidor'
import { renderizarRota } from './renderizar'

function aprovado(): UltimoHealthCheck {
  return { iniciado_em: new Date(Date.now() - 20_000).toISOString(),
    concluido_em: new Date(Date.now() - 10_000).toISOString(),
    valido_ate: new Date(Date.now() + 400_000).toISOString(), aprovado: true, tipo_dip: '4x4',
    itens: healthCheck(8).map((item) => ({ ...item, aprovado: true })) }
}

const impedimentos: Array<[string, () => UltimoHealthCheck | null, RegExp]> = [
  ['ausente', () => null, /Realize o health-check antes/],
  ['pendente', () => ({ ...aprovado(), aprovado: null, concluido_em: null, valido_ate: null }), /Aguardando o health-check do robô/],
  ['reprovado', () => ({ ...aprovado(), aprovado: false }), /Health-check reprovado/],
  ['expirado', () => ({ ...aprovado(), valido_ate: new Date(Date.now() - 1_000).toISOString() }), /sem validade vigente/],
  ['sem validade', () => ({ ...aprovado(), valido_ate: null }), /sem validade vigente/],
  ['DIP divergente', () => ({ ...aprovado(), tipo_dip: '8x4' }), /O robô informou 8x4/],
]

describe('pré-requisito da largada conforme UC20 e backend atual', () => {
  it.each(impedimentos)('não abre uma execução com health-check %s', async (_nome, resultado, mensagem) => {
    let criacoes = 0
    servidor.use(
      http.get('*/api/health-check/ultimo', () => {
        const dados = resultado()
        return dados ? HttpResponse.json(dados) : new HttpResponse(null, { status: 204 })
      }),
      http.post('*/api/execucoes', () => { criacoes++; return new HttpResponse(null, { status: 500 }) }),
    )
    renderizarRota('/')
    expect(await screen.findByText(mensagem)).toBeInTheDocument()
    const iniciar = screen.getByRole('button', { name: /Nova execução/ })
    expect(iniciar).toBeDisabled()
    await userEvent.click(iniciar)
    expect(criacoes).toBe(0)
    expect(screen.queryByLabelText('Tentativa 1 de 3')).not.toBeInTheDocument()
  })

  it.each(impedimentos)('mantém a tentativa usada e impede Retomar com health-check %s', async (_nome, resultado, mensagem) => {
    let retomadas = 0
    servidor.use(
      http.get('*/api/health-check/ultimo', () => {
        const dados = resultado()
        return dados ? HttpResponse.json(dados) : new HttpResponse(null, { status: 204 })
      }),
      http.post('*/api/execucoes/:id/retomar', () => { retomadas++; return new HttpResponse(null, { status: 500 }) }),
    )
    renderizarRota('/?modal=retomar', '08-encerrada-como-falha')
    const modal = within(await screen.findByRole('dialog'))
    await modal.findByText(mensagem)
    for (const caixa of modal.getAllByRole('checkbox')) await userEvent.click(caixa)
    const confirmar = modal.getByRole('button', { name: 'Retomar da célula B2' })
    expect(confirmar).toBeDisabled()
    await userEvent.click(confirmar)
    expect(retomadas).toBe(0)
    expect(screen.getByLabelText('Tentativa 1 de 3')).toBeInTheDocument()
  })

  it('solicita uma vez, aguarda um resultado novo e só abre a tentativa após a aprovação', async () => {
    let dados = aprovado()
    let consultas = 0
    let solicitacoes = 0
    let criacoes = 0
    let liberar!: () => void
    const pendente = new Promise<void>((resolve) => { liberar = resolve })
    const pedido = new Date().toISOString()
    servidor.use(
      http.get('*/api/health-check/ultimo', () => { consultas++; return HttpResponse.json(dados) }),
      http.post('*/api/health-check/solicitar', async () => {
        solicitacoes++
        await pendente
        return HttpResponse.json({ solicitado_em: pedido }, { status: 202 })
      }),
    )
    const observar = ({ request }: { request: Request }) => {
      if (request.method === 'POST' && new URL(request.url).pathname === '/api/execucoes') criacoes++
    }
    servidor.events.on('request:start', observar)
    try {
      renderizarRota('/')
      const iniciar = await screen.findByRole('button', { name: /Nova execução/ })
      await waitFor(() => expect(iniciar).toBeEnabled())
      await userEvent.dblClick(screen.getByRole('button', { name: 'Solicitar health-check' }))
      await waitFor(() => expect(solicitacoes).toBe(1))
      expect(screen.getByRole('button', { name: 'Solicitando…' })).toBeDisabled()
      expect(iniciar).toBeDisabled()
      // A solicitação continua pendente mesmo se o operador trocar a seleção.
      await userEvent.click(screen.getByRole('radio', { name: /^8x4/ }))
      await userEvent.click(screen.getByRole('radio', { name: /^4x4/ }))
      liberar()
      await screen.findByText(/Aguardando o health-check do robô/)
      const anteriores = consultas
      await waitFor(() => expect(consultas).toBeGreaterThan(anteriores), { timeout: 2_000 })
      // Um GET que ainda traz a aprovação anterior não atende ao novo pedido.
      expect(iniciar).toBeDisabled()
      expect(criacoes).toBe(0)
      dados = { ...aprovado(), iniciado_em: pedido }
      await waitFor(() => expect(iniciar).toBeEnabled(), { timeout: 2_000 })
      await userEvent.click(iniciar)
      await screen.findByRole('heading', { name: 'Execução ativa #0042' })
      expect(screen.getByRole('button', { name: 'Encerrar tentativa' })).toBeEnabled()
      expect(screen.getByLabelText('Tentativa 1 de 3')).toBeInTheDocument()
      expect(criacoes).toBe(1)
      expect(solicitacoes).toBe(1)
    } finally {
      liberar()
      servidor.events.removeListener('request:start', observar)
    }
  })

  it('não libera a largada se não consegue consultar o backend', async () => {
    servidor.use(http.get('*/api/health-check/ultimo', () => new HttpResponse(null, { status: 503 })))
    renderizarRota('/')
    expect(await screen.findByRole('alert')).toHaveTextContent('Não foi possível consultar o health-check')
    expect(screen.getByRole('button', { name: /Nova execução/ })).toBeDisabled()
  })

  it('mostra o motivo real da recusa de uma solicitação', async () => {
    servidor.use(http.post('*/api/health-check/solicitar', () => HttpResponse.json({
      detail: 'Há uma tentativa em execução; encerre-a antes de pedir um novo health-check.',
    }, { status: 409 })))
    renderizarRota('/')
    await userEvent.click(await screen.findByRole('button', { name: 'Solicitar health-check' }))
    expect(await screen.findByRole('alert')).toHaveTextContent('Há uma tentativa em execução')
    expect(screen.queryByLabelText('Tentativa 1 de 3')).not.toBeInTheDocument()
  })
})
