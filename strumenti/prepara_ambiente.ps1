# PS-RX - prepara l'ambiente di compilazione su Windows (una volta sola).
#
#   powershell -ExecutionPolicy Bypass -File strumenti\prepara_ambiente.ps1
#
# Tutto finisce in %USERPROFILE%\.ds5-build (fuori da Nextcloud), condiviso con il progetto PS250:
# 1. controlla toolchain ARM e Pico SDK 2.2.0 (installati dallo script di PS250 o a mano)
# 2. pico-sdk-dlb: lo stesso SDK con TinyUSB al commit fissato dalla CI di DS5-Linux-Bridge
# 3. picotool per Windows
# 4. opus in firmware\lib\opus (non e' nella repo)
# 5. copia pulita di DS5-Linux-Bridge v2.3.0-beta.1 (riferimento per heap e codice macchina)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$fw = Join-Path $repo 'firmware'
$ds5 = Join-Path $env:USERPROFILE '.ds5-build'

$dlbUrl = 'https://github.com/kungaa/ds5-linux-bridge.git'
$dlbCommit = '1e15ea2d47d302437580c3b13850ec937cbc1c88'      # tag v2.3.0-beta.1
$tinyusbDlb = '08f9855e3dc51291a8c30e2eeccab31f8f07d0a9'     # TINYUSB_REF della CI di DS5-Linux-Bridge
$opusUrl = 'https://github.com/xiph/opus'
$opusCommit = '2d862ea14b233e5a3f3afaf74d96050691af3cd5'     # sottomodulo lib/opus di DS5-Linux-Bridge

# --- 1. toolchain + SDK -------------------------------------------------------------
if (-not (Test-Path "$ds5\pico-sdk\pico_sdk_init.cmake") -or -not (Test-Path "$ds5\arm-gnu-toolchain\bin")) {
    throw ("Mancano toolchain ARM e Pico SDK in $ds5. Installali con strumenti\prepara_ambiente.ps1 del " +
           "progetto PS250 (usa lo script di DS5Dongle), oppure mettili a mano in arm-gnu-toolchain e pico-sdk.")
}
Write-Host '[PS-RX] Toolchain e Pico SDK presenti in' $ds5

# --- 2. pico-sdk-dlb ------------------------------------------------------------------
$sdkDlb = Join-Path $ds5 'pico-sdk-dlb'
if (-not (Test-Path "$sdkDlb\pico_sdk_init.cmake")) {
    Write-Host '[PS-RX] Creo pico-sdk-dlb (copia di pico-sdk)'
    robocopy "$ds5\pico-sdk" $sdkDlb /E /MT:16 /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw 'copia di pico-sdk fallita' }
}
$ErrorActionPreference = 'Continue'
git -C "$sdkDlb\lib\tinyusb" cat-file -e "$tinyusbDlb^{commit}" 2>$null
if ($LASTEXITCODE -ne 0) { git -C "$sdkDlb\lib\tinyusb" fetch --quiet origin $tinyusbDlb }
git -C "$sdkDlb\lib\tinyusb" checkout --quiet --detach $tinyusbDlb
$ErrorActionPreference = 'Stop'
Write-Host "[PS-RX] pico-sdk-dlb: TinyUSB @ $($tinyusbDlb.Substring(0,8))"

# --- 3. picotool --------------------------------------------------------------------------
$pt = Join-Path $ds5 'picotool-win\picotool\picotool.exe'
if (-not (Test-Path $pt)) {
    Write-Host '[PS-RX] Scarico picotool 2.3.1 per Windows'
    $zip = Join-Path $ds5 'picotool.zip'
    Invoke-WebRequest -UseBasicParsing -OutFile $zip `
        -Uri 'https://github.com/raspberrypi/pico-sdk-tools/releases/download/v2.3.1-0/picotool-2.3.1-x64-win.zip'
    Expand-Archive -Force $zip (Join-Path $ds5 'picotool-win')
}

# --- 4. opus ----------------------------------------------------------------------------------
$opus = Join-Path $fw 'lib\opus'
if (-not (Test-Path (Join-Path $opus 'CMakeLists.txt'))) {
    Write-Host "[PS-RX] Scarico firmware\lib\opus @ $($opusCommit.Substring(0,8))"
    $locale = Join-Path $ds5 'dlb-pulita\lib\opus'
    $ErrorActionPreference = 'Continue'
    if (Test-Path (Join-Path $locale '.git')) { git clone --quiet $locale $opus } else { git clone --quiet $opusUrl $opus }
    git -C $opus checkout --quiet $opusCommit
    $ErrorActionPreference = 'Stop'
}

# --- 5. copia pulita di DS5-Linux-Bridge ---------------------------------------------------------
$pulita = Join-Path $ds5 'dlb-pulita'
if (-not (Test-Path (Join-Path $pulita 'CMakeLists.txt'))) {
    Write-Host '[PS-RX] Scarico la copia pulita di DS5-Linux-Bridge v2.3.0-beta.1'
    $ErrorActionPreference = 'Continue'
    git clone --quiet $dlbUrl $pulita
    git -C $pulita checkout --quiet $dlbCommit
    git clone --quiet $opus (Join-Path $pulita 'lib\opus')
    git -C (Join-Path $pulita 'lib\opus') checkout --quiet $opusCommit
    $ErrorActionPreference = 'Stop'
}

Write-Host '[PS-RX] Ambiente pronto: strumenti\compila_firmware.ps1'
