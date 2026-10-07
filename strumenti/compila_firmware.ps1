# PS-RX - compila il firmware (base DS5-Linux-Bridge v2.3.0-beta.1).
#
#   powershell -ExecutionPolicy Bypass -File strumenti\compila_firmware.ps1
#   powershell -ExecutionPolicy Bypass -File strumenti\compila_firmware.ps1 -Versione ps-rx-0.1.0
#   powershell -ExecutionPolicy Bypass -File strumenti\compila_firmware.ps1 -SenzaConfronto
#
# 1. test della logica sul PC (strumenti\prova_logica.ps1)
# 2. firmware ufficiale in %USERPROFILE%\.ds5-build\build-psrx\normale (fuori da Nextcloud: il
#    client di sincronizzazione blocca i file della build)
# 3. build pulita di DS5-Linux-Bridge con le stesse opzioni, come riferimento
# 4. heap e confronto del codice macchina dei percorsi critici (strumenti\confronta_codice.py)
#
# Prima volta: strumenti\prepara_ambiente.ps1

param(
    [string]$Versione = 'ps-rx-dev',
    [switch]$SenzaConfronto
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$fw = Join-Path $repo 'firmware'
$ds5 = Join-Path $env:USERPROFILE '.ds5-build'
$sdk = Join-Path $ds5 'pico-sdk-dlb'
$build = Join-Path $ds5 'build-psrx'

$env:Path = "$ds5\arm-gnu-toolchain\bin;$ds5\mingw64\mingw64\bin;C:\Program Files\CMake\bin;" +
            "$env:LOCALAPPDATA\Microsoft\WinGet\Links;$env:Path"
$python = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python" -Recurse -Filter python.exe -ErrorAction SilentlyContinue |
           Select-Object -First 1).FullName
if (-not $python) { $python = (Get-Command python).Source }

if (-not (Test-Path "$fw\lib\opus\CMakeLists.txt") -or -not (Test-Path "$sdk\pico_sdk_init.cmake")) {
    throw 'manca l''ambiente: lancia strumenti\prepara_ambiente.ps1'
}

# Opzioni uguali per PS-RX e per il riferimento. WiFi acceso (Wake-on-LAN), niente OTA da GitHub,
# niente LED del Pico per la batteria (la batteria la segnalano app e plugin).
$opzioni = @('-DENABLE_WIFI_WOL=ON', '-DENABLE_OTA=OFF', '-DENABLE_BATT_LED=OFF')

# --- 1. test della logica --------------------------------------------------------------
& (Join-Path $PSScriptRoot 'prova_logica.ps1')

function Compila([string]$sorgente, [string]$cartella, [string]$versione) {
    # CMake e ninja scrivono anche su stderr: in Windows PowerShell 5.1 con 'Stop' ogni riga
    # diventerebbe un errore fatale. Qui conta solo il codice d'uscita.
    $ErrorActionPreference = 'Continue'
    New-Item -ItemType Directory -Force $cartella | Out-Null
    $log = Join-Path $cartella 'build.log'
    cmake -S $sorgente -B $cartella -G Ninja -DCMAKE_BUILD_TYPE=Release "-DPICO_SDK_PATH=$sdk" `
        "-DPython3_EXECUTABLE=$($python -replace '\\','/')" @opzioni "-DVERSION=$versione" *> $log
    if ($LASTEXITCODE -ne 0) { Get-Content $log -Tail 30; throw "configurazione fallita ($sorgente)" }
    cmake --build $cartella --target ds5-bridge *>> $log
    $ok = $LASTEXITCODE -eq 0
    $rumore = 'lib[/\\]opus|#warning|^\s+\d+ \||NativeCommandError|CategoryInfo|FullyQualifiedErrorId|' +
              '^\s*\+ |^In [A-Z]:\\|^\s*\* OPUS_|^\s*~+$'
    Get-Content $log | Where-Object { $_ -notmatch $rumore } |
        Select-String -Pattern 'error|warning:|FAILED|size guard' | ForEach-Object { $_.Line }
    if (-not $ok) { throw "compilazione fallita ($sorgente): log completo in $log" }
}

# --- 2. firmware --------------------------------------------------------------------------
$normale = Join-Path $build 'normale'
Write-Host "[PS-RX] Compilo il firmware $Versione"
Compila $fw $normale $Versione
Write-Host "[PS-RX] OK: $normale\ds5-bridge.uf2"
if ($SenzaConfronto) { & $python (Join-Path $PSScriptRoot 'confronta_codice.py') --solo-heap "$normale\ds5-bridge.elf"; return }

# --- 3. riferimento: DS5-Linux-Bridge pulito, stesse opzioni ---------------------------------
$pulita = Join-Path $ds5 'dlb-pulita'
$rif = Join-Path $build 'riferimento'
Write-Host '[PS-RX] Compilo il riferimento (DS5-Linux-Bridge v2.3.0-beta.1 pulito)'
Compila $pulita $rif 'dlb-v2.3.0-beta.1'

# --- 4. heap e codice macchina ---------------------------------------------------------------
& $python (Join-Path $PSScriptRoot 'confronta_codice.py') "$rif\ds5-bridge.elf" "$normale\ds5-bridge.elf"
if ($LASTEXITCODE -ne 0) { throw 'confronto del codice macchina: funzioni critiche cambiate senza motivo' }
