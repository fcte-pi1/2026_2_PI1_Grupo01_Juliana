import { useEffect, useState } from 'react'
import { api } from '../api/clienteApi'

/** Horário atual, atualizado a cada `intervaloMs` (para "atualizado há X s"). */
export function useAgora(intervaloMs = 1000): number {
  const [agora, setAgora] = useState(() => api.agora())
  useEffect(() => {
    const id = setInterval(() => setAgora(api.agora()), intervaloMs)
    return () => clearInterval(id)
  }, [intervaloMs])
  return agora
}
