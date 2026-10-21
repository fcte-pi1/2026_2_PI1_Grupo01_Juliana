import { render, screen } from '@testing-library/react'
import { describe, expect, it } from 'vitest'
import type { PassoTrajeto, Tentativa } from '../api/tipos'
import { deduzirEixo, dimensoes, dividirEmTrechos, eixoParaGrade, passosDaTentativa } from '../components/labirinto/trajeto'
import { VisualizacaoLabirinto } from '../components/labirinto/VisualizacaoLabirinto'
import { trajeto12x4Longo } from '../mocks/dados'

function passos(celulas: Array<[number, number]>, retomadaEm?: number): PassoTrajeto[] {
  return celulas.map(([x, y], i) => ({ seq: i + 1, x, y, paredes_mask: 0, retomada: i === retomadaEm }))
}

function pontos(linha: Element): string[] {
  return (linha.getAttribute('points') ?? '').split(' ').filter(Boolean)
}

describe('VisualizacaoLabirinto', () => {
  it('desenha os 144 passos do 12x4 numa linha só', () => {
    const trajeto = trajeto12x4Longo()
    expect(trajeto).toHaveLength(144)

    const { container } = render(<VisualizacaoLabirinto largura={12} altura={4} trajeto={trajeto} />)

    expect(screen.getByRole('img', { name: 'Labirinto 12x4 com 48 células visitadas' })).toBeInTheDocument()
    const linhas = container.querySelectorAll('polyline.labirinto__trajeto')
    expect(linhas).toHaveLength(1)
    expect(pontos(linhas[0])).toHaveLength(144)
  })

  it('acompanha o trajeto passo a passo sem multiplicar a grade', () => {
    const trajeto = trajeto12x4Longo()
    const { container, rerender } = render(<VisualizacaoLabirinto largura={12} altura={4} trajeto={[]} compacto />)
    const inicio = performance.now()

    for (let n = 1; n <= trajeto.length; n++) {
      rerender(<VisualizacaoLabirinto largura={12} altura={4} trajeto={trajeto.slice(0, n)} compacto />)
    }

    expect(performance.now() - inicio).toBeLessThan(3000)
    expect(container.querySelectorAll('rect.labirinto__celula')).toHaveLength(48)
    expect(pontos(container.querySelector('polyline.labirinto__trajeto')!)).toHaveLength(144)
    expect(container.querySelector('.labirinto__trajeto--novo')).toBeInTheDocument()
  })

  it('separa a retomada em outro trecho e marca a retomada na célula da falha', () => {
    const trajeto = passos(
      [
        [0, 0],
        [0, 1],
        [1, 1],
        [1, 1],
        [1, 2],
      ],
      3,
    )
    const { container } = render(
      <VisualizacaoLabirinto largura={4} altura={4} trajeto={trajeto} falhas={[{ x: 1, y: 1, tentativa: 1 }]} />,
    )

    expect(container.querySelectorAll('polyline.labirinto__trajeto')).toHaveLength(2)
    expect(container.querySelector('.labirinto__trajeto--retomada')).toBeInTheDocument()
    expect(screen.getByText('RETOMADA')).toBeInTheDocument()
    expect(screen.getByText('FALHA T1')).toBeInTheDocument()
    const falha = container.querySelector('.labirinto__destaque--falha')!
    const retomada = container.querySelector('.labirinto__destaque--retomada')!
    expect([retomada.getAttribute('x'), retomada.getAttribute('y')]).toEqual([falha.getAttribute('x'), falha.getAttribute('y')])
    // a falha da tentativa 1 continua no mapa, mas o robô já seguiu na tentativa 2
    expect(container.querySelector('.labirinto__robo--falha')).not.toBeInTheDocument()
  })

  it('pinta o robô de vermelho quando a tentativa termina em falha', () => {
    const { container } = render(
      <VisualizacaoLabirinto largura={4} altura={4} trajeto={passos([[0, 0], [0, 1], [1, 1]])} falhas={[{ x: 1, y: 1, tentativa: 1 }]} />,
    )

    expect(container.querySelector('.labirinto__robo--falha')).toBeInTheDocument()
  })

  it('move o marcador a cada passo, mesmo com a última leitura atrasada', () => {
    const trajeto = passos([[0, 0], [0, 1], [0, 2]])
    const { rerender } = render(
      <VisualizacaoLabirinto largura={4} altura={4} trajeto={trajeto.slice(0, 2)} celulaAtual={{ x: 0, y: 0 }} />,
    )
    expect(screen.getByText('Robô em A2')).toBeInTheDocument()

    rerender(<VisualizacaoLabirinto largura={4} altura={4} trajeto={trajeto} celulaAtual={{ x: 0, y: 0 }} />)
    expect(screen.getByText('Robô em A3')).toBeInTheDocument()
  })

  it('desenha o 12x4 inteiro antes de o robô sair do canto da largada', () => {
    const { container, rerender } = render(
      <VisualizacaoLabirinto largura={12} altura={4} trajeto={passos([[0, 0], [1, 0], [2, 0], [3, 0]])} />,
    )
    expect(container.querySelectorAll('rect.labirinto__celula')).toHaveLength(48)
    expect(screen.getByText('Objetivo L4')).toBeInTheDocument()

    rerender(<VisualizacaoLabirinto largura={12} altura={4} trajeto={[]} />)
    expect(container.querySelectorAll('rect.labirinto__celula')).toHaveLength(48)
  })

  it('usa a posição reconciliada sem descartar o percurso e as falhas do FRONT-04', () => {
    const { container, rerender } = render(
      <VisualizacaoLabirinto largura={4} altura={4} trajeto={passos([[0, 0], [0, 1]])}
        posicaoAtual={{ x: 0, y: 2 }} />,
    )
    expect(screen.getByText('Robô em A3')).toBeInTheDocument()
    expect(pontos(container.querySelector('polyline.labirinto__trajeto')!)).toHaveLength(2)

    rerender(<VisualizacaoLabirinto largura={4} altura={4} trajeto={passos([[0, 0], [0, 1]])}
      posicaoAtual={{ x: 1, y: 1 }} falhas={[{ x: 1, y: 1, tentativa: 1 }]} />)
    expect(screen.getByText('Robô em B2 · falhou')).toBeInTheDocument()
    expect(screen.getByText('FALHA T1')).toBeInTheDocument()
    expect(pontos(container.querySelector('polyline.labirinto__trajeto')!)).toHaveLength(2)
  })

  it('põe o lado longo em y quando o robô passa de y = 3', () => {
    const { container } = render(
      <VisualizacaoLabirinto largura={8} altura={4} trajeto={passos([[0, 0], [0, 1], [0, 2], [0, 3], [0, 4]])} />,
    )

    const eixo = Array.from(container.querySelectorAll('.labirinto__eixo text'), (t) => t.textContent)
    expect(eixo).toEqual(['A', 'B', 'C', 'D', '1', '2', '3', '4', '5', '6', '7', '8'])
    expect(screen.getByText('Objetivo D8')).toBeInTheDocument()
  })

  it('usa o eixo_longo do backend sem esperar o robô sair do 4x4', () => {
    const { container } = render(
      <VisualizacaoLabirinto largura={8} altura={4} eixoLongo="y" trajeto={passos([[0, 0], [0, 1]])} />,
    )

    expect(container.querySelectorAll('rect.labirinto__celula')).toHaveLength(32)
    expect(screen.getByText('Objetivo D8')).toBeInTheDocument()
  })
})

describe('funções do trajeto', () => {
  it('começa um trecho novo em cada passo de retomada', () => {
    const trechos = dividirEmTrechos(passos([[0, 0], [0, 1], [0, 1], [1, 1], [1, 1]], 2))
    expect(trechos.map((t) => t.length)).toEqual([2, 3])
    expect(dividirEmTrechos([])).toEqual([])
  })

  it('separa os passos por tentativa pela hora, mesmo quando uma tentativa não andou', () => {
    const hora = (s: number) => new Date(Date.UTC(2026, 9, 20, 22, 0, s)).toISOString()
    // A tentativa 1 caiu por link_lost antes do primeiro passo; as retomadas repartem de A1.
    const tentativas = [1, 2, 3].map((i) => ({ attempt_index: i, iniciada_em: hora(i * 10) })) as Tentativa[]
    const trajeto = [[0, 0, 21], [0, 1, 22], [0, 0, 31], [0, 1, 32], [1, 1, 33]].map(([x, y, s], i) => ({
      seq: i + 1, x, y, paredes_mask: 0, retomada: i === 0 || i === 2, entrou_em: hora(s),
    }))
    expect(passosDaTentativa(trajeto, tentativas, 1)).toEqual([])
    expect(passosDaTentativa(trajeto, tentativas, 2).map((p) => p.seq)).toEqual([1, 2])
    expect(passosDaTentativa(trajeto, tentativas, 3).map((p) => p.seq)).toEqual([3, 4, 5])
    // Sem entrou_em, vale a ordem dos trechos.
    const semHora = passos([[0, 0], [0, 1], [0, 0], [1, 0]], 2)
    expect(passosDaTentativa(semHora, tentativas, 2).map((p) => p.seq)).toEqual([3, 4])
  })

  it('deduz o eixo longo e as dimensões da grade', () => {
    expect(deduzirEixo(passos([[11, 3]]), 4)).toBe('x')
    expect(deduzirEixo(passos([[3, 11]]), 4)).toBe('y')
    expect(deduzirEixo(passos([[3, 3]]), 4)).toBeNull()
    expect(eixoParaGrade(12, 4, null)).toBe('x')
    expect(eixoParaGrade(4, 4, null)).toBeNull()
    expect(eixoParaGrade(8, 4, 'y')).toBe('y')
    expect(dimensoes(12, 4, null)).toEqual({ colunas: 4, linhas: 4 })
    expect(dimensoes(12, 4, 'x')).toEqual({ colunas: 12, linhas: 4 })
    expect(dimensoes(12, 4, 'y')).toEqual({ colunas: 4, linhas: 12 })
  })
})
