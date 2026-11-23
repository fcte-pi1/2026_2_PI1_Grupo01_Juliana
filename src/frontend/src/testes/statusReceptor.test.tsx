import { render, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { describe, expect, it, vi } from 'vitest'
import { StatusReceptor } from '../components/layout/StatusReceptor'
import { estadoDoReceptor } from '../hooks/useEstadoRobo'
import { servidor } from '../mocks/servidor'
import { renderizarRota } from './renderizar'

describe('estado do receptor a partir do sinal e do enlace da ponte', () => {
  it.each([
    [null, null, 'desconhecido'],
    [4, 'conectada', 'conectado'],
    [4, 'desligada', 'conectado'],
    [300, 'conectada', 'aberto'],
    [4, 'reconectando', 'reconectando'],
    [300, 'desligada', 'desconectado'],
    [null, 'desligada', 'sem-ponte'],
    [null, 'conectada', 'aberto'],
  ] as const)('sinal %s e ponte %s dão %s', (segundos, ponte, esperado) => {
    expect(estadoDoReceptor(segundos, ponte)).toBe(esperado)
  })
})

describe('receptor Bluetooth na barra lateral', () => {
  it('sinal recente mostra que o robô está chegando', () => {
    render(<StatusReceptor estado="conectado" segundosSemSinal={4.2} />)
    expect(screen.getByText('Recebendo sinal')).toBeInTheDocument()
    expect(screen.getByText('último sinal há 4 s')).toBeInTheDocument()
  })

  it('porta aberta sem sinal recente não afirma desconexão', () => {
    render(<StatusReceptor estado="aberto" segundosSemSinal={300} />)
    expect(screen.getByText('Porta aberta')).toBeInTheDocument()
    expect(screen.getByText('último sinal há 5 min')).toBeInTheDocument()
  })

  it('ponte desligada explica como ligar e não oferece reconectar', () => {
    render(<StatusReceptor estado="sem-ponte" aoReconectar={() => {}} />)
    expect(screen.getByText('Ponte desligada')).toBeInTheDocument()
    expect(screen.getByText(/scripts\/ambiente\.sh/)).toBeInTheDocument()
    expect(screen.queryByRole('button', { name: 'Reconectar' })).not.toBeInTheDocument()
  })

  it('o botão Reconectar chama o pedido e fica desabilitado enquanto reconecta', async () => {
    const aoReconectar = vi.fn()
    const { rerender } = render(<StatusReceptor estado="aberto" segundosSemSinal={90} aoReconectar={aoReconectar} />)
    await userEvent.click(screen.getByRole('button', { name: 'Reconectar' }))
    expect(aoReconectar).toHaveBeenCalledTimes(1)
    rerender(<StatusReceptor estado="reconectando" aoReconectar={aoReconectar} />)
    expect(screen.getByRole('button', { name: 'Reconectando…' })).toBeDisabled()
  })

  it('mostra o erro do pedido', () => {
    render(<StatusReceptor estado="aberto" aoReconectar={() => {}} erro="A ponte não está ligada." />)
    expect(screen.getByRole('alert')).toHaveTextContent('A ponte não está ligada.')
  })
})

describe('Reconectar na tela', () => {
  it('pede a reconexão ao back-end ao clicar', async () => {
    let pedidos = 0
    servidor.use(
      http.get('*/api/robo/estado', () => HttpResponse.json({
        ultimo_sinal_em: null, segundos_sem_sinal: 300, ponte: 'conectada', porta: '/dev/rfcomm0', reconexao_pedida: false,
      })),
      http.post('*/api/robo/reconectar', () => { pedidos += 1; return HttpResponse.json({ aceito: true }, { status: 202 }) }),
    )
    renderizarRota('/')
    expect(await screen.findByText('Porta aberta')).toBeInTheDocument()
    await userEvent.click(screen.getByRole('button', { name: 'Reconectar' }))
    await vi.waitFor(() => expect(pedidos).toBe(1))
  })

  it('avisa quando a ponte não está ligada (409)', async () => {
    servidor.use(
      http.get('*/api/robo/estado', () => HttpResponse.json({
        ultimo_sinal_em: null, segundos_sem_sinal: 300, ponte: 'conectada', porta: '/dev/rfcomm0', reconexao_pedida: false,
      })),
      http.post('*/api/robo/reconectar', () => HttpResponse.json({ detail: 'A ponte não está ligada.' }, { status: 409 })),
    )
    renderizarRota('/')
    await userEvent.click(await screen.findByRole('button', { name: 'Reconectar' }))
    expect(await screen.findByText('A ponte não está ligada.')).toBeInTheDocument()
  })
})
