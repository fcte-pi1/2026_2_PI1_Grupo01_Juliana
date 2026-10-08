import { Link } from 'react-router'
import type { ResumoExecucao } from '../../api/tipos'
import { formatarDataHora, formatarNumero, formatarNumeroExecucao, formatarTempo, LIMITE_TENTATIVAS } from '../../utils/formatacao'
import { PilulaResultado } from '../execucao/PilulaResultado'

interface TabelaExecucoesProps {
  /** Já na ordem de exibição: da mais recente para a mais antiga (RF03). */
  execucoes: ResumoExecucao[]
}

export function TabelaExecucoes({ execucoes }: TabelaExecucoesProps) {
  if (execucoes.length === 0) return <p className="estado-vazio">Nenhuma execução registrada.</p>

  return (
    <div className="rolagem-horizontal">
      <table className="tabela tabela--sem-destaque">
        <thead>
          <tr>
            <th>Execução</th>
            <th>Labirinto</th>
            <th>Data</th>
            <th>Resultado</th>
            <th>Tentativas</th>
            <th className="alinhar-direita">Tempo</th>
            <th className="alinhar-direita">Velocidade</th>
          </tr>
        </thead>
        <tbody>
          {execucoes.map((e) => (
            <tr key={e.execucao_id}>
              <td>
                <Link to={`/execucoes/${e.execucao_id}`}>{formatarNumeroExecucao(e.numero, e.execucao_id)}</Link>
              </td>
              <td>{e.labirinto}</td>
              <td>{formatarDataHora(e.iniciada_em)}</td>
              <td>
                <PilulaResultado resultado={e.resultado ?? e.status} />
              </td>
              <td>
                {e.tentativas_usadas}/{LIMITE_TENTATIVAS}
              </td>
              <td className="alinhar-direita">{e.tempo_total_s !== null ? formatarTempo(e.tempo_total_s) : '—'}</td>
              <td className="alinhar-direita">{e.velocidade_media !== null ? `${formatarNumero(e.velocidade_media, 3)} m/s` : '—'}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  )
}
