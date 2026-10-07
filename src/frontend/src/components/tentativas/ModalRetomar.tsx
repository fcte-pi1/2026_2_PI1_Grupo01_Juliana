import { useState } from 'react'
import { LIMITE_TENTATIVAS } from '../../utils/formatacao'
import { Modal } from './Modal'

interface ModalRetomarProps {
  numeroExecucao: string
  /** attempt_index da tentativa que será aberta (2 ou 3). */
  proxima: number
  celulaFalha: string
  enviando?: boolean
  aoCancelar: () => void
  aoConfirmar: () => void
}

/** Retomar tentativa (n/3) a partir da célula da falha, com o mapa preservado (RF38). */
export function ModalRetomar({ numeroExecucao, proxima, celulaFalha, enviando, aoCancelar, aoConfirmar }: ModalRetomarProps) {
  const [posicionado, setPosicionado] = useState(false)
  const [bateriaOk, setBateriaOk] = useState(false)

  return (
    <Modal
      titulo={`Retomar a execução ${numeroExecucao}?`}
      aoFechar={aoCancelar}
      rodape={
        <>
          <button type="button" className="botao" onClick={aoCancelar}>
            Cancelar
          </button>
          <button
            type="button"
            className="botao botao--retomada"
            disabled={enviando || !posicionado || !bateriaOk}
            onClick={aoConfirmar}
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
        tempo continua contando no limite de 10 min do labirinto.
      </p>
      <div className="chips">
        <span className="chip">Célula da falha {celulaFalha}</span>
        <span className="chip">10 min por labirinto</span>
      </div>
      <fieldset className="opcoes">
        <legend>Antes de retomar</legend>
        <label className="opcao">
          <input type="checkbox" checked={posicionado} onChange={(e) => setPosicionado(e.target.checked)} />
          <span>Robô posicionado em {celulaFalha}</span>
        </label>
        <label className="opcao">
          <input type="checkbox" checked={bateriaOk} onChange={(e) => setBateriaOk(e.target.checked)} />
          <span>Bateria acima do nível de alerta (7,4 V na LiPo)</span>
        </label>
      </fieldset>
      <p className="texto-suave">
        A retomada começa quando o robô enviar a primeira mensagem a partir de {celulaFalha}. Nenhum comando é enviado ao robô.
      </p>
    </Modal>
  )
}
