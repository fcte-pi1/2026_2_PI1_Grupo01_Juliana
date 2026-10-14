import { useEffect } from 'react'
import { api } from '../api/clienteApi'
import { buscarExecucaoAtual } from '../api/endpoints'
import type { ExecucaoAtualResposta } from '../api/tipos'
import { useRequisicao } from './useRequisicao'

/**
 * Execução da tela inicial: busca GET /execucoes/em-andamento e, havendo execução,
 * assina o stream SSE. Por enquanto só as leituras são aplicadas; o tratamento
 * completo dos eventos fica para o FRONT-03.
 */
export function useExecucaoAtual() {
  const requisicao = useRequisicao<ExecucaoAtualResposta>(buscarExecucaoAtual, 'atual')
  const { definirDados } = requisicao
  const execucaoId = requisicao.dados?.execucao?.execucao_id

  useEffect(() => {
    if (!execucaoId) return
    return api.assinar(execucaoId, (evento) => {
      if (evento.tipo !== 'leitura') return
      definirDados((atual) => {
        if (!atual?.execucao || atual.execucao.execucao_id !== execucaoId) return atual
        const { execucao } = atual
        return {
          ...atual,
          execucao: {
            ...execucao,
            leituras: [...execucao.leituras.slice(-4), evento.leitura],
            ultima_mensagem_em: evento.leitura.enviado_em,
          },
        }
      })
    })
  }, [execucaoId, definirDados])

  return requisicao
}
