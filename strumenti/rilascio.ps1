# PS-RX - nuova release su GitHub (loddi-o/PS-RX), quella che app e plugin trovano con "Cerca aggiornamenti".
#
#   powershell -ExecutionPolicy Bypass -File strumenti\rilascio.ps1 -Versione 0.2.0 [-Note note.md] [-Prova]
#
# 1. scrive la versione nell'app (app/psrx/__init__.py) e nel plugin (app/decky/package.json) e fa il commit;
# 2. compila firmware (con confronto del codice macchina), exe Windows e plugin Decky; lancia i test;
# 3. prepara i file con i nomi che gli aggiornamenti cercano (vedi app/psrx/aggiornamenti.py):
#      ps-rx-firmware-X.Y.Z.uf2 / .bin, PS-RX-Setup-X.Y.Z.exe, ps-rx-decky-X.Y.Z.zip, ps-rx-X.Y.Z.html
# 4. tag vX.Y.Z, push, release "Latest"; le release precedenti diventano prerelease.
# -Prova si ferma prima del commit e della pubblicazione (file in %USERPROFILE%\.ds5-build\rilascio\X.Y.Z).

param(
    [Parameter(Mandatory = $true)][ValidatePattern('^\d+\.\d+\.\d+$')][string]$Versione,
    [string]$Note = '',
    [switch]$Prova
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$uscita = Join-Path $env:USERPROFILE ".ds5-build\rilascio\$Versione"
$tag = "v$Versione"

Push-Location $repo
try {
    if (-not $Prova) {
        if (git status --porcelain) { throw 'ci sono modifiche non salvate: fai prima il commit' }
        if (git tag --list $tag) { throw "il tag $tag esiste gia'" }
    }

    # 1. versione nell'app e nel plugin
    # UTF-8 senza BOM (Set-Content -Encoding utf8 di PowerShell 5 lo aggiunge e json.load lo rifiuta)
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    $init = Join-Path $repo 'app\psrx\__init__.py'
    [IO.File]::WriteAllText($init, ([IO.File]::ReadAllText($init) -replace "VERSIONE_APP = '[^']*'", "VERSIONE_APP = '$Versione'"), $utf8)
    $pkg = Join-Path $repo 'app\decky\package.json'
    [IO.File]::WriteAllText($pkg, ([IO.File]::ReadAllText($pkg) -replace '"version": "[^"]*"', "`"version`": `"$Versione`""), $utf8)

    # 2. build e test
    & powershell -ExecutionPolicy Bypass -File strumenti\prova_logica.ps1
    if ($LASTEXITCODE -ne 0) { throw 'test della logica falliti' }
    Push-Location app
    try { python -m unittest discover -s test; if ($LASTEXITCODE -ne 0) { throw 'test Python falliti' } } finally { Pop-Location }
    & powershell -ExecutionPolicy Bypass -File strumenti\compila_firmware.ps1 -Versione "ps-rx-$Versione"
    if ($LASTEXITCODE -ne 0) { throw 'firmware non compilato' }
    & powershell -ExecutionPolicy Bypass -File strumenti\compila_app.ps1
    if ($LASTEXITCODE -ne 0) { throw 'app non compilata' }
    & powershell -ExecutionPolicy Bypass -File strumenti\compila_installer.ps1 -Versione $Versione
    if ($LASTEXITCODE -ne 0) { throw 'installer non compilato' }
    & powershell -ExecutionPolicy Bypass -File strumenti\compila_decky.ps1
    if ($LASTEXITCODE -ne 0) { throw 'plugin non compilato' }

    # 3. file della release
    New-Item -ItemType Directory -Force $uscita | Out-Null
    Get-ChildItem $uscita | Remove-Item -Force
    $fw = Join-Path $env:USERPROFILE '.ds5-build\build-psrx\normale'
    Copy-Item "$fw\ds5-bridge.uf2" "$uscita\ps-rx-firmware-$Versione.uf2"
    Copy-Item "$fw\ds5-bridge.bin" "$uscita\ps-rx-firmware-$Versione.bin"
    Copy-Item (Join-Path $env:USERPROFILE ".ds5-build\build-app\installer\PS-RX-Setup-$Versione.exe") "$uscita\PS-RX-Setup-$Versione.exe"
    Copy-Item (Join-Path $env:USERPROFILE ".ds5-build\build-psrx\app\ps-rx-decky-$Versione.zip") "$uscita\ps-rx-decky-$Versione.zip"
    Copy-Item 'web\ps-rx.html' "$uscita\ps-rx-$Versione.html"
    $controllo = python -c "import sys; sys.path.insert(0, 'app'); from psrx.firmware import leggi; print(leggi(r'$uscita\ps-rx-firmware-$Versione.uf2').versione)"
    if ($controllo -ne "ps-rx-$Versione") { throw "il firmware riporta la versione '$controllo'" }
    Write-Host "[PS-RX] file della release in $uscita"
    Get-ChildItem $uscita | Format-Table Name, Length -AutoSize | Out-Host
    if ($Prova) { Write-Host '[PS-RX] prova: niente commit ne'' pubblicazione'; return }

    # 4. commit, tag, release
    if (git status --porcelain $init $pkg) {
        git add $init $pkg
        git commit -m "Versione $Versione`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
    }
    git tag -a $tag -m "PS-RX $Versione"
    git push origin main $tag
    $precedenti = gh release list --repo loddi-o/PS-RX --json tagName --jq '.[].tagName'
    $argomenti = @($tag, '--repo', 'loddi-o/PS-RX', '--title', "PS-RX $Versione", '--latest')
    if ($Note) { $argomenti += @('--notes-file', $Note) } else { $argomenti += @('--generate-notes') }
    gh release create @argomenti (Get-ChildItem $uscita).FullName
    foreach ($t in $precedenti) { if ($t -and $t -ne $tag) { gh release edit $t --repo loddi-o/PS-RX --prerelease | Out-Null } }
    Write-Host "[PS-RX] release $tag pubblicata: https://github.com/loddi-o/PS-RX/releases/tag/$tag"
} finally {
    Pop-Location
}
