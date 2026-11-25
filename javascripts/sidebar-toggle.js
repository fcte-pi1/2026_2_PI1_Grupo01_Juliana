/*
 * Recolhe a navegação (esquerda) e o índice (direita) no tema Material.
 * A escolha fica no navegador e vale nas páginas seguintes.
 */

(() => {
  const chave = "rt-sidebars"
  const telaNav = window.matchMedia("(min-width: 76.25em)")
  const telaIndice = window.matchMedia("(min-width: 60em)")
  const icones = {
    nav: "M4 4h16a1 1 0 0 1 1 1v14a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V5a1 1 0 0 1 1-1zm1 2v12h4V6H5zm6 0v12h8V6h-8z",
    toc: "M4 6h2v2H4V6zm4 0h12v2H8V6zM4 11h2v2H4v-2zm4 0h12v2H8v-2zM4 16h2v2H4v-2zm4 0h12v2H8v-2z",
  }
  const rotulos = {
    nav: ["Recolher navegação", "Mostrar navegação"],
    toc: ["Recolher índice", "Mostrar índice"],
  }

  const ler = () => {
    try {
      const salvo = JSON.parse(localStorage.getItem(chave))
      return { nav: !!salvo?.nav, toc: !!salvo?.toc }
    } catch {
      return { nav: false, toc: false }
    }
  }

  const gravar = (estado) => {
    try {
      localStorage.setItem(chave, JSON.stringify(estado))
    } catch {
      /* navegação segue funcionando nesta página */
    }
  }

  const botao = (qual) => {
    const el = document.createElement("button")
    el.type = "button"
    el.className = "md-header__button md-icon"
    el.dataset.rtToggle = qual
    el.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path d="${icones[qual]}"/></svg>`
    return el
  }

  const garantirBotoes = () => {
    if (document.querySelector(".rt-sidebar-toggles")) return
    const cabecalho = document.querySelector(".md-header__inner")
    if (!cabecalho) return
    const grupo = document.createElement("div")
    grupo.className = "rt-sidebar-toggles"
    grupo.append(botao("nav"), botao("toc"))
    const ancora = cabecalho.querySelector("[data-md-component='palette']")
      || cabecalho.querySelector("[for='__search']")
    cabecalho.insertBefore(grupo, ancora)
  }

  const sincronizar = (qual, recolhido) => {
    const el = document.querySelector(`[data-rt-toggle="${qual}"]`)
    if (!el) return
    const rotulo = rotulos[qual][recolhido ? 1 : 0]
    el.title = rotulo
    el.setAttribute("aria-label", rotulo)
    el.setAttribute("aria-expanded", recolhido ? "false" : "true")
  }

  const aplicar = () => {
    const estado = ler()
    const navegacao = document.querySelector(".md-sidebar--primary")
    const indice = document.querySelector(".md-sidebar--secondary")
    if (navegacao) {
      if (telaNav.matches && estado.nav) navegacao.setAttribute("hidden", "")
      else navegacao.removeAttribute("hidden")
    }
    if (indice) {
      if (telaIndice.matches && estado.toc) indice.setAttribute("hidden", "")
      else indice.removeAttribute("hidden")
    }
    sincronizar("nav", estado.nav)
    sincronizar("toc", estado.toc)
  }

  const alternar = (qual) => {
    const estado = ler()
    estado[qual] = !estado[qual]
    gravar(estado)
    aplicar()
  }

  const iniciar = () => {
    garantirBotoes()
    aplicar()
  }

  document.addEventListener("click", (evento) => {
    const alvo = evento.target.closest("[data-rt-toggle]")
    if (!alvo) return
    alternar(alvo.dataset.rtToggle)
  })

  telaNav.addEventListener("change", aplicar)
  telaIndice.addEventListener("change", aplicar)

  if (typeof document$ !== "undefined") document$.subscribe(iniciar)
  else document.addEventListener("DOMContentLoaded", iniciar)
})()
