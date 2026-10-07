# PS-RX - carica un firmware sul Pico con picotool.
#
#   powershell -ExecutionPolicy Bypass -File strumenti\flash.ps1 release\ps-rx-0.1.0\ps-rx-0.1.0.uf2
#
# Il Pico deve essere in BOOTSEL: tieni premuto BOOTSEL mentre colleghi l'USB, oppure (con PS-RX in
# funzione) usa "Riavvia in modalita' aggiornamento" dall'app. Di solito non serve: l'app aggiorna il
# firmware via USB.

param([Parameter(Mandatory = $true)][string]$Uf2)

$ErrorActionPreference = 'Stop'
$pt = Join-Path $env:USERPROFILE '.ds5-build\picotool-win\picotool\picotool.exe'
if (-not (Test-Path $pt)) { throw 'picotool non trovato: lancia strumenti\prepara_ambiente.ps1' }

Write-Host '[PS-RX] Attendo il Pico in BOOTSEL (30 s)...'
for ($i = 0; $i -lt 30; $i++) {
    if ((& $pt info 2>$null) -match 'Program Information') { break }
    Start-Sleep 1
}
& $pt load -v -x $Uf2
