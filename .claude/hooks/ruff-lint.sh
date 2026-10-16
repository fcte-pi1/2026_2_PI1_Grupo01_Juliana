#!/usr/bin/env bash
# PostToolUse: roda o ruff no arquivo Python editado pelo Claude.
# Aplica as correcoes seguras do lint, formata o arquivo e devolve ao
# Claude (exit 2) os problemas de lint que sobrarem.
f=$(jq -r '.tool_input.file_path // .tool_response.filePath // empty')
[[ "$f" == *.py && -f "$f" ]] || exit 0
command -v uvx >/dev/null || exit 0

# O ruff acha sozinho o pyproject.toml mais proximo (backend ou simulador).
lint=$(uvx ruff check --fix --quiet "$f" 2>&1)
lint_status=$?

# Formata depois do lint, para limpar o que as correcoes deixaram
# (por exemplo, a linha em branco de um import removido).
fmt=$(uvx ruff format --quiet "$f" 2>&1)
fmt_status=$?

if [[ $lint_status -ne 0 || $fmt_status -ne 0 ]]; then
  echo "ruff encontrou problemas em $f:" >&2
  [[ -n "$lint" ]] && echo "$lint" >&2
  [[ -n "$fmt" ]] && echo "$fmt" >&2
  exit 2
fi
