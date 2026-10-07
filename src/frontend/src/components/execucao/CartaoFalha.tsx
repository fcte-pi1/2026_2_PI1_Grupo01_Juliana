import type { Falha } from '../../api/tipos'
import { formatarHora, nomeCelula, ROTULO_COMPONENTE, ROTULO_MOTIVO_FALHA } from '../../utils/formatacao'

interface CartaoFalhaProps {
  falha: Falha
  attemptIndex: number
}

/** Registro da falha de uma tentativa: célula, motivo e momento (RF15). */
export function CartaoFalha({ falha, attemptIndex }: CartaoFalhaProps) {
  return (
    <section className="cartao cartao--perigo">
      <div className="cartao__cabecalho">
        <h2 className="cartao__titulo">Registro da falha · tentativa {attemptIndex}</h2>
        <span className="mono">{falha.motivo}</span>
      </div>
      <dl className="mini-metricas">
        <div>
          <dt>Célula</dt>
          <dd className="mono">{nomeCelula({ x: falha.celula_x, y: falha.celula_y })}</dd>
        </div>
        <div>
          <dt>Motivo</dt>
          <dd>{ROTULO_MOTIVO_FALHA[falha.motivo]}</dd>
        </div>
        <div>
          <dt>Momento</dt>
          <dd className="mono">{formatarHora(falha.momento_falha)}</dd>
        </div>
      </dl>
      <p className="texto-suave">
        {falha.origem === 'encerrado_operador' ? 'Encerrada pelo operador.' : 'Detectada automaticamente.'}
        {falha.componente && ` Componente: ${ROTULO_COMPONENTE[falha.componente]}.`}
        {falha.observacao && ` Observação: ${falha.observacao}`}
      </p>
    </section>
  )
}
