import type { ReactNode } from 'react'
import type { Execucao, Tentativa } from '../../api/tipos'
import { formatarMedicao, formatarTempo, LIMITE_EXECUCAO_S, nomeCelula } from '../../utils/formatacao'
import { CartaoMetrica } from './CartaoMetrica'

interface MetricasTempoRealProps {
  execucao: Execucao
  tentativa: Tentativa | null
  agora: number
  compacta?: boolean
  children?: ReactNode
}

export function MetricasTempoReal({ execucao, tentativa, agora, compacta = false, children }: MetricasTempoRealProps) {
  const ultima = execucao.leituras.at(-1)
  const celula = execucao.celula_atual ?? ultima ?? execucao.trajeto.at(-1)
  const execucaoAberta = execucao.status === 'em_andamento'
  const tentativaAberta = execucaoAberta && tentativa?.status === 'running'
  const tempoExecucao = tempoDecorrido(execucao.iniciada_em, execucao.tempo_total_s, execucaoAberta, agora)
  const tempoTentativa = tentativa
    ? tempoDecorrido(tentativa.iniciada_em, tentativa.tempo_s, tentativaAberta, agora)
    : null
  const bateria = !tentativaAberta && tentativa?.bateria_final != null
    ? tentativa.bateria_final
    : ultima?.bateria ?? tentativa?.health_check.find((item) => item.componente === 'bateria')?.valor_lido
  const percentual = ultima?.bateria === bateria ? ultima?.bateria_pct : null
  const visitadas = new Set(execucao.trajeto.map((passo) => `${passo.x},${passo.y}`)).size

  return (
    <div className={`metricas-tempo-real${compacta ? ' metricas-tempo-real--compactas' : ''}`} aria-label="Dados da execução em tempo real">
      <section className="cartao tempo tempo--principal">
        <div>
          <span className="rotulo">Tempo da tentativa</span>
          <p className="tempo__cronometro mono">
            {tempoTentativa === null ? '—' : formatarTempo(tempoTentativa)}<span className="metrica__unidade">min:s</span>
          </p>
          <p className="texto-suave">{tentativaAberta ? 'desde o início desta tentativa' : 'tempo final informado pelo servidor'}</p>
        </div>
        <div className="tempo__limite">
          <section className="tempo__total">
            <span className="rotulo">Tempo da execução</span>
            <p className="mono">{tempoExecucao === null ? '—' : formatarTempo(tempoExecucao)}<span className="metrica__unidade">min:s</span></p>
          </section>
          <div className="tempo__linha">
            <span className="rotulo">Limite de 10 min</span>
            <span className="texto-suave">restam <strong className="mono">{tempoExecucao === null ? '—' : formatarTempo(Math.max(LIMITE_EXECUCAO_S - tempoExecucao, 0))}</strong></span>
          </div>
          <div className="barra"><div className="barra__preenchida" style={{ width: `${tempoExecucao === null ? 0 : Math.min(tempoExecucao / LIMITE_EXECUCAO_S, 1) * 100}%` }} /></div>
          <p className="texto-suave">10 min por execução, incluindo intervalos.</p>
        </div>
      </section>
      <CartaoMetrica
        rotulo="Velocidade"
        valor={ultima?.velocidade == null ? '—' : formatarMedicao(ultima.velocidade)}
        unidade="mm/s"
        detalhe={`Velocidade média: ${tentativa?.velocidade_media == null ? '—' : `${formatarMedicao(tentativa.velocidade_media)} m/s`}`}
      />
      <CartaoMetrica
        rotulo="Bateria"
        valor={bateria == null ? '—' : formatarMedicao(bateria)}
        unidade="mV"
        detalhe={percentual != null ? `${formatarMedicao(percentual)} % de carga` : 'Percentual pendente'}
        progresso={percentual ?? undefined}
      />
      <CartaoMetrica
        rotulo="Célula atual"
        valor={celula ? nomeCelula(celula) : '—'}
        detalhe={`${visitadas} de ${Number(execucao.labirinto.split('x')[0]) * Number(execucao.labirinto.split('x')[1])} células visitadas`}
      />
      {children}
    </div>
  )
}

function tempoDecorrido(iniciadaEm: string, tempoFinal: number | null, aberta: boolean, agora: number): number | null {
  if (!aberta) return tempoFinal
  const inicio = Date.parse(iniciadaEm)
  if (!Number.isFinite(inicio) || !Number.isFinite(agora)) return null
  return Math.max((agora - inicio) / 1000, 0)
}
