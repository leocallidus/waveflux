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
if (-not (Test-Path -LiteralPath (Join-Path $effectiveMsysPrefix "bin\gcc.exe"))) {
    $candidates = @()
    if ($env:RUNNER_TEMP) {
        $candidates += (Join-Path $env:RUNNER_TEMP "setup-msys2\msys64\ucrt64")
    }
    $candidates += @(
        "C:\msys64\ucrt64",
        "C:\tools\msys64\ucrt64"
    )
    foreach ($cand in $candidates) {
        if ($cand -and (Test-Path -LiteralPath (Join-Path $cand "bin\gcc.exe"))) {
            $effectiveMsysPrefix = $cand
            break
        }
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
    $filtered = @($env:PATH -split ';' | Where-Object { $_ -and ($_ -notmatch '(?i)(mingw|strawberry|perl|llvm)') })
    $env:PATH = "$msysBinDir;$msysPrefix\..\usr\bin;" + ($filtered -join ';')
}

$cCandidateNames = @("gcc.exe", "x86_64-w64-mingw32-gcc.exe", "clang.exe")
$cxxCandidateNames = @("g++.exe", "c++.exe", "x86_64-w64-mingw32-g++.exe", "x86_64-w64-mingw32-c++.exe", "clang++.exe")

$cCompiler = ""
foreach ($name in $cCandidateNames) {
    $p = Join-Path $msysBinDir $name
    if (Test-Path -LiteralPath $p) {
        $cCompiler = $p.Replace("\", "/")
        break
    }
}

$cxxCompiler = ""
foreach ($name in $cxxCandidateNames) {
    $p = Join-Path $msysBinDir $name
    if (Test-Path -LiteralPath $p) {
        $cxxCompiler = $p.Replace("\", "/")
        break
    }
}
$cmakePrefixPath = $msysPrefix.Replace("\", "/")

Write-Host "MSYS2 prefix: $msysPrefix"
Write-Host "C compiler: $cCompiler"
Write-Host "CXX compiler: $cxxCompiler"
Write-Host "CMake path: $cmakePath"

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

    if (Test-Path -LiteralPath (Join-Path $msysPrefix "lib\cmake\Qt6")) {
        $cmakeArgs += "-DQt6_DIR=$cmakePrefixPath/lib/cmake/Qt6"
    }

    if ($cCompiler) {
        $cmakeArgs += "-DCMAKE_C_COMPILER=$cCompiler"
    }
    if ($cxxCompiler) {
        $cmakeArgs += "-DCMAKE_CXX_COMPILER=$cxxCompiler"
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
