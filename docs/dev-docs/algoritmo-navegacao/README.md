# Fontes do algoritmo de navegação

Material de apoio à escolha do algoritmo de navegação do Micromouse (seção "Algoritmo de navegação" do [Projeto conceitual de software](../../4.4%20-%20Projeto%20conceitual%20de%20software.md)).

Só entram na revisão de literatura as fontes marcadas como conferidas na coluna **Conferida**. Os PDFs desta pasta são de acesso aberto; artigos da IEEE e páginas web ficam apenas como link.

## Artigos e relatórios (PDF nesta pasta)

| # | Fonte | Arquivo | Afirmação usada | Onde conferir | Conferida |
| :-: | :--- | :--- | :--- | :--- | :-: |
| 1 | LAW, G. *Quantitative comparison of flood fill and modified flood fill algorithms*. IJCTE, v. 5, n. 3, p. 503-508, 2013. | [PDF](Law-2013-IJCTE-flood-fill.pdf) · [original](https://www.ijcte.org/papers/738-T012.pdf) | O flood fill venceu primeiros lugares em competições internacionais. | p. 503, Abstract | [ ] |
| 2 | Law (2013) | [PDF](Law-2013-IJCTE-flood-fill.pdf) | O modified flood fill é o mais usado em competições. | p. 503, Seção II | [ ] |
| 3 | Law (2013) | [PDF](Law-2013-IJCTE-flood-fill.pdf) | Flood fill e modified flood fill percorrem o mesmo número de células; o modified faz muito menos atualizações (135.936 vs 3.197; 82.688 vs 3.307). | p. 506, Tabela I (não há frase no texto, só a tabela) | [ ] |
| 4 | SHARMA, K.; MUNSHI, C. *A comprehensive and comparative study of maze-solving techniques by implementing graph theory*. IOSR-JCE, v. 17, n. 1, p. 24-29, 2015. | [PDF](Sharma-Munshi-2015-IOSR-JCE.pdf) · [original](https://www.iosrjournals.org/iosr-jce/papers/Vol17-issue1/Version-4/E017142429.pdf) | A DFS não garante o menor caminho e perde tempo explorando o labirinto inteiro. | p. 24, Seção I; p. 28 | [ ] |
| 5 | Sharma e Munshi (2015) | [PDF](Sharma-Munshi-2015-IOSR-JCE.pdf) | Recomenda o modified flood fill para competição. | p. 29, Conclusão | [ ] |
| 6 | MISHRA, S.; BANDE, P. *Maze solving algorithms for micro mouse*. IEEE SITIS, p. 86-93, 2008. | [link](https://swati-mishra.com/wp-content/uploads/2020/02/04725791.pdf) (IEEE, sem cópia aqui) | O seguidor de parede falha e entra em laço em alguns labirintos. | p. 87-88, item 2.3 e Fig. 2 | [ ] |
| 7 | Mishra e Bande (2008) | [link](https://swati-mishra.com/wp-content/uploads/2020/02/04725791.pdf) | O flood fill mapeia e resolve ao mesmo tempo e é "by far the most effective". | p. 92, Seções 5 e 6 | [ ] |
| 8 | TJIHARJADI, S.; WIJAYA, M. C.; SETIAWAN, E. *Optimization maze robot using A\* and flood fill algorithm*. IJMERR, v. 6, n. 5, p. 366-372, 2017. | [PDF](Tjiharjadi-2017-IJMERR-Astar-flood-fill.pdf) · [original](https://www.ijmerr.com/uploadfile/2017/0904/20170904105839434.pdf) | O A\* gasta mais memória por guardar a lista aberta. | p. 366 (revisão da literatura do artigo, não do experimento) | [ ] |
| 9 | Tjiharjadi et al. (2017) | [PDF](Tjiharjadi-2017-IJMERR-Astar-flood-fill.pdf) | No labirinto 5x5 ambos acham o menor caminho, mas o tamanho não permite diferenciá-los. | p. 368; p. 372, Conclusões 2 e 3 | [ ] |
| 10 | SUGAWARA, K.; MIMURA, N. *迷路探索アルゴリズムの評価に関する研究*. SICE Tohoku, doc. 305-13, 2016. | [PDF](Sugawara-Mimura-2016-SICE-Tohoku.pdf) · [original](https://www.topic.ad.jp/sice/htdocs/papers/305/305-13.pdf) | O método da mão esquerda entra em laço infinito. | p. ‐2‐, §3 | [ ] |
| 11 | Sugawara e Mimura (2016) | [PDF](Sugawara-Mimura-2016-SICE-Tohoku.pdf) | O método Adachi "não é o mais eficiente, mas é o melhor dentro das regras". | p. ‐2‐ §3 e ‐4‐ §6 | [ ] |
| 12 | Sugawara e Mimura (2016) | [PDF](Sugawara-Mimura-2016-SICE-Tohoku.pdf) | Probabilidade média de falha nas bifurcações de 45,7% (33,3% a 70,6%) em 10 labirintos. | p. ‐3‐, §5 | [ ] |
| 13 | APEC. *31th Annual Micromouse Contest Report*, 2017. | [PDF](APEC-2017-Contest-Report.pdf) · [original](https://micromouseonline.com/wp-content/uploads/2017/04/APEC2017Report.pdf) | O vencedor, Diu-Gow 4 (Lunghwa), também venceu o All Japan 2015 e 2016. | PDF p. 1 e 3 | [ ] |
| 14 | APEC (2017) | [PDF](APEC-2017-Contest-Report.pdf) | O vencedor usa Renesas RX62T com 16 KB de RAM; o Decimus 5α usa menos de 10% de um STM32F407. | PDF p. 3-4 | [ ] |
| 22 | KUNIYOSHI, H. *迷路を短時間で全探索するアルゴリズムの研究*. Pôster, 82º Congresso Nacional IPSJ, 2020. | [PDF](Kuniyoshi-2020-IPSJ-poster.pdf) · [original](https://www.ipsj.or.jp/event/taikai/82/82PosterSession/img/portfolio/82P_1028.pdf) | O micromouse pressupõe tamanho conhecido; para tamanho desconhecido, segue para a célula não visitada mais próxima; cerca de 17% mais rápido (30x30, 500 labirintos). | página única. Ressalva: trabalho de aluno do ensino fundamental II. | [ ] |

## Páginas web e regras (apenas link)

| # | Fonte | Afirmação usada | Onde conferir | Conferida |
| :-: | :--- | :--- | :--- | :-: |
| 15 | HARRISON, P. [Solving the maze](https://micromouseonline.com/micromouse-book/mazes-and-maze-solving/solving-the-maze/). Micromouse Online. | O flood fill (Bellman) é o método mais simples. | corpo do texto | [ ] |
| 16 | Harrison, Solving the maze | Paredes ainda não vistas são tratadas como abertas. | comentário do autor de 11/11/2014 | [ ] |
| 17 | HARRISON, P. [Exploration tuning for micromouse](https://micromouseonline.com/2014/09/07/exploration-tune/). Micromouse Online, 2014. | Um flood completo leva cerca de 700 µs. | comentário do autor de 08/09/2014 | [ ] |
| 18 | HARRISON, P. [Adachi micromouse maze solving algorithms](https://micromouseonline.com/2008/05/27/adachi-micromouse-maze-solving-algorithms/). Micromouse Online, 2008. | Os quatro métodos de Adachi. | seções "Adachi Method 0–3" | [ ] |
| 19 | NEW TECHNOLOGY FOUNDATION. [Rules for Classic Micromouse](https://www.ntf.or.jp/mouse/micromouse2011/ruleclassic-EN.html), 2011. | Labirinto 16x16 com células de 18 cm; objetivo nas 4 células centrais; proibido informar o labirinto; 10 min. | Arts. 2-2, 2-3, 3-2 e 3-6 | [ ] |
| 20 | APEC. [MicroMouse Contest Rules](https://www.thierry-lequeu.fr/data/APEC/APEC_MicroMouse_Contest_Rules.html). | Objetivo central; 10 min e até 10 corridas; proibido informar o labirinto. | Seções I e III | [ ] |
| 21 | JSDKK. [どこよりも詳しいマイクロマウス解説](https://jsdkk.com/home/glossary/glossary-micromouse/). | Quase todos os mouses da classe superior usam o método Adachi, que trata paredes desconhecidas como livres. | Seção 4-5 | [ ] |
