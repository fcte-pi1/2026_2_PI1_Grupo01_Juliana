import { ALERTA_SEM_DADOS_S } from '../../utils/formatacao'

interface UltimaAtualizacaoProps {
  /** Segundos desde a última mensagem do robô; null se nenhuma chegou ainda. */
  segundos: number | null
}

/** Pílula "Atualizado há X s" (RF04); fica vermelha sem dados por mais de 5 s. */
export function UltimaAtualizacao({ segundos }: UltimaAtualizacaoProps) {
  if (segundos === null) return <span className="pilula">Aguardando dados</span>
  const semDados = segundos > ALERTA_SEM_DADOS_S
  return (
    <span className={`pilula mono ${semDados ? 'pilula--perigo' : 'pilula--sucesso'}`}>
      {semDados ? 'Sem dados há' : 'Atualizado há'} {Math.floor(segundos)} s
    </span>
  )
}
