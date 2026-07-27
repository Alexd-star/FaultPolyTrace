from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = (ROOT / "scripts/连续运行稳定性检查.ps1").read_text(encoding="utf-8")


def test_stability_script_defaults_to_fifty_runs_and_records_resources():
    assert "[int]$Runs = 50" in SCRIPT
    assert "PeakWorkingSet64" in SCRIPT
    assert "HandleCount" in SCRIPT
    assert "Get-FileHash" in SCRIPT


def test_stability_script_checks_determinism_failure_and_cleanup():
    assert "结果哈希不一致" in SCRIPT
    assert "failure_injection_missing.dat" in SCRIPT
    assert "Remove-Item -LiteralPath $tempDir -Recurse -Force" in SCRIPT
    assert "稳定性摘要.json" in SCRIPT


def test_performance_report_reads_total_elapsed_from_timing_section():
    source = (ROOT / "scripts/生成性能内存报告.py").read_text(encoding="utf-8")
    assert 'diag.get("timing", {}).get("elapsedMs", 0)' in source
    assert 'diag.get("results", {}).get("elapsedMs", 0)' not in source


def test_performance_regression_can_reuse_a_verified_build():
    source = (ROOT / "scripts/性能回归检查.ps1").read_text(encoding="utf-8")
    assert "[switch]$SkipBuild" in source
    assert "if (!$SkipBuild)" in source
    assert "Core executable not found" in source
