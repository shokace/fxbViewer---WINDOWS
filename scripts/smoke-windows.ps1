[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Archive, [Parameter(Mandatory=$true)][string]$EvidenceDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$Archive = (Resolve-Path $Archive).Path
New-Item -ItemType Directory -Force $EvidenceDirectory | Out-Null
$EvidenceDirectory = (Resolve-Path $EvidenceDirectory).Path
# Spaces and non-ASCII paths catch deployment/argument quoting regressions.
$unpack = Join-Path $EvidenceDirectory 'Extracted package Étoile'
if (Test-Path $unpack) { throw 'Use a fresh evidence directory' }
Expand-Archive $Archive $unpack
$root = (Get-ChildItem $unpack -Directory | Select-Object -First 1).FullName
$manifest = Get-Content (Join-Path $root 'package-manifest.json') -Raw | ConvertFrom-Json
foreach ($file in $manifest.files) {
    $path = Join-Path $root $file.path
    if (-not (Test-Path $path) -or (Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.sha256) { throw "Package hash mismatch: $($file.path)" }
}
$exe = Join-Path $root 'fxbViewer.exe'
# Windows VERSIONINFO has four numeric components; the app uses semver.
$nativeVersion = (Get-Item $exe).VersionInfo.ProductVersion
if ($nativeVersion -ne "$($manifest.version).0") { throw "Executable version mismatch: $nativeVersion vs $($manifest.version).0" }
$previousPath = $env:PATH
$previousPlatform = $env:QT_QPA_PLATFORM
$previousPlugin = $env:QT_PLUGIN_PATH
$previousPlatformPlugin = $env:QT_QPA_PLATFORM_PLUGIN_PATH
$previousQtdir = $env:QTDIR
try {
    # A fresh runner job has no Qt SDK. Also remove development PATH overrides.
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $env:QT_QPA_PLATFORM = 'windows'
    $env:QT_PLUGIN_PATH = $null
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $null
    $env:QTDIR = $null
    $tour = Join-Path $EvidenceDirectory 'tour'
    $process = Start-Process $exe -ArgumentList @('--smoke-test', "`"$tour`"") -WorkingDirectory $root -PassThru -RedirectStandardError (Join-Path $EvidenceDirectory 'stderr.log') -RedirectStandardOutput (Join-Path $EvidenceDirectory 'stdout.log')
    $modules = @()
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while (-not $process.HasExited -and $timer.Elapsed.TotalSeconds -lt 20) {
        $process.Refresh()
        if ($process.MainWindowHandle -ne 0) {
            $modules = @($process.Modules | Where-Object { $_.ModuleName -match '^(Qt6|qwindows|msvcp140|vcruntime140)' } | ForEach-Object { [ordered]@{name=$_.ModuleName; path=$_.FileName} })
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not $process.WaitForExit(45000)) { $process.Kill(); throw 'Packaged app timed out' }
    if ($process.ExitCode -ne 0) { throw "Packaged app failed: $($process.ExitCode)" }
    foreach ($required in @('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','qwindows.dll','msvcp140.dll','vcruntime140.dll')) {
        $found = @($modules | Where-Object { $_.name -eq $required })
        if ($found.Count -ne 1 -or -not $found[0].path.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw "Dependency did not load from package: $required" }
    }
    $modules | ConvertTo-Json | Set-Content (Join-Path $EvidenceDirectory 'loaded-modules.json')
    $report = @(Get-Content (Join-Path $tour 'tour.json') -Raw | ConvertFrom-Json)
    if ($report.Count -ne 4) { throw 'Missing smoke steps' }
    $counts = @(3072,3072,3072,24)
    for ($i = 0; $i -lt 4; $i++) {
        if ($report[$i].results -ne $counts[$i] -or $report[$i].typo_recovery -ne ($i -eq 2)) { throw "Bad search results at step $i" }
        if ((Get-Item (Join-Path $tour $report[$i].screenshot)).Length -lt 10000) { throw 'Missing or empty screenshot' }
    }
    # Failure paths must exit nonzero instead of hanging or reporting success.
    $blocker = Join-Path $EvidenceDirectory 'not-a-directory'
    'x' | Set-Content $blocker
    $bad = Start-Process $exe -ArgumentList @('--smoke-test', "`"$blocker`"") -WorkingDirectory $root -PassThru
    if (-not $bad.WaitForExit(10000)) { $bad.Kill(); throw 'Invalid-output check hung' }
    if ($bad.ExitCode -ne 2) { throw "Invalid-output check returned $($bad.ExitCode)" }
    [ordered]@{passed=$true; commit=$manifest.commit; version=$manifest.version; qt=$manifest.qt; os=[Environment]::OSVersion.VersionString; archive_sha256=(Get-FileHash $Archive -Algorithm SHA256).Hash.ToLowerInvariant()} | ConvertTo-Json | Set-Content (Join-Path $EvidenceDirectory 'smoke-result.json')
} finally {
    $env:PATH = $previousPath; $env:QT_QPA_PLATFORM = $previousPlatform
    $env:QT_PLUGIN_PATH = $previousPlugin; $env:QT_QPA_PLATFORM_PLUGIN_PATH = $previousPlatformPlugin; $env:QTDIR = $previousQtdir
}
Write-Output 'Packaged Windows smoke test passed.'
