import type { Execucao, ExecucaoAtualResposta, TipoLabirinto } from '../api/tipos'
import { buscarCenario, type Cenario } from './cenarios'

// Cenário exibido pelos mocks. Vem de ?cenario= na URL (útil para abrir um
// estado direto) e fica guardado na sessão para sobreviver à navegação.

const CHAVE = 'mock:cenario'

let atual: string | null = null
let criada: ExecucaoAtualResposta | null = null
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
  }
  return buscarCenario(atual)
}

export function definirCenario(id: string): void {
  criada = null
  registradas.clear()
  salvarCenario(id)
}

function salvarCenario(id: string): void {
  atual = id
  try {
    sessionStorage.setItem(CHAVE, id)
  } catch {
    // sem sessionStorage o cenário só vale até recarregar a página
  }
}

export function dadosAtuais(agora: number): ExecucaoAtualResposta {
  return criada ?? cenarioAtual().atual(agora)
}

export function buscarExecucaoRegistrada(id: string): Execucao | undefined {
  return registradas.get(id)
}

/** Mantém a identidade da execução criada ao usar os comandos já existentes. */
export function avancarCenario(id: string): void {
  if (!criada?.execucao) {
    definirCenario(id)
    return
  }
  const anterior = criada.execucao
  const seguinte = buscarCenario(id).atual(Date.now()).execucao!
  const atualizada: Execucao = {
    ...seguinte,
    execucao_id: anterior.execucao_id,
    numero: anterior.numero,
    labirinto: anterior.labirinto,
    iniciada_em: anterior.iniciada_em,
    tentativas: seguinte.tentativas.map((tentativa) => ({
      ...tentativa,
      tentativa_id: anterior.tentativas.find((t) => t.attempt_index === tentativa.attempt_index)?.tentativa_id ?? crypto.randomUUID(),
    })),
  }
  registradas.set(atualizada.execucao_id, atualizada)
  criada = { ...criada, execucao: atualizada, recusa: null }
  salvarCenario(id)
}

/** Simula a criação atômica: cancela a anterior e abre a primeira tentativa. */
export function registrarNovaExecucao(labirinto: TipoLabirinto, agora: number): Execucao {
  const anterior = dadosAtuais(agora)
  const modelo = buscarCenario('02-health-check').atual(agora).execucao!
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
      bateria_inicial: null,
      health_check: [],
    }],
    trajeto: [],
    leituras: [],
  }
  if (anterior.execucao) {
    const encerrada = anterior.execucao.status === 'em_andamento'
      ? { ...anterior.execucao, status: 'cancelada' as const, encerrada_em: inicio }
      : anterior.execucao
    registradas.set(encerrada.execucao_id, encerrada)
  }
  registradas.set(nova.execucao_id, nova)
  criada = { ...anterior, execucao: nova, recusa: null, proximo_numero: numero + 1 }
  salvarCenario('02-health-check')
  return nova
}
