import type { Execucao, Tentativa } from '../../api/tipos'
import { formatarMedicao, formatarTempo, nomeCelula } from '../../utils/formatacao'
import { CartaoMetrica } from './CartaoMetrica'

interface MetricasTempoRealProps {
  execucao: Execucao
  tentativa: Tentativa | null
  agora: number
}

export function MetricasTempoReal({ execucao, tentativa, agora }: MetricasTempoRealProps) {
  const ultima = execucao.leituras.at(-1)
  const celula = execucao.celula_atual ?? ultima ?? execucao.trajeto.at(-1)
  const execucaoAberta = execucao.status === 'em_andamento'
  const tentativaAberta = execucaoAberta && (tentativa?.status === 'health-check' || tentativa?.status === 'running')
  const tempoExecucao = tempoDecorrido(execucao.iniciada_em, execucao.encerrada_em, execucao.tempo_total_s, execucaoAberta, agora)
  const tempoTentativa = tentativa
    ? tempoDecorrido(tentativa.iniciada_em, tentativa.encerrada_em, tentativa.tempo_s, tentativaAberta, agora)
    : null
  const bateria = !tentativaAberta && tentativa?.bateria_final != null
    ? tentativa.bateria_final
    : ultima?.bateria ?? tentativa?.health_check.find((item) => item.componente === 'bateria')?.valor_lido

  return (
    <div className="metricas-tempo-real" aria-label="Dados da execução em tempo real">
      <CartaoMetrica
        rotulo="Tempo da tentativa"
        valor={tempoTentativa === null ? '—' : formatarTempo(tempoTentativa)}
        unidade="min:s"
        detalhe={tentativaAberta ? 'desde o início desta tentativa' : 'tempo final informado pelo servidor'}
      />
      <CartaoMetrica
        rotulo="Tempo da execução"
        valor={tempoExecucao === null ? '—' : formatarTempo(tempoExecucao)}
        unidade="min:s"
        detalhe={execucaoAberta ? 'desde Nova execução, incluindo intervalos' : 'tempo final informado pelo servidor'}
      />
      <CartaoMetrica rotulo="Célula atual" valor={celula ? nomeCelula(celula) : '—'} detalhe="última célula informada pelo robô" />
      <CartaoMetrica rotulo="Bateria" valor={bateria == null ? '—' : formatarMedicao(bateria)} unidade="V" detalhe="tensão informada pelo robô" />
      <CartaoMetrica
        rotulo="Velocidade"
        valor={ultima?.velocidade == null ? '—' : formatarMedicao(ultima.velocidade)}
        unidade="m/s"
        detalhe={`Velocidade média: ${tentativa?.velocidade_media == null ? '—' : `${formatarMedicao(tentativa.velocidade_media)} m/s`}`}
      />
    </div>
  )
}

function tempoDecorrido(iniciadaEm: string, encerradaEm: string | null, tempoFinal: number | null, aberta: boolean, agora: number): number | null {
  if (!aberta && tempoFinal !== null) return tempoFinal
  const inicio = Date.parse(iniciadaEm)
  const fim = aberta ? agora : encerradaEm ? Date.parse(encerradaEm) : null
  if (!Number.isFinite(inicio) || fim === null || !Number.isFinite(fim)) return null
  return Math.max((fim - inicio) / 1000, 0)
}
