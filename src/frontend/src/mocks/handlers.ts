import { http, HttpResponse, sse } from 'msw'
import type { EventoExecucao, Execucao, TipoLabirinto } from '../api/tipos'
import { cenarioAtual, definirCenario } from './cenarioAtual'
import { buscarCenario } from './cenarios'
import { execucao, HISTORICO, LABIRINTOS, tentativa, trajetoAte } from './dados'

const API = '*/api'

function execucaoDoCenario(): Execucao {
  return cenarioAtual().atual(Date.now()).execucao ?? buscarCenario('04-concluida').atual(Date.now()).execucao!
}

/** Execuções antigas do histórico, montadas a partir da linha da tabela. */
function execucaoDoHistorico(id: string): Execucao | null {
  const resumo = HISTORICO.find((e) => e.execucao_id === id)
  if (!resumo) return null
  const agora = new Date(resumo.iniciada_em).getTime() + (resumo.tempo_total_s ?? 0) * 1000
  const sucesso = resumo.resultado === 'success'
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
  http.get(`${API}/labirintos`, () => HttpResponse.json(LABIRINTOS)),

  http.get(`${API}/execucoes/atual`, () => HttpResponse.json(cenarioAtual().atual(Date.now()))),

  http.get(`${API}/execucoes`, ({ request }) => {
    const labirinto = new URL(request.url).searchParams.get('labirinto') as TipoLabirinto | null
    return HttpResponse.json(labirinto ? HISTORICO.filter((e) => e.labirinto === labirinto) : HISTORICO)
  }),

  http.get(`${API}/execucoes/:id`, ({ params }) => {
    const id = String(params.id)
    const encontrada = id === 'e-0042' ? execucaoDoCenario() : execucaoDoHistorico(id)
    return encontrada ? HttpResponse.json(encontrada) : HttpResponse.json({ detail: 'Execução não encontrada' }, { status: 404 })
  }),

  // Comandos: trocam o cenário para o estado seguinte, como o backend faria.
  http.post(`${API}/execucoes`, () => {
    definirCenario('02-health-check')
    return HttpResponse.json(execucaoDoCenario(), { status: 201 })
  }),

  http.post(`${API}/execucoes/:id/encerrar`, () => {
    definirCenario('08-encerrada-como-falha')
    return HttpResponse.json(execucaoDoCenario())
  }),

  http.post(`${API}/execucoes/:id/retomar`, () => {
    const recusa = cenarioAtual().atual(Date.now()).recusa
    if (recusa) return HttpResponse.json(recusa, { status: 409 })
    definirCenario('10-retomada-tentativa-2')
    return HttpResponse.json(execucaoDoCenario())
  }),
]

/** Stream SSE da execução. Só no navegador: o jsdom dos testes não tem EventSource. */
export function criarHandlerStream() {
  return sse<{ message: string }>(`${API}/execucoes/:id/stream`, ({ client }) => {
    if (!cenarioAtual().aoVivo) return
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
