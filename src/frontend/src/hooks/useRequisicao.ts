import { useCallback, useEffect, useState } from 'react'

export interface Requisicao<T> {
  dados: T | null
  erro: Error | null
  carregando: boolean
  recarregar: () => void
  /** Troca os dados localmente (ex.: ao receber um evento SSE). */
  definirDados: (atualizar: (atual: T | null) => T | null) => void
}

/** Busca dados ao montar e sempre que `chave` mudar; cancela a busca anterior. */
export function useRequisicao<T>(buscar: (sinal: AbortSignal) => Promise<T>, chave: string): Requisicao<T> {
  const [dados, setDados] = useState<T | null>(null)
  const [erro, setErro] = useState<Error | null>(null)
  const [versao, setVersao] = useState(0)
  const [concluida, setConcluida] = useState<string | null>(null)
  const atual = `${chave}#${versao}`

  useEffect(() => {
    const controle = new AbortController()
    buscar(controle.signal)
      .then((resultado) => {
        setDados(resultado)
        setErro(null)
      })
      .catch((e: unknown) => {
        if (!controle.signal.aborted) setErro(e instanceof Error ? e : new Error(String(e)))
      })
      .finally(() => {
        if (!controle.signal.aborted) setConcluida(atual)
      })
    return () => controle.abort()
    // `buscar` muda a cada render; quem decide quando buscar de novo é a chave.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [atual])

  const recarregar = useCallback(() => setVersao((v) => v + 1), [])

  return { dados, erro, carregando: concluida !== atual, recarregar, definirDados: setDados }
}
