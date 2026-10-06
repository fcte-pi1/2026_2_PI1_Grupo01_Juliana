import type { StatusExecucao, StatusTentativa } from '../../api/tipos'

interface PilulaResultadoProps {
  status: StatusExecucao
  resultado: 'success' | 'failed' | null
}

/** Resultado de uma execução lógica no histórico. */
export function PilulaResultado({ status, resultado }: PilulaResultadoProps) {
  if (status === 'em_andamento') return <span className="pilula pilula--primaria">Em andamento</span>
  if (resultado === 'success') return <span className="pilula pilula--sucesso">Sucesso</span>
  return <span className="pilula pilula--perigo">{status === 'cancelada' ? 'Cancelada' : 'Falha'}</span>
}

const STATUS_TENTATIVA: Record<StatusTentativa, { texto: string; classe: string }> = {
  'health-check': { texto: 'Health-check', classe: 'pilula--alerta' },
  running: { texto: 'Em execução', classe: 'pilula--primaria' },
  success: { texto: 'Sucesso', classe: 'pilula--sucesso' },
  failed: { texto: 'Falha', classe: 'pilula--perigo' },
}

export function PilulaStatusTentativa({ status }: { status: StatusTentativa }) {
  const { texto, classe } = STATUS_TENTATIVA[status]
  return <span className={`pilula ${classe}`}>{texto}</span>
}
