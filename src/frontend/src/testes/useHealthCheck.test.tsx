import { act, cleanup, renderHook } from '@testing-library/react'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { api } from '../api/clienteApi'
import { buscarUltimoHealthCheck, solicitarHealthCheck } from '../api/endpoints'
import type { TipoLabirinto, UltimoHealthCheck } from '../api/tipos'
import { useHealthCheck } from '../hooks/useHealthCheck'
import { healthCheck } from '../mocks/dados'

vi.mock('../api/endpoints', () => ({
  buscarUltimoHealthCheck: vi.fn(),
  solicitarHealthCheck: vi.fn(),
}))

const consultar = vi.mocked(buscarUltimoHealthCheck)
const solicitar = vi.mocked(solicitarHealthCheck)
const INICIO = Date.parse('2026-10-22T12:00:00Z')
const iso = (instante: number) => new Date(instante).toISOString()

function aprovado(iniciado = Date.now() - 20_000): UltimoHealthCheck {
  return {
    iniciado_em: iso(iniciado),
    concluido_em: iso(Date.now()),
    valido_ate: iso(Date.now() + 420_000),
    aprovado: true,
    tipo_dip: '4x4',
    itens: healthCheck(8),
  }
}

function parcial(iniciado = Date.now()): UltimoHealthCheck {
  return {
    ...aprovado(iniciado),
    aprovado: null,
    concluido_em: null,
    valido_ate: null,
    itens: healthCheck(3).filter((item) => item.aprovado === true).map((item) => ({ ...item, aprovado: true })),
  }
}

async function avancar(ms: number) {
  await act(async () => { await vi.advanceTimersByTimeAsync(ms) })
}

async function montar() {
  const hook = renderHook(({ tipo }: { tipo: TipoLabirinto }) => useHealthCheck(tipo, true), {
    initialProps: { tipo: '4x4' as TipoLabirinto },
  })
  await act(async () => { await Promise.resolve() })
  return hook
}

describe('consulta e prazo do health-check independente da tentativa', () => {
  beforeEach(() => {
    vi.useFakeTimers()
    vi.setSystemTime(INICIO)
    vi.spyOn(api, 'agora').mockImplementation(() => Date.now())
    consultar.mockReset().mockResolvedValue(null)
    solicitar.mockReset().mockImplementation(async () => ({ solicitado_em: iso(Date.now()) }))
  })

  afterEach(() => {
    cleanup()
    vi.restoreAllMocks()
    vi.useRealTimers()
  })

  it('consulta imediatamente e usa intervalo de dez segundos quando não há verificação pendente', async () => {
    await montar()
    expect(consultar).toHaveBeenCalledTimes(1)

    await avancar(9_000)
    expect(consultar).toHaveBeenCalledTimes(1)
    await avancar(1_000)
    expect(consultar).toHaveBeenCalledTimes(2)
  })

  it('consulta a cada segundo durante o pedido e só libera após um resultado novo concluído', async () => {
    let resposta: UltimoHealthCheck = aprovado()
    consultar.mockImplementation(async () => resposta)
    const { result } = await montar()
    expect(result.current.apto).toBe(true)

    await act(async () => { await result.current.solicitar() })
    expect(result.current.verificando).toBe(true)
    expect(result.current.apto).toBe(false)
    expect(result.current.dados).toBeNull()

    resposta = parcial(INICIO)
    await avancar(1_000)
    expect(result.current.dados?.aprovado).toBeNull()
    expect(result.current.verificando).toBe(true)
    expect(result.current.apto).toBe(false)

    resposta = aprovado(INICIO)
    await avancar(1_000)
    expect(result.current.verificando).toBe(false)
    expect(result.current.apto).toBe(true)
    const consultasConcluidas = consultar.mock.calls.length
    await avancar(9_000)
    expect(consultar).toHaveBeenCalledTimes(consultasConcluidas)
    await avancar(1_000)
    expect(consultar.mock.calls.length).toBeGreaterThan(consultasConcluidas)
  })

  it('encerra a espera de um resultado parcial pelo horário do servidor, inclusive após recarregar', async () => {
    consultar.mockResolvedValue(parcial(INICIO - 25_000))
    const { result } = await montar()
    expect(result.current.verificando).toBe(true)
    await avancar(4_000)
    expect(result.current.verificando).toBe(true)
    await avancar(1_000)

    expect(result.current.erro).toBeTruthy()
    expect(result.current.verificando).toBe(false)
    expect(result.current.solicitando).toBe(false)
    expect(result.current.apto).toBe(false)

    await avancar(10_000)
    expect(result.current.erro).toBeTruthy()
    expect(result.current.verificando).toBe(false)
    expect(result.current.apto).toBe(false)
  })

  it('libera uma nova solicitação após trinta segundos sem aceitar a aprovação anterior', async () => {
    consultar.mockResolvedValue(aprovado())
    const { result } = await montar()
    await act(async () => { await result.current.solicitar() })
    await avancar(30_000)

    expect(result.current.erro).toBeTruthy()
    expect(result.current.verificando).toBe(false)
    expect(result.current.solicitando).toBe(false)
    expect(result.current.apto).toBe(false)
    expect(result.current.dados).toBeNull()

    await avancar(10_000)
    expect(result.current.erro).toBeTruthy()
    expect(result.current.apto).toBe(false)
    expect(result.current.dados).toBeNull()
  })

  it('preserva o pedido e o prazo ao trocar de labirinto, verificando o DIP do resultado tardio', async () => {
    let resposta = aprovado()
    consultar.mockImplementation(async () => resposta)
    const { result, rerender } = await montar()
    await act(async () => { await result.current.solicitar() })
    await avancar(25_000)
    rerender({ tipo: '8x4' })
    await avancar(5_000)

    expect(result.current.erro).toBeTruthy()
    expect(result.current.verificando).toBe(false)
    expect(result.current.apto).toBe(false)
    expect(result.current.dados).toBeNull()

    resposta = aprovado(INICIO)
    await avancar(10_000)
    expect(result.current.erro).toBeNull()
    expect(result.current.apto).toBe(false)
    rerender({ tipo: '4x4' })
    await act(async () => { await Promise.resolve() })
    expect(result.current.apto).toBe(true)
  })

  it('reinicia prazo e referência no novo pedido, impedindo POST duplicado e resultado do pedido anterior', async () => {
    let resposta = aprovado()
    consultar.mockImplementation(async () => resposta)
    const { result } = await montar()
    await act(async () => { await result.current.solicitar() })
    await avancar(30_000)

    let liberar!: (pedido: { solicitado_em: string }) => void
    const recebimento = new Promise<{ solicitado_em: string }>((resolve) => { liberar = resolve })
    solicitar.mockReturnValue(recebimento)
    const novoPedido = Date.now()
    let envio!: Promise<void>
    act(() => {
      envio = result.current.solicitar()
      void result.current.solicitar()
    })
    expect(solicitar).toHaveBeenCalledTimes(2)
    expect(result.current.solicitando).toBe(true)
    await act(async () => {
      liberar({ solicitado_em: iso(novoPedido) })
      await envio
    })
    expect(result.current.verificando).toBe(true)
    expect(result.current.erro).toBeNull()

    resposta = aprovado(INICIO)
    await avancar(10_000)
    expect(result.current.verificando).toBe(true)
    expect(result.current.apto).toBe(false)
    expect(result.current.dados).toBeNull()

    resposta = aprovado(novoPedido)
    await avancar(1_000)
    expect(result.current.verificando).toBe(false)
    expect(result.current.apto).toBe(true)
  })

  it('recupera a aprovação quando o resultado novo chega depois do limite de espera', async () => {
    let resposta = parcial(INICIO)
    consultar.mockImplementation(async () => resposta)
    const { result } = await montar()
    await avancar(30_000)
    expect(result.current.erro).toBeTruthy()
    expect(result.current.apto).toBe(false)

    resposta = aprovado(INICIO)
    await avancar(10_000)
    expect(result.current.erro).toBeNull()
    expect(result.current.verificando).toBe(false)
    expect(result.current.apto).toBe(true)
  })

  it('expira a aprovação pelo relógio local mesmo quando a próxima consulta não responde', async () => {
    const resposta = { ...aprovado(), valido_ate: iso(INICIO + 12_000) }
    consultar.mockResolvedValueOnce(resposta).mockImplementation(() => new Promise(() => {}))
    const { result } = await montar()
    expect(result.current.apto).toBe(true)
    await avancar(10_000)
    expect(consultar.mock.calls.length).toBeGreaterThan(1)
    expect(result.current.apto).toBe(true)

    await avancar(3_000)
    expect(result.current.valido).toBe(false)
    expect(result.current.apto).toBe(false)
  })
})
