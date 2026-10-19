import type { EventoExecucao } from '../api/tipos'
import { buscarExecucaoRegistrada, cenarioAtual, registrarEvento } from './cenarioAtual'
import { buscarCenario } from './cenarios'
import { healthCheck, trajetoAte } from './dados'

/**
 * Demonstração local: reproduz somente health-check, caminho e medições dos
 * exemplos 4x4 já definidos. O intervalo não representa dados reais do robô.
 * Sem um exemplo de percurso 8x4/12x4, a corrida fica aguardando dados.
 */
export function avancarDemonstracao(id: string, agora: number): EventoExecucao[] {
  const atual = buscarExecucaoRegistrada(id)
  const tentativa = atual?.tentativas.at(-1)
  if (!atual || !tentativa || atual.status !== 'em_andamento' || !cenarioAtual().aoVivo) return []
  const eventos: EventoExecucao[] = []
  if (tentativa.status === 'health-check') {
    const pendente = healthCheck(8).find((exemplo) => !tentativa.health_check.some((item) =>
      item.componente === exemplo.componente && item.aprovado === true))
    if (tentativa.health_check.some((item) => item.aprovado === false)) return []
    if (pendente) {
      eventos.push({ tipo: 'health_check', tentativa_id: tentativa.tentativa_id,
        componente: pendente.componente, aprovado: true, valor_lido: pendente.valor_lido })
    } else {
      eventos.push({ tipo: 'status', tentativa_id: tentativa.tentativa_id,
        attempt_index: tentativa.attempt_index, status_tentativa: 'running',
        status_execucao: 'em_andamento', tipo_dip: atual.labirinto })
      if (tentativa.tipo_inicio === 'retomada' && atual.trajeto.length) {
        const ultima = atual.trajeto.at(-1)!
        const seq = ultima.seq + 1
        eventos.push({ tipo: 'passo', ...ultima, seq, passo_id: seq,
          retomada: true, entrou_em: new Date(agora).toISOString() })
      }
    }
  } else if (tentativa.status === 'running' && atual.labirinto === '4x4') {
    const caminho = trajetoAte(10)
    const ultima = atual.trajeto.at(-1)
    const indice = ultima ? caminho.findIndex((passo) => passo.x === ultima.x && passo.y === ultima.y) : -1
    const proximo = caminho[indice + 1]
    if (proximo) {
      const seq = (ultima?.seq ?? 0) + 1
      eventos.push({ tipo: 'passo', ...proximo, seq, passo_id: seq,
        entrou_em: new Date(agora).toISOString() })
      const exemplos = ['03-em-execucao', '04-concluida'].flatMap((cenario) =>
        buscarCenario(cenario).atual(agora).execucao!.leituras)
      const leitura = exemplos.findLast((leitura) => leitura.x === proximo.x && leitura.y === proximo.y)
      if (leitura) eventos.push({ tipo: 'telemetria', ...leitura,
        ordem: (atual.leituras.at(-1)?.ordem ?? 0) + 1, enviado_em: new Date(agora).toISOString() })
    } else if (indice === caminho.length - 1) {
      eventos.push({ tipo: 'sucesso', tentativa_id: tentativa.tentativa_id,
        x: ultima!.x, y: ultima!.y, status_execucao: 'concluida' })
    }
  }
  for (const evento of eventos) registrarEvento(id, evento)
  return eventos
}
