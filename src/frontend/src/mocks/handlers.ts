import { http, HttpResponse, sse } from 'msw'
import type { EventoExecucao, Execucao, NovaExecucaoPedido, TipoLabirinto } from '../api/tipos'
import { avancarCenario, buscarExecucaoRegistrada, cenarioAtual, dadosAtuais, registrarNovaExecucao } from './cenarioAtual'
import { buscarCenario } from './cenarios'
import { detalheAPI, execucao, HISTORICO, resumoAPI, tentativa, trajetoAte } from './dados'

const API = '*/api'

function execucaoDoCenario(): Execucao {
  return dadosAtuais(Date.now()).execucao ?? buscarCenario('04-concluida').atual(Date.now()).execucao!
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
      }),
    ],
    trajeto: resumo.labirinto === '4x4' ? trajetoAte(sucesso ? 10 : 6) : [],
    ultima_mensagem_em: new Date(agora).toISOString(),
  })
}

export const handlers = [
  http.get(`${API}/execucoes/em-andamento`, () => {
    const atual = dadosAtuais(Date.now()).execucao
    return atual ? HttpResponse.json(detalheAPI(atual)) : new HttpResponse(null, { status: 204 })
  }),

  http.get(`${API}/execucoes`, ({ request }) => {
    const labirinto = new URL(request.url).searchParams.get('tipo_labirinto') as TipoLabirinto | null
    return HttpResponse.json((labirinto ? HISTORICO.filter((e) => e.labirinto === labirinto) : HISTORICO).map(resumoAPI))
  }),

  http.get(`${API}/execucoes/:id`, ({ params }) => {
    const id = String(params.id)
    const encontrada = buscarExecucaoRegistrada(id) ?? (id === 'e-0042' ? execucaoDoCenario() : execucaoDoHistorico(id))
    return encontrada ? HttpResponse.json(detalheAPI(encontrada)) : HttpResponse.json({ detail: 'Execução não encontrada' }, { status: 404 })
  }),

  // Comandos: trocam o cenário para o estado seguinte, como o backend faria.
  http.post(`${API}/execucoes`, async ({ request }) => {
    const recusa = dadosAtuais(Date.now()).recusa
    if (recusa) return HttpResponse.json(recusa, { status: 409 })
    const pedido = await request.json() as NovaExecucaoPedido
    if (!['4x4', '8x4', '12x4'].includes(pedido.tipo_labirinto)) return new HttpResponse(null, { status: 422 })
    const nova = registrarNovaExecucao(pedido.tipo_labirinto, Date.now())
    return HttpResponse.json({
      execucao_id: nova.execucao_id,
      tentativa_id: nova.tentativas[0].tentativa_id,
      attempt_index: 1,
    }, { status: 201 })
  }),

  http.post(`${API}/execucoes/:id/encerrar`, () => {
    avancarCenario('08-encerrada-como-falha')
    return HttpResponse.json(execucaoDoCenario())
  }),

  http.post(`${API}/execucoes/:id/retomar`, () => {
    const recusa = dadosAtuais(Date.now()).recusa
    if (recusa) return HttpResponse.json(recusa, { status: 409 })
    avancarCenario('10-retomada-tentativa-2')
    return HttpResponse.json(execucaoDoCenario())
  }),
]

/** Stream SSE da execução. Só no navegador: o jsdom dos testes não tem EventSource. */
export function criarHandlerStream() {
  return sse<{ message: string }>(`${API}/execucoes/:id/stream`, ({ client }) => {
    if (!cenarioAtual().aoVivo || execucaoDoCenario().leituras.length === 0) return
    let ordem = 100
    const intervalo = setInterval(() => {
      const ultima = execucaoDoCenario().leituras.at(-1)
      const evento: EventoExecucao = {
        tipo: 'leitura',
        leitura: {
          ordem: ordem++,
          x: ultima?.x ?? 0,
          y: ultima?.y ?? 0,
          bateria: ultima?.bateria ?? 6.1,
          velocidade: 0.09,
          enviado_em: new Date().toISOString(),
        },
      }
      try {
        client.send({ data: JSON.stringify(evento) })
      } catch {
        clearInterval(intervalo)
      }
    }, 1000)
  })
}
