import type { EncerrarPedido, EventoExecucao, Execucao, ExecucaoAtualResposta, TipoLabirinto } from '../api/tipos'
import { aplicarEvento } from '../api/eventosExecucao'
import { buscarCenario, type Cenario } from './cenarios'
import { limparHealthCheckMock, ultimoHealthCheckMock } from './healthCheckAtual'

// Cenário exibido pelos mocks. Vem de ?cenario= na URL (útil para abrir um
// estado direto) e fica guardado na sessão para sobreviver à navegação.

const CHAVE = 'mock:cenario'
// Não reutiliza sessões antigas com V/m/s ou health-check dentro da tentativa.
const CHAVE_DADOS = 'mock:execucoes:v3'

let atual: string | null = null
let criada: ExecucaoAtualResposta | null = null
let inicioCenario: number | null = null
let origemUrl: string | null = null
const registradas = new Map<string, Execucao>()

function lerSessao(): string | null {
  try {
    return sessionStorage.getItem(CHAVE)
  } catch {
    return null
  }
}

export function cenarioAtual(): Cenario {
  if (atual === null) {
    const daUrl = typeof window === 'undefined' ? null : new URLSearchParams(window.location.search).get('cenario')
    atual = daUrl ?? lerSessao()
    origemUrl = daUrl
    try {
      const salvos = JSON.parse(sessionStorage.getItem(CHAVE_DADOS) ?? 'null')
      if (salvos && (salvos.cenario === atual || salvos.origemUrl === daUrl) && Number.isFinite(salvos.inicio)) {
        atual = salvos.cenario
        origemUrl = salvos.origemUrl
        inicioCenario = salvos.inicio
        criada = salvos.dados
        for (const [id, execucao] of salvos.registradas ?? []) registradas.set(id, execucao)
      }
    } catch {
      // Uma sessão ausente ou inválida inicia o cenário definido na URL.
    }
  }
  return buscarCenario(atual)
}

export function definirCenario(id: string): void {
  limparHealthCheckMock()
  criada = null
  registradas.clear()
  inicioCenario = Date.now()
  origemUrl = id
  salvarCenario(id)
}

function salvarCenario(id: string): void {
  atual = id
  try {
    sessionStorage.setItem(CHAVE, id)
    salvarDados()
  } catch {
    // sem sessionStorage o cenário só vale até recarregar a página
  }
}

export function dadosAtuais(agora = Date.now()): ExecucaoAtualResposta {
  const cenario = cenarioAtual()
  if (!criada) {
    inicioCenario ??= agora
    criada = cenario.atual(inicioCenario)
    if (criada.execucao) registradas.set(criada.execucao.execucao_id, criada.execucao)
    salvarDados()
  }
  return criada
}

function salvarDados(): void {
  try {
    sessionStorage.setItem(CHAVE_DADOS, JSON.stringify({ cenario: atual, origemUrl, inicio: inicioCenario,
      dados: criada, registradas: [...registradas] }))
  } catch {
    // Sem armazenamento, os mocks continuam funcionando até a página ser recarregada.
  }
}

/** Persiste os eventos dos exemplos existentes para o GET restaurar o mesmo estado. */
export function registrarEvento(id: string, evento: EventoExecucao): void {
  const atual = buscarExecucaoRegistrada(id)
  if (!atual) return
  let atualizada = aplicarEvento(atual, evento)
  if (evento.tipo === 'health_check' && evento.componente === 'bateria' && evento.aprovado) {
    atualizada = { ...atualizada, tentativas: atualizada.tentativas.map((tentativa) =>
      tentativa.tentativa_id === evento.tentativa_id ? { ...tentativa, bateria_inicial: evento.valor_lido ?? null } : tentativa) }
  }
  if (evento.tipo === 'sucesso') {
    const fim = new Date().toISOString()
    atualizada = { ...atualizada, encerrada_em: fim, tentativas: atualizada.tentativas.map((tentativa) =>
      tentativa.tentativa_id === evento.tentativa_id ? { ...tentativa, encerrada_em: fim,
        bateria_final: atualizada.leituras.at(-1)?.bateria ?? null } : tentativa) }
  }
  atualizarExecucao(atualizada)
}

function atualizarExecucao(atualizada: Execucao): void {
  const id = atualizada.execucao_id
  registradas.set(id, atualizada)
  if (criada?.execucao?.execucao_id === id) criada = { ...criada, execucao: atualizada }
  salvarDados()
}

export function buscarExecucaoRegistrada(id: string): Execucao | undefined {
  dadosAtuais(Date.now())
  return registradas.get(id)
}

export function listarExecucoesRegistradas(): Execucao[] {
  dadosAtuais(Date.now())
  return [...registradas.values()]
}

/** O comando altera a tentativa conhecida, preservando trajeto, paredes e medições. */
export function encerrarTentativaRegistrada(id: string, pedido: EncerrarPedido, agora: number): Execucao {
  const anterior = buscarExecucaoRegistrada(id)!
  const tentativa = anterior.tentativas.at(-1)!
  const celula = anterior.celula_atual ?? anterior.leituras.at(-1) ?? anterior.trajeto.at(-1) ?? { x: 0, y: 0 }
  registrarEvento(id, { tipo: 'falha', tentativa_id: tentativa.tentativa_id,
    status_execucao: anterior.tentativas_usadas >= 3 ? 'cancelada' : 'em_andamento',
    motivo: pedido.motivo, origem: 'encerrado_operador', componente: null,
    celula_x: celula.x, celula_y: celula.y, observacao: pedido.observacao,
    momento_falha: new Date(agora).toISOString() })
  const encerrada = buscarExecucaoRegistrada(id)!
  atualizarExecucao({ ...encerrada, encerrada_em: encerrada.status === 'cancelada' ? new Date(agora).toISOString() : null,
    tentativas: encerrada.tentativas.map((item) => item.tentativa_id === tentativa.tentativa_id
      ? { ...item, bateria_final: anterior.leituras.at(-1)?.bateria ?? null } : item) })
  return buscarExecucaoRegistrada(id)!
}

/** Cancela a execução. Uma tentativa aberta vira falha do operador e não há retomada. */
export function encerrarExecucaoRegistrada(id: string, agora: number): Execucao {
  const anterior = buscarExecucaoRegistrada(id)!
  const tentativa = [...anterior.tentativas].sort((a, b) => b.attempt_index - a.attempt_index)[0]
  const aberta = tentativa?.status === 'running'
  if (aberta) {
    const celula = anterior.celula_atual ?? anterior.leituras.at(-1) ?? anterior.trajeto.at(-1) ?? { x: 0, y: 0 }
    registrarEvento(id, {
      tipo: 'falha',
      tentativa_id: tentativa.tentativa_id,
      status_execucao: 'cancelada',
      motivo: 'encerrado_operador',
      origem: 'encerrado_operador',
      componente: null,
      celula_x: celula.x,
      celula_y: celula.y,
      observacao: null,
      momento_falha: new Date(agora).toISOString(),
    })
  }
  const atual = buscarExecucaoRegistrada(id)!
  atualizarExecucao({ ...atual, status: 'cancelada', encerrada_em: new Date(agora).toISOString() })
  return buscarExecucaoRegistrada(id)!
}

/** O health-check é anterior à abertura da nova tentativa, como no backend. */
export function retomarTentativaRegistrada(id: string, agora: number): Execucao {
  const anterior = buscarExecucaoRegistrada(id)!
  const modelo = buscarCenario('03-em-execucao').atual(agora).execucao!.tentativas[0]
  const hc = ultimoHealthCheckMock(agora, cenarioAtual().id)
  const nova = { ...modelo, tentativa_id: crypto.randomUUID(),
    attempt_index: anterior.tentativas_usadas + 1, tipo_inicio: 'retomada' as const,
    iniciada_em: new Date(agora).toISOString(), bateria_inicial: hc.itens.find((item) => item.componente === 'bateria')?.valor_lido ?? null,
    health_check: hc.itens.map((item) => ({ ...item, valor_lido: item.valor_lido ?? null })) }
  atualizarExecucao({ ...anterior, tentativas_usadas: nova.attempt_index,
    tentativas: [...anterior.tentativas, nova], leituras: [], ultima_mensagem_em: null })
  salvarCenario('03-em-execucao')
  return buscarExecucaoRegistrada(id)!
}

/** Simula a criação atômica: cancela a anterior e abre a primeira tentativa. */
export function registrarNovaExecucao(labirinto: TipoLabirinto, agora: number): Execucao {
  const anterior = dadosAtuais(agora)
  const modelo = buscarCenario('03-em-execucao').atual(agora).execucao!
  const hc = ultimoHealthCheckMock(agora, cenarioAtual().id)
  const inicio = new Date(agora).toISOString()
  const numero = Math.max(anterior.proximo_numero ?? 0, (anterior.execucao?.numero ?? 0) + 1)
  const nova: Execucao = {
    ...modelo,
    execucao_id: crypto.randomUUID(),
    numero,
    labirinto,
    iniciada_em: inicio,
    ultima_mensagem_em: null,
    tentativas: [{
      ...modelo.tentativas[0],
      tentativa_id: crypto.randomUUID(),
      iniciada_em: inicio,
      bateria_inicial: hc.itens.find((item) => item.componente === 'bateria')?.valor_lido ?? null,
      health_check: hc.itens.map((item) => ({ ...item, valor_lido: item.valor_lido ?? null })),
    }],
    trajeto: [],
    leituras: [],
  }
  if (anterior.execucao) {
    const encerrada = anterior.execucao.status === 'em_andamento'
      ? { ...anterior.execucao, status: 'cancelada' as const, encerrada_em: inicio,
        tentativas: anterior.execucao.tentativas.map((tentativa) => tentativa.status === 'running'
          ? { ...tentativa, status: 'failed' as const, encerrada_em: inicio } : tentativa) }
      : anterior.execucao
    registradas.set(encerrada.execucao_id, encerrada)
  }
  registradas.set(nova.execucao_id, nova)
  criada = { ...anterior, execucao: nova, recusa: null, proximo_numero: numero + 1 }
  salvarCenario('03-em-execucao')
  return nova
}
