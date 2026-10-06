import { useEffect, type ReactNode } from 'react'

interface ModalProps {
  titulo: ReactNode
  aoFechar: () => void
  children: ReactNode
  rodape: ReactNode
}

/** Base dos modais de Encerrar e Retomar tentativa: fundo escuro, Esc fecha. */
export function Modal({ titulo, aoFechar, children, rodape }: ModalProps) {
  useEffect(() => {
    const aoTeclar = (e: KeyboardEvent) => {
      if (e.key === 'Escape') aoFechar()
    }
    window.addEventListener('keydown', aoTeclar)
    return () => window.removeEventListener('keydown', aoTeclar)
  }, [aoFechar])

  return (
    <div className="modal__fundo" onClick={aoFechar}>
      <div className="modal" role="dialog" aria-modal="true" aria-labelledby="modal-titulo" onClick={(e) => e.stopPropagation()}>
        <h2 id="modal-titulo" className="modal__titulo">
          {titulo}
        </h2>
        <div className="modal__corpo">{children}</div>
        <div className="modal__rodape">{rodape}</div>
      </div>
    </div>
  )
}
