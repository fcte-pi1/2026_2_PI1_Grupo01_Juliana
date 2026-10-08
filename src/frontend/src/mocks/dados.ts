import type {
  Execucao,
  Falha,
  ItemHealthCheck,
  Labirinto,
  LeituraTelemetria,
  PassoTrajeto,
  ResumoExecucao,
  Tentativa,
} from '../api/tipos'

// Dados de exemplo do labirinto 4x4 do protótipo (execução #0042, largada em A1,
// objetivo em D4). As datas são relativas ao carregamento da página, para que
// "atualizado há X s" faça sentido.

export const LABIRINTOS: Labirinto[] = [
  { tipo: '4x4', largura: 4, altura: 4, melhor_tempo_s: 27.9, melhor_execucao_numero: 36 },
  { tipo: '8x4', largura: 8, altura: 4, melhor_tempo_s: 108.6, melhor_execucao_numero: 32 },
  { tipo: '12x4', largura: 12, altura: 4, melhor_tempo_s: null, melhor_execucao_numero: null },
]

const N = 1
const S = 2
const L = 4
const O = 8

/** Caminho até D4: A1 A2 A3 A4 B4 B3 B2 C2 C3 D3 D4. */
const CAMINHO: Array<[number, number]> = [
  [0, 0], [0, 1], [0, 2], [0, 3], [1, 3], [1, 2], [1, 1], [2, 1], [2, 2], [3, 2], [3, 3],
]

/** Paredes internas que não cortam o caminho: A1|B1, A2|B2, A3|B3, B4|C4, C2|D2. */
const PAREDES_INTERNAS_LESTE = new Set(['0,0', '0,1', '0,2', '1,3', '2,1'])

function paredes(x: number, y: number): number {
  let mascara = 0
  if (y === 3) mascara |= N
  if (y === 0) mascara |= S
  if (x === 3 || PAREDES_INTERNAS_LESTE.has(`${x},${y}`)) mascara |= L
  if (x === 0 || PAREDES_INTERNAS_LESTE.has(`${x - 1},${y}`)) mascara |= O
  return mascara
}

function passo(seq: number, [x, y]: [number, number], retomada = false): PassoTrajeto {
  return { seq, x, y, paredes_mask: paredes(x, y), retomada }
}

/** Trajeto da tentativa 1 até a célula `ate` (índice em CAMINHO). */
export function trajetoAte(ate: number): PassoTrajeto[] {
  return CAMINHO.slice(0, ate + 1).map((celula, i) => passo(i + 1, celula))
}

/** Tentativa 1 até B2 (falha) e tentativa 2 retomando da própria B2 até `ate`. */
export function trajetoComRetomada(ate: number): PassoTrajeto[] {
  const primeira = trajetoAte(6)
  const retomada = CAMINHO.slice(7, ate + 1)
  return [
    ...primeira,
    passo(primeira.length + 1, [1, 1], true),
    ...retomada.map((celula, i) => passo(primeira.length + 2 + i, celula)),
  ]
}

export function healthCheck(aprovados: number): ItemHealthCheck[] {
  const componentes: ItemHealthCheck['componente'][] = [
    'bateria',
    'tof_frontal',
    'tof_esquerdo',
    'tof_direito',
    'motor_esquerdo',
    'motor_direito',
    'encoder_esquerdo',
    'encoder_direito',
  ]
  return componentes.map((componente, i) => ({
    componente,
    aprovado: i < aprovados ? true : null,
    valor_lido: componente === 'bateria' && i < aprovados ? 6.16 : null,
  }))
}

export function relativo(agora: number, segundos: number): string {
  return new Date(agora + segundos * 1000).toISOString()
}

export function leituras(agora: number, trajeto: PassoTrajeto[], ultimaHaS: number): LeituraTelemetria[] {
  return trajeto.slice(-5).map((p, i, lista) => ({
    ordem: p.seq,
    x: p.x,
    y: p.y,
    bateria: 6.16 - p.seq * 0.005,
    velocidade: 0.09,
    enviado_em: relativo(agora, -ultimaHaS - (lista.length - 1 - i) * 1.5),
  }))
}

export function tentativa(agora: number, parcial: Partial<Tentativa> & Pick<Tentativa, 'attempt_index' | 'status'>): Tentativa {
  return {
    tentativa_id: `t-0042-${parcial.attempt_index}`,
    tipo_inicio: parcial.attempt_index === 1 ? 'nova' : 'retomada',
    tipo_descoberto: null,
    iniciada_em: relativo(agora, -19),
    encerrada_em: null,
    tempo_s: null,
    velocidade_media: null,
    bateria_inicial: 88,
    bateria_final: null,
    consumo_bateria: null,
    health_check: healthCheck(8),
    falha: null,
    ...parcial,
  }
}

export function falha(agora: number, parcial: Partial<Falha> = {}): Falha {
  return {
    motivo: 'stuck',
    origem: 'encerrado_operador',
    celula_x: 1,
    celula_y: 1,
    componente: null,
    observacao: null,
    momento_falha: relativo(agora, -30),
    ...parcial,
  }
}

export function execucao(agora: number, parcial: Partial<Execucao> = {}): Execucao {
  return {
    execucao_id: 'e-0042',
    numero: 42,
    labirinto: '4x4',
    status: 'em_andamento',
    tentativas_usadas: 1,
    iniciada_em: relativo(agora, -20),
    encerrada_em: null,
    tempo_total_s: null,
    tentativas: [],
    trajeto: [],
    leituras: [],
    ultima_mensagem_em: relativo(agora, -1),
    ...parcial,
  }
}

export const ULTIMA_EXECUCAO: ResumoExecucao = {
  execucao_id: 'e-0041',
  numero: 41,
  labirinto: '4x4',
  status: 'concluida',
  resultado: 'success',
  tentativas_usadas: 2,
  iniciada_em: '2026-09-24T21:12:47Z',
  tempo_total_s: 31.4,
  velocidade_media: 0.08,
  consumo_bateria: 3.2,
}

export const HISTORICO: ResumoExecucao[] = [
  ULTIMA_EXECUCAO,
  {
    execucao_id: 'e-0040',
    numero: 40,
    labirinto: '8x4',
    status: 'cancelada',
    resultado: 'failed',
    tentativas_usadas: 3,
    iniciada_em: '2026-09-24T20:40:02Z',
    tempo_total_s: 412.7,
    velocidade_media: 0.05,
    consumo_bateria: 9.8,
  },
  {
    execucao_id: 'e-0036',
    numero: 36,
    labirinto: '4x4',
    status: 'concluida',
    resultado: 'success',
    tentativas_usadas: 1,
    iniciada_em: '2026-09-23T19:05:10Z',
    tempo_total_s: 27.9,
    velocidade_media: 0.09,
    consumo_bateria: 2.6,
  },
  {
    execucao_id: 'e-0032',
    numero: 32,
    labirinto: '8x4',
    status: 'concluida',
    resultado: 'success',
    tentativas_usadas: 1,
    iniciada_em: '2026-09-22T18:20:44Z',
    tempo_total_s: 108.6,
    velocidade_media: 0.07,
    consumo_bateria: 6.1,
  },
]
