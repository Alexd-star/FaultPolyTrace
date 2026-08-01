#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SubmissionRoot,
    [Parameter(Mandatory = $true)][string]$EvaluationPackageDir,
    [string]$FinalResultSupplementDir = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$submission = (Resolve-Path -LiteralPath $SubmissionRoot).Path
$evaluation = (Resolve-Path -LiteralPath $EvaluationPackageDir).Path
foreach ($sentinel in @("00_提交包说明.md", "唯一构建清单.json", "03_GeoEast插件", "04_复现与验证")) {
    if (!(Test-Path -LiteralPath (Join-Path $submission $sentinel))) {
        throw "目标不是可识别的 FaultPolyTrace 正式代码提交根目录，缺少：$sentinel"
    }
}
foreach ($sentinel in @("FaultPolyTrace.exe", "pyalgo_faultpolytrace\faultpolytrace_launcher.exe", "pyalgo_faultpolytrace\faultpolytrace_geoeast_runner.exe")) {
    if (!(Test-Path -LiteralPath (Join-Path $evaluation $sentinel) -PathType Leaf)) {
        throw "评测运行包缺少：$sentinel"
    }
}

$identity = Get-Content -LiteralPath (Join-Path $submission "唯一构建清单.json") -Raw -Encoding UTF8 | ConvertFrom-Json
$version = [string]$identity.version
$plainVersion = $version.TrimStart('v')
$templateRoot = Join-Path $repoRoot "packaging\submission"

function Copy-ExpandedTemplate {
    param([string]$Source, [string]$Destination)
    $text = [System.IO.File]::ReadAllText($Source, [System.Text.Encoding]::UTF8)
    $text = $text.Replace("{{VERSION}}", $version).
        Replace("{{PLAIN_VERSION}}", $plainVersion).
        Replace("{{BUILD_ID}}", [string]$identity.buildId).
        Replace("{{GIT_HEAD}}", [string]$identity.gitHead)
    $parent = Split-Path -Parent $Destination
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    [System.IO.File]::WriteAllText($Destination, $text, (New-Object System.Text.UTF8Encoding($false)))
}

Copy-ExpandedTemplate (Join-Path $templateRoot "00_提交包说明.md") (Join-Path $submission "00_提交包说明.md")
Copy-ExpandedTemplate (Join-Path $templateRoot "代码提交目录清单.md") (Join-Path $submission "代码提交目录清单.md")
Copy-ExpandedTemplate `
    (Join-Path $templateRoot "03_GeoEast插件\GeoEast插件安装部署与使用说明.md") `
    (Join-Path $submission "03_GeoEast插件\GeoEast插件安装部署与使用说明.md")
Copy-ExpandedTemplate `
    (Join-Path $templateRoot "04_复现与验证\官方数据复现与验证说明.md") `
    (Join-Path $submission "04_复现与验证\官方数据复现与验证说明.md")
Copy-Item -LiteralPath (Join-Path $templateRoot "04_复现与验证\参数.txt") -Destination (Join-Path $submission "04_复现与验证\参数.txt") -Force
Copy-Item -LiteralPath (Join-Path $templateRoot "04_复现与验证\最终构建与评测部署勘误说明.md") -Destination (Join-Path $submission "04_复现与验证\最终构建与评测部署勘误说明.md") -Force
Copy-Item -LiteralPath (Join-Path $templateRoot "04_复现与验证\必须由队长完成的GeoEast操作.txt") -Destination (Join-Path $submission "04_复现与验证\必须由队长完成的GeoEast操作.txt") -Force

$pluginPackage = Get-ChildItem -LiteralPath (Join-Path $submission "03_GeoEast插件") -Directory -Filter "FaultPolyTrace_GeoEastPlugin_*" |
    Sort-Object Name -Descending | Select-Object -First 1
if (!$pluginPackage) { throw "未找到展开的 GeoEast 插件运行包。" }
Copy-Item -LiteralPath (Join-Path $repoRoot "geoeast_plugin\bo_runner\说明.md") -Destination (Join-Path $pluginPackage.FullName "BO_Runner\说明.md") -Force

$pluginZip = Join-Path $pluginPackage.Parent.FullName ($pluginPackage.Name + ".zip")
$temporaryPluginZip = Join-Path $pluginPackage.Parent.FullName ($pluginPackage.Name + ".tmp.zip")
if (Test-Path -LiteralPath $temporaryPluginZip) { Remove-Item -LiteralPath $temporaryPluginZip -Force }
Compress-Archive -Path (Join-Path $pluginPackage.FullName "*") -DestinationPath $temporaryPluginZip -CompressionLevel Optimal
if (Test-Path -LiteralPath $pluginZip) { Remove-Item -LiteralPath $pluginZip -Force }
Move-Item -LiteralPath $temporaryPluginZip -Destination $pluginZip

$evaluationParent = Join-Path $submission "06_评测运行程序"
New-Item -ItemType Directory -Force -Path $evaluationParent | Out-Null
$evaluationName = Split-Path -Leaf $evaluation
$evaluationTarget = Join-Path $evaluationParent $evaluationName
if (Test-Path -LiteralPath $evaluationTarget) {
    $resolvedTarget = [System.IO.Path]::GetFullPath($evaluationTarget)
    $resolvedParent = [System.IO.Path]::GetFullPath($evaluationParent) + [System.IO.Path]::DirectorySeparatorChar
    if (!$resolvedTarget.StartsWith($resolvedParent, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "拒绝清理评测运行目录之外的路径：$resolvedTarget"
    }
    Remove-Item -LiteralPath $evaluationTarget -Recurse -Force
}
Copy-Item -LiteralPath $evaluation -Destination $evaluationTarget -Recurse -Force
$evaluationZip = "$evaluation.zip"
if (!(Test-Path -LiteralPath $evaluationZip -PathType Leaf)) { throw "缺少评测运行 ZIP：$evaluationZip" }
Copy-Item -LiteralPath $evaluationZip -Destination (Join-Path $evaluationParent (Split-Path -Leaf $evaluationZip)) -Force

$sourceTarget = Join-Path $evaluationParent "启动器源码与构建验收脚本"
New-Item -ItemType Directory -Force -Path $sourceTarget | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "geoeast_plugin\evaluation_bootstrap\main.cpp") -Destination $sourceTarget -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "geoeast_plugin\evaluation_bootstrap\FaultPolyTraceEvaluation.rc") -Destination $sourceTarget -Force
foreach ($scriptName in @("build_evaluation_bootstrap.ps1", "build_evaluation_package.ps1", "test_evaluation_package.ps1")) {
    Copy-Item -LiteralPath (Join-Path $repoRoot "scripts\$scriptName") -Destination $sourceTarget -Force
}

if (![string]::IsNullOrWhiteSpace($FinalResultSupplementDir)) {
    New-Item -ItemType Directory -Force -Path $FinalResultSupplementDir | Out-Null
    Copy-Item -LiteralPath (Join-Path $templateRoot "04_复现与验证\参数.txt") -Destination (Join-Path $FinalResultSupplementDir "参数.txt") -Force
    Copy-Item -LiteralPath (Join-Path $templateRoot "04_复现与验证\参数.txt") -Destination (Join-Path $FinalResultSupplementDir "lc_参数.txt") -Force
    Copy-Item -LiteralPath (Join-Path $templateRoot "04_复现与验证\必须由队长完成的GeoEast操作.txt") -Destination (Join-Path $FinalResultSupplementDir "必须由队长完成的GeoEast操作.txt") -Force
}

$hashPath = Join-Path $submission "SHA256SUMS.txt"
$hashLines = New-Object System.Collections.Generic.List[string]
Get-ChildItem -LiteralPath $submission -Recurse -File |
    Where-Object { $_.FullName -ne $hashPath } |
    Sort-Object FullName |
    ForEach-Object {
        $relative = $_.FullName.Substring($submission.Length).TrimStart('\').Replace('\', '/')
        $hashLines.Add("$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash) *$relative")
    }
[System.IO.File]::WriteAllLines($hashPath, $hashLines, (New-Object System.Text.UTF8Encoding($false)))

& (Join-Path $submission "04_复现与验证\核验提交包哈希.ps1") -CodeRoot $submission
Write-Host "评测部署补丁已同步到：$submission"
Write-Host "提交包哈希条目：$($hashLines.Count)"
