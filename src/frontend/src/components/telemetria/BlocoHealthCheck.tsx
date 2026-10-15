import type { ItemHealthCheck } from '../../api/tipos'
import { formatarNumero, ROTULO_COMPONENTE } from '../../utils/formatacao'

interface BlocoHealthCheckProps {
  itens: ItemHealthCheck[]
}

/** Dados do health-check em bloco próprio, separados do trajeto (RF05). */
export function BlocoHealthCheck({ itens }: BlocoHealthCheckProps) {
  const aprovados = itens.filter((i) => i.aprovado).length
  return (
    <section className="cartao">
      <div className="cartao__cabecalho">
        <h2 className="cartao__titulo">Health-check</h2>
        <span className="mono texto-suave">
          {itens.length > 0 ? `${aprovados} de ${itens.length} OK` : 'Aguardando'}
        </span>
      </div>
      {itens.length === 0 ? <p className="texto-suave">Aguardando os dados do health-check enviados pelo robô.</p> : <ul className="health-check">
        {itens.map((item) => (
          <li key={item.componente} className={`health-check__item health-check__item--${situacao(item)}`}>
            <span>{ROTULO_COMPONENTE[item.componente]}</span>
            <span className="mono">
              {item.valor_lido !== null && `${formatarNumero(item.valor_lido, 2)} V · `}
              {item.aprovado === null ? 'verificando' : item.aprovado ? 'OK' : 'falhou'}
            </span>
          </li>
        ))}
      </ul>}
    </section>
  )
}

function situacao(item: ItemHealthCheck): string {
  if (item.aprovado === null) return 'pendente'
  return item.aprovado ? 'ok' : 'falha'
}
