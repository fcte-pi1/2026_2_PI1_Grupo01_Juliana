import type { MotivoRecusa } from '../../api/tipos'
import { ROTULO_RECUSA } from '../../utils/formatacao'

interface AvisoRecusaProps {
  motivo: MotivoRecusa
  titulo?: string
  aoFechar?: () => void
}

/** Aviso de recusa de abertura de tentativa (RF19). */
export function AvisoRecusa({ motivo, titulo = 'Tentativa recusada', aoFechar }: AvisoRecusaProps) {
  return (
    <section className="aviso aviso--alerta" role="alert">
      <span className="aviso__icone" aria-hidden="true">
        !
      </span>
      <div>
        <strong>{titulo}</strong>
        <p>
          {ROTULO_RECUSA[motivo]}{' '}
          Nenhuma nova tentativa foi registrada.
        </p>
      </div>
      {aoFechar && (
        <button type="button" className="botao" onClick={aoFechar}>
          Entendi
        </button>
      )}
    </section>
  )
}
