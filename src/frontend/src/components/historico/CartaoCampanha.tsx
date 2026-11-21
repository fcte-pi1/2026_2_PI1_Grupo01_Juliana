import type { ResumoCampanha, TipoLabirinto } from '../../api/tipos'
import { enderecoExportacao } from '../../api/endpoints'
import { formatarTempo } from '../../utils/formatacao'

interface CartaoCampanhaProps {
  campanha: ResumoCampanha
  labirinto: TipoLabirinto | null
  configuracao?: string | null
}

const pct = (valor: number | null | undefined) => (valor == null ? '—' : `${valor.toLocaleString('pt-BR')}%`)
const tempo = (valor: number | null | undefined) => (valor == null ? '—' : formatarTempo(valor))

/** Coleta de dados: resumo da campanha de corridas e a exportação, sem sair do front (FIRM-13). */
export function CartaoCampanha({ campanha, labirinto, configuracao = null }: CartaoCampanhaProps) {
  const encerradas = campanha.concluidas + campanha.canceladas
  return (
    <section className="cartao" aria-label="Resumo da campanha">
      <div className="cartao__cabecalho">
        <h2 className="cartao__titulo">Campanha de corridas</h2>
        <div className="cartao__acoes">
          <a className="botao" href={enderecoExportacao('csv', labirinto, configuracao)} download="execucoes.csv">Baixar CSV</a>
          <a className="botao" href={enderecoExportacao('json', labirinto, configuracao)} download="execucoes.json">Baixar JSON</a>
        </div>
      </div>
      <dl className="mini-metricas">
        <div><dt>Execuções</dt><dd className="mono">{campanha.total}</dd></div>
        <div><dt>Concluídas</dt><dd className="mono">{campanha.concluidas} de {encerradas}</dd></div>
        <div><dt>Taxa de sucesso</dt><dd className="mono">{pct(campanha.taxa_sucesso_pct)}</dd></div>
        <div><dt>Tempo médio</dt><dd className="mono">{tempo(campanha.tempo_medio_s)}</dd></div>
        <div><dt>Tempo máximo</dt><dd className="mono">{tempo(campanha.tempo_maximo_s)}</dd></div>
        <div><dt>Abaixo de 10 min</dt><dd className="mono">{campanha.concluidas_abaixo_de_10_min}</dd></div>
      </dl>
      {campanha.por_configuracao.length > 0 && (
        <ul className="lista-configuracoes" aria-label="Resumo por configuração de paredes">
          {campanha.por_configuracao.map((grupo) => (
            <li key={grupo.configuracao ?? '—'}>
              <strong>{grupo.configuracao ?? 'sem rótulo'}</strong>
              <span className="texto-suave">
                {grupo.total} execuções · {grupo.concluidas} concluídas · taxa {pct(grupo.taxa_sucesso_pct)} · médio {tempo(grupo.tempo_medio_s)} · máximo {tempo(grupo.tempo_maximo_s)}
              </span>
            </li>
          ))}
        </ul>
      )}
    </section>
  )
}
