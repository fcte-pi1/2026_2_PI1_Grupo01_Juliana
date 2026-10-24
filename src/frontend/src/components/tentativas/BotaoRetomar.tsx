import { LIMITE_TENTATIVAS } from '../../utils/formatacao'

interface BotaoRetomarProps {
  /** attempt_index da tentativa que será aberta (2 ou 3). */
  proxima: number
  aoClicar: () => void
  desabilitado?: boolean
}

/** "Retomar tentativa (n/3)": só aparece com a última tentativa failed e tentativas sobrando (US07). */
export function BotaoRetomar({ proxima, aoClicar, desabilitado = false }: BotaoRetomarProps) {
  return (
    <button type="button" className="botao botao--retomada" disabled={desabilitado} onClick={aoClicar}>
      ↻ Retomar tentativa ({proxima}/{LIMITE_TENTATIVAS})
    </button>
  )
}
