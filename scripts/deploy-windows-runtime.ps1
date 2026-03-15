param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,

    [Parameter(Mandatory = $true)]
    [string]$QmlDir,

    [string]$MsysPrefix = "C:\msys64\ucrt64"
)

$ErrorActionPreference = "Stop"

function Resolve-NormalizedPath {
    param([string]$PathValue)

    return [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $PathValue).Path)
}

function Get-ImportedDllNames {
    param(
        [string]$BinaryPath,
        [string]$ObjdumpPath
    )

    $output = & $ObjdumpPath -p $BinaryPath 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "objdump failed for '$BinaryPath'."
    }

    $imports = New-Object 'System.Collections.Generic.List[string]'
    foreach ($line in $output) {
        if ($line -match 'DLL Name:\s+(.+)$') {
            $imports.Add($Matches[1].Trim())
        }
    }

    return $imports
}

function Test-SystemDllName {
    param([string]$DllName)

    $name = $DllName.ToLowerInvariant()
    if ($name.StartsWith("api-ms-win-") -or $name.StartsWith("ext-ms-win-")) {
        return $true
    }

    $systemDlls = @(
        "advapi32.dll",
        "comdlg32.dll",
        "gdi32.dll",
        "kernel32.dll",
        "ole32.dll",
        "oleaut32.dll",
        "rpcrt4.dll",
        "shell32.dll",
        "shlwapi.dll",
        "user32.dll",
        "uxtheme.dll",
        "winmm.dll",
        "ws2_32.dll"
    )

    return $systemDlls -contains $name
}

function Copy-GStreamerPluginIfPresent {
    param(
        [string]$PluginName,
        [string]$SourceDir,
        [string]$DestinationDir
    )

    $sourcePath = Join-Path $SourceDir $PluginName
    if (Test-Path -LiteralPath $sourcePath) {
        Copy-Item -LiteralPath $sourcePath -Destination $DestinationDir -Force
    }
}

$exePath = Resolve-NormalizedPath -PathValue $ExePath
$qmlDir = Resolve-NormalizedPath -PathValue $QmlDir
$msysPrefix = Resolve-NormalizedPath -PathValue $MsysPrefix
$deployDir = Split-Path -Parent $exePath
$msysBinDir = Join-Path $msysPrefix "bin"
$qtShareDir = Join-Path $msysPrefix "share\qt6"
$qtPluginsSourceDir = Join-Path $qtShareDir "plugins"
$qtQmlSourceDir = Join-Path $qtShareDir "qml"
$gstPluginSourceDir = Join-Path $msysPrefix "lib\gstreamer-1.0"
$objdumpPath = Join-Path $msysBinDir "objdump.exe"

$env:PATH = "$msysBinDir;$env:PATH"

if (-not (Test-Path -LiteralPath $objdumpPath)) {
    throw "objdump was not found at '$objdumpPath'."
}

if (-not (Test-Path -LiteralPath $qtPluginsSourceDir)) {
    throw "Qt plugins directory was not found at '$qtPluginsSourceDir'."
}

$qtPluginsDeployDir = Join-Path $deployDir "plugins"
$qtQmlDeployDir = Join-Path $deployDir "qml"
$gstPluginDeployDir = Join-Path $deployDir "lib\gstreamer-1.0"
New-Item -ItemType Directory -Force -Path $qtPluginsDeployDir | Out-Null
New-Item -ItemType Directory -Force -Path $qtQmlDeployDir | Out-Null
New-Item -ItemType Directory -Force -Path $gstPluginDeployDir | Out-Null

Get-ChildItem -LiteralPath $qtPluginsSourceDir | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $qtPluginsDeployDir -Recurse -Force
}

if (Test-Path -LiteralPath $qtQmlSourceDir) {
    Get-ChildItem -LiteralPath $qtQmlSourceDir | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $qtQmlDeployDir -Recurse -Force
    }
}

if (Test-Path -LiteralPath $gstPluginSourceDir) {
    Get-ChildItem -LiteralPath $gstPluginDeployDir -Filter *.dll -File -ErrorAction SilentlyContinue |
        Remove-Item -Force

    # Deploy only the audio-focused plugin subset used by WaveFlux. Copying the
    # entire MSYS2 plugin tree pulls in optional plugins with missing extras and
    # slows startup due to plugin scanning.
    $gstPluginAllowList = @(
        "libgstapp.dll",
        "libgstapetag.dll",
        "libgstaiff.dll",
        "libgstasf.dll",
        "libgstaudioconvert.dll",
        "libgstaudiofx.dll",
        "libgstaudioparsers.dll",
        "libgstaudioresample.dll",
        "libgstautodetect.dll",
        "libgstcoreelements.dll",
        "libgstequalizer.dll",
        "libgstflac.dll",
        "libgstid3demux.dll",
        "libgstisomp4.dll",
        "libgstlibav.dll",
        "libgstmodplug.dll",
        "libgstmusepack.dll",
        "libgstmpg123.dll",
        "libgstogg.dll",
        "libgstopenmpt.dll",
        "libgstopus.dll",
        "libgstpbtypes.dll",
        "libgstplayback.dll",
        "libgstreplaygain.dll",
        "libgstspeex.dll",
        "libgstsoundtouch.dll",
        "libgstspectrum.dll",
        "libgsttypefindfunctions.dll",
        "libgstvorbis.dll",
        "libgstvolume.dll",
        "libgstwavpack.dll",
        "libgstwasapi.dll",
        "libgstdirectsound.dll",
        "libgstwavparse.dll"
    )

    foreach ($pluginName in $gstPluginAllowList) {
        Copy-GStreamerPluginIfPresent -PluginName $pluginName `
            -SourceDir $gstPluginSourceDir `
            -DestinationDir $gstPluginDeployDir
    }
}

$qtConfPath = Join-Path $deployDir "qt.conf"
@"
[Paths]
Prefix=.
Plugins=plugins
QmlImports=qml
Qml2Imports=qml
"@ | Set-Content -LiteralPath $qtConfPath -Encoding ASCII

$visited = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
$copiedDlls = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
$queue = New-Object 'System.Collections.Generic.Queue[string]'

$queue.Enqueue($exePath)
Get-ChildItem -LiteralPath $qtPluginsDeployDir -Filter *.dll -File -Recurse | ForEach-Object {
    $queue.Enqueue($_.FullName)
}
Get-ChildItem -LiteralPath $qtQmlDeployDir -Filter *.dll -File -Recurse | ForEach-Object {
    $queue.Enqueue($_.FullName)
}
Get-ChildItem -LiteralPath $gstPluginDeployDir -Filter *.dll -File | ForEach-Object {
    $queue.Enqueue($_.FullName)
}

while ($queue.Count -gt 0) {
    $currentBinary = $queue.Dequeue()
    if (-not $visited.Add($currentBinary)) {
        continue
    }

    foreach ($dllName in Get-ImportedDllNames -BinaryPath $currentBinary -ObjdumpPath $objdumpPath) {
        if (Test-SystemDllName -DllName $dllName) {
            continue
        }

        $deployedDllPath = Join-Path $deployDir $dllName
        if (Test-Path -LiteralPath $deployedDllPath) {
            if ($visited.Contains($deployedDllPath) -eq $false) {
                $queue.Enqueue($deployedDllPath)
            }
            continue
        }

        $sourceDllPath = Join-Path $msysBinDir $dllName
        if (-not (Test-Path -LiteralPath $sourceDllPath)) {
            continue
        }

        Copy-Item -LiteralPath $sourceDllPath -Destination $deployedDllPath -Force
        $copiedDlls.Add($dllName) | Out-Null
        $queue.Enqueue($deployedDllPath)
    }
}

Write-Host "Windows runtime deployment completed for '$exePath'."
Write-Host "Copied $($copiedDlls.Count) non-system DLLs to '$deployDir'."
