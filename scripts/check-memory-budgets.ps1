param(
    [string]$InputPath = "",
    [double]$MaxSteadyWorkingSetMiB = 120,
    [double]$MaxSteadyPrivateMiB = 120,
    [double]$MaxPeakWorkingSetMiB = 180,
    [double]$MaxCommitMiB = 220
)

$ErrorActionPreference = "Stop"

function Resolve-SnapshotPath {
    param([string]$PathValue)

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        $profilingDir = Join-Path $env:LOCALAPPDATA "WaveFlux\\WaveFlux\\profiling"
        if (-not (Test-Path -LiteralPath $profilingDir)) {
            throw "Profiling directory was not found at '$profilingDir'."
        }
        $latest = Get-ChildItem -LiteralPath $profilingDir -Filter "snapshot_*.json" -File |
            Sort-Object LastWriteTimeUtc -Descending |
            Select-Object -First 1
        if (-not $latest) {
            throw "No profiler snapshot_*.json files were found in '$profilingDir'."
        }
        return $latest.FullName
    }

    $resolved = (Resolve-Path -LiteralPath $PathValue).Path
    if ((Get-Item -LiteralPath $resolved) -is [System.IO.DirectoryInfo]) {
        $latest = Get-ChildItem -LiteralPath $resolved -Filter "snapshot_*.json" -File |
            Sort-Object LastWriteTimeUtc -Descending |
            Select-Object -First 1
        if (-not $latest) {
            throw "No profiler snapshot_*.json files were found in '$resolved'."
        }
        return $latest.FullName
    }

    return $resolved
}

function Convert-ToMiB {
    param([double]$Bytes)
    return [math]::Round(($Bytes / 1MB), 2)
}

function Test-SteadyCheckpoint {
    param([string]$Label)

    if ([string]::IsNullOrWhiteSpace($Label)) {
        return $false
    }

    $steadyLabels = @(
        "app.startup_ready",
        "session.restore_complete",
        "playlist.count_stable",
        "waveform.peaks_ready",
        "audio.current_file_changed"
    )

    if ($steadyLabels -contains $Label) {
        return $true
    }

    return $Label -like "dialog.*.closed"
}

$snapshotPath = Resolve-SnapshotPath -PathValue $InputPath
$snapshot = Get-Content -LiteralPath $snapshotPath -Raw | ConvertFrom-Json

if (-not $snapshot.memory) {
    throw "Snapshot '$snapshotPath' does not contain a 'memory' object."
}

$failures = New-Object 'System.Collections.Generic.List[string]'

$peakWorkingSetMiB = Convert-ToMiB $snapshot.memory.peak_working_set_bytes
$commitMiB = Convert-ToMiB $snapshot.memory.commit_bytes

if ($peakWorkingSetMiB -gt $MaxPeakWorkingSetMiB) {
    $failures.Add("Peak working set $peakWorkingSetMiB MiB exceeded budget $MaxPeakWorkingSetMiB MiB.")
}

if ($commitMiB -gt $MaxCommitMiB) {
    $failures.Add("Commit $commitMiB MiB exceeded budget $MaxCommitMiB MiB.")
}

$steadyRows = @()
foreach ($checkpoint in @($snapshot.memory_checkpoints)) {
    $workingSetMiB = Convert-ToMiB $checkpoint.working_set_bytes
    $privateMiB = Convert-ToMiB $checkpoint.private_bytes
    $steady = Test-SteadyCheckpoint -Label $checkpoint.label

    $steadyRows += [pscustomobject]@{
        Label = [string]$checkpoint.label
        WorkingSetMiB = $workingSetMiB
        PrivateMiB = $privateMiB
        SteadyBudget = $steady
    }

    if (-not $steady) {
        continue
    }

    if ($workingSetMiB -gt $MaxSteadyWorkingSetMiB) {
        $failures.Add("Checkpoint '$($checkpoint.label)' working set $workingSetMiB MiB exceeded steady budget $MaxSteadyWorkingSetMiB MiB.")
    }
    if ($privateMiB -gt $MaxSteadyPrivateMiB) {
        $failures.Add("Checkpoint '$($checkpoint.label)' private bytes $privateMiB MiB exceeded steady budget $MaxSteadyPrivateMiB MiB.")
    }
}

Write-Host "Snapshot:" $snapshotPath
Write-Host "Peak Working Set:" $peakWorkingSetMiB "MiB"
Write-Host "Commit:" $commitMiB "MiB"
Write-Host ""
Write-Host "Checkpoint summary:"
$steadyRows | Format-Table -AutoSize

if ($failures.Count -gt 0) {
    Write-Host ""
    Write-Host "Budget violations:" -ForegroundColor Red
    foreach ($failure in $failures) {
        Write-Host "- $failure" -ForegroundColor Red
    }
    exit 1
}

Write-Host ""
Write-Host "All configured memory budgets passed." -ForegroundColor Green
