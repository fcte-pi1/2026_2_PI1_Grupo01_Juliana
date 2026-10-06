import type { Execucao, Labirinto, Tentativa } from '../../api/tipos'
import { CartaoMetrica } from '../../components/telemetria/CartaoMetrica'
import { CartaoComunicacao } from '../../components/telemetria/CartaoComunicacao'
import { CartaoTempo } from '../../components/telemetria/CartaoTempo'
import { BlocoHealthCheck } from '../../components/telemetria/BlocoHealthCheck'
import { TabelaLeituras } from '../../components/telemetria/TabelaLeituras'
import { VisualizacaoLabirinto } from '../../components/labirinto/VisualizacaoLabirinto'
import { formatarNumero, LADO_CELULA_M, nomeCelula } from '../../utils/formatacao'

interface PainelAoVivoProps {
  execucao: Execucao
  tentativa: Tentativa
  labirinto: Labirinto
  agora: number
}

/** Telemetria da tentativa aberta: health-check ou em execução (RF01, RF04). */
export function PainelAoVivo({ execucao, tentativa, labirinto, agora }: PainelAoVivoProps) {
  const ultima = execucao.leituras.at(-1)
  const tempoS = Math.max((agora - new Date(execucao.iniciada_em).getTime()) / 1000, 0)
  const celulasVisitadas = new Set(execucao.trajeto.map((p) => `${p.x},${p.y}`)).size
  const distanciaM = Math.max(execucao.trajeto.length - 1, 0) * LADO_CELULA_M
  const totalCelulas = labirinto.largura * labirinto.altura

  return (
    <div className="pagina-duas-colunas">
      <div className="coluna">
        {tentativa.status === 'health-check' ? (
          <BlocoHealthCheck itens={tentativa.health_check} />
        ) : (
          <>
            <CartaoTempo rotulo="Tempo da execução" tempoS={tempoS} detalhe="contado desde Nova execução · min:s" />
            <div className="grade grade--3">
              <CartaoMetrica
                rotulo="Velocidade"
                valor={ultima ? formatarNumero(ultima.velocidade, 3) : '—'}
                unidade="m/s"
                detalhe={`${formatarNumero(distanciaM, 2)} m percorridos`}
              />
              <CartaoMetrica
                rotulo="Bateria"
                valor={ultima ? formatarNumero(ultima.bateria, 2) : '—'}
                unidade="V"
                detalhe="tensão informada pelo robô"
              />
              <CartaoMetrica
                rotulo="Célula atual"
                valor={ultima ? nomeCelula(ultima) : '—'}
                unidade="coluna · linha"
                detalhe={`${celulasVisitadas} de ${totalCelulas} células já visitadas`}
              />
            </div>
          </>
        )}
        {execucao.leituras.length > 0 && <TabelaLeituras leituras={execucao.leituras} largadaEm={tentativa.iniciada_em} />}
      </div>

      <div className="coluna">
        <section className="cartao">
          <h2 className="cartao__titulo">Trajeto</h2>
          <VisualizacaoLabirinto
            largura={labirinto.largura}
            altura={labirinto.altura}
            trajeto={execucao.trajeto}
            celulaAtual={ultima ?? null}
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
  )
}
