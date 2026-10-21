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
  const [motivo, setMotivo] = useState<MotivoEncerramento | ''>('')
  const [observacao, setObservacao] = useState('')

  return (
    <Modal
      titulo={`Encerrar a tentativa ${attemptIndex} da execução ${numeroExecucao}?`}
      aoFechar={() => { if (!enviando) aoCancelar() }}
      rodape={
        <>
          <button type="button" className="botao" disabled={enviando} onClick={aoCancelar}>
            Cancelar
          </button>
          <button
            type="button"
            className="botao botao--perigo"
            disabled={enviando || !motivo}
            onClick={() => { if (!enviando && motivo) aoConfirmar({ motivo, observacao: observacao.trim() || null }) }}
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
      <fieldset className="opcoes" disabled={enviando}>
        <legend>Motivo da falha (obrigatório)</legend>
        {MOTIVOS.map((m) => (
          <label key={m} className={`opcao ${motivo === m ? 'opcao--marcada' : ''}`}>
            <input type="radio" name="motivo" value={m} checked={motivo === m} required onChange={() => setMotivo(m)} />
            <span>{ROTULO_MOTIVO_FALHA[m]}</span>
            <span className="mono texto-suave">{m}</span>
          </label>
        ))}
      </fieldset>
      <label className="campo">
        <span>Observação (opcional)</span>
        <textarea
          maxLength={100}
          disabled={enviando}
          rows={2}
          placeholder="Ex.: roda direita patinou na curva"
          value={observacao}
          onChange={(e) => setObservacao(e.target.value.slice(0, 100))}
        />
      </label>
    </Modal>
  )
}
