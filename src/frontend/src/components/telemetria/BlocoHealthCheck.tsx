import type { ComponenteHealthCheck, ItemHealthCheck } from '../../api/tipos'
import { formatarMedicao, ROTULO_COMPONENTE } from '../../utils/formatacao'

interface BlocoHealthCheckProps {
  itens: ItemHealthCheck[]
}

const COMPONENTES = Object.keys(ROTULO_COMPONENTE) as ComponenteHealthCheck[]

/** Dados do health-check em bloco próprio, separados do trajeto (RF05). */
export function BlocoHealthCheck({ itens }: BlocoHealthCheckProps) {
  const aprovados = itens.filter((i) => i.aprovado).length
  const recebeuResultados = itens.some((i) => i.aprovado !== null)
  return (
    <section className="cartao">
      <div className="cartao__cabecalho">
        <h2 className="cartao__titulo">Health-check</h2>
        <span className="mono texto-suave">
          {recebeuResultados ? `${aprovados} de ${COMPONENTES.length} OK` : 'Aguardando'}
        </span>
      </div>
      {!recebeuResultados && <p className="texto-suave">Aguardando os dados do health-check enviados pelo robô.</p>}
      <ul className="health-check">
        {COMPONENTES.map((componente) => {
          const item = itens.find((i) => i.componente === componente)
          return (
          <li key={componente} className={`health-check__item health-check__item--${situacao(item)}`}>
            <span>{ROTULO_COMPONENTE[componente]}</span>
            <span className="mono">
              {item?.valor_lido != null && `${formatarMedicao(item.valor_lido)}${componente === 'bateria' ? ' V' : ''} · `}
              {!item || item.aprovado === null ? 'pendente' : item.aprovado ? 'OK' : 'falhou'}
            </span>
          </li>
          )
        })}
      </ul>
    </section>
  )
}

function situacao(item: ItemHealthCheck | undefined): string {
  if (!item || item.aprovado === null) return 'pendente'
  return item.aprovado ? 'ok' : 'falha'
}
