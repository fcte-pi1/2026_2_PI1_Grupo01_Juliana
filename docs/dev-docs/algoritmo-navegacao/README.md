# Fontes do algoritmo de navegação

Material de apoio à escolha do algoritmo de navegação do Micromouse (seção "Algoritmo de navegação" do [Projeto conceitual de software](../../4.4%20-%20Projeto%20conceitual%20de%20software.md)).

Todas as fontes abaixo foram conferidas manualmente pela equipe. Os PDFs desta pasta são de acesso aberto; as páginas web ficam apenas como link.

## Artigos (PDF nesta pasta)

| # | Fonte | Arquivo | Afirmação usada | Onde conferir |
| :-: | :--- | :--- | :--- | :--- |
| 1 | LAW, G. *Quantitative comparison of flood fill and modified flood fill algorithms*. IJCTE, v. 5, n. 3, p. 503-508, 2013. | [PDF](Law-2013-IJCTE-flood-fill.pdf) · [original](https://www.ijcte.org/papers/738-T012.pdf) | O flood fill venceu primeiros lugares em competições internacionais. | p. 503, Abstract |
| 2 | Law (2013) | [PDF](Law-2013-IJCTE-flood-fill.pdf) | O modified flood fill é o mais usado em competições. | p. 503, Seção II |
| 3 | Law (2013) | [PDF](Law-2013-IJCTE-flood-fill.pdf) | Flood fill e modified flood fill percorrem o mesmo número de células; o modified faz muito menos atualizações (135.936 vs 3.197; 82.688 vs 3.307). | p. 506, Tabela I (não há frase no texto, só a tabela) |
| 4 | SHARMA, K.; MUNSHI, C. *A comprehensive and comparative study of maze-solving techniques by implementing graph theory*. IOSR-JCE, v. 17, n. 1, p. 24-29, 2015. | [PDF](Sharma-Munshi-2015-IOSR-JCE.pdf) · [original](https://www.iosrjournals.org/iosr-jce/papers/Vol17-issue1/Version-4/E017142429.pdf) | A DFS não garante o menor caminho e perde tempo explorando o labirinto inteiro. | p. 24, Seção I; p. 28 |
| 5 | Sharma e Munshi (2015) | [PDF](Sharma-Munshi-2015-IOSR-JCE.pdf) | Recomenda o modified flood fill para competição. | p. 29, Conclusão |
| 6 | TJIHARJADI, S.; WIJAYA, M. C.; SETIAWAN, E. *Optimization maze robot using A\* and flood fill algorithm*. IJMERR, v. 6, n. 5, p. 366-372, 2017. | [PDF](Tjiharjadi-2017-IJMERR-Astar-flood-fill.pdf) · [original](https://www.ijmerr.com/uploadfile/2017/0904/20170904105839434.pdf) | O A\* gasta mais memória por guardar a lista aberta. | p. 366 (revisão da literatura do artigo, não do experimento) |
| 7 | Tjiharjadi et al. (2017) | [PDF](Tjiharjadi-2017-IJMERR-Astar-flood-fill.pdf) | No labirinto 5x5 ambos acham o menor caminho, mas o tamanho não permite diferenciá-los. | p. 368; p. 372, Conclusões 2 e 3 |

## Páginas web (apenas link)

| # | Fonte | Afirmação usada | Onde conferir |
| :-: | :--- | :--- | :--- |
| 8 | HARRISON, P. [Solving the maze](https://micromouseonline.com/micromouse-book/mazes-and-maze-solving/solving-the-maze/). Micromouse Online. | O flood fill (Bellman) é o método mais simples. | corpo do texto |
| 9 | HARRISON, P. [Adachi micromouse maze solving algorithms](https://micromouseonline.com/2008/05/27/adachi-micromouse-maze-solving-algorithms/). Micromouse Online, 2008. | Os quatro métodos de Adachi. | seções "Adachi Method 0–3" |
| 10 | JSDKK. [どこよりも詳しいマイクロマウス解説](https://jsdkk.com/home/glossary/glossary-micromouse/). | Quase todos os mouses da classe superior usam o método Adachi, que trata paredes desconhecidas como livres. | Seção 4-5 |
