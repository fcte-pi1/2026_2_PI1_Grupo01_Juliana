import { Link } from 'react-router'
import { Cabecalho } from '../components/layout/Cabecalho'

export function NaoEncontrada() {
  return (
    <>
      <Cabecalho trilha="Erro 404" titulo="Página não encontrada" />
      <div className="conteudo">
        <p className="estado-vazio">
          Esse endereço não existe. <Link to="/">Voltar ao início</Link>
        </p>
      </div>
    </>
  )
}
