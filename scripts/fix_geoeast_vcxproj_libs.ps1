$ErrorActionPreference = "Stop"

$projectPath = Join-Path $PSScriptRoot "..\geoeast_plugin\bo_runner\FaultPolyTraceGeoEast.vcxproj"
$projectPath = (Resolve-Path -LiteralPath $projectPath).Path

$text = Get-Content -LiteralPath $projectPath -Raw
$updated = $text.Replace("bdpthreadlck.lib", '$(IECO)\libso\ndp\dpthreadlck.lib')

if ($updated -ne $text) {
    Set-Content -LiteralPath $projectPath -Value $updated -Encoding UTF8
    Write-Host "Updated stale bdpthreadlck.lib reference in $projectPath"
} else {
    Write-Host "No stale bdpthreadlck.lib reference found in $projectPath"
}
