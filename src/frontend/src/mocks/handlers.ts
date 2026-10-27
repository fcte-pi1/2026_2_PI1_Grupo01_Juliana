import { http, HttpResponse, sse } from 'msw'
import type { EncerrarPedido, EventoExecucao, Execucao, NovaExecucaoPedido, TipoLabirinto } from '../api/tipos'
import { buscarExecucaoRegistrada, cenarioAtual, dadosAtuais, encerrarExecucaoRegistrada, encerrarTentativaRegistrada, listarExecucoesRegistradas, registrarNovaExecucao, retomarTentativaRegistrada } from './cenarioAtual'
import { recusaHealthCheckMock, solicitarHealthCheckMock, ultimoHealthCheckMock } from './healthCheckAtual'
import { detalheAPI, execucao, falha, HISTORICO, resumoAPI, tentativa, trajeto12x4Longo, trajetoAte } from './dados'
import { avancarDemonstracao } from './fluxoDemonstracao'

const API = '*/api'

function trajetoDoHistorico(labirinto: TipoLabirinto, sucesso: boolean) {
  if (labirinto === '4x4') return trajetoAte(sucesso ? 10 : 6)
  if (labirinto === '12x4') return trajeto12x4Longo()
  return []
}

/** Execuções antigas do histórico, montadas a partir da linha da tabela. */
function execucaoDoHistorico(id: string): Execucao | null {
  const resumo = HISTORICO.find((e) => e.execucao_id === id)
  if (!resumo) return null
  const agora = new Date(resumo.iniciada_em).getTime() + (resumo.tempo_total_s ?? 0) * 1000
  const sucesso = resumo.resultado === 'concluida'
  return execucao(agora, {
    execucao_id: resumo.execucao_id,
    numero: resumo.numero,
    labirinto: resumo.labirinto,
    status: resumo.status,
    tentativas_usadas: resumo.tentativas_usadas,
    tempo_total_s: resumo.tempo_total_s,
    encerrada_em: new Date(agora).toISOString(),
    tentativas: [
      tentativa(agora, {
        tentativa_id: `t-${resumo.numero}`,
        attempt_index: resumo.tentativas_usadas,
        status: sucesso ? 'success' : 'failed',
        tipo_descoberto: sucesso ? resumo.labirinto : null,
        tempo_s: resumo.tempo_total_s,
        velocidade_media: resumo.velocidade_media,
        consumo_bateria: resumo.consumo_bateria,
        falha:
          resumo.labirinto === '12x4' && !sucesso
            ? falha(agora, { motivo: 'time_exceeded', origem: 'automatica', celula_x: 11, celula_y: 2 })
            : null,
      }),
    ],
    trajeto: trajetoDoHistorico(resumo.labirinto, sucesso),
    ultima_mensagem_em: new Date(agora).toISOString(),
  })
}

export const handlers = [
  http.get(`${API}/health-check/ultimo`, () => HttpResponse.json(ultimoHealthCheckMock(Date.now(), cenarioAtual().id))),
  http.post(`${API}/health-check/solicitar`, () => {
    if (dadosAtuais().execucao?.tentativas.some((tentativa) => tentativa.status === 'running')) {
      return HttpResponse.json({ detail: 'Há uma tentativa em execução; encerre-a antes de pedir um novo health-check.' }, { status: 409 })
    }
    return HttpResponse.json(solicitarHealthCheckMock(Date.now(), cenarioAtual().id), { status: 202 })
  }),
  http.get(`${API}/execucoes/em-andamento`, () => {
    const atual = dadosAtuais(Date.now()).execucao
    return atual?.status === 'em_andamento' ? HttpResponse.json(detalheAPI(atual)) : new HttpResponse(null, { status: 204 })
  }),

  http.get(`${API}/execucoes`, ({ request }) => {
    const busca = new URL(request.url).searchParams
    const labirinto = busca.get('tipo_labirinto') as TipoLabirinto | null
    const offset = Number(busca.get('offset') ?? 0)
    const limite = Number(busca.get('limite') ?? 50)
    const registradas = listarExecucoesRegistradas().map((item) => ({ ...item, resultado: item.status,
      velocidade_media: item.tentativas.at(-1)?.velocidade_media ?? null,
      consumo_bateria: item.tentativas.at(-1)?.consumo_bateria ?? null }))
    const historico = [...registradas, ...HISTORICO.filter((item) => !registradas.some((registrada) => registrada.execucao_id === item.execucao_id))]
      .sort((a, b) => Date.parse(b.iniciada_em) - Date.parse(a.iniciada_em))
    const filtrado = labirinto ? historico.filter((e) => e.labirinto === labirinto) : historico
    return HttpResponse.json(filtrado.slice(offset, offset + limite).map(resumoAPI),
      { headers: { 'X-Total-Count': String(filtrado.length) } })
  }),

  http.get(`${API}/execucoes/:id`, ({ params }) => {
    const id = String(params.id)
    const encontrada = buscarExecucaoRegistrada(id) ?? execucaoDoHistorico(id)
    return encontrada ? HttpResponse.json(detalheAPI(encontrada)) : HttpResponse.json({ detail: 'Execução não encontrada' }, { status: 404 })
  }),

  // Comandos atualizam a mesma execução persistida, como no contrato REST.
  http.post(`${API}/execucoes`, async ({ request }) => {
    const recusa = dadosAtuais(Date.now()).recusa
    if (recusa) return HttpResponse.json(recusa, { status: 409 })
    const pedido = await request.json() as NovaExecucaoPedido
    if (!['4x4', '8x4', '12x4'].includes(pedido.tipo_labirinto)) return new HttpResponse(null, { status: 422 })
    const motivo = recusaHealthCheckMock(pedido.tipo_labirinto, Date.now(), cenarioAtual().id)
    if (motivo) return HttpResponse.json({ motivo, ocorrida_em: new Date().toISOString() }, { status: 409 })
    const nova = registrarNovaExecucao(pedido.tipo_labirinto, Date.now())
    return HttpResponse.json({
      execucao_id: nova.execucao_id,
      tentativa_id: nova.tentativas[0].tentativa_id,
      attempt_index: 1,
    }, { status: 201 })
  }),

  http.post(`${API}/execucoes/:id/encerrar-tentativa`, async ({ params, request }) => {
    const id = String(params.id)
    const atual = buscarExecucaoRegistrada(id)
    if (!atual) return HttpResponse.json({ detail: 'Execução não encontrada' }, { status: 404 })
    if (atual.status !== 'em_andamento' || atual.tentativas.at(-1)?.status !== 'running') {
      return HttpResponse.json({ detail: 'Não há tentativa aberta' }, { status: 409 })
    }
    const pedido = await request.json() as EncerrarPedido
    if (!['collision', 'stuck', 'out_of_track'].includes(pedido.motivo)
      || (pedido.observacao != null && (typeof pedido.observacao !== 'string' || pedido.observacao.length > 100))) return new HttpResponse(null, { status: 422 })
    return HttpResponse.json(detalheAPI(encerrarTentativaRegistrada(id, pedido, Date.now())))
  }),

  http.post(`${API}/execucoes/:id/encerrar`, ({ params }) => {
    const id = String(params.id)
    const atual = buscarExecucaoRegistrada(id)
    if (!atual) return HttpResponse.json({ detail: 'Execução não encontrada.' }, { status: 404 })
    if (atual.status !== 'em_andamento') {
      return HttpResponse.json({ detail: 'A execução não está em andamento.' }, { status: 409 })
    }
    return HttpResponse.json(detalheAPI(encerrarExecucaoRegistrada(id, Date.now())))
  }),

  http.post(`${API}/execucoes/:id/retomar`, ({ params }) => {
    const id = String(params.id)
    const atual = buscarExecucaoRegistrada(id)
    if (!atual) return HttpResponse.json({ detail: 'Execução não encontrada' }, { status: 404 })
    const recusa = dadosAtuais(Date.now()).recusa
    if (recusa) return HttpResponse.json(recusa, { status: 409 })
    const motivo = atual.tentativas.at(-1)?.status === 'running' ? 'tentativa_aberta'
      : atual.tentativas_usadas >= 3 ? 'limite_3'
        : atual.status !== 'em_andamento' || atual.tentativas.at(-1)?.status !== 'failed' ? 'execucao_cancelada'
          : recusaHealthCheckMock(atual.labirinto, Date.now(), cenarioAtual().id)
    if (motivo) return HttpResponse.json({ motivo, ocorrida_em: new Date().toISOString() }, { status: 409 })
    const retomada = retomarTentativaRegistrada(id, Date.now())
    const tentativa = retomada.tentativas.at(-1)!
    return HttpResponse.json({ execucao_id: id, tentativa_id: tentativa.tentativa_id,
      attempt_index: tentativa.attempt_index }, { status: 201 })
  }),
]

/** Stream do health-check. O primeiro evento é o GET; o relógio de 1 s republica o retrato. */
export function criarHandlerHealthCheck() {
  return sse<{ health_check: string }>(`${API}/health-check/stream`, ({ client, request }) => {
    const enviar = () => {
      const atual = ultimoHealthCheckMock(Date.now(), cenarioAtual().id)
      client.send({ event: 'health_check', data: JSON.stringify({ tipo: 'health_check', ...atual }) })
    }
    enviar()
    const relogio = setInterval(() => {
      try {
        enviar()
      } catch {
        clearInterval(relogio)
      }
    }, 1_000)
    request.signal.addEventListener('abort', () => clearInterval(relogio), { once: true })
  })
}

/** Stream SSE da execução. Só no navegador: o jsdom dos testes não tem EventSource. */
export function criarHandlerStream() {
  return sse<Record<EventoExecucao['tipo'], string>>(`${API}/execucoes/:id/stream`, ({ client, params, request }) => {
    const id = String(params.id)
    const atual = buscarExecucaoRegistrada(id)
    if (!atual) { client.close(); return }
    const pedidoCursor = Number(request.headers.get('Last-Event-ID') ?? 0)
    const cursor = Number.isFinite(pedidoCursor) && pedidoCursor >= 0 ? pedidoCursor : 0
    for (const passo of atual.trajeto) {
      const passoId = passo.passo_id ?? passo.seq
      if (passoId > cursor) client.send({ event: 'passo', id: String(passoId), data: JSON.stringify({ tipo: 'passo', ...passo, passo_id: passoId }) })
    }
    const tentativa = atual.tentativas.at(-1)
    if (tentativa) {
      for (const item of tentativa.health_check) {
        if (item.aprovado !== null) client.send({ event: 'health_check', data: JSON.stringify({ tipo: 'health_check', tentativa_id: tentativa.tentativa_id, ...item }) })
      }
      client.send({ event: 'status', data: JSON.stringify({ tipo: 'status', tentativa_id: tentativa.tentativa_id,
        attempt_index: tentativa.attempt_index, status_tentativa: tentativa.status, status_execucao: atual.status, tipo_dip: tentativa.tipo_descoberto }) })
    }
    if (atual.status !== 'em_andamento') { client.close(); return }
    const intervalo = setInterval(() => {
      const atual = buscarExecucaoRegistrada(id)
      if (!atual || atual.status !== 'em_andamento') {
        clearInterval(intervalo)
        client.close()
        return
      }
      try {
        for (const evento of avancarDemonstracao(id, Date.now())) {
          client.send({ event: evento.tipo, ...('passo_id' in evento ? { id: String(evento.passo_id) } : {}), data: JSON.stringify(evento) })
        }
      } catch {
        clearInterval(intervalo)
      }
    }, 1000)
    request.signal.addEventListener('abort', () => clearInterval(intervalo), { once: true })
  })
}
