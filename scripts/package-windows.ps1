[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [Parameter(Mandatory=$true)][string]$QtSourceNotices
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
$version = (Get-Content (Join-Path $repo 'VERSION') -Raw).Trim()
$qtConfig = Get-Content (Join-Path $repo 'packaging/qt.json') -Raw | ConvertFrom-Json
$qtVersion = (& qmake -query QT_VERSION).Trim()
if ($LASTEXITCODE -ne 0 -or $qtVersion -ne $qtConfig.version) { throw 'Unexpected Qt version for release packaging' }
$qtBins = (& qmake -query QT_INSTALL_BINS).Trim()
$qtPlugins = (& qmake -query QT_INSTALL_PLUGINS).Trim()
$Executable = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path
$name = "FXB-Viewer-$version-windows-x64"
$stage = Join-Path $OutputDirectory $name
if (Test-Path $stage) { throw "Refusing to overwrite existing package directory: $stage" }
New-Item -ItemType Directory $stage | Out-Null
Copy-Item $Executable (Join-Path $stage 'fxbViewer.exe')
& (Join-Path $qtBins 'windeployqt.exe') --release --no-translations --no-plugins --no-opengl-sw --no-system-d3d-compiler --no-compiler-runtime (Join-Path $stage 'fxbViewer.exe')
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed' }
New-Item -ItemType Directory (Join-Path $stage 'platforms') | Out-Null
Copy-Item (Join-Path $qtPlugins 'platforms/qwindows.dll') (Join-Path $stage 'platforms')
# Use only Qtbase: optional SVG/image/style/network plugins are unnecessary.
foreach ($dll in Get-ChildItem $stage -Filter 'Qt6*.dll') {
    if ($dll.Name -notin @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll')) { throw "Unexpected Qt dependency: $($dll.Name)" }
}
if (-not $env:VCToolsRedistDir) { throw 'Run from the MSVC x64 developer environment' }
$crt = Get-ChildItem (Join-Path $env:VCToolsRedistDir 'x64') -Directory -Filter 'Microsoft.VC*.CRT' | Select-Object -First 1
if (-not $crt) { throw 'MSVC app-local runtime directory not found' }
Copy-Item (Join-Path $crt.FullName '*.dll') $stage
foreach ($required in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'vcruntime140.dll', 'msvcp140.dll', 'platforms/qwindows.dll')) {
    if (-not (Test-Path (Join-Path $stage $required))) { throw "Missing runtime dependency: $required" }
}
"[Paths]`nPrefix=.`nPlugins=.`n" | Set-Content (Join-Path $stage 'qt.conf') -Encoding ascii
Copy-Item (Join-Path $repo 'LICENSE') $stage
Copy-Item (Join-Path $repo 'packaging/THIRD_PARTY_NOTICES.txt') $stage
Copy-Item (Join-Path $repo 'packaging/README.txt') $stage
New-Item -ItemType Directory -Force (Join-Path $stage 'third-party') | Out-Null
Copy-Item $QtSourceNotices (Join-Path $stage 'third-party/qt') -Recurse
$sourceName = $qtConfig.source_url.Split('/')[-1]
if ((Get-FileHash (Join-Path $stage "third-party/qt/$sourceName") -Algorithm SHA256).Hash.ToLowerInvariant() -ne $qtConfig.source_sha256) { throw 'Packaged Qt source does not match lockfile' }
Copy-Item (Join-Path $repo 'packaging/MICROSOFT_RUNTIME_LICENSE.txt') $stage
$commit = (& git -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify source commit' }
$manifest = [ordered]@{
    application = 'FXB Viewer'; version = $version; commit = $commit
    qt = $qtVersion; architecture = 'x64'; compiler = 'MSVC 2022'; signed = $false
    files = @(Get-ChildItem $stage -File -Recurse | Sort-Object FullName | ForEach-Object {
        [ordered]@{path = [IO.Path]::GetRelativePath($stage, $_.FullName).Replace('\','/'); bytes = $_.Length; sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
    })
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $stage 'package-manifest.json') -Encoding utf8
$zip = Join-Path $OutputDirectory "$name.zip"
Compress-Archive -Path $stage -DestinationPath $zip
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $name.zip" | Set-Content (Join-Path $OutputDirectory 'SHA256SUMS.txt') -Encoding ascii
$pdb = Join-Path (Split-Path $Executable -Parent) 'fxbViewer.pdb'
if (-not (Test-Path $pdb)) { throw 'Missing release debugging symbols' }
$symbols = Join-Path $OutputDirectory "$name-symbols.zip"
Compress-Archive -Path $pdb -DestinationPath $symbols
"$((Get-FileHash $symbols -Algorithm SHA256).Hash.ToLowerInvariant())  $name-symbols.zip" | Add-Content (Join-Path $OutputDirectory 'SHA256SUMS.txt') -Encoding ascii
Write-Output $zip
