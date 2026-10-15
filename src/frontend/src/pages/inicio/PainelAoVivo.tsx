import type { Execucao, Labirinto, Tentativa } from '../../api/tipos'
import { CartaoComunicacao } from '../../components/telemetria/CartaoComunicacao'
import { BlocoHealthCheck } from '../../components/telemetria/BlocoHealthCheck'
import { MetricasTempoReal } from '../../components/telemetria/MetricasTempoReal'
import { TabelaLeituras } from '../../components/telemetria/TabelaLeituras'
import { VisualizacaoLabirinto } from '../../components/labirinto/VisualizacaoLabirinto'

interface PainelAoVivoProps {
  execucao: Execucao
  tentativa: Tentativa
  labirinto: Labirinto
  agora: number
}

/** Telemetria da tentativa aberta: health-check ou em execução (RF01, RF04). */
export function PainelAoVivo({ execucao, tentativa, labirinto, agora }: PainelAoVivoProps) {
  const celula = execucao.celula_atual ?? execucao.leituras.at(-1) ?? execucao.trajeto.at(-1)

  return (
    <div className="coluna painel-tempo-real">
      <MetricasTempoReal execucao={execucao} tentativa={tentativa} agora={agora} />
      <div className="pagina-duas-colunas">
        <div className="coluna">
          <BlocoHealthCheck itens={tentativa.health_check} />
          {execucao.leituras.length > 0 && <TabelaLeituras leituras={execucao.leituras} largadaEm={tentativa.iniciada_em} />}
        </div>

        <div className="coluna">
          <section className="cartao">
            <h2 className="cartao__titulo">Trajeto</h2>
            <VisualizacaoLabirinto
              largura={labirinto.largura}
              altura={labirinto.altura}
              trajeto={execucao.trajeto}
              celulaAtual={celula ?? null}
              compacto
            />
            <p className="texto-suave espaco-acima">
              O trajeto é desenhado célula a célula conforme o robô entra em cada uma; o mapa completo aparece quando a execução
              terminar.
            </p>
          </section>
          <CartaoComunicacao ultimaMensagemEm={execucao.ultima_mensagem_em} />
        </div>
      </div>
    </div>
  )
}
