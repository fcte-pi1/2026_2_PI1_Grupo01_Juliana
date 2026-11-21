export type EstadoReceptor = 'conectado' | 'aberto' | 'reconectando' | 'sem-ponte' | 'desconectado' | 'desconhecido'

const TEXTO: Record<EstadoReceptor, string> = {
  conectado: 'Recebendo sinal',
  aberto: 'Porta aberta',
  reconectando: 'Reconectando…',
  'sem-ponte': 'Ponte desligada',
  desconectado: 'Sem sinal recente',
  desconhecido: 'Sem informação',
}

interface StatusReceptorProps {
  estado: EstadoReceptor
  segundosSemSinal?: number | null
  /** Quando informado, mostra o botão que pede à ponte para reabrir a porta do robô. */
  aoReconectar?: () => void
  reconectando?: boolean
  erro?: string | null
}

function descreverAtraso(segundos: number): string {
  if (segundos < 60) return `${Math.floor(segundos)} s`
  if (segundos < 3600) return `${Math.floor(segundos / 60)} min`
  return `${Math.floor(segundos / 3600)} h`
}

function detalhe(estado: EstadoReceptor, segundosSemSinal: number | null): string {
  if (estado === 'sem-ponte') return 'ligue a ponte: scripts/ambiente.sh'
  if (segundosSemSinal === null) return 'ESP32 · Bluetooth clássico SPP'
  return `último sinal há ${descreverAtraso(segundosSemSinal)}`
}

export function StatusReceptor({ estado, segundosSemSinal = null, aoReconectar, reconectando = false, erro = null }: StatusReceptorProps) {
  const podeReconectar = aoReconectar !== undefined && estado !== 'sem-ponte' && estado !== 'desconhecido'
  return (
    <div className="receptor">
      <strong>Receptor Bluetooth</strong>
      <span className={`receptor__estado receptor__estado--${estado}`}>{TEXTO[estado]}</span>
      <span className="receptor__detalhe mono">{detalhe(estado, segundosSemSinal)}</span>
      {podeReconectar && (
        <button type="button" className="receptor__botao" disabled={reconectando || estado === 'reconectando'} onClick={aoReconectar}>
          {reconectando || estado === 'reconectando' ? 'Reconectando…' : 'Reconectar'}
        </button>
      )}
      {erro && <span role="alert" className="receptor__erro">{erro}</span>}
    </div>
  )
}
