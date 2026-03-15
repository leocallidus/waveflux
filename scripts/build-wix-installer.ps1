param(
    [string]$BuildDir = "E:\audioplayer\waveflux\build-win-runtime",
    [string]$Target = "waveflux",
    [string]$MsysPrefix = "C:\msys64\ucrt64",
    [string]$DistDir = "E:\audioplayer\waveflux\dist\windows",
    [string]$WixExe = "C:\Program Files\WiX Toolset v6.0\bin\wix.exe",
    [string]$Version,
    [switch]$RunTests,
    [switch]$SkipBuild
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

function Get-SafeId {
    param(
        [string]$Prefix,
        [string]$Text
    )

    $normalized = $Text -replace '[^A-Za-z0-9_]', '_'
    $normalized = $normalized -replace '_+', '_'
    $normalized = $normalized.Trim('_')
    if ([string]::IsNullOrWhiteSpace($normalized)) {
        $normalized = "Root"
    }

    $sha1 = [System.Security.Cryptography.SHA1]::Create()
    try {
        $hashBytes = $sha1.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($Text))
    } finally {
        $sha1.Dispose()
    }

    $hash = [System.BitConverter]::ToString($hashBytes).Replace("-", "").Substring(0, 10)

    $body = "$Prefix$normalized"
    if ($body.Length -gt 54) {
        $body = $body.Substring(0, 54)
    }

    return "$body`_$hash"
}

function Get-StableGuid {
    param([string]$Seed)

    $md5 = [System.Security.Cryptography.MD5]::Create()
    try {
        $bytes = $md5.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($Seed))
    } finally {
        $md5.Dispose()
    }

    return [guid]::new($bytes).ToString().ToUpperInvariant()
}

function Escape-Xml {
    param([string]$Value)

    return [System.Security.SecurityElement]::Escape($Value)
}

function New-DirectoryNode {
    param(
        [string]$Name,
        [string]$RelativePath
    )

    return [pscustomobject]@{
        Name = $Name
        RelativePath = $RelativePath
        DirectoryId = $(if ([string]::IsNullOrEmpty($RelativePath)) { "INSTALLFOLDER" } else { Get-SafeId -Prefix "Dir_" -Text $RelativePath })
        Files = New-Object System.Collections.Generic.List[object]
        Children = New-Object System.Collections.Generic.List[object]
    }
}

function Add-DirectoryTree {
    param(
        [string]$SourceRoot,
        [string]$RelativePath,
        [object]$Node
    )

    $currentPath = if ([string]::IsNullOrEmpty($RelativePath)) { $SourceRoot } else { Join-Path $SourceRoot $RelativePath }
    $entries = Get-ChildItem -LiteralPath $currentPath | Sort-Object @{ Expression = { -not $_.PSIsContainer } }, Name

    foreach ($entry in $entries) {
        if ($entry.PSIsContainer) {
            $childRelativePath = if ([string]::IsNullOrEmpty($RelativePath)) { $entry.Name } else { Join-Path $RelativePath $entry.Name }
            $childNode = New-DirectoryNode -Name $entry.Name -RelativePath $childRelativePath
            Add-DirectoryTree -SourceRoot $SourceRoot -RelativePath $childRelativePath -Node $childNode
            $Node.Children.Add($childNode)
            continue
        }

        $fileRelativePath = if ([string]::IsNullOrEmpty($RelativePath)) { $entry.Name } else { Join-Path $RelativePath $entry.Name }
        $componentId = Get-SafeId -Prefix "Cmp_" -Text $fileRelativePath
        $fileId = Get-SafeId -Prefix "Fil_" -Text $fileRelativePath
        $Node.Files.Add([pscustomobject]@{
            RelativePath = $fileRelativePath
            Name = $entry.Name
            FullPath = $entry.FullName
            ComponentId = $componentId
            FileId = $fileId
            ComponentGuid = Get-StableGuid -Seed "waveflux-installer::$fileRelativePath"
        })
    }
}

function Write-DirectoryXml {
    param(
        [System.Text.StringBuilder]$Builder,
        [object]$Node,
        [int]$IndentLevel,
        [System.Collections.Generic.List[string]]$ComponentIds
    )

    $indent = ("  " * $IndentLevel)

    foreach ($file in $Node.Files) {
        [void]$Builder.AppendLine("$indent<Component Id=""$($file.ComponentId)"" Guid=""$($file.ComponentGuid)"">")
        [void]$Builder.AppendLine("$indent  <File Id=""$($file.FileId)"" Source=""$(Escape-Xml $file.FullPath)"" KeyPath=""yes"" />")
        [void]$Builder.AppendLine("$indent</Component>")
        $ComponentIds.Add($file.ComponentId)
    }

    foreach ($child in $Node.Children) {
        [void]$Builder.AppendLine("$indent<Directory Id=""$($child.DirectoryId)"" Name=""$(Escape-Xml $child.Name)"">")
        Write-DirectoryXml -Builder $Builder -Node $child -IndentLevel ($IndentLevel + 1) -ComponentIds $ComponentIds
        [void]$Builder.AppendLine("$indent</Directory>")
    }
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildDir = Resolve-NormalizedPath -PathValue $BuildDir
$distDir = Resolve-NormalizedPath -PathValue $DistDir
$wixExePath = Resolve-NormalizedPath -PathValue $WixExe
$effectiveVersion = if ([string]::IsNullOrWhiteSpace($Version)) {
    Get-ProjectVersion -RepoRoot $repoRoot
} else {
    $Version.Trim()
}

if (-not (Test-Path -LiteralPath $wixExePath)) {
    throw "wix.exe was not found at '$wixExePath'."
}

$portableScriptPath = Join-Path $PSScriptRoot "build-portable-zip.ps1"
if (-not (Test-Path -LiteralPath $portableScriptPath)) {
    throw "Portable packaging script was not found at '$portableScriptPath'."
}

$templatePath = Join-Path $repoRoot "packaging\wix\Product.wxs"
if (-not (Test-Path -LiteralPath $templatePath)) {
    throw "WiX product template was not found at '$templatePath'."
}

$iconPath = Join-Path $repoRoot "resources\icons\waveflux.ico"
if (-not (Test-Path -LiteralPath $iconPath)) {
    throw "Installer icon was not found at '$iconPath'."
}

$powershellExe = Join-Path $env:SystemRoot "System32\WindowsPowerShell\v1.0\powershell.exe"
$portableScriptArgs = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-File", $portableScriptPath,
    "-BuildDir", $buildDir,
    "-Target", $Target,
    "-MsysPrefix", $MsysPrefix,
    "-DistDir", $distDir,
    "-Version", $effectiveVersion,
    "-SkipZip"
)
if ($RunTests) {
    $portableScriptArgs += "-RunTests"
}
if ($SkipBuild) {
    $portableScriptArgs += "-SkipBuild"
}

& $powershellExe @portableScriptArgs
if ($LASTEXITCODE -ne 0) {
    throw "Portable staging step failed."
}

$packageBaseName = "WaveFlux-$effectiveVersion-windows-portable"
$stageRoot = Join-Path $distDir $packageBaseName
if (-not (Test-Path -LiteralPath $stageRoot)) {
    throw "Portable staging directory was not found at '$stageRoot'."
}

$wixWorkRoot = Join-Path $distDir "wix"
$wixWorkDir = Join-Path $wixWorkRoot "WaveFlux-$effectiveVersion"
Ensure-CleanDirectory -Path $wixWorkDir

$filesWxsPath = Join-Path $wixWorkDir "WaveFlux.Files.wxs"
$msiPath = Join-Path $distDir "WaveFlux-$effectiveVersion-windows-x64.msi"
if (Test-Path -LiteralPath $msiPath) {
    Remove-Item -LiteralPath $msiPath -Force
}

$rootNode = New-DirectoryNode -Name "WaveFlux" -RelativePath ""
Add-DirectoryTree -SourceRoot $stageRoot -RelativePath "" -Node $rootNode

$componentIds = New-Object System.Collections.Generic.List[string]
$builder = [System.Text.StringBuilder]::new()
[void]$builder.AppendLine('<?xml version="1.0" encoding="UTF-8"?>')
[void]$builder.AppendLine('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">')
[void]$builder.AppendLine('  <Fragment>')
[void]$builder.AppendLine('    <DirectoryRef Id="INSTALLFOLDER">')
Write-DirectoryXml -Builder $builder -Node $rootNode -IndentLevel 3 -ComponentIds $componentIds
[void]$builder.AppendLine('    </DirectoryRef>')
[void]$builder.AppendLine('  </Fragment>')
[void]$builder.AppendLine('  <Fragment>')
[void]$builder.AppendLine('    <ComponentGroup Id="WaveFluxFiles">')
foreach ($componentId in $componentIds) {
    [void]$builder.AppendLine("      <ComponentRef Id=""$componentId"" />")
}
[void]$builder.AppendLine('    </ComponentGroup>')
[void]$builder.AppendLine('  </Fragment>')
[void]$builder.AppendLine('</Wix>')
$builder.ToString() | Set-Content -LiteralPath $filesWxsPath -Encoding UTF8

$upgradeCode = "8F8B7194-78CB-4E14-96F7-9D0B9A6C7F77"
$startMenuShortcutGuid = "0F6A3B3A-3D29-4CE1-A605-1B4D4C3E2A11"

& $wixExePath build `
    $templatePath `
    $filesWxsPath `
    -arch x64 `
    -d ProductName=WaveFlux `
    -d Manufacturer=WaveFlux `
    -d ProductVersion=$effectiveVersion `
    -d UpgradeCode=$upgradeCode `
    -d InstallDirName=WaveFlux `
    -d IconPath=$iconPath `
    -d StartMenuShortcutGuid=$startMenuShortcutGuid `
    -o $msiPath

if ($LASTEXITCODE -ne 0) {
    throw "WiX build failed."
}

Write-Host "WiX sources generated at '$wixWorkDir'."
Write-Host "MSI installer created at '$msiPath'."
