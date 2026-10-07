#!/usr/bin/env bash
# mcp-setup.sh — скрипт активации MCP-серверов Espressif (Linux/macOS)
# Использование:  ./mcp-setup.sh [claude|codex|generic]
set -u
AGENT="${1:-generic}"

SERVERS=(
  "esp-component-registry|https://components.espressif.com/mcp"
  "esp-pilot-mcp|https://mcp.esp-pilot.espressif.com/mcp"
  "esp-vision|https://mcp.vision.espressif.com"
  "espressif-documentation|https://mcp.espressif.com/docs"
)

echo "=== Активация MCP-серверов (агент: $AGENT) ==="

# 1. Health-check каждого сервера (JSON-RPC initialize)
declare -a OK_LIST
for entry in "${SERVERS[@]}"; do
  name="${entry%%|*}"; url="${entry##*|}"
  code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 10 -X POST "$url" \
    -H "Content-Type: application/json" \
    -H "Accept: application/json, text/event-stream" \
    -d '{"jsonrpc":"2.0","method":"initialize","id":1,"params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"mcp-setup","version":"1"}}}' 2>/dev/null)
  if [ "$code" = "200" ]; then
    echo "[OK]   $name ($url)"
    OK_LIST+=("$entry")
  elif [ "$code" = "401" ] || [ "$code" = "403" ]; then
    echo "[AUTH] $name — требуется OAuth/регистрация: откройте $url в браузере"
  else
    echo "[FAIL] $name — HTTP $code"
  fi
done

# 2. Запись конфигурации в зависимости от агента
case "$AGENT" in
  claude)
    # Claude Code: project .mcp.json уже содержит http-секции; дописываем через CLI, если он установлен
    if command -v claude >/dev/null; then
      for entry in "${OK_LIST[@]}"; do
        name="${entry%%|*}"; url="${entry##*|}"
        claude mcp add --transport http --scope project "$name" "$url" 2>/dev/null \
          && echo "[ADD]  $name -> Claude Code (project scope)"
      done
    else
      echo "! CLI 'claude' не найден — используйте .mcp.json проекта"
    fi
    ;;
  codex)
    CFG="$HOME/.codex/config.toml"; mkdir -p "$HOME/.codex"; touch "$CFG"
    for entry in "${OK_LIST[@]}"; do
      name="${entry%%|*}"; url="${entry##*|}"
      if ! grep -q "\[mcp_servers.$name\]" "$CFG"; then
        printf '\n[mcp_servers.%s]\nurl = "%s"\n' "$name" "$url" >> "$CFG"
        echo "[ADD]  $name -> $CFG"
      else
        echo "[SKIP] $name уже в $CFG"
      fi
    done
    ;;
  generic)
    OUT=".mcp.json"
    python3 - "$OUT" <<'PY'
import json, sys, os
out = sys.argv[1]
data = {"mcpServers": {}}
if os.path.exists(out):
    try: data = json.load(open(out))
    except Exception: pass
srv = data.setdefault("mcpServers", {})
for name, url in [
    ("esp-component-registry","https://components.espressif.com/mcp"),
    ("esp-pilot-mcp","https://mcp.esp-pilot.espressif.com/mcp"),
    ("esp-vision","https://mcp.vision.espressif.com"),
    ("espressif-documentation","https://mcp.espressif.com/docs"),
]:
    srv[name] = {"type":"http","url":url}
json.dump(data, open(out,"w"), indent=2, ensure_ascii=False)
print(f"[WRITE] {out} — 4 http-сервера")
PY
    ;;
esac

echo
echo "Готово. Перезапустите агент, чтобы он перечитал конфиг."
echo "Для сервера espressif-documentation (401) пройдите регистрацию на https://mcp.espressif.com/docs"
