import { describe, expect, it } from 'vitest'
import { buscarCenario } from '../mocks/cenarios'
import { celulaCheckpoint, derivarEstadoInicio, type EstadoInicio } from '../pages/inicio/estadoInicio'

const ESPERADO: Array<[string, EstadoInicio]> = [
  ['01-inicio', 'sem-execucao'],
  ['02-health-check', 'health-check'],
  ['03-em-execucao', 'em-execucao'],
  ['04-concluida', 'concluida'],
  ['05-execucao-recusada', 'em-execucao'],
  ['06-sem-comunicacao', 'sem-comunicacao'],
  ['07-encerrar-execucao', 'em-execucao'],
  ['08-encerrada-como-falha', 'falha-retomavel'],
  ['09-retomar-do-checkpoint', 'falha-retomavel'],
  ['10-retomada-tentativa-2', 'em-execucao'],
  ['11-concluida-apos-retomada', 'concluida'],
  ['12-falha-sem-retomada', 'falha-sem-retomada'],
]

describe('derivarEstadoInicio', () => {
  it.each(ESPERADO)('cenário %s → %s', (id, estado) => {
    const agora = Date.now()
    const { execucao } = buscarCenario(id).atual(agora)
    expect(derivarEstadoInicio(execucao, agora)).toBe(estado)
  })

  it('passa para sem-comunicacao depois de 5 s sem mensagem', () => {
    const agora = Date.now()
    const { execucao } = buscarCenario('03-em-execucao').atual(agora)
    expect(derivarEstadoInicio(execucao, agora + 4_000)).toBe('em-execucao')
    expect(derivarEstadoInicio(execucao, agora + 6_000)).toBe('sem-comunicacao')
  })
})

describe('celulaCheckpoint', () => {
  it('é a célula anterior à falha no trajeto (B3 antes de B2)', () => {
    const { execucao } = buscarCenario('08-encerrada-como-falha').atual(Date.now())
    expect(celulaCheckpoint(execucao!, { x: 1, y: 1 })).toEqual({ x: 1, y: 2 })
  })

  it('é a própria célula quando a falha foi na largada', () => {
    const { execucao } = buscarCenario('02-health-check').atual(Date.now())
    expect(celulaCheckpoint(execucao!, { x: 0, y: 0 })).toEqual({ x: 0, y: 0 })
  })
})
