# mcp-setup.ps1 — активация MCP-серверов Espressif (Windows)
# Запуск:  powershell -ExecutionPolicy Bypass -File .\tools\mcp-setup.ps1 [-Agent claude|codex|generic]
param(
    [string]$Agent = "generic",
    [ValidateSet("check","auth")]
    [string]$Mode = "check"
)

# Важные URL:
#  - Url      — endpoint MCP-транспорта (Streamable HTTP). Принимает ТОЛЬКО POST с JSON-RPC.
#               Открытие этого адреса в браузере (GET) ВСЕГДА даёт «Method Not Allowed» (405) — это норма!
#  - AuthUrl  — человекочитаемая страница регистрации/OAuth для открытия в браузере.
$Servers = @(
    @{Name="esp-component-registry";  Url="https://components.espressif.com/mcp";       AuthUrl="https://components.espressif.com/"},
    @{Name="esp-pilot-mcp";           Url="https://mcp.esp-pilot.espressif.com/mcp";    AuthUrl="https://mcp.esp-pilot.espressif.com/"},
    @{Name="esp-vision";              Url="https://mcp.vision.espressif.com";           AuthUrl="https://mcp.vision.espressif.com/"},
    @{Name="espressif-documentation"; Url="https://mcp.espressif.com/docs";             AuthUrl="https://mcp.espressif.com/"}
)

Write-Host "=== Активация MCP-серверов (агент: $Agent, режим: $Mode) ===" -ForegroundColor Cyan
$Ok = @()
$body = '{"jsonrpc":"2.0","method":"initialize","id":1,"params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"mcp-setup","version":"1"}}}'

foreach ($s in $Servers) {
    # --- Режим AUTH: открыть окно регистрации БЕЗ проверки транспорта ---
    if ($Mode -eq "auth") {
        Write-Host ("[OPEN] {0} -> {1}" -f $s.Name, $s.AuthUrl) -ForegroundColor Yellow
        Start-Process $s.AuthUrl
        continue
    }

    # --- Режим CHECK: JSON-RPC initialize по POST (браузерный GET тут запрещён) ---
    try {
        $r = Invoke-WebRequest -Uri $s.Url -Method Post -ContentType "application/json" `
             -Headers @{Accept="application/json, text/event-stream"} -Body $body -TimeoutSec 10 -UseBasicParsing
        Write-Host ("[OK]   {0}" -f $s.Name) -ForegroundColor Green
        $Ok += $s
    } catch {
        $code = $null
        try { $code = $_.Exception.Response.StatusCode.value__ } catch {}
        switch ($code) {
            401 { # OAuth required — открываем СТРАНИЦУ РЕГИСТРАЦИИ (не MCP-endpoint!)
                Write-Host ("[AUTH] {0} — требуется регистрация, открываю окно: {1}" -f $s.Name, $s.AuthUrl) -ForegroundColor Yellow
                Start-Process $s.AuthUrl
            }
            403 {
                Write-Host ("[AUTH] {0} — доступ запрещён (нужен аккаунт), открываю: {1}" -f $s.Name, $s.AuthUrl) -ForegroundColor Yellow
                Start-Process $s.AuthUrl
            }
            405 {
                # Это НЕ ошибка сервера: означает, что дошло до Streamable HTTP, но метод не тот.
                Write-Host ("[OK?]  {0} — endpoint живой (405 = нужен POST, так и задумано)" -f $s.Name) -ForegroundColor DarkGreen
                $Ok += $s
            }
            $null {
                Write-Host ("[FAIL] {0} — сетевая ошибка: {1}" -f $s.Name, $_.Exception.Message) -ForegroundColor Red
            }
            default {
                Write-Host ("[FAIL] {0} — HTTP {1}" -f $s.Name, $code) -ForegroundColor Red
            }
        }
    }
}

if ($Mode -eq "auth") {
    Write-Host "`nОкна регистрации открыты. После регистрации перезапустите скрипт без -Mode auth." -ForegroundColor Cyan
    return
}

switch ($Agent) {
    "claude" {
        if (Get-Command claude -ErrorAction SilentlyContinue) {
            foreach ($s in $Ok) {
                claude mcp add --transport http --scope project $s.Name $s.Url
                Write-Host ("[ADD]  {0} -> Claude Code" -f $s.Name) -ForegroundColor Green
            }
        } else { Write-Host "! CLI 'claude' не найден — используйте .mcp.json проекта" -ForegroundColor Yellow }
    }
    "codex" {
        $cfg = "$env:USERPROFILE\.codex\config.toml"
        New-Item -ItemType Directory -Force (Split-Path $cfg) | Out-Null
        if (-not (Test-Path $cfg)) { New-Item $cfg -ItemType File | Out-Null }
        foreach ($s in $Ok) {
            if (-not (Select-String -Path $cfg -SimpleMatch "[mcp_servers.$($s.Name)]" -Quiet)) {
                Add-Content $cfg "`n[mcp_servers.$($s.Name)]`nurl = `"$($s.Url)`""
                Write-Host ("[ADD]  {0} -> config.toml" -f $s.Name) -ForegroundColor Green
            }
        }
    }
    default {
        $json = @{ mcpServers = [ordered]@{} }
        foreach ($s in $Servers) { $json.mcpServers[$s.Name] = @{ type = "http"; url = $s.Url } }
        $json | ConvertTo-Json -Depth 4 | Set-Content ".mcp.json" -Encoding UTF8
        Write-Host "[WRITE] .mcp.json — 4 http-сервера" -ForegroundColor Green
    }
}
Write-Host "`nГотово. Перезапустите агент для перечитывания конфига." -ForegroundColor Cyan
