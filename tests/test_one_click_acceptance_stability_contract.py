# -*- coding: utf-8 -*-
from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "一键总验收.ps1"
GATE_SCRIPT = ROOT / "scripts" / "AcceptanceGate.ps1"


def test_one_click_acceptance_exposes_optional_parameter_stability_gate():
    text = SCRIPT.read_text(encoding="utf-8-sig")

    assert "[switch]$RunParameterStabilityGate" in text
    assert "参数扰动稳定性验证.py" in text
    assert "--percentile 0.979" in text
    assert "--weak-percentile 0.964" in text
    assert "--percentile 0.981" in text
    assert "--weak-percentile 0.966" in text
    assert "参数扰动稳定性门禁" in text


def test_stability_gate_failure_is_not_silently_ignored():
    text = SCRIPT.read_text(encoding="utf-8-sig")

    assert "$stabilityGatePassed = ($LASTEXITCODE -eq 0)" in text
    assert "Pass = $stabilityGatePassed" in text


def test_failed_checks_block_deployment_and_packaging():
    text = SCRIPT.read_text(encoding="utf-8-sig")

    assert 'AcceptanceGate.ps1' in text
    assert "Invoke-AcceptedDelivery" in text
    assert "-Checks $checks" in text
    assert "-DeployAction $deployAction" in text
    assert "-PackageAction $packageAction" in text


def test_delivery_gate_does_not_invoke_actions_after_failed_check(tmp_path):
    shell = shutil.which("pwsh") or shutil.which("powershell")
    assert shell is not None
    deploy_marker = tmp_path / "deploy-called.txt"
    package_marker = tmp_path / "package-called.txt"
    command = f"""
. '{GATE_SCRIPT}'
if (!(Get-Command Invoke-AcceptedDelivery -ErrorAction SilentlyContinue)) {{
    exit 2
}}
$checks = @([pscustomobject]@{{ Item = 'stability'; Pass = $false; Detail = 'failed' }})
try {{
    Invoke-AcceptedDelivery `
        -Checks $checks `
        -ReportPath 'report.md' `
        -DeployAction {{ Set-Content -LiteralPath '{deploy_marker}' -Value 'called' }} `
        -PackageAction {{ Set-Content -LiteralPath '{package_marker}' -Value 'called' }}
}} catch {{
}}
if ((Test-Path -LiteralPath '{deploy_marker}') -or (Test-Path -LiteralPath '{package_marker}')) {{
    exit 1
}}
exit 0
"""

    result = subprocess.run(
        [shell, "-NoProfile", "-Command", command],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )

    assert result.returncode == 0, result.stderr
