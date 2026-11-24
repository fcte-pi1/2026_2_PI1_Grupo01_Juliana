interface FiltroConfiguracaoProps {
  /** Rótulos que existem nas execuções (vêm do resumo da campanha). */
  configuracoes: string[]
  valor: string | null
  aoMudar: (valor: string | null) => void
}

/** Escolhe a configuração de paredes; o resumo, a tabela e a exportação seguem o mesmo filtro. */
export function FiltroConfiguracao({ configuracoes, valor, aoMudar }: FiltroConfiguracaoProps) {
  if (configuracoes.length === 0) return null
  const opcoes: Array<{ valor: string | null; rotulo: string }> = [
    { valor: null, rotulo: 'Todas' },
    ...configuracoes.map((c) => ({ valor: c, rotulo: c })),
  ]
  return (
    <div className="filtro" role="group" aria-label="Filtrar por configuração de paredes">
      {opcoes.map((opcao) => (
        <button
          key={opcao.rotulo}
          type="button"
          className={`filtro__opcao ${valor === opcao.valor ? 'filtro__opcao--ativa' : ''}`}
          aria-pressed={valor === opcao.valor}
          onClick={() => aoMudar(opcao.valor)}
        >
          {opcao.rotulo}
        </button>
      ))}
    </div>
  )
}
