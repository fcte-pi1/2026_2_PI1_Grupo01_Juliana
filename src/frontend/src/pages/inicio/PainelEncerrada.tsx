import type { Execucao, Labirinto, Tentativa } from '../../api/tipos'
import { CartaoFalha } from '../../components/execucao/CartaoFalha'
import { VisualizacaoLabirinto } from '../../components/labirinto/VisualizacaoLabirinto'
import { CartaoMetrica } from '../../components/telemetria/CartaoMetrica'
import { formatarNumero, formatarTempo, LADO_CELULA_M } from '../../utils/formatacao'

interface PainelEncerradaProps {
  execucao: Execucao
  tentativa: Tentativa
  labirinto: Labirinto
}

/** Tentativa encerrada (sucesso ou falha): trajeto completo e métricas finais (RF01, RF09, RF15). */
export function PainelEncerrada({ execucao, tentativa, labirinto }: PainelEncerradaProps) {
  const { falha } = tentativa
  const celulasVisitadas = new Set(execucao.trajeto.map((p) => `${p.x},${p.y}`)).size
  const tempoS = execucao.tempo_total_s ?? tentativa.tempo_s

  return (
    <div className="pagina-duas-colunas">
      <section className="cartao">
        <div className="cartao__cabecalho">
          <div>
            <h2 className="cartao__titulo">Trajeto percorrido</h2>
            <p className="texto-suave">Montado com as {execucao.trajeto.length} células gravadas, em ordem</p>
          </div>
          <span className="mono texto-suave">
            {formatarNumero(Math.max(execucao.trajeto.length - 1, 0) * LADO_CELULA_M, 2)} m
          </span>
        </div>
        <VisualizacaoLabirinto
          largura={labirinto.largura}
          altura={labirinto.altura}
          trajeto={execucao.trajeto}
          celulaFalha={falha ? { x: falha.celula_x, y: falha.celula_y } : null}
        />
      </section>

      <div className="coluna">
        <div className="grade grade--2 metricas-compactas">
          <CartaoMetrica rotulo={falha ? 'Tempo até a falha' : 'Tempo total'} valor={tempoS !== null ? formatarTempo(tempoS) : '—'} unidade="min:s" />
          <CartaoMetrica
            rotulo="Velocidade média"
            valor={tentativa.velocidade_media !== null ? formatarNumero(tentativa.velocidade_media, 3) : '—'}
            unidade="m/s"
          />
          <CartaoMetrica
            rotulo="Bateria usada"
            valor={tentativa.consumo_bateria !== null ? formatarNumero(tentativa.consumo_bateria, 1) : '—'}
            unidade="%"
          />
          <CartaoMetrica rotulo="Células visitadas" valor={`${celulasVisitadas} de ${labirinto.largura * labirinto.altura}`} />
        </div>
        {falha && <CartaoFalha falha={falha} attemptIndex={tentativa.attempt_index} />}
      </div>
    </div>
  )
}
