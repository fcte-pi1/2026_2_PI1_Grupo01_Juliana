import type { StatusExecucao, StatusTentativa } from '../../api/tipos'

interface PilulaResultadoProps {
  resultado: StatusExecucao | null
}

/** Resultado de uma execução lógica no histórico. */
export function PilulaResultado({ resultado }: PilulaResultadoProps) {
  if (resultado === 'em_andamento') return <span className="pilula pilula--primaria">Em andamento</span>
  if (resultado === 'concluida') return <span className="pilula pilula--sucesso">Concluída</span>
  if (resultado === 'cancelada') return <span className="pilula pilula--perigo">Cancelada</span>
  return <span className="pilula">—</span>
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
