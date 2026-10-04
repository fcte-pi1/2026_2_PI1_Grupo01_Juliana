import type { MotivoRecusa } from '../../api/tipos'
import { ROTULO_RECUSA } from '../../utils/formatacao'

interface AvisoRecusaProps {
  motivo: MotivoRecusa
  numeroExecucao: string
  aoFechar?: () => void
}

/** Aviso de recusa de abertura de tentativa (RF19). */
export function AvisoRecusa({ motivo, numeroExecucao, aoFechar }: AvisoRecusaProps) {
  return (
    <section className="aviso aviso--alerta" role="alert">
      <span className="aviso__icone" aria-hidden="true">
        !
      </span>
      <div>
        <strong>Tentativa recusada</strong>
        <p>
          {ROTULO_RECUSA[motivo]} O pedido ficou registrado no histórico da execução {numeroExecucao} como rejeitado.
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
