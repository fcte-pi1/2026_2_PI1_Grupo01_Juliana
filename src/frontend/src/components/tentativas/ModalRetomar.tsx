import { useState, type ReactNode } from 'react'
import { LIMITE_TENTATIVAS } from '../../utils/formatacao'
import { Modal } from './Modal'

interface ModalRetomarProps {
  numeroExecucao: string
  /** attempt_index da tentativa que será aberta (2 ou 3). */
  proxima: number
  celulaFalha: string
  enviando?: boolean
  liberado: boolean
  /** Só a bateria reprovou: a escolha de continuar passa pelo aviso de bateria baixa. */
  bateriaBaixa?: boolean
  verificacao?: ReactNode
  aoCancelar: () => void
  aoConfirmar: () => void
}

/** Retomar tentativa (n/3) a partir da célula da falha, com o mapa preservado (RF38). */
export function ModalRetomar({ numeroExecucao, proxima, celulaFalha, enviando, liberado, bateriaBaixa = false, verificacao, aoCancelar, aoConfirmar }: ModalRetomarProps) {
  const [posicionado, setPosicionado] = useState(false)
  const [bateriaOk, setBateriaOk] = useState(false)

  return (
    <Modal
      titulo={`Retomar a execução ${numeroExecucao}?`}
      aoFechar={() => { if (!enviando) aoCancelar() }}
      rodape={
        <>
          <button type="button" className="botao" disabled={enviando} onClick={aoCancelar}>
            Cancelar
          </button>
          <button
            type="button"
            className="botao botao--retomada"
            disabled={enviando || !liberado || !posicionado || !(bateriaOk || bateriaBaixa)}
            onClick={() => { if (!enviando && liberado && posicionado && (bateriaOk || bateriaBaixa)) aoConfirmar() }}
          >
            Retomar da célula {celulaFalha}
          </button>
        </>
      }
    >
      <p>
        A{' '}
        <strong className="texto-retomada">
          tentativa {proxima} de {LIMITE_TENTATIVAS}
        </strong>{' '}
        começa na célula <strong>{celulaFalha}</strong>, onde a tentativa anterior falhou. O trajeto já gravado é mantido e o
        tempo continua contando no limite de 10 min da execução.
      </p>
      <div className="chips">
        <span className="chip">Célula da falha {celulaFalha}</span>
        <span className="chip">10 min por execução</span>
      </div>
      <fieldset className="opcoes" disabled={enviando}>
        <legend>Antes de retomar</legend>
        <label className="opcao">
          <input type="checkbox" checked={posicionado} onChange={(e) => setPosicionado(e.target.checked)} />
          <span>Robô posicionado em {celulaFalha}</span>
        </label>
        {bateriaBaixa
          ? <p className="texto-perigo">A bateria está abaixo do padrão seguro. Ao confirmar, você ainda verá um aviso antes de retomar.</p>
          : (
            <label className="opcao">
              <input type="checkbox" checked={bateriaOk} onChange={(e) => setBateriaOk(e.target.checked)} />
              <span>Bateria acima do nível de alerta (7,4 V na LiPo)</span>
            </label>
          )}
      </fieldset>
      {verificacao}
      <p className="texto-suave">
        Com o health-check aprovado e válido, confirme para o robô continuar a partir de {celulaFalha}.
      </p>
    </Modal>
  )
}
