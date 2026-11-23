import { BATERIA_CORTE_MV } from '../../utils/formatacao'
import { Modal } from './Modal'

interface ModalBateriaBaixaProps {
  /** Leitura da bateria no health-check, em mV (nula se o robô não a informou). */
  valorMv: number | null
  /** Retomada: o texto fala em continuar a execução, e não em começar uma. */
  retomada?: boolean
  enviando?: boolean
  aoCancelar: () => void
  aoContinuar: () => void
}

/** Aviso antes de largar com a bateria abaixo do corte seguro: o operador decide continuar ou cancelar. */
export function ModalBateriaBaixa({ valorMv, retomada = false, enviando = false, aoCancelar, aoContinuar }: ModalBateriaBaixaProps) {
  return (
    <Modal
      titulo="Bateria abaixo do padrão seguro"
      aoFechar={() => { if (!enviando) aoCancelar() }}
      rodape={
        <>
          <button type="button" className="botao" disabled={enviando} onClick={aoCancelar}>Cancelar</button>
          <button type="button" className="botao botao--perigo" disabled={enviando} onClick={aoContinuar}>
            {enviando ? 'Iniciando…' : 'Continuar mesmo assim'}
          </button>
        </>
      }
    >
      <p>
        O health-check reprovou a bateria
        {valorMv !== null && <> (<strong className="mono">{valorMv} mV</strong>)</>}, abaixo do corte de{' '}
        <strong className="mono">{BATERIA_CORTE_MV} mV</strong> (3,5 V por célula). Todos os outros itens passaram.
      </p>
      <p className="texto-suave">
        {retomada ? 'Retomar' : 'Largar'} assim pode descarregar a LiPo além do limite seguro, danificá-la e fazer o robô perder
        força durante a corrida. O resultado dessa execução não vale como medida de desempenho com a bateria dentro do padrão.
      </p>
      <p>Deseja continuar mesmo assim?</p>
    </Modal>
  )
}
