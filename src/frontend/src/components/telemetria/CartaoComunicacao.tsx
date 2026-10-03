import { ALERTA_SEM_DADOS_S, formatarHora } from '../../utils/formatacao'

interface CartaoComunicacaoProps {
  ultimaMensagemEm: string | null
}

export function CartaoComunicacao({ ultimaMensagemEm }: CartaoComunicacaoProps) {
  return (
    <section className="cartao">
      <h2 className="cartao__titulo">Comunicação</h2>
      <dl className="lista-dados">
        <div>
          <dt>Última mensagem</dt>
          <dd className="mono">{ultimaMensagemEm ? formatarHora(ultimaMensagemEm) : '—'}</dd>
        </div>
        <div>
          <dt>Alerta de perda</dt>
          <dd className="mono">após {ALERTA_SEM_DADOS_S} s sem dados</dd>
        </div>
        <div>
          <dt>Recebido via</dt>
          <dd>Bluetooth → API</dd>
        </div>
      </dl>
    </section>
  )
}
