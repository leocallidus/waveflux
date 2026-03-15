param(
    [string]$BuildDir = "E:\audioplayer\waveflux\build-win-runtime",
    [string]$Target = "waveflux",
    [string]$MsysPrefix = "C:\msys64\ucrt64",
    [string]$DistDir = "E:\audioplayer\waveflux\dist\windows",
    [string]$Version,
    [switch]$RunTests,
    [switch]$SkipBuild,
    [switch]$SkipZip
)

$ErrorActionPreference = "Stop"

function Resolve-NormalizedPath {
    param([string]$PathValue)

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        throw "Path value must not be empty."
    }

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\$PathValue"))
}

function Get-ProjectVersion {
    param([string]$RepoRoot)

    $cmakeListsPath = Join-Path $RepoRoot "CMakeLists.txt"
    if (-not (Test-Path -LiteralPath $cmakeListsPath)) {
        throw "CMakeLists.txt was not found at '$cmakeListsPath'."
    }

    $match = Select-String -Path $cmakeListsPath -Pattern 'project\s*\(\s*waveflux\s+VERSION\s+([0-9]+(?:\.[0-9]+){1,3})' |
        Select-Object -First 1
    if ($null -eq $match) {
        throw "Could not determine project version from '$cmakeListsPath'."
    }

    return $match.Matches[0].Groups[1].Value
}

function Ensure-CleanDirectory {
    param([string]$Path)

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }

    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function Copy-DirectoryIfPresent {
    param(
        [string]$SourcePath,
        [string]$DestinationPath
    )

    if (-not (Test-Path -LiteralPath $SourcePath)) {
        return
    }

    Copy-Item -LiteralPath $SourcePath -Destination $DestinationPath -Recurse -Force
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildDir = Resolve-NormalizedPath -PathValue $BuildDir
$distDir = Resolve-NormalizedPath -PathValue $DistDir
$effectiveVersion = if ([string]::IsNullOrWhiteSpace($Version)) {
    Get-ProjectVersion -RepoRoot $repoRoot
} else {
    $Version.Trim()
}

$buildScriptPath = Join-Path $PSScriptRoot "build-win-runtime.ps1"
if (-not (Test-Path -LiteralPath $buildScriptPath)) {
    throw "Windows runtime build script was not found at '$buildScriptPath'."
}

$powershellExe = Join-Path $env:SystemRoot "System32\WindowsPowerShell\v1.0\powershell.exe"
if (-not (Test-Path -LiteralPath $powershellExe)) {
    throw "powershell.exe was not found at '$powershellExe'."
}

$buildScriptArgs = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $buildScriptPath,
    "-BuildDir", $buildDir,
    "-Target", $Target,
    "-MsysPrefix", $MsysPrefix
)
if ($RunTests) {
    $buildScriptArgs += "-RunTests"
}
if ($SkipBuild) {
    $buildScriptArgs += "-SkipBuild"
}

& $powershellExe @buildScriptArgs
if ($LASTEXITCODE -ne 0) {
    throw "Portable build preparation failed."
}

$exePath = Join-Path $buildDir "$Target.exe"
if (-not (Test-Path -LiteralPath $exePath)) {
    throw "Built executable was not found at '$exePath'."
}

$packageBaseName = "WaveFlux-$effectiveVersion-windows-portable"
$stageRoot = Join-Path $distDir $packageBaseName
$zipPath = Join-Path $distDir "$packageBaseName.zip"

New-Item -ItemType Directory -Path $distDir -Force | Out-Null
Ensure-CleanDirectory -Path $stageRoot
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

$rootFiles = Get-ChildItem -LiteralPath $buildDir -File | Where-Object {
    $_.Name -eq "$Target.exe" -or
    $_.Name -eq "qt.conf" -or
    $_.Extension -eq ".dll"
}

foreach ($file in $rootFiles) {
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $stageRoot $file.Name) -Force
}

$runtimeDirectories = @(
    "plugins",
    "qml",
    "lib",
    "WaveFlux"
)

foreach ($directoryName in $runtimeDirectories) {
    $sourcePath = Join-Path $buildDir $directoryName
    $destinationPath = Join-Path $stageRoot $directoryName
    Copy-DirectoryIfPresent -SourcePath $sourcePath -DestinationPath $destinationPath
}

$debugPluginDirectories = @(
    "plugins\qmllint",
    "plugins\qmlls",
    "plugins\qmltooling"
)

foreach ($relativePath in $debugPluginDirectories) {
    $targetPath = Join-Path $stageRoot $relativePath
    if (Test-Path -LiteralPath $targetPath) {
        Remove-Item -LiteralPath $targetPath -Recurse -Force
    }
}

$optionalRootExcludes = @(
    "Qt6Test.dll",
    "Qt6QuickTest.dll"
)

foreach ($fileName in $optionalRootExcludes) {
    $targetPath = Join-Path $stageRoot $fileName
    if (Test-Path -LiteralPath $targetPath) {
        Remove-Item -LiteralPath $targetPath -Force
    }
}

Copy-Item -LiteralPath (Join-Path $repoRoot "LICENSE") -Destination (Join-Path $stageRoot "LICENSE.txt") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "README.md") -Destination (Join-Path $stageRoot "README.md") -Force

$portableNoticePath = Join-Path $stageRoot "PORTABLE.txt"
@"
WaveFlux Portable
=================

Start:
  waveflux.exe

This ZIP package is a no-install Windows bundle built from the runtime deployment.

Notes:
  - No installer is required.
  - Settings, session data, and caches are still stored in the standard Qt/Windows
    app-data locations for the current build.
  - To rebuild this package from source, run:
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build-portable-zip.ps1
"@ | Set-Content -LiteralPath $portableNoticePath -Encoding ASCII

if (-not $SkipZip) {
    Compress-Archive -Path (Join-Path $stageRoot "*") -DestinationPath $zipPath -CompressionLevel Optimal
}

Write-Host "Portable staging directory prepared at '$stageRoot'."
if (-not $SkipZip) {
    Write-Host "Portable ZIP package created at '$zipPath'."
}
