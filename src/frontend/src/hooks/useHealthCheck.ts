import { useEffect, useRef, useState } from 'react'
import { api, ErroApi } from '../api/clienteApi'
import { buscarUltimoHealthCheck, solicitarHealthCheck } from '../api/endpoints'
import type { TipoLabirinto, UltimoHealthCheck } from '../api/tipos'

/** UC20: consulta o pré-requisito do robô, sem abrir uma tentativa ou aprovar itens localmente. */
export function useHealthCheck(tipo: TipoLabirinto, habilitado: boolean) {
  const [dados, setDados] = useState<UltimoHealthCheck | null>(null)
  const [erro, setErro] = useState<string | null>(null)
  const configuracao = `${tipo}:${habilitado}`
  const [consultada, setConsultada] = useState<string | null>(null)
  const [solicitando, setSolicitando] = useState(false)
  const [aguardando, setAguardando] = useState<string | null>(null)
  const [agora, setAgora] = useState(api.agora())
  const envio = useRef(false)

  useEffect(() => {
    const controle = new AbortController()
    let proxima: ReturnType<typeof setTimeout>
    if (!habilitado) return () => controle.abort()
    async function consultar() {
      try {
        const resultado = await buscarUltimoHealthCheck(controle.signal)
        if (controle.signal.aborted) return
        setDados(resultado)
        setErro(null)
        setAgora(api.agora())
        setAguardando((pedido) => pedido && resultado && Date.parse(resultado.iniciado_em) >= Date.parse(pedido) ? null : pedido)
      } catch {
        if (!controle.signal.aborted) setErro('Não foi possível consultar o health-check. Aguardando comunicação com o servidor.')
      } finally {
        if (!controle.signal.aborted) {
          setConsultada(configuracao)
          proxima = setTimeout(() => { void consultar() }, 1_000)
        }
      }
    }
    void consultar()
    return () => { controle.abort(); clearTimeout(proxima) }
  }, [habilitado, configuracao])

  async function solicitar() {
    if (envio.current || !habilitado) return
    envio.current = true
    setSolicitando(true)
    try {
      const pedido = await solicitarHealthCheck()
      // A solicitação é global ao robô; trocar o labirinto não invalida seu retorno.
      setAguardando(pedido.solicitado_em)
      setErro(null)
    } catch (e) {
      const detalhe = e instanceof ErroApi && e.corpo && typeof e.corpo === 'object' && 'detail' in e.corpo ? e.corpo.detail : null
      setErro(typeof detalhe === 'string' ? detalhe : 'Não foi possível solicitar o health-check. Tente novamente.')
    } finally {
      envio.current = false
      setSolicitando(false)
    }
  }

  const verificando = !!aguardando || (!!dados && dados.aprovado == null)
  const carregando = consultada !== configuracao
  const valido = dados?.valido_ate != null && Date.parse(dados.valido_ate) >= agora
  const apto = habilitado && !carregando && !erro && !solicitando && !verificando
    && dados?.aprovado === true && valido && dados.tipo_dip === tipo
  // Não apresenta a aprovação anterior como se fosse o resultado do pedido novo.
  const resultadoAtual = aguardando && (!dados || Date.parse(dados.iniciado_em) < Date.parse(aguardando)) ? null : dados
  return { dados: resultadoAtual, erro, carregando, solicitando, verificando, valido, apto, solicitar }
}
