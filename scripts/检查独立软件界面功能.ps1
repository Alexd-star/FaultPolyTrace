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
$mainPath = "standalone_app\qt_desktop\main.cpp"
$proPath = "standalone_app\qt_desktop\FaultPolyTraceDesktop.pro"
$packageScriptPath = "scripts\打包发布包.ps1"
$cpp = Read-TextFile $cppPath
$header = Read-TextFile $headerPath
$main = Read-TextFile $mainPath
$pro = Read-TextFile $proPath
$packageScript = Read-TextFile $packageScriptPath
$rows = [System.Collections.Generic.List[object]]::new()

Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "默认窗口" -Requirement "最小窗口不低于规划值" -Text $cpp -Needle "setMinimumSize(1100, 700)" -Detail "$cppPath 设置最小窗口"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "默认窗口" -Requirement "默认窗口接近 1280 x 760 以上" -Text $cpp -Needle "resize(1280, 780)" -Detail "$cppPath 设置默认窗口"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "工具栏图标" -Requirement "图标清晰且不使用过小尺寸" -Text $cpp -Needle "toolbar->setIconSize(QSize(20, 20))" -Detail "$cppPath 设置工具栏图标尺寸"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "工具栏文字" -Requirement "图标配合文字或工具提示" -Text $cpp -Needle "toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon)" -Detail "$cppPath 设置工具栏图标文字样式"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "侧栏滚动" -Requirement "窗口缩小时参数区可滚动" -Text $cpp -Needle "sidebarScroll->setWidgetResizable(true)" -Detail "$cppPath 侧栏使用 QScrollArea"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "运行时图标" -Requirement "窗口和任务栏使用产品图标" -Text $main -Needle "app.setWindowIcon(locateApplicationIcon())" -Detail "$mainPath 设置运行时图标"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "EXE图标" -Requirement "Windows 可执行文件嵌入产品图标" -Text $pro -Needle "RC_ICONS = ../../assets/icons/faultpolytrace.ico" -Detail "$proPath 配置 RC_ICONS"
Add-ContainsCheck -Rows $rows -Group "发布链路" -Name "发布图标" -Requirement "发布包根目录携带产品图标文件" -Text $packageScript -Needle 'faultpolytrace.ico' -Detail "$packageScriptPath 复制 faultpolytrace.ico"

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
    Add-ContainsCheck -Rows $rows -Group "视图工具" -Name $tool.Name -Requirement "实际调用视图方法" -Text $cpp -Needle "view_->$($tool.Method)" -Detail "$cppPath 调用 $($tool.Method)"
}

Add-ContainsCheck -Rows $rows -Group "左侧主操作" -Name "开始追踪按钮" -Requirement "按钮连接运行槽" -Text $cpp -Needle "connect(runButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::runTrace)" -Detail "$cppPath 左侧按钮连接 runTrace"
Add-ContainsCheck -Rows $rows -Group "左侧主操作" -Name "停止按钮" -Requirement "按钮连接停止槽" -Text $cpp -Needle "connect(stopButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::stopTrace)" -Detail "$cppPath 左侧按钮连接 stopTrace"
Add-ContainsCheck -Rows $rows -Group "运行状态联动" -Name "运行按钮" -Requirement "运行中禁用开始动作" -Text $cpp -Needle "runAction_->setEnabled(!running)" -Detail "$cppPath setRunning 更新 runAction_"
Add-ContainsCheck -Rows $rows -Group "运行状态联动" -Name "停止按钮" -Requirement "运行中启用停止动作" -Text $cpp -Needle "stopAction_->setEnabled(running)" -Detail "$cppPath setRunning 更新 stopAction_"
Add-ContainsCheck -Rows $rows -Group "运行状态联动" -Name "动作成员" -Requirement "运行 QAction 成员声明" -Text $header -Needle "QAction* runAction_" -Detail "$headerPath 声明 runAction_"
Add-ContainsCheck -Rows $rows -Group "运行状态联动" -Name "动作成员" -Requirement "停止 QAction 成员声明" -Text $header -Needle "QAction* stopAction_" -Detail "$headerPath 声明 stopAction_"

$resultChecks = @(
    @{ Name="结果查看分组"; Needle="QGroupBox* resultBox = new QGroupBox(QString::fromUtf8(`"结果查看`")"; Requirement="左侧结果查看入口存在" },
    @{ Name="查看叠加按钮"; Needle="showOverlayButton_ = new QPushButton(QString::fromUtf8(`"查看叠加`")"; Requirement="可直接切换到叠加结果" },
    @{ Name="打开 CSV 按钮"; Needle="openCsvButton_ = new QPushButton(QString::fromUtf8(`"打开CSV`")"; Requirement="可直接打开结果 CSV" },
    @{ Name="打开 GeoJSON 按钮"; Needle="openGeoJsonButton_ = new QPushButton(QString::fromUtf8(`"GeoJSON`")"; Requirement="可直接打开 GeoJSON" },
    @{ Name="打开诊断按钮"; Needle="openDiagnosticsButton_ = new QPushButton(QString::fromUtf8(`"诊断JSON`")"; Requirement="可直接打开诊断 JSON" },
    @{ Name="结果区刷新"; Needle="void FaultPolyTraceWindow::updateResultPanel()"; Requirement="结果文件状态有统一刷新函数" },
    @{ Name="完成后自动叠加"; Needle="viewModeCombo_->setCurrentText(QString::fromUtf8(`"叠加结果`"))"; Requirement="运行完成或历史载入后自动显示叠加结果" },
    @{ Name="帮助说明结果入口"; Needle="在左侧【结果查看】中打开 CSV、GeoJSON、诊断 JSON 或输出目录"; Requirement="帮助提示说明结果查看方式" }
)
foreach ($check in $resultChecks) {
    Add-ContainsCheck -Rows $rows -Group "结果查看" -Name $check.Name -Requirement $check.Requirement -Text $cpp -Needle $check.Needle -Detail "$cppPath 包含 $($check.Name)"
}

$resultSlots = @(
    @{ Name="查看叠加"; Slot="showOverlayResult" },
    @{ Name="打开 CSV"; Slot="openResultCsv" },
    @{ Name="打开 GeoJSON"; Slot="openResultGeoJson" },
    @{ Name="打开诊断 JSON"; Slot="openDiagnosticsJson" }
)
foreach ($slot in $resultSlots) {
    Add-ContainsCheck -Rows $rows -Group "结果查看" -Name $slot.Name -Requirement "结果按钮连接实际槽函数" -Text $cpp -Needle "FaultPolyTraceWindow::$($slot.Slot)" -Detail "$cppPath 连接 $($slot.Slot)"
    Add-ContainsCheck -Rows $rows -Group "结果查看" -Name $slot.Name -Requirement "槽函数声明存在" -Text $header -Needle "void $($slot.Slot)()" -Detail "$headerPath 声明 $($slot.Slot)"
}

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

Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "配置自检声明" -Requirement "独立软件在读写 GeoEast 数据库前必须先校验配置完整性" -Text $header -Needle "bool ensureGeoEastConfigReady" -Detail "$headerPath 声明 ensureGeoEastConfigReady"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "缺失字段清单声明" -Requirement "配置不完整时必须明确列出缺失字段" -Text $header -Needle "QStringList missingGeoEastConfig" -Detail "$headerPath 声明 missingGeoEastConfig"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "刷新前自检" -Requirement "刷新 GeoEast 网格前不能直接调用执行程序，必须先做配置自检" -Text $cpp -Needle 'ensureGeoEastConfigReady(QString::fromUtf8("刷新网格列表")' -Detail "$cppPath refreshGeoEastGrids 调用配置自检"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "追踪前自检" -Requirement "GeoEast 数据库追踪前必须校验输入网格和输出名称" -Text $cpp -Needle 'ensureGeoEastConfigReady(QString::fromUtf8("开始追踪")' -Detail "$cppPath runTrace 调用配置自检"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "删除前自检" -Requirement "删除 GeoEast 结果前必须校验数据库配置和输出对象名" -Text $cpp -Needle 'ensureGeoEastConfigReady(QString::fromUtf8("删除当前结果")' -Detail "$cppPath deleteGeoEastResult 调用配置自检"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "配置对话入口" -Requirement "配置不完整时应能直接打开 GeoEast 配置对话框" -Text $cpp -Needle "打开数据库配置" -Detail "$cppPath 提供打开数据库配置按钮"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "执行程序术语" -Requirement "GeoEast 配置使用执行程序这一用户术语" -Text $cpp -Needle "执行程序" -Detail "$cppPath 使用 执行程序"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "结果视图术语" -Requirement "运行完成提示使用结果视图文件这一用户术语" -Text $cpp -Needle "结果视图文件" -Detail "$cppPath 使用 结果视图文件"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "GeoEast网格术语" -Requirement "GeoEast 输入对象在界面上表述为 GeoEast 网格" -Text $cpp -Needle "GeoEast 网格" -Detail "$cppPath 使用 GeoEast 网格"
Add-Check -Rows $rows -Group "GeoEast数据库操作" -Name "不用计算引擎" -Requirement "用户界面不使用计算引擎这一术语" -Passed (-not $cpp.Contains("计算引擎")) -Detail "$cppPath 不应包含 计算引擎"
Add-Check -Rows $rows -Group "GeoEast数据库操作" -Name "不用真实视图" -Requirement "用户界面不使用真实视图这一实现术语" -Passed (-not $cpp.Contains("真实视图")) -Detail "$cppPath 不应包含 真实视图"
Add-Check -Rows $rows -Group "GeoEast数据库操作" -Name "不用载入真实" -Requirement "用户界面不使用载入真实这一实现术语" -Passed (-not $cpp.Contains("载入真实")) -Detail "$cppPath 不应包含 载入真实"

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
