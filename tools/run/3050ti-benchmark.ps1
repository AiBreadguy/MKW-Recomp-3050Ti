# What: Windows benchmark launcher for AiBreadguy/MKW-Recomp-3050Ti.
# Why: A/B runs on the same Windows laptop need identical mode flags and repeatable VRAM samples.
# Usage: .\3050ti-benchmark.ps1 -Exe "C:\mkw-recomp\mkw.exe" -Mode baseline
#        .\3050ti-benchmark.ps1 -Exe "C:\mkw-recomp\mkw.exe" -Mode drawcache
#        .\3050ti-benchmark.ps1 -Exe "C:\mkw-recomp\mkw.exe" -Mode vram
# Tested by code inspection; run on your own Windows laptop for actual measurements.
param(
    [ValidateSet("baseline", "drawcache", "vram", "both")]
    [string] $Mode = "baseline",
    [string] $Exe = ".\mkw.exe",
    [ValidateRange(1, 60)]
    [int] $IntervalSeconds = 2
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { throw "Executable not found: $Exe" }
$Exe = (Resolve-Path -LiteralPath $Exe).Path
$dir = Join-Path (Get-Location).Path ("mkw-3050ti-" + (Get-Date -Format "yyyyMMdd-HHmmss") + "-" + $Mode)
New-Item -Path $dir -ItemType Directory -Force | Out-Null
$keys = @("MKW_DRAW_CACHE", "MKW_TEX_POOL_BUDGETED", "MKW_VRAM_LOG")
$old = @{}
foreach ($key in $keys) {
    $old[$key] = [Environment]::GetEnvironmentVariable($key, "Process")
}
try {
    [Environment]::SetEnvironmentVariable("MKW_DRAW_CACHE", $null, "Process")
    [Environment]::SetEnvironmentVariable("MKW_TEX_POOL_BUDGETED", $null, "Process")
    [Environment]::SetEnvironmentVariable("MKW_VRAM_LOG", "300", "Process")
    if ($Mode -eq "drawcache" -or $Mode -eq "both") {
        [Environment]::SetEnvironmentVariable("MKW_DRAW_CACHE", "1", "Process")
    }
    if ($Mode -eq "vram" -or $Mode -eq "both") {
        [Environment]::SetEnvironmentVariable("MKW_TEX_POOL_BUDGETED", "1", "Process")
    }

    $smi = Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue
    $smiPath = if ($smiPath) { $smi.Source } else { $null }
    if (-not $smiPath) {
        $fallback = "C:\Program Files\NVIDIA Corporation\NVSMI\nvidia-smi.exe"
        if (Test-Path -LiteralPath $fallback) { $smiPath = $fallback }
    }
    $meta = @{
        mode = $Mode
        executable = $Exe
        started = (Get-Date).ToString("o")
        gpu_monitor = [bool]$smiPath
        note = "This records GPU memory and utilization, NOT true game FPS; use the in-game performance overlay and game log for FPS, latency and speed."
    }
    $meta | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dir "run.json") -Encoding UTF8
    Write-Host "Launching $Exe with mode $Mode"
    Write-Host "Use identical course, resolution, texture settings and power mode for all runs."
    Write-Host "This may increase VRAM/CPU overhead slightly; do not interpret it as a precise frame-time profiler."
    $p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path -Parent $Exe) -PassThru
    $csv = Join-Path $dir "gpu-samples.csv"
    $samples = @()
    while ($true) {
        $p.Refresh()
        if ($p.HasExited) { break }
        $gpuUsed = $null; $gpuTotal = $null; $gpuPct = $null
        if ($smi) {
            try {
                $out = & $smiPath "--query-gpu=memory.used,memory.total,utilization.gpu" "--format=csv,noheader,nounits" 2>$null
                if ($LASTEXITCODE -eq 0 -and $out) {
                    $v = ($out | Select-Object -First 1) -split ","
                    if ($v.Length -ge 3) {
                        $gpuUsed = $v[0].Trim(); $gpuTotal = $v[1].Trim(); $gpuPct = $v[2].Trim()
                    }
                }
            } catch { }
        }
        $workingSet = $null
        try { $workingSet = [math]::Round($p.WorkingSet64 / 1MB, 1) } catch { }
        $samples += [pscustomobject]@{
            timestamp = (Get-Date).ToString("o")
            gpu_used_mib = $gpuUsed
            gpu_total_mib = $gpuTotal
            gpu_utilization_pct = $gpuPct
            process_workingset_mib = $workingSet
        }
        Start-Sleep -Seconds $IntervalSeconds
    }
    if ($samples.Count -gt 0) { $samples | Export-Csv -NoTypeInformation -Path $csv }
    Write-Host "Benchmark samples saved to $dir"
    Write-Host "Also keep the game's latest log and screenshot of the full performance overlay."
} finally {
    foreach ($key in $keys) {
        [Environment]::SetEnvironmentVariable($key, $old[$key], "Process")
    }
}
