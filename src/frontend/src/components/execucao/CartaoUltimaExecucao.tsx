import { Link } from 'react-router'
import type { ResumoExecucao } from '../../api/tipos'
import { formatarDataHora, formatarNumero, formatarNumeroExecucao, formatarTempo } from '../../utils/formatacao'
import { PilulaResultado } from './PilulaResultado'

interface CartaoUltimaExecucaoProps {
  execucao: ResumoExecucao
}

export function CartaoUltimaExecucao({ execucao }: CartaoUltimaExecucaoProps) {
  return (
    <section className="cartao">
      <span className="rotulo">Última execução · {execucao.labirinto}</span>
      <div className="cartao__cabecalho">
        <strong className="mono texto-grande">{formatarNumeroExecucao(execucao.numero, execucao.execucao_id)}</strong>
        <PilulaResultado resultado={execucao.resultado ?? execucao.status} />
      </div>
      <p className="texto-suave">
        {execucao.tentativas_usadas} tentativa(s) · {formatarDataHora(execucao.iniciada_em)}
      </p>
      <dl className="mini-metricas">
        <div>
          <dt>Tempo</dt>
          <dd className="mono">{execucao.tempo_total_s !== null ? formatarTempo(execucao.tempo_total_s) : '—'}</dd>
        </div>
        <div>
          <dt>Velocidade média</dt>
          <dd className="mono">{execucao.velocidade_media !== null ? `${formatarNumero(execucao.velocidade_media, 3)} m/s` : '—'}</dd>
        </div>
        <div>
          <dt>Bateria usada</dt>
          <dd className="mono">{execucao.consumo_bateria !== null ? `${formatarNumero(execucao.consumo_bateria, 1)} %` : '—'}</dd>
        </div>
      </dl>
      <Link to={`/execucoes/${execucao.execucao_id}`}>Ver trajeto e detalhes →</Link>
    </section>
  )
}
