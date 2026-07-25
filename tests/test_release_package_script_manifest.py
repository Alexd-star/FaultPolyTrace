# -*- coding: utf-8 -*-
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_release_package_includes_peak_quality_scripts():
    script = (ROOT / "scripts" / "打包发布包.ps1").read_text(encoding="utf-8")
    required_scripts = [
        "冲顶候选集成入口.py",
        "冲顶候选后处理流水线.py",
        "局部候选修形实验.py",
        "分支归属拆分实验.py",
        "局部形态质量诊断.py",
        "局部修形拓扑核验.py",
        "生成纯断层多边形图.py",
        "全图分块视觉复核.py",
        "冲顶实验晋级验收.py",
    ]

    for name in required_scripts:
        assert name in script


def test_release_manifest_is_evidence_driven_and_records_build_identity():
    script = (ROOT / "scripts" / "打包发布包.ps1").read_text(encoding="utf-8")
    assert "FaultPolyTrace_Result=465" not in script
    assert "rev-parse HEAD" in script
    assert "status --porcelain" in script
    assert "Get-FileHash -Algorithm SHA256" in script
    assert "构建与依赖清单.json" in script
    assert "sourceSnapshot" in script
    assert "packageContentSha256" in script
    assert "Get-AggregateSha256" in script
    assert "GenerateBuildManifest.ps1" in script
    assert "GeoEast 现场数据库往返：待人工确认" in script
    assert 'Get-ChildItem -LiteralPath $docsDir -Recurse -File -Include "*.lic", "*.key", "*.pem"' in script


def test_one_click_acceptance_defaults_to_current_release_candidate():
    script = (ROOT / "scripts" / "一键总验收.ps1").read_text(encoding="utf-8-sig")
    assert '[string]$PackageVersion = "v1.0.1-contest-final"' in script
    assert "v0.9.55-enterprise-audit-fix" not in script


def test_release_package_defaults_to_current_contest_version():
    script = (ROOT / "scripts" / "打包发布包.ps1").read_text(encoding="utf-8-sig")
    assert '[string]$Version = "v1.0.1-contest-final"' in script


def test_release_package_excludes_generated_cache_and_local_launcher_config():
    script = (ROOT / "scripts" / "打包发布包.ps1").read_text(encoding="utf-8-sig")
    assert '$_.Name -ne "faultpolytrace_launcher.ini"' in script
    for forbidden in ("__pycache__", ".pytest_cache", ".pyc"):
        assert forbidden in script


def test_final_manifest_binds_package_zip_and_installer_to_one_build_id():
    script = (ROOT / "scripts" / "GenerateBuildManifest.ps1").read_text(encoding="utf-8-sig")
    for required in (
        "buildId",
        "sourceSnapshotSha256",
        "packageContentSha256",
        "release-zip",
        "windows-installer",
    ):
        assert required in script
