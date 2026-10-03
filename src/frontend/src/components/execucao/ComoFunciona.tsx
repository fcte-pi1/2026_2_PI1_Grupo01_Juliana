const PASSOS = [
  { titulo: 'Nova execução', texto: 'Você escolhe o labirinto e cria a execução aqui.' },
  { titulo: 'Health-check', texto: 'Ligue o robô em A1. Ele testa bateria, sensores ToF, motores e encoders e envia o resultado.' },
  { titulo: 'Em execução', texto: 'A largada é automática. A tela mostra tempo, velocidade, bateria e célula atual.' },
  { titulo: 'Resultado', texto: 'No fim, o trajeto aparece no mapa. Se falhar, dá para retomar do checkpoint (até 3 tentativas).' },
]

export function ComoFunciona() {
  return (
    <section className="cartao">
      <h2 className="cartao__titulo">Como funciona</h2>
      <ol className="como-funciona">
        {PASSOS.map((passo, i) => (
          <li key={passo.titulo}>
            <span className="como-funciona__numero">{i + 1}</span>
            <div>
              <strong>{passo.titulo}</strong>
              <p className="texto-suave">{passo.texto}</p>
            </div>
          </li>
        ))}
      </ol>
    </section>
  )
}
