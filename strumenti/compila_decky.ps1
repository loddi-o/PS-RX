# PS-RX - compila il plugin Decky e crea lo zip da installare su SteamOS.
#
#   powershell -ExecutionPolicy Bypass -File strumenti\compila_decky.ps1 [-Uscita cartella]
#
# Lavora in %USERPROFILE%\.ds5-build\build-psrx\decky (fuori da Nextcloud): copia app\decky,
# mette la libreria app\psrx in py_modules\, installa le dipendenze con pnpm (corepack di
# Node.js), compila il frontend con rollup e crea ps-rx-decky-<versione>.zip con la cartella
# ps-rx-decky\ dentro (il formato che Decky Loader accetta con "Installa plugin da ZIP").

param(
    [string]$Uscita = (Join-Path $env:USERPROFILE '.ds5-build\build-psrx\app')
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$decky = Join-Path $repo 'app\decky'
$lavoro = Join-Path $env:USERPROFILE '.ds5-build\build-psrx\decky'
$python = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python" -Recurse -Filter python.exe -ErrorAction SilentlyContinue |
           Select-Object -First 1).FullName
if (-not $python) { $python = (Get-Command python).Source }

New-Item -ItemType Directory -Force $lavoro, $Uscita | Out-Null
robocopy $decky $lavoro /MIR /XD node_modules dist py_modules out /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw 'copia del plugin fallita' }
robocopy (Join-Path $repo 'app\psrx') (Join-Path $lavoro 'py_modules\psrx') *.py /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw 'copia della libreria fallita' }

# Firmware incluso nel plugin (procedura "Nuovo ricevitore" senza internet): l'ultimo compilato.
$fwBuild = Join-Path $env:USERPROFILE '.ds5-build\build-psrx\normale\ds5-bridge.uf2'
New-Item -ItemType Directory -Force (Join-Path $lavoro 'firmware') | Out-Null
if (Test-Path $fwBuild) { Copy-Item $fwBuild (Join-Path $lavoro 'firmware\ps-rx-firmware.uf2') -Force }

Push-Location $lavoro
try {
    # pnpm scrive avvisi su stderr: in Windows PowerShell 5.1 non devono diventare errori fatali.
    $ErrorActionPreference = 'Continue'
    if (-not (Test-Path 'node_modules\@decky\ui')) {
        Write-Host '[PS-RX] Installo le dipendenze del plugin (pnpm)'
        corepack pnpm@9.15.9 install --reporter=silent 2>&1 | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'pnpm install fallito' }
    }
    Write-Host '[PS-RX] Compilo il frontend del plugin'
    corepack pnpm@9.15.9 run build 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) { throw 'compilazione del frontend fallita' }
} finally {
    Pop-Location
    $ErrorActionPreference = 'Stop'
}

# Zip con le barre "/" (Compress-Archive di PowerShell 5 scrive "\"): lo crea Python.
$versione = (Get-Content (Join-Path $decky 'package.json') -Raw | ConvertFrom-Json).version
$zip = Join-Path $Uscita "ps-rx-decky-$versione.zip"
$script = @"
import os, sys, zipfile
lavoro, zip_path = sys.argv[1], sys.argv[2]
voci = ['plugin.json', 'package.json', 'main.py', 'LICENSE', 'README.md', 'dist', 'py_modules', 'firmware']
with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as z:
    for voce in voci:
        percorso = os.path.join(lavoro, voce)
        if os.path.isfile(percorso):
            z.write(percorso, 'ps-rx-decky/' + voce)
            continue
        for radice, cartelle, file in os.walk(percorso):
            cartelle[:] = [c for c in cartelle if c != '__pycache__']
            for f in file:
                completo = os.path.join(radice, f)
                z.write(completo, 'ps-rx-decky/' + os.path.relpath(completo, lavoro).replace(os.sep, '/'))
print('[PS-RX] OK:', zip_path)
"@
$script | & $python - $lavoro $zip
if ($LASTEXITCODE -ne 0) { throw 'creazione dello zip fallita' }
