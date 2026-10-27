import { createElement } from 'react'
import { render, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { buscarExecucaoRegistrada, dadosAtuais, definirCenario, registrarNovaExecucao } from '../mocks/cenarioAtual'
import { buscarCenario } from '../mocks/cenarios'
import { avancarDemonstracao } from '../mocks/fluxoDemonstracao'
import { criarHandlerStream } from '../mocks/handlers'
import { PainelCenarios } from '../mocks/PainelCenarios'
import { servidor } from '../mocks/servidor'

const API = 'http://localhost/api'

async function comando(id: string, acao: string, corpo?: unknown) {
  return fetch(`${API}/execucoes/${id}/${acao}`, {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: corpo === undefined ? undefined : JSON.stringify(corpo),
  })
}

function avancar(id: string, segundos: number, inicio = Date.now()) {
  return Array.from({ length: segundos }, (_, i) => avancarDemonstracao(id, inicio + (i + 1) * 1_000)).flat()
}

beforeEach(() => definirCenario('01-inicio'))
afterEach(() => { vi.useRealTimers(); vi.unstubAllGlobals() })

describe('mocks conectados: somente demonstração local', () => {
  it.each(['04-concluida', '11-concluida-apos-retomada', '12-falha-sem-retomada'])(
    'seleciona explicitamente a execução encerrada do cenário %s sem alterar /em-andamento', async (id) => {
      const navegar = vi.fn()
      const janela = window
      vi.stubGlobal('window', new Proxy(janela, {
        get(alvo, chave) {
          return chave === 'location' ? { search: '', assign: navegar } : Reflect.get(alvo, chave, alvo)
        },
      }))
      render(createElement(PainelCenarios))
      await userEvent.click(screen.getByRole('button', { name: /^Mock/ }))
      const cenario = buscarCenario(id)
      await userEvent.click(screen.getByRole('button', { name: new RegExp(cenario.titulo.replace(/[()]/g, '\\$&')) }))
      expect(navegar).toHaveBeenCalledWith(`/?cenario=${id}&execucao=${cenario.atual(Date.now()).execucao!.execucao_id}`)
      expect((await fetch(`${API}/execucoes/em-andamento`)).status).toBe(204)
    },
  )

  it('mantém o SSE da e-0042 sem exigir um GET anterior e reproduz medições nas unidades do backend', async () => {
    definirCenario('03-em-execucao')
    vi.useFakeTimers({ toFake: ['setInterval', 'clearInterval', 'Date'] })
    // O teste consome o ReadableStream HTTP diretamente; o jsdom não tem EventSource.
    vi.stubGlobal('EventSource', class {})
    servidor.use(criarHandlerStream())
    const controller = new AbortController()
    const resposta = await fetch(`${API}/execucoes/e-0042/stream`, {
      signal: controller.signal, headers: { Accept: 'text/event-stream' },
    })
    const leitor = resposta.body!.getReader()
    const decoder = new TextDecoder()
    let recebido = ''
    async function ate(texto: string) {
      while (!recebido.includes(texto)) {
        const parte = await leitor.read()
        expect(parte.done).toBe(false)
        recebido += decoder.decode(parte.value)
      }
    }
    try {
      await ate('event:status')
      await vi.advanceTimersByTimeAsync(2_000)
      await ate('event:telemetria')
      expect(recebido).toContain('"bateria":7810')
      expect(recebido).toContain('"velocidade":210')
      expect(buscarExecucaoRegistrada('e-0042')!.leituras.at(-1)?.bateria).toBe(7810)
    } finally {
      controller.abort()
      await leitor.cancel()
    }
  })

  it('abre após o health-check, conecta corrida e sucesso e encontra o mesmo registro no histórico e detalhe', async () => {
    const inicio = Date.now()
    const resposta = await fetch(`${API}/execucoes`, { method: 'POST',
      headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ tipo_labirinto: '4x4' }) })
    expect(resposta.status).toBe(201)
    const criada = await resposta.json()
    expect(buscarExecucaoRegistrada(criada.execucao_id)!.tentativas[0].health_check).toHaveLength(8)
    expect(buscarExecucaoRegistrada(criada.execucao_id)!.tentativas[0].status).toBe('running')
    const eventos = avancar(criada.execucao_id, 12, inicio)
    expect(eventos.filter((evento) => evento.tipo === 'health_check')).toHaveLength(0)
    expect(eventos.at(-1)).toMatchObject({ tipo: 'sucesso', x: 3, y: 3 })
    const detalhe = await (await fetch(`${API}/execucoes/${criada.execucao_id}`)).json()
    expect(detalhe).toMatchObject({ execucao_id: criada.execucao_id, status: 'concluida',
      tentativas: [{ tentativa_id: criada.tentativa_id, status: 'success', bateria_inicial: 7840, bateria_final: 7560 }] })
    expect(detalhe.trajeto).toHaveLength(11)
    expect(detalhe.trajeto.some((passo: { paredes_mask: number }) => passo.paredes_mask > 0)).toBe(true)
    expect((await fetch(`${API}/execucoes/em-andamento`)).status).toBe(204)
    const historico = await (await fetch(`${API}/execucoes`)).json()
    expect(historico[0]).toMatchObject({ execucao_id: criada.execucao_id, status: 'concluida' })
  })

  it('preserva motivo, observação, posição e paredes da falha ao retomar sem trocar a identidade', async () => {
    definirCenario('03-em-execucao')
    const antes = dadosAtuais(Date.now()).execucao!
    const resposta = await comando(antes.execucao_id, 'encerrar-tentativa', { motivo: 'collision', observacao: 'Parede norte' })
    expect(resposta.status).toBe(200)
    const encerrada = await resposta.json()
    expect(encerrada.tentativas[0].falha).toMatchObject({ motivo: 'collision', origem: 'encerrado_operador',
      celula_x: 1, celula_y: 3, observacao: 'Parede norte' })
    expect(encerrada.trajeto.map((passo: { paredes_mask: number }) => passo.paredes_mask))
      .toEqual(antes.trajeto.map((passo) => passo.paredes_mask))
    const respostaRetomada = await comando(antes.execucao_id, 'retomar')
    expect(respostaRetomada.status).toBe(201)
    expect(await respostaRetomada.json()).toMatchObject({ execucao_id: antes.execucao_id, attempt_index: 2 })
    const retomada = buscarExecucaoRegistrada(antes.execucao_id)!
    expect(retomada.tentativas[0].tentativa_id).toBe(antes.tentativas[0].tentativa_id)
    expect(retomada.tentativas[0].falha).toEqual(encerrada.tentativas[0].falha)
    expect(retomada.tentativas[1]).toMatchObject({ status: 'running', bateria_inicial: 7840 })
    expect(retomada.tentativas[1].health_check).toHaveLength(8)
    expect(retomada.trajeto).toEqual(antes.trajeto)
    avancar(antes.execucao_id, 22)
    const concluida = buscarExecucaoRegistrada(antes.execucao_id)!
    expect(concluida.status).toBe('concluida')
    expect(concluida.tentativas).toHaveLength(2)
    expect(concluida.trajeto.filter((passo) => passo.retomada)).toHaveLength(1)
    expect(concluida.tentativas[0].falha).toEqual(encerrada.tentativas[0].falha)
  })

  it('restaura o progresso, os IDs e o início pela sessão após recarregar o módulo', async () => {
    const nova = registrarNovaExecucao('4x4', Date.now())
    avancar(nova.execucao_id, 13)
    const antes = buscarExecucaoRegistrada(nova.execucao_id)!
    vi.resetModules()
    const recarregada = await import('../mocks/cenarioAtual')
    expect(recarregada.dadosAtuais(Date.now() + 20_000).execucao).toEqual(antes)
    expect(recarregada.buscarExecucaoRegistrada(nova.execucao_id)).toEqual(antes)
  })

  it('recusa retomada com tentativa aberta, cancela no limite e rejeita IDs desconhecidos', async () => {
    definirCenario('03-em-execucao')
    const aberta = await comando('e-0042', 'retomar')
    expect(aberta.status).toBe(409)
    expect(await aberta.json()).toMatchObject({ motivo: 'tentativa_aberta' })
    for (let tentativa = 1; tentativa <= 3; tentativa += 1) {
      expect((await comando('e-0042', 'encerrar-tentativa', { motivo: 'stuck', observacao: null })).status).toBe(200)
      if (tentativa < 3) expect((await comando('e-0042', 'retomar')).status).toBe(201)
    }
    const limite = await comando('e-0042', 'retomar')
    expect(limite.status).toBe(409)
    expect(await limite.json()).toMatchObject({ motivo: 'limite_3' })
    const cancelada = await comando('e-0042', 'encerrar')
    expect(cancelada.status).toBe(409)
    const desconhecida = await comando('ausente', 'encerrar-tentativa', { motivo: 'stuck', observacao: null })
    expect(desconhecida.status).toBe(404)
    expect((await (await fetch(`${API}/execucoes`)).json()).find((item: { execucao_id: string }) => item.execucao_id === 'e-0042').status).toBe('cancelada')
  })

  it('mantém os labirintos sem exemplo de percurso aguardando dados, sem criar medições', () => {
    const nova = registrarNovaExecucao('8x4', Date.now())
    avancar(nova.execucao_id, 30)
    const atual = buscarExecucaoRegistrada(nova.execucao_id)!
    expect(atual.labirinto).toBe('8x4')
    expect(atual.tentativas[0].status).toBe('running')
    expect(atual.trajeto).toEqual([])
    expect(atual.leituras).toEqual([])
  })

  it.each(['04-concluida', '08-encerrada-como-falha', '10-retomada-tentativa-2', '11-concluida-apos-retomada'])(
    'mantém as datas e as durações já definidas coerentes no exemplo %s', (cenario) => {
      const atual = buscarCenario(cenario).atual(Date.now()).execucao!
      for (const tentativa of atual.tentativas) {
        expect(Date.parse(tentativa.iniciada_em)).toBeGreaterThanOrEqual(Date.parse(atual.iniciada_em))
        if (tentativa.encerrada_em && tentativa.tempo_s !== null) {
          expect((Date.parse(tentativa.encerrada_em) - Date.parse(tentativa.iniciada_em)) / 1_000).toBeCloseTo(tentativa.tempo_s)
        }
        if (tentativa.falha) {
          expect(Date.parse(tentativa.falha.momento_falha)).toBeGreaterThanOrEqual(Date.parse(tentativa.iniciada_em))
          expect(Date.parse(tentativa.falha.momento_falha)).toBeLessThanOrEqual(Date.parse(tentativa.encerrada_em!))
        }
      }
      if (atual.encerrada_em && atual.tempo_total_s !== null) {
        expect((Date.parse(atual.encerrada_em) - Date.parse(atual.iniciada_em)) / 1_000).toBeCloseTo(atual.tempo_total_s)
      }
    },
  )
})
