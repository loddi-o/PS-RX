# PS-RX - app Windows con PyInstaller: cartella con PS-RX.exe (un solo processo, senza console).
# Non "un file": l'exe a file unico lancia due processi e il lanciatore resta vivo quando Windows o un
# installer chiudono l'app (Restart Manager), tenendo bloccato l'exe durante gli aggiornamenti.
#
#   powershell -ExecutionPolicy Bypass -File strumenti\compila_app.ps1
#
# Serve Python con PySide6-Essentials e PyInstaller (pip install PySide6-Essentials pyinstaller).
# Risultato: %USERPROFILE%\.ds5-build\build-app\dist\PS-RX\PS-RX.exe (la cartella la installa l'installer)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$app = Join-Path $repo 'app'
$lavoro = Join-Path $env:USERPROFILE '.ds5-build\build-app'
New-Item -ItemType Directory -Force $lavoro | Out-Null

# Icona dell'exe disegnata dall'app stessa (nessun file binario nella repo).
$ico = Join-Path $lavoro 'ps-rx.ico'
$env:QT_QPA_PLATFORM = 'offscreen'
python -c "import sys; sys.path.insert(0, r'$app'); from PySide6.QtGui import QGuiApplication; a = QGuiApplication([]); from psrx_app.icona import salva_ico; sys.exit(0 if salva_ico(r'$ico') else 1)"
if ($LASTEXITCODE -ne 0) { throw 'icona non creata' }
Remove-Item Env:\QT_QPA_PLATFORM

# Firmware incluso nell'exe (procedura "Prepara un nuovo ricevitore" senza internet): l'ultimo compilato.
$fwIncluso = Join-Path $lavoro 'ps-rx-firmware.uf2'
$fwBuild = Join-Path $env:USERPROFILE '.ds5-build\build-psrx\normale\ds5-bridge.uf2'
if (Test-Path $fwBuild) { Copy-Item $fwBuild $fwIncluso -Force } else { Write-Host '[PS-RX] attenzione: nessun firmware da includere' }

# Moduli Qt non usati: fuori, l'exe resta piu' piccolo.
$fuori = @('PySide6.QtQml', 'PySide6.QtQuick', 'PySide6.QtPdf', 'PySide6.QtOpenGL', 'PySide6.QtSql',
           'PySide6.QtTest', 'PySide6.QtXml', 'PySide6.QtConcurrent', 'PySide6.QtDBus', 'PySide6.QtSvg',
           'PySide6.QtDesigner', 'PySide6.QtHelp', 'PySide6.QtMultimedia', 'PySide6.QtWebEngineCore',
           'tkinter', 'unittest', 'pydoc')
$args_ = @('--noconfirm', '--clean', '--onedir', '--windowed', '--name', 'PS-RX', '--icon', $ico,
           '--paths', $app, '--distpath', (Join-Path $lavoro 'dist'), '--workpath', (Join-Path $lavoro 'build'),
           '--specpath', $lavoro)
foreach ($m in $fuori) { $args_ += @('--exclude-module', $m) }
if (Test-Path $fwIncluso) { $args_ += @('--add-data', "$fwIncluso;firmware") }
python -m PyInstaller @args_ (Join-Path $app 'ps-rx.pyw')
if ($LASTEXITCODE -ne 0) { throw 'PyInstaller non riuscito' }

$exe = Join-Path $lavoro 'dist\PS-RX\PS-RX.exe'
$mb = [math]::Round((Get-Item $exe).Length / 1MB, 1)
Write-Host "[PS-RX] app: $exe ($mb MB)"
