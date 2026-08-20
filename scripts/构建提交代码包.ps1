#Requires -Version 5.1
<#
.SYNOPSIS
    从干净 Git 提交和统一 V1.1.3 构建产物生成正式竞赛代码提交包。

.DESCRIPTION
    源码只通过 git archive 导出跟踪文件，避免把 outputs、缓存、编译产物、
    本机配置或历史版本带入提交目录。独立软件和 GeoEast 插件分别组织，
    提交说明来自 packaging/submission 中的版本化模板。
#>
[CmdletBinding()]
param(
    [string]$Version = "v1.1.3",
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
$Destination = [System.IO.Path]::GetFullPath($Destination)

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

$trackedPowerShell = @(& git -c core.quotepath=false -C $repoRoot ls-files "*.ps1")
foreach ($relative in $trackedPowerShell) {
    $scriptPath = Join-Path $repoRoot $relative
    $bytes = [System.IO.File]::ReadAllBytes($scriptPath)
    $hasNonAscii = @($bytes | Where-Object { $_ -ge 0x80 }).Count -gt 0
    $hasUtf8Bom = $bytes.Length -ge 3 -and
        $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
    if ($hasNonAscii -and !$hasUtf8Bom) {
        throw "Windows PowerShell 5.1 兼容检查失败：含非 ASCII 字符的脚本必须使用 UTF-8 BOM：$relative"
    }
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
    $sourcePayloadPrefix = "01_源码\$srcName\"
    $sourceGuideRelative = "${sourcePayloadPrefix}源码构建与工程说明.md"
    $portablePayloadPrefix = "02_独立软件\FaultPolyTrace_Portable_$Version\"
    $pluginPayloadPrefix = "03_GeoEast插件\$pluginName\"
    $evaluationPayloadPrefix = "06_评测运行程序\$evaluationName\"
    Get-ChildItem -LiteralPath $Root -Recurse -File |
        Where-Object { $_.Extension.ToLowerInvariant() -in @(".md", ".ps1", ".json") } |
        Where-Object {
            $relative = $_.FullName.Substring($Root.Length).TrimStart('\')
            $insideSourcePayload = $relative.StartsWith($sourcePayloadPrefix, [System.StringComparison]::OrdinalIgnoreCase)
            (!$insideSourcePayload -or $relative.Equals($sourceGuideRelative, [System.StringComparison]::OrdinalIgnoreCase)) -and
                !$relative.StartsWith($portablePayloadPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and
                !$relative.StartsWith($pluginPayloadPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and
                !$relative.StartsWith($evaluationPayloadPrefix, [System.StringComparison]::OrdinalIgnoreCase)
        } |
        ForEach-Object {
        $text = [System.IO.File]::ReadAllText($_.FullName, [System.Text.Encoding]::UTF8)
        $updated = $text.Replace("{{VERSION}}", $Version).
            Replace("{{PLAIN_VERSION}}", $plain).
            Replace("{{BUILD_ID}}", [string]$manifestData.buildId).
            Replace("{{GIT_HEAD}}", $gitHead).
            Replace("{{INSTALLER_SHA256}}", $installerHash).
            Replace("{{RELEASE_ZIP_SHA256}}", $releaseZipHash)
        if (![string]::Equals($updated, $text, [System.StringComparison]::Ordinal)) {
            $withBom = $_.Extension.Equals(".ps1", [System.StringComparison]::OrdinalIgnoreCase)
            [System.IO.File]::WriteAllText($_.FullName, $updated, (New-Object System.Text.UTF8Encoding($withBom)))
        }
        }
}

function Convert-AbsolutePathToPortableLabel {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value) -or $Value -notmatch '^[A-Za-z]:[\\/]') {
        return $Value
    }

    $normalized = $Value.Replace('\', '/')
    $leaf = [System.IO.Path]::GetFileName($Value)
    if ($normalized -match '/test\.dat$') {
        return '<official test.dat>'
    }
    if ($normalized -match '/cli_robustness/cases/([^/]+)$') {
        return "cases/$($Matches[1])"
    }
    if ($normalized -match '/application_verifier/application_verifier_log\.xml$') {
        return 'ApplicationVerifier日志.xml'
    }
    if ($normalized -match '/application_verifier/verified_run\.log$') {
        return 'verified_run.log'
    }
    if ($normalized -match '/AppVerifierLogs/([^/]+)$') {
        return "ApplicationVerifier日志/$($Matches[1])"
    }
    if ($normalized -match '/official_result\.csv$' -or $normalized -match '/official_final_200\.csv$') {
        return '正式官方结果/FaultPolyTrace_Official_Result.csv'
    }
    if ($normalized -match '/faultpolytrace\.exe$') {
        return '<validated CLI executable>'
    }
    if (![string]::IsNullOrWhiteSpace($leaf)) {
        return "<validation workspace>/$leaf"
    }
    return '<validation workspace>'
}

function Protect-JsonNodePaths {
    param([object]$Node)

    if ($null -eq $Node) { return }
    if ($Node -is [System.Collections.IList]) {
        for ($index = 0; $index -lt $Node.Count; $index++) {
            if ($Node[$index] -is [string]) {
                $Node[$index] = Convert-AbsolutePathToPortableLabel -Value $Node[$index]
            } else {
                Protect-JsonNodePaths -Node $Node[$index]
            }
        }
        return
    }
    foreach ($property in $Node.PSObject.Properties) {
        if ($property.Value -is [string]) {
            $property.Value = Convert-AbsolutePathToPortableLabel -Value $property.Value
        } elseif ($property.Value -is [System.Collections.IList] -or
            $property.Value -is [System.Management.Automation.PSCustomObject]) {
            Protect-JsonNodePaths -Node $property.Value
        }
    }
}

function Protect-VerificationJsonPaths {
    param([string]$Root)

    Get-ChildItem -LiteralPath $Root -Recurse -File -Filter '*.json' | ForEach-Object {
        $data = Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
        Protect-JsonNodePaths -Node $data
        $json = $data | ConvertTo-Json -Depth 100
        [System.IO.File]::WriteAllText($_.FullName, $json + [Environment]::NewLine,
            (New-Object System.Text.UTF8Encoding($false)))
    }
}

function Assert-NoDevelopmentPathLeak {
    param([string]$Root)

    $legacyDownloadRoot = 'D:' + [System.IO.Path]::DirectorySeparatorChar + 'BaiduNetdiskDownload'
    $forbidden = @(
        $repoRoot,
        $repoRoot.Replace('\', '/'),
        [Environment]::GetFolderPath('UserProfile'),
        [Environment]::GetFolderPath('UserProfile').Replace('\', '/'),
        $legacyDownloadRoot,
        $legacyDownloadRoot.Replace('\', '/')
    ) | Where-Object { ![string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique
    # Python contract tests intentionally contain the forbidden strings as assertions;
    # generated reports and executable-facing text remain covered by this scan.
    $textExtensions = @('.csv', '.fmt', '.h', '.hpp', '.in', '.ini', '.iss', '.json', '.md',
        '.ps1', '.txt', '.xml', '.yaml', '.yml')
    $leaks = New-Object System.Collections.Generic.List[string]

    Get-ChildItem -LiteralPath $Root -Recurse -File |
        Where-Object { $textExtensions -contains $_.Extension.ToLowerInvariant() } |
        ForEach-Object {
            $text = [System.IO.File]::ReadAllText($_.FullName)
            foreach ($needle in $forbidden) {
                if ($text.IndexOf($needle, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
                    $relative = $_.FullName.Substring($Root.Length).TrimStart('\')
                    $leaks.Add("$relative -> $needle")
                    break
                }
            }
        }
    if ($leaks.Count -gt 0) {
        throw "提交包包含开发机绝对路径：`n$($leaks -join [Environment]::NewLine)"
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
# packaging/submission is the outer delivery template. Embedding it inside the
# source payload recursively duplicates formal results and can leave reviewers
# with more than one apparent DAT source of truth.
Remove-Item -LiteralPath (Join-Path $srcDir "packaging\submission") -Recurse -Force -ErrorAction SilentlyContinue
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
$smokeMatch = Get-ChildItem -LiteralPath (Join-Path $releaseRoot "installer_smoke") -Directory -ErrorAction SilentlyContinue |
    ForEach-Object {
        $candidateResult = Join-Path $_.FullName "installer_smoke_summary.json"
        if (Test-Path -LiteralPath $candidateResult -PathType Leaf) {
            try {
                $candidateData = Get-Content -LiteralPath $candidateResult -Raw -Encoding UTF8 | ConvertFrom-Json
                if ($candidateData.status -eq "PASS" -and $candidateData.buildId -eq $manifestData.buildId) {
                    [pscustomobject]@{
                        Root = $_
                        Result = $candidateResult
                        LastWriteTimeUtc = (Get-Item -LiteralPath $candidateResult).LastWriteTimeUtc
                    }
                }
            } catch {
                Write-Warning "忽略无法解析的安装器烟测结果：$candidateResult"
            }
        }
    } |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First 1
if (!$smokeMatch) { throw "缺少与当前构建 $($manifestData.buildId) 匹配且通过的安装器完整运行检查。" }
$smokeRoot = $smokeMatch.Root
$smokeResult = $smokeMatch.Result
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

$complexPlanarEvidence = Join-Path $repoRoot "outputs\evidence\adaptive_boundary_cross_domain_20260805\complex12"
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
$currentAsanReport = Join-Path $repoRoot "outputs\verification\msvc_asan\asan_report.json"
$currentStabilityRoot = Join-Path $repoRoot "examples\outputs\stability_support_bridge"
$currentGeoEastReadback = Join-Path $repoRoot "outputs\verification\geoeast_product_default_v1.1.3\transaction_readback.json"
$currentParameterStability = Join-Path $repoRoot "outputs\verification\parameter_stability_support_bridge\离散分位平台核验.json"
foreach ($requiredEvidence in @(
    $currentAsanReport,
    (Join-Path $currentStabilityRoot "稳定性摘要.json"),
    (Join-Path $currentStabilityRoot "连续运行明细.csv"),
    $currentGeoEastReadback,
    $currentParameterStability
)) {
    if (!(Test-Path -LiteralPath $requiredEvidence -PathType Leaf)) {
        throw "缺少v1.1.3正式默认线可靠性证据：$requiredEvidence"
    }
}
$latestGeosValidation = Get-ChildItem -LiteralPath (Join-Path $repoRoot "outputs\regression") -Recurse -File -Filter "geos_geometry_validation.json" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
foreach ($item in @(
    @{
        Source = $currentAsanReport
        Target = "AddressSanitizer验证摘要.json"
    },
    @{
        Source = (Join-Path $currentStabilityRoot "稳定性摘要.json")
        Target = "连续运行稳定性摘要.json"
    },
    @{
        Source = (Join-Path $currentStabilityRoot "连续运行明细.csv")
        Target = "连续运行明细.csv"
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
        Source = $currentGeoEastReadback
        Target = "GeoEast事务写入与回读.json"
    },
    @{
        Source = $currentParameterStability
        Target = "离散分位平台内扰动核验.json"
    },
    @{
        Source = $(if ($latestGeosValidation) { $latestGeosValidation.FullName } else { $null })
        Target = "完整几何GEOS交叉验证.json"
    }
)) {
    if ($item.Source -and (Test-Path -LiteralPath $item.Source -PathType Leaf)) {
        Copy-Item -LiteralPath $item.Source -Destination (Join-Path $reliabilityTarget $item.Target) -Force
    }
}

Write-Host "[5/6] 写入提交说明与许可"
Copy-Tree -Source $templateRoot -Target $Destination -ExcludeDirs @(
    (Join-Path $templateRoot "01_源码"),
    (Join-Path $templateRoot "04_复现与验证\正式官方结果")
)
foreach ($name in @("lc_result.dat", "lc_result.fmt")) {
    Copy-Item -LiteralPath (Join-Path $templateRoot "04_复现与验证\正式官方结果\$name") `
        -Destination $officialOut -Force
}
Copy-Item -LiteralPath $manifestFile.FullName -Destination (Join-Path $Destination "唯一构建清单.json") -Force
Expand-SubmissionTokens -Root $Destination
Protect-VerificationJsonPaths -Root $verifyDir
Assert-NoDevelopmentPathLeak -Root $Destination

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
