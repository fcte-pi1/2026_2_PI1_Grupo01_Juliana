import { render, screen, waitFor, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse } from 'msw'
import { describe, expect, it, vi } from 'vitest'
import type { EncerrarPedido, StatusExecucao, StatusTentativa } from '../api/tipos'
import { combinarRetrato } from '../api/eventosExecucao'
import { ControleTentativas } from '../components/tentativas/ControleTentativas'
import { ModalEncerrar } from '../components/tentativas/ModalEncerrar'
import { ModalRetomar } from '../components/tentativas/ModalRetomar'
import { buscarExecucaoRegistrada, encerrarTentativaRegistrada, retomarTentativaRegistrada } from '../mocks/cenarioAtual'
import { detalheAPI, execucao, falha, tentativa } from '../mocks/dados'
import { servidor } from '../mocks/servidor'
import { renderizarRota } from './renderizar'

function dadosTentativas(status: StatusTentativa, usadas = 1, statusExecucao: StatusExecucao = 'em_andamento') {
  const agora = Date.now()
  return execucao(agora, { status: statusExecucao, tentativas_usadas: usadas,
    tentativas: Array.from({ length: usadas }, (_, i) => tentativa(agora, {
      attempt_index: i + 1, status: i + 1 === usadas ? status : 'failed', falha: falha(agora),
    })) })
}

describe('ControleTentativas — US07', () => {
  it.each([
    ['running', 1], ['running', 3],
  ] as const)('oferece somente Encerrar com tentativa %s aberta (%s/3)', async (status, usadas) => {
    const aoEncerrar = vi.fn()
    const aoRetomar = vi.fn()
    render(<ControleTentativas execucao={dadosTentativas(status, usadas)} aoEncerrar={aoEncerrar} aoRetomar={aoRetomar} />)
    await userEvent.click(screen.getByRole('button', { name: 'Encerrar tentativa' }))
    expect(aoEncerrar).toHaveBeenCalledOnce()
    expect(screen.queryByRole('button', { name: /Retomar tentativa/ })).not.toBeInTheDocument()
    expect(aoRetomar).not.toHaveBeenCalled()
  })

  it.each([1, 2])('oferece a próxima tentativa após %s usada(s) com falha', async (usadas) => {
    const aoRetomar = vi.fn()
    render(<ControleTentativas execucao={dadosTentativas('failed', usadas)} aoEncerrar={vi.fn()} aoRetomar={aoRetomar} />)
    await userEvent.click(screen.getByRole('button', { name: `↻ Retomar tentativa (${usadas + 1}/3)` }))
    expect(aoRetomar).toHaveBeenCalledOnce()
    expect(screen.queryByRole('button', { name: 'Encerrar tentativa' })).not.toBeInTheDocument()
  })

  it.each([
    ['failed', 3, 'em_andamento'], ['failed', 3, 'cancelada'],
    ['failed', 1, 'cancelada'], ['running', 1, 'cancelada'], ['success', 1, 'concluida'],
  ] as const)('oculta os controles com %s, %s usada(s), execução %s', (status, usadas, estado) => {
    render(<ControleTentativas execucao={dadosTentativas(status, usadas, estado)} aoEncerrar={vi.fn()} aoRetomar={vi.fn()} />)
    expect(screen.queryByRole('button')).not.toBeInTheDocument()
  })

  it('não oferece ações sem uma tentativa recebida', () => {
    render(<ControleTentativas execucao={execucao(Date.now())} aoEncerrar={vi.fn()} aoRetomar={vi.fn()} />)
    expect(screen.queryByRole('button')).not.toBeInTheDocument()
  })

  it('não oferece retomada enquanto qualquer tentativa estiver aberta', () => {
    const dados = dadosTentativas('failed', 2)
    dados.tentativas[0].status = 'running'
    render(<ControleTentativas execucao={dados} aoEncerrar={vi.fn()} aoRetomar={vi.fn()} />)
    expect(screen.getByRole('button', { name: 'Encerrar tentativa' })).toBeInTheDocument()
    expect(screen.queryByRole('button', { name: /Retomar tentativa/ })).not.toBeInTheDocument()
  })

  it('usa o attempt_index mais alto mesmo quando o GET vem fora de ordem', () => {
    const dados = dadosTentativas('failed', 2)
    dados.tentativas.reverse()
    render(<ControleTentativas execucao={dados} aoEncerrar={vi.fn()} aoRetomar={vi.fn()} />)
    expect(screen.getByRole('button', { name: '↻ Retomar tentativa (3/3)' })).toBeInTheDocument()
  })

  it.each(['running', 'failed'] as const)('desabilita a ação durante o pedido com tentativa %s', async (status) => {
    const acao = vi.fn()
    render(<ControleTentativas execucao={dadosTentativas(status)} enviando aoEncerrar={acao} aoRetomar={acao} />)
    expect(screen.getByRole('button')).toBeDisabled()
    await userEvent.dblClick(screen.getByRole('button'))
    expect(acao).not.toHaveBeenCalled()
  })
})

const propsEncerrar = { numeroExecucao: '#0042', attemptIndex: 1, ultimaCelula: 'B2', tempo: '00:20,4' }
const propsRetomar = { numeroExecucao: '#0042', proxima: 2, celulaFalha: 'B2', liberado: true }

describe('ModalEncerrar — US22', () => {
  it('inicia sem motivo selecionado e não permite confirmar', async () => {
    const aoConfirmar = vi.fn()
    const usuario = userEvent.setup()
    render(<ModalEncerrar {...propsEncerrar} aoCancelar={vi.fn()} aoConfirmar={aoConfirmar} />)
    for (const radio of screen.getAllByRole('radio')) expect(radio).not.toBeChecked()
    const confirmar = screen.getByRole('button', { name: 'Encerrar como falha' })
    expect(confirmar).toBeDisabled()
    await usuario.click(confirmar)
    await usuario.click(screen.getByRole('textbox'))
    await usuario.keyboard('{Enter}')
    expect(aoConfirmar).not.toHaveBeenCalled()
  })

  it.each(['collision', 'stuck', 'out_of_track'] as const)('envia somente o motivo %s escolhido pelo operador', async (motivo) => {
    const aoConfirmar = vi.fn()
    render(<ModalEncerrar {...propsEncerrar} aoCancelar={vi.fn()} aoConfirmar={aoConfirmar} />)
    await userEvent.click(screen.getByRole('radio', { name: new RegExp(motivo) }))
    await userEvent.click(screen.getByRole('button', { name: 'Encerrar como falha' }))
    expect(aoConfirmar).toHaveBeenCalledExactlyOnceWith({ motivo, observacao: null })
  })

  it('bloqueia o 101º caractere digitado e envia a observação completa até 100', async () => {
    const aoConfirmar = vi.fn()
    const usuario = userEvent.setup()
    render(<ModalEncerrar {...propsEncerrar} aoCancelar={vi.fn()} aoConfirmar={aoConfirmar} />)
    await usuario.click(screen.getByRole('radio', { name: /collision/ }))
    await usuario.type(screen.getByRole('textbox'), 'a'.repeat(100) + 'b')
    expect(screen.getByRole('textbox')).toHaveValue('a'.repeat(100))
    await usuario.click(screen.getByRole('button', { name: 'Encerrar como falha' }))
    expect(aoConfirmar).toHaveBeenCalledExactlyOnceWith({ motivo: 'collision', observacao: 'a'.repeat(100) })
  })

  it('limita também uma observação colada a 100 caracteres', async () => {
    const usuario = userEvent.setup()
    render(<ModalEncerrar {...propsEncerrar} aoCancelar={vi.fn()} aoConfirmar={vi.fn()} />)
    await usuario.click(screen.getByRole('textbox'))
    await usuario.paste('a'.repeat(100) + 'b')
    expect(screen.getByRole('textbox')).toHaveValue('a'.repeat(100))
  })

  it('bloqueia edição, confirmação, cancelar, Esc e fundo durante o envio', async () => {
    const aoCancelar = vi.fn()
    const aoConfirmar = vi.fn()
    const props = { ...propsEncerrar, aoCancelar, aoConfirmar }
    const { rerender } = render(<ModalEncerrar {...props} />)
    await userEvent.click(screen.getByRole('radio', { name: /stuck/ }))
    rerender(<ModalEncerrar {...props} enviando />)
    for (const campo of [...screen.getAllByRole('radio'), screen.getByRole('textbox'), ...screen.getAllByRole('button')]) {
      expect(campo).toBeDisabled()
    }
    await userEvent.click(screen.getByRole('button', { name: 'Encerrar como falha' }))
    await userEvent.click(screen.getByRole('button', { name: 'Cancelar' }))
    await userEvent.keyboard('{Escape}')
    await userEvent.click(screen.getByRole('dialog').parentElement!)
    expect(aoConfirmar).not.toHaveBeenCalled()
    expect(aoCancelar).not.toHaveBeenCalled()
    rerender(<ModalEncerrar {...props} enviando={false} />)
    expect(screen.getByRole('button', { name: 'Encerrar como falha' })).toBeEnabled()
  })
})

describe('ModalRetomar — US30 e US31', () => {
  it('exige as duas confirmações do protótipo e mantém a célula da falha', async () => {
    const aoConfirmar = vi.fn()
    render(<ModalRetomar {...propsRetomar} aoCancelar={vi.fn()} aoConfirmar={aoConfirmar} />)
    const confirmar = screen.getByRole('button', { name: 'Retomar da célula B2' })
    const caixas = screen.getAllByRole('checkbox')
    expect(confirmar).toBeDisabled()
    await userEvent.click(caixas[0])
    expect(confirmar).toBeDisabled()
    await userEvent.click(caixas[1])
    expect(confirmar).toBeEnabled()
    await userEvent.click(confirmar)
    expect(aoConfirmar).toHaveBeenCalledOnce()
  })

  it('mantém checklist e modal bloqueados enquanto o pedido está pendente', async () => {
    const aoCancelar = vi.fn()
    const aoConfirmar = vi.fn()
    const props = { ...propsRetomar, aoCancelar, aoConfirmar }
    const { rerender } = render(<ModalRetomar {...props} />)
    for (const caixa of screen.getAllByRole('checkbox')) await userEvent.click(caixa)
    rerender(<ModalRetomar {...props} enviando />)
    for (const campo of [...screen.getAllByRole('checkbox'), ...screen.getAllByRole('button')]) expect(campo).toBeDisabled()
    await userEvent.dblClick(screen.getByRole('button', { name: 'Retomar da célula B2' }))
    await userEvent.click(screen.getByRole('button', { name: 'Cancelar' }))
    await userEvent.keyboard('{Escape}')
    await userEvent.click(screen.getByRole('dialog').parentElement!)
    expect(aoConfirmar).not.toHaveBeenCalled()
    expect(aoCancelar).not.toHaveBeenCalled()
  })
})

function pedidoPendente() {
  let concluir!: () => void
  const resposta = new Promise<void>((resolve) => { concluir = resolve })
  return { resposta, concluir }
}

describe('FRONT-05 no fluxo REST existente', () => {
  it.each([
    ['health_check_ausente', /Não há health-check concluído/],
    ['health_check_reprovado', /health-check foi reprovado/],
    ['health_check_expirado', /validade do health-check expirou/],
    ['dip_divergente', /tipo de labirinto informado pelo robô difere/],
  ] as const)('mostra a recusa %s da dev sem consumir outra tentativa', async (motivo, mensagem) => {
    servidor.use(http.post('*/api/execucoes/:id/retomar', () => HttpResponse.json({ motivo, ocorrida_em: new Date().toISOString() }, { status: 409 })))
    renderizarRota('/?modal=retomar', '08-encerrada-como-falha')
    const modal = within(await screen.findByRole('dialog'))
    for (const caixa of modal.getAllByRole('checkbox')) await userEvent.click(caixa)
    await userEvent.click(modal.getByRole('button', { name: 'Retomar da célula B2' }))
    expect(await screen.findByRole('alert')).toHaveTextContent(mensagem)
    expect(screen.getByLabelText('Tentativa 1 de 3')).toBeInTheDocument()
    expect(modal.getByRole('button', { name: 'Retomar da célula B2' })).toBeEnabled()
  })

  it('usa o detalhe do HTTP 200 para encerrar mesmo sem SSE e com o GET indisponível', async () => {
    let consultas = 0
    servidor.use(http.get('*/api/execucoes/e-0042', () => {
      consultas++
      return new HttpResponse(null, { status: 500 })
    }))
    renderizarRota('/?modal=encerrar', '03-em-execucao')
    const modal = within(await screen.findByRole('dialog'))
    await userEvent.click(modal.getByRole('radio', { name: /collision/ }))
    await userEvent.click(modal.getByRole('button', { name: 'Encerrar como falha' }))
    expect(await screen.findByRole('button', { name: /Retomar tentativa \(2\/3\)/ })).toBeEnabled()
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument()
    expect(consultas).toBe(0)
  })

  it('mantém o pedido bloqueado até o GET trazer a tentativa aberta pelo HTTP 201', async () => {
    const pendente = pedidoPendente()
    servidor.use(http.get('*/api/execucoes/e-0042', async () => {
      await pendente.resposta
      return HttpResponse.json(detalheAPI(buscarExecucaoRegistrada('e-0042')!))
    }))
    try {
      renderizarRota('/?modal=retomar', '08-encerrada-como-falha')
      const modal = within(await screen.findByRole('dialog'))
      for (const caixa of modal.getAllByRole('checkbox')) await userEvent.click(caixa)
      await userEvent.click(modal.getByRole('button', { name: 'Retomar da célula B2' }))
      expect(await screen.findByRole('heading', { name: 'Retomada registrada' })).toBeInTheDocument()
      expect(screen.getByLabelText('Tentativa 2 de 3')).toBeInTheDocument()
      expect(screen.getByRole('button', { name: 'Carregar dados da tentativa' })).toBeDisabled()
      expect(screen.queryByRole('button', { name: /Retomar tentativa/ })).not.toBeInTheDocument()
      pendente.concluir()
      expect(await screen.findByRole('button', { name: 'Encerrar tentativa' })).toBeEnabled()
      expect(screen.queryByRole('heading', { name: 'Retomada registrada' })).not.toBeInTheDocument()
    } finally { pendente.concluir() }
  })

  it('refaz somente o GET se a tentativa foi aberta mas seus dados não carregaram', async () => {
    let consultas = 0
    let comandos = 0
    servidor.use(
      http.post('*/api/execucoes/:id/retomar', ({ params }) => {
        comandos++
        const dados = retomarTentativaRegistrada(String(params.id), Date.now())
        const nova = dados.tentativas.at(-1)!
        return HttpResponse.json({ execucao_id: dados.execucao_id, tentativa_id: nova.tentativa_id, attempt_index: nova.attempt_index }, { status: 201 })
      }),
      http.get('*/api/execucoes/e-0042', () => {
        consultas++
        return consultas === 1 ? new HttpResponse(null, { status: 500 })
          : HttpResponse.json(detalheAPI(buscarExecucaoRegistrada('e-0042')!))
      }),
    )
    renderizarRota('/?modal=retomar', '08-encerrada-como-falha')
    const modal = within(await screen.findByRole('dialog'))
    for (const caixa of modal.getAllByRole('checkbox')) await userEvent.click(caixa)
    await userEvent.click(modal.getByRole('button', { name: 'Retomar da célula B2' }))
    expect(await screen.findByRole('alert')).toHaveTextContent('A tentativa foi aberta')
    await userEvent.click(screen.getByRole('button', { name: 'Carregar dados da tentativa' }))
    expect(await screen.findByRole('button', { name: 'Encerrar tentativa' })).toBeEnabled()
    expect(screen.getByLabelText('Tentativa 2 de 3')).toBeInTheDocument()
    expect(comandos).toBe(1)
    expect(consultas).toBe(2)
  })

  it('um clique duplo encerra somente uma vez e preserva motivo, observação e retomada', async () => {
    const pendente = pedidoPendente()
    let chamadas = 0
    let recebido: EncerrarPedido | undefined
    servidor.use(http.post('*/api/execucoes/:id/encerrar-tentativa', async ({ params, request }) => {
      chamadas++
      recebido = await request.json() as EncerrarPedido
      await pendente.resposta
      return HttpResponse.json(detalheAPI(encerrarTentativaRegistrada(String(params.id), recebido, Date.now())))
    }))
    try {
      renderizarRota('/', '03-em-execucao')
      await userEvent.click(await screen.findByRole('button', { name: 'Encerrar tentativa' }))
      const modal = within(screen.getByRole('dialog'))
      await userEvent.click(modal.getByRole('radio', { name: /out_of_track/ }))
      await userEvent.type(modal.getByRole('textbox'), 'Roda fora do corredor')
      await userEvent.dblClick(modal.getByRole('button', { name: 'Encerrar como falha' }))
      await waitFor(() => expect(chamadas).toBe(1))
      expect(recebido).toEqual({ motivo: 'out_of_track', observacao: 'Roda fora do corredor' })
      expect(screen.getByRole('button', { name: 'Encerrar tentativa' })).toBeDisabled()
      expect(screen.getByRole('button', { name: /Nova execução/ })).toBeDisabled()
      expect(modal.getByRole('button', { name: 'Cancelar' })).toBeDisabled()
      pendente.concluir()
      expect(await screen.findByRole('button', { name: /Retomar tentativa \(2\/3\)/ })).toBeEnabled()
      expect(screen.queryByRole('dialog')).not.toBeInTheDocument()
      expect(screen.getByText(/Observação: Roda fora do corredor/)).toBeInTheDocument()
      expect(chamadas).toBe(1)
    } finally { pendente.concluir() }
  })

  it('um clique duplo retoma uma única vez e bloqueia as duas entradas de retomada', async () => {
    const pendente = pedidoPendente()
    let chamadas = 0
    servidor.use(http.post('*/api/execucoes/:id/retomar', async ({ params }) => {
      chamadas++
      await pendente.resposta
      const dados = retomarTentativaRegistrada(String(params.id), Date.now())
      const nova = dados.tentativas.at(-1)!
      return HttpResponse.json({ execucao_id: dados.execucao_id, tentativa_id: nova.tentativa_id, attempt_index: nova.attempt_index }, { status: 201 })
    }))
    try {
      renderizarRota('/?modal=retomar', '08-encerrada-como-falha')
      const modal = within(await screen.findByRole('dialog'))
      for (const caixa of modal.getAllByRole('checkbox')) await userEvent.click(caixa)
      await userEvent.dblClick(modal.getByRole('button', { name: 'Retomar da célula B2' }))
      await waitFor(() => expect(chamadas).toBe(1))
      expect(screen.getByRole('button', { name: /Retomar tentativa \(2\/3\)/ })).toBeDisabled()
      expect(screen.getByRole('button', { name: 'Retomar da célula B2 →' })).toBeDisabled()
      expect(screen.getByRole('button', { name: /Nova execução/ })).toBeDisabled()
      pendente.concluir()
      expect(await screen.findByLabelText('Tentativa 2 de 3')).toBeInTheDocument()
      expect(await screen.findByRole('button', { name: 'Encerrar tentativa' })).toBeEnabled()
      expect(screen.queryByRole('dialog')).not.toBeInTheDocument()
      expect(chamadas).toBe(1)
    } finally { pendente.concluir() }
  })

  it('conserva a escolha e libera os controles se o backend recusar o encerramento', async () => {
    servidor.use(http.post('*/api/execucoes/:id/encerrar-tentativa', () => HttpResponse.json({ detail: 'Não há tentativa aberta' }, { status: 409 })))
    renderizarRota('/?modal=encerrar', '03-em-execucao')
    const modal = within(await screen.findByRole('dialog'))
    await userEvent.click(modal.getByRole('radio', { name: /collision/ }))
    await userEvent.type(modal.getByRole('textbox'), 'Colisão observada')
    await userEvent.click(modal.getByRole('button', { name: 'Encerrar como falha' }))
    expect(await screen.findByRole('alert')).toHaveTextContent('Não há tentativa aberta')
    expect(modal.getByRole('radio', { name: /collision/ })).toBeChecked()
    expect(modal.getByRole('textbox')).toHaveValue('Colisão observada')
    expect(modal.getByRole('button', { name: 'Encerrar como falha' })).toBeEnabled()
    expect(screen.getByRole('button', { name: 'Encerrar tentativa' })).toBeEnabled()
  })
})

describe('consulta atrasada após um comando confirmado', () => {
  it.each(['running'] as const)('não reabre uma tentativa já encerrada como %s', (status) => {
    const atual = dadosTentativas('failed')
    expect(combinarRetrato(atual, dadosTentativas(status))).toBe(atual)
  })

  it('não volta para a tentativa anterior após retomar', () => {
    const atual = dadosTentativas('running', 2)
    expect(combinarRetrato(atual, dadosTentativas('failed', 1))).toBe(atual)
  })

  it('não reabre uma execução cancelada com a falha na terceira tentativa', () => {
    const atual = dadosTentativas('failed', 3, 'cancelada')
    expect(combinarRetrato(atual, dadosTentativas('failed', 3))).toBe(atual)
  })
})
