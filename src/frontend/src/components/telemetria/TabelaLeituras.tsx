import type { LeituraTelemetria } from '../../api/tipos'
import { formatarHora, formatarNumero, nomeCelula } from '../../utils/formatacao'

interface TabelaLeiturasProps {
  leituras: LeituraTelemetria[]
  /** Início da tentativa, para a coluna "desde a largada". */
  largadaEm: string
}

export function TabelaLeituras({ leituras, largadaEm }: TabelaLeiturasProps) {
  const largada = new Date(largadaEm).getTime()
  const recentes = [...leituras].reverse()
  return (
    <section className="cartao">
      <div className="cartao__cabecalho">
        <h2 className="cartao__titulo">Últimas leituras recebidas</h2>
        <span className="texto-suave">o que o robô envia a cada célula, sem arredondar</span>
      </div>
      <div className="rolagem-horizontal">
        <table className="tabela">
          <thead>
            <tr>
              <th>Hora do envio</th>
              <th>Célula</th>
              <th>Bateria</th>
              <th className="alinhar-direita">Desde a largada</th>
            </tr>
          </thead>
          <tbody>
            {recentes.map((leitura) => (
              <tr key={leitura.ordem}>
                <td>{formatarHora(leitura.enviado_em)}</td>
                <td>
                  <strong>{nomeCelula(leitura)}</strong>
                </td>
                <td>{formatarNumero(leitura.bateria, 2)} V</td>
                <td className="alinhar-direita">
                  +{formatarNumero(Math.max((new Date(leitura.enviado_em).getTime() - largada) / 1000, 0), 1)} s
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  )
}
