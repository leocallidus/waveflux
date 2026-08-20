param(
    [string]$BuildDir = "build-win-runtime",
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
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

$effectiveMsysPrefix = $MsysPrefix
if (-not (Test-Path -LiteralPath $effectiveMsysPrefix)) {
    $gccCmd = Get-Command gcc.exe -ErrorAction SilentlyContinue
    if ($gccCmd) {
        $effectiveMsysPrefix = Split-Path -Parent (Split-Path -Parent $gccCmd.Source)
    } elseif (Test-Path "C:\msys64\ucrt64") {
        $effectiveMsysPrefix = "C:\msys64\ucrt64"
    } elseif (Test-Path "C:\tools\msys64\ucrt64") {
        $effectiveMsysPrefix = "C:\tools\msys64\ucrt64"
    }
}

$msysPrefix = Resolve-NormalizedPath -PathValue $effectiveMsysPrefix
$msysBinDir = Join-Path $msysPrefix "bin"

$cmakePath = if (Test-Path -LiteralPath (Join-Path $msysBinDir "cmake.exe")) {
    Join-Path $msysBinDir "cmake.exe"
} elseif (Get-Command cmake.exe -ErrorAction SilentlyContinue) {
    (Get-Command cmake.exe).Source
} else {
    throw "cmake.exe was not found."
}

$ctestPath = if (Test-Path -LiteralPath (Join-Path $msysBinDir "ctest.exe")) {
    Join-Path $msysBinDir "ctest.exe"
} elseif (Get-Command ctest.exe -ErrorAction SilentlyContinue) {
    (Get-Command ctest.exe).Source
} else {
    "ctest.exe"
}

if (Test-Path -LiteralPath $msysBinDir) {
    $filtered = @($env:PATH -split ';' | Where-Object { $_ -and ($_ -notmatch 'mingw64') })
    $env:PATH = "$msysBinDir;" + ($filtered -join ';')
}

$cCompiler = Join-Path $msysBinDir "gcc.exe"
$cxxCompiler = Join-Path $msysBinDir "g++.exe"
$cmakePrefixPath = $msysPrefix.Replace("\", "/")

if (-not $SkipBuild) {
    $cmakeArgs = @(
        "-S", $repoRoot,
        "-B", $buildDir,
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_TESTING=ON",
        "-DCMAKE_PREFIX_PATH=$cmakePrefixPath",
        "-DCMAKE_FIND_ROOT_PATH=$cmakePrefixPath",
        "-DWAVEFLUX_MSYS2_UCRT64_ROOT=$cmakePrefixPath"
    )

    if (Test-Path -LiteralPath $cCompiler) {
        $cmakeArgs += "-DCMAKE_C_COMPILER=$($cCompiler.Replace('\', '/'))"
    }
    if (Test-Path -LiteralPath $cxxCompiler) {
        $cmakeArgs += "-DCMAKE_CXX_COMPILER=$($cxxCompiler.Replace('\', '/'))"
    }

    & $cmakePath @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Configure failed for build directory '$buildDir'."
    }

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
