param(
    [string]$BuildDir = "E:\audioplayer\waveflux\build-win-runtime",
    [string]$Target = "waveflux",
    [string]$MsysPrefix = "C:\msys64\ucrt64",
    [switch]$RunTests,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Resolve-NormalizedPath {
    param([string]$PathValue)

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\$PathValue"))
}

$buildDir = Resolve-NormalizedPath -PathValue $BuildDir
$msysPrefix = Resolve-NormalizedPath -PathValue $MsysPrefix
$msysBinDir = Join-Path $msysPrefix "bin"
$cmakePath = Join-Path $msysBinDir "cmake.exe"
$ctestPath = Join-Path $msysBinDir "ctest.exe"

if (-not (Test-Path -LiteralPath $msysBinDir)) {
    throw "MSYS2 UCRT64 bin directory was not found at '$msysBinDir'."
}

if (-not (Test-Path -LiteralPath $cmakePath)) {
    throw "cmake.exe was not found at '$cmakePath'."
}

$env:PATH = "$msysBinDir;$env:PATH"

if (-not $SkipBuild) {
    & $cmakePath --build $buildDir --target $Target -j 8
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for target '$Target'."
    }
}

if ($RunTests) {
    if (-not (Test-Path -LiteralPath $ctestPath)) {
        throw "ctest.exe was not found at '$ctestPath'."
    }

    & $ctestPath --test-dir $buildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "Tests failed for build directory '$buildDir'."
    }
}
