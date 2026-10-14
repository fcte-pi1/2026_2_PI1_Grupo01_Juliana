import { LIMITE_TENTATIVAS } from '../../utils/formatacao'

interface BotaoRetomarProps {
  /** attempt_index da tentativa que será aberta (2 ou 3). */
  proxima: number
  aoClicar: () => void
}

/** "Retomar tentativa (n/3)": só aparece com a última tentativa failed e tentativas sobrando (US07). */
export function BotaoRetomar({ proxima, aoClicar }: BotaoRetomarProps) {
  return (
    <button type="button" className="botao botao--retomada" onClick={aoClicar}>
      ↻ Retomar tentativa ({proxima}/{LIMITE_TENTATIVAS})
    </button>
  )
}
