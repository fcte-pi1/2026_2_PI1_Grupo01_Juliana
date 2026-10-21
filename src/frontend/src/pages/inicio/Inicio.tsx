import { useEffect, useRef, useState } from 'react'
import { Link, useSearchParams } from 'react-router'
import { ErroApi } from '../../api/clienteApi'
import { buscarExecucao, criarExecucao, encerrarExecucao, encerrarTentativa, retomarTentativa } from '../../api/endpoints'
import { combinarRetrato } from '../../api/eventosExecucao'
import type { EncerrarPedido, Execucao, Labirinto, Recusa, RetornoNovaExecucao, StatusExecucao, TipoLabirinto } from '../../api/tipos'
import { CartaoUltimaExecucao } from '../../components/execucao/CartaoUltimaExecucao'
import { ComoFunciona } from '../../components/execucao/ComoFunciona'
import { EtapasExecucao } from '../../components/execucao/EtapasExecucao'
import { FormNovaExecucao, SeletorLabirinto } from '../../components/execucao/FormNovaExecucao'
import { PilulaStatusTentativa } from '../../components/execucao/PilulaResultado'
import { ResumoExecucao } from '../../components/execucao/ResumoExecucao'
import { Cabecalho } from '../../components/layout/Cabecalho'
import { deduzirEixo, dimensoes as dimensoesDoMapa, eixoParaGrade } from '../../components/labirinto/trajeto'
import { UltimaAtualizacao } from '../../components/telemetria/UltimaAtualizacao'
import { VerificacaoHealthCheck } from '../../components/telemetria/VerificacaoHealthCheck'
import { AvisoRecusa } from '../../components/tentativas/AvisoRecusa'
import { ControleTentativas } from '../../components/tentativas/ControleTentativas'
import { ContadorTentativas } from '../../components/tentativas/ContadorTentativas'
import { Modal } from '../../components/tentativas/Modal'
import { ModalEncerrar } from '../../components/tentativas/ModalEncerrar'
import { ModalEncerrarExecucao } from '../../components/tentativas/ModalEncerrarExecucao'
import { ModalRetomar } from '../../components/tentativas/ModalRetomar'
import { useAgora } from '../../hooks/useAgora'
import { useExecucaoAtual } from '../../hooks/useExecucaoAtual'
import { useLabirintos } from '../../hooks/useLabirintos'
import { useHealthCheck } from '../../hooks/useHealthCheck'
import { tentativaAberta } from '../../utils/tentativas'
import { formatarNumeroExecucao, formatarTempo, nomeCelula, ROTULO_RECUSA } from '../../utils/formatacao'
import { derivarEstadoInicio, segundosSemDados, tentativaAtual, type EstadoInicio } from './estadoInicio'
import { PainelAoVivo } from './PainelAoVivo'
import { PainelEncerrada } from './PainelEncerrada'

type TipoModal = 'nova' | 'encerrar' | 'retomar' | 'encerrar-execucao' | null

function detalheTexto(corpo: unknown): string | null {
  if (!corpo || typeof corpo !== 'object' || !('detail' in corpo)) return null
  const { detail } = corpo as { detail: unknown }
  return typeof detail === 'string' && detail.length > 0 ? detail : null
}

function lerRecusa(corpo: unknown): Recusa | null {
  if (!corpo || typeof corpo !== 'object' || !('motivo' in corpo) || !('ocorrida_em' in corpo)) return null
  const { motivo, ocorrida_em } = corpo
  if (typeof ocorrida_em !== 'string') return null
  if (typeof motivo !== 'string' || !Object.hasOwn(ROTULO_RECUSA, motivo)) return null
  return { motivo: motivo as Recusa['motivo'], ocorrida_em }
}

const TITULO: Record<StatusExecucao, string> = {
  em_andamento: 'Execução ativa',
  concluida: 'Execução concluída',
  cancelada: 'Execução cancelada',
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
  const envioEmAndamento = useRef(false)
  const [erroAcao, setErroAcao] = useState<string | null>(null)
  const [criacaoPendente, setCriacaoPendente] = useState<RetornoNovaExecucao | null>(null)
  const [retomadaPendente, setRetomadaPendente] = useState<RetornoNovaExecucao | null>(null)
  const [tentativaCriada, setTentativaCriada] = useState<RetornoNovaExecucao | null>(null)
  const encerradaId = atual.dados?.execucao && atual.dados.execucao.status !== 'em_andamento'
    ? atual.dados.execucao.execucao_id : null

  useEffect(() => {
    if (atual.carregando || !encerradaId || parametros.get('execucao') === encerradaId) return
    setParametros((p) => { p.set('execucao', encerradaId); return p }, { replace: true })
  }, [atual.carregando, encerradaId, parametros, setParametros])

  const modal = (parametros.get('modal') as TipoModal) ?? null
  const abrirModal = (novo: TipoModal) =>
    setParametros(
      (p) => {
        if (novo) p.set('modal', novo)
        else p.delete('modal')
        return p
      },
      { replace: true },
    )

  const recebida = atual.dados?.execucao
  const temTentativaAberta = !!recebida && tentativaAberta(recebida) !== null
  const tipoVerificado = modal === 'nova' || !recebida ? selecionado : recebida.labirinto
  const healthCheck = useHealthCheck(tipoVerificado, !temTentativaAberta || modal === 'nova')
  const verificacao = <VerificacaoHealthCheck verificacao={healthCheck} tipo={tipoVerificado}
    bloqueado={enviando || temTentativaAberta} tentativaAberta={temTentativaAberta} />

  async function enviar(acao: () => Promise<unknown>, novaExecucao = false) {
    if (envioEmAndamento.current) return
    envioEmAndamento.current = true
    setEnviando(true)
    try {
      await acao()
      setRecusaLocal(null)
      setErroAcao(null)
      setParametros((p) => {
        p.delete('modal')
        // A criação já carregou o detalhe novo; abandona a seleção do resultado anterior.
        if (novaExecucao) p.delete('execucao')
        return p
      }, { replace: true })
    } catch (e) {
      const recusa = e instanceof ErroApi && e.status === 409 ? lerRecusa(e.corpo) : null
      if (recusa) {
        setRecusaLocal(recusa)
        setErroAcao(null)
      } else {
        setRecusaLocal(null)
        const detalhe = e instanceof ErroApi ? detalheTexto(e.corpo) : null
        if (detalhe) setErroAcao(detalhe)
        else if (e instanceof ErroApi && e.status === 409) {
          setErroAcao('O servidor recusou o pedido, mas não informou um motivo válido.')
        } else setErroAcao(e instanceof Error ? e.message : String(e))
      }
    } finally {
      envioEmAndamento.current = false
      setEnviando(false)
    }
  }

  function fecharRecusa() {
    setRecusaLocal(null)
    atual.definirDados((dados) => dados ? { ...dados, recusa: null } : dados)
  }

  function atualizarExecucao(retrato: Execucao) {
    atual.definirDados((dados) => dados?.execucao?.execucao_id === retrato.execucao_id
      ? { ...dados, execucao: combinarRetrato(dados.execucao, retrato), recusa: null } : dados)
  }

  async function registrarEncerramento(id: string, pedido: EncerrarPedido) {
    // O HTTP 200 já contém o estado persistido, mesmo se o SSE estiver reconectando.
    atualizarExecucao(await encerrarTentativa(id, pedido))
  }

  async function registrarRetomada(id: string) {
    let registrada = retomadaPendente
    if (!registrada) {
      registrada = await retomarTentativa(id)
      setRetomadaPendente(registrada)
    }
    let retrato: Execucao
    try {
      retrato = await buscarExecucao(registrada.execucao_id)
      if (!retrato.tentativas.some((tentativa) => tentativa.tentativa_id === registrada.tentativa_id)) {
        throw new Error('A tentativa registrada ainda não está disponível na consulta.')
      }
    } catch {
      throw new Error('A tentativa foi aberta, mas não foi possível carregar seus dados. Use “Carregar dados da tentativa” para tentar novamente.')
    }
    atualizarExecucao(retrato)
    setRetomadaPendente(null)
  }

  async function iniciarNovaExecucao() {
    let criada = criacaoPendente
    if (!criada) {
      criada = await criarExecucao({ tipo_labirinto: selecionado })
      setCriacaoPendente(criada)
      setTentativaCriada(criada)
    }
    let nova: Execucao
    try {
      nova = await buscarExecucao(criada.execucao_id)
    } catch {
      throw new Error('A execução foi criada, mas não foi possível carregar seus dados. Use “Carregar dados da execução” para tentar novamente.')
    }
    atual.definirDados((dados) => dados ? { ...dados, execucao: nova, recusa: null } : dados)
    setCriacaoPendente(null)
  }

  const registroPendente = retomadaPendente ?? criacaoPendente
  if (registroPendente) {
    return (
      <>
        <Cabecalho trilha="Início" titulo={retomadaPendente ? 'Retomada registrada' : 'Nova execução registrada'} acoes={
          <button type="button" className="botao botao--primario" disabled={enviando}
            onClick={() => retomadaPendente ? enviar(() => registrarRetomada(retomadaPendente.execucao_id)) : enviar(iniciarNovaExecucao, true)}>
            {retomadaPendente ? 'Carregar dados da tentativa' : 'Carregar dados da execução'}
          </button>
        } />
        {erroAcao && <div className="faixa faixa--perigo" role="alert">{erroAcao}</div>}
        <div className="conteudo">
          <ContadorTentativas atual={registroPendente.attempt_index} />
          <p className="estado-vazio">Aguardando os dados da execução {registroPendente.execucao_id}.</p>
        </div>
      </>
    )
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
  const titulo = execucao ? TITULO[execucao.status] : 'Nenhuma execução ativa'
  const recusa = recusaLocal ?? atual.dados.recusa
  const estado = derivarEstadoInicio(execucao, agora)
  const faixaErro = erroAcao && (
    <div className="faixa faixa--perigo" role="alert">
      Não foi possível concluir a ação: {erroAcao}
    </div>
  )
  const faixaSincronizacao = atual.erroSincronizacao && (
    <div className="faixa faixa--alerta" role="status">
      {atual.erroSincronizacao}
      <button type="button" className="botao faixa__acao" onClick={atual.sincronizar}>Atualizar dados</button>
    </div>
  )
  const novaExecucao = (
    <button
      type="button"
      className="botao botao--primario"
      disabled={enviando || (!execucao && !healthCheck.apto)}
      onClick={() => execucao ? abrirModal('nova') : enviar(iniciarNovaExecucao, true)}
    >
      + Nova execução
    </button>
  )

  if (!execucao) {
    return (
      <>
        <Cabecalho trilha="Início" titulo={titulo} acoes={novaExecucao} />
        {faixaErro}
        {recusa && <AvisoRecusa motivo={recusa.motivo} titulo="Nova execução recusada" aoFechar={fecharRecusa} />}
        <div className="conteudo pagina-duas-colunas">
          <FormNovaExecucao
            labirintos={labirintos.dados ?? []}
            selecionado={selecionado}
            proximoNumero={proximo_numero}
            aoSelecionar={setSelecionado}
            desabilitado={enviando}
            verificacao={verificacao}
          />
          <div className="coluna">
            <ComoFunciona />
            {ultima && <CartaoUltimaExecucao execucao={ultima} />}
          </div>
        </div>
      </>
    )
  }

  const numero = formatarNumeroExecucao(execucao.numero, execucao.execucao_id)
  const modalNovaExecucao = modal === 'nova' && (
    <Modal
      titulo="Nova execução"
      aoFechar={() => { if (!enviando) abrirModal(null) }}
      rodape={
        <>
          <button type="button" className="botao" disabled={enviando} onClick={() => abrirModal(null)}>Cancelar</button>
          <button type="button" className="botao botao--primario" disabled={enviando || !healthCheck.apto} onClick={() => enviar(iniciarNovaExecucao, true)}>
            {enviando ? 'Iniciando…' : 'Confirmar nova execução'}
          </button>
        </>
      }
    >
      {execucao.status === 'em_andamento'
        ? <p>A execução <strong>{numero}</strong> está em andamento. Ao confirmar, ela será <strong>cancelada</strong> e uma nova execução começará na <strong>Tentativa 1/3</strong>. Se o robô ainda estiver andando, pare-o pelo botão BOOT.</p>
        : <p>Escolha o labirinto. A nova execução começará na <strong>Tentativa 1/3</strong>.</p>}
      <SeletorLabirinto labirintos={labirintos.dados ?? []} selecionado={selecionado} aoSelecionar={setSelecionado} desabilitado={enviando} />
      {verificacao}
      {recusaLocal && <AvisoRecusa motivo={recusaLocal.motivo} titulo="Nova execução recusada" aoFechar={fecharRecusa} />}
      {faixaErro}
    </Modal>
  )
  const tentativa = tentativaAtual(execucao)
  const botaoEncerrarExecucao = execucao.status === 'em_andamento' && (
    <button type="button" className="botao botao--perigo" disabled={enviando} onClick={() => abrirModal('encerrar-execucao')}>
      Encerrar execução
    </button>
  )
  const modalEncerrarExecucao = modal === 'encerrar-execucao' && execucao.status === 'em_andamento' && (
    <ModalEncerrarExecucao
      numeroExecucao={numero}
      attemptIndex={tentativa?.attempt_index ?? null}
      tentativaAberta={tentativa?.status === 'running'}
      enviando={enviando}
      aoCancelar={() => abrirModal(null)}
      aoConfirmar={() => enviar(async () => atualizarExecucao(await encerrarExecucao(execucao.execucao_id)))}
    />
  )
  if (!tentativa) {
    return (
      <>
        <Cabecalho trilha="Início" titulo={`${titulo} ${numero}`} acoes={<>{novaExecucao}{botaoEncerrarExecucao}</>} />
        {modal !== 'nova' && faixaErro}
        {faixaSincronizacao}
        <div className="conteudo">
          {tentativaCriada?.execucao_id === execucao.execucao_id && <ContadorTentativas atual={tentativaCriada.attempt_index} />}
          <p className="estado-vazio">Aguardando os dados da tentativa enviados pelo backend.</p>
        </div>
        {modalNovaExecucao}
        {modalEncerrarExecucao}
      </>
    )
  }
  const labirinto = dimensoes(execucao.labirinto, labirintos.dados ?? [])
  const eixo = eixoParaGrade(
    labirinto.largura,
    labirinto.altura,
    execucao.eixo_longo ?? deduzirEixo(execucao.trajeto, Math.min(labirinto.largura, labirinto.altura)),
  )
  const grade = dimensoesDoMapa(labirinto.largura, labirinto.altura, eixo)
  const objetivo = nomeCelula({ x: grade.colunas - 1, y: grade.linhas - 1 })
  const aberta = execucao.status === 'em_andamento' && tentativa.status === 'running'
  const { falha } = tentativa
  // Após uma falha com retomada disponível, só a tentativa terminou; a execução segue ativa.
  const trilha = aberta ? 'execução ativa' : execucao.status === 'em_andamento' ? 'tentativa encerrada' : 'execução encerrada'
  const celulaFalha = falha ? { x: falha.celula_x, y: falha.celula_y } : null
  const celulaAtual = execucao.celula_atual ?? execucao.leituras.at(-1) ?? execucao.trajeto.at(-1)

  return (
    <>
      <Cabecalho
        trilha={`Início · ${trilha}`}
        titulo={`${titulo} ${numero}`}
        acoes={
          <>
            {aberta && <UltimaAtualizacao segundos={segundosSemDados(execucao, agora)} />}
            <PilulaStatusTentativa status={tentativa.status} />
            {novaExecucao}
            {!aberta && (
              <Link to={`/execucoes/${execucao.execucao_id}`} className="botao">
                Ver detalhes
              </Link>
            )}
            {botaoEncerrarExecucao}
            {!aberta && (
              <>
                {execucao.status !== 'em_andamento' && <Link to="/" className="botao" onClick={() => atual.recarregar()}>Voltar ao início</Link>}
              </>
            )}
            <ControleTentativas execucao={execucao} enviando={enviando}
              aoEncerrar={() => abrirModal('encerrar')} aoRetomar={() => abrirModal('retomar')} />
          </>
        }
      />

      {modal !== 'nova' && faixaErro}
      {faixaSincronizacao}
      <FaixaEstado estado={estado} execucao={execucao} agora={agora} />

      <div className="conteudo execucao-tempo-real">
        <ResumoExecucao
          numero={numero}
          labirinto={execucao.labirinto}
          attemptIndex={tentativa.attempt_index}
          criadaEm={execucao.iniciada_em}
          largada={tentativa.tipo_inicio === 'retomada' ? 'Retomada' : 'A1'}
          objetivo={objetivo}
        />
        {recusa && modal !== 'nova' && <AvisoRecusa motivo={recusa.motivo} titulo={recusaLocal ? 'Pedido recusado' : undefined} aoFechar={fecharRecusa} />}
        {aberta && <EtapasExecucao
          status={tentativa.status}
          healthCheckAprovados={tentativa.health_check.filter((i) => i.aprovado).length}
          healthCheckReprovados={tentativa.health_check.filter((i) => i.aprovado === false).length}
          healthCheckTotal={tentativa.health_check.length}
        />}
        {aberta ? (
          <PainelAoVivo execucao={execucao} tentativa={tentativa} labirinto={labirinto} agora={agora} />
        ) : (
          <PainelEncerrada execucao={execucao} tentativa={tentativa} labirinto={labirinto} agora={agora} enviando={enviando} aoRetomar={() => abrirModal('retomar')} />
        )}
      </div>

      {modalNovaExecucao}
      {modalEncerrarExecucao}
      {modal === 'encerrar' && aberta && (
        <ModalEncerrar
          numeroExecucao={numero}
          attemptIndex={tentativa.attempt_index}
          ultimaCelula={celulaAtual ? nomeCelula(celulaAtual) : '—'}
          tempo={formatarTempo(Math.max((agora - new Date(execucao.iniciada_em).getTime()) / 1000, 0))}
          enviando={enviando}
          aoCancelar={() => abrirModal(null)}
          aoConfirmar={(pedido: EncerrarPedido) => enviar(() => registrarEncerramento(execucao.execucao_id, pedido))}
        />
      )}
      {modal === 'retomar' && estado === 'falha-retomavel' && celulaFalha && (
        <ModalRetomar
          numeroExecucao={numero}
          proxima={tentativa.attempt_index + 1}
          celulaFalha={nomeCelula(celulaFalha)}
          enviando={enviando}
          liberado={healthCheck.apto}
          verificacao={verificacao}
          aoCancelar={() => abrirModal(null)}
          aoConfirmar={() => enviar(() => registrarRetomada(execucao.execucao_id))}
        />
      )}
    </>
  )
}

function FaixaEstado({ estado, execucao, agora }: { estado: EstadoInicio; execucao: Execucao; agora: number }) {
  const tentativa = tentativaAtual(execucao)!
  const numero = formatarNumeroExecucao(execucao.numero, execucao.execucao_id)
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
      if (execucao.status === 'cancelada') {
        return (
          <div className="faixa faixa--perigo" role="status">
            Execução {numero} cancelada. Sem retomada: esta execução não aceita novas tentativas.
            <Link to="/execucoes" className="faixa__acao">Ver no histórico →</Link>
          </div>
        )
      }
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
