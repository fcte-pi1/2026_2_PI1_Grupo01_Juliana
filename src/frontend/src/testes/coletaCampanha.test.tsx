import { render, screen, waitFor, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { MemoryRouter, Route, Routes } from 'react-router'
import { describe, expect, it } from 'vitest'
import type { ResumoCampanha, ResumoExecucao } from '../api/tipos'
import { CartaoCampanha } from '../components/historico/CartaoCampanha'
import { TabelaExecucoes } from '../components/historico/TabelaExecucoes'
import { CamposRotulo } from '../components/execucao/FormNovaExecucao'
import { ultimoHealthCheckMock } from '../mocks/healthCheckAtual'
import { servidor } from '../mocks/servidor'
import { renderizarRota } from './renderizar'

const campanha: ResumoCampanha = {
  total: 10,
  concluidas: 8,
  canceladas: 2,
  em_andamento: 0,
  taxa_sucesso_pct: 80,
  tempo_medio_s: 212.5,
  tempo_maximo_s: 540,
  concluidas_abaixo_de_10_min: 8,
  por_configuracao: [
    { configuracao: 'paredes A', total: 6, concluidas: 5, taxa_sucesso_pct: 83.3, tempo_medio_s: 200, tempo_maximo_s: 300 },
    { configuracao: null, total: 4, concluidas: 3, taxa_sucesso_pct: 75, tempo_medio_s: 230, tempo_maximo_s: 540 },
  ],
}

describe('CartaoCampanha', () => {
  it('mostra a taxa de sucesso, os tempos e o resumo por configuração', () => {
    render(<CartaoCampanha campanha={campanha} labirinto={null} />)

    const cartao = screen.getByRole('region', { name: 'Resumo da campanha' })
    expect(within(cartao).getByText('80%')).toBeInTheDocument()
    expect(within(cartao).getByText('8 de 10')).toBeInTheDocument()
    expect(within(cartao).getByText('Abaixo de 10 min').nextSibling).toHaveTextContent('8')
    expect(within(cartao).getByText('paredes A')).toBeInTheDocument()
    expect(within(cartao).getByText('sem rótulo')).toBeInTheDocument()
  })

  it('oferece o CSV e o JSON, com o filtro de labirinto no endereço', () => {
    render(<CartaoCampanha campanha={campanha} labirinto="4x4" />)

    const csv = screen.getByRole('link', { name: 'Baixar CSV' })
    const json = screen.getByRole('link', { name: 'Baixar JSON' })
    expect(csv.getAttribute('href')).toContain('/execucoes/exportar?formato=csv&tipo_labirinto=4x4')
    expect(json.getAttribute('href')).toContain('/execucoes/exportar?formato=json&tipo_labirinto=4x4')
    expect(csv).toHaveAttribute('download', 'execucoes.csv')
  })

  it('sem taxa nem tempos mostra um traço, e não zero', () => {
    render(
      <CartaoCampanha
        campanha={{ ...campanha, total: 0, concluidas: 0, canceladas: 0, taxa_sucesso_pct: null, tempo_medio_s: null,
          tempo_maximo_s: null, concluidas_abaixo_de_10_min: 0, por_configuracao: [] }}
        labirinto={null}
      />,
    )
    expect(screen.getByText('Taxa de sucesso').nextSibling).toHaveTextContent('—')
  })
})

describe('TabelaExecucoes com o rótulo', () => {
  const execucao = (extra: Partial<ResumoExecucao>): ResumoExecucao => ({
    execucao_id: 'e-1', labirinto: '4x4', status: 'concluida', resultado: 'concluida', tentativas_usadas: 1,
    iniciada_em: '2026-11-21T12:00:00Z', tempo_total_s: 120, velocidade_media: 0.2, consumo_bateria: null, ...extra,
  })

  it('mostra a configuração de paredes e, na dica, a observação', () => {
    render(
      <MemoryRouter>
        <TabelaExecucoes execucoes={[execucao({ configuracao: 'paredes A', observacao: 'QA-05' }), execucao({ execucao_id: 'e-2' })]} />
      </MemoryRouter>,
    )
    expect(screen.getByRole('columnheader', { name: 'Configuração' })).toBeInTheDocument()
    const celula = screen.getByText('paredes A')
    expect(celula).toHaveAttribute('title', 'QA-05')
    expect(screen.getByText('sem rótulo')).toBeInTheDocument()
  })

  it('abre o detalhe ao clicar em qualquer ponto da linha', async () => {
    render(
      <MemoryRouter>
        <Routes>
          <Route path="/" element={<TabelaExecucoes execucoes={[execucao({ configuracao: 'paredes A' })]} />} />
          <Route path="/execucoes/:id" element={<p>detalhe aberto</p>} />
        </Routes>
      </MemoryRouter>,
    )
    await userEvent.click(screen.getByText('paredes A'))
    expect(await screen.findByText('detalhe aberto')).toBeInTheDocument()
  })
})

describe('CamposRotulo', () => {
  it('avisa cada digitação com o campo certo', async () => {
    const mudancas: Array<{ configuracao?: string; observacao?: string }> = []
    render(<CamposRotulo configuracao="" observacao="" aoMudarRotulo={(c) => mudancas.push(c)} />)

    await userEvent.type(screen.getByLabelText(/Configuração de paredes/), 'A')
    await userEvent.type(screen.getByLabelText(/Observação/), 'x')

    expect(mudancas).toEqual([{ configuracao: 'A' }, { observacao: 'x' }])
  })
})

describe('Histórico com a campanha', () => {
  it('traz o resumo da campanha da API junto da tabela', async () => {
    servidor.use(http.get('*/api/execucoes/campanha', () => HttpResponse.json(campanha)))
    renderizarRota('/execucoes')

    const cartao = await screen.findByRole('region', { name: 'Resumo da campanha' })
    expect(within(cartao).getByText('80%')).toBeInTheDocument()
    await waitFor(() => expect(screen.getByRole('columnheader', { name: 'Configuração' })).toBeInTheDocument())
  })

  it('a nova execução manda a configuração e a observação que o operador preenche, sem os espaços das pontas', async () => {
    let corpo: unknown = null
    servidor.use(
      http.get('*/api/health-check/ultimo', () => {
        const dados = ultimoHealthCheckMock(Date.now(), '01-inicio')
        dados.tipo_dip = '4x4'
        return HttpResponse.json(dados)
      }),
      http.post('*/api/execucoes', async ({ request }) => {
        corpo = await request.json()
        return new HttpResponse(null, { status: 422 })
      }),
    )
    renderizarRota('/', '03-em-execucao')
    await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
    const modal = await screen.findByRole('dialog')
    await userEvent.type(within(modal).getByLabelText(/Configuração de paredes/), '  paredes B ')
    await userEvent.type(within(modal).getByLabelText(/Observação/), 'QA-05')
    await userEvent.click(within(modal).getByRole('button', { name: 'Confirmar nova execução' }))

    await waitFor(() => expect(corpo).toEqual({ tipo_labirinto: '4x4', configuracao: 'paredes B', observacao: 'QA-05' }))
  })

  it('sem preencher o rótulo, o pedido segue só com o labirinto', async () => {
    let corpo: unknown = null
    servidor.use(
      http.get('*/api/health-check/ultimo', () => {
        const dados = ultimoHealthCheckMock(Date.now(), '01-inicio')
        dados.tipo_dip = '4x4'
        return HttpResponse.json(dados)
      }),
      http.post('*/api/execucoes', async ({ request }) => {
        corpo = await request.json()
        return new HttpResponse(null, { status: 422 })
      }),
    )
    renderizarRota('/', '03-em-execucao')
    await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
    const modal = await screen.findByRole('dialog')
    await userEvent.click(within(modal).getByRole('button', { name: 'Confirmar nova execução' }))

    await waitFor(() => expect(corpo).toEqual({ tipo_labirinto: '4x4' }))
  })
})

describe('Filtro por configuração de paredes', () => {
  it('filtra o histórico, o resumo e a exportação pela configuração escolhida', async () => {
    const pedidos: string[] = []
    servidor.use(
      http.get('*/api/execucoes/campanha', ({ request }) => {
        pedidos.push(`campanha ${new URL(request.url).search}`)
        return HttpResponse.json(campanha)
      }),
      http.get('*/api/execucoes', ({ request }) => {
        pedidos.push(`lista ${new URL(request.url).search}`)
        return HttpResponse.json([])
      }),
    )
    renderizarRota('/execucoes')

    await userEvent.click(await screen.findByRole('button', { name: 'paredes A' }))

    await waitFor(() => {
      expect(pedidos.some((p) => p.startsWith('campanha') && p.includes('configuracao=paredes%20A'))).toBe(true)
      expect(pedidos.some((p) => p.startsWith('lista') && p.includes('configuracao=paredes%20A'))).toBe(true)
    })
    expect(screen.getByRole('link', { name: 'Baixar CSV' }).getAttribute('href')).toContain('configuracao=paredes%20A')
    expect(screen.getByRole('button', { name: 'paredes A' })).toHaveAttribute('aria-pressed', 'true')
    // as outras configurações continuam na lista: o filtro escolhido não apaga as opções
    expect(screen.getByRole('button', { name: 'Todas' })).toBeInTheDocument()
  })

  it('sem nenhuma configuração rotulada, o filtro nem aparece', async () => {
    servidor.use(
      http.get('*/api/execucoes/campanha', () =>
        HttpResponse.json({ ...campanha, por_configuracao: [{ ...campanha.por_configuracao[1] }] }),
      ),
    )
    renderizarRota('/execucoes')

    await screen.findByRole('region', { name: 'Resumo da campanha' })
    expect(screen.queryByRole('group', { name: 'Filtrar por configuração de paredes' })).not.toBeInTheDocument()
  })
})

