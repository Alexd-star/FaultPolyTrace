[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string]$PackageDir,
    [string]$Version,
    [switch]$SkipGui,
    [switch]$NoScreenshots,
    [int]$GuiTimeoutSeconds = 45
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$releaseRoot = Join-Path $repoRoot "release"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$acceptanceRoot = Join-Path $repoRoot "examples\outputs\standalone_acceptance_$timestamp"
$figureRoot = Join-Path $releaseRoot "figure\standalone_acceptance_$timestamp"

function Resolve-PackageDir {
    if (![string]::IsNullOrWhiteSpace($PackageDir)) {
        return (Resolve-Path -LiteralPath $PackageDir).Path
    }
    if (![string]::IsNullOrWhiteSpace($Version)) {
        $candidate = Join-Path $releaseRoot ("FaultPolyTrace_" + $Version)
        if (!(Test-Path -LiteralPath $candidate)) {
            throw "指定版本发布包不存在：$candidate"
        }
        return (Resolve-Path -LiteralPath $candidate).Path
    }
    $latest = Get-ChildItem -LiteralPath $releaseRoot -Directory |
        Where-Object { $_.Name -like "FaultPolyTrace_*" } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($null -eq $latest) {
        throw "未找到发布包目录。请先运行 scripts\打包发布包.ps1。"
    }
    return $latest.FullName
}

function Assert-FileExists {
    param([string]$Path, [string]$Name)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "缺少$Name：$Path"
    }
    return (Get-Item -LiteralPath $Path)
}

function Assert-DirExists {
    param([string]$Path, [string]$Name)
    if (!(Test-Path -LiteralPath $Path -PathType Container)) {
        throw "缺少$Name：$Path"
    }
    return (Get-Item -LiteralPath $Path)
}

function Assert-OutputFile {
    param([string]$Path, [string]$Name)
    $item = Assert-FileExists -Path $Path -Name $Name
    if ($item.Length -le 0) {
        throw "$Name 为空：$Path"
    }
    return $item
}

function Invoke-CoreCli {
    param(
        [string]$Exe,
        [string]$InputPath,
        [string]$OutputBase
    )
    $csv = "$OutputBase.csv"
    $diagnostics = "$OutputBase.diagnostics.json"
    $branches = "$OutputBase.branches.csv"
    $topology = "$OutputBase.topology.csv"

    & $Exe `
        --input $InputPath `
        --input-format auto `
        --output $csv `
        --diagnostics $diagnostics `
        --branches-output $branches `
        --topology-output $topology `
        --trace-mode ridge-belt `
        --threshold-mode percentile `
        --percentile 0.98 `
        --min-area 120 `
        --close-radius 1 `
        --belt-radius 3 `
        --post-belt-close-radius 1 `
        --weak-percentile 0.965 `
        --weak-connect-radius 1 `
        --attribute-envelope-radius 1 `
        --min-branch-length 4 `
        --min-junction-branch-length 2 `
        --min-junction-branch-mean 0.94 `
        --max-gap-distance 12 `
        --max-gap-angle 55 `
        --min-gap-support-ratio 0.25 `
        --no-separate-branch-belts `
        --no-attribute-constrained-belt `
        --no-root-anti-merge `
        --no-local-anti-merge `
        --no-residual-trace `
        --simplify-tolerance 2 `
        --smooth-iterations 1

    if ($LASTEXITCODE -ne 0) {
        throw "核心 CLI 验收失败，退出码：$LASTEXITCODE"
    }

    Assert-OutputFile -Path $csv -Name "核心 CLI CSV" | Out-Null
    Assert-OutputFile -Path $diagnostics -Name "核心 CLI 诊断 JSON" | Out-Null
    Assert-OutputFile -Path $branches -Name "核心 CLI 枝干 CSV" | Out-Null
    Assert-OutputFile -Path $topology -Name "核心 CLI 拓扑 CSV" | Out-Null
    return @{
        csv = $csv
        diagnostics = $diagnostics
        branches = $branches
        topology = $topology
    }
}

function Invoke-RootForward {
    param(
        [string]$Exe,
        [string]$InputPath,
        [string]$OutputBase
    )
    $csv = "$OutputBase.csv"
    $diagnostics = "$OutputBase.diagnostics.json"
    $branches = "$OutputBase.branches.csv"
    $topology = "$OutputBase.topology.csv"
    $args = @(
        "--input", $InputPath,
        "--input-format", "auto",
        "--output", $csv,
        "--diagnostics", $diagnostics,
        "--branches-output", $branches,
        "--topology-output", $topology,
        "--trace-mode", "ridge-belt",
        "--threshold-mode", "percentile",
        "--percentile", "0.98",
        "--min-area", "120"
    )
    $process = Start-Process -FilePath $Exe -ArgumentList $args -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        throw "根目录 GUI 参数转发失败，退出码：$($process.ExitCode)"
    }

    Assert-OutputFile -Path $csv -Name "根目录转发 CSV" | Out-Null
    Assert-OutputFile -Path $diagnostics -Name "根目录转发诊断 JSON" | Out-Null
    Assert-OutputFile -Path $branches -Name "根目录转发枝干 CSV" | Out-Null
    Assert-OutputFile -Path $topology -Name "根目录转发拓扑 CSV" | Out-Null
    return @{
        csv = $csv
        diagnostics = $diagnostics
        branches = $branches
        topology = $topology
    }
}

function Add-UiAutomationTypes {
    Add-Type -AssemblyName UIAutomationClient
    Add-Type -AssemblyName UIAutomationTypes
}

function Add-ScreenshotTypes {
    Add-Type -AssemblyName System.Drawing
    if (-not ("FaultPolyTraceWin32" -as [type])) {
        Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class FaultPolyTraceWin32 {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
}
"@
    }
}

function Save-WindowScreenshot {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$Path
    )
    if ($NoScreenshots) {
        return
    }
    Add-ScreenshotTypes
    [FaultPolyTraceWin32]::SetForegroundWindow($Process.MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 250
    $rect = New-Object FaultPolyTraceWin32+RECT
    [FaultPolyTraceWin32]::GetWindowRect($Process.MainWindowHandle, [ref]$rect) | Out-Null
    $width = [Math]::Max(1, $rect.Right - $rect.Left)
    $height = [Math]::Max(1, $rect.Bottom - $rect.Top)
    $bitmap = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $hdc = $graphics.GetHdc()
        try {
            $printed = [FaultPolyTraceWin32]::PrintWindow($Process.MainWindowHandle, $hdc, 2)
        } finally {
            $graphics.ReleaseHdc($hdc)
        }
        if (!$printed) {
            [FaultPolyTraceWin32]::SetForegroundWindow($Process.MainWindowHandle) | Out-Null
            Start-Sleep -Milliseconds 250
            $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
        }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Find-ButtonByName {
    param(
        [IntPtr]$WindowHandle,
        [string]$Name,
        [int]$TimeoutSeconds
    )
    Add-UiAutomationTypes
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $root = [System.Windows.Automation.AutomationElement]::FromHandle($WindowHandle)
        $condition = New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
            [System.Windows.Automation.ControlType]::Button
        )
        $buttons = $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, $condition)
        for ($i = 0; $i -lt $buttons.Count; $i++) {
            if ($buttons.Item($i).Current.Name -eq $Name) {
                return $buttons.Item($i)
            }
        }
        Start-Sleep -Milliseconds 250
    }
    throw "未找到按钮：$Name"
}

function Invoke-GuiClickAcceptance {
    param(
        [string]$Exe,
        [string]$InputPath,
        [string]$OutputDir
    )
    $configDir = Join-Path $env:LOCALAPPDATA "BGPCup2026\FaultPolyTrace"
    $config = Join-Path $configDir "desktop.ini"
    $backup = Join-Path $configDir ("desktop.ini.acceptance_backup_" + $timestamp)
    New-Item -ItemType Directory -Force -Path $configDir | Out-Null
    if (Test-Path -LiteralPath $config) {
        Copy-Item -LiteralPath $config -Destination $backup -Force
    }

    $outputName = "GuiClick_Result"
    $csv = Join-Path $OutputDir "$outputName.csv"
    $inputIni = $InputPath -replace "\\", "/"
    $outIni = $OutputDir -replace "\\", "/"

    Set-Content -LiteralPath $config -Encoding UTF8 -Value @(
        "[General]",
        "input=$inputIni",
        "outputDir=$outIni",
        "outputName=$outputName",
        "inputFormat=auto",
        "viewMode=\x53e0\x52a0\x7ed3\x679c",
        "thresholdMode=percentile",
        "percentile=0.98",
        "threshold=0.95",
        "minArea=120",
        "closeRadius=1",
        "beltRadius=3",
        "postBeltCloseRadius=1",
        "weakPercentile=0.965",
        "weakConnectRadius=1",
        "attributeEnvelopeRadius=1",
        "minBranchLength=4",
        "minJunctionBranchLength=2",
        "minJunctionBranchMean=0.94",
        "maxGapDistance=12",
        "maxGapAngle=55",
        "rootAntiMerge=false",
        "localAntiMerge=false",
        "residualTrace=false",
        "smoothIterations=1",
        "simplifyTolerance=2",
        "color=#00FFFF",
        "lineWidth=0.03",
        "alpha=1"
    )

    $process = Start-Process -FilePath $Exe -PassThru
    try {
        for ($i = 0; $i -lt ($GuiTimeoutSeconds * 10) -and $process.MainWindowHandle -eq 0; $i++) {
            Start-Sleep -Milliseconds 100
            $process.Refresh()
        }
        if ($process.MainWindowHandle -eq 0) {
            throw "独立软件窗口未打开。"
        }

        Save-WindowScreenshot -Process $process -Path (Join-Path $figureRoot "01_GUI启动.png")
        $button = Find-ButtonByName -WindowHandle $process.MainWindowHandle -Name "开始追踪" -TimeoutSeconds $GuiTimeoutSeconds
        $button.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()

        $deadline = (Get-Date).AddSeconds($GuiTimeoutSeconds)
        while ((Get-Date) -lt $deadline) {
            if (Test-Path -LiteralPath $csv) {
                $item = Get-Item -LiteralPath $csv
                if ($item.Length -gt 0) {
                    break
                }
            }
            Start-Sleep -Milliseconds 250
        }
        Assert-OutputFile -Path $csv -Name "GUI 点击 CSV" | Out-Null
        Assert-OutputFile -Path (Join-Path $OutputDir "$outputName.geojson") -Name "GUI 点击 GeoJSON" | Out-Null
        Assert-OutputFile -Path (Join-Path $OutputDir "$outputName.diagnostics.json") -Name "GUI 点击诊断 JSON" | Out-Null
        Assert-OutputFile -Path (Join-Path $OutputDir "$outputName.branches.csv") -Name "GUI 点击枝干 CSV" | Out-Null
        Assert-OutputFile -Path (Join-Path $OutputDir "$outputName.topology.csv") -Name "GUI 点击拓扑 CSV" | Out-Null
        Save-WindowScreenshot -Process $process -Path (Join-Path $figureRoot "02_GUI运行后.png")
    } finally {
        if (!$process.HasExited) {
            $process.CloseMainWindow() | Out-Null
            Start-Sleep -Milliseconds 500
            if (!$process.HasExited) {
                $process.Kill()
            }
        }
        if (Test-Path -LiteralPath $backup) {
            Copy-Item -LiteralPath $backup -Destination $config -Force
        }
    }

    return @{
        csv = $csv
        geojson = Join-Path $OutputDir "$outputName.geojson"
        diagnostics = Join-Path $OutputDir "$outputName.diagnostics.json"
        branches = Join-Path $OutputDir "$outputName.branches.csv"
        topology = Join-Path $OutputDir "$outputName.topology.csv"
    }
}

function Test-PersonalPathLeak {
    param([string]$Root)
    $patterns = @(
        'C:\\Users\\[^\\]+\\Documents\\BGPCup\\FaultPolyTrace',
        'C:/Users/[^/]+/Documents/BGPCup/FaultPolyTrace',
        'C:\\Users\\[^\\]+\\GeoEast',
        'C:/Users/[^/]+/GeoEast',
        '[A-Za-z]:[\\/]+BaiduNetdiskDownload'
    )
    $matches = @()
    Get-ChildItem -LiteralPath $Root -Recurse -File -Include *.md,*.txt,*.ini,*.ps1,*.cmd,*.py |
        ForEach-Object {
            $text = Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8
            foreach ($pattern in $patterns) {
                if ([regex]::IsMatch($text, $pattern)) {
                    $matches += $_.FullName
                    break
                }
            }
        }
    if ($matches.Count -gt 0) {
        throw "发布包文本仍包含个人路径：`n$($matches -join "`n")"
    }
}

$resolvedPackage = Resolve-PackageDir
if ($WhatIfPreference) {
    Write-Host "WhatIf: 将验收独立软件发布包：$resolvedPackage"
    return
}

New-Item -ItemType Directory -Force -Path $acceptanceRoot | Out-Null
New-Item -ItemType Directory -Force -Path $figureRoot | Out-Null

$rootExe = (Assert-FileExists -Path (Join-Path $resolvedPackage "FaultPolyTrace.exe") -Name "独立桌面软件").FullName
$coreExe = (Assert-FileExists -Path (Join-Path $resolvedPackage "bin\faultpolytrace.exe") -Name "核心算法程序").FullName
$launcherExe = (Assert-FileExists -Path (Join-Path $resolvedPackage "qt_launcher\faultpolytrace_launcher.exe") -Name "GeoEast 集成控件").FullName
Assert-DirExists -Path (Join-Path $resolvedPackage "docs") -Name "中文文档目录" | Out-Null
Assert-DirExists -Path (Join-Path $resolvedPackage "examples\synthetic") -Name "示例数据目录" | Out-Null
Assert-DirExists -Path (Join-Path $resolvedPackage "platforms") -Name "Qt platforms 插件目录" | Out-Null
Assert-FileExists -Path (Join-Path $resolvedPackage "README_先打开我.md") -Name "优先阅读 README" | Out-Null
Assert-FileExists -Path (Join-Path $resolvedPackage "发布包清单.md") -Name "发布包清单" | Out-Null

$input = (Assert-FileExists -Path (Join-Path $resolvedPackage "examples\synthetic\broken_ridge_fault_grid.txt") -Name "默认示例数据").FullName
Test-PersonalPathLeak -Root $resolvedPackage

$cliDir = Join-Path $acceptanceRoot "01_core_cli"
$forwardDir = Join-Path $acceptanceRoot "02_root_forward"
$guiDir = Join-Path $acceptanceRoot "03_gui_click"
New-Item -ItemType Directory -Force -Path $cliDir, $forwardDir, $guiDir | Out-Null

$cliOutputs = Invoke-CoreCli -Exe $coreExe -InputPath $input -OutputBase (Join-Path $cliDir "CoreCli_Result")
$forwardOutputs = Invoke-RootForward -Exe $rootExe -InputPath $input -OutputBase (Join-Path $forwardDir "RootForward_Result")
$guiOutputs = $null
if (!$SkipGui) {
    $guiOutputs = Invoke-GuiClickAcceptance -Exe $rootExe -InputPath $input -OutputDir $guiDir
}

$reportPath = Join-Path $acceptanceRoot "独立软件发布包验收报告.md"
$report = @()
$report += "# 独立软件发布包验收报告"
$report += ""
$report += "验收时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$report += "发布包：$resolvedPackage"
$report += "图件目录：$figureRoot"
$report += ""
$report += "## 验收结果"
$report += ""
$report += "| 项目 | 结果 |"
$report += "| --- | --- |"
$report += "| 发布包结构 | 通过 |"
$report += "| 个人路径扫描 | 通过 |"
$report += "| 核心 CLI 运行 | 通过 |"
$report += "| 根目录 GUI 参数转发 | 通过 |"
$report += "| GUI 点击运行 | $(if ($SkipGui) { '已跳过' } else { '通过' }) |"
$report += ""
$report += "## 输出文件"
$report += ""
$report += "- 核心 CLI CSV：$($cliOutputs.csv)"
$report += "- 根目录转发 CSV：$($forwardOutputs.csv)"
if ($guiOutputs) {
    $report += "- GUI 点击 CSV：$($guiOutputs.csv)"
    $report += "- GUI 点击 GeoJSON：$($guiOutputs.geojson)"
}
$report += ""
$report += "## 说明"
$report += ""
$report += "本验收覆盖独立软件发布包的基本结构、核心算法可独立运行、根目录参数转发、GUI 点击运行和输出文件完整性。GeoEast 真实数据库读写需在有效 GeoEast license 与工区环境中另行验收。"

Set-Content -LiteralPath $reportPath -Value ($report -join "`n") -Encoding UTF8

Write-Host "独立软件发布包验收通过。"
Write-Host "验收目录:" $acceptanceRoot
Write-Host "图件目录:" $figureRoot
Write-Host "报告:" $reportPath
