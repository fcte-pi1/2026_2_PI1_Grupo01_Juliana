import { useEffect, useState } from 'react'

/** Horário atual, atualizado a cada `intervaloMs` (para "atualizado há X s"). */
export function useAgora(intervaloMs = 1000): number {
  const [agora, setAgora] = useState(() => Date.now())
  useEffect(() => {
    const id = setInterval(() => setAgora(Date.now()), intervaloMs)
    return () => clearInterval(id)
  }, [intervaloMs])
  return agora
}
