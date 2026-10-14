import { LIMITE_TENTATIVAS } from '../../utils/formatacao'

interface ContadorTentativasProps {
  /** attempt_index da tentativa atual (1 a 3). */
  atual: number
}

/** Indicador "Tentativa n/3" da execução lógica (RF18, US07). */
export function ContadorTentativas({ atual }: ContadorTentativasProps) {
  return (
    <span className="chip" aria-label={`Tentativa ${atual} de ${LIMITE_TENTATIVAS}`}>
      Tentativa {atual}/{LIMITE_TENTATIVAS}
    </span>
  )
}
