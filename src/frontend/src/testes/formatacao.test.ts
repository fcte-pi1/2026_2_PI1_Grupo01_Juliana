import { describe, expect, it } from 'vitest'
import { formatarNumeroExecucao, formatarTempo, nomeCelula } from '../utils/formatacao'

describe('formatação', () => {
  it('nomeia a célula com coluna em letra e linha a partir de 1', () => {
    expect(nomeCelula({ x: 0, y: 0 })).toBe('A1')
    expect(nomeCelula({ x: 3, y: 3 })).toBe('D4')
    expect(nomeCelula({ x: 11, y: 3 })).toBe('L4')
  })

  it('formata o tempo como mm:ss,d', () => {
    expect(formatarTempo(20.4)).toBe('00:20,4')
    expect(formatarTempo(108.6)).toBe('01:48,6')
    expect(formatarTempo(600)).toBe('10:00,0')
  })

  it('formata o número da execução com 4 dígitos', () => {
    expect(formatarNumeroExecucao(42)).toBe('#0042')
  })
})
