import type { ExecucaoAtualResposta } from '../api/tipos'
import { execucao, falha, healthCheck, leituras, relativo, tentativa, trajetoAte, trajetoComRetomada, ULTIMA_EXECUCAO } from './dados'

export type ModalCenario = 'encerrar' | 'retomar'

export interface Cenario {
  id: string
  titulo: string
  /** Modal aberto ao exibir o cenário (estados 07 e 09 do protótipo). */
  modal?: ModalCenario
  /** O stream SSE manda uma leitura por segundo enquanto o cenário estiver aberto. */
  aoVivo?: boolean
  atual: (agora: number) => ExecucaoAtualResposta
}

const base = { ultima: ULTIMA_EXECUCAO, recusa: null, proximo_numero: 42 }

function emExecucao(agora: number, ultimaHaS: number) {
  const trajeto = trajetoAte(4)
  return execucao(agora, {
    tentativas: [tentativa(agora, { attempt_index: 1, status: 'running' })],
    trajeto,
    leituras: leituras(agora, trajeto, ultimaHaS),
    ultima_mensagem_em: relativo(agora, -ultimaHaS),
  })
}

function encerradaComoFalha(agora: number) {
  const trajeto = trajetoAte(6)
  return execucao(agora, {
    tentativas: [
      tentativa(agora, {
        attempt_index: 1,
        status: 'failed',
        encerrada_em: relativo(agora, -30),
        tempo_s: 20.4,
        velocidade_media: 0.088,
        bateria_final: 85.9,
        consumo_bateria: 2.1,
        falha: falha(agora),
      }),
    ],
    trajeto,
    leituras: leituras(agora, trajeto, 30),
    ultima_mensagem_em: relativo(agora, -30),
  })
}

/** Os 12 estados de tela do protótipo (docs/figs/prototipo). */
export const CENARIOS: Cenario[] = [
  {
    id: '01-inicio',
    titulo: 'Início, sem execução ativa',
    atual: () => ({ ...base, execucao: null }),
  },
  {
    id: '02-health-check',
    titulo: 'Health-check em andamento',
    aoVivo: true,
    atual: (agora) => ({
      ...base,
      execucao: execucao(agora, {
        iniciada_em: relativo(agora, -4),
        tentativas: [tentativa(agora, { attempt_index: 1, status: 'health-check', health_check: healthCheck(6) })],
        trajeto: trajetoAte(0),
      }),
    }),
  },
  {
    id: '03-em-execucao',
    titulo: 'Em execução',
    aoVivo: true,
    atual: (agora) => ({ ...base, execucao: emExecucao(agora, 1) }),
  },
  {
    id: '04-concluida',
    titulo: 'Concluída na tentativa 1',
    atual: (agora) => {
      const trajeto = trajetoAte(10)
      return {
        ...base,
        execucao: execucao(agora, {
          status: 'concluida',
          encerrada_em: relativo(agora, -5),
          tempo_total_s: 33.1,
          tentativas: [
            tentativa(agora, {
              attempt_index: 1,
              status: 'success',
              tipo_descoberto: '4x4',
              encerrada_em: relativo(agora, -5),
              tempo_s: 25.3,
              velocidade_media: 0.078,
              bateria_final: 85.2,
              consumo_bateria: 2.8,
            }),
          ],
          trajeto,
          leituras: leituras(agora, trajeto, 5),
          ultima_mensagem_em: relativo(agora, -5),
        }),
      }
    },
  },
  {
    id: '05-execucao-recusada',
    titulo: 'Abertura de tentativa recusada',
    atual: (agora) => ({
      ...base,
      execucao: emExecucao(agora, 1),
      recusa: { motivo: 'tentativa_aberta', ocorrida_em: relativo(agora, -2) },
    }),
  },
  {
    id: '06-sem-comunicacao',
    titulo: 'Sem comunicação com o robô',
    atual: (agora) => ({ ...base, execucao: emExecucao(agora, 12) }),
  },
  {
    id: '07-encerrar-execucao',
    titulo: 'Modal Encerrar tentativa',
    modal: 'encerrar',
    aoVivo: true,
    atual: (agora) => ({ ...base, execucao: emExecucao(agora, 1) }),
  },
  {
    id: '08-encerrada-como-falha',
    titulo: 'Tentativa encerrada como falha',
    atual: (agora) => ({ ...base, execucao: encerradaComoFalha(agora) }),
  },
  {
    id: '09-retomar-do-checkpoint',
    titulo: 'Modal Retomar tentativa',
    modal: 'retomar',
    atual: (agora) => ({ ...base, execucao: encerradaComoFalha(agora) }),
  },
  {
    id: '10-retomada-tentativa-2',
    titulo: 'Tentativa 2 em execução após retomada',
    aoVivo: true,
    atual: (agora) => {
      const anterior = encerradaComoFalha(agora)
      const trajeto = trajetoComRetomada(9)
      return {
        ...base,
        execucao: {
          ...anterior,
          tentativas_usadas: 2,
          tentativas: [...anterior.tentativas, tentativa(agora, { attempt_index: 2, status: 'running', bateria_inicial: 85.9 })],
          trajeto,
          leituras: leituras(agora, trajeto, 1),
          ultima_mensagem_em: relativo(agora, -1),
        },
      }
    },
  },
  {
    id: '11-concluida-apos-retomada',
    titulo: 'Concluída na tentativa 2',
    atual: (agora) => {
      const anterior = encerradaComoFalha(agora)
      const trajeto = trajetoComRetomada(10)
      return {
        ...base,
        execucao: {
          ...anterior,
          status: 'concluida',
          tentativas_usadas: 2,
          encerrada_em: relativo(agora, -3),
          tempo_total_s: 58.2,
          tentativas: [
            ...anterior.tentativas,
            tentativa(agora, {
              attempt_index: 2,
              status: 'success',
              tipo_descoberto: '4x4',
              encerrada_em: relativo(agora, -3),
              tempo_s: 11.0,
              velocidade_media: 0.082,
              bateria_inicial: 85.9,
              bateria_final: 84.8,
              consumo_bateria: 1.1,
            }),
          ],
          trajeto,
          leituras: leituras(agora, trajeto, 3),
          ultima_mensagem_em: relativo(agora, -3),
        },
      }
    },
  },
  {
    id: '12-falha-sem-retomada',
    titulo: 'Falha sem retomada (3 tentativas usadas)',
    atual: (agora) => {
      const trajeto = trajetoAte(6)
      const falhou = (attempt_index: number, motivo: 'stuck' | 'collision' | 'link_lost') =>
        tentativa(agora, {
          attempt_index,
          status: 'failed',
          encerrada_em: relativo(agora, -10 * (4 - attempt_index)),
          tempo_s: 15 + attempt_index,
          falha: falha(agora, { motivo, origem: motivo === 'link_lost' ? 'automatica' : 'encerrado_operador' }),
        })
      return {
        ...base,
        execucao: execucao(agora, {
          status: 'cancelada',
          tentativas_usadas: 3,
          encerrada_em: relativo(agora, -10),
          tempo_total_s: 96.5,
          tentativas: [falhou(1, 'stuck'), falhou(2, 'collision'), falhou(3, 'link_lost')],
          trajeto,
          leituras: leituras(agora, trajeto, 10),
          ultima_mensagem_em: relativo(agora, -10),
        }),
      }
    },
  },
]

export const CENARIO_PADRAO = CENARIOS[0]

export function buscarCenario(id: string | null): Cenario {
  return CENARIOS.find((c) => c.id === id) ?? CENARIO_PADRAO
}
