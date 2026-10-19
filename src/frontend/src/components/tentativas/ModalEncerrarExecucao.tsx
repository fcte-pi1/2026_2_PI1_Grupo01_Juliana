import { Modal } from './Modal'

interface ModalEncerrarExecucaoProps {
  numeroExecucao: string
  attemptIndex: number | null
  tentativaAberta: boolean
  enviando?: boolean
  aoCancelar: () => void
  aoConfirmar: () => void
}

/** Cancela a execução lógica inteira, e não só a tentativa em curso. */
export function ModalEncerrarExecucao({
  numeroExecucao,
  attemptIndex,
  tentativaAberta,
  enviando,
  aoCancelar,
  aoConfirmar,
}: ModalEncerrarExecucaoProps) {
  return (
    <Modal
      titulo={`Encerrar a execução ${numeroExecucao} por completo?`}
      aoFechar={aoCancelar}
      rodape={
        <>
          <button type="button" className="botao" disabled={enviando} onClick={aoCancelar}>
            Cancelar
          </button>
          <button type="button" className="botao botao--perigo" disabled={enviando} onClick={aoConfirmar}>
            {enviando ? 'Encerrando…' : 'Encerrar execução'}
          </button>
        </>
      }
    >
      {tentativaAberta ? (
        <p>
          A tentativa {attemptIndex ?? 'em andamento'} será registrada como{' '}
          <strong className="texto-perigo">Falha</strong> (encerrado pelo operador) e a execução será{' '}
          <strong>cancelada</strong>. Não será possível retomá-la. O sistema envia ao robô apenas o comando de
          interrupção; se ele continuar andando, pare-o pelo botão BOOT.
        </p>
      ) : (
        <p>
          A execução será <strong>cancelada</strong> e não aceitará novas tentativas. O trajeto já gravado permanece
          no histórico.
        </p>
      )}
    </Modal>
  )
}
