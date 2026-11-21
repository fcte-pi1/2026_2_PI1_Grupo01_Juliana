import { useCallback, useEffect, useState } from 'react'
import { ErroApi } from '../api/clienteApi'
import { buscarEstadoRobo, pedirReconexao } from '../api/endpoints'
import type { EstadoRobo } from '../api/tipos'
import type { EstadoReceptor } from '../components/layout/StatusReceptor'

const INTERVALO_MS = 3_000
/** O robô parado não envia nada; passado esse tempo o sinal deixa de ser considerado recente. */
export const LIMITE_SINAL_RECENTE_S = 60

type Enlace = EstadoRobo['ponte']

export function estadoDoReceptor(segundosSemSinal: number | null, ponte: Enlace | null = null): EstadoReceptor {
  if (ponte === null && segundosSemSinal === null) return 'desconhecido'
  if (ponte === 'reconectando') return 'reconectando'
  if (segundosSemSinal !== null && segundosSemSinal <= LIMITE_SINAL_RECENTE_S) return 'conectado'
  if (ponte === 'conectada') return 'aberto'
  if (segundosSemSinal !== null) return 'desconectado'
  return ponte === 'desligada' ? 'sem-ponte' : 'desconhecido'
}

export function useEstadoRobo() {
  const [dados, setDados] = useState<EstadoRobo | null>(null)
  const [pedindo, setPedindo] = useState(false)
  const [erro, setErro] = useState<string | null>(null)
  const [versao, setVersao] = useState(0)

  useEffect(() => {
    const controle = new AbortController()
    const consultar = () => {
      buscarEstadoRobo(controle.signal)
        .then((resposta) => { if (!controle.signal.aborted) setDados(resposta) })
        .catch(() => { if (!controle.signal.aborted) setDados(null) })
    }
    consultar()
    const id = setInterval(consultar, INTERVALO_MS)
    return () => { controle.abort(); clearInterval(id) }
  }, [versao])

  const reconectar = useCallback(async () => {
    setPedindo(true)
    setErro(null)
    try {
      await pedirReconexao()
      setVersao((v) => v + 1)
    } catch (e) {
      setErro(e instanceof ErroApi && e.status === 409 ? 'A ponte não está ligada.' : 'Não foi possível pedir a reconexão.')
    } finally {
      setPedindo(false)
    }
  }, [])

  const segundosSemSinal = dados?.segundos_sem_sinal ?? null
  const ponte = dados?.ponte ?? null
  return {
    estado: estadoDoReceptor(segundosSemSinal, ponte),
    segundosSemSinal,
    ponte,
    reconexaoPedida: dados?.reconexao_pedida ?? false,
    pedindo,
    erro,
    reconectar,
  }
}
