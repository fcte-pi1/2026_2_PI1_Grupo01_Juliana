import type { Execucao, Labirinto, Tentativa } from '../../api/tipos'
import { CartaoFalha } from '../../components/execucao/CartaoFalha'
import { VisualizacaoLabirinto } from '../../components/labirinto/VisualizacaoLabirinto'
import { CartaoMetrica } from '../../components/telemetria/CartaoMetrica'
import { BlocoHealthCheck } from '../../components/telemetria/BlocoHealthCheck'
import { MetricasTempoReal } from '../../components/telemetria/MetricasTempoReal'
import { TabelaLeituras } from '../../components/telemetria/TabelaLeituras'
import { formatarMedicao, formatarNumero, LADO_CELULA_M } from '../../utils/formatacao'

interface PainelEncerradaProps {
  execucao: Execucao
  tentativa: Tentativa
  labirinto: Labirinto
  agora: number
}

/** Tentativa encerrada (sucesso ou falha): trajeto completo e métricas finais (RF01, RF09, RF15). */
export function PainelEncerrada({ execucao, tentativa, labirinto, agora }: PainelEncerradaProps) {
  const { falha } = tentativa
  const celulasVisitadas = new Set(execucao.trajeto.map((p) => `${p.x},${p.y}`)).size

  return (
    <div className="coluna painel-tempo-real">
      <MetricasTempoReal execucao={execucao} tentativa={tentativa} agora={agora} />
      <div className="pagina-duas-colunas">
        <div className="coluna">
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
              celulaAtual={execucao.celula_atual ?? execucao.leituras.at(-1) ?? execucao.trajeto.at(-1) ?? null}
              celulaFalha={falha ? { x: falha.celula_x, y: falha.celula_y } : null}
            />
          </section>
          {execucao.leituras.length > 0 && <TabelaLeituras leituras={execucao.leituras} largadaEm={tentativa.iniciada_em} />}
        </div>

        <div className="coluna">
          <div className="grade grade--2 metricas-compactas">
            <CartaoMetrica
              rotulo="Bateria usada"
              valor={tentativa.consumo_bateria !== null ? formatarMedicao(tentativa.consumo_bateria) : '—'}
              unidade="V"
            />
            <CartaoMetrica rotulo="Células visitadas" valor={`${celulasVisitadas} de ${labirinto.largura * labirinto.altura}`} />
          </div>
          <BlocoHealthCheck itens={tentativa.health_check} />
          {falha && <CartaoFalha falha={falha} attemptIndex={tentativa.attempt_index} />}
        </div>
      </div>
    </div>
  )
}
