# mcp-setup.ps1 — активация MCP-серверов Espressif (Windows)
# Запуск:  powershell -ExecutionPolicy Bypass -File .\tools\mcp-setup.ps1 [-Agent claude|codex|generic]
param([string]$Agent = "generic")

$Servers = @(
    @{Name="esp-component-registry"; Url="https://components.espressif.com/mcp"},
    @{Name="esp-pilot-mcp";          Url="https://mcp.esp-pilot.espressif.com/mcp"},
    @{Name="esp-vision";             Url="https://mcp.vision.espressif.com"},
    @{Name="espressif-documentation";Url="https://mcp.espressif.com/docs"}
)

Write-Host "=== Активация MCP-серверов (агент: $Agent) ===" -ForegroundColor Cyan
$Ok = @()
$body = '{"jsonrpc":"2.0","method":"initialize","id":1,"params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"mcp-setup","version":"1"}}}'
foreach ($s in $Servers) {
    try {
        $r = Invoke-WebRequest -Uri $s.Url -Method Post -ContentType "application/json" `
             -Headers @{Accept="application/json, text/event-stream"} -Body $body -TimeoutSec 10 -UseBasicParsing
        Write-Host ("[OK]   {0}" -f $s.Name) -ForegroundColor Green
        $Ok += $s
    } catch {
        $code = $_.Exception.Response.StatusCode.value__
        if ($code -eq 401 -or $code -eq 403) {
            Write-Host ("[AUTH] {0} — регистрация: откройте {1} в браузере" -f $s.Name, $s.Url) -ForegroundColor Yellow
            Start-Process $s.Url
        } else {
            Write-Host ("[FAIL] {0} — {1}" -f $s.Name, $code) -ForegroundColor Red
        }
    }
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
