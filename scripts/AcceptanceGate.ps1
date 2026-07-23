function Invoke-AcceptedDelivery {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Checks,

        [Parameter(Mandatory = $true)]
        [string]$ReportPath,

        [scriptblock]$DeployAction,

        [scriptblock]$PackageAction
    )

    $failedChecks = @($Checks | Where-Object { -not $_.Pass })
    if ($failedChecks.Count -gt 0) {
        $failedText = ($failedChecks | ForEach-Object {
            "$($_.Item): $($_.Detail)"
        }) -join "；"
        throw "一键总验收未通过：$failedText。已阻止 GeoEast 部署和发布包生成。详见 $ReportPath"
    }

    if ($DeployAction) {
        & $DeployAction
    }
    if ($PackageAction) {
        & $PackageAction
    }
}
