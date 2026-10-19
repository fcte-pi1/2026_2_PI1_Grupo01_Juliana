import { act, render, renderHook, screen, waitFor, within } from '@testing-library/react'
import { http, HttpResponse } from 'msw'
import { MemoryRouter } from 'react-router'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { api, ClienteAPI } from '../api/clienteApi'
import { buscarExecucao, retomarTentativa } from '../api/endpoints'
import { aplicarEvento, combinarRetrato, lerEvento } from '../api/eventosExecucao'
import type { components } from '../api/schema'
import type { Execucao } from '../api/tipos'
import { buscarCenario } from '../mocks/cenarios'
import { detalheAPI, LABIRINTOS } from '../mocks/dados'
import { servidor } from '../mocks/servidor'
import { useExecucaoAtual } from '../hooks/useExecucaoAtual'
import { derivarEstadoInicio } from '../pages/inicio/estadoInicio'
import { PainelAoVivo } from '../pages/inicio/PainelAoVivo'
import { formatarTempo } from '../utils/formatacao'
import { renderizarRota } from './renderizar'

type Telemetria = components['schemas']['EventoTelemetria']

/** Transporte controlado; os dados abaixo são exemplos do contrato e dos mocks. */
class EventSourceControlado extends EventTarget {
  static instancias: EventSourceControlado[] = []
  static CONNECTING = 0
  static OPEN = 1
  static CLOSED = 2
  readonly url: string
  readyState = EventSourceControlado.CONNECTING
  onopen: ((evento: Event) => unknown) | null = null
  onerror: ((evento: Event) => unknown) | null = null
  onmessage: ((evento: MessageEvent<string>) => unknown) | null = null
  close = vi.fn(() => { this.readyState = EventSourceControlado.CLOSED })

  constructor(url: string | URL) {
    super()
    this.url = String(url)
    EventSourceControlado.instancias.push(this)
  }

  abrir() {
    this.readyState = EventSourceControlado.OPEN
    const evento = new Event('open')
    this.dispatchEvent(evento)
    this.onopen?.(evento)
  }

  falhar() {
    this.readyState = EventSourceControlado.CONNECTING
    const evento = new Event('error')
    this.dispatchEvent(evento)
    this.onerror?.(evento)
  }

  emitir(nome: string, dados: unknown, id = '') {
    this.emitirTexto(nome, JSON.stringify(dados), id)
  }

  emitirTexto(nome: string, dados: string, id = '') {
    const evento = new MessageEvent<string>(nome, { data: dados, lastEventId: id })
    this.dispatchEvent(evento)
    if (nome === 'message') this.onmessage?.(evento)
  }
}

let dados: Execucao
let consultasDetalhe: number

function leitura(agora = Date.now()): Telemetria {
  return {
    tipo: 'telemetria', ordem: 42, x: 0, y: 2, bateria: 7810, bateria_pct: 56,
    velocidade: 210, rumo: 'N', eixo_longo: null,
    enviado_em: new Date(agora).toISOString(),
  }
}

function cartao(rotulo: string) {
  const elemento = screen.getAllByText(rotulo).find((item) => item.classList.contains('rotulo'))
  expect(elemento).toBeDefined()
  return within(elemento!.closest('section')!)
}

async function conectar() {
  await screen.findByRole('heading', { name: 'Execução ativa #0042' })
  await waitFor(() => expect(EventSourceControlado.instancias).toHaveLength(1))
  const fonte = EventSourceControlado.instancias[0]
  act(() => fonte.abrir())
  await waitFor(() => expect(consultasDetalhe).toBeGreaterThan(0))
  return fonte
}

beforeEach(() => {
  EventSourceControlado.instancias = []
  vi.stubGlobal('EventSource', EventSourceControlado)
  consultasDetalhe = 0
  dados = buscarCenario('03-em-execucao').atual(Date.now()).execucao!
  servidor.use(
    http.get('*/api/execucoes/em-andamento', () => HttpResponse.json(detalheAPI(dados))),
    http.get('*/api/execucoes/:id', () => {
      consultasDetalhe += 1
      return HttpResponse.json(detalheAPI(dados))
    }),
  )
})

afterEach(() => {
  vi.unstubAllGlobals()
  vi.restoreAllMocks()
})

describe('FRONT-03: acompanhamento SSE', () => {
  it.each(['04-concluida', '12-falha-sem-retomada'])('restaura a execução selecionada %s pela URL mesmo sem execução em andamento', async (cenario) => {
    dados = buscarCenario(cenario).atual(Date.now()).execucao!
    let consultasAtuais = 0
    servidor.use(http.get('*/api/execucoes/em-andamento', () => {
      consultasAtuais += 1
      return new HttpResponse(null, { status: 204 })
    }))
    const titulo = dados.status === 'concluida' ? 'Execução concluída #0042' : 'Execução cancelada #0042'
    const caminho = `/?execucao=${dados.execucao_id}`
    const primeira = renderizarRota(caminho)
    await screen.findByRole('heading', { name: titulo })
    expect(screen.getByRole('heading', { name: 'Trajeto percorrido' })).toBeInTheDocument()
    expect(consultasDetalhe).toBeGreaterThan(0)
    expect(consultasAtuais).toBe(0)
    const consultasAntes = consultasDetalhe
    primeira.unmount()
    renderizarRota(caminho)
    await screen.findByRole('heading', { name: titulo })
    expect(screen.getByRole('heading', { name: 'Trajeto percorrido' })).toBeInTheDocument()
    expect(consultasDetalhe).toBeGreaterThan(consultasAntes)
    expect(consultasAtuais).toBe(0)
  })

  it('sem seleção na URL mantém o início quando a API responde 204', async () => {
    servidor.use(http.get('*/api/execucoes/em-andamento', () => new HttpResponse(null, { status: 204 })))
    renderizarRota('/')
    await screen.findByRole('heading', { name: 'Nenhuma execução ativa' })
    expect(screen.queryByRole('heading', { name: /Execução concluída|Execução cancelada/ })).not.toBeInTheDocument()
    expect(screen.queryByRole('heading', { name: 'Trajeto percorrido' })).not.toBeInTheDocument()
    expect(consultasDetalhe).toBe(0)
  })

  it('aplica telemetria nomeada em até 2 s preservando os valores e as unidades', async () => {
    renderizarRota('/')
    const fonte = await conectar()
    const recebidaEm = performance.now()
    act(() => fonte.emitir('telemetria', leitura()))
    await waitFor(() => {
      expect(cartao('Célula atual').getByText('A3')).toBeInTheDocument()
      expect(cartao('Bateria').getByText(/^7810/)).toHaveTextContent('mV')
      expect(cartao('Velocidade').getByText(/^210/)).toHaveTextContent('mm/s')
    }, { timeout: 2_000 })
    expect(performance.now() - recebidaEm).toBeLessThan(2_000)
    expect(cartao('Bateria').queryByText(/7,81/)).not.toBeInTheDocument()
    expect(cartao('Velocidade').queryByText(/0,21/)).not.toBeInTheDocument()
  })

  it('aplica health_check e status nomeados e mantém os resultados visíveis em running', async () => {
    dados = buscarCenario('02-health-check').atual(Date.now()).execucao!
    dados.tentativas[0].health_check = []
    renderizarRota('/')
    const fonte = await conectar()
    act(() => fonte.emitir('health_check', {
      tipo: 'health_check', tentativa_id: dados.tentativas[0].tentativa_id,
      componente: 'bateria', aprovado: true, valor_lido: 7840,
    }))
    const bloco = screen.getByRole('heading', { name: 'Health-check' }).closest('section')!
    await waitFor(() => expect(within(bloco).getByText(/7840/)).toHaveTextContent('mV · OK'))

    dados.tentativas[0].status = 'running'
    dados.tentativas[0].health_check = [{ componente: 'bateria', aprovado: true, valor_lido: 7840 }]
    const anteriores = consultasDetalhe
    act(() => fonte.emitir('status', {
      tipo: 'status', tentativa_id: dados.tentativas[0].tentativa_id, attempt_index: 1,
      status_tentativa: 'running', status_execucao: 'em_andamento', tipo_dip: '4x4',
    }))
    await waitFor(() => expect(consultasDetalhe).toBeGreaterThan(anteriores))
    expect(screen.getByLabelText('Tentativa 1 de 3')).toBeInTheDocument()
    expect(screen.getByRole('heading', { name: 'Execução ativa #0042' })).toBeInTheDocument()
    expect(screen.getByRole('heading', { name: 'Health-check' })).toBeInTheDocument()
    await waitFor(() => expect(cartao('Tempo da tentativa').getByText(/min:s/, { selector: '.tempo__cronometro .metrica__unidade' })).toBeInTheDocument())
  })

  it.each(['parcial', 'reprovado'] as const)('não afirma aprovação de um health-check %s após uma falha', async (resultado) => {
    dados = buscarCenario('08-encerrada-como-falha').atual(Date.now()).execucao!
    const tentativa = dados.tentativas[0]
    tentativa.health_check = resultado === 'parcial'
      ? tentativa.health_check.slice(0, 1)
      : tentativa.health_check.map((item) => item.componente === 'tof_frontal' ? { ...item, aprovado: false } : item)
    renderizarRota('/')
    await screen.findByRole('heading', { name: 'Execução ativa #0042' })
    expect(screen.queryByText(/^aprovado ·/)).not.toBeInTheDocument()
    expect(screen.queryByText('Em execução', { selector: 'strong' })).not.toBeInTheDocument()
    const componente = screen.getByText('ToF frontal').closest('li')!
    expect(componente).toHaveTextContent(resultado === 'parcial' ? 'pendente' : 'falhou')
    if (resultado === 'reprovado') expect(componente).toHaveClass('health-check__item--falha')
  })

  it('chama de tentativa encerrada a falha que ainda permite retomar', async () => {
    dados = buscarCenario('08-encerrada-como-falha').atual(Date.now()).execucao!
    renderizarRota('/')
    await screen.findByRole('heading', { name: 'Execução ativa #0042' })
    expect(screen.getByText('Início · tentativa encerrada')).toBeInTheDocument()
    expect(screen.queryByText('Início · execução encerrada')).not.toBeInTheDocument()
  })

  it('descarta JSON inválido e tipos divergentes e conserva a conexão durante o retry nativo', () => {
    const receber = vi.fn()
    const abrir = vi.fn()
    const encerrar = api.assinar(dados.execucao_id, receber, abrir)
    const fonte = EventSourceControlado.instancias[0]
    fonte.abrir()
    fonte.emitirTexto('telemetria', '{')
    fonte.emitir('telemetria', { tipo: 'status', status_execucao: 'concluida' })
    fonte.emitir('message', leitura())
    expect(receber).not.toHaveBeenCalled()
    fonte.emitir('telemetria', leitura())
    expect(receber).toHaveBeenCalledTimes(1)
    fonte.falhar()
    expect(fonte.close).not.toHaveBeenCalled()
    fonte.abrir()
    expect(abrir).toHaveBeenCalledTimes(2)
    expect(EventSourceControlado.instancias).toHaveLength(1)
    encerrar()
    expect(fonte.close).toHaveBeenCalledTimes(1)
  })

  it('mostra o aviso após 5 s sem dados e o remove quando chega uma amostra nova', async () => {
    const agora = Date.now()
    dados.ultima_mensagem_em = new Date(agora).toISOString()
    expect(derivarEstadoInicio(dados, agora + 4_999)).toBe('em-execucao')
    expect(derivarEstadoInicio(dados, agora + 5_001)).toBe('sem-comunicacao')
    dados.ultima_mensagem_em = new Date(agora - 6_000).toISOString()
    renderizarRota('/')
    const fonte = await conectar()
    expect(await screen.findByText(/Sem dados do robô há/)).toBeInTheDocument()
    act(() => fonte.emitir('telemetria', leitura()))
    await waitFor(() => expect(screen.queryByText(/Sem dados do robô há/)).not.toBeInTheDocument())
  })

  it('reconsulta os dados finais no sucesso e mantém a última célula, bateria e velocidade', async () => {
    renderizarRota('/')
    const fonte = await conectar()
    dados = buscarCenario('04-concluida').atual(Date.now()).execucao!
    dados.tentativas[0].bateria_final = 7560
    dados.leituras = [{ ordem: 42, x: 3, y: 3, bateria: 7560, velocidade: 0, enviado_em: dados.encerrada_em! }]
    const anteriores = consultasDetalhe
    act(() => fonte.emitir('sucesso', {
      tipo: 'sucesso', tentativa_id: dados.tentativas[0].tentativa_id,
      x: 3, y: 3, status_execucao: 'concluida',
    }))
    await screen.findByRole('heading', { name: 'Execução concluída #0042' })
    await waitFor(() => {
      expect(consultasDetalhe).toBeGreaterThan(anteriores)
      expect(cartao('Tempo da execução').getByText(formatarTempo(33.1))).toBeInTheDocument()
      expect(cartao('Tempo da tentativa').getByText(formatarTempo(25.3))).toBeInTheDocument()
      expect(cartao('Célula atual').getByText('D4')).toBeInTheDocument()
      expect(cartao('Bateria').getByText(/^7560/)).toHaveTextContent('mV')
      expect(cartao('Velocidade').getByText(/^0(?:,0+)?$/)).toHaveTextContent('mm/s')
    })
  })

  it('congela os tempos fornecidos pelo backend mesmo quando o relógio da tela avança', () => {
    const agora = Date.now()
    const encerrada = buscarCenario('04-concluida').atual(agora).execucao!
    const propriedades = { execucao: encerrada, tentativa: encerrada.tentativas[0], labirinto: LABIRINTOS[0] }
    const tela = render(<PainelAoVivo {...propriedades} agora={agora} />)
    expect(cartao('Tempo da execução').getByText(formatarTempo(33.1))).toBeInTheDocument()
    expect(cartao('Tempo da tentativa').getByText(formatarTempo(25.3))).toBeInTheDocument()
    tela.rerender(<PainelAoVivo {...propriedades} agora={agora + 60_000} />)
    expect(cartao('Tempo da execução').getByText(formatarTempo(33.1))).toBeInTheDocument()
    expect(cartao('Tempo da tentativa').getByText(formatarTempo(25.3))).toBeInTheDocument()
  })

  it('restaura a célula da falha persistida mesmo com a última leitura anterior à falha', async () => {
    const primeira = renderizarRota('/')
    const fonte = await conectar()
    expect(cartao('Célula atual').getByText('B4')).toBeInTheDocument()
    const final = buscarCenario('08-encerrada-como-falha').atual(Date.now()).execucao!
    dados.tentativas = final.tentativas
    act(() => fonte.emitir('status', {
      tipo: 'status', tentativa_id: dados.tentativas[0].tentativa_id, attempt_index: 1,
      status_tentativa: 'failed', status_execucao: 'em_andamento',
    }))
    await waitFor(() => expect(cartao('Célula atual').getByText('B2')).toBeInTheDocument())
    primeira.unmount()
    renderizarRota('/')
    await screen.findByRole('heading', { name: 'Execução ativa #0042' })
    expect(cartao('Célula atual').getByText('B2')).toBeInTheDocument()
  })

  it('restaura a leitura e o health-check pelo GET ao remontar e fecha o stream anterior', async () => {
    const primeira = renderizarRota('/')
    const fonte = await conectar()
    const amostra = leitura()
    dados.leituras = [{ ...amostra, velocidade: amostra.velocidade ?? null }]
    dados.ultima_mensagem_em = amostra.enviado_em
    dados.tentativas[0].health_check = [{ componente: 'bateria', aprovado: true, valor_lido: 7840 }]
    act(() => fonte.emitir('telemetria', { tipo: 'telemetria', ...amostra }))
    await waitFor(() => expect(cartao('Célula atual').getByText('A3')).toBeInTheDocument())
    primeira.unmount()
    expect(fonte.close).toHaveBeenCalledTimes(1)
    renderizarRota('/')
    await screen.findByRole('heading', { name: 'Execução ativa #0042' })
    expect(cartao('Célula atual').getByText('A3')).toBeInTheDocument()
    expect(cartao('Bateria').getByText(/^7810/)).toHaveTextContent('mV')
    const healthCheck = screen.getByRole('heading', { name: 'Health-check' }).closest('section')!
    expect(within(healthCheck).getByText(/7840/)).toHaveTextContent('mV · OK')
    await waitFor(() => expect(EventSourceControlado.instancias).toHaveLength(2))
  })

  it('reenvio de um passo não duplica o trajeto nem recua a célula atual', () => {
    dados.trajeto = []
    const passo: components['schemas']['EventoPasso'] = { tipo: 'passo', passo_id: 42, seq: 1, x: 0, y: 2, paredes_mask: 8, retomada: false }
    const primeira = aplicarEvento(dados, passo)
    const segunda = aplicarEvento(primeira, passo)
    expect(segunda.trajeto).toHaveLength(1)
    expect(segunda.celula_atual).toEqual({ x: 0, y: 2 })
    const anterior = aplicarEvento(segunda, { ...passo, passo_id: 41, x: 0, y: 1 })
    expect(anterior.celula_atual).toEqual({ x: 0, y: 2 })
  })

  it('republicação do mesmo passo atualiza paredes sem duplicar nem mover a posição', () => {
    dados.trajeto = [{ passo_id: 41, seq: 1, x: 0, y: 1, paredes_mask: 8, retomada: false }]
    dados.celula_atual = { x: 0, y: 2 }
    dados.ultima_atualizacao_em = Date.now() - 6_000
    const atualizada = aplicarEvento(dados, {
      tipo: 'passo', ...dados.trajeto[0], passo_id: 41, paredes_mask: 9,
    }, Date.now())
    expect(atualizada.trajeto).toHaveLength(1)
    expect(atualizada.trajeto[0].paredes_mask).toBe(9)
    expect(atualizada.celula_atual).toEqual(dados.celula_atual)
    expect(atualizada.ultima_atualizacao_em).toBe(dados.ultima_atualizacao_em)
  })

  it('conserva orientação, rumo e percentual do SSE ao reconciliar a mesma leitura pelo REST', () => {
    const evento = { ...leitura(), bateria_pct: 56.123456, eixo_longo: 'y' as const }
    const atualizada = aplicarEvento(dados, evento)
    const retrato = { ...dados, leituras: [{ ordem: evento.ordem, x: evento.x, y: evento.y,
      bateria: evento.bateria, velocidade: evento.velocidade ?? null, enviado_em: evento.enviado_em,
      bateria_pct: null, rumo: null }] }
    const combinada = combinarRetrato(atualizada, retrato)
    expect(combinada.eixo_longo).toBe('y')
    expect(combinada.leituras.at(-1)).toMatchObject({ bateria: 7810, velocidade: 210, bateria_pct: 56.123456, rumo: 'N' })
    // Uma resposta REST que chegou primeiro também pode receber os campos extras.
    expect(aplicarEvento(retrato, evento).leituras.at(-1)?.bateria_pct).toBe(56.123456)
  })

  it('preserva valores e unidades do GET sem calcular percentuais nem arredondar a comparação', async () => {
    const detalhe = detalheAPI(dados)
    servidor.use(http.get('*/api/execucoes/:id', () => HttpResponse.json({ ...detalhe,
      variacao_melhor_tempo_pct: -5.123456,
      leituras: [{ ordem: 42, x: 0, y: 2, bateria: 7810, velocidade: 210, rumo: 'N', enviado_em: new Date().toISOString() }],
      tentativas: [{ ...detalhe.tentativas[0], bateria_inicial: 7840, bateria_final: 7560,
        consumo_bateria: 280, consumo_bateria_pct: 28.123456, velocidade_media: 0.078123456 }],
    })))
    const restaurada = await buscarExecucao(dados.execucao_id)
    expect(restaurada.variacao_melhor_tempo_pct).toBe(-5.123456)
    expect(restaurada.leituras[0]).toMatchObject({ bateria: 7810, velocidade: 210, bateria_pct: null, rumo: 'N' })
    expect(restaurada.tentativas[0]).toMatchObject({ bateria_inicial: 7840, bateria_final: 7560,
      consumo_bateria: 280, consumo_bateria_pct: 28.123456, velocidade_media: 0.078123456 })
  })

  it('mantém comparação e consumo percentual pendentes quando o GET não os informa', async () => {
    const detalhe = detalheAPI(dados)
    servidor.use(http.get('*/api/execucoes/:id', () => HttpResponse.json({ ...detalhe,
      variacao_melhor_tempo_pct: null,
      tentativas: [{ ...detalhe.tentativas[0], consumo_bateria_pct: null }],
    })))
    const restaurada = await buscarExecucao(dados.execucao_id)
    expect(restaurada.variacao_melhor_tempo_pct).toBeNull()
    expect(restaurada.tentativas[0].consumo_bateria_pct).toBeNull()
  })

  it.each([
    { bateria_pct: -1 }, { bateria_pct: 101 }, { bateria_pct: '56' }, { rumo: 'NE' }, { eixo_longo: 'z' },
  ])('descarta telemetria com campos adicionais inválidos: %j', (invalido) => {
    expect(lerEvento('telemetria', JSON.stringify({ ...leitura(), ...invalido }))).toBeNull()
  })

  it('uma consulta antiga na reconexão não substitui a amostra SSE mais recente', () => {
    const atualizada = aplicarEvento(dados, leitura())
    const combinada = combinarRetrato(atualizada, dados)
    expect(combinada.leituras.at(-1)?.ordem).toBe(42)
    expect(combinada.leituras.at(-1)?.bateria).toBe(7810)
    expect(combinada.celula_atual).toEqual({ x: 0, y: 2 })
  })

  it('não associa à tentativa atual uma amostra enviada antes do seu início', () => {
    const inicio = Date.parse(dados.tentativas[0].iniciada_em)
    const anterior = leitura(inicio - 1)
    expect(aplicarEvento(dados, anterior)).toBe(dados)
  })

  it('replay de um passo faltante completa o trajeto sem renovar o aviso de atualização', () => {
    dados.trajeto = [{ passo_id: 3, seq: 3, x: 0, y: 2, paredes_mask: 8, retomada: false }]
    dados.ultima_atualizacao_em = Date.now() - 6_000
    const replay = aplicarEvento(dados, { tipo: 'passo', passo_id: 2, seq: 2, x: 0, y: 1, paredes_mask: 8, retomada: false }, Date.now())
    expect(replay.trajeto).toHaveLength(2)
    expect(replay.ultima_atualizacao_em).toBe(dados.ultima_atualizacao_em)
    expect(derivarEstadoInicio(replay, Date.now())).toBe('sem-comunicacao')
  })

  it('uma falha atrasada da tentativa anterior não altera a célula nem o aviso da tentativa atual', () => {
    const agora = Date.now()
    dados = aplicarEvento(buscarCenario('10-retomada-tentativa-2').atual(agora).execucao!, leitura(agora), agora - 6_000)
    const anterior = dados.tentativas[0]
    const atualizada = aplicarEvento(dados, {
      tipo: 'falha', tentativa_id: anterior.tentativa_id, status_execucao: 'em_andamento', ...anterior.falha!,
    }, agora)
    expect(atualizada.celula_atual).toEqual({ x: 0, y: 2 })
    expect(atualizada.ultima_atualizacao_em).toBe(dados.ultima_atualizacao_em)
    expect(atualizada.tentativas[1].status).toBe('running')
    expect(derivarEstadoInicio(atualizada, agora)).toBe('sem-comunicacao')
  })

  it('um passo reenviado anterior à última telemetria completa o mapa sem recuar a posição nem renovar o alerta', () => {
    const agora = Date.now()
    const atualizada = aplicarEvento(dados, leitura(agora), agora - 6_000)
    const replay = aplicarEvento(atualizada, {
      tipo: 'passo', passo_id: 100, seq: 100, x: 0, y: 1, paredes_mask: 8, retomada: false,
      entrou_em: new Date(agora - 1_000).toISOString(),
    }, agora)
    expect(replay.trajeto).toHaveLength(atualizada.trajeto.length + 1)
    expect(replay.celula_atual).toEqual({ x: 0, y: 2 })
    expect(replay.ultima_atualizacao_em).toBe(atualizada.ultima_atualizacao_em)
    expect(derivarEstadoInicio(replay, agora)).toBe('sem-comunicacao')
  })

  it('um passo anterior ao início da tentativa atual completa só o histórico', () => {
    const agora = Date.now()
    dados = aplicarEvento(buscarCenario('10-retomada-tentativa-2').atual(agora).execucao!, leitura(agora), agora - 6_000)
    const passo = dados.trajeto[0]
    const replay = aplicarEvento(dados, {
      tipo: 'passo', ...passo, passo_id: 42,
      entrou_em: new Date(Date.parse(dados.tentativas[1].iniciada_em) - 1).toISOString(),
    }, agora)
    expect(replay.trajeto).toHaveLength(dados.trajeto.length + 1)
    expect(replay.celula_atual).toEqual(dados.celula_atual)
    expect(replay.ultima_atualizacao_em).toBe(dados.ultima_atualizacao_em)
  })

  it('um passo recebido após o término completa o histórico sem mover a célula final', () => {
    dados = buscarCenario('04-concluida').atual(Date.now()).execucao!
    dados.celula_atual = { x: 3, y: 3 }
    const replay = aplicarEvento(dados, { tipo: 'passo', ...dados.trajeto[0], passo_id: 42 }, Date.now())
    expect(replay.trajeto).toHaveLength(dados.trajeto.length + 1)
    expect(replay.celula_atual).toEqual(dados.celula_atual)
    expect(replay.ultima_atualizacao_em).toBe(dados.ultima_atualizacao_em)
  })

  it('mantém a célula de chegada do evento sucesso sem fabricar bateria ou velocidade', () => {
    const ultima = dados.leituras.at(-1)
    const encerrada = aplicarEvento(dados, { tipo: 'sucesso', tentativa_id: dados.tentativas[0].tentativa_id, x: 3, y: 3, status_execucao: 'concluida' })
    expect(encerrada.celula_atual).toEqual({ x: 3, y: 3 })
    expect(encerrada.leituras.at(-1)).toEqual(ultima)
  })

  it('descarta uma falha com observação inválida em vez de quebrar a tela', () => {
    expect(lerEvento('falha', JSON.stringify({ tipo: 'falha', tentativa_id: dados.tentativas[0].tentativa_id,
      motivo: 'stuck', origem: 'automatica', celula_x: 0, celula_y: 2, momento_falha: new Date().toISOString(),
      status_execucao: 'cancelada', observacao: { texto: 'inválido' } }))).toBeNull()
  })

  it('sincroniza o relógio com o cabeçalho Date do servidor', async () => {
    const horarioServidor = new Date(Date.now() - 60_000).toUTCString()
    servidor.use(http.get('http://teste/api/execucoes', () => HttpResponse.json([], { headers: { Date: horarioServidor } })))
    const cliente = new ClienteAPI('http://teste/api')
    await cliente.get('/execucoes')
    expect(Math.abs(cliente.agora() - Date.parse(horarioServidor))).toBeLessThan(1_000)
    expect(Date.now() - cliente.agora()).toBeGreaterThan(59_000)
  })

  it('guarda a amostra de uma nova tentativa até o GET confirmar seu ID após uma falha temporária', async () => {
    dados = buscarCenario('08-encerrada-como-falha').atual(Date.now()).execucao!
    renderizarRota('/')
    const fonte = await conectar()
    const nova = buscarCenario('10-retomada-tentativa-2').atual(Date.now()).execucao!
    nova.leituras = []
    nova.ultima_mensagem_em = null
    servidor.use(http.get('*/api/execucoes/:id', () => new HttpResponse(null, { status: 503 })))
    act(() => fonte.emitir('status', { tipo: 'status', tentativa_id: nova.tentativas[1].tentativa_id,
      attempt_index: 2, status_tentativa: 'running', status_execucao: 'em_andamento' }))
    await screen.findByRole('button', { name: 'Atualizar dados' })
    act(() => fonte.emitir('telemetria', { ...leitura(), ordem: 1 }))
    servidor.use(http.get('*/api/execucoes/:id', () => HttpResponse.json(detalheAPI(nova))))
    act(() => screen.getByRole('button', { name: 'Atualizar dados' }).click())
    await screen.findByLabelText('Tentativa 2 de 3')
    await waitFor(() => expect(cartao('Bateria').getByText(/^7810/)).toHaveTextContent('mV'))
    expect(cartao('Célula atual').getByText('A3')).toBeInTheDocument()
    expect(EventSourceControlado.instancias).toHaveLength(1)
  })

  it.each(['telemetria', 'passo'] as const)('recarrega o detalhe após retomar sem perder %s durante um GET lento', async (tipo) => {
    dados = buscarCenario('08-encerrada-como-falha').atual(Date.now()).execucao!
    const { result } = renderHook(() => useExecucaoAtual(), {
      wrapper: ({ children }) => <MemoryRouter>{children}</MemoryRouter>,
    })
    await waitFor(() => expect(result.current.dados?.execucao?.execucao_id).toBe(dados.execucao_id))
    await waitFor(() => expect(EventSourceControlado.instancias).toHaveLength(1))
    const fonte = EventSourceControlado.instancias[0]
    const retomada = buscarCenario('10-retomada-tentativa-2').atual(Date.now()).execucao!
    retomada.leituras = []
    retomada.trajeto = []
    let liberar: () => void = () => {}
    const espera = new Promise<void>((resolver) => { liberar = resolver })
    let consultasAtuais = 0
    let consultaIniciada = false
    servidor.use(
      http.post('*/api/execucoes/:id/retomar', () => HttpResponse.json({ execucao_id: dados.execucao_id,
        tentativa_id: retomada.tentativas[1].tentativa_id, attempt_index: 2 }, { status: 201 })),
      http.get('*/api/execucoes/em-andamento', () => { consultasAtuais += 1; return new HttpResponse(null, { status: 204 }) }),
      http.get('*/api/execucoes/:id', async () => {
        consultaIniciada = true
        await espera
        return HttpResponse.json(detalheAPI(retomada))
      }),
    )
    await retomarTentativa(dados.execucao_id)
    act(() => result.current.recarregar())
    await waitFor(() => expect(consultaIniciada).toBe(true))
    act(() => {
      fonte.emitir('status', { tipo: 'status', tentativa_id: retomada.tentativas[1].tentativa_id,
        attempt_index: 2, status_tentativa: 'running', status_execucao: 'em_andamento' })
      if (tipo === 'telemetria') fonte.emitir('telemetria', leitura())
      else fonte.emitir('passo', { tipo: 'passo', passo_id: 42, seq: 1, x: 0, y: 2,
        paredes_mask: 9, retomada: true, entrou_em: new Date().toISOString() })
      liberar()
    })
    await waitFor(() => expect(result.current.dados?.execucao?.tentativas.at(-1)?.attempt_index).toBe(2))
    await waitFor(() => expect(result.current.dados?.execucao?.celula_atual).toEqual({ x: 0, y: 2 }))
    if (tipo === 'telemetria') expect(result.current.dados?.execucao?.leituras.at(-1)).toMatchObject({ bateria: 7810, bateria_pct: 56, velocidade: 210 })
    else expect(result.current.dados?.execucao?.trajeto.filter((passo) => passo.passo_id === 42)).toMatchObject([{ passo_id: 42, paredes_mask: 9 }])
    expect(result.current.dados?.execucao?.execucao_id).toBe(dados.execucao_id)
    expect(consultasAtuais).toBe(0)
    expect(EventSourceControlado.instancias).toHaveLength(1)
  })
})
