import { useState } from 'react'
import { cenarioAtual, definirCenario } from './cenarioAtual'
import { CENARIOS } from './cenarios'
import './painelCenarios.css'

/** Painel flutuante (só no modo mock) para trocar entre os 12 estados do protótipo. */
export function PainelCenarios() {
  const [aberto, setAberto] = useState(false)
  const atual = cenarioAtual()

  function trocar(id: string) {
    definirCenario(id)
    const cenario = CENARIOS.find((c) => c.id === id)
    const busca = new URLSearchParams({ cenario: id })
    if (cenario?.modal) busca.set('modal', cenario.modal)
    // Recarrega para buscar os dados do novo cenário e reabrir o stream SSE.
    window.location.assign(`/?${busca}`)
  }

  return (
    <div className="painel-cenarios">
      {aberto && (
        <ul className="painel-cenarios__lista">
          {CENARIOS.map((c) => (
            <li key={c.id}>
              <button
                type="button"
                className={c.id === atual.id ? 'painel-cenarios__ativo' : undefined}
                onClick={() => trocar(c.id)}
              >
                <span className="mono">{c.id.slice(0, 2)}</span> {c.titulo}
              </button>
            </li>
          ))}
        </ul>
      )}
      <button type="button" className="painel-cenarios__botao" onClick={() => setAberto((a) => !a)}>
        Mock · {atual.id.slice(0, 2)} {atual.titulo}
      </button>
    </div>
  )
}
