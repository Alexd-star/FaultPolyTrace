#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PackageDir,
    [switch]$SkipLaunch
)

$ErrorActionPreference = "Stop"
$package = (Resolve-Path -LiteralPath $PackageDir).Path
$rootExe = Join-Path $package "FaultPolyTrace.exe"
$runtime = Join-Path $package "pyalgo_faultpolytrace"
$launcher = Join-Path $runtime "faultpolytrace_launcher.exe"
$runner = Join-Path $runtime "faultpolytrace_geoeast_runner.exe"

$rootItems = @(Get-ChildItem -LiteralPath $package -Force)
if ($rootItems.Count -ne 2 -or !(Test-Path -LiteralPath $rootExe -PathType Leaf) -or !(Test-Path -LiteralPath $runtime -PathType Container)) {
    throw "FAIL：根目录不是严格的一个 EXE 加一个 pyalgo_faultpolytrace 子目录。"
}
foreach ($required in @($launcher, $runner, (Join-Path $runtime "qt.conf"), (Join-Path $runtime "platforms\qwindows.dll"))) {
    if (!(Test-Path -LiteralPath $required -PathType Leaf)) { throw "FAIL：缺少 $required" }
}
if (Test-Path -LiteralPath (Join-Path $runtime "faultpolytrace_launcher.ini")) {
    throw "FAIL：评测包混入了客户机配置 faultpolytrace_launcher.ini。"
}

$hashFile = Join-Path $runtime "SHA256SUMS.txt"
foreach ($line in Get-Content -LiteralPath $hashFile -Encoding UTF8) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    if ($line -notmatch '^([0-9A-Fa-f]{64}) [ *](.+)$') { throw "FAIL：哈希清单行无效：$line" }
    $expected = $Matches[1].ToUpperInvariant()
    $target = Join-Path $package ($Matches[2] -replace '/', '\')
    if (!(Test-Path -LiteralPath $target -PathType Leaf)) { throw "FAIL：哈希目标缺失：$target" }
    $actual = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    if ($actual -ne $expected) { throw "FAIL：哈希不一致：$target" }
}

if (!$SkipLaunch) {
    $before = @(Get-Process -Name "faultpolytrace_launcher" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id)
    if ($before.Count -gt 0) {
        throw "启动烟测前已有 faultpolytrace_launcher 进程，请先正常关闭现有界面后重试。"
    }
    $oldConfig = $env:FPT_CONFIG_ROOT
    $testConfig = Join-Path ([System.IO.Path]::GetTempPath()) ("FPT_eval_" + [guid]::NewGuid().ToString("N"))
    $bootstrapProcess = $null
    $newProcess = $null
    New-Item -ItemType Directory -Force -Path $testConfig | Out-Null
    try {
        $env:FPT_CONFIG_ROOT = $testConfig
        # 故意从与程序无关的临时目录启动，验证顶层 EXE 不依赖当前工作目录。
        $bootstrapProcess = Start-Process -FilePath $rootExe -WorkingDirectory ([System.IO.Path]::GetTempPath()) -PassThru
        $bootstrapProcess.WaitForExit(10000) | Out-Null
        if (!$bootstrapProcess.HasExited -or $bootstrapProcess.ExitCode -ne 0) {
            throw "FAIL：顶层启动器未正常退出。"
        }
        $deadline = (Get-Date).AddSeconds(15)
        do {
            Start-Sleep -Milliseconds 250
            $newProcess = Get-Process -Name "faultpolytrace_launcher" -ErrorAction SilentlyContinue | Select-Object -First 1
        } while (!$newProcess -and (Get-Date) -lt $deadline)
        if (!$newProcess) { throw "FAIL：顶层启动器未拉起内部 Qt Launcher。" }
        $actualPath = $null
        $pathDeadline = (Get-Date).AddSeconds(5)
        do {
            try {
                $newProcess.Refresh()
                $actualPath = $newProcess.Path
                if ([string]::IsNullOrWhiteSpace($actualPath)) {
                    $actualPath = $newProcess.MainModule.FileName
                }
            } catch {
                $actualPath = $null
            }
            if ([string]::IsNullOrWhiteSpace($actualPath)) {
                Start-Sleep -Milliseconds 100
            }
        } while ([string]::IsNullOrWhiteSpace($actualPath) -and (Get-Date) -lt $pathDeadline)
        if ([string]::IsNullOrWhiteSpace($actualPath)) {
            throw "FAIL：已拉起内部 Qt Launcher，但无法读取其可执行文件路径。"
        }
        if (![string]::Equals($actualPath, $launcher, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "FAIL：启动了错误位置的 Launcher：$actualPath"
        }
    } finally {
        if ($newProcess) {
            Stop-Process -Id $newProcess.Id -Force -ErrorAction SilentlyContinue
            $newProcess.WaitForExit(5000) | Out-Null
        }
        if ($bootstrapProcess -and !$bootstrapProcess.HasExited) {
            Stop-Process -Id $bootstrapProcess.Id -Force -ErrorAction SilentlyContinue
        }
        $env:FPT_CONFIG_ROOT = $oldConfig
        if (Test-Path -LiteralPath $testConfig) { Remove-Item -LiteralPath $testConfig -Recurse -Force }
    }
}

Write-Host "PASS：评测运行包结构、依赖、哈希和相对路径启动检查通过。"
