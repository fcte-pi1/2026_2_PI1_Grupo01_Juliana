import type { StatusTentativa } from '../../api/tipos'

interface EtapasExecucaoProps {
  status: StatusTentativa
  healthCheckAprovados: number
  healthCheckTotal: number
}

type Situacao = 'feita' | 'atual' | 'pendente' | 'falha'

/** Linha do tempo da tentativa: health-check → em execução → sucesso ou falha. */
export function EtapasExecucao({ status, healthCheckAprovados, healthCheckTotal }: EtapasExecucaoProps) {
  const etapas: Array<{ titulo: string; detalhe: string; situacao: Situacao }> = [
    {
      titulo: 'Health-check',
      detalhe: status === 'health-check' ? `verificando · ${healthCheckAprovados} de ${healthCheckTotal}` : `aprovado · ${healthCheckAprovados} de ${healthCheckTotal} OK`,
      situacao: status === 'health-check' ? 'atual' : 'feita',
    },
    {
      titulo: 'Em execução',
      detalhe: 'o robô percorre o labirinto sozinho',
      situacao: status === 'running' ? 'atual' : status === 'health-check' ? 'pendente' : 'feita',
    },
    {
      titulo: status === 'success' ? 'Sucesso' : status === 'failed' ? 'Falha' : 'Sucesso ou falha',
      detalhe: status === 'success' ? 'chegou ao objetivo' : status === 'failed' ? 'tentativa encerrada' : 'aí o trajeto aparece no mapa',
      situacao: status === 'success' ? 'feita' : status === 'failed' ? 'falha' : 'pendente',
    },
  ]

  return (
    <ol className="cartao etapas">
      {etapas.map((etapa, i) => (
        <li key={etapa.titulo} className={`etapas__item etapas__item--${etapa.situacao}`}>
          <span className="etapas__marcador" aria-hidden="true">
            {etapa.situacao === 'feita' ? '✓' : etapa.situacao === 'falha' ? '✕' : i + 1}
          </span>
          <div>
            <strong>{etapa.titulo}</strong>
            <p>{etapa.detalhe}</p>
          </div>
        </li>
      ))}
    </ol>
  )
}
