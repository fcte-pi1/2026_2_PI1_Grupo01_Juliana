import { useState } from 'react'
import { Link, useSearchParams } from 'react-router'
import { ErroApi } from '../../api/clienteApi'
import { criarExecucao, encerrarTentativa, retomarTentativa } from '../../api/endpoints'
import type { EncerrarPedido, Execucao, Labirinto, Recusa, TipoLabirinto } from '../../api/tipos'
import { CartaoUltimaExecucao } from '../../components/execucao/CartaoUltimaExecucao'
import { ComoFunciona } from '../../components/execucao/ComoFunciona'
import { EtapasExecucao } from '../../components/execucao/EtapasExecucao'
import { FormNovaExecucao } from '../../components/execucao/FormNovaExecucao'
import { PilulaStatusTentativa } from '../../components/execucao/PilulaResultado'
import { ResumoExecucao } from '../../components/execucao/ResumoExecucao'
import { Cabecalho } from '../../components/layout/Cabecalho'
import { UltimaAtualizacao } from '../../components/telemetria/UltimaAtualizacao'
import { AvisoRecusa } from '../../components/tentativas/AvisoRecusa'
import { BotaoRetomar } from '../../components/tentativas/BotaoRetomar'
import { ModalEncerrar } from '../../components/tentativas/ModalEncerrar'
import { ModalRetomar } from '../../components/tentativas/ModalRetomar'
import { useAgora } from '../../hooks/useAgora'
import { useExecucaoAtual } from '../../hooks/useExecucaoAtual'
import { useLabirintos } from '../../hooks/useLabirintos'
import { formatarNumeroExecucao, formatarTempo, nomeCelula } from '../../utils/formatacao'
import { derivarEstadoInicio, segundosSemDados, tentativaAtual, type EstadoInicio } from './estadoInicio'
import { PainelAoVivo } from './PainelAoVivo'
import { PainelEncerrada } from './PainelEncerrada'

type Modal = 'encerrar' | 'retomar' | null

const TITULO: Record<EstadoInicio, string> = {
  'sem-execucao': 'Nenhuma execução ativa',
  'health-check': 'Execução ativa',
  'em-execucao': 'Execução ativa',
  'sem-comunicacao': 'Execução ativa',
  concluida: 'Execução concluída',
  'falha-retomavel': 'Execução encerrada',
  'falha-sem-retomada': 'Execução encerrada',
}

function dimensoes(tipo: TipoLabirinto, labirintos: Labirinto[]): Labirinto {
  const [largura, altura] = tipo.split('x').map(Number)
  return labirintos.find((l) => l.tipo === tipo) ?? { tipo, largura, altura, melhor_tempo_s: null, melhor_execucao_numero: null }
}

/** Rota `/`: Nova execução, execução em andamento e encerramento (FRONT-02 a FRONT-05). */
export function Inicio() {
  const [parametros, setParametros] = useSearchParams()
  const atual = useExecucaoAtual()
  const labirintos = useLabirintos()
  const agora = useAgora()
  const [selecionado, setSelecionado] = useState<TipoLabirinto>('4x4')
  const [recusaLocal, setRecusaLocal] = useState<Recusa | null>(null)
  const [enviando, setEnviando] = useState(false)
  const [erroAcao, setErroAcao] = useState<string | null>(null)

  const modal = (parametros.get('modal') as Modal) ?? null
  const abrirModal = (novo: Modal) =>
    setParametros(
      (p) => {
        if (novo) p.set('modal', novo)
        else p.delete('modal')
        return p
      },
      { replace: true },
    )

  async function enviar(acao: () => Promise<unknown>) {
    setEnviando(true)
    try {
      await acao()
      setRecusaLocal(null)
      setErroAcao(null)
    } catch (e) {
      if (e instanceof ErroApi && e.status === 409) setRecusaLocal(e.corpo as Recusa)
      else setErroAcao(e instanceof Error ? e.message : String(e))
    } finally {
      setEnviando(false)
      abrirModal(null)
      atual.recarregar()
    }
  }

  if (atual.erro) {
    return (
      <>
        <Cabecalho trilha="Início" titulo="Sem conexão com o backend" />
        <p className="estado-vazio">Não foi possível buscar a execução atual: {atual.erro.message}</p>
      </>
    )
  }
  if (!atual.dados) return <p className="estado-vazio">Carregando…</p>

  const { execucao, ultima, proximo_numero } = atual.dados
  const recusa = recusaLocal ?? atual.dados.recusa
  const estado = derivarEstadoInicio(execucao, agora)
  const faixaErro = erroAcao && (
    <div className="faixa faixa--perigo" role="alert">
      Não foi possível concluir a ação: {erroAcao}
    </div>
  )
  const novaExecucao = (
    <button type="button" className="botao botao--primario" disabled={enviando} onClick={() => enviar(() => criarExecucao({ labirinto: selecionado }))}>
      + Nova execução
    </button>
  )

  if (!execucao) {
    return (
      <>
        <Cabecalho trilha="Início" titulo={TITULO[estado]} acoes={novaExecucao} />
        {faixaErro}
        <div className="conteudo pagina-duas-colunas">
          <FormNovaExecucao
            labirintos={labirintos.dados ?? []}
            selecionado={selecionado}
            proximoNumero={proximo_numero}
            aoSelecionar={setSelecionado}
          />
          <div className="coluna">
            <ComoFunciona />
            {ultima && <CartaoUltimaExecucao execucao={ultima} />}
          </div>
        </div>
      </>
    )
  }

  const tentativa = tentativaAtual(execucao)!
  const numero = formatarNumeroExecucao(execucao.numero)
  const labirinto = dimensoes(execucao.labirinto, labirintos.dados ?? [])
  const aberta = tentativa.status === 'health-check' || tentativa.status === 'running'
  const { falha } = tentativa
  const celulaFalha = falha ? { x: falha.celula_x, y: falha.celula_y } : null
  const ultimaLeitura = execucao.leituras.at(-1)

  return (
    <>
      <Cabecalho
        trilha={`Início · ${aberta ? 'execução ativa' : 'execução encerrada'}`}
        titulo={`${TITULO[estado]} ${numero}`}
        acoes={
          <>
            {aberta && <UltimaAtualizacao segundos={segundosSemDados(execucao, agora)} />}
            <PilulaStatusTentativa status={tentativa.status} />
            {aberta ? (
              <button type="button" className="botao botao--perigo-contorno" onClick={() => abrirModal('encerrar')}>
                Encerrar tentativa
              </button>
            ) : (
              <>
                <Link to={`/execucoes/${execucao.execucao_id}`} className="botao">
                  Ver detalhes
                </Link>
                {estado === 'falha-retomavel' ? (
                  <BotaoRetomar proxima={tentativa.attempt_index + 1} aoClicar={() => abrirModal('retomar')} />
                ) : (
                  novaExecucao
                )}
              </>
            )}
          </>
        }
      />

      {faixaErro}
      <FaixaEstado estado={estado} execucao={execucao} agora={agora} />

      <div className="conteudo">
        <ResumoExecucao
          numero={numero}
          labirinto={execucao.labirinto}
          attemptIndex={tentativa.attempt_index}
          criadaEm={execucao.iniciada_em}
          largada={tentativa.tipo_inicio === 'retomada' ? 'Retomada' : 'A1'}
          objetivo={nomeCelula({ x: labirinto.largura - 1, y: labirinto.altura - 1 })}
        />
        {recusa && <AvisoRecusa motivo={recusa.motivo} numeroExecucao={numero} aoFechar={() => setRecusaLocal(null)} />}
        <EtapasExecucao
          status={tentativa.status}
          healthCheckAprovados={tentativa.health_check.filter((i) => i.aprovado).length}
          healthCheckTotal={tentativa.health_check.length}
        />
        {aberta ? (
          <PainelAoVivo execucao={execucao} tentativa={tentativa} labirinto={labirinto} agora={agora} />
        ) : (
          <PainelEncerrada execucao={execucao} tentativa={tentativa} labirinto={labirinto} />
        )}
      </div>

      {modal === 'encerrar' && aberta && (
        <ModalEncerrar
          numeroExecucao={numero}
          attemptIndex={tentativa.attempt_index}
          ultimaCelula={ultimaLeitura ? nomeCelula(ultimaLeitura) : '—'}
          tempo={formatarTempo(Math.max((agora - new Date(execucao.iniciada_em).getTime()) / 1000, 0))}
          enviando={enviando}
          aoCancelar={() => abrirModal(null)}
          aoConfirmar={(pedido: EncerrarPedido) => enviar(() => encerrarTentativa(execucao.execucao_id, pedido))}
        />
      )}
      {modal === 'retomar' && estado === 'falha-retomavel' && celulaFalha && (
        <ModalRetomar
          numeroExecucao={numero}
          proxima={tentativa.attempt_index + 1}
          celulaFalha={nomeCelula(celulaFalha)}
          enviando={enviando}
          aoCancelar={() => abrirModal(null)}
          aoConfirmar={() => enviar(() => retomarTentativa(execucao.execucao_id))}
        />
      )}
    </>
  )
}

function FaixaEstado({ estado, execucao, agora }: { estado: EstadoInicio; execucao: Execucao; agora: number }) {
  const tentativa = tentativaAtual(execucao)!
  const numero = formatarNumeroExecucao(execucao.numero)
  switch (estado) {
    case 'sem-comunicacao':
      return (
        <div className="faixa faixa--alerta" role="status">
          Sem dados do robô há {Math.floor(segundosSemDados(execucao, agora) ?? 0)} s. Confira o receptor Bluetooth; se a
          comunicação não voltar, a tentativa é encerrada por link_lost.
        </div>
      )
    case 'concluida':
      return (
        <div className="faixa faixa--sucesso" role="status">
          Execução {numero} concluída na tentativa {tentativa.attempt_index}
          {tentativa.tipo_descoberto && `: o robô identificou o labirinto ${tentativa.tipo_descoberto}`}.
          <Link to="/execucoes" className="faixa__acao">
            Ver no histórico →
          </Link>
        </div>
      )
    case 'falha-retomavel':
    case 'falha-sem-retomada':
      return (
        <div className="faixa faixa--perigo" role="status">
          Tentativa {tentativa.attempt_index} registrada como Falha
          {tentativa.falha && ` (${tentativa.falha.motivo} em ${nomeCelula({ x: tentativa.falha.celula_x, y: tentativa.falha.celula_y })})`}.{' '}
          {estado === 'falha-retomavel'
            ? 'Pode ser retomada da célula da falha.'
            : 'Sem retomada: as 3 tentativas foram usadas ou a execução foi cancelada.'}
          <Link to="/execucoes" className="faixa__acao">
            Ver no histórico →
          </Link>
        </div>
      )
    default:
      return null
  }
}
