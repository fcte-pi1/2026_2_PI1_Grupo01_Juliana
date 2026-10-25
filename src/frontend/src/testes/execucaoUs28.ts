import type { Execucao, Falha, PassoTrajeto, Tentativa } from '../api/tipos'
import { execucao, falha, tentativa } from '../mocks/dados'

const INICIO = Date.parse('2026-10-24T03:41:05.171Z')

/** Paredes do corredor A1→A4→D4. N = 1, S = 2, L = 4, O = 8; o lado aberto não leva bit. */
const MASCARA: Record<string, number> = {
  '0,0': 14,
  '0,1': 12,
  '0,2': 12,
  '0,3': 9,
  '1,3': 3,
  '2,3': 3,
  '3,3': 7,
}

/** US28: T1 e T2 interrompidas pelo operador; T3 conclui o 4x4 sem atravessar parede. */
export function execucaoUs28(): Execucao {
  const trechos: Array<{
    status: Tentativa['status']
    motivo: Falha['motivo'] | null
    celulas: Array<[number, number]>
    tempo: number
    velocidade: number
    bateriaInicial: number
    bateriaFinal: number
  }> = [
    {
      status: 'failed', motivo: 'collision', celulas: [[0, 0], [0, 1], [0, 2]],
      tempo: 6.2, velocidade: 0.12, bateriaInicial: 8400, bateriaFinal: 8300,
    },
    {
      status: 'failed', motivo: 'stuck', celulas: [[0, 2], [0, 3], [1, 3]],
      tempo: 4.8, velocidade: 0.15, bateriaInicial: 8300, bateriaFinal: 8200,
    },
    {
      status: 'success', motivo: null, celulas: [[1, 3], [2, 3], [3, 3]],
      tempo: 9.5, velocidade: 0.18, bateriaInicial: 8200, bateriaFinal: 8000,
    },
  ]
  const trajeto: PassoTrajeto[] = []
  const tentativas = trechos.map((trecho, indice) => {
    const inicio = INICIO + indice * 40_000
    trecho.celulas.forEach(([x, y], ordem) => {
      trajeto.push({
        seq: trajeto.length + 1,
        x,
        y,
        paredes_mask: MASCARA[`${x},${y}`],
        retomada: indice > 0 && ordem === 0,
        entrou_em: new Date(inicio + ordem * 300).toISOString(),
      })
    })
    const [celulaX, celulaY] = trecho.celulas.at(-1)!
    return tentativa(inicio, {
      tentativa_id: `t-0043-${indice + 1}`,
      attempt_index: indice + 1,
      status: trecho.status,
      tipo_inicio: indice === 0 ? 'nova' : 'retomada',
      iniciada_em: new Date(inicio).toISOString(),
      encerrada_em: new Date(inicio + 35_000).toISOString(),
      tempo_s: trecho.tempo,
      velocidade_media: trecho.velocidade,
      bateria_inicial: trecho.bateriaInicial,
      bateria_final: trecho.bateriaFinal,
      consumo_bateria: trecho.bateriaInicial - trecho.bateriaFinal,
      falha: trecho.motivo
        ? falha(inicio, {
          motivo: trecho.motivo,
          origem: 'encerrado_operador',
          celula_x: celulaX,
          celula_y: celulaY,
          componente: null,
          momento_falha: new Date(inicio + 30_000).toISOString(),
        })
        : null,
    })
  })
  return execucao(INICIO, {
    execucao_id: 'e-0043',
    numero: 43,
    status: 'concluida',
    tentativas_usadas: 3,
    iniciada_em: new Date(INICIO).toISOString(),
    encerrada_em: new Date(INICIO + 120_000).toISOString(),
    tempo_total_s: 20.5,
    variacao_melhor_tempo_pct: 12.5,
    tentativas,
    trajeto,
    ultima_mensagem_em: new Date(INICIO + 80_600).toISOString(),
  })
}
