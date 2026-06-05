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
$userManualPath = "docs\用户操作手册.md"
$productManualPath = "docs\企业级用户操作手册与产品说明书.md"
$projectStructureDocPath = "docs\项目目录结构与文件说明.md"
$cpp = Read-TextFile $cppPath
$header = Read-TextFile $headerPath
$main = Read-TextFile $mainPath
$pro = Read-TextFile $proPath
$packageScript = Read-TextFile $packageScriptPath
$userManual = Read-TextFile $userManualPath
$productManual = Read-TextFile $productManualPath
$projectStructureDoc = Read-TextFile $projectStructureDocPath
$rows = [System.Collections.Generic.List[object]]::new()

Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "默认窗口" -Requirement "最小窗口不低于规划值" -Text $cpp -Needle "setMinimumSize(1100, 700)" -Detail "$cppPath 设置最小窗口"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "默认窗口" -Requirement "默认窗口接近 1280 x 760 以上" -Text $cpp -Needle "resize(1280, 780)" -Detail "$cppPath 设置默认窗口"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "工具栏图标" -Requirement "图标清晰且不使用过小尺寸" -Text $cpp -Needle "toolbar->setIconSize(QSize(20, 20))" -Detail "$cppPath 设置工具栏图标尺寸"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "工具栏文字" -Requirement "图标配合文字或工具提示" -Text $cpp -Needle "toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon)" -Detail "$cppPath 设置工具栏图标文字样式"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "侧栏滚动" -Requirement "窗口缩小时参数区可滚动" -Text $cpp -Needle "sidebarScroll->setWidgetResizable(true)" -Detail "$cppPath 侧栏使用 QScrollArea"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "运行时图标" -Requirement "窗口和任务栏使用产品图标" -Text $main -Needle "app.setWindowIcon(locateApplicationIcon())" -Detail "$mainPath 设置运行时图标"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "EXE图标" -Requirement "Windows 可执行文件嵌入产品图标" -Text $pro -Needle "RC_ICONS = ../../assets/icons/faultpolytrace.ico" -Detail "$proPath 配置 RC_ICONS"
Add-ContainsCheck -Rows $rows -Group "发布链路" -Name "发布图标" -Requirement "发布包根目录携带产品图标文件" -Text $packageScript -Needle 'faultpolytrace.ico' -Detail "$packageScriptPath 复制 faultpolytrace.ico"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "侧栏最小宽度" -Requirement "左侧参数区必须保留最小可用宽度，避免文字或输入框遮挡" -Text $cpp -Needle "leftPane->setMinimumWidth(340)" -Detail "$cppPath 设置左侧参数区最小宽度"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "可拖动主分隔栏" -Requirement "左侧参数区与结果视图区之间必须可通过鼠标拖动调整宽度" -Text $header -Needle "QSplitter* mainSplitter_" -Detail "$headerPath 声明主分隔栏"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "分隔栏防折叠" -Requirement "拖动时不能把参数区或结果区完全折叠" -Text $cpp -Needle "mainSplitter_->setChildrenCollapsible(false)" -Detail "$cppPath 设置分隔栏不可完全折叠"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "保存分隔栏宽度" -Requirement "用户调整后的左右宽度必须在下次启动时恢复" -Text $cpp -Needle 'settings.setValue("mainSplitterState"' -Detail "$cppPath 保存分隔栏状态"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "恢复分隔栏宽度" -Requirement "启动时必须恢复用户上次调整的左右宽度" -Text $cpp -Needle 'settings.value("mainSplitterState")' -Detail "$cppPath 恢复分隔栏状态"
Add-Check -Rows $rows -Group "窗口与布局" -Name "侧栏不再固定宽度" -Requirement "使用可拖动分隔栏后，左侧参数区不能继续固定为 420 像素" -Passed (-not $cpp.Contains("leftPane->setFixedWidth(420)")) -Detail "$cppPath 移除固定宽度"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "表单标签宽度" -Requirement "表单标签列应留足中文字段宽度" -Text $cpp -Needle "label->setFixedWidth(100)" -Detail "$cppPath 设置表单标签宽度"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "首个分组留白" -Requirement "滚动区顶部必须留出标题绘制空间，避免首个分组标题被裁剪" -Text $cpp -Needle "sidebarLayout->setContentsMargins(0, 8, 4, 0)" -Detail "$cppPath 设置侧栏顶部留白"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "分组标题背景" -Requirement "分组标题应使用稳定背景，避免浮动标题与边框或子控件混叠" -Text $cpp -Needle "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 5px; background: #F0F0F0;" -Detail "$cppPath 设置 QGroupBox 标题背景"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "首组内嵌标题" -Requirement "第一个分组不使用容易被滚动区裁剪的浮动标题，应使用组内稳定标题行" -Text $cpp -Needle 'dataTitle->setObjectName("sectionTitle")' -Detail "$cppPath 数据与输出使用组内标题"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "首组非浮动标题容器" -Requirement "第一个分组应使用 QFrame 容器，避免空 QGroupBox 标题槽形成白框" -Text $cpp -Needle 'dataBox->setObjectName("sectionBox")' -Detail "$cppPath 数据与输出使用 sectionBox"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "输入文件占位提示" -Requirement "独立软件默认不选择样例文件，输入框必须提示用户主动选择" -Text $cpp -Needle "请选择平面断层属性文件" -Detail "$cppPath 设置输入文件占位提示"
Add-ContainsCheck -Rows $rows -Group "窗口与布局" -Name "内部配置字段隐藏" -Requirement "仅供数据库配置对话框使用的内部 QLineEdit 不得作为未布局控件显示在侧栏左上角" -Text $cpp -Needle "internalConfigEdit->hide()" -Detail "$cppPath 隐藏内部配置字段"

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
    Add-ContainsCheck -Rows $rows -Group "视图切换" -Name $viewAction.Name -Requirement "菜单切换到对应视图" -Text $cpp -Needle "setComboByKey(viewModeCombo_, QString::fromUtf8(`"$($viewAction.Value)`"))" -Detail "$cppPath 使用稳定键切换到 $($viewAction.Value)"
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
    @{ Name="完成后自动叠加"; Needle="setComboByKey(viewModeCombo_, QString::fromUtf8(`"叠加结果`"))"; Requirement="运行完成或历史载入后自动显示叠加结果" },
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

Add-ContainsCheck -Rows $rows -Group "默认值与结果状态" -Name "默认输入为空" -Requirement "独立软件首次启动不得自动选择随包测试文件" -Text $cpp -Needle 'settingOrCurrent(settings, "input", QString(), schemaVersion)' -Detail "$cppPath loadSettings 使用空输入作为默认值"
Add-Check -Rows $rows -Group "默认值与结果状态" -Name "恢复默认不改输入" -Requirement "恢复默认参数不得改动用户选择的输入文件" -Passed (-not (Get-Section -Text $cpp -StartNeedle "void FaultPolyTraceWindow::restoreDefaults()" -EndNeedle "void FaultPolyTraceWindow::appendHistory").Contains("inputEdit_->setText")) -Detail "$cppPath restoreDefaults 不调用 inputEdit_->setText"
Add-Check -Rows $rows -Group "默认值与结果状态" -Name "恢复默认不改输出目录" -Requirement "恢复默认参数不得改动输出目录和输出名称" -Passed (-not (Get-Section -Text $cpp -StartNeedle "void FaultPolyTraceWindow::restoreDefaults()" -EndNeedle "void FaultPolyTraceWindow::appendHistory").Contains("outputDirEdit_->setText") -and -not (Get-Section -Text $cpp -StartNeedle "void FaultPolyTraceWindow::restoreDefaults()" -EndNeedle "void FaultPolyTraceWindow::appendHistory").Contains("outputNameEdit_->setText")) -Detail "$cppPath restoreDefaults 不重置输出路径"
Add-ContainsCheck -Rows $rows -Group "默认值与结果状态" -Name "输入变化清理结果" -Requirement "输入文件改变后必须清理上一轮结果状态，避免新输入叠加旧多边形" -Text $header -Needle "void clearResultStateForNewInput()" -Detail "$headerPath 声明 clearResultStateForNewInput"
Add-ContainsCheck -Rows $rows -Group "默认值与结果状态" -Name "输入变化连接" -Requirement "输入文件文本变化必须触发结果状态清理" -Text $cpp -Needle "FaultPolyTraceWindow::clearResultStateForNewInput" -Detail "$cppPath 连接输入变化清理"
Add-ContainsCheck -Rows $rows -Group "默认值与结果状态" -Name "视图清理多边形" -Requirement "真实图层视图区必须支持清空旧多边形图层" -Text $cpp -Needle "void clearPolygons" -Detail "$cppPath AttributeView 提供 clearPolygons"
Add-Check -Rows $rows -Group "默认值与结果状态" -Name "结果预览不回退预计路径" -Requirement "没有本次结果时不得从预计输出路径自动载入旧 CSV" -Passed (-not (Get-Section -Text $cpp -StartNeedle "void FaultPolyTraceWindow::refreshResultPreview()" -EndNeedle "void FaultPolyTraceWindow::runTrace").Contains("outputCsvPath()")) -Detail "$cppPath refreshResultPreview 只载入 lastOutputCsv_"
Add-ContainsCheck -Rows $rows -Group "默认值与结果状态" -Name "未追踪提示" -Requirement "已选择输入但尚未追踪时必须明确说明当前不是处理后的叠加结果" -Text $cpp -Needle "已载入原始网格，尚未运行追踪" -Detail "$cppPath 提供未追踪状态提示"
Add-ContainsCheck -Rows $rows -Group "默认值与结果状态" -Name "恢复范围提示" -Requirement "恢复默认参数后必须提示只恢复算法与显示参数" -Text $cpp -Needle "已恢复推荐算法与显示参数" -Detail "$cppPath restoreDefaults 提示作用范围"
Add-ContainsCheck -Rows $rows -Group "默认值与结果状态" -Name "空输入任务预览" -Requirement "没有输入且没有结果时必须显示任务预览和参数摘要" -Text $cpp -Needle 'setComboByKey(viewModeCombo_, QString::fromUtf8("任务预览"))' -Detail "$cppPath 空输入回到任务预览"
Add-ContainsCheck -Rows $rows -Group "默认值与结果状态" -Name "未追踪原始网格" -Requirement "已选择输入但没有结果时应显示原始网格，不应默认停留在叠加结果" -Text $cpp -Needle 'setComboByKey(viewModeCombo_, QString::fromUtf8("原始网格"))' -Detail "$cppPath 未追踪时回到原始网格"

Add-ContainsCheck -Rows $rows -Group "帮助文档" -Name "用户操作手册入口" -Requirement "帮助菜单必须能打开完整用户操作手册" -Text $header -Needle "void openUserManual()" -Detail "$headerPath 声明 openUserManual"
Add-ContainsCheck -Rows $rows -Group "帮助文档" -Name "产品说明书入口" -Requirement "帮助菜单必须能打开企业级产品说明书" -Text $header -Needle "void openProductManual()" -Detail "$headerPath 声明 openProductManual"
Add-ContainsCheck -Rows $rows -Group "帮助文档" -Name "目录结构入口" -Requirement "帮助菜单必须能打开项目目录结构说明" -Text $header -Needle "void openProjectStructureDoc()" -Detail "$headerPath 声明 openProjectStructureDoc"
Add-ContainsCheck -Rows $rows -Group "帮助文档" -Name "相对路径定位" -Requirement "文档定位不能写死用户电脑路径，必须相对发布包或源码根目录查找" -Text $header -Needle "QString locateBundledFile" -Detail "$headerPath 声明 locateBundledFile"
Add-ContainsCheck -Rows $rows -Group "帮助文档" -Name "统一打开函数" -Requirement "文档打开失败时必须提示并引导用户检查发布包 docs 目录" -Text $header -Needle "bool openBundledDocument" -Detail "$headerPath 声明 openBundledDocument"
Add-ContainsCheck -Rows $rows -Group "帮助文档" -Name "帮助菜单挂载用户手册" -Requirement "用户操作手册必须出现在帮助菜单" -Text $cpp -Needle "helpMenu->addAction(userManualAction)" -Detail "$cppPath 帮助菜单添加用户手册"
Add-ContainsCheck -Rows $rows -Group "帮助文档" -Name "帮助菜单挂载产品说明书" -Requirement "产品说明书必须出现在帮助菜单" -Text $cpp -Needle "helpMenu->addAction(productManualAction)" -Detail "$cppPath 帮助菜单添加产品说明书"
Add-ContainsCheck -Rows $rows -Group "帮助文档" -Name "帮助菜单挂载目录结构" -Requirement "目录结构说明必须出现在帮助菜单" -Text $cpp -Needle "helpMenu->addAction(projectStructureAction)" -Detail "$cppPath 帮助菜单添加目录结构说明"
Add-ContainsCheck -Rows $rows -Group "帮助文档" -Name "用户手册内容" -Requirement "用户操作手册必须说明软件入口和前置提示" -Text $userManual -Needle "软件入口与随包文档" -Detail "$userManualPath 包含入口说明"
Add-ContainsCheck -Rows $rows -Group "帮助文档" -Name "产品说明书内容" -Requirement "企业级产品说明书必须可作为帮助文档打开" -Text $productManual -Needle "企业级用户操作手册" -Detail "$productManualPath 内容存在"
Add-ContainsCheck -Rows $rows -Group "帮助文档" -Name "目录结构文档内容" -Requirement "项目目录结构文档必须说明入口和目录作用" -Text $projectStructureDoc -Needle "独立软件入口" -Detail "$projectStructureDocPath 内容存在"

Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "配置自检声明" -Requirement "独立软件在读写 GeoEast 数据库前必须先校验配置完整性" -Text $header -Needle "bool ensureGeoEastConfigReady" -Detail "$headerPath 声明 ensureGeoEastConfigReady"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "缺失字段清单声明" -Requirement "配置不完整时必须明确列出缺失字段" -Text $header -Needle "QStringList missingGeoEastConfig" -Detail "$headerPath 声明 missingGeoEastConfig"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "刷新前自检" -Requirement "刷新 GeoEast 网格前不能直接调用执行程序，必须先做配置自检" -Text $cpp -Needle 'ensureGeoEastConfigReady(QString::fromUtf8("刷新网格列表")' -Detail "$cppPath refreshGeoEastGrids 调用配置自检"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "引导式目录下拉框" -Requirement "空的网格或结果下拉框必须拦截弹出动作并执行引导逻辑" -Text $cpp -Needle "class GuidedCatalogComboBox" -Detail "$cppPath 定义引导式目录下拉框"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "空网格下拉引导" -Requirement "GeoEast 模式点击空网格下拉框时必须检查配置并引导刷新" -Text $cpp -Needle "选择输入网格" -Detail "$cppPath 处理空网格下拉框"
Add-ContainsCheck -Rows $rows -Group "结果查看" -Name "空结果下拉引导" -Requirement "点击空结果下拉框时必须说明缺少配置或当前目录没有结果" -Text $cpp -Needle "选择已有结果" -Detail "$cppPath 处理空结果下拉框"
Add-ContainsCheck -Rows $rows -Group "结果查看" -Name "空目录自动刷新" -Requirement "配置完整时点击空目录下拉框应自动刷新对应目录" -Text $cpp -Needle "列表为空，正在自动刷新" -Detail "$cppPath 提示并启动自动刷新"
Add-Check -Rows $rows -Group "结果查看" -Name "空结果下拉保持可点击" -Requirement "没有结果时下拉框仍须可点击，以便向用户显示原因和下一步" -Passed (-not $cpp.Contains("resultCatalogCombo_->setEnabled(false)")) -Detail "$cppPath 不禁用空结果下拉框"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "追踪前自检" -Requirement "GeoEast 数据库追踪前必须校验输入网格和输出名称" -Text $cpp -Needle 'ensureGeoEastConfigReady(QString::fromUtf8("开始追踪")' -Detail "$cppPath runTrace 调用配置自检"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "删除前自检" -Requirement "删除 GeoEast 结果前必须校验数据库配置和输出对象名" -Text $cpp -Needle 'ensureGeoEastConfigReady(QString::fromUtf8("删除当前结果")' -Detail "$cppPath deleteGeoEastResult 调用配置自检"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "结果列表自检" -Requirement "刷新 GeoEast 已有结果前必须校验数据库配置" -Text $cpp -Needle 'ensureGeoEastConfigReady(QString::fromUtf8("刷新已有结果")' -Detail "$cppPath refreshAvailableResults 调用配置自检"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "结果对象枚举" -Requirement "独立软件应支持列出当前项目中的 BOFaultPolygon 结果对象" -Text $cpp -Needle '"--list-fault-polygons"' -Detail "$cppPath 通过 runner 刷新 GeoEast 结果列表"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "历史结果导出" -Requirement "独立软件应支持把已有 BOFaultPolygon 导出为本地 CSV 并载入查看" -Text $cpp -Needle '"--export-fault-polygons-output"' -Detail "$cppPath 通过 runner 导出 GeoEast 历史结果"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "写库成功提示" -Requirement "GeoEast 模式完成后必须明确提示结果写入 BOFaultPolygon" -Text $cpp -Needle "GeoEast 数据库写入对象：BOFaultPolygon" -Detail "$cppPath 记录数据库写入对象"
Add-ContainsCheck -Rows $rows -Group "结果查看" -Name "已有结果下拉" -Requirement "独立软件应提供已有结果下拉列表" -Text $header -Needle "QComboBox* resultCatalogCombo_" -Detail "$headerPath 声明 resultCatalogCombo_"
Add-ContainsCheck -Rows $rows -Group "结果查看" -Name "载入已有结果" -Requirement "独立软件应能载入输出目录中的已有结果 CSV" -Text $header -Needle "void loadSelectedResult()" -Detail "$headerPath 声明 loadSelectedResult"
Add-ContainsCheck -Rows $rows -Group "结果查看" -Name "外部结果扫描" -Requirement "外部文件模式应扫描输出目录中的已有 CSV 结果" -Text $header -Needle "QStringList localResultCandidates() const" -Detail "$headerPath 声明 localResultCandidates"
Add-ContainsCheck -Rows $rows -Group "结果查看" -Name "历史结果任务" -Requirement "历史 GeoEast 结果导出应有独立任务状态，不与重新追踪混淆" -Text $header -Needle "ExportGeoEastResult" -Detail "$headerPath 声明 ExportGeoEastResult"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "配置对话入口" -Requirement "配置不完整时应能直接打开 GeoEast 配置对话框" -Text $cpp -Needle "打开数据库配置" -Detail "$cppPath 提供打开数据库配置按钮"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "执行程序术语" -Requirement "GeoEast 配置使用执行程序这一用户术语" -Text $cpp -Needle "执行程序" -Detail "$cppPath 使用 执行程序"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "结果视图术语" -Requirement "运行完成提示使用结果视图文件这一用户术语" -Text $cpp -Needle "结果视图文件" -Detail "$cppPath 使用 结果视图文件"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "GeoEast网格术语" -Requirement "GeoEast 输入对象在界面上表述为 GeoEast 网格" -Text $cpp -Needle "GeoEast 网格" -Detail "$cppPath 使用 GeoEast 网格"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "配置帮助入口" -Requirement "数据库配置弹窗必须提供帮助入口，说明每个配置项从哪里获取" -Text $cpp -Needle "GeoEast 配置帮助" -Detail "$cppPath 提供 GeoEast 配置帮助"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "自动检测配置" -Requirement "数据库配置弹窗必须能自动检测执行程序、GeoEast 根目录、数据根目录、数据库和用户" -Text $cpp -Needle "自动检测" -Detail "$cppPath 提供自动检测按钮"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "检查配置" -Requirement "数据库配置弹窗必须能在保存前检查缺失和无效项" -Text $cpp -Needle "检查配置" -Detail "$cppPath 提供检查配置按钮"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "配置校验函数" -Requirement "数据库配置弹窗校验应集中在可复用函数中，避免只靠运行时报错" -Text $cpp -Needle "validateGeoEastConfigValues" -Detail "$cppPath 包含配置值校验"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "数据库名自动识别" -Requirement "数据库名应可从环境变量或 pgdata 目录自动识别" -Text $cpp -Needle "detectGeoEastDatabaseName" -Detail "$cppPath 包含数据库名识别"
Add-ContainsCheck -Rows $rows -Group "GeoEast数据库操作" -Name "项目名边界提示" -Requirement "项目名不能瞎猜，界面必须提示用户从 GeoEast 当前项目确认" -Text $cpp -Needle "项目名无法安全推断" -Detail "$cppPath 提示项目名边界"
Add-Check -Rows $rows -Group "GeoEast数据库操作" -Name "数据库配置弹窗无最大化" -Requirement "数据库配置是小窗，不应提供最大化按钮" -Passed (-not (Get-Section -Text $cpp -StartNeedle "void FaultPolyTraceWindow::showGeoEastConfigDialog()" -EndNeedle "QStringList FaultPolyTraceWindow::missingGeoEastConfig").Contains("Qt::WindowMaximizeButtonHint |")) -Detail "$cppPath 数据库配置弹窗不添加最大化按钮"
Add-Check -Rows $rows -Group "GeoEast数据库操作" -Name "数据库配置弹窗无内嵌长说明" -Requirement "数据库配置弹窗为高密度表单，配置来源说明应放入帮助按钮，不能在表单中插入多行说明造成遮挡" -Passed (-not (Get-Section -Text $cpp -StartNeedle "void FaultPolyTraceWindow::showGeoEastConfigDialog()" -EndNeedle "QStringList FaultPolyTraceWindow::missingGeoEastConfig").Contains("fieldHint") -and -not (Get-Section -Text $cpp -StartNeedle "void FaultPolyTraceWindow::showGeoEastConfigDialog()" -EndNeedle "QStringList FaultPolyTraceWindow::missingGeoEastConfig").Contains("addHint")) -Detail "$cppPath 数据库配置弹窗不包含 fieldHint/addHint"
Add-Check -Rows $rows -Group "GeoEast数据库操作" -Name "不用计算引擎" -Requirement "用户界面不使用计算引擎这一术语" -Passed (-not $cpp.Contains("计算引擎")) -Detail "$cppPath 不应包含 计算引擎"
Add-Check -Rows $rows -Group "GeoEast数据库操作" -Name "不用真实视图" -Requirement "用户界面不使用真实视图这一实现术语" -Passed (-not $cpp.Contains("真实视图")) -Detail "$cppPath 不应包含 真实视图"
Add-Check -Rows $rows -Group "GeoEast数据库操作" -Name "不用载入真实" -Requirement "用户界面不使用载入真实这一实现术语" -Passed (-not $cpp.Contains("载入真实")) -Detail "$cppPath 不应包含 载入真实"

Add-ContainsCheck -Rows $rows -Group "语言切换" -Name "语言枚举" -Requirement "独立软件维护当前界面语言状态" -Text $header -Needle "enum class UiLanguage" -Detail "$headerPath 声明 UiLanguage"
Add-ContainsCheck -Rows $rows -Group "语言切换" -Name "切换槽函数" -Requirement "独立软件支持运行时切换语言" -Text $header -Needle "void setLanguage" -Detail "$headerPath 声明 setLanguage"
Add-ContainsCheck -Rows $rows -Group "语言切换" -Name "重绘界面文字" -Requirement "语言变化后刷新菜单、工具栏、侧栏和状态文字" -Text $header -Needle "void retranslateUi()" -Detail "$headerPath 声明 retranslateUi"
Add-ContainsCheck -Rows $rows -Group "语言切换" -Name "语言菜单" -Requirement "菜单栏提供语言切换入口" -Text $cpp -Needle 'menuBar()->addMenu(uiText("语言(&I)"))' -Detail "$cppPath 添加语言菜单"
Add-ContainsCheck -Rows $rows -Group "语言切换" -Name "中文动作" -Requirement "语言菜单包含简体中文" -Text $cpp -Needle "languageChineseAction_" -Detail "$cppPath 创建中文语言动作"
Add-ContainsCheck -Rows $rows -Group "语言切换" -Name "英文动作" -Requirement "语言菜单包含 English" -Text $cpp -Needle "languageEnglishAction_" -Detail "$cppPath 创建英文语言动作"
Add-ContainsCheck -Rows $rows -Group "语言切换" -Name "保存语言" -Requirement "关闭后记住上次语言" -Text $cpp -Needle 'settings.setValue("language"' -Detail "$cppPath 保存 language"
Add-ContainsCheck -Rows $rows -Group "语言切换" -Name "加载语言" -Requirement "启动时恢复上次语言" -Text $cpp -Needle 'settings.value("language"' -Detail "$cppPath 加载 language"
Add-ContainsCheck -Rows $rows -Group "语言切换" -Name "视图区语言" -Requirement "中间结果视图区随语言切换" -Text $cpp -Needle "view_->setLanguage(currentLanguage_)" -Detail "$cppPath 设置视图区语言"
Add-ContainsCheck -Rows $rows -Group "语言切换" -Name "共享本地化模块" -Requirement "独立软件必须复用统一本地化模块，避免散落翻译" -Text $cpp -Needle '#include "UiLocalization.h"' -Detail "$cppPath 引用 UiLocalization"
Add-ContainsCheck -Rows $rows -Group "语言切换" -Name "本地化源码编译" -Requirement "构建脚本必须编译共享本地化实现" -Text $pro -Needle "../../src/ui/UiLocalization.cpp" -Detail "$proPath 编译 UiLocalization.cpp"
Add-ContainsCheck -Rows $rows -Group "语言切换" -Name "本地化头文件纳入工程" -Requirement "构建脚本必须纳入共享本地化头文件" -Text $pro -Needle "../../src/ui/UiLocalization.h" -Detail "$proPath 纳入 UiLocalization.h"

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
