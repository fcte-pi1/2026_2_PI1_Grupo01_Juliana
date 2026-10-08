import { ROTULO_COMPONENTE } from '../utils/formatacao'
import { api } from './clienteApi'
import type { components } from './schema'
import type {
  ComponenteHealthCheck, EncerrarPedido, Execucao, ExecucaoAtualResposta,
  Labirinto, NovaExecucaoPedido, ResumoExecucao, RetornoNovaExecucao, TipoLabirinto,
} from './tipos'

type DetalheAPI = components['schemas']['ExecucaoDetalhe'] & { numero?: number }
type ResumoAPI = components['schemas']['ResumoExecucao'] & { numero?: number }

function normalizarExecucao(dados: DetalheAPI): Execucao {
  return {
    ...dados,
    labirinto: dados.tipo_labirinto,
    encerrada_em: dados.encerrada_em ?? null,
    tempo_total_s: dados.tempo_total_s ?? null,
    ultima_mensagem_em: dados.ultima_mensagem_em ?? null,
    trajeto: dados.trajeto ?? [],
    leituras: (dados.leituras ?? []).map((leitura) => ({ ...leitura, velocidade: leitura.velocidade ?? null })),
    tentativas: (dados.tentativas ?? []).map((tentativa) => ({
      ...tentativa,
      tipo_descoberto: tentativa.tipo_dip ?? null,
      encerrada_em: tentativa.encerrada_em ?? null,
      tempo_s: tentativa.tempo_s ?? null,
      velocidade_media: tentativa.velocidade_media ?? null,
      bateria_inicial: tentativa.bateria_inicial ?? null,
      bateria_final: tentativa.bateria_final ?? null,
      consumo_bateria: tentativa.consumo_bateria ?? null,
      // Componentes sem resposta aparecem como pendentes, sem inventar resultados.
      health_check: !tentativa.health_check?.length ? [] : (Object.keys(ROTULO_COMPONENTE) as ComponenteHealthCheck[]).map((componente) => {
        const item = tentativa.health_check.find((i) => i.componente === componente)
        return { componente, aprovado: item?.aprovado ?? null, valor_lido: item?.valor_lido ?? null }
      }),
      falha: tentativa.falha ? {
        ...tentativa.falha,
        componente: tentativa.falha.componente ?? null,
        observacao: tentativa.falha.observacao ?? null,
      } : null,
    })),
  }
}

export const listarExecucoes = async (labirinto: TipoLabirinto | null, sinal?: AbortSignal): Promise<ResumoExecucao[]> => {
  const dados = await api.get<ResumoAPI[]>(labirinto ? `/execucoes?tipo_labirinto=${labirinto}` : '/execucoes', sinal)
  return dados.map((resumo) => ({
    ...resumo,
    labirinto: resumo.tipo_labirinto,
    resultado: resumo.status,
    tempo_total_s: resumo.tempo_total_s ?? null,
    velocidade_media: resumo.velocidade_media ?? null,
    consumo_bateria: resumo.consumo_bateria ?? null,
  }))
}

export const buscarExecucaoAtual = async (sinal?: AbortSignal): Promise<ExecucaoAtualResposta> => {
  const [execucao, historico] = await Promise.all([
    api.get<DetalheAPI | null>('/execucoes/em-andamento', sinal),
    listarExecucoes(null, sinal),
  ])
  return {
    execucao: execucao ? normalizarExecucao(execucao) : null,
    ultima: historico.find((resumo) => resumo.status !== 'em_andamento') ?? null,
    recusa: null,
  }
}

/** Os três tipos são definidos no requisito. As marcas vêm do histórico da API. */
export const listarLabirintos = async (sinal?: AbortSignal): Promise<Labirinto[]> => {
  const historico = await listarExecucoes(null, sinal)
  const tipos: TipoLabirinto[] = ['4x4', '8x4', '12x4']
  return tipos.map((tipo) => {
    const [largura, altura] = tipo.split('x').map(Number)
    const melhor = historico.filter((e) => e.labirinto === tipo && e.status === 'concluida' && e.tempo_total_s !== null)
      .sort((a, b) => a.tempo_total_s! - b.tempo_total_s!)[0]
    return { tipo, largura, altura, melhor_tempo_s: melhor?.tempo_total_s ?? null, melhor_execucao_numero: melhor?.numero ?? null }
  })
}

export const buscarExecucao = async (id: string, sinal?: AbortSignal) =>
  normalizarExecucao(await api.get<DetalheAPI>(`/execucoes/${id}`, sinal))

export const criarExecucao = (pedido: NovaExecucaoPedido) =>
  api.post<RetornoNovaExecucao>('/execucoes', pedido)

export const encerrarTentativa = (id: string, pedido: EncerrarPedido) =>
  api.post<Execucao>(`/execucoes/${id}/encerrar`, pedido)

export const retomarTentativa = (id: string) =>
  api.post<Execucao>(`/execucoes/${id}/retomar`)
