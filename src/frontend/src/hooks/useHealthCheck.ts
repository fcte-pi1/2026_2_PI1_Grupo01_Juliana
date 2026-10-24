import { useEffect, useRef, useState } from 'react'
import { api, ErroApi } from '../api/clienteApi'
import { buscarUltimoHealthCheck, solicitarHealthCheck } from '../api/endpoints'
import type { TipoLabirinto, UltimoHealthCheck } from '../api/tipos'
import { useAgora } from './useAgora'

const INTERVALO_AGUARDANDO_MS = 1_000
const INTERVALO_REPOUSO_MS = 10_000
const LIMITE_ESPERA_MS = 30_000

interface Pedido {
  solicitadoEm: string
  limiteLocal: number
}

function escolher(anterior: UltimoHealthCheck | null, chegando: UltimoHealthCheck | null) {
  if (chegando == null) return anterior
  if (anterior == null) return chegando
  const inicioAnterior = Date.parse(anterior.iniciado_em)
  const inicioChegando = Date.parse(chegando.iniciado_em)
  if (inicioChegando > inicioAnterior) return chegando
  if (inicioChegando < inicioAnterior) return anterior
  if (anterior.concluido_em && !chegando.concluido_em) return anterior
  if (chegando.itens.length < anterior.itens.length && !chegando.concluido_em) return anterior
  return chegando
}

function situacao(dados: UltimoHealthCheck | null, pedido: Pedido | null, agora: number) {
  // Uma aprovação anterior não atende ao pedido, mesmo depois de esgotar a espera.
  const resultado = pedido && (!dados || Date.parse(dados.iniciado_em) < Date.parse(pedido.solicitadoEm)) ? null : dados
  const pendente = (!!pedido && !resultado) || (!!resultado && resultado.aprovado == null)
  const tempoEsgotado = pendente && (pedido
    ? Date.now() >= pedido.limiteLocal
    : agora >= Date.parse(resultado!.iniciado_em) + LIMITE_ESPERA_MS)
  return { resultado, pendente, tempoEsgotado }
}

/** UC20: consulta o pré-requisito do robô, sem abrir uma tentativa ou aprovar itens localmente. */
export function useHealthCheck(tipo: TipoLabirinto, habilitado: boolean) {
  const [dados, setDados] = useState<UltimoHealthCheck | null>(null)
  const [erroConsulta, setErroConsulta] = useState<string | null>(null)
  const [erroSolicitacao, setErroSolicitacao] = useState<string | null>(null)
  const configuracao = `${tipo}:${habilitado}`
  const [consultada, setConsultada] = useState<string | null>(null)
  const [solicitando, setSolicitando] = useState(false)
  const [pedido, setPedido] = useState<Pedido | null>(null)
  const agora = useAgora()
  const envio = useRef(false)

  const dadosRef = useRef<UltimoHealthCheck | null>(null)

  useEffect(() => {
    const controle = new AbortController()
    let proxima: ReturnType<typeof setTimeout>
    let consultaAtual = 0
    if (!habilitado) return () => controle.abort()

    function aplicar(chegando: UltimoHealthCheck | null) {
      const escolhido = escolher(dadosRef.current, chegando)
      dadosRef.current = escolhido
      setDados(escolhido)
      setErroConsulta(null)
      if (pedido && situacao(escolhido, pedido, api.agora()).resultado?.aprovado != null) {
        setPedido((anterior) => anterior === pedido ? null : anterior)
      }
    }

    async function consultar() {
      const id = ++consultaAtual
      const inicio = Date.now()
      // O GET e o relógio de 1 s começam juntos. Sem pendência, a próxima espera 10 s desde esse GET.
      const armar = (ms: number) => {
        if (id !== consultaAtual || controle.signal.aborted) return
        clearTimeout(proxima)
        proxima = setTimeout(() => { void consultar() }, ms)
      }
      armar(INTERVALO_AGUARDANDO_MS)
      try {
        const resultado = await buscarUltimoHealthCheck(controle.signal)
        if (controle.signal.aborted || id !== consultaAtual) return
        aplicar(resultado)
        const atual = situacao(dadosRef.current, pedido, api.agora())
        if (!(atual.pendente && !atual.tempoEsgotado)) {
          armar(Math.max(0, INTERVALO_REPOUSO_MS - (Date.now() - inicio)))
        }
      } catch {
        if (!controle.signal.aborted && id === consultaAtual) {
          setErroConsulta('Não foi possível consultar o health-check. Aguardando comunicação com o servidor.')
        }
      } finally {
        if (!controle.signal.aborted && id === consultaAtual) setConsultada(configuracao)
      }
    }

    const encerrarStream = api.assinarHealthCheck((healthCheck) => {
      if (!controle.signal.aborted) aplicar(healthCheck)
    })
    void consultar()
    return () => {
      consultaAtual += 1
      controle.abort()
      clearTimeout(proxima)
      encerrarStream()
    }
  }, [habilitado, configuracao, pedido])

  async function solicitar() {
    if (envio.current || !habilitado) return
    envio.current = true
    setSolicitando(true)
    setErroSolicitacao(null)
    try {
      const resposta = await solicitarHealthCheck()
      // A solicitação é global ao robô; trocar o labirinto não invalida seu retorno.
      setPedido({ solicitadoEm: resposta.solicitado_em, limiteLocal: Date.now() + LIMITE_ESPERA_MS })
    } catch (e) {
      const detalhe = e instanceof ErroApi && e.corpo && typeof e.corpo === 'object' && 'detail' in e.corpo ? e.corpo.detail : null
      setErroSolicitacao(typeof detalhe === 'string' ? detalhe : 'Não foi possível solicitar o health-check. Tente novamente.')
    } finally {
      envio.current = false
      setSolicitando(false)
    }
  }

  const { resultado, pendente, tempoEsgotado } = situacao(dados, pedido, agora)
  const verificando = pendente && !tempoEsgotado
  const erro = erroSolicitacao ?? (tempoEsgotado
    ? 'O robô não concluiu o health-check em 30 s. Solicite novamente.' : erroConsulta)
  const carregando = consultada !== configuracao
  const valido = resultado?.valido_ate != null && Date.parse(resultado.valido_ate) >= agora
  const apto = habilitado && !carregando && !erro && !solicitando && !verificando
    && resultado?.aprovado === true && valido && resultado.tipo_dip === tipo
  return { dados: resultado, erro, carregando, solicitando, verificando, tempoEsgotado, valido, apto, solicitar }
}
