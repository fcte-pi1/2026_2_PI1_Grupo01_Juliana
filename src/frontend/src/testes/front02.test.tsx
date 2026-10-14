import { screen, waitFor, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { delay, http, HttpResponse } from 'msw'
import { afterEach, beforeEach, describe, expect, it } from 'vitest'
import { buscarExecucao, buscarExecucaoAtual } from '../api/endpoints'
import type { components } from '../api/schema'
import type { MotivoRecusa, TipoLabirinto } from '../api/tipos'
import { buscarCenario } from '../mocks/cenarios'
import { detalheAPI } from '../mocks/dados'
import { servidor } from '../mocks/servidor'
import { ROTULO_RECUSA } from '../utils/formatacao'
import { renderizarRota } from './renderizar'

let requisicoes: Request[] = []
const observar = ({ request }: { request: Request }) => requisicoes.push(request.clone())
const criacoes = () => requisicoes.filter((r) => r.method === 'POST' && new URL(r.url).pathname === '/api/execucoes')
const consultasAtuais = () => requisicoes.filter((r) => new URL(r.url).pathname === '/api/execucoes/em-andamento').length

function detalheDoContrato(): components['schemas']['ExecucaoDetalhe'] {
  const iniciada_em = new Date().toISOString()
  return {
    execucao_id: '123e4567-e89b-12d3-a456-426614174000',
    tipo_labirinto: '8x4',
    status: 'em_andamento',
    tentativas_usadas: 1,
    iniciada_em,
    tentativas: [{
      tentativa_id: '7c9e6679-7425-40de-944b-e07fc1f90ae7',
      attempt_index: 1,
      status: 'health-check',
      tipo_inicio: 'nova',
      iniciada_em,
      health_check: [],
    }],
    trajeto: [],
    leituras: [],
  }
}

beforeEach(() => {
  requisicoes = []
  servidor.events.on('request:start', observar)
})
afterEach(() => servidor.events.removeListener('request:start', observar))

describe('FRONT-02: nova execução', () => {
  it.each<TipoLabirinto>(['4x4', '8x4', '12x4'])('inicia %s na tela ao vivo com Tentativa 1/3', async (labirinto) => {
    renderizarRota('/')
    await userEvent.click(await screen.findByRole('radio', { name: new RegExp(`^${labirinto}`) }))
    expect(screen.getAllByRole('radio')).toHaveLength(3)
    await userEvent.click(screen.getByRole('button', { name: /Nova execução/ }))

    expect(await screen.findByRole('heading', { name: 'Execução ativa #0042' })).toBeInTheDocument()
    expect(screen.getByLabelText('Tentativa 1 de 3')).toHaveTextContent('Tentativa 1/3')
    expect(screen.getByText(`Labirinto ${labirinto}`)).toBeInTheDocument()
    expect(screen.getByText('Aguardando os dados do health-check enviados pelo robô.')).toBeInTheDocument()
    expect(criacoes()).toHaveLength(1)
    expect(await criacoes()[0].json()).toEqual({ tipo_labirinto: labirinto })
    const atual = (await buscarExecucaoAtual()).execucao!
    expect(atual.labirinto).toBe(labirinto)
    expect(atual.tentativas[0].attempt_index).toBe(1)
    expect(atual.tentativas[0].health_check).toEqual([])
    expect(atual.tentativas[0].bateria_inicial).toBeNull()
    expect(atual.trajeto).toEqual([])
    expect(atual.leituras).toEqual([])
  })

  it('consome o 201 com apenas os IDs e busca o detalhe da execução criada', async () => {
    const nova = buscarCenario('02-health-check').atual(Date.now()).execucao!
    servidor.use(
      http.post('*/api/execucoes', () => HttpResponse.json({
        execucao_id: nova.execucao_id,
        tentativa_id: nova.tentativas[0].tentativa_id,
        attempt_index: 1,
      }, { status: 201 })),
      http.get(`*/api/execucoes/${nova.execucao_id}`, () => HttpResponse.json(detalheAPI(nova))),
    )
    renderizarRota('/')
    await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
    expect(await screen.findByRole('heading', { name: 'Execução ativa #0042' })).toBeInTheDocument()
    expect(screen.getByLabelText('Tentativa 1 de 3')).toBeInTheDocument()
    expect(requisicoes.some((r) => r.method === 'GET' && new URL(r.url).pathname === `/api/execucoes/${nova.execucao_id}`)).toBe(true)
    expect(consultasAtuais()).toBe(1)
  })

  it('renderiza o formato do OpenAPI sem número sequencial nem métricas opcionais', async () => {
    const nova = detalheDoContrato()
    servidor.use(
      http.post('*/api/execucoes', () => HttpResponse.json({ execucao_id: nova.execucao_id, tentativa_id: nova.tentativas[0].tentativa_id, attempt_index: 1 }, { status: 201 })),
      http.get(`*/api/execucoes/${nova.execucao_id}`, () => HttpResponse.json(nova)),
    )
    renderizarRota('/')
    await userEvent.click(await screen.findByRole('radio', { name: /^8x4/ }))
    expect(screen.queryByText(/próximo ID/)).not.toBeInTheDocument()
    await userEvent.click(screen.getByRole('button', { name: /Nova execução/ }))
    expect(await screen.findByRole('heading', { name: `Execução ativa ${nova.execucao_id}` })).toBeInTheDocument()
    expect(screen.getByLabelText('Tentativa 1 de 3')).toBeInTheDocument()
    expect(screen.getByText('Labirinto 8x4')).toBeInTheDocument()
    expect(screen.queryByText(/#undefined|#0000/)).not.toBeInTheDocument()
    const detalhe = await buscarExecucao(nova.execucao_id)
    expect(detalhe.tempo_total_s).toBeNull()
    expect(detalhe.tentativas[0].falha).toBeNull()
    expect(detalhe.tentativas[0].velocidade_media).toBeNull()
  })

  it('aceita o detalhe mínimo do backend em dev sem inventar dados de telemetria', async () => {
    const contrato = detalheDoContrato()
    const nova = {
      execucao_id: contrato.execucao_id,
      tipo_labirinto: contrato.tipo_labirinto,
      status: contrato.status,
      tentativas_usadas: contrato.tentativas_usadas,
      iniciada_em: contrato.iniciada_em,
      tentativas: contrato.tentativas.map((tentativa) => ({
        tentativa_id: tentativa.tentativa_id,
        attempt_index: tentativa.attempt_index,
        status: tentativa.status,
        tipo_inicio: tentativa.tipo_inicio,
        iniciada_em: tentativa.iniciada_em,
      })),
    }
    servidor.use(
      http.post('*/api/execucoes', () => HttpResponse.json({ execucao_id: nova.execucao_id, tentativa_id: nova.tentativas[0].tentativa_id, attempt_index: 1 }, { status: 201 })),
      http.get(`*/api/execucoes/${nova.execucao_id}`, () => HttpResponse.json(nova)),
    )
    renderizarRota('/')
    await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
    expect(await screen.findByRole('heading', { name: `Execução ativa ${nova.execucao_id}` })).toBeInTheDocument()
    expect(screen.getByLabelText('Tentativa 1 de 3')).toBeInTheDocument()
    expect(screen.getByText('Aguardando os dados do health-check enviados pelo robô.')).toBeInTheDocument()
    const detalhe = await buscarExecucao(nova.execucao_id)
    expect(detalhe.trajeto).toEqual([])
    expect(detalhe.leituras).toEqual([])
    expect(detalhe.tentativas[0].health_check).toEqual([])
    expect(detalhe.tentativas[0].bateria_inicial).toBeNull()
    expect(screen.queryByRole('alert')).not.toBeInTheDocument()
  })

  it('mostra a última execução após 204 sem exigir campos ausentes do contrato', async () => {
    const ultima = detalheDoContrato()
    servidor.use(http.get('*/api/execucoes', () => HttpResponse.json([{
      execucao_id: ultima.execucao_id,
      tipo_labirinto: ultima.tipo_labirinto,
      status: 'cancelada',
      tentativas_usadas: 1,
      iniciada_em: ultima.iniciada_em,
    }])))
    renderizarRota('/')
    expect(await screen.findByRole('heading', { name: 'Nenhuma execução ativa' })).toBeInTheDocument()
    const cartao = (await screen.findByText('Última execução · 8x4')).closest('section')!
    expect(within(cartao).getByText('Cancelada')).toBeInTheDocument()
    expect(within(cartao).getByText(ultima.execucao_id)).toBeInTheDocument()
    expect(within(cartao).getAllByText('—')).toHaveLength(3)
    expect(requisicoes.some((r) => /\/labirintos|\/execucoes\/atual/.test(new URL(r.url).pathname))).toBe(false)
  })

  it.each(['success', 'failed'] as const)('mantém o estado da execução quando a tentativa está %s', async (status) => {
    const atual = detalheDoContrato()
    servidor.use(http.get('*/api/execucoes/em-andamento', () => HttpResponse.json({
      ...atual,
      tentativas: [{ ...atual.tentativas[0], status }],
    })))
    renderizarRota('/')
    expect(await screen.findByRole('heading', { name: `Execução ativa ${atual.execucao_id}` })).toBeInTheDocument()
    expect(screen.queryByRole('heading', { name: /Execução concluída|Execução cancelada|Execução encerrada/ })).not.toBeInTheDocument()
    expect(screen.queryByText(/concluída na tentativa/)).not.toBeInTheDocument()
  })

  it('não repete o POST se a execução foi criada mas a busca do detalhe falhou', async () => {
    const nova = detalheDoContrato()
    let consultas = 0
    servidor.use(
      http.post('*/api/execucoes', () => HttpResponse.json({ execucao_id: nova.execucao_id, tentativa_id: nova.tentativas[0].tentativa_id, attempt_index: 1 }, { status: 201 })),
      http.get(`*/api/execucoes/${nova.execucao_id}`, () => ++consultas === 1 ? new HttpResponse(null, { status: 503 }) : HttpResponse.json(nova)),
    )
    renderizarRota('/')
    await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
    expect(await screen.findByRole('alert')).toHaveTextContent('A execução foi criada, mas não foi possível carregar seus dados.')
    expect(screen.getByRole('heading', { name: 'Nova execução registrada' })).toBeInTheDocument()
    expect(screen.getByLabelText('Tentativa 1 de 3')).toBeInTheDocument()
    await userEvent.click(screen.getByRole('button', { name: 'Carregar dados da execução' }))
    expect(await screen.findByRole('heading', { name: `Execução ativa ${nova.execucao_id}` })).toBeInTheDocument()
    expect(criacoes()).toHaveLength(1)
    expect(consultas).toBe(2)
  })

  it('mantém Tentativa 1/3 sem quebrar se o detalhe ainda não contiver a tentativa', async () => {
    const nova = detalheDoContrato()
    servidor.use(
      http.post('*/api/execucoes', () => HttpResponse.json({ execucao_id: nova.execucao_id, tentativa_id: nova.tentativas[0].tentativa_id, attempt_index: 1 }, { status: 201 })),
      http.get(`*/api/execucoes/${nova.execucao_id}`, () => HttpResponse.json({ ...nova, tentativas: [] })),
    )
    renderizarRota('/')
    await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
    expect(await screen.findByRole('heading', { name: `Execução ativa ${nova.execucao_id}` })).toBeInTheDocument()
    expect(screen.getByLabelText('Tentativa 1 de 3')).toBeInTheDocument()
    expect(screen.getByText('Aguardando os dados da tentativa enviados pelo backend.')).toBeInTheDocument()
    expect(screen.queryByRole('button', { name: 'Encerrar tentativa' })).not.toBeInTheDocument()
  })

  it('aguarda confirmação e permite desistir sem cancelar a execução em andamento', async () => {
    renderizarRota('/', '03-em-execucao')
    await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
    const modal = await screen.findByRole('dialog', { name: 'Nova execução' })
    expect(within(modal).getByText(/Ao confirmar, ela será/)).toHaveTextContent('cancelada')
    expect(criacoes()).toHaveLength(0)
    await userEvent.click(within(modal).getByRole('button', { name: 'Cancelar' }))
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument()
    expect(screen.getByRole('heading', { name: 'Execução ativa #0042' })).toBeInTheDocument()
    expect((await buscarExecucao('e-0042')).status).toBe('em_andamento')
    expect(criacoes()).toHaveLength(0)
  })

  it('confirma uma única criação, cancelando a anterior e respeitando o labirinto selecionado', async () => {
    renderizarRota('/', '03-em-execucao')
    await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
    const modal = await screen.findByRole('dialog')
    await userEvent.click(within(modal).getByRole('radio', { name: /^12x4/ }))
    await userEvent.click(within(modal).getByRole('button', { name: 'Confirmar nova execução' }))
    expect(await screen.findByRole('heading', { name: 'Execução ativa #0043' })).toBeInTheDocument()
    expect(screen.getByText('Labirinto 12x4')).toBeInTheDocument()
    expect(screen.getByLabelText('Tentativa 1 de 3')).toBeInTheDocument()
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument()
    expect((await buscarExecucao('e-0042')).status).toBe('cancelada')
    const atual = (await buscarExecucaoAtual()).execucao!
    expect(atual.execucao_id).not.toBe('e-0042')
    expect(atual.status).toBe('em_andamento')
    expect(criacoes()).toHaveLength(1)
    expect(await criacoes()[0].json()).toEqual({ tipo_labirinto: '12x4' })
    expect(requisicoes.filter((r) => r.method === 'POST')).toHaveLength(1)
  })

  it('mantém o ID e o labirinto criados ao usar os comandos existentes de encerrar e retomar', async () => {
    renderizarRota('/')
    await userEvent.click(await screen.findByRole('radio', { name: /^8x4/ }))
    await userEvent.click(screen.getByRole('button', { name: /Nova execução/ }))
    await screen.findByRole('heading', { name: 'Execução ativa #0042' })
    const criada = (await buscarExecucaoAtual()).execucao!
    await userEvent.click(screen.getByRole('button', { name: 'Encerrar tentativa' }))
    const encerrar = await screen.findByRole('dialog')
    await userEvent.click(within(encerrar).getByLabelText(/Colisão com parede/))
    await userEvent.click(within(encerrar).getByRole('button', { name: 'Encerrar como falha' }))
    await userEvent.click(await screen.findByRole('button', { name: /Retomar tentativa \(2\/3\)/ }))
    const retomar = await screen.findByRole('dialog')
    for (const caixa of within(retomar).getAllByRole('checkbox')) await userEvent.click(caixa)
    await userEvent.click(within(retomar).getByRole('button', { name: 'Retomar da célula B2' }))
    expect(await screen.findByLabelText('Tentativa 2 de 3')).toBeInTheDocument()
    const retomada = (await buscarExecucaoAtual()).execucao!
    expect(retomada.execucao_id).toBe(criada.execucao_id)
    expect(retomada.labirinto).toBe('8x4')
    expect(retomada.numero).toBe(criada.numero)
    expect(retomada.tentativas[0].tentativa_id).toBe(criada.tentativas[0].tentativa_id)
    expect(retomada.tentativas[1].tentativa_id).not.toBe(criada.tentativas[0].tentativa_id)
    expect((await buscarExecucao(criada.execucao_id)).labirinto).toBe('8x4')
  })

  const motivos: MotivoRecusa[] = ['tentativa_aberta', 'limite_3', 'execucao_cancelada']
  it.each(motivos)('mostra o motivo %s do 409 sem sair da tela inicial', async (motivo) => {
    servidor.use(http.post('*/api/execucoes', () => HttpResponse.json({ motivo, ocorrida_em: new Date().toISOString() }, { status: 409 })))
    const { router } = renderizarRota('/')
    await userEvent.click(await screen.findByRole('radio', { name: /^8x4/ }))
    await userEvent.click(screen.getByRole('button', { name: /Nova execução/ }))
    expect(await screen.findByText('Nova execução recusada')).toBeInTheDocument()
    expect(screen.getByRole('alert')).toHaveTextContent(ROTULO_RECUSA[motivo])
    expect(screen.getByRole('heading', { name: 'Nenhuma execução ativa' })).toBeInTheDocument()
    expect(screen.getByRole('radio', { name: /^8x4/ })).toHaveAttribute('aria-checked', 'true')
    expect(screen.queryByLabelText('Tentativa 1 de 3')).not.toBeInTheDocument()
    expect(consultasAtuais()).toBe(1)
    expect(router.state.location.pathname).toBe('/')
    expect(router.state.location.search).toBe('')
  })

  it.each(motivos)('mantém a execução atual e a confirmação abertas quando recebe 409 por %s', async (motivo) => {
    servidor.use(http.post('*/api/execucoes', () => HttpResponse.json({ motivo, ocorrida_em: new Date().toISOString() }, { status: 409 })))
    const { router } = renderizarRota('/', '03-em-execucao')
    await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
    const modal = await screen.findByRole('dialog')
    await userEvent.click(within(modal).getByRole('radio', { name: /^8x4/ }))
    await userEvent.click(within(modal).getByRole('button', { name: 'Confirmar nova execução' }))
    expect(await within(modal).findByText('Nova execução recusada')).toBeInTheDocument()
    expect(within(modal).getByRole('alert')).toHaveTextContent(ROTULO_RECUSA[motivo])
    expect(screen.getByRole('heading', { name: 'Execução ativa #0042' })).toBeInTheDocument()
    expect(screen.getByText('Labirinto 4x4')).toBeInTheDocument()
    expect(within(modal).getByRole('radio', { name: /^8x4/ })).toHaveAttribute('aria-checked', 'true')
    expect(consultasAtuais()).toBe(1)
    expect(router.state.location.search).toBe('?modal=nova')
    expect((await buscarExecucao('e-0042')).status).toBe('em_andamento')
  })

  it('não inventa um motivo quando o 409 retorna um corpo inválido', async () => {
    servidor.use(http.post('*/api/execucoes', () => new HttpResponse(null, { status: 409 })))
    renderizarRota('/')
    await userEvent.click(await screen.findByRole('button', { name: /Nova execução/ }))
    expect(await screen.findByRole('alert')).toHaveTextContent('O servidor recusou o pedido, mas não informou um motivo válido.')
    expect(screen.getByRole('heading', { name: 'Nenhuma execução ativa' })).toBeInTheDocument()
    expect(consultasAtuais()).toBe(1)
  })

  it('bloqueia cliques repetidos enquanto o pedido está pendente', async () => {
    let liberar!: () => void
    const pendente = new Promise<void>((resolve) => { liberar = resolve })
    servidor.use(http.post('*/api/execucoes', async () => {
      await pendente
      return HttpResponse.json({ motivo: 'tentativa_aberta', ocorrida_em: new Date().toISOString() }, { status: 409 })
    }))
    renderizarRota('/')
    const botao = await screen.findByRole('button', { name: /Nova execução/ })
    try {
      await userEvent.dblClick(botao)
      await waitFor(() => expect(criacoes()).toHaveLength(1))
      expect(botao).toBeDisabled()
      expect(screen.getAllByRole('radio').every((radio) => (radio as HTMLButtonElement).disabled)).toBe(true)
    } finally {
      liberar()
    }
    expect(await screen.findByText('Nova execução recusada')).toBeInTheDocument()
    expect(botao).toBeEnabled()
  })

  it('preserva a tela e permite tentar novamente se o servidor ficar indisponível', async () => {
    servidor.use(http.post('*/api/execucoes', async () => {
      await delay(10)
      return new HttpResponse(null, { status: 503 })
    }))
    renderizarRota('/')
    const botao = await screen.findByRole('button', { name: /Nova execução/ })
    await userEvent.click(botao)
    expect(await screen.findByRole('alert')).toHaveTextContent('Sem comunicação com o servidor.')
    expect(screen.getByRole('heading', { name: 'Nenhuma execução ativa' })).toBeInTheDocument()
    expect(botao).toBeEnabled()
    expect(consultasAtuais()).toBe(1)
    servidor.resetHandlers()
    await userEvent.click(botao)
    expect(await screen.findByRole('heading', { name: 'Execução ativa #0042' })).toBeInTheDocument()
    expect(screen.queryByRole('alert')).not.toBeInTheDocument()
  })
})
