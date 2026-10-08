import type { Celula, ComponenteHealthCheck, MotivoFalha, MotivoRecusa } from '../api/tipos'

/** Lado da célula em metros (labirinto de 18 cm). */
export const LADO_CELULA_M = 0.18

/**
 * Limite de tempo por execução lógica (RF32): 10 min corridos desde Nova execução,
 * incluindo os intervalos entre tentativas, para até 3 tentativas.
 */
export const LIMITE_EXECUCAO_S = 600

export const LIMITE_TENTATIVAS = 3

/** Sem mensagem por mais que isso, a tela mostra "Sem comunicação". */
export const ALERTA_SEM_DADOS_S = 5

/** Nome da célula como no protótipo: coluna em letra e linha a partir de 1 (A1 = largada). */
export function nomeCelula({ x, y }: Celula): string {
  return `${String.fromCharCode(65 + x)}${y + 1}`
}

export function formatarNumero(valor: number, casas: number): string {
  return valor.toLocaleString('pt-BR', { minimumFractionDigits: casas, maximumFractionDigits: casas })
}

/** 20,4 s → "00:20,4" */
export function formatarTempo(segundos: number): string {
  const minutos = Math.floor(segundos / 60)
  const resto = segundos - minutos * 60
  return `${String(minutos).padStart(2, '0')}:${formatarNumero(resto, 1).padStart(4, '0')}`
}

export function formatarHora(iso: string): string {
  const data = new Date(iso)
  const decimos = Math.floor(data.getMilliseconds() / 100)
  return `${data.toLocaleTimeString('pt-BR', { hour12: false })},${decimos}`
}

export function formatarDataHora(iso: string): string {
  const data = new Date(iso)
  return `${data.toLocaleDateString('pt-BR')} · ${data.toLocaleTimeString('pt-BR', { hour12: false })}`
}

export function formatarNumeroExecucao(numero: number | undefined, id?: string): string {
  return numero === undefined ? id ?? '—' : `#${String(numero).padStart(4, '0')}`
}

export const ROTULO_MOTIVO_FALHA: Record<MotivoFalha, string> = {
  collision: 'Colisão com parede',
  stuck: 'Robô travado ou parado',
  out_of_track: 'Saiu do labirinto',
  time_exceeded: 'Tempo esgotado',
  falha_componente: 'Falha de componente',
  low_battery: 'Bateria baixa',
  health_check_failed: 'Health-check reprovado',
  link_lost: 'Perda de comunicação',
  encerrado_operador: 'Encerrado pelo operador',
}

export const ROTULO_COMPONENTE: Record<ComponenteHealthCheck, string> = {
  bateria: 'Bateria',
  tof_frontal_esq: 'ToF frontal esquerdo',
  tof_frontal_dir: 'ToF frontal direito',
  tof_esquerdo: 'ToF esquerdo',
  tof_direito: 'ToF direito',
  motor_esquerdo: 'Motor esquerdo',
  motor_direito: 'Motor direito',
  encoder_esquerdo: 'Encoder esquerdo',
  encoder_direito: 'Encoder direito',
}

export const ROTULO_RECUSA: Record<MotivoRecusa, string> = {
  tentativa_aberta: 'Já existe uma tentativa aberta nesta execução.',
  limite_3: 'As 3 tentativas desta execução já foram usadas.',
  execucao_cancelada: 'A execução foi cancelada e não aceita novas tentativas.',
}
