import type { Execucao } from '../../api/tipos'
import { podeRetomar, tentativaAberta, tentativaAtual } from '../../utils/tentativas'
import { BotaoRetomar } from './BotaoRetomar'

interface ControleTentativasProps {
  execucao: Execucao
  enviando?: boolean
  aoEncerrar: () => void
  aoRetomar: () => void
}

/** Controles operacionais da US07: o estado persistido define a ação disponível. */
export function ControleTentativas({ execucao, enviando = false, aoEncerrar, aoRetomar }: ControleTentativasProps) {
  if (tentativaAberta(execucao)) {
    return <button type="button" className="botao botao--perigo-contorno" disabled={enviando} onClick={aoEncerrar}>
      Encerrar tentativa
    </button>
  }
  if (podeRetomar(execucao)) {
    return <BotaoRetomar proxima={tentativaAtual(execucao)!.attempt_index + 1} aoClicar={aoRetomar} desabilitado={enviando} />
  }
  return null
}
