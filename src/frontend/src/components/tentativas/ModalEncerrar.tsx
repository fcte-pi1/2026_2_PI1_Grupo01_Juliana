import { useState } from 'react'
import type { EncerrarPedido, MotivoEncerramento } from '../../api/tipos'
import { ROTULO_MOTIVO_FALHA } from '../../utils/formatacao'
import { Modal } from './Modal'

const MOTIVOS: MotivoEncerramento[] = ['collision', 'stuck', 'out_of_track']

interface ModalEncerrarProps {
  numeroExecucao: string
  attemptIndex: number
  ultimaCelula: string
  tempo: string
  enviando?: boolean
  aoCancelar: () => void
  aoConfirmar: (pedido: EncerrarPedido) => void
}

/** Encerrar tentativa pela web (RF30): registra a falha e envia só a interrupção. */
export function ModalEncerrar({ numeroExecucao, attemptIndex, ultimaCelula, tempo, enviando, aoCancelar, aoConfirmar }: ModalEncerrarProps) {
  const [motivo, setMotivo] = useState<MotivoEncerramento>('stuck')
  const [observacao, setObservacao] = useState('')

  return (
    <Modal
      titulo={`Encerrar a tentativa ${attemptIndex} da execução ${numeroExecucao}?`}
      aoFechar={aoCancelar}
      rodape={
        <>
          <button type="button" className="botao" onClick={aoCancelar}>
            Cancelar
          </button>
          <button
            type="button"
            className="botao botao--perigo"
            disabled={enviando}
            onClick={() => aoConfirmar({ motivo, observacao: observacao.trim() || null })}
          >
            Encerrar como falha
          </button>
        </>
      }
    >
      <p>
        Ela será registrada como <strong className="texto-perigo">Falha</strong> com o motivo escolhido. O sistema envia ao robô
        apenas o comando de interrupção; se ele continuar andando, pare-o pelo botão BOOT.
      </p>
      <div className="chips">
        <span className="chip">Última célula {ultimaCelula}</span>
        <span className="chip">Tempo {tempo}</span>
      </div>
      <fieldset className="opcoes">
        <legend>Motivo da falha</legend>
        {MOTIVOS.map((m) => (
          <label key={m} className={`opcao ${motivo === m ? 'opcao--marcada' : ''}`}>
            <input type="radio" name="motivo" value={m} checked={motivo === m} onChange={() => setMotivo(m)} />
            <span>{ROTULO_MOTIVO_FALHA[m]}</span>
            <span className="mono texto-suave">{m}</span>
          </label>
        ))}
      </fieldset>
      <label className="campo">
        <span>Observação (opcional)</span>
        <textarea
          maxLength={100}
          rows={2}
          placeholder="Ex.: roda direita patinou na curva"
          value={observacao}
          onChange={(e) => setObservacao(e.target.value)}
        />
      </label>
    </Modal>
  )
}
