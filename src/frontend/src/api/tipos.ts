// Tipos trocados com o backend.
//
// Provisórios: seguem o dicionário de dados do 4.4 (Persistência de dados) e
// serão substituídos pelos tipos gerados do OpenAPI quando o ARQ-02 fechar o
// contrato REST + SSE. Até lá, mudanças no DER precisam ser refletidas aqui.

export type TipoLabirinto = '4x4' | '8x4' | '12x4'

export type StatusExecucao = 'em_andamento' | 'concluida' | 'cancelada'

export type StatusTentativa = 'health-check' | 'running' | 'success' | 'failed'

export type TipoInicio = 'nova' | 'retomada'

export type ComponenteHealthCheck =
  | 'bateria'
  | 'tof_frontal'
  | 'tof_esquerdo'
  | 'tof_direito'
  | 'motor_esquerdo'
  | 'motor_direito'
  | 'encoder_esquerdo'
  | 'encoder_direito'

export type MotivoFalha =
  | 'collision'
  | 'stuck'
  | 'out_of_track'
  | 'time_exceeded'
  | 'falha_componente'
  | 'low_battery'
  | 'health_check_failed'
  | 'link_lost'
  | 'encerrado_operador'

/** Motivos que o operador pode escolher em Encerrar tentativa (RF30). */
export type MotivoEncerramento = Extract<MotivoFalha, 'collision' | 'stuck' | 'out_of_track'>

export type OrigemFalha = 'automatica' | 'encerrado_operador'

export type MotivoRecusa = 'tentativa_aberta' | 'limite_3' | 'execucao_cancelada'

export interface Celula {
  x: number
  y: number
}

export interface Labirinto {
  tipo: TipoLabirinto
  largura: number
  altura: number
  melhor_tempo_s: number | null
  melhor_execucao_numero: number | null
}

export interface ItemHealthCheck {
  componente: ComponenteHealthCheck
  aprovado: boolean | null
  valor_lido: number | null
}

export interface LeituraTelemetria {
  ordem: number
  x: number
  y: number
  bateria: number
  velocidade: number
  enviado_em: string
}

export interface PassoTrajeto {
  seq: number
  x: number
  y: number
  /** Paredes lidas na célula, em bits: N = 1, S = 2, L = 4, O = 8. */
  paredes_mask: number
  retomada: boolean
}

export interface Falha {
  motivo: MotivoFalha
  origem: OrigemFalha
  celula_x: number
  celula_y: number
  componente: ComponenteHealthCheck | null
  observacao: string | null
  momento_falha: string
}

export interface Tentativa {
  tentativa_id: string
  attempt_index: number
  status: StatusTentativa
  tipo_inicio: TipoInicio
  tipo_descoberto: TipoLabirinto | null
  iniciada_em: string
  encerrada_em: string | null
  tempo_s: number | null
  velocidade_media: number | null
  bateria_inicial: number | null
  bateria_final: number | null
  consumo_bateria: number | null
  health_check: ItemHealthCheck[]
  falha: Falha | null
}

export interface Recusa {
  motivo: MotivoRecusa
  ocorrida_em: string
}

/** Execução lógica com tudo o que as telas de Início e Detalhe exibem. */
export interface Execucao {
  execucao_id: string
  numero: number
  labirinto: TipoLabirinto
  status: StatusExecucao
  tentativas_usadas: number
  iniciada_em: string
  encerrada_em: string | null
  tempo_total_s: number | null
  tentativas: Tentativa[]
  trajeto: PassoTrajeto[]
  leituras: LeituraTelemetria[]
  ultima_mensagem_em: string | null
}

/** Linha do histórico (GET /execucoes). */
export interface ResumoExecucao {
  execucao_id: string
  numero: number
  labirinto: TipoLabirinto
  status: StatusExecucao
  resultado: 'success' | 'failed' | null
  tentativas_usadas: number
  iniciada_em: string
  tempo_total_s: number | null
  velocidade_media: number | null
  consumo_bateria: number | null
}

/** Resposta de GET /execucoes/atual. */
export interface ExecucaoAtualResposta {
  /** Execução lógica mais recente, em andamento ou recém-encerrada; null se não houver. */
  execucao: Execucao | null
  ultima: ResumoExecucao | null
  /** Última recusa de abertura de tentativa (RF19), se houver. */
  recusa: Recusa | null
  proximo_numero: number
}

export interface NovaExecucaoPedido {
  labirinto: TipoLabirinto
}

export interface EncerrarPedido {
  motivo: MotivoEncerramento
  observacao: string | null
}

/** Eventos do stream SSE da execução (GET /execucoes/{id}/stream). */
export type EventoExecucao =
  | { tipo: 'leitura'; leitura: LeituraTelemetria }
  | { tipo: 'passo'; passo: PassoTrajeto }
  | { tipo: 'tentativa'; tentativa: Tentativa }
