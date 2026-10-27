/*
 * Setas para recolher o menu da esquerda e o índice da direita, deixando mais
 * espaço para o texto. A escolha fica salva no navegador de quem lê.
 */

const LATERAIS = [
  { lado: "nav", seletor: ".md-sidebar--primary", rotulo: "menu", aberta: "‹", fechada: "›" },
  { lado: "toc", seletor: ".md-sidebar--secondary", rotulo: "índice", aberta: "›", fechada: "‹" },
]

const lerEscolha = (lado) => {
  try { return localStorage.getItem(`laterais-sem-${lado}`) === "1" } catch { return false }
}

const salvarEscolha = (lado, recolhida) => {
  try { localStorage.setItem(`laterais-sem-${lado}`, recolhida ? "1" : "0") } catch { /* sem armazenamento */ }
}

function aplicar(lateral, botao) {
  const recolhida = document.documentElement.classList.contains(`laterais-sem-${lateral.lado}`)
  botao.textContent = recolhida ? lateral.fechada : lateral.aberta
  botao.setAttribute("aria-pressed", String(recolhida))
  botao.setAttribute("aria-label", `${recolhida ? "Mostrar" : "Recolher"} o ${lateral.rotulo}`)
  botao.title = botao.getAttribute("aria-label")
}

function montarBotoes() {
  for (const lateral of LATERAIS) {
    document.documentElement.classList.toggle(`laterais-sem-${lateral.lado}`, lerEscolha(lateral.lado))
    let botao = document.querySelector(`.laterais-botao--${lateral.lado}`)
    if (!botao) {
      botao = document.createElement("button")
      botao.type = "button"
      botao.className = `laterais-botao laterais-botao--${lateral.lado}`
      botao.addEventListener("click", () => {
        const recolhida = document.documentElement.classList.toggle(`laterais-sem-${lateral.lado}`)
        salvarEscolha(lateral.lado, recolhida)
        aplicar(lateral, botao)
      })
      document.body.append(botao)
    }
    /* Páginas que já escondem a lateral (hide no front matter) não mostram a seta. */
    const barra = document.querySelector(lateral.seletor)
    botao.hidden = !barra || barra.hidden
    aplicar(lateral, botao)
  }
}

document$.subscribe(montarBotoes)
