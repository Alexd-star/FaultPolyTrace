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

$dialogCppPath = "geoeast_plugin\qt_launcher\FaultTraceDialog.cpp"
$dialogHeaderPath = "geoeast_plugin\qt_launcher\FaultTraceDialog.h"
$runnerMainPath = "geoeast_plugin\bo_runner\main.cpp"
$adapterCppPath = "geoeast_plugin\bo_runner\GeoEastBoAdapter.cpp"
$adapterHeaderPath = "geoeast_plugin\bo_runner\GeoEastBoAdapter.h"
$dialogCpp = Read-TextFile $dialogCppPath
$dialogHeader = Read-TextFile $dialogHeaderPath
$runnerMain = Read-TextFile $runnerMainPath
$adapterCpp = Read-TextFile $adapterCppPath
$adapterHeader = Read-TextFile $adapterHeaderPath

$rows = [System.Collections.Generic.List[object]]::new()

Add-ContainsCheck -Rows $rows -Group "result-management" -Name "delete-slot" -Requirement "Launcher provides a delete-output slot." -Text $dialogHeader -Needle "void deleteOutput()" -Detail "$dialogHeaderPath declares deleteOutput"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "delete-button-member" -Requirement "Left panel has a real delete button member." -Text $dialogHeader -Needle "QPushButton* deleteOutputButton_" -Detail "$dialogHeaderPath declares deleteOutputButton_"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "delete-menu-member" -Requirement "Menu action exists." -Text $dialogHeader -Needle "QAction* deleteOutputAction_" -Detail "$dialogHeaderPath declares deleteOutputAction_"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "delete-button-connect" -Requirement "Delete button connects to the slot." -Text $dialogCpp -Needle "connect(deleteOutputButton_, &QPushButton::clicked, this, &FaultTraceDialog::deleteOutput)" -Detail "$dialogCppPath connects deleteOutput"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "delete-confirmation" -Requirement "Delete output requires confirmation." -Text $dialogCpp -Needle "QMessageBox::warning" -Detail "$dialogCppPath includes confirmation dialog"
Add-ContainsCheck -Rows $rows -Group "result-management" -Name "delete-runner-arg" -Requirement "Launcher passes --delete-output to the runner." -Text $dialogCpp -Needle '"--delete-output"' -Detail "$dialogCppPath passes delete-output"

Add-ContainsCheck -Rows $rows -Group "runner-delete" -Name "parse-delete-output" -Requirement "bo_runner supports --delete-output." -Text $runnerMain -Needle 'arg == "--delete-output"' -Detail "$runnerMainPath parses delete-output"
Add-ContainsCheck -Rows $rows -Group "runner-delete" -Name "require-output-name" -Requirement "Deletion must specify an output object name." -Text $runnerMain -Needle "Missing --output for --delete-output" -Detail "$runnerMainPath validates output name"
Add-ContainsCheck -Rows $rows -Group "runner-delete" -Name "call-adapter" -Requirement "Runner calls the BO adapter deletion function." -Text $runnerMain -Needle "deleteFaultPolygons(project, options.outputName)" -Detail "$runnerMainPath calls adapter"
Add-ContainsCheck -Rows $rows -Group "runner-delete" -Name "adapter-declaration" -Requirement "Adapter declares deleteFaultPolygons." -Text $adapterHeader -Needle "void deleteFaultPolygons" -Detail "$adapterHeaderPath declares deleteFaultPolygons"
Add-ContainsCheck -Rows $rows -Group "runner-delete" -Name "adapter-implementation" -Requirement "Adapter deletes BOFaultPolygon." -Text $adapterCpp -Needle "eraseChild<geobo::BOFaultPolygon>" -Detail "$adapterCppPath calls eraseChild"

Add-ContainsCheck -Rows $rows -Group "display-control" -Name "result-display-group" -Requirement "Result/display functions are separated from data management." -Text $dialogCpp -Needle 'new QGroupBox(QString::fromUtf8("' -Detail "$dialogCppPath contains a QGroupBox result/display area"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "input-layer-checkbox" -Requirement "User can toggle input grid preview." -Text $dialogHeader -Needle "QCheckBox* showInputLayerCheck_" -Detail "$dialogHeaderPath declares showInputLayerCheck_"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "result-layer-checkbox" -Requirement "User can toggle result polygon preview." -Text $dialogHeader -Needle "QCheckBox* showResultLayerCheck_" -Detail "$dialogHeaderPath declares showResultLayerCheck_"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "view-menu-input-action" -Requirement "View menu can toggle input preview." -Text $dialogHeader -Needle "QAction* showInputLayerAction_" -Detail "$dialogHeaderPath declares showInputLayerAction_"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "view-menu-result-action" -Requirement "View menu can toggle result preview." -Text $dialogHeader -Needle "QAction* showResultLayerAction_" -Detail "$dialogHeaderPath declares showResultLayerAction_"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "visibility-sync" -Requirement "Layer visibility is synchronized." -Text $dialogCpp -Needle "void FaultTraceDialog::updateLayerVisibility()" -Detail "$dialogCppPath implements updateLayerVisibility"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "render-widget-toggle" -Requirement "Preview rendering uses layer visibility flags." -Text $dialogCpp -Needle "setLayerVisibility(bool showInputLayer, bool showResultLayer)" -Detail "$dialogCppPath RenderViewWidget supports layer visibility"
Add-ContainsCheck -Rows $rows -Group "display-control" -Name "geoeast-layer-tree-note" -Requirement "UI does not pretend to directly control GeoEast layers." -Text $dialogCpp -Needle "GeoEast" -Detail "$dialogCppPath contains GeoEast layer guidance"

Add-ContainsCheck -Rows $rows -Group "settings" -Name "save-input-layer" -Requirement "Input preview choice is persisted." -Text $dialogCpp -Needle 'settings.setValue("showInputLayer"' -Detail "$dialogCppPath saves showInputLayer"
Add-ContainsCheck -Rows $rows -Group "settings" -Name "save-result-layer" -Requirement "Result preview choice is persisted." -Text $dialogCpp -Needle 'settings.setValue("showResultLayer"' -Detail "$dialogCppPath saves showResultLayer"
Add-ContainsCheck -Rows $rows -Group "settings" -Name "load-input-layer" -Requirement "Input preview choice is restored." -Text $dialogCpp -Needle 'settings.value("showInputLayer"' -Detail "$dialogCppPath loads showInputLayer"
Add-ContainsCheck -Rows $rows -Group "settings" -Name "load-result-layer" -Requirement "Result preview choice is restored." -Text $dialogCpp -Needle 'settings.value("showResultLayer"' -Detail "$dialogCppPath loads showResultLayer"

$failed = @($rows | Where-Object { $_.Status -eq "FAIL" })
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# GeoEast Plugin UI Function Check Report") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("Time: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')") | Out-Null
$lines.Add("Repo: $repoRoot") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Summary") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Item | Count |") | Out-Null
$lines.Add("|---|---:|") | Out-Null
$lines.Add("| PASS | $($rows.Count - $failed.Count) |") | Out-Null
$lines.Add("| FAIL | $($failed.Count) |") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Details") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| Group | Item | Status | Requirement | Evidence |") | Out-Null
$lines.Add("|---|---|---|---|---|") | Out-Null
foreach ($row in $rows) {
    $lines.Add("| $(Escape-Md $row.Group) | $(Escape-Md $row.Name) | $($row.Status) | $(Escape-Md $row.Requirement) | $(Escape-Md $row.Detail) |") | Out-Null
}
$lines | Set-Content -LiteralPath $reportPath -Encoding UTF8

Write-Host "GeoEast plugin UI function report: $reportPath"
Write-Host "PASS: $($rows.Count - $failed.Count)  FAIL: $($failed.Count)"

if ($failed.Count -gt 0) {
    throw "GeoEast plugin UI function check failed."
}
