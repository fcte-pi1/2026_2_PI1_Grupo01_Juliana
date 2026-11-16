#!/usr/bin/env bash
# Varredura de robustez da navegação (FIRM-13): roda o simulador em vários
# níveis de leituras erradas, com várias sementes, e imprime uma tabela em
# Markdown com os resultados de cada nível.
#
# Uso (dentro de src/firmware/simulador-navegacao):
#   ./varredura.sh                    # 8x4 e 12x4, sementes 1 a 50, 3 leituras da frente
#   ./varredura.sh -a 7 5 10 20       # só 5%, 10% e 20%, com 7 leituras da frente
#   LABIRINTOS="labirintos/4x4-*.txt" ./varredura.sh
#
# Sai com código 1 se alguma corrida terminar em sucesso_falso ou limite.
set -euo pipefail
cd "$(dirname "$0")"

amostras=3
sementes=50
while getopts "a:s:h" opcao; do
    case "$opcao" in
    a) amostras="$OPTARG" ;;
    s) sementes="$OPTARG" ;;
    *)
        echo "uso: $0 [-a leituras_da_frente] [-s sementes] [ruido_pct...]" >&2
        exit 2
        ;;
    esac
done
shift $((OPTIND - 1))
ruidos=("$@")
if [ ${#ruidos[@]} -eq 0 ]; then
    ruidos=(0 1 2 5 8 10 15 20 30)
fi
# Sem aspas de propósito: o shell expande os padrões em nomes de arquivos.
# shellcheck disable=SC2206
labirintos=(${LABIRINTOS:-labirintos/8x4-*.txt labirintos/12x4-*.txt})

make -s build/simulador

echo "Labirintos: ${#labirintos[@]} · sementes 1 a $sementes · $amostras leituras da frente"
echo
echo "| Leituras erradas | Sucesso | colisao | sem_caminho | sucesso_falso | limite | Células (média / máx.) |"
echo "| ---: | ---: | ---: | ---: | ---: | ---: | ---: |"

ruim=0
for ruido in "${ruidos[@]}"; do
    linha=$(
        for semente in $(seq 1 "$sementes"); do
            ./build/simulador --ruido "$ruido" --semente "$semente" \
                --amostras-frente "$amostras" "${labirintos[@]}" | grep -v '^total'
        done | LC_ALL=C awk -v ruido="$ruido" '
            {
                n++
                r[$3]++
                if ($3 == "sucesso") {
                    split($4, c, "=")
                    soma += c[2]
                    if (c[2] > max) max = c[2]
                }
            }
            END {
                media = r["sucesso"] ? soma / r["sucesso"] : 0
                printf "| %s%% | %d de %d | %d | %d | %d | %d | %.1f / %d |\n",
                    ruido, r["sucesso"], n, r["colisao"], r["sem_caminho"],
                    r["sucesso_falso"], r["limite"], media, max
                if (n == 0 || r["sucesso_falso"] + r["limite"] > 0) exit 1
            }'
    ) || ruim=1
    echo "$linha"
done

exit "$ruim"
