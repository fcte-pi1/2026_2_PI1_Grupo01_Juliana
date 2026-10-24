import { act, screen, waitFor, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { describe, expect, it } from 'vitest'
import type { ComponenteHealthCheck, MotivoFalha } from '../api/tipos'
import { buscarCenario } from '../mocks/cenarios'
import { detalheAPI, HISTORICO, resumoAPI } from '../mocks/dados'
import { servidor } from '../mocks/servidor'
import { execucaoUs28 } from './execucaoUs28'
import { renderizarRota } from './renderizar'

// Respostas controladas: verificam o consumo pela interface, não a persistência real.
describe('FRONT-01/06 — US03, US26, US27: navegação e histórico completo', () => {
  it.each(['4x4', '8x4', '12x4'] as const)('aplica o filtro %s da URL em todas as páginas, inclusive a página final vazia', async (tipo) => {
    const base = resumoAPI(HISTORICO.find((e) => e.labirinto === tipo)!)
    const consultas: string[] = []
    // Reutiliza o exemplo existente; IDs distintos servem apenas à paginação do teste.
    const pagina = Array.from({ length: 100 }, (_, indice) => ({ ...base, execucao_id: `paginacao-${indice}`, numero: undefined }))
    servidor.use(http.get('*/api/execucoes', ({ request }) => {
      const busca = new URL(request.url).searchParams
      consultas.push(`${busca.get('tipo_labirinto')}/${busca.get('limite')}/${busca.get('offset')}`)
      return HttpResponse.json(busca.get('offset') === '0' ? pagina : [])
    }))
    renderizarRota(`/execucoes?labirinto=${tipo}`)
    const links = await screen.findAllByRole('link', { name: /^paginacao-/ })
    expect(links).toHaveLength(100)
    expect(consultas).toEqual([`${tipo}/100/0`, `${tipo}/100/100`])
    expect(screen.getByRole('button', { name: tipo })).toHaveAttribute('aria-pressed', 'true')
    for (const linha of within(screen.getByRole('table')).getAllByRole('row').slice(1)) {
      expect(within(linha).getByText(tipo)).toBeInTheDocument()
    }
    expect(links[0]).toHaveAttribute('href', '/execucoes/paginacao-0')
    expect(links.at(-1)).toHaveTextContent('paginacao-99')
  })

  it('informa falha na segunda página sem apresentar a primeira como histórico completo', async () => {
    const base = resumoAPI(HISTORICO[0])
    servidor.use(http.get('*/api/execucoes', ({ request }) => {
      const offset = new URL(request.url).searchParams.get('offset')
      return offset === '0'
        ? HttpResponse.json(Array.from({ length: 100 }, (_, i) => ({ ...base, execucao_id: `pagina-${i}` })))
        : new HttpResponse(null, { status: 503 })
    }))
    renderizarRota('/execucoes')
    expect(await screen.findByText(/Não foi possível carregar o histórico/)).toBeInTheDocument()
    expect(screen.queryByRole('table')).not.toBeInTheDocument()
    expect(screen.getByRole('navigation', { name: 'Principal' })).toBeInTheDocument()
  })

  it('mostra histórico vazio sem criar registros, métricas ou links de execução', async () => {
    servidor.use(http.get('*/api/execucoes', () => HttpResponse.json([])))
    renderizarRota('/execucoes')
    expect(await screen.findByText('Nenhuma execução registrada.')).toBeInTheDocument()
    expect(screen.queryByRole('table')).not.toBeInTheDocument()
    expect(screen.queryByRole('link', { name: /^#/ })).not.toBeInTheDocument()
  })

  it('não substitui o filtro atual quando a resposta do filtro anterior chega atrasada', async () => {
    let liberar!: () => void
    const espera = new Promise<void>((resolver) => { liberar = resolver })
    let iniciou = false
    let sinal: AbortSignal | undefined
    servidor.use(http.get('*/api/execucoes', async ({ request }) => {
      const tipo = new URL(request.url).searchParams.get('tipo_labirinto')
      if (tipo === '4x4') {
        iniciou = true
        sinal = request.signal
        await espera
      }
      return HttpResponse.json(HISTORICO.filter((e) => e.labirinto === tipo).map(resumoAPI))
    }))
    renderizarRota('/execucoes?labirinto=4x4')
    try {
      await waitFor(() => expect(iniciou).toBe(true))
      await userEvent.click(screen.getByRole('button', { name: '8x4' }))
      expect(await screen.findByRole('link', { name: '#0040' })).toBeInTheDocument()
      await waitFor(() => expect(sinal?.aborted).toBe(true))
      await act(async () => { liberar(); await espera })
      expect(within(screen.getByRole('table')).getAllByRole('link').map((link) => link.textContent)).toEqual(['#0040', '#0032'])
      expect(screen.getByRole('button', { name: '8x4' })).toHaveAttribute('aria-pressed', 'true')
    } finally {
      liberar()
    }
  })
})

const falhas: Array<[MotivoFalha, string]> = [
  ['collision', 'Colisão com parede'],
  ['stuck', 'Robô travado ou parado'],
  ['out_of_track', 'Saiu do labirinto'],
  ['time_exceeded', 'Tempo esgotado'],
  ['falha_componente', 'Falha de componente'],
  ['low_battery', 'Bateria baixa'],
  ['health_check_failed', 'Health-check reprovado'],
  ['link_lost', 'Perda de comunicação'],
  ['encerrado_operador', 'Encerrado pelo operador'],
]

describe('FRONT-04/06 — US06, US17, US23, US24, US28: falha antes do primeiro passo', () => {
  it.each(falhas)('reexibe %s com célula e componente correspondentes à resposta', async (motivo, rotulo) => {
    const dados = buscarCenario('08-encerrada-como-falha').atual(Date.now()).execucao!
    dados.trajeto = []
    dados.leituras = []
    Object.assign(dados.tentativas[0].falha!, { motivo, celula_x: 0, celula_y: 0,
      componente: motivo === 'falha_componente' ? 'tof_frontal' : null, observacao: null,
      origem: motivo === 'encerrado_operador' ? 'encerrado_operador' : 'automatica' })
    servidor.use(http.get('*/api/execucoes/e-0042', () => HttpResponse.json(detalheAPI(dados))))
    renderizarRota('/execucoes/e-0042')
    const titulo = await screen.findByRole('heading', { name: 'Registro da falha · tentativa 1' })
    const registro = within(titulo.closest('section')!)
    expect(registro.getByText(rotulo)).toBeInTheDocument()
    expect(registro.getByText('A1')).toBeInTheDocument()
    expect(registro.getByText(motivo === 'encerrado_operador' ? /Encerrada pelo operador\./ : /Detectada automaticamente\./)).toBeInTheDocument()
    if (motivo === 'falha_componente') {
      expect(registro.getByText(/Componente: ToF frontal\./)).toBeInTheDocument()
    } else {
      expect(registro.queryByText(/Componente:/)).not.toBeInTheDocument()
    }
    expect(screen.getByText('Robô em A1 · falhou')).toBeInTheDocument()
    expect(screen.getByText('FALHA T1')).toBeInTheDocument()
    expect(document.querySelector('.labirinto__robo--falha')).not.toBeNull()
    expect(document.querySelector('polyline.labirinto__trajeto')).toBeNull()
  })
})

const componentes: Array<[ComponenteHealthCheck, string]> = [
  ['bateria', 'Bateria'], ['tof_frontal', 'ToF frontal'],
  ['tof_esquerdo', 'ToF esquerdo'], ['tof_direito', 'ToF direito'],
  ['motor_esquerdo', 'Motor esquerdo'], ['motor_direito', 'Motor direito'],
  ['encoder_esquerdo', 'Encoder esquerdo'], ['encoder_direito', 'Encoder direito'],
]

describe('FRONT-04/06 — US17, US30: componente da falha persistida', () => {
  it.each(componentes)('preserva %s no detalhe sem confundir com os resultados do health-check', async (componente, rotulo) => {
    const dados = buscarCenario('08-encerrada-como-falha').atual(Date.now()).execucao!
    Object.assign(dados.tentativas[0].falha!, { motivo: 'falha_componente', componente, origem: 'automatica' })
    servidor.use(http.get('*/api/execucoes/e-0042', () => HttpResponse.json(detalheAPI(dados))))
    renderizarRota('/execucoes/e-0042')
    const titulo = await screen.findByRole('heading', { name: 'Registro da falha · tentativa 1' })
    const registro = within(titulo.closest('section')!)
    expect(registro.getByText('Falha de componente')).toBeInTheDocument()
    expect(registro.getByText(new RegExp(`Componente: ${rotulo}\\.`))).toBeInTheDocument()
    expect(registro.getByText('B2')).toBeInTheDocument()
    expect(screen.getByRole('heading', { name: 'Health-check' }).closest('section')).not.toBe(titulo.closest('section'))
  })
})

describe('FRONT-03/05 — US07, US18, US23, US24: estado recebido do backend', () => {
  it.each([
    ['link_lost', 1, 'em_andamento'],
    ['link_lost', 2, 'em_andamento'],
    ['link_lost', 3, 'cancelada'],
    ['time_exceeded', 1, 'cancelada'],
    ['time_exceeded', 2, 'cancelada'],
    ['time_exceeded', 3, 'cancelada'],
  ] as const)('%s na tentativa %s respeita o estado %s e a possibilidade de retomada', async (motivo, indice, status) => {
    const dados = execucaoUs28()
    dados.status = status
    dados.tentativas = dados.tentativas.slice(0, indice)
    dados.tentativas_usadas = indice
    const ultima = dados.tentativas.at(-1)!
    dados.trajeto = dados.trajeto.filter((passo) => Date.parse(passo.entrou_em!) <= Date.parse(ultima.encerrada_em!))
    const posicao = dados.trajeto.at(-1)!
    if (motivo === 'time_exceeded') {
      // US24: o encerramento ocorre aos dez minutos da execução, incluindo pausas.
      ultima.encerrada_em = new Date(Date.parse(dados.iniciada_em) + 600_000).toISOString()
      ultima.tempo_s = (Date.parse(ultima.encerrada_em) - Date.parse(ultima.iniciada_em)) / 1000
    }
    dados.encerrada_em = status === 'em_andamento' ? null : ultima.encerrada_em
    dados.tempo_total_s = status === 'em_andamento' ? null : (Date.parse(ultima.encerrada_em!) - Date.parse(dados.iniciada_em)) / 1000
    // US23: perda de link é registrada após dez segundos sem mensagem do robô.
    dados.ultima_mensagem_em = motivo === 'link_lost'
      ? new Date(Date.parse(ultima.encerrada_em!) - 10_000).toISOString()
      : ultima.encerrada_em
    ultima.status = 'failed'
    ultima.falha = { ...dados.tentativas[0].falha!, motivo, origem: 'automatica',
      celula_x: posicao.x, celula_y: posicao.y, momento_falha: ultima.encerrada_em! }
    let consultasAtuais = 0
    servidor.use(
      http.get('*/api/execucoes/em-andamento', () => {
        consultasAtuais += 1
        return status === 'em_andamento' ? HttpResponse.json(detalheAPI(dados)) : new HttpResponse(null, { status: 204 })
      }),
      http.get('*/api/execucoes/e-0043', () => HttpResponse.json(detalheAPI(dados))),
    )
    // O backend não retorna execuções canceladas na consulta de execução ativa.
    renderizarRota(status === 'em_andamento' ? '/' : '/?execucao=e-0043')
    await screen.findByRole('heading', { name: `Registro da falha · tentativa ${indice}` })
    if (status === 'em_andamento') {
      expect(screen.getByRole('button', { name: new RegExp(`Retomar tentativa \\(${indice + 1}/3\\)`) })).toBeInTheDocument()
      expect(consultasAtuais).toBeGreaterThan(0)
    } else {
      expect(screen.queryByRole('button', { name: /Retomar tentativa/ })).not.toBeInTheDocument()
      expect(screen.getByRole('heading', { name: 'Execução cancelada #0043' })).toBeInTheDocument()
      expect(consultasAtuais).toBe(0)
    }
    expect(screen.queryByRole('button', { name: 'Encerrar tentativa' })).not.toBeInTheDocument()
    expect(screen.getByLabelText(`Tentativa ${indice} de 3`)).toBeInTheDocument()
  })
})

describe('FRONT-06 — US02, US05, US16, US28, US31: filtro recorta somente o percurso', () => {
  it('preserva tabela, métricas, health-check e falhas ao alternar Todas/T1/T2/T3/Todas', async () => {
    const dados = execucaoUs28()
    servidor.use(http.get('*/api/execucoes/e-0043', () => HttpResponse.json(detalheAPI(dados))))
    renderizarRota('/execucoes/e-0043')
    await screen.findByRole('heading', { name: 'Execução #0043' })
    const tabela = screen.getByRole('table').textContent
    const health = () => screen.getByRole('heading', { name: 'Health-check' }).closest('section')!
    const verificacoes = health().textContent
    const trajeto = () => Array.from(document.querySelectorAll('polyline.labirinto__trajeto'), (linha) => linha.getAttribute('points'))
    const completo = trajeto()
    const grupo = within(screen.getByRole('group', { name: 'Trajeto por tentativa' }))
    for (const nome of ['T1', 'T2', 'T3', 'Todas']) {
      await userEvent.click(grupo.getByRole('button', { name: nome }))
      expect(grupo.getByRole('button', { name: nome })).toHaveAttribute('aria-pressed', 'true')
      expect(screen.getByRole('table')).toHaveTextContent(tabela!)
      expect(health()).toBeInTheDocument()
      expect(health()).toHaveTextContent(verificacoes!)
      expect(screen.getByText('12,5')).toBeInTheDocument()
      expect(screen.getByRole('heading', { name: 'Registro da falha · tentativa 1' })).toBeInTheDocument()
      expect(screen.getByRole('heading', { name: 'Registro da falha · tentativa 2' })).toBeInTheDocument()
      expect(trajeto()).toHaveLength(nome === 'Todas' ? 3 : 1)
    }
    expect(trajeto()).toEqual(completo)
    expect(screen.getByText('Robô em D4')).toBeInTheDocument()
  })
})
