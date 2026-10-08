import { buscarCenario, type Cenario } from './cenarios'

// Cenário exibido pelos mocks. Vem de ?cenario= na URL (útil para abrir um
// estado direto) e fica guardado na sessão para sobreviver à navegação.

const CHAVE = 'mock:cenario'

let atual: string | null = null

function lerSessao(): string | null {
  try {
    return sessionStorage.getItem(CHAVE)
  } catch {
    return null
  }
}

export function cenarioAtual(): Cenario {
  if (atual === null) {
    const daUrl = typeof window === 'undefined' ? null : new URLSearchParams(window.location.search).get('cenario')
    atual = daUrl ?? lerSessao()
  }
  return buscarCenario(atual)
}

export function definirCenario(id: string): void {
  atual = id
  try {
    sessionStorage.setItem(CHAVE, id)
  } catch {
    // sem sessionStorage o cenário só vale até recarregar a página
  }
}
