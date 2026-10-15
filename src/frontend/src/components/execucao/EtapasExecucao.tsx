import type { StatusTentativa } from '../../api/tipos'
import { ROTULO_COMPONENTE } from '../../utils/formatacao'

interface EtapasExecucaoProps {
  status: StatusTentativa
  healthCheckAprovados: number
  healthCheckReprovados: number
  healthCheckTotal: number
}

type Situacao = 'feita' | 'atual' | 'pendente' | 'falha'
const TOTAL_COMPONENTES = Object.keys(ROTULO_COMPONENTE).length

/** Linha do tempo da tentativa: health-check → em execução → sucesso ou falha. */
export function EtapasExecucao({ status, healthCheckAprovados, healthCheckReprovados, healthCheckTotal }: EtapasExecucaoProps) {
  const healthCheckReprovado = healthCheckReprovados > 0
  const healthCheckAprovado = healthCheckAprovados === TOTAL_COMPONENTES && !healthCheckReprovado
  const etapas: Array<{ titulo: string; detalhe: string; situacao: Situacao }> = [
    {
      titulo: 'Health-check',
      detalhe: healthCheckReprovado
        ? `reprovado · ${healthCheckReprovados} ${healthCheckReprovados === 1 ? 'componente' : 'componentes'} com falha`
        : healthCheckAprovado
          ? `aprovado · ${healthCheckAprovados} de ${TOTAL_COMPONENTES} OK`
          : healthCheckTotal === 0
            ? 'aguardando dados do health-check'
            : `aguardando resultados · ${healthCheckAprovados} de ${TOTAL_COMPONENTES} OK`,
      situacao: healthCheckReprovado ? 'falha' : healthCheckAprovado ? 'feita' : status === 'health-check' ? 'atual' : 'pendente',
    },
    {
      titulo: 'Em execução',
      detalhe: healthCheckReprovado
        ? 'health-check reprovado'
        : !healthCheckAprovado
          ? 'aguardando resultados do health-check'
          : 'o robô percorre o labirinto sozinho',
      situacao: healthCheckReprovado || status === 'health-check'
        ? 'pendente'
        : status === 'running'
          ? 'atual'
          : healthCheckAprovado ? 'feita' : 'pendente',
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
