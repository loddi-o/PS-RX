# PS-RX - installer Windows (Inno Setup 6) dall'exe di strumenti\compila_app.ps1.
#
#   powershell -ExecutionPolicy Bypass -File strumenti\compila_installer.ps1 -Versione 0.1.1
#
# Risultato: %USERPROFILE%\.ds5-build\build-app\installer\PS-RX-Setup-X.Y.Z.exe

param([Parameter(Mandatory = $true)][ValidatePattern('^\d+\.\d+\.\d+$')][string]$Versione)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$lavoro = Join-Path $env:USERPROFILE '.ds5-build\build-app'
$iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
          "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw 'Inno Setup 6 non trovato (winget install JRSoftware.InnoSetup)' }
$exe = Join-Path $lavoro 'dist\PS-RX.exe'
$ico = Join-Path $lavoro 'ps-rx.ico'
if (-not (Test-Path $exe)) { throw 'manca l''exe: lancia prima strumenti\compila_app.ps1' }
$uscita = Join-Path $lavoro 'installer'
New-Item -ItemType Directory -Force $uscita | Out-Null

& $iscc /Q "/DVersione=$Versione" "/DExe=$exe" "/DIcona=$ico" "/O$uscita" (Join-Path $repo 'app\installer\ps-rx.iss')
if ($LASTEXITCODE -ne 0) { throw 'installer non compilato' }
$file = Join-Path $uscita "PS-RX-Setup-$Versione.exe"
Write-Host "[PS-RX] installer: $file ($([math]::Round((Get-Item $file).Length / 1MB, 1)) MB)"
