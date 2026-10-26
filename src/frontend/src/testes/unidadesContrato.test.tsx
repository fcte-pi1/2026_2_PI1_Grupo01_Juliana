import { render, screen, within } from '@testing-library/react'
import { http, HttpResponse } from 'msw'
import { describe, expect, it } from 'vitest'
import { buscarCenario } from '../mocks/cenarios'
import { detalheAPI, LABIRINTOS } from '../mocks/dados'
import { servidor } from '../mocks/servidor'
import { PainelAoVivo } from '../pages/inicio/PainelAoVivo'
import { PainelEncerrada } from '../pages/inicio/PainelEncerrada'
import { renderizarRota } from './renderizar'

function cartao(rotulo: string) {
  return within(screen.getByText(rotulo, { selector: '.metrica .rotulo' }).closest('section')!)
}

describe('unidades recebidas do backend (RNF-B01)', () => {
  it('exibe mV e mm/s sem converter valores e mantém a precisão do percentual recebido', () => {
    const agora = Date.now()
    const execucao = buscarCenario('03-em-execucao').atual(agora).execucao!
    execucao.leituras = [{ ordem: 1, x: 0, y: 1, bateria: 7810, velocidade: 210,
      bateria_pct: 51.234567, enviado_em: new Date(agora).toISOString() }]
    execucao.tentativas[0].health_check = [
      { componente: 'bateria', aprovado: true, valor_lido: 7840 },
      { componente: 'tof_frontal', aprovado: true, valor_lido: 90 },
    ]
    render(<PainelAoVivo execucao={execucao} tentativa={execucao.tentativas[0]} labirinto={LABIRINTOS[0]} agora={agora} />)
    expect(cartao('Bateria').getByText('7810')).toHaveTextContent('mV')
    expect(cartao('Bateria').getByText('51,234567 % de carga')).toBeInTheDocument()
    expect(screen.getByRole('progressbar', { name: 'Bateria' })).toHaveAttribute('aria-valuenow', '51.234567')
    expect(cartao('Velocidade').getByText('210')).toHaveTextContent('mm/s')
    expect(screen.getByText('7840 mV · OK')).toBeInTheDocument()
    expect(screen.getByText('90 mm · OK')).toBeInTheDocument()
    expect(screen.getByRole('table')).toHaveTextContent('7810 mV')
    expect(screen.getByRole('table')).toHaveTextContent('210 mm/s')
  })

  it('mantém consumo e comparação recebidos no painel encerrado', () => {
    const agora = Date.now()
    const execucao = buscarCenario('04-concluida').atual(agora).execucao!
    execucao.variacao_melhor_tempo_pct = -5.123456
    const tentativa = execucao.tentativas[0]
    tentativa.consumo_bateria = 280
    tentativa.consumo_bateria_pct = 28.123456
    render(<PainelEncerrada execucao={execucao} tentativa={tentativa} labirinto={LABIRINTOS[0]} agora={agora} />)
    expect(cartao('Bateria usada').getByText('280')).toHaveTextContent('mV')
    expect(cartao('Bateria usada').getByText(/28,123456/)).toBeInTheDocument()
    expect(cartao('Variação contra o melhor tempo').getByText('-5,123456')).toHaveTextContent('%')
  })

  it('restaura consumo, velocidade média e comparação no detalhe sem arredondar', async () => {
    const execucao = buscarCenario('04-concluida').atual(Date.now()).execucao!
    execucao.variacao_melhor_tempo_pct = -5.123456
    Object.assign(execucao.tentativas[0], { consumo_bateria: 280, consumo_bateria_pct: 28.123456, velocidade_media: 0.078123456 })
    servidor.use(http.get('*/api/execucoes/:id', () => HttpResponse.json(detalheAPI(execucao))))
    renderizarRota('/execucoes/e-0042')
    await screen.findByRole('heading', { name: 'Execução #0042' })
    expect(screen.getByRole('table')).toHaveTextContent('280 mV')
    expect(screen.getByRole('table')).toHaveTextContent('28,123456')
    expect(screen.getByRole('table')).toHaveTextContent('0,078123456 m/s')
    expect(cartao('Variação contra o melhor tempo').getByText('-5,123456')).toHaveTextContent('%')
  })

  it('mostra comparação e consumo percentual ausentes como pendentes', () => {
    const agora = Date.now()
    const execucao = buscarCenario('04-concluida').atual(agora).execucao!
    execucao.variacao_melhor_tempo_pct = null
    execucao.tentativas[0].consumo_bateria_pct = null
    render(<PainelEncerrada execucao={execucao} tentativa={execucao.tentativas[0]} labirinto={LABIRINTOS[0]} agora={agora} />)
    expect(cartao('Bateria usada').getByText('Percentual pendente')).toBeInTheDocument()
    expect(cartao('Variação contra o melhor tempo').getByText('—')).toBeInTheDocument()
  })

  it.each([null, 0])('preserva tempo final %s sem substituir pela diferença entre datas', (tempo) => {
    const agora = Date.now()
    const execucao = buscarCenario('04-concluida').atual(agora).execucao!
    execucao.tempo_total_s = tempo
    execucao.tentativas[0].tempo_s = tempo
    render(<PainelEncerrada execucao={execucao} tentativa={execucao.tentativas[0]} labirinto={LABIRINTOS[0]} agora={agora} />)
    const cronometro = screen.getByText('Tempo da tentativa').closest('section')!
    expect(within(cronometro).getByText(tempo === null ? '—' : '00:00,0', { selector: '.tempo__cronometro' })).toBeInTheDocument()
    const total = screen.getByText('Tempo da execução').closest('section')!
    expect(within(total).getByText(tempo === null ? '—' : '00:00,0', { selector: '.mono' })).toBeInTheDocument()
  })
})
