import { screen, waitFor, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { describe, expect, it } from 'vitest'
import type { UltimoHealthCheck } from '../api/tipos'
import { healthCheck } from '../mocks/dados'
import { servidor } from '../mocks/servidor'
import { soBateriaReprovou } from '../hooks/useHealthCheck'
import { renderizarRota } from './renderizar'

function reprovadoSoPelaBateria(extra: Partial<Record<string, boolean>> = {}): UltimoHealthCheck {
  const itens = healthCheck(8).map((item) => ({
    ...item,
    aprovado: item.componente === 'bateria' ? false : extra[item.componente] ?? true,
    valor_lido: item.componente === 'bateria' ? 6861 : item.valor_lido,
  }))
  return {
    iniciado_em: new Date(Date.now() - 20_000).toISOString(),
    concluido_em: new Date(Date.now() - 10_000).toISOString(),
    valido_ate: null, aprovado: false, tipo_dip: '4x4', itens,
  }
}

function servirHealthCheck(resultado: UltimoHealthCheck) {
  servidor.use(http.get('*/api/health-check/ultimo', () => HttpResponse.json(resultado)))
}

describe('soBateriaReprovou', () => {
  it('só vale com os 8 itens, a bateria reprovada e os outros aprovados', () => {
    expect(soBateriaReprovou(reprovadoSoPelaBateria())).toEqual({ valorMv: 6861 })
    expect(soBateriaReprovou(reprovadoSoPelaBateria({ tof_esquerdo: false }))).toBeNull()
    const faltando = reprovadoSoPelaBateria()
    expect(soBateriaReprovou({ ...faltando, itens: faltando.itens.slice(1) })).toBeNull()
    expect(soBateriaReprovou({ ...reprovadoSoPelaBateria(), aprovado: true })).toBeNull()
    expect(soBateriaReprovou(null)).toBeNull()
  })
})

describe('largar com a bateria abaixo do padrão seguro', () => {
  it('mantém Nova execução ativo e pede confirmação antes de largar', async () => {
    const pedidos: unknown[] = []
    servirHealthCheck(reprovadoSoPelaBateria())
    servidor.use(http.post('*/api/execucoes', async ({ request }) => {
      pedidos.push(await request.json())
      return new HttpResponse(null, { status: 500 })
    }))
    renderizarRota('/')
    const iniciar = await screen.findByRole('button', { name: /Nova execução/ })
    await waitFor(() => expect(screen.getByText(/Só a bateria reprovou/)).toBeInTheDocument())
    expect(iniciar).toBeEnabled()

    await userEvent.click(iniciar)
    const aviso = within(await screen.findByRole('dialog', { name: 'Bateria abaixo do padrão seguro' }))
    expect(aviso.getByText('6861 mV')).toBeInTheDocument()
    expect(aviso.getByText('7000 mV')).toBeInTheDocument()
    expect(pedidos).toHaveLength(0)

    await userEvent.click(aviso.getByRole('button', { name: 'Cancelar' }))
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument()
    expect(pedidos).toHaveLength(0)

    await userEvent.click(iniciar)
    await userEvent.click(within(await screen.findByRole('dialog')).getByRole('button', { name: 'Continuar mesmo assim' }))
    await waitFor(() => expect(pedidos).toHaveLength(1))
    expect(pedidos[0]).toMatchObject({ tipo_labirinto: '4x4', ignorar_bateria: true })
  })

  it('não libera quando outro item também reprovou', async () => {
    servirHealthCheck(reprovadoSoPelaBateria({ tof_direito: false }))
    renderizarRota('/')
    expect(await screen.findByText(/Health-check reprovado/)).toBeInTheDocument()
    expect(screen.getByRole('button', { name: /Nova execução/ })).toBeDisabled()
  })

  it('o modal de nova execução também passa pelo aviso e envia a confirmação', async () => {
    const pedidos: unknown[] = []
    servirHealthCheck(reprovadoSoPelaBateria())
    servidor.use(http.post('*/api/execucoes', async ({ request }) => {
      pedidos.push(await request.json())
      return new HttpResponse(null, { status: 500 })
    }))
    renderizarRota('/', '03-em-execucao')
    await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
    const modal = within(await screen.findByRole('dialog', { name: 'Nova execução' }))
    const confirmar = await modal.findByRole('button', { name: 'Confirmar nova execução' })
    await waitFor(() => expect(confirmar).toBeEnabled())
    await userEvent.click(confirmar)
    await userEvent.click(within(await screen.findByRole('dialog', { name: 'Bateria abaixo do padrão seguro' }))
      .getByRole('button', { name: 'Continuar mesmo assim' }))
    await waitFor(() => expect(pedidos).toHaveLength(1))
    expect(pedidos[0]).toMatchObject({ ignorar_bateria: true })
  })

  it('a retomada pede a confirmação e manda ignorar_bateria', async () => {
    const corpos: unknown[] = []
    servirHealthCheck(reprovadoSoPelaBateria())
    servidor.use(http.post('*/api/execucoes/:id/retomar', async ({ request }) => {
      corpos.push(await request.json())
      return new HttpResponse(null, { status: 500 })
    }))
    renderizarRota('/?modal=retomar', '08-encerrada-como-falha')
    const modal = within(await screen.findByRole('dialog'))
    await userEvent.click(await modal.findByRole('checkbox', { name: /Robô posicionado/ }))
    const retomar = modal.getByRole('button', { name: 'Retomar da célula B2' })
    await waitFor(() => expect(retomar).toBeEnabled())
    await userEvent.click(retomar)
    await userEvent.click(within(await screen.findByRole('dialog', { name: 'Bateria abaixo do padrão seguro' }))
      .getByRole('button', { name: 'Continuar mesmo assim' }))
    await waitFor(() => expect(corpos).toHaveLength(1))
    expect(corpos[0]).toEqual({ ignorar_bateria: true })
  })
})
