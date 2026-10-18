import { useCallback, useEffect, useRef, useState } from 'react'
import { useSearchParams } from 'react-router'
import { api } from '../api/clienteApi'
import { buscarExecucao, buscarExecucaoAtual } from '../api/endpoints'
import { aplicarEvento, combinarRetrato } from '../api/eventosExecucao'
import type { EventoExecucao, ExecucaoAtualResposta } from '../api/tipos'
import { useRequisicao } from './useRequisicao'

/** Restaura o estado pelo REST e acompanha os seis eventos nomeados do contrato SSE. */
export function useExecucaoAtual() {
  const [parametros] = useSearchParams()
  const selecionada = parametros.get('execucao') || null
  const buscar = useCallback(async (sinal: AbortSignal): Promise<ExecucaoAtualResposta> => {
    if (!selecionada) return buscarExecucaoAtual(sinal)
    return { execucao: await buscarExecucao(selecionada, sinal), ultima: null, recusa: null }
  }, [selecionada])
  const requisicao = useRequisicao<ExecucaoAtualResposta>(buscar, selecionada ?? 'atual')
  const { definirDados } = requisicao
  const execucaoId = requisicao.dados?.execucao?.execucao_id
  const dadosRef = useRef(requisicao.dados)
  const sincronizarRef = useRef<() => void>(() => {})
  const [erro, setErro] = useState<{ id: string; mensagem: string } | null>(null)

  useEffect(() => { dadosRef.current = requisicao.dados }, [requisicao.dados])

  useEffect(() => {
    if (!execucaoId) return
    const controle = new AbortController()
    let consultando = false
    let consultarNovamente = false
    let tentativaPendente: string | null = null
    let idsConhecidos = new Set(dadosRef.current?.execucao?.tentativas.map((tentativa) => tentativa.tentativa_id))
    let recebidos: { evento: EventoExecucao; recebidoEm: number }[] = []

    function guardar(evento: EventoExecucao, recebidoEm: number) {
      recebidos.push({ evento, recebidoEm })
      // A tela mantém cinco amostras; passos e mudanças de estado não são descartados.
      if (evento.tipo === 'telemetria' && recebidos.filter((item) => item.evento.tipo === 'telemetria').length > 5) {
        const primeira = recebidos.findIndex((item) => item.evento.tipo === 'telemetria')
        recebidos.splice(primeira, 1)
      }
    }

    async function sincronizar() {
      if (consultando) {
        consultarNovamente = true
        return
      }
      consultando = true
      try {
        const retrato = await buscarExecucao(execucaoId!, controle.signal)
        if (controle.signal.aborted) return
        if (tentativaPendente && !retrato.tentativas.some((tentativa) => tentativa.tentativa_id === tentativaPendente)) {
          throw new Error('O retrato ainda não contém a nova tentativa.')
        }
        idsConhecidos = new Set(retrato.tentativas.map((tentativa) => tentativa.tentativa_id))
        tentativaPendente = null
        const duranteConsulta = recebidos
        recebidos = []
        definirDados((atual) => {
          if (atual?.execucao?.execucao_id !== execucaoId) return atual
          const atualizado = combinarRetrato(atual.execucao, retrato)
          return { ...atual, execucao: duranteConsulta.reduce((execucao, item) => aplicarEvento(execucao, item.evento, item.recebidoEm), atualizado) }
        })
        setErro(null)
        if (retrato.status !== 'em_andamento') encerrar()
      } catch {
        if (!controle.signal.aborted) setErro({ id: execucaoId!, mensagem: 'Não foi possível atualizar todos os dados da execução. Os últimos dados recebidos foram mantidos.' })
      } finally {
        consultando = false
        if (consultarNovamente && !controle.signal.aborted) {
          consultarNovamente = false
          void sincronizar()
        }
      }
    }
    sincronizarRef.current = () => { void sincronizar() }

    const encerrar = api.assinar(execucaoId, (evento) => {
      if (controle.signal.aborted) return
      const recebidoEm = api.agora()
      const desconhecida = 'tentativa_id' in evento && !idsConhecidos.has(evento.tentativa_id)
      if (desconhecida) tentativaPendente = evento.tentativa_id
      const consultar = desconhecida || evento.tipo === 'status' || evento.tipo === 'falha' || evento.tipo === 'sucesso'
      if (consultando || tentativaPendente || consultar) guardar(evento, recebidoEm)
      if (!tentativaPendente || evento.tipo !== 'telemetria' && evento.tipo !== 'passo') {
        definirDados((atual) => atual?.execucao?.execucao_id === execucaoId
          ? { ...atual, execucao: aplicarEvento(atual.execucao, evento, recebidoEm) } : atual)
      }
      // Esses eventos não trazem todas as datas/métricas; consultar o retrato persistido.
      if (consultar) void sincronizar()
    }, () => { void sincronizar() })

    return () => {
      controle.abort()
      sincronizarRef.current = () => {}
      encerrar()
    }
  }, [execucaoId, definirDados])

  return { ...requisicao,
    // Com uma execução conhecida, comandos também usam o GET reconciliado com o SSE.
    recarregar: execucaoId ? () => sincronizarRef.current() : requisicao.recarregar,
    erroSincronizacao: erro && erro.id === execucaoId ? erro.mensagem : null,
    sincronizar: () => sincronizarRef.current() }
}
