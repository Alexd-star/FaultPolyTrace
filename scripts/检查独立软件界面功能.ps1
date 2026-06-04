[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$reportDir = Join-Path $repoRoot "examples\outputs\desktop_ui_function_$timestamp"
$reportPath = Join-Path $reportDir "独立软件界面功能检查报告.md"

function Read-TextFile {
    param([string]$RelativePath)
    $path = Join-Path $repoRoot $RelativePath
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "缺少待检查文件：$path"
    }
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Escape-Md {
    param([string]$Text)
    return ($Text -replace "\|", "\|" -replace "`r?`n", " ")
}

function Add-Check {
    param(
        [System.Collections.Generic.List[object]]$Rows,
        [string]$Group,
        [string]$Name,
        [string]$Requirement,
        [bool]$Passed,
        [string]$Detail
    )
    $Rows.Add([pscustomobject]@{
        Group = $Group
        Name = $Name
        Requirement = $Requirement
        Status = if ($Passed) { "通过" } else { "失败" }
        Detail = $Detail
    }) | Out-Null
}

function Add-ContainsCheck {
    param(
        [System.Collections.Generic.List[object]]$Rows,
        [string]$Group,
        [string]$Name,
        [string]$Requirement,
        [string]$Text,
        [string]$Needle,
        [string]$Detail
    )
    Add-Check -Rows $Rows -Group $Group -Name $Name -Requirement $Requirement `
        -Passed $Text.Contains($Needle) -Detail $Detail
}

function Get-Section {
    param(
        [string]$Text,
        [string]$StartNeedle,
        [string]$EndNeedle
    )
    $start = $Text.IndexOf($StartNeedle)
    if ($start -lt 0) {
        return ""
    }
    $end = $Text.IndexOf($EndNeedle, $start)
    if ($end -lt 0) {
        return $Text.Substring($start)
    }
    return $Text.Substring($start, $end - $start)
}

$cppPath = "standalone_app\qt_desktop\FaultPolyTraceWindow.cpp"
$headerPath = "standalone_app\qt_desktop\FaultPolyTraceWindow.h"
$packageScriptPath = "scripts\打包发布包.ps1"
$cpp = Read-TextFile $cppPath
$header = Read-TextFile $headerPath
$packageScript = Read-TextFile $packageScriptPath
$rows = [System.Collections.Generic.List[object]]::new()

Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "默认窗口" -Requirement "最小窗口不低于规划值" -Text $cpp -Needle "setMinimumSize(1100, 700)" -Detail "$cppPath 设置最小窗口"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "默认窗口" -Requirement "默认窗口接近 1280 x 760 以上" -Text $cpp -Needle "resize(1280, 780)" -Detail "$cppPath 设置默认窗口"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "工具栏图标" -Requirement "图标清晰且不使用过小尺寸" -Text $cpp -Needle "toolbar->setIconSize(QSize(20, 20))" -Detail "$cppPath 设置工具栏图标尺寸"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "工具栏文字" -Requirement "图标配合文字或工具提示" -Text $cpp -Needle "toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon)" -Detail "$cppPath 设置工具栏图标文字样式"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "侧栏滚动" -Requirement "窗口缩小时参数区可滚动" -Text $cpp -Needle "sidebarScroll->setWidgetResizable(true)" -Detail "$cppPath 侧栏使用 QScrollArea"

$menus = @(
    @{ Name="文件"; Needle="menuBar()->addMenu(QString::fromUtf8(`"文件(&F)`"))" },
    @{ Name="编辑"; Needle="menuBar()->addMenu(QString::fromUtf8(`"编辑(&E)`"))" },
    @{ Name="视图"; Needle="menuBar()->addMenu(QString::fromUtf8(`"视图(&V)`"))" },
    @{ Name="断层"; Needle="menuBar()->addMenu(QString::fromUtf8(`"断层(&L)`"))" },
    @{ Name="层位"; Needle="menuBar()->addMenu(QString::fromUtf8(`"层位(&H)`"))" },
    @{ Name="工具"; Needle="menuBar()->addMenu(QString::fromUtf8(`"工具(&T)`"))" },
    @{ Name="帮助"; Needle="menuBar()->addMenu(QString::fromUtf8(`"帮助(&H)`"))" }
)
foreach ($menu in $menus) {
    Add-ContainsCheck -Rows $rows -Group "菜单结构" -Name $menu.Name -Requirement "顶层菜单存在" -Text $cpp -Needle $menu.Needle -Detail "$cppPath 包含 $($menu.Name) 菜单"
}

$actions = @(
    @{ Name="打开 DAT 文件"; Var="openAction"; Label="打开 DAT 文件"; Slot="browseInput"; Menu="fileMenu"; Toolbar=$true },
    @{ Name="开始追踪"; Var="runAction_"; Label="开始追踪"; Slot="runTrace"; Menu="faultMenu"; Toolbar=$true },
    @{ Name="停止"; Var="stopAction_"; Label="停止"; Slot="stopTrace"; Menu="faultMenu"; Toolbar=$true },
    @{ Name="导出 PNG"; Var="pngAction"; Label="导出PNG"; Slot="exportPng"; Menu="fileMenu"; Toolbar=$true },
    @{ Name="保存参数模板"; Var="saveTplAction"; Label="保存当前配置"; Slot="saveTemplate"; Menu="fileMenu"; Toolbar=$true },
    @{ Name="打开参数模板"; Var="loadTplAction"; Label="打开配置"; Slot="loadTemplate"; Menu="fileMenu"; Toolbar=$false },
    @{ Name="恢复默认"; Var="resetAction"; Label="恢复默认参数"; Slot="restoreDefaults"; Menu="editMenu"; Toolbar=$true },
    @{ Name="打开输出目录"; Var="outputDirAction"; Label="打开输出目录"; Slot="openOutputDir"; Menu="toolsMenu"; Toolbar=$true },
    @{ Name="帮助"; Var="helpAction"; Label="帮助提示"; Slot="openHelp"; Menu="helpMenu"; Toolbar=$true }
)

foreach ($action in $actions) {
    $var = [string]$action.Var
    Add-ContainsCheck -Rows $rows -Group "动作定义" -Name $action.Name -Requirement "QAction 已创建" -Text $cpp -Needle "$var = new QAction" -Detail "$cppPath 创建 $var"
    Add-ContainsCheck -Rows $rows -Group "动作定义" -Name $action.Name -Requirement "中文标签正确" -Text $cpp -Needle "QString::fromUtf8(`"$($action.Label)`")" -Detail "$cppPath 标签 $($action.Label)"
    Add-ContainsCheck -Rows $rows -Group "动作定义" -Name $action.Name -Requirement "触发槽函数已连接" -Text $cpp -Needle "FaultPolyTraceWindow::$($action.Slot)" -Detail "$cppPath 连接 $($action.Slot)"
    Add-ContainsCheck -Rows $rows -Group "菜单动作" -Name $action.Name -Requirement "动作进入对应菜单" -Text $cpp -Needle "$($action.Menu)->addAction($var)" -Detail "$cppPath $($action.Menu) 添加 $var"
    if ([bool]$action.Toolbar) {
        Add-ContainsCheck -Rows $rows -Group "工具栏动作" -Name $action.Name -Requirement "动作进入工具栏" -Text $cpp -Needle "toolbar->addAction($var)" -Detail "$cppPath 工具栏添加 $var"
    }
}

$viewActions = @(
    @{ Name="任务预览"; Var="taskViewAction"; Value="任务预览" },
    @{ Name="原始网格"; Var="rawViewAction"; Value="原始网格" },
    @{ Name="结果多边形"; Var="resultViewAction"; Value="结果多边形" },
    @{ Name="叠加结果"; Var="overlayViewAction"; Value="叠加结果" }
)
foreach ($viewAction in $viewActions) {
    Add-ContainsCheck -Rows $rows -Group "视图切换" -Name $viewAction.Name -Requirement "视图菜单项存在" -Text $cpp -Needle "$($viewAction.Var) = viewMenu->addAction" -Detail "$cppPath 创建 $($viewAction.Var)"
    Add-ContainsCheck -Rows $rows -Group "视图切换" -Name $viewAction.Name -Requirement "菜单切换到对应视图" -Text $cpp -Needle "viewModeCombo_->setCurrentText(QString::fromUtf8(`"$($viewAction.Value)`"))" -Detail "$cppPath 切换到 $($viewAction.Value)"
}

$toolActions = @(
    @{ Name="平移"; Var="panAction"; Method="setPanMode" },
    @{ Name="放大"; Var="zoomInAction"; Method="zoomIn" },
    @{ Name="缩小"; Var="zoomOutAction"; Method="zoomOut" },
    @{ Name="重置视图"; Var="resetViewAction"; Method="resetView" }
)
foreach ($tool in $toolActions) {
    Add-ContainsCheck -Rows $rows -Group "视图工具" -Name $tool.Name -Requirement "工具动作进入视图菜单" -Text $cpp -Needle "$($tool.Var) = viewMenu->addAction" -Detail "$cppPath 创建 $($tool.Var)"
    Add-ContainsCheck -Rows $rows -Group "视图工具" -Name $tool.Name -Requirement "工具动作进入工具栏" -Text $cpp -Needle "toolbar->addAction($($tool.Var))" -Detail "$cppPath 工具栏添加 $($tool.Var)"
    Add-ContainsCheck -Rows $rows -Group "视图工具" -Name $tool.Name -Requirement "真实调用视图方法" -Text $cpp -Needle "view_->$($tool.Method)" -Detail "$cppPath 调用 $($tool.Method)"
}

Add-ContainsCheck -Rows $rows -Group "左侧主操作" -Name "开始追踪按钮" -Requirement "按钮连接运行槽" -Text $cpp -Needle "connect(runButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::runTrace)" -Detail "$cppPath 左侧按钮连接 runTrace"
Add-ContainsCheck -Rows $rows -Group "左侧主操作" -Name "停止按钮" -Requirement "按钮连接停止槽" -Text $cpp -Needle "connect(stopButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::stopTrace)" -Detail "$cppPath 左侧按钮连接 stopTrace"
Add-ContainsCheck -Rows $rows -Group "运行状态联动" -Name "运行按钮" -Requirement "运行中禁用开始动作" -Text $cpp -Needle "runAction_->setEnabled(!running)" -Detail "$cppPath setRunning 更新 runAction_"
Add-ContainsCheck -Rows $rows -Group "运行状态联动" -Name "停止按钮" -Requirement "运行中启用停止动作" -Text $cpp -Needle "stopAction_->setEnabled(running)" -Detail "$cppPath setRunning 更新 stopAction_"
Add-ContainsCheck -Rows $rows -Group "运行状态联动" -Name "动作成员" -Requirement "运行 QAction 成员声明" -Text $header -Needle "QAction* runAction_" -Detail "$headerPath 声明 runAction_"
Add-ContainsCheck -Rows $rows -Group "运行状态联动" -Name "动作成员" -Requirement "停止 QAction 成员声明" -Text $header -Needle "QAction* stopAction_" -Detail "$headerPath 声明 stopAction_"

$supportFunctions = @(
    @{ Name="导出 PNG"; Needle="void FaultPolyTraceWindow::exportPng()" },
    @{ Name="保存模板"; Needle="void FaultPolyTraceWindow::saveTemplate()" },
    @{ Name="打开模板"; Needle="void FaultPolyTraceWindow::loadTemplate()" },
    @{ Name="恢复默认"; Needle="void FaultPolyTraceWindow::restoreDefaults()" },
    @{ Name="加载历史"; Needle="void FaultPolyTraceWindow::loadHistory()" },
    @{ Name="打开输出目录"; Needle="void FaultPolyTraceWindow::openOutputDir()" },
    @{ Name="帮助"; Needle="void FaultPolyTraceWindow::openHelp()" },
    @{ Name="停止追踪"; Needle="void FaultPolyTraceWindow::stopTrace()" }
)
foreach ($function in $supportFunctions) {
    Add-ContainsCheck -Rows $rows -Group "功能实现" -Name $function.Name -Requirement "槽函数实现存在" -Text $cpp -Needle $function.Needle -Detail "$cppPath 包含 $($function.Needle)"
}

Add-ContainsCheck -Rows $rows -Group "发布链路" -Name "界面检查脚本" -Requirement "发布包携带本检查脚本" -Text $packageScript -Needle "检查独立软件界面功能.ps1" -Detail "$packageScriptPath 脚本清单包含 检查独立软件界面功能.ps1"

$failed = @($rows | Where-Object { $_.Status -eq "失败" })
$passed = @($rows | Where-Object { $_.Status -eq "通过" })

New-Item -ItemType Directory -Force -Path $reportDir | Out-Null

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# 独立软件界面功能检查报告") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("生成时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## 检查目的") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("本报告用于核对独立桌面软件的菜单、工具栏、左侧主操作、视图切换、运行状态联动和发布链路是否满足《独立软件与 GeoEast 插件全流程开发规划》的阶段 2 要求。") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## 汇总") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| 项目 | 数量 |") | Out-Null
$lines.Add("|---|---:|") | Out-Null
$lines.Add("| 通过 | $($passed.Count) |") | Out-Null
$lines.Add("| 失败 | $($failed.Count) |") | Out-Null
$lines.Add("") | Out-Null

foreach ($group in ($rows | Select-Object -ExpandProperty Group -Unique)) {
    $lines.Add("## $group") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("| 检查项 | 要求 | 状态 | 细节 |") | Out-Null
    $lines.Add("|---|---|---|---|") | Out-Null
    foreach ($row in ($rows | Where-Object { $_.Group -eq $group })) {
        $lines.Add("| $(Escape-Md $row.Name) | $(Escape-Md $row.Requirement) | $($row.Status) | $(Escape-Md $row.Detail) |") | Out-Null
    }
    $lines.Add("") | Out-Null
}

if ($failed.Count -gt 0) {
    $lines.Add("## 失败项处理建议") | Out-Null
    $lines.Add("") | Out-Null
    foreach ($row in $failed) {
        $lines.Add("- $($row.Group) / $($row.Name) / $($row.Requirement)：$($row.Detail)") | Out-Null
    }
    $lines.Add("") | Out-Null
}

[System.IO.File]::WriteAllText($reportPath, ($lines -join [Environment]::NewLine), [System.Text.Encoding]::UTF8)

Write-Host "独立软件界面功能检查报告：$reportPath"
Write-Host "通过：$($passed.Count)  失败：$($failed.Count)"

if ($failed.Count -gt 0) {
    exit 1
}
