import type { Labirinto, TipoLabirinto } from '../../api/tipos'
import { formatarNumeroExecucao, formatarTempo, LADO_CELULA_M } from '../../utils/formatacao'
import { VisualizacaoLabirinto } from '../labirinto/VisualizacaoLabirinto'

interface SeletorLabirintoProps {
  labirintos: Labirinto[]
  selecionado: TipoLabirinto
  aoSelecionar: (tipo: TipoLabirinto) => void
  desabilitado?: boolean
}

interface FormNovaExecucaoProps extends SeletorLabirintoProps {
  proximoNumero?: number
}

export function SeletorLabirinto({ labirintos, selecionado, aoSelecionar, desabilitado }: SeletorLabirintoProps) {
  const cm = (celulas: number) => Math.round(celulas * LADO_CELULA_M * 100)

  return (
    <div className="seletor-labirinto" role="radiogroup" aria-label="Tipo de labirinto">
      {labirintos.map((l) => (
        <button
          key={l.tipo}
          type="button"
          role="radio"
          aria-checked={l.tipo === selecionado}
          disabled={desabilitado}
          className={`seletor-labirinto__opcao ${l.tipo === selecionado ? 'seletor-labirinto__opcao--ativa' : ''}`}
          onClick={() => aoSelecionar(l.tipo)}
        >
          <strong>{l.tipo}</strong>
          <span className="texto-suave">{cm(l.largura)} × {cm(l.altura)} cm</span>
          <span className="seletor-labirinto__melhor texto-suave">
            Melhor tempo{' '}
            <span className="mono">
              {l.melhor_tempo_s !== null
                ? `${formatarTempo(l.melhor_tempo_s)}${l.melhor_execucao_numero !== null ? ` · ${formatarNumeroExecucao(l.melhor_execucao_numero)}` : ''}`
                : 'nenhum ainda'}
            </span>
          </span>
        </button>
      ))}
    </div>
  )
}

/** Escolha do tipo de labirinto em Nova execução (US12). O tipo fica só no backend (RF27). */
export function FormNovaExecucao({ labirintos, selecionado, proximoNumero, aoSelecionar, desabilitado }: FormNovaExecucaoProps) {
  const atual = labirintos.find((l) => l.tipo === selecionado)

  return (
    <section className="cartao nova-execucao">
      <div className="cartao__cabecalho">
        <div>
          <h2 className="cartao__titulo">Nova execução</h2>
          <p className="texto-suave">Escolha o labirinto e crie a execução. O resto acontece sozinho.</p>
        </div>
        {proximoNumero !== undefined && <span className="chip">próximo ID {formatarNumeroExecucao(proximoNumero)}</span>}
      </div>

      <span className="rotulo">Labirinto</span>
      <SeletorLabirinto labirintos={labirintos} selecionado={selecionado} aoSelecionar={aoSelecionar} desabilitado={desabilitado} />

      {atual && <VisualizacaoLabirinto largura={atual.largura} altura={atual.altura} trajeto={[]} />}

      <p className="aviso aviso--info">
        Nova execução, no topo da tela, abre uma execução com a Tentativa 1/3 em health-check. Depois é só ligar o
        robô em A1.
      </p>
    </section>
  )
}
