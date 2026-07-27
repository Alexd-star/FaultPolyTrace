[CmdletBinding()]
param([string]$CodeRoot = "")

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($CodeRoot)) { $CodeRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path }
$manifest = Join-Path $CodeRoot "SHA256SUMS.txt"
if (!(Test-Path -LiteralPath $manifest -PathType Leaf)) { throw "未找到哈希清单：$manifest" }

$failed = 0
foreach ($line in Get-Content -LiteralPath $manifest -Encoding UTF8) {
    if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith("#")) { continue }
    if ($line -notmatch '^([0-9A-Fa-f]{64}) [ *](.+)$') {
        Write-Host "INVALID $line"
        $failed++
        continue
    }
    $expected = $Matches[1].ToUpperInvariant()
    $relative = $Matches[2]
    $path = Join-Path $CodeRoot ($relative -replace "/", "\")
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { Write-Host "MISSING $relative"; $failed++; continue }
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if ($actual -ne $expected) { Write-Host "FAIL    $relative"; $failed++ }
}
if ($failed -gt 0) { throw "提交包哈希核验失败，共 $failed 项。" }
Write-Host "PASS：提交包全部文件哈希一致。"


