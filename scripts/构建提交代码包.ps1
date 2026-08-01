#Requires -Version 5.1
<#
.SYNOPSIS
    从干净 Git 提交和统一 V1.1.1 构建产物生成正式竞赛代码提交包。

.DESCRIPTION
    源码只通过 git archive 导出跟踪文件，避免把 outputs、缓存、编译产物、
    本机配置或历史版本带入提交目录。独立软件和 GeoEast 插件分别组织，
    提交说明来自 packaging/submission 中的版本化模板。
#>
[CmdletBinding()]
param(
    [string]$Version = "v1.1.1",
    [Parameter(Mandatory = $true)][string]$Destination,
    [switch]$KeepExisting,
    [switch]$AllowDirty
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$plain = $Version.TrimStart("v")
$releaseRoot = Join-Path $repoRoot "release"
$releaseDir = Join-Path $releaseRoot "FaultPolyTrace_$Version"
$releaseZip = Join-Path $releaseRoot "FaultPolyTrace_$Version.zip"
$installer = Join-Path $releaseRoot "installer\FaultPolyTrace_Setup_v$plain.exe"
$templateRoot = Join-Path $repoRoot "packaging\submission"

foreach ($required in @($releaseDir, $releaseZip, $installer, $templateRoot)) {
    if (!(Test-Path -LiteralPath $required)) {
        throw "缺少正式构建输入：$required"
    }
}

$gitHead = (& git -C $repoRoot rev-parse HEAD).Trim()
$gitStatus = @(& git -C $repoRoot status --porcelain --untracked-files=all)
if (!$AllowDirty -and $gitStatus.Count -gt 0) {
    throw "工作区存在未提交内容，不能生成正式提交包。请先提交，或仅在调试时使用 -AllowDirty。"
}

$manifestFile = Get-ChildItem -LiteralPath $releaseRoot -Filter "FPT-$Version-*_unique_build_manifest.json" |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (!$manifestFile) { throw "缺少 $Version 唯一构建清单。" }
$manifestData = Get-Content -LiteralPath $manifestFile.FullName -Raw | ConvertFrom-Json
if ($manifestData.version -ne $Version -or $manifestData.gitHead -ne $gitHead -or $manifestData.gitDirty) {
    throw "唯一构建清单与当前干净源码不一致，请重新构建发布包、安装器和构建清单。"
}

if ((Test-Path -LiteralPath $Destination) -and -not $KeepExisting) {
    $backup = "$Destination`_旧版备份_" + (Get-Date -Format "yyyyMMdd_HHmmss")
    Write-Host "备份现有提交包 -> $backup"
    Move-Item -LiteralPath $Destination -Destination $backup
}
New-Item -ItemType Directory -Force -Path $Destination | Out-Null

function New-Zip {
    param([string]$SourceDir, [string]$ZipPath)
    if (Test-Path -LiteralPath $ZipPath) { Remove-Item -LiteralPath $ZipPath -Force }
    $sevenZip = "C:\Program Files\7-Zip\7z.exe"
    if (Test-Path -LiteralPath $sevenZip) {
        & $sevenZip a -tzip -mx=5 -bso0 -bsp0 $ZipPath (Join-Path $SourceDir "*") | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "打包失败：$ZipPath" }
        & $sevenZip t -bso0 -bsp0 $ZipPath | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "压缩包完整性检查失败：$ZipPath" }
    } else {
        Compress-Archive -Path (Join-Path $SourceDir "*") -DestinationPath $ZipPath -Force
    }
}

function Copy-Tree {
    param([string]$Source, [string]$Target, [string[]]$ExcludeDirs = @())
    New-Item -ItemType Directory -Force -Path $Target | Out-Null
    $roboArgs = @($Source, $Target, "/E", "/NFL", "/NDL", "/NJH", "/NJS", "/NP", "/R:1", "/W:1")
    if ($ExcludeDirs.Count -gt 0) { $roboArgs += "/XD"; $roboArgs += $ExcludeDirs }
    & robocopy @roboArgs | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "复制失败：$Source -> $Target" }
    $global:LASTEXITCODE = 0
}

function Expand-SubmissionTokens {
    param([string]$Root)
    $installerHash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash
    $releaseZipHash = (Get-FileHash -LiteralPath $releaseZip -Algorithm SHA256).Hash
    Get-ChildItem -LiteralPath $Root -Recurse -File |
        Where-Object { $_.Extension.ToLowerInvariant() -in @(".md", ".ps1", ".json") } |
        ForEach-Object {
        $text = [System.IO.File]::ReadAllText($_.FullName, [System.Text.Encoding]::UTF8)
        $updated = $text.Replace("{{VERSION}}", $Version).
            Replace("{{PLAIN_VERSION}}", $plain).
            Replace("{{BUILD_ID}}", [string]$manifestData.buildId).
            Replace("{{GIT_HEAD}}", $gitHead).
            Replace("{{INSTALLER_SHA256}}", $installerHash).
            Replace("{{RELEASE_ZIP_SHA256}}", $releaseZipHash)
        $withBom = $_.Extension.Equals(".ps1", [System.StringComparison]::OrdinalIgnoreCase)
        [System.IO.File]::WriteAllText($_.FullName, $updated, (New-Object System.Text.UTF8Encoding($withBom)))
        }
}

Write-Host "[1/6] 导出 Git 跟踪源码"
$srcName = "FaultPolyTrace_Source_$Version"
$srcDir = Join-Path $Destination "01_源码\$srcName"
$sourceArchive = Join-Path ([System.IO.Path]::GetTempPath()) ("FaultPolyTrace_source_" + [guid]::NewGuid().ToString("N") + ".zip")
New-Item -ItemType Directory -Force -Path $srcDir | Out-Null
& git -C $repoRoot archive --format=zip --output=$sourceArchive HEAD
if ($LASTEXITCODE -ne 0) { throw "git archive 导出源码失败。" }
Expand-Archive -LiteralPath $sourceArchive -DestinationPath $srcDir -Force
Remove-Item -LiteralPath $sourceArchive -Force
Copy-Item -LiteralPath (Join-Path $releaseDir "构建与依赖清单.json") -Destination $srcDir -Force
Copy-Item -LiteralPath (Join-Path $templateRoot "01_源码\源码构建与工程说明.md") -Destination $srcDir -Force

$sourceFiles = @(Get-ChildItem -LiteralPath $srcDir -Recurse -File)
$sourceBytes = ($sourceFiles | Measure-Object Length -Sum).Sum
if ($sourceFiles.Count -gt 1000 -or $sourceBytes -gt 50MB) {
    throw "源码包异常膨胀：$($sourceFiles.Count) 个文件，$([math]::Round($sourceBytes / 1MB, 2)) MiB。"
}
foreach ($forbidden in @("outputs", "build", "release", ".git", ".pytest_tmp", ".pytest_cache")) {
    if (Test-Path -LiteralPath (Join-Path $srcDir $forbidden)) { throw "源码包含禁止目录：$forbidden" }
}
New-Zip -SourceDir $srcDir -ZipPath (Join-Path $Destination "01_源码\$srcName.zip")

Write-Host "[2/6] 组织独立软件"
$appDir = Join-Path $Destination "02_独立软件"
$portable = Join-Path $appDir "FaultPolyTrace_Portable_$Version"
Copy-Tree -Source $releaseDir -Target $portable -ExcludeDirs @(
    (Join-Path $releaseDir "geoeast_plugin"), (Join-Path $releaseDir "qt_launcher"),
    (Join-Path $releaseDir "src"), (Join-Path $releaseDir "standalone_app"),
    (Join-Path $releaseDir "scripts"), (Join-Path $releaseDir "figure"),
    (Join-Path $releaseDir "答辩评审材料包"), (Join-Path $releaseDir "examples\outputs"),
    (Join-Path $releaseDir "docs\作品说明书图件"), (Join-Path $releaseDir "docs\source_extracts"))
Remove-Item -LiteralPath (Join-Path $portable "FaultPolyTrace.iss") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $portable "ChineseSimplified.isl") -Force -ErrorAction SilentlyContinue
New-Zip -SourceDir $portable -ZipPath (Join-Path $appDir "FaultPolyTrace_Portable_$Version.zip")
Copy-Item -LiteralPath $installer -Destination (Join-Path $appDir "FaultPolyTrace_Setup_v$plain.exe") -Force
$installerManifest = Join-Path $releaseRoot "installer\installer_manifest.json"
if (Test-Path -LiteralPath $installerManifest) { Copy-Item -LiteralPath $installerManifest -Destination $appDir -Force }

Write-Host "[3/6] 组织 GeoEast 插件"
$pluginDir = Join-Path $Destination "03_GeoEast插件"
$pluginName = "FaultPolyTrace_GeoEastPlugin_$Version"
$pluginRoot = Join-Path $pluginDir $pluginName
Copy-Tree -Source (Join-Path $releaseDir "qt_launcher") -Target (Join-Path $pluginRoot "Launcher")
$boTarget = Join-Path $pluginRoot "BO_Runner"
$mainframeTarget = Join-Path $pluginRoot "Mainframe_Plugin"
New-Item -ItemType Directory -Force -Path $boTarget, $mainframeTarget | Out-Null
Copy-Item -LiteralPath (Join-Path $releaseDir "geoeast_plugin\bo_runner\faultpolytrace_geoeast_runner.exe") -Destination $boTarget -Force
Copy-Item -LiteralPath (Join-Path $releaseDir "geoeast_plugin\bo_runner\说明.md") -Destination $boTarget -Force
Copy-Item -LiteralPath (Join-Path $releaseDir "geoeast_plugin\mainframe_plugin\built\FaultPolyTraceMainframePlugin.dll") -Destination $mainframeTarget -Force
Copy-Item -LiteralPath (Join-Path $releaseDir "geoeast_plugin\mainframe_plugin\built\manifest.xml") -Destination $mainframeTarget -Force
Copy-Item -LiteralPath (Join-Path $releaseDir "geoeast_plugin\mainframe_plugin\说明.md") -Destination $mainframeTarget -Force
New-Item -ItemType Directory -Force -Path (Join-Path $pluginRoot "图标") | Out-Null
Copy-Item -LiteralPath (Join-Path $releaseDir "faultpolytrace.ico"), (Join-Path $releaseDir "faultpolytrace.png") -Destination (Join-Path $pluginRoot "图标") -Force
Copy-Item -LiteralPath (Join-Path $releaseDir "构建与依赖清单.json") -Destination $pluginRoot -Force
New-Zip -SourceDir $pluginRoot -ZipPath (Join-Path $pluginDir "$pluginName.zip")

$evaluationDir = Join-Path $Destination "06_评测运行程序"
$evaluationName = "FaultPolyTrace_评测运行程序_$Version"
$evaluationStaging = Join-Path $releaseRoot $evaluationName
& (Join-Path $PSScriptRoot "build_evaluation_package.ps1") `
    -Version $Version `
    -OutputDir $evaluationStaging `
    -LauncherPayloadDir (Join-Path $releaseDir "qt_launcher") `
    -RunnerExe (Join-Path $releaseDir "geoeast_plugin\bo_runner\faultpolytrace_geoeast_runner.exe")
Copy-Tree -Source $evaluationStaging -Target (Join-Path $evaluationDir $evaluationName)
Copy-Item -LiteralPath "$evaluationStaging.zip" -Destination (Join-Path $evaluationDir "$evaluationName.zip") -Force

Write-Host "[4/6] 组织复现资料"
$verifyDir = Join-Path $Destination "04_复现与验证"
New-Item -ItemType Directory -Force -Path $verifyDir | Out-Null
Copy-Item -LiteralPath $manifestFile.FullName -Destination (Join-Path $verifyDir "唯一构建清单.json") -Force
Copy-Item -LiteralPath (Join-Path $releaseDir "构建与依赖清单.json") -Destination $verifyDir -Force
$smokeRoot = Get-ChildItem -LiteralPath (Join-Path $releaseRoot "installer_smoke") -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | Select-Object -First 1
if (!$smokeRoot) { throw "缺少安装器完整运行检查目录。" }
$smokeResult = Join-Path $smokeRoot.FullName "installer_smoke_summary.json"
if (!(Test-Path -LiteralPath $smokeResult)) { throw "缺少安装器完整运行检查结果。" }
Copy-Item -LiteralPath $smokeResult -Destination (Join-Path $verifyDir "安装器完整运行检查.json") -Force
$officialOut = Join-Path $verifyDir "正式官方结果"
New-Item -ItemType Directory -Force -Path $officialOut | Out-Null
$smokeOfficialResult = Get-ChildItem -LiteralPath $smokeRoot.FullName -Recurse -File -Filter "official_result.csv" |
    Select-Object -First 1
if (!$smokeOfficialResult) { throw "安装器完整运行检查目录中缺少 official_result.csv。" }
Get-ChildItem -LiteralPath $smokeOfficialResult.DirectoryName -File | Copy-Item -Destination $officialOut -Force

$secondPlanarEvidence = Join-Path $repoRoot "outputs\evidence\second_planar_generalization_20260730"
if (Test-Path -LiteralPath $secondPlanarEvidence -PathType Container) {
    $secondPlanarTarget = Join-Path $verifyDir "第二套同域平面验证"
    New-Item -ItemType Directory -Force -Path $secondPlanarTarget | Out-Null
    foreach ($name in @(
        "01_第二套同域八类输入真值结果.png",
        "02_两套同域指标与扰动稳定性.png",
        "第二套同域验证与PPT展示文案.md",
        "第二套同域验证摘要.json"
    )) {
        $source = Join-Path $secondPlanarEvidence $name
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            Copy-Item -LiteralPath $source -Destination $secondPlanarTarget -Force
        }
    }
}

$complexPlanarEvidence = Join-Path $repoRoot "outputs\evidence\official_like_v2_topology_fixed_20260730"
if (Test-Path -LiteralPath $complexPlanarEvidence -PathType Container) {
    $complexPlanarTarget = Join-Path $verifyDir "复杂同域平面验证"
    New-Item -ItemType Directory -Force -Path $complexPlanarTarget | Out-Null
    foreach ($name in @("复杂同域验证报告.md", "复杂同域验证摘要.json")) {
        $source = Join-Path $complexPlanarEvidence $name
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            Copy-Item -LiteralPath $source -Destination $complexPlanarTarget -Force
        }
    }
    foreach ($case in @("case_01_mixed_network", "case_04_mixed_network", "case_08_mixed_network")) {
        $source = Join-Path $complexPlanarEvidence "baseline\$case\复核图.png"
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $complexPlanarTarget "$case`_正式基线复核图.png") -Force
        }
    }
}

$reliabilityTarget = Join-Path $verifyDir "可靠性验证"
New-Item -ItemType Directory -Force -Path $reliabilityTarget | Out-Null
$latestAcceptance = Get-ChildItem -LiteralPath (Join-Path $repoRoot "examples\outputs") -Directory -Filter "acceptance_*_official_root_antimerge" -ErrorAction SilentlyContinue |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "参数扰动稳定性\离散分位平台核验.json") -PathType Leaf } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
$latestFullScoreAudit = Get-ChildItem -LiteralPath (Join-Path $repoRoot "examples\outputs") -Directory -Filter "full_score_audit_*" -ErrorAction SilentlyContinue |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "赛题满分交付审计报告.md") -PathType Leaf } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
$latestGeoEastReadback = Get-ChildItem -LiteralPath (Join-Path $repoRoot "outputs\evidence") -Directory -Filter "geoeast_transaction_readback_*" -ErrorAction SilentlyContinue |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "transaction.json") -PathType Leaf } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
$latestGeosValidation = Get-ChildItem -LiteralPath (Join-Path $repoRoot "outputs\regression") -Recurse -File -Filter "geos_geometry_validation.json" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
foreach ($item in @(
    @{
        Source = (Join-Path $repoRoot "outputs\verification\msvc_asan\asan_report.json")
        Target = "AddressSanitizer验证摘要.json"
    },
    @{
        Source = (Join-Path $repoRoot "examples\outputs\stability\稳定性摘要.json")
        Target = "50次连续运行稳定性摘要.json"
    },
    @{
        Source = (Join-Path $repoRoot "examples\outputs\stability\连续运行明细.csv")
        Target = "50次连续运行明细.csv"
    },
    @{
        Source = (Join-Path $repoRoot "outputs\verification\application_verifier\application_verifier_report.json")
        Target = "ApplicationVerifier验证摘要.json"
    },
    @{
        Source = (Join-Path $repoRoot "outputs\verification\application_verifier\application_verifier_log.xml")
        Target = "ApplicationVerifier日志.xml"
    },
    @{
        Source = (Join-Path $repoRoot "outputs\verification\cli_robustness\cli_robustness_report.json")
        Target = "异常输入与容错验证.json"
    },
    @{
        Source = $(if ($latestGeoEastReadback) { Join-Path $latestGeoEastReadback.FullName "transaction.json" } else { $null })
        Target = "GeoEast事务写入与回读.json"
    },
    @{
        Source = $(if ($latestAcceptance) { Join-Path $latestAcceptance.FullName "参数扰动稳定性\离散分位平台核验.json" } else { $null })
        Target = "离散分位平台内扰动核验.json"
    },
    @{
        Source = $(if ($latestGeosValidation) { $latestGeosValidation.FullName } else { $null })
        Target = "完整几何GEOS交叉验证.json"
    },
    @{
        Source = $(if ($latestFullScoreAudit) { Join-Path $latestFullScoreAudit.FullName "赛题满分交付审计报告.md" } else { $null })
        Target = "赛题满分交付审计报告.md"
    }
)) {
    if ($item.Source -and (Test-Path -LiteralPath $item.Source -PathType Leaf)) {
        Copy-Item -LiteralPath $item.Source -Destination (Join-Path $reliabilityTarget $item.Target) -Force
    }
}

Write-Host "[5/6] 写入提交说明与许可"
Copy-Tree -Source $templateRoot -Target $Destination -ExcludeDirs @((Join-Path $templateRoot "01_源码"))
Copy-Item -LiteralPath $manifestFile.FullName -Destination (Join-Path $Destination "唯一构建清单.json") -Force
Expand-SubmissionTokens -Root $Destination

Write-Host "[6/6] 计算提交包哈希"
$hashPath = Join-Path $Destination "SHA256SUMS.txt"
$lines = New-Object System.Collections.Generic.List[string]
Get-ChildItem -LiteralPath $Destination -Recurse -File |
    Where-Object { $_.FullName -ne $hashPath } |
    Sort-Object FullName |
    ForEach-Object {
        $relative = $_.FullName.Substring($Destination.Length).TrimStart('\').Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        $lines.Add("$hash *$relative")
    }
[System.IO.File]::WriteAllLines($hashPath, $lines, (New-Object System.Text.UTF8Encoding($false)))

Write-Host ""
Write-Host "提交包已生成：$Destination"
Write-Host "文件总数：$($lines.Count)"
Write-Host "源码文件数：$($sourceFiles.Count)"
Write-Host "源码大小：$([math]::Round($sourceBytes / 1MB, 2)) MiB"
