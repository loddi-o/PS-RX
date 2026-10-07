# PS-RX - controllo rapido del ricevitore collegato a questo PC Windows.
#
#   powershell -ExecutionPolicy Bypass -File strumenti\verifica_usb.ps1
#
# Mostra come Windows vede il ricevitore (in ogni modalita': PlayStation 054C, Xbox 1209:0001,
# Steam 28DE:1304) e prova a parlarci con la libreria psrx.

$repo = Split-Path -Parent $PSScriptRoot
Write-Host '[PS-RX] Dispositivi USB del ricevitore visti da Windows:'
Get-PnpDevice -PresentOnly |
    Where-Object { $_.InstanceId -match 'VID_054C|VID_1209&PID_0001|VID_28DE&PID_1304' } |
    Format-Table Status, Class, FriendlyName, InstanceId -AutoSize | Out-String -Width 220 | Write-Host

Write-Host '[PS-RX] Stato letto dalla libreria (WinUSB):'
Push-Location (Join-Path $repo 'app')
try { python -m psrx stato } finally { Pop-Location }
