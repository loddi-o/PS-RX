# PS-RX - test della logica pura del firmware sul PC.
#
#   powershell -ExecutionPolicy Bypass -File strumenti\prova_logica.ps1
#
# Compila con g++ (MinGW di .ds5-build) i moduli di firmware\src\psrx che non toccano l'hardware
# insieme a firmware\test\test_logica.cpp, e li esegue.

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$fw = Join-Path $repo 'firmware'
$ds5 = Join-Path $env:USERPROFILE '.ds5-build'
$env:Path = "$ds5\mingw64\mingw64\bin;$env:Path"

$src = Join-Path $fw 'src\psrx'
# Moduli di sola logica (niente SDK del Pico): l'elenco cresce con le fasi.
$moduli = @(Get-Content (Join-Path $fw 'test\moduli.txt') | Where-Object { $_ -and -not $_.StartsWith('#') } |
    ForEach-Object { Join-Path $src $_.Trim() })
$exe = Join-Path $ds5 'build-psrx\test_logica.exe'
New-Item -ItemType Directory -Force (Split-Path $exe) | Out-Null

Write-Host '[PS-RX] Compilo i test della logica'
g++ -std=c++17 -O1 -Wall -Wextra -Werror -DPSRX_TEST_HOST -I"$src" (Join-Path $fw 'test\test_logica.cpp') @moduli -o $exe
if ($LASTEXITCODE -ne 0) { throw 'compilazione dei test fallita' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'test della logica FALLITI' }
