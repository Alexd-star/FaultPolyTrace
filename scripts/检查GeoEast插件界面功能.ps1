[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$reportDir = Join-Path $repoRoot "examples\outputs\geoeast_plugin_ui_function_$timestamp"
$reportPath = Join-Path $reportDir "GeoEast_plugin_ui_function_report.md"

function Read-TextFile {
    param([string]$RelativePath)
    $path = Join-Path $repoRoot $RelativePath
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing file: $path"
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
        Status = if ($Passed) { "PASS" } else { "FAIL" }
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

function Add-NotContainsCheck {
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
        -Passed (-not $Text.Contains($Needle)) -Detail $Detail
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

$dialogCppPath = "geoeast_plugin\qt_launcher\FaultTraceDialog.cpp"
$dialogHeaderPath = "geoeast_plugin\qt_launcher\FaultTraceDialog.h"
$launcherMainPath = "geoeast_plugin\qt_launcher\main.cpp"
$launcherProPath = "geoeast_plugin\qt_launcher\FaultPolyTraceLauncher.pro"
$deployScriptPath = "scripts\部署到GeoEast.ps1"
$runnerMainPath = "geoeast_plugin\bo_runner\main.cpp"
$adapterCppPath = "geoeast_plugin\bo_runner\GeoEastBoAdapter.cpp"
$adapterHeaderPath = "geoeast_plugin\bo_runner\GeoEastBoAdapter.h"
$dialogCpp = Read-TextFile $dialogCppPath
$dialogHeader = Read-TextFile $dialogHeaderPath
$launcherMain = Read-TextFile $launcherMainPath
$launcherPro = Read-TextFile $launcherProPath
$deployScript = Read-TextFile $deployScriptPath
$runnerMain = Read-TextFile $runnerMainPath
$adapterCpp = Read-TextFile $adapterCppPath
$adapterHeader = Read-TextFile $adapterHeaderPath

$rows = [System.Collections.Generic.List[object]]::new()

Add-ContainsCheck -Rows $rows -Group "branding" -Name "runtime-icon" -Requirement "Launcher window/taskbar uses the product icon." -Text $launcherMain -Needle "app.setWindowIcon(locateApplicationIcon())" -Detail "$launcherMainPath sets application icon"
Add-ContainsCheck -Rows $rows -Group "branding" -Name "exe-icon" -Requirement "Launcher exe embeds the product icon." -Text $launcherPro -Needle "RC_ICONS = ../../assets/icons/faultpolytrace.ico" -Detail "$launcherProPath sets RC_ICONS"
Add-ContainsCheck -Rows $rows -Group "branding" -Name "deploy-icon" -Requirement "GeoEast deployment copies icon files beside the launcher." -Text $deployScript -Needle "FaultPolyTraceLauncher" -Detail "$deployScriptPath copies launcher files and icons"
Add-ContainsCheck -Rows $rows -Group "branding" -Name "deploy-icon-file" -Requirement "GeoEast deployment includes faultpolytrace.ico." -Text $deployScript -Needle "faultpolytrace.ico" -Detail "$deployScriptPath copies faultpolytrace.ico"

Add-ContainsCheck -Rows $rows -Group "result-management" -Name "delete-slot" -Requirement "Launcher provides a delete-output slot." -Text $dialogHeader -Needle "void deleteOutput()" -Detail "$dialogHeaderPath declares deleteOutput"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "delete-button-member" -Requirement "Left panel has a real delete button member." -Text $dialogHeader -Needle "QPushButton* deleteOutputButton_" -Detail "$dialogHeaderPath declares deleteOutputButton_"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "delete-menu-member" -Requirement "Menu action exists." -Text $dialogHeader -Needle "QAction* deleteOutputAction_" -Detail "$dialogHeaderPath declares deleteOutputAction_"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "delete-button-connect" -Requirement "Delete button connects to the slot." -Text $dialogCpp -Needle "connect(deleteOutputButton_, &QPushButton::clicked, this, &FaultTraceDialog::deleteOutput)" -Detail "$dialogCppPath connects deleteOutput"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "delete-confirmation" -Requirement "Delete output requires confirmation." -Text $dialogCpp -Needle "QMessageBox::warning" -Detail "$dialogCppPath includes confirmation dialog"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "delete-runner-arg" -Requirement "Launcher passes --delete-output to the runner." -Text $dialogCpp -Needle '"--delete-output"' -Detail "$dialogCppPath passes delete-output"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "result-catalog-combo" -Requirement "Launcher provides a result catalog combo for existing BOFaultPolygon objects." -Text $dialogHeader -Needle "QComboBox* outputCatalogCombo_" -Detail "$dialogHeaderPath declares outputCatalogCombo_"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "refresh-output-slot" -Requirement "Launcher can refresh existing result objects." -Text $dialogHeader -Needle "void refreshOutputs()" -Detail "$dialogHeaderPath declares refreshOutputs"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "select-output-slot" -Requirement "Launcher can select an existing result as the current output object." -Text $dialogHeader -Needle "void loadSelectedOutput()" -Detail "$dialogHeaderPath declares loadSelectedOutput"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "list-fault-polygons-arg" -Requirement "Launcher uses runner support to list BOFaultPolygon objects." -Text $dialogCpp -Needle '"--list-fault-polygons"' -Detail "$dialogCppPath passes list-fault-polygons"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "export-fault-polygons-arg" -Requirement "Launcher can export an existing BOFaultPolygon to a local CSV view file." -Text $dialogCpp -Needle '"--export-fault-polygons-output"' -Detail "$dialogCppPath passes export-fault-polygons-output"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "write-db-log" -Requirement "Launcher logs the BOFaultPolygon object written to GeoEast." -Text $dialogCpp -Needle "GeoEast 数据库写入对象：BOFaultPolygon" -Detail "$dialogCppPath logs written object"

Add-ContainsCheck -Rows $rows -Group "runner-delete" -Name "parse-delete-output" -Requirement "bo_runner supports --delete-output." -Text $runnerMain -Needle 'arg == "--delete-output"' -Detail "$runnerMainPath parses delete-output"
Add-ContainsCheck -Rows $rows -Group "runner-delete" -Name "require-output-name" -Requirement "Deletion must specify an output object name." -Text $runnerMain -Needle "Missing --output for --delete-output" -Detail "$runnerMainPath validates output name"
Add-ContainsCheck -Rows $rows -Group "runner-delete" -Name "call-adapter" -Requirement "Runner calls the BO adapter deletion function." -Text $runnerMain -Needle "deleteFaultPolygons(project, options.outputName)" -Detail "$runnerMainPath calls adapter"
Add-ContainsCheck -Rows $rows -Group "runner-delete" -Name "adapter-declaration" -Requirement "Adapter declares deleteFaultPolygons." -Text $adapterHeader -Needle "void deleteFaultPolygons" -Detail "$adapterHeaderPath declares deleteFaultPolygons"
Add-ContainsCheck -Rows $rows -Group "runner-delete" -Name "adapter-implementation" -Requirement "Adapter deletes BOFaultPolygon." -Text $adapterCpp -Needle "eraseChild<geobo::BOFaultPolygon>" -Detail "$adapterCppPath calls eraseChild"
Add-ContainsCheck -Rows $rows -Group "runner-list-results" -Name "parse-list-fault-polygons" -Requirement "bo_runner supports --list-fault-polygons." -Text $runnerMain -Needle 'arg == "--list-fault-polygons"' -Detail "$runnerMainPath parses list-fault-polygons"
Add-ContainsCheck -Rows $rows -Group "runner-list-results" -Name "call-list-adapter" -Requirement "Runner calls the BO adapter result-list function." -Text $runnerMain -Needle "listFaultPolygons(project)" -Detail "$runnerMainPath calls listFaultPolygons"
Add-ContainsCheck -Rows $rows -Group "runner-list-results" -Name "adapter-list-declaration" -Requirement "Adapter declares listFaultPolygons." -Text $adapterHeader -Needle "std::vector<std::string> listFaultPolygons" -Detail "$adapterHeaderPath declares listFaultPolygons"
Add-ContainsCheck -Rows $rows -Group "runner-list-results" -Name "adapter-list-implementation" -Requirement "Adapter lists BOFaultPolygon children." -Text $adapterCpp -Needle "listNameIDsOfChildren<geobo::BOFaultPolygon>" -Detail "$adapterCppPath lists BOFaultPolygon"
Add-ContainsCheck -Rows $rows -Group "runner-list-results" -Name "parse-export-fault-polygons" -Requirement "bo_runner supports exporting an existing BOFaultPolygon." -Text $runnerMain -Needle 'arg == "--export-fault-polygons-output"' -Detail "$runnerMainPath parses export-fault-polygons-output"
Add-ContainsCheck -Rows $rows -Group "runner-list-results" -Name "adapter-read-declaration" -Requirement "Adapter declares readFaultPolygons." -Text $adapterHeader -Needle "std::vector<fpt::Polygon> readFaultPolygons" -Detail "$adapterHeaderPath declares readFaultPolygons"
Add-ContainsCheck -Rows $rows -Group "runner-list-results" -Name "adapter-read-implementation" -Requirement "Adapter reads BOFaultPolygon geometry from GeoEast." -Text $adapterCpp -Needle "boPolygon->readData(head, data)" -Detail "$adapterCppPath reads BOFaultPolygon"

Add-ContainsCheck -Rows $rows -Group "display-control" -Name "result-display-group" -Requirement "Result/view functions are separated from data management." -Text $dialogHeader -Needle "QPushButton* loadResultViewButton_" -Detail "$dialogHeaderPath declares loadResultViewButton_"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "input-layer-checkbox" -Requirement "User can toggle actual input grid layer in the launcher view." -Text $dialogHeader -Needle "QCheckBox* showInputLayerCheck_" -Detail "$dialogHeaderPath declares showInputLayerCheck_"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "result-layer-checkbox" -Requirement "User can toggle actual result polygon layer in the launcher view." -Text $dialogHeader -Needle "QCheckBox* showResultLayerCheck_" -Detail "$dialogHeaderPath declares showResultLayerCheck_"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "view-menu-input-action" -Requirement "View menu can toggle actual input grid layer." -Text $dialogHeader -Needle "QAction* showInputLayerAction_" -Detail "$dialogHeaderPath declares showInputLayerAction_"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "view-menu-result-action" -Requirement "View menu can toggle actual result polygon layer." -Text $dialogHeader -Needle "QAction* showResultLayerAction_" -Detail "$dialogHeaderPath declares showResultLayerAction_"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "visibility-sync" -Requirement "Layer visibility is synchronized." -Text $dialogCpp -Needle "void FaultTraceDialog::updateLayerVisibility()" -Detail "$dialogCppPath implements updateLayerVisibility"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "render-widget-toggle" -Requirement "Real rendering uses layer visibility flags." -Text $dialogCpp -Needle "setLayerVisibility(bool showInputLayer, bool showResultLayer)" -Detail "$dialogCppPath RenderViewWidget supports layer visibility"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "geoeast-layer-tree-note" -Requirement "UI does not pretend to directly control GeoEast layers." -Text $dialogCpp -Needle "GeoEast" -Detail "$dialogCppPath contains GeoEast layer guidance"

Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "restore-default-slot" -Requirement "GeoEast launcher must provide a restore-defaults slot." -Text $dialogHeader -Needle "void restoreDefaults()" -Detail "$dialogHeaderPath declares restoreDefaults"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "restore-default-action" -Requirement "Restore defaults must be visible as a real toolbar/menu action." -Text $dialogHeader -Needle "QAction* restoreDefaultsAction_" -Detail "$dialogHeaderPath declares restoreDefaultsAction_"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "restore-default-connect" -Requirement "Restore defaults action must call the restore slot." -Text $dialogCpp -Needle "FaultTraceDialog::restoreDefaults" -Detail "$dialogCppPath connects restoreDefaults"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "toolbar-refresh-action" -Requirement "Toolbar refresh uses a QAction so the left panel button is not moved out of the toolbar." -Text $dialogCpp -Needle "toolbar->addAction(refreshGridAction_)" -Detail "$dialogCppPath adds refresh action to toolbar"
Add-NotContainsCheck -Rows $rows -Group "workflow-usability" -Name "no-toolbar-widget-refresh" -Requirement "Toolbar must not reuse the same refresh QPushButton that is also placed in the left panel." -Text $dialogCpp -Needle "toolbar->addWidget(refreshButton_)" -Detail "$dialogCppPath must not add refreshButton_ as toolbar widget"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "refresh-clear-label" -Requirement "Left panel refresh button should clearly refresh the GeoEast grid list." -Text $dialogCpp -Needle "刷新网格列表" -Detail "$dialogCppPath uses clear refresh label"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "geoeast-config-guard" -Requirement "GeoEast operations validate required configuration before calling the execution program." -Text $dialogHeader -Needle "bool ensureGeoEastConfigReady" -Detail "$dialogHeaderPath declares ensureGeoEastConfigReady"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "geoeast-config-missing-list" -Requirement "Configuration guard reports exactly which fields are missing." -Text $dialogHeader -Needle "QStringList missingGeoEastConfig" -Detail "$dialogHeaderPath declares missingGeoEastConfig"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "refresh-uses-guard" -Requirement "Refreshing grids must be blocked with a clear prompt when database config is incomplete." -Text $dialogCpp -Needle 'ensureGeoEastConfigReady(QString::fromUtf8("刷新网格列表")' -Detail "$dialogCppPath guards refreshGrids"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "guided-catalog-combo" -Requirement "Empty catalog combo boxes must intercept popup attempts and provide guided actions." -Text $dialogCpp -Needle "class GuidedCatalogComboBox" -Detail "$dialogCppPath defines a guided catalog combo"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "empty-grid-popup-guide" -Requirement "Clicking an empty input-grid combo must validate configuration and guide the user instead of opening a blank popup." -Text $dialogCpp -Needle "选择输入网格" -Detail "$dialogCppPath handles an empty grid popup"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "empty-result-popup-guide" -Requirement "Clicking an empty result combo must validate configuration and guide the user instead of doing nothing." -Text $dialogCpp -Needle "选择已有结果" -Detail "$dialogCppPath handles an empty result popup"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "empty-catalog-auto-refresh" -Requirement "When configuration is complete, an empty catalog popup should start the corresponding refresh automatically." -Text $dialogCpp -Needle "列表为空，正在自动刷新" -Detail "$dialogCppPath reports and starts automatic refresh"
Add-Check -Rows $rows -Group "workflow-usability" -Name "empty-result-combo-remains-clickable" -Requirement "An empty result combo must remain enabled so clicking it can show guidance." -Passed (-not (Get-Section -Text $dialogCpp -StartNeedle "if (refreshingOutputs_ && exitCode == 0" -EndNeedle "if (refreshingOutputs_)").Contains("outputCatalogCombo_->setEnabled(false)")) -Detail "$dialogCppPath does not disable the empty output catalog"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "adjustable-main-splitter" -Requirement "The sidebar/workspace boundary must be draggable." -Text $dialogHeader -Needle "QSplitter* mainSplitter_" -Detail "$dialogHeaderPath declares a persistent main splitter"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "splitter-prevents-collapse" -Requirement "Dragging the boundary must not collapse either functional region completely." -Text $dialogCpp -Needle "mainSplitter_->setChildrenCollapsible(false)" -Detail "$dialogCppPath keeps both regions usable"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "splitter-state-saved" -Requirement "The adjusted sidebar width must be restored next time." -Text $dialogCpp -Needle 'settings.setValue("mainSplitterState"' -Detail "$dialogCppPath saves splitter state"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "splitter-state-loaded" -Requirement "The saved sidebar width must be restored on startup." -Text $dialogCpp -Needle 'settings.value("mainSplitterState")' -Detail "$dialogCppPath restores splitter state"
Add-Check -Rows $rows -Group "workflow-usability" -Name "sidebar-not-fixed-width" -Requirement "The sidebar must not use a fixed width when a draggable splitter is provided." -Passed (-not $dialogCpp.Contains("sidebar->setFixedWidth(360)")) -Detail "$dialogCppPath removes the fixed sidebar width"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "window-minmax-buttons" -Requirement "Launcher title bar must provide minimize and maximize buttons." -Text $dialogCpp -Needle "Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint" -Detail "$dialogCppPath sets min/max/close buttons"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "no-titlebar-help-question" -Requirement "Launcher title bar must not show the Windows context-help question mark." -Text $dialogCpp -Needle "~Qt::WindowContextHelpButtonHint" -Detail "$dialogCppPath clears context help button"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "config-dialog-help" -Requirement "GeoEast database configuration dialog provides field-level help." -Text $dialogCpp -Needle "GeoEast 配置帮助" -Detail "$dialogCppPath provides GeoEast config help"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "config-dialog-auto-detect" -Requirement "GeoEast database configuration dialog can auto-detect paths and defaults." -Text $dialogCpp -Needle "自动检测" -Detail "$dialogCppPath provides auto-detect button"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "config-dialog-check" -Requirement "GeoEast database configuration dialog can validate values before use." -Text $dialogCpp -Needle "检查配置" -Detail "$dialogCppPath provides config check button"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "config-validator" -Requirement "Configuration validation is centralized instead of relying on late runner errors." -Text $dialogCpp -Needle "validateGeoEastConfigValues" -Detail "$dialogCppPath contains config validator"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "database-auto-detect" -Requirement "Database name can be detected from environment or pgdata." -Text $dialogCpp -Needle "detectGeoEastDatabaseName" -Detail "$dialogCppPath detects database name"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "project-boundary-help" -Requirement "Project name must be confirmed from the current GeoEast project, not guessed blindly." -Text $dialogCpp -Needle "项目名无法安全推断" -Detail "$dialogCppPath explains project-name boundary"
Add-Check -Rows $rows -Group "workflow-usability" -Name "database-config-no-maximize" -Requirement "Database config is a small modal dialog and should not provide maximize." -Passed (-not (($dialogCpp.Substring($dialogCpp.IndexOf("void FaultTraceDialog::showDatabaseConfigDialog()"), $dialogCpp.IndexOf("void FaultTraceDialog::saveCurrentPreset()") - $dialogCpp.IndexOf("void FaultTraceDialog::showDatabaseConfigDialog()"))).Contains("Qt::WindowMaximizeButtonHint |"))) -Detail "$dialogCppPath database config dialog does not add maximize button"
Add-Check -Rows $rows -Group "workflow-usability" -Name "database-config-no-inline-long-help" -Requirement "Database config is a dense form; source explanations belong in the Help button, not inline multi-line labels that can overlap fields." -Passed (-not (($dialogCpp.Substring($dialogCpp.IndexOf("void FaultTraceDialog::showDatabaseConfigDialog()"), $dialogCpp.IndexOf("void FaultTraceDialog::saveCurrentPreset()") - $dialogCpp.IndexOf("void FaultTraceDialog::showDatabaseConfigDialog()"))).Contains("fieldHint")) -and -not (($dialogCpp.Substring($dialogCpp.IndexOf("void FaultTraceDialog::showDatabaseConfigDialog()"), $dialogCpp.IndexOf("void FaultTraceDialog::saveCurrentPreset()") - $dialogCpp.IndexOf("void FaultTraceDialog::showDatabaseConfigDialog()"))).Contains("addHint"))) -Detail "$dialogCppPath database config dialog has no fieldHint/addHint"
Add-NotContainsCheck -Rows $rows -Group "workflow-usability" -Name "no-toolbar-load-result-view" -Requirement "Toolbar should contain primary workflow actions only; result reload belongs to result/menu area." -Text $dialogCpp -Needle "toolbar->addAction(loadResultViewAction_)" -Detail "$dialogCppPath must not add loadResultViewAction_ to toolbar"
Add-NotContainsCheck -Rows $rows -Group "workflow-usability" -Name "no-toolbar-help-action" -Requirement "Toolbar should not be crowded by help; help remains in the Help menu." -Text $dialogCpp -Needle "toolbar->addAction(helpAction)" -Detail "$dialogCppPath must not add helpAction to toolbar"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "short-toolbar-open-label" -Requirement "Toolbar labels should stay short enough to avoid ellipsis at normal window width." -Text $dialogCpp -Needle 'openAction->setText(QString::fromUtf8("打开DAT"))' -Detail "$dialogCppPath shortens toolbar/menu action label"
Add-ContainsCheck -Rows $rows -Group "workflow-usability" -Name "reload-result-user-wording" -Requirement "Cached result view reload must use user-facing wording instead of implementation jargon." -Text $dialogCpp -Needle "重新载入结果视图" -Detail "$dialogCppPath uses user-facing wording"
Add-NotContainsCheck -Rows $rows -Group "workflow-usability" -Name "no-calculation-engine-term" -Requirement "User-facing wording uses 执行程序, not 计算引擎." -Text $dialogCpp -Needle "计算引擎" -Detail "$dialogCppPath should not contain 计算引擎"
Add-NotContainsCheck -Rows $rows -Group "workflow-usability" -Name "no-real-view-term" -Requirement "User-facing result wording must not expose 真实视图 implementation jargon." -Text $dialogCpp -Needle "真实视图" -Detail "$dialogCppPath should not contain 真实视图"
Add-NotContainsCheck -Rows $rows -Group "workflow-usability" -Name "no-load-real-view-term" -Requirement "User-facing result wording must not say 载入真实." -Text $dialogCpp -Needle "载入真实" -Detail "$dialogCppPath should not contain 载入真实"
Add-ContainsCheck -Rows $rows -Group "real-view" -Name "load-result-view-slot" -Requirement "Launcher can load actual exported grid and polygon files." -Text $dialogHeader -Needle "void loadResultView()" -Detail "$dialogHeaderPath declares loadResultView"
Add-ContainsCheck -Rows $rows -Group "real-view" -Name "export-view-slot" -Requirement "Launcher can export the actual current view as PNG." -Text $dialogHeader -Needle "void exportResultViewPng()" -Detail "$dialogHeaderPath declares exportResultViewPng"
Add-ContainsCheck -Rows $rows -Group "real-view" -Name "render-load-grid" -Requirement "RenderViewWidget reads actual grid export." -Text $dialogCpp -Needle "loadGridFile(const QString& path" -Detail "$dialogCppPath implements loadGridFile"
Add-ContainsCheck -Rows $rows -Group "real-view" -Name "render-load-polygons" -Requirement "RenderViewWidget reads actual polygon CSV export." -Text $dialogCpp -Needle "loadPolygonCsv(const QString& path" -Detail "$dialogCppPath implements loadPolygonCsv"
Add-ContainsCheck -Rows $rows -Group "real-view" -Name "runner-view-grid-arg" -Requirement "Launcher asks runner to export an actual grid view file." -Text $dialogCpp -Needle '"--view-grid-output"' -Detail "$dialogCppPath passes --view-grid-output"
Add-ContainsCheck -Rows $rows -Group "real-view" -Name "runner-view-polygon-arg" -Requirement "Launcher asks runner to export an actual polygon view file." -Text $dialogCpp -Needle '"--view-polygons-output"' -Detail "$dialogCppPath passes --view-polygons-output"
Add-ContainsCheck -Rows $rows -Group "real-view" -Name "runner-supports-view-grid" -Requirement "Runner supports actual grid view export." -Text $runnerMain -Needle 'arg == "--preview-grid-output" || arg == "--view-grid-output"' -Detail "$runnerMainPath parses --view-grid-output"
Add-ContainsCheck -Rows $rows -Group "real-view" -Name "runner-supports-view-polygons" -Requirement "Runner supports actual polygon view export." -Text $runnerMain -Needle 'arg == "--preview-polygons-output" || arg == "--view-polygons-output"' -Detail "$runnerMainPath parses --view-polygons-output"
Add-ContainsCheck -Rows $rows -Group "official-view" -Name "open-basemap-slot" -Requirement "Launcher can open GeoEast GeoBasemap for official layer-tree viewing." -Text $dialogHeader -Needle "void openGeoEastBasemap()" -Detail "$dialogHeaderPath declares openGeoEastBasemap"
Add-ContainsCheck -Rows $rows -Group "official-view" -Name "open-mapping-slot" -Requirement "Launcher can open GeoEast GeoMapping for official layer-tree viewing." -Text $dialogHeader -Needle "void openGeoEastMapping()" -Detail "$dialogHeaderPath declares openGeoEastMapping"
Add-ContainsCheck -Rows $rows -Group "official-view" -Name "launch-geoeast-app" -Requirement "Launcher starts official GeoEast applications with configured environment." -Text $dialogCpp -Needle "launchGeoEastApp" -Detail "$dialogCppPath implements launchGeoEastApp"
Add-NotContainsCheck -Rows $rows -Group "anti-fake-preview" -Name "no-old-fake-belt-gradient" -Requirement "Old fake colored belt drawing must not remain in launcher UI." -Text $dialogCpp -Needle "beltGradient" -Detail "$dialogCppPath should not contain old fake belt gradient"
Add-NotContainsCheck -Rows $rows -Group "anti-fake-preview" -Name "no-old-fake-style-preview" -Requirement "Old fixed style preview drawing must not remain in launcher UI." -Text $dialogCpp -Needle "style preview" -Detail "$dialogCppPath should not contain old fixed style preview"

Add-ContainsCheck -Rows $rows -Group "settings" -Name "save-input-layer" -Requirement "Input preview choice is persisted." -Text $dialogCpp -Needle 'settings.setValue("showInputLayer"' -Detail "$dialogCppPath saves showInputLayer"
Add-ContainsCheck -Rows $rows -Group "settings" -Name "save-result-layer" -Requirement "Result preview choice is persisted." -Text $dialogCpp -Needle 'settings.setValue("showResultLayer"' -Detail "$dialogCppPath saves showResultLayer"
Add-ContainsCheck -Rows $rows -Group "settings" -Name "load-input-layer" -Requirement "Input preview choice is restored." -Text $dialogCpp -Needle 'settings.value("showInputLayer"' -Detail "$dialogCppPath loads showInputLayer"
Add-ContainsCheck -Rows $rows -Group "settings" -Name "load-result-layer" -Requirement "Result preview choice is restored." -Text $dialogCpp -Needle 'settings.value("showResultLayer"' -Detail "$dialogCppPath loads showResultLayer"

Add-ContainsCheck -Rows $rows -Group "language" -Name "language-enum" -Requirement "Launcher exposes a product language state." -Text $dialogHeader -Needle "enum class UiLanguage" -Detail "$dialogHeaderPath declares UiLanguage"
Add-ContainsCheck -Rows $rows -Group "language" -Name "language-slot" -Requirement "Launcher can switch language at runtime." -Text $dialogHeader -Needle "void setLanguage" -Detail "$dialogHeaderPath declares setLanguage"
Add-ContainsCheck -Rows $rows -Group "language" -Name "language-retranslate" -Requirement "Launcher refreshes all visible UI text after language changes." -Text $dialogHeader -Needle "void retranslateUi()" -Detail "$dialogHeaderPath declares retranslateUi"
Add-ContainsCheck -Rows $rows -Group "language" -Name "language-menu" -Requirement "Menu bar contains a language menu." -Text $dialogCpp -Needle 'menuBar->addMenu(uiText("语言(L)"))' -Detail "$dialogCppPath adds language menu"
Add-ContainsCheck -Rows $rows -Group "language" -Name "language-english-action" -Requirement "Language menu provides English." -Text $dialogCpp -Needle 'languageEnglishAction_' -Detail "$dialogCppPath wires English action"
Add-ContainsCheck -Rows $rows -Group "language" -Name "language-chinese-action" -Requirement "Language menu provides Simplified Chinese." -Text $dialogCpp -Needle 'languageChineseAction_' -Detail "$dialogCppPath wires Chinese action"
Add-ContainsCheck -Rows $rows -Group "language" -Name "save-language" -Requirement "Selected language is persisted." -Text $dialogCpp -Needle 'settings.setValue("language"' -Detail "$dialogCppPath saves language"
Add-ContainsCheck -Rows $rows -Group "language" -Name "load-language" -Requirement "Selected language is restored." -Text $dialogCpp -Needle 'settings.value("language"' -Detail "$dialogCppPath loads language"
Add-ContainsCheck -Rows $rows -Group "language" -Name "language-render-view" -Requirement "Result view canvas uses the selected language." -Text $dialogCpp -Needle "setLanguage(currentLanguage_)" -Detail "$dialogCppPath passes language to render view"
Add-ContainsCheck -Rows $rows -Group "language" -Name "shared-localization" -Requirement "Launcher reuses the shared localization module instead of scattered translations." -Text $dialogCpp -Needle '#include "UiLocalization.h"' -Detail "$dialogCppPath includes UiLocalization"
Add-ContainsCheck -Rows $rows -Group "language" -Name "compile-localization-source" -Requirement "Launcher build includes the shared localization implementation." -Text $launcherPro -Needle "../../src/ui/UiLocalization.cpp" -Detail "$launcherProPath compiles UiLocalization.cpp"
Add-ContainsCheck -Rows $rows -Group "language" -Name "include-localization-header" -Requirement "Launcher project includes the shared localization header." -Text $launcherPro -Needle "../../src/ui/UiLocalization.h" -Detail "$launcherProPath includes UiLocalization.h"
Add-ContainsCheck -Rows $rows -Group "language" -Name "translate-result-refresh" -Requirement "The result refresh button must be translated in English mode." -Text $dialogCpp -Needle 'uiText(QString::fromUtf8("刷新结果"))' -Detail "$dialogCppPath explicitly refreshes the dynamic button label"
Add-ContainsCheck -Rows $rows -Group "language" -Name "translate-result-select" -Requirement "The select-current-result button must be translated in English mode." -Text $dialogCpp -Needle 'uiText(QString::fromUtf8("选为当前"))' -Detail "$dialogCppPath explicitly refreshes the dynamic button label"
Add-ContainsCheck -Rows $rows -Group "language" -Name "translate-render-guidance" -Requirement "Dynamic render guidance must have an English mapping." -Text $dialogCpp -Needle 'uiText(QString::fromUtf8("结果查看：追踪完成后自动显示原始网格和结果多边形；如需复查，可在左侧重新载入结果视图。"))' -Detail "$dialogCppPath translates render guidance"
Add-ContainsCheck -Rows $rows -Group "language" -Name "translate-colorbar" -Requirement "The color bar title must follow the selected language." -Text $dialogCpp -Needle "static_cast<ColorBarWidget*>(colorBarWidget_)->setLanguage(currentLanguage_)" -Detail "$dialogCppPath updates the color bar language"

Add-ContainsCheck -Rows $rows -Group "help-docs" -Name "portable-docs-deploy-source" -Requirement "GeoEast deployment must define the repository docs source." -Text $deployScript -Needle '$docsSource = Join-Path $repoRoot "docs"' -Detail "$deployScriptPath locates docs"
Add-ContainsCheck -Rows $rows -Group "help-docs" -Name "portable-docs-deploy-target" -Requirement "GeoEast deployment must copy docs into the portable launcher directory." -Text $deployScript -Needle 'Join-Path $launcherTargetDir "docs"' -Detail "$deployScriptPath copies docs"
Add-ContainsCheck -Rows $rows -Group "help-docs" -Name "relative-doc-locator" -Requirement "Launcher must locate documents relative to source, release package, or GeoEast portable launcher directory." -Text $dialogCpp -Needle "locateBundledPath" -Detail "$dialogCppPath uses locateBundledPath"
Add-ContainsCheck -Rows $rows -Group "help-docs" -Name "project-structure-doc-link" -Requirement "Launcher help menu should expose project directory structure documentation." -Text $dialogCpp -Needle "docs/项目目录结构与文件说明.md" -Detail "$dialogCppPath links project structure doc"

$failed = @($rows | Where-Object { $_.Status -eq "FAIL" })
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
$passCount = $rows.Count - $failed.Count
$failCount = $failed.Count
$lines = @(
    '# GeoEast Plugin UI Function Check Report'
    ''
    ('Time: {0}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
    ('Repo: {0}' -f $repoRoot)
    ''
    '## Summary'
    ''
    '| Item | Count |'
    '|---|---:|'
    ('| PASS | {0} |' -f $passCount)
    ('| FAIL | {0} |' -f $failCount)
    ''
    '## Details'
    ''
    '| Group | Item | Status | Requirement | Evidence |'
    '|---|---|---|---|---|'
)
foreach ($row in $rows) {
    $lines += ('| {0} | {1} | {2} | {3} | {4} |' -f (Escape-Md $row.Group), (Escape-Md $row.Name), $row.Status, (Escape-Md $row.Requirement), (Escape-Md $row.Detail))
}
$lines | Set-Content -LiteralPath $reportPath -Encoding UTF8

Write-Host ('GeoEast plugin UI function report: {0}' -f $reportPath)
Write-Host ('PASS: {0}  FAIL: {1}' -f $passCount, $failCount)

if ($failed.Count -gt 0) {
    throw 'GeoEast plugin UI function check failed.'
}
