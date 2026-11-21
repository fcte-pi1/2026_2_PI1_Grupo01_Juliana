#!/usr/bin/env bash
# Sobe o ambiente de ensaio do Micromouse: banco (Postgres no Docker), back-end (FastAPI) e front (Vite, sem mock).
# Em ambiente novo instala o que falta; em ambiente pronto só confere e sobe. Depois é só abrir a tela e operar por ela.
#
#   scripts/ambiente.sh                  sobe banco, back-end e front
#   scripts/ambiente.sh subir --ponte --bluetooth 88:57:21:70:7F:0E
#                                        também liga a ponte (precisa do robô pareado; usa sudo no rfcomm)
#   scripts/ambiente.sh status           mostra o que está no ar
#   scripts/ambiente.sh logs [api|front|ponte]
#   scripts/ambiente.sh parar [--tudo]   para o que este script subiu (--tudo para o banco também)
#
# Opções do subir: --ponte, --bluetooth ENDERECO, --porta-serial DEV (padrão /dev/rfcomm0), --abrir (abre o navegador),
#                  --instalar-sistema (apt: python3-venv, node, docker; precisa de sudo, só Debian e Ubuntu)
# Variáveis: FRONT_PORTA (padrão 5173). O back-end sobe sempre na 8000, que é a que o proxy do front usa.

set -euo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BACK="$RAIZ/src/backend"
FRONT="$RAIZ/src/frontend"
RUN="$RAIZ/.ambiente"
LOGS="$RUN/logs"
API_PORTA=8000
FRONT_PORTA="${FRONT_PORTA:-5173}"
API_URL="http://127.0.0.1:${API_PORTA}"
FRONT_URL="http://127.0.0.1:${FRONT_PORTA}"
PYTHON_MIN="3.11"
NODE_MIN=20

if [ -t 1 ]; then NEGRITO=$'\033[1m'; VERDE=$'\033[32m'; AMARELO=$'\033[33m'; VERMELHO=$'\033[31m'; ZERA=$'\033[0m'; else NEGRITO=""; VERDE=""; AMARELO=""; VERMELHO=""; ZERA=""; fi
passo() { printf '%s==>%s %s\n' "$NEGRITO" "$ZERA" "$*"; }
ok() { printf '  %s✓%s %s\n' "$VERDE" "$ZERA" "$*"; }
aviso() { printf '  %s!%s %s\n' "$AMARELO" "$ZERA" "$*"; }
erro() { printf '%serro:%s %s\n' "$VERMELHO" "$ZERA" "$*" >&2; }
falhar() { erro "$*"; exit 1; }

COMANDO="subir"
PONTE=0
BLUETOOTH=""
PORTA_SERIAL="/dev/rfcomm0"
ABRIR=0
INSTALAR_SISTEMA=0
TUDO=0
ALVO_LOG="api"

if [ $# -gt 0 ]; then
  case "$1" in
    subir|parar|status|logs|ajuda|-h|--help) COMANDO="$1"; shift ;;
  esac
fi
while [ $# -gt 0 ]; do
  case "$1" in
    --ponte) PONTE=1 ;;
    --bluetooth) BLUETOOTH="${2:?informe o endereço do robô, por exemplo 88:57:21:70:7F:0E}"; PONTE=1; shift ;;
    --porta-serial) PORTA_SERIAL="${2:?informe a porta serial}"; shift ;;
    --abrir) ABRIR=1 ;;
    --instalar-sistema) INSTALAR_SISTEMA=1 ;;
    --tudo) TUDO=1 ;;
    api|front|ponte) ALVO_LOG="$1" ;;
    *) falhar "opção desconhecida: $1 (veja scripts/ambiente.sh ajuda)" ;;
  esac
  shift
done

ajuda() { sed -n '2,15p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }

# ---------- utilidades ----------
tem() { command -v "$1" >/dev/null 2>&1; }
versao_ok() { # versao_ok ATUAL MINIMA (compara major.minor)
  [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n1)" = "$2" ]
}
no_ar() { curl -fsS -m 3 -o /dev/null "$1" 2>/dev/null; }
esperar() { # esperar URL SEGUNDOS
  local url="$1" limite="$2" i=0
  while [ "$i" -lt "$limite" ]; do
    if no_ar "$url"; then return 0; fi
    sleep 1; i=$((i + 1))
  done
  return 1
}
pid_vivo() { [ -f "$1" ] && kill -0 "$(cat "$1")" 2>/dev/null; }
parar_pid() { # parar_pid ARQUIVO NOME
  if pid_vivo "$1"; then
    local pid; pid="$(cat "$1")"
    kill -- "-$pid" 2>/dev/null || kill "$pid" 2>/dev/null || true
    ok "$2 parado"
  fi
  rm -f "$1"
}
iniciar_em_segundo_plano() { # iniciar_em_segundo_plano NOME PASTA LOG -- comando...
  local nome="$1" pasta="$2" log="$3"; shift 4
  mkdir -p "$LOGS"
  ( cd "$pasta" && exec setsid nohup "$@" >"$log" 2>&1 ) &
  echo $! >"$RUN/$nome.pid"
}
quem_escuta() { # quem_escuta PORTA: pids que escutam a porta TCP
  ss -ltnp "sport = :$1" 2>/dev/null | sed -n 's/.*pid=\([0-9]*\).*/\1/p' | sort -u | tr '\n' ' '
}
ultimas_linhas() { [ -f "$1" ] && tail -n 15 "$1" | sed 's/^/    /' || true; }

# ---------- pré-requisitos do sistema ----------
checar_sistema() {
  passo "Conferindo o que o sistema precisa"
  local faltam=()

  if ! tem docker; then faltam+=("docker"); elif ! docker compose version >/dev/null 2>&1; then faltam+=("docker compose"); fi

  local py="python3"
  if tem python3; then
    local v; v="$(python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])')"
    if ! versao_ok "$v" "$PYTHON_MIN"; then faltam+=("python3 >= $PYTHON_MIN (achei $v)"); fi
    if ! python3 -c 'import venv, ensurepip' >/dev/null 2>&1; then faltam+=("python3-venv"); fi
  else
    faltam+=("python3 >= $PYTHON_MIN")
  fi
  : "$py"

  if tem node && tem npm; then
    local n; n="$(node -p 'process.versions.node.split(".")[0]')"
    if [ "$n" -lt "$NODE_MIN" ]; then faltam+=("node >= $NODE_MIN (achei $n)"); fi
  else
    faltam+=("node >= $NODE_MIN e npm")
  fi
  tem curl || faltam+=("curl")

  if [ "${#faltam[@]}" -gt 0 ]; then
    if [ "$INSTALAR_SISTEMA" -eq 1 ] && tem apt-get; then
      aviso "Instalando pelo apt (vai pedir a senha de administrador): ${faltam[*]}"
      sudo apt-get update
      sudo apt-get install -y python3 python3-venv nodejs npm docker.io docker-compose-v2 curl
      aviso "Se o Docker acabou de ser instalado: sudo usermod -aG docker \$USER e abra uma sessão nova."
    else
      erro "Faltam: ${faltam[*]}"
      cat >&2 <<EOF
  Em Ubuntu ou Debian:   scripts/ambiente.sh subir --instalar-sistema
  À mão:                 sudo apt-get install -y python3 python3-venv docker.io docker-compose-v2 curl
                         Node $NODE_MIN ou mais novo: https://github.com/nvm-sh/nvm  (nvm install --lts)
  Outros sistemas:       instale Docker (com o compose), Python $PYTHON_MIN+, Node $NODE_MIN+ e curl.
EOF
      exit 1
    fi
  fi
  docker info >/dev/null 2>&1 || falhar "o Docker está instalado, mas o serviço não responde. Inicie-o (sudo systemctl start docker) e confira que seu usuário está no grupo docker."
  ok "docker, python, node e curl no lugar"
}

# ---------- dependências do projeto ----------
preparar_python() {
  passo "Dependências do back-end (Python)"
  local venv="$BACK/.venv"
  if [ ! -x "$venv/bin/python" ]; then
    python3 -m venv "$venv"
    ok "ambiente virtual criado em src/backend/.venv"
  fi
  local modulos="fastapi, uvicorn, alembic, psycopg, sqlalchemy, pydantic_settings"
  [ "$PONTE" -eq 1 ] && modulos="$modulos, serial, httpx"
  if "$venv/bin/python" -c "import $modulos" >/dev/null 2>&1; then
    ok "dependências já instaladas"
  else
    aviso "instalando as dependências (a primeira vez demora um pouco)"
    "$venv/bin/python" -m pip install --quiet --upgrade pip
    local extras="dev"
    [ "$PONTE" -eq 1 ] && extras="dev,ponte"
    ( cd "$BACK" && "$venv/bin/python" -m pip install --quiet -e ".[${extras}]" )
    ok "dependências instaladas"
  fi
}

preparar_node() {
  passo "Dependências do front (Node)"
  if [ -x "$FRONT/node_modules/.bin/vite" ]; then
    ok "dependências já instaladas"
  else
    aviso "instalando as dependências (a primeira vez demora um pouco)"
    ( cd "$FRONT" && npm ci --no-audit --no-fund )
    ok "dependências instaladas"
  fi
}

# ---------- serviços ----------
subir_banco() {
  passo "Banco de dados (Postgres no Docker)"
  ( cd "$BACK" && docker compose up -d db >/dev/null )
  local id i=0 estado=""
  id="$(cd "$BACK" && docker compose ps -q db)"
  while [ "$i" -lt 60 ]; do
    estado="$(docker inspect -f '{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}' "$id" 2>/dev/null || true)"
    [ "$estado" = "healthy" ] && break
    sleep 1; i=$((i + 1))
  done
  [ "$estado" = "healthy" ] || falhar "o banco não ficou saudável em 60 s (estado: ${estado:-desconhecido}). Veja: cd src/backend && docker compose logs db"
  local publicada
  publicada="$(cd "$BACK" && docker compose port db 5432 | sed 's/.*://')"
  [ -n "$publicada" ] || falhar "não achei a porta publicada do banco (docker compose port db 5432)"
  export DATABASE_URL="postgresql+psycopg://micromouse:micromouse@localhost:${publicada}/micromouse"
  ok "banco saudável na porta ${publicada}"
}

migrar() {
  passo "Migrações do banco"
  ( cd "$BACK" && "$BACK/.venv/bin/alembic" upgrade head >"$LOGS/migracao.log" 2>&1 ) \
    || { ultimas_linhas "$LOGS/migracao.log"; falhar "a migração falhou (log: .ambiente/logs/migracao.log)"; }
  ok "banco na versão mais nova"
}

subir_api() {
  passo "Back-end (API na porta ${API_PORTA})"
  if no_ar "$API_URL/back-health"; then
    aviso "já há uma API respondendo em $API_URL; vou usar a que está no ar"
    return 0
  fi
  local ocupante; ocupante="$(quem_escuta "$API_PORTA")"
  if [ -n "${ocupante// /}" ]; then
    erro "a porta $API_PORTA está ocupada (pid ${ocupante}) por uma API que não responde em $API_URL/back-health: ela travou."
    erro "pare-a com: kill ${ocupante}  (se não sair, kill -9 ${ocupante}) e rode o script de novo."
    exit 1
  fi
  iniciar_em_segundo_plano api "$BACK" "$LOGS/api.log" -- "$BACK/.venv/bin/uvicorn" app.main:app --host 0.0.0.0 --port "$API_PORTA" --reload
  if esperar "$API_URL/back-health" 60; then
    ok "API no ar: $API_URL/docs"
  else
    ultimas_linhas "$LOGS/api.log"
    falhar "a API não respondeu em 60 s (log: .ambiente/logs/api.log)"
  fi
}

subir_front() {
  passo "Front (tela, sem mock, na porta ${FRONT_PORTA})"
  if no_ar "$FRONT_URL/"; then
    aviso "já há um servidor em $FRONT_URL; vou usar o que está no ar (confira que ele está sem mock)"
    return 0
  fi
  local ocupante; ocupante="$(quem_escuta "$FRONT_PORTA")"
  if [ -n "${ocupante// /}" ]; then
    erro "a porta $FRONT_PORTA está ocupada (pid ${ocupante}) por um servidor que não responde."
    erro "pare-o com: kill ${ocupante} e rode o script de novo."
    exit 1
  fi
  # a variável do shell vale mais que o .env do Vite: a tela fala com a API de verdade
  iniciar_em_segundo_plano front "$FRONT" "$LOGS/front.log" -- env VITE_API_MOCK=false npm run dev -- --host 0.0.0.0 --port "$FRONT_PORTA" --strictPort
  if esperar "$FRONT_URL/" 60; then
    ok "front no ar: $FRONT_URL"
  else
    ultimas_linhas "$LOGS/front.log"
    falhar "o front não respondeu em 60 s (log: .ambiente/logs/front.log). A porta ${FRONT_PORTA} está livre?"
  fi
}

subir_ponte() {
  passo "Ponte (robô → back-end)"
  if [ -n "$BLUETOOTH" ]; then
    tem rfcomm || falhar "o comando rfcomm não existe (sudo apt-get install bluez)"
    aviso "ligando o rfcomm (pede a senha de administrador)"
    sudo rfcomm release all 2>/dev/null || true
    sudo rfcomm bind "$PORTA_SERIAL" "$BLUETOOTH" 1
    ok "$PORTA_SERIAL ligado ao robô $BLUETOOTH"
  fi
  [ -e "$PORTA_SERIAL" ] || falhar "a porta $PORTA_SERIAL não existe. Pareie o robô e rode de novo com --bluetooth ENDERECO, ou confira a porta."
  if [ ! -f /etc/udev/rules.d/99-rfcomm-ignorar-modemmanager.rules ] && systemctl is-active --quiet ModemManager 2>/dev/null; then
    aviso "o ModemManager está ativo e pode tomar as primeiras linhas do robô. Regra que resolve (README do firmware):"
    aviso "  echo 'KERNEL==\"rfcomm*\", ENV{ID_MM_DEVICE_IGNORE}=\"1\"' | sudo tee /etc/udev/rules.d/99-rfcomm-ignorar-modemmanager.rules && sudo udevadm control --reload-rules"
  fi
  if pid_vivo "$RUN/ponte.pid"; then
    aviso "a ponte já está rodando"
    return 0
  fi
  # duas pontes na mesma porta dividem os bytes e as linhas chegam quebradas
  local outras
  outras="$(pgrep -f "python -m ponte --porta $PORTA_SERIAL" | tr '\n' ' ' || true)"
  if [ -n "${outras// /}" ]; then
    falhar "já há uma ponte lendo $PORTA_SERIAL (pid ${outras}). Duas pontes na mesma porta quebram as linhas. Pare a outra (kill ${outras}) e rode de novo."
  fi
  iniciar_em_segundo_plano ponte "$BACK" "$LOGS/ponte.log" -- "$BACK/.venv/bin/python" -m ponte --porta "$PORTA_SERIAL" --url "$API_URL"
  sleep 2
  if pid_vivo "$RUN/ponte.pid"; then
    ok "ponte rodando em $PORTA_SERIAL (log: .ambiente/logs/ponte.log)"
  else
    ultimas_linhas "$LOGS/ponte.log"
    falhar "a ponte parou logo ao subir (log: .ambiente/logs/ponte.log)"
  fi
}

# ---------- comandos ----------
cmd_subir() {
  mkdir -p "$LOGS"
  checar_sistema
  preparar_python
  preparar_node
  subir_banco
  migrar
  subir_api
  subir_front
  [ "$PONTE" -eq 1 ] && subir_ponte
  echo
  printf '%sTudo no ar.%s Abra a tela e opere por ela:\n' "$NEGRITO" "$ZERA"
  printf '  Tela:     %s\n' "$FRONT_URL"
  printf '  API:      %s/docs\n' "$API_URL"
  if [ "$PONTE" -eq 0 ]; then
    printf '  Robô:     sem ponte. Para ligá-la: scripts/ambiente.sh subir --ponte --bluetooth ENDERECO\n'
  fi
  printf '  Parar:    scripts/ambiente.sh parar     Ver o que está no ar: scripts/ambiente.sh status\n'
  if [ "$ABRIR" -eq 1 ]; then
    if tem xdg-open; then xdg-open "$FRONT_URL" >/dev/null 2>&1 || true; elif tem open; then open "$FRONT_URL" || true; fi
  fi
}

cmd_status() {
  passo "O que está no ar"
  local id estado
  id="$(cd "$BACK" && docker compose ps -q db 2>/dev/null || true)"
  if [ -n "$id" ]; then
    estado="$(docker inspect -f '{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}' "$id" 2>/dev/null || echo "?")"
    ok "banco: $estado"
  else
    aviso "banco: parado"
  fi
  if no_ar "$API_URL/back-health"; then ok "API: $API_URL/docs ($(curl -fsS -m 3 "$API_URL/back-health"))"; else aviso "API: fora do ar"; fi
  if no_ar "$FRONT_URL/"; then ok "tela: $FRONT_URL"; else aviso "tela: fora do ar"; fi
  if pid_vivo "$RUN/ponte.pid"; then ok "ponte: rodando (pid $(cat "$RUN/ponte.pid"))"; else aviso "ponte: não está rodando por este script"; fi
}

cmd_parar() {
  passo "Parando o que este script subiu"
  parar_pid "$RUN/front.pid" "front"
  parar_pid "$RUN/api.pid" "API"
  parar_pid "$RUN/ponte.pid" "ponte"
  if [ "$TUDO" -eq 1 ]; then
    ( cd "$BACK" && docker compose stop db >/dev/null ) && ok "banco parado (os dados ficam no volume)"
  else
    aviso "o banco continua no ar (use --tudo para parar também)"
  fi
}

cmd_logs() {
  local arquivo="$LOGS/${ALVO_LOG}.log"
  [ -f "$arquivo" ] || falhar "ainda não há log de $ALVO_LOG ($arquivo)"
  tail -n 60 -f "$arquivo"
}

case "$COMANDO" in
  subir) cmd_subir ;;
  status) cmd_status ;;
  parar) cmd_parar ;;
  logs) cmd_logs ;;
  ajuda|-h|--help) ajuda ;;
esac
