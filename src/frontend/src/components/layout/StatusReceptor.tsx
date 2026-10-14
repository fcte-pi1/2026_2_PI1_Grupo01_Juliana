export type EstadoReceptor = 'conectado' | 'desconectado' | 'desconhecido'

const TEXTO: Record<EstadoReceptor, string> = {
  conectado: 'Conectado',
  desconectado: 'Desconectado',
  desconhecido: 'Sem informação',
}

interface StatusReceptorProps {
  estado: EstadoReceptor
}

export function StatusReceptor({ estado }: StatusReceptorProps) {
  return (
    <div className="receptor">
      <strong>Receptor Bluetooth</strong>
      <span className={`receptor__estado receptor__estado--${estado}`}>{TEXTO[estado]}</span>
      <span className="receptor__detalhe mono">ESP32 · Bluetooth clássico SPP</span>
    </div>
  )
}
