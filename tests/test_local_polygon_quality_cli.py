import json
import shutil
import subprocess
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture(scope="module")
def core_exe(tmp_path_factory):
    build = subprocess.run(
        [
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(ROOT / "scripts" / "build.ps1"),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    assert build.returncode == 0, build.stdout + build.stderr
    isolated = tmp_path_factory.mktemp("local_quality_core") / "faultpolytrace.exe"
    shutil.copy2(ROOT / "build" / "faultpolytrace.exe", isolated)
    return isolated


@pytest.fixture()
def small_grid(tmp_path):
    path = tmp_path / "grid.txt"
    path.write_text(
        "\n".join(
            (
                "9 7",
                "100 -50",
                "2 3",
                "0 0 0 0 0 0 0 0 0",
                "0 0 1 1 1 1 1 0 0",
                "0 0 1 1 1 1 1 0 0",
                "0 0 1 1 1 1 1 0 0",
                "0 0 0 0 0 0 0 0 0",
                "0 0 0 0 0 0 0 0 0",
                "0 0 0 0 0 0 0 0 0",
            )
        ),
        encoding="ascii",
    )
    return path


def test_cli_contract_maps_local_quality_options_and_build_manifests():
    main_cpp = (ROOT / "src" / "app" / "main.cpp").read_text(encoding="utf-8")
    for option in (
        "--local-polygon-quality",
        "--local-quality-width-ratio",
        "--local-quality-min-extra-cells",
        "--local-quality-support-threshold-ratio",
        "--local-quality-support-buffer-cells",
        "--local-quality-max-area-loss-ratio",
        "--local-quality-min-stability-jaccard",
        "--local-quality-max-candidates-per-object",
        "--local-quality-max-total-candidates",
        "--local-quality-max-window-cells",
        "--local-quality-max-object-cells",
        "--local-quality-max-evidence-records",
    ):
        assert option in main_cpp
    assert "traceOptions.localPolygonQuality = cli.localPolygonQuality" in main_cpp
    assert (
        "diagnostics.localPolygonQualityEvidence =\n"
        "            result.localPolygonQualityEvidence"
    ) in main_cpp

    for manifest in (
        ROOT / "scripts" / "build.ps1",
        ROOT / "geoeast_plugin" / "bo_runner" / "FaultPolyTraceGeoEast.pro",
        ROOT / "geoeast_plugin" / "bo_runner" / "FaultPolyTraceGeoEast.vcxproj",
    ):
        assert "LocalPolygonQuality.cpp" in manifest.read_text(encoding="utf-8")


@pytest.mark.parametrize(
    ("option", "value", "message"),
    (
        ("--local-quality-width-ratio", "nan", "finite"),
        ("--local-quality-support-buffer-cells", "-1", "cell counts"),
        ("--local-quality-max-area-loss-ratio", "1.1", "between 0 and 1"),
        ("--local-quality-support-threshold-ratio", "2.1", "between 0 and 2"),
        (
            "--local-quality-max-candidates-per-object",
            "0",
            "processing budgets must be positive",
        ),
        (
            "--local-quality-max-total-candidates",
            "0",
            "processing budgets must be positive",
        ),
        (
            "--local-quality-max-window-cells",
            "0",
            "processing budgets must be positive",
        ),
        (
            "--local-quality-max-object-cells",
            "0",
            "processing budgets must be positive",
        ),
    ),
)
def test_cli_rejects_invalid_local_quality_parameters(
    core_exe, small_grid, tmp_path, option, value, message
):
    output = tmp_path / "invalid.csv"
    diagnostics = tmp_path / "invalid.json"
    result = subprocess.run(
        [
            str(core_exe),
            "--input",
            str(small_grid),
            "--input-format",
            "matrix",
            "--output",
            str(output),
            "--diagnostics",
            str(diagnostics),
            "--local-polygon-quality",
            option,
            value,
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode != 0
    assert message in result.stderr
    assert not output.exists()
    assert not diagnostics.exists()


def test_cli_does_not_validate_local_quality_semantics_while_disabled(
    core_exe, small_grid, tmp_path
):
    output = tmp_path / "disabled.csv"
    diagnostics = tmp_path / "disabled.json"
    result = subprocess.run(
        [
            str(core_exe),
            "--input",
            str(small_grid),
            "--input-format",
            "matrix",
            "--output",
            str(output),
            "--diagnostics",
            str(diagnostics),
            "--no-local-polygon-quality",
            "--local-quality-width-ratio",
            "0.5",
            "--local-quality-max-candidates-per-object",
            "0",
            "--local-quality-max-total-candidates",
            "0",
            "--local-quality-max-window-cells",
            "0",
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    payload = json.loads(diagnostics.read_text(encoding="utf-8"))
    assert payload["parameters"]["localPolygonQuality"] is False


def test_cli_writes_local_quality_parameters_stats_and_evidence_array(
    core_exe, small_grid, tmp_path
):
    output = tmp_path / "result.csv"
    diagnostics = tmp_path / "diagnostics.json"
    result = subprocess.run(
        [
            str(core_exe),
            "--input",
            str(small_grid),
            "--input-format",
            "matrix",
            "--output",
            str(output),
            "--diagnostics",
            str(diagnostics),
            "--trace-mode",
            "region",
            "--threshold-mode",
            "fixed",
            "--threshold",
            "0.9",
            "--local-polygon-quality",
            "--local-quality-width-ratio",
            "2.8",
            "--local-quality-min-extra-cells",
            "5",
            "--local-quality-support-threshold-ratio",
            "0.75",
            "--local-quality-support-buffer-cells",
            "2",
            "--local-quality-max-area-loss-ratio",
            "0.12",
            "--local-quality-min-stability-jaccard",
            "0.95",
            "--local-quality-max-candidates-per-object",
            "11",
            "--local-quality-max-total-candidates",
            "22",
            "--local-quality-max-window-cells",
            "333",
            "--local-quality-max-object-cells",
            "2000000",
            "--local-quality-max-evidence-records",
            "4",
            "--min-area",
            "0",
            "--simplify-tolerance",
            "0",
            "--smooth-iterations",
            "0",
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    payload = json.loads(diagnostics.read_text(encoding="utf-8"))
    parameters = payload["parameters"]
    assert parameters["localPolygonQuality"] is True
    assert parameters["localQualityWidthRatio"] == 2.8
    assert parameters["localQualityMinExtraCells"] == 5
    assert parameters["localQualitySupportThresholdRatio"] == 0.75
    assert parameters["localQualitySupportBufferCells"] == 2
    assert parameters["localQualityMaxAreaLossRatio"] == 0.12
    assert parameters["localQualityMinStabilityJaccard"] == 0.95
    assert parameters["localQualityMaxCandidatesPerObject"] == 11
    assert parameters["localQualityMaxTotalCandidates"] == 22
    assert parameters["localQualityMaxWindowCells"] == 333
    assert parameters["localQualityMaxObjectCells"] == 2000000
    assert parameters["localQualityMaxEvidenceRecords"] == 4

    quality = payload["localPolygonQuality"]
    for field in (
        "detected",
        "accepted",
        "review",
        "rejected",
        "objectBudgetReviewCount",
        "rejectedInvalidGeometry",
        "postRepairFilteredCount",
        "totalEvidence",
        "storedEvidence",
        "truncatedEvidence",
    ):
        assert quality[field] >= 0
    assert isinstance(quality["candidateEvidence"], list)
    assert len(quality["candidateEvidence"]) <= 4
    assert quality["storedEvidence"] == len(quality["candidateEvidence"])
    assert quality["totalEvidence"] == (
        quality["storedEvidence"] + quality["truncatedEvidence"]
    )
    rejection_reasons = (
        "rejectedInsufficientEvidence",
        "rejectedNoMeaningfulChange",
        "rejectedAreaLoss",
        "rejectedStrongCoverage",
        "rejectedWeakCoverage",
        "rejectedBackgroundLeakage",
        "rejectedTopology",
        "rejectedStability",
        "rejectedInvalidGeometry",
    )
    assert quality["rejected"] == sum(quality[field] for field in rejection_reasons)


def test_diagnostics_serializes_candidate_location_with_polygon_id(tmp_path):
    helper_source = tmp_path / "write_local_quality_diagnostics.cpp"
    helper_exe = tmp_path / "write_local_quality_diagnostics.exe"
    output = tmp_path / "local_quality_diagnostics.json"
    helper_source.write_text(
        r'''
#include "src/io/DiagnosticsIO.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    fpt::RunDiagnostics diagnostics;
    fpt::LocalPolygonQualityEvidence evidence;
    evidence.objectIndex = 3;
    evidence.polygonId = 999;
    evidence.type = fpt::LocalQualityCandidateType::LocalBulge;
    evidence.minGridX = 12;
    evidence.maxGridX = 18;
    evidence.minGridY = 7;
    evidence.maxGridY = 13;
    evidence.decision = fpt::LocalQualityDecision::Rejected;
    evidence.rejectionReason =
        fpt::LocalQualityRejectionReason::StabilityRisk;
    evidence.supportRatio = 0.625;
    evidence.areaLossRatio = 0.125;
    evidence.strongCoverageLossRatio = 0.01;
    evidence.weakCoverageLossRatio = 0.02;
    evidence.backgroundLeakageDelta = 0.03;
    evidence.stabilityJaccard = 0.94;
    diagnostics.localQualityMaxEvidenceRecords = 2;
    diagnostics.localQualityTotalEvidence = 3;
    diagnostics.localQualityStoredEvidence = 2;
    diagnostics.localQualityTruncatedEvidence = 1;
    diagnostics.localQualityPostRepairFilteredCount = 1;
    diagnostics.localQualityRejectedCount = 3;
    diagnostics.localQualityStabilityRejectedCount = 2;
    diagnostics.localQualityInvalidGeometryRejectedCount = 1;
    diagnostics.localPolygonQualityEvidence.push_back(evidence);
    evidence.objectIndex = 4;
    diagnostics.localPolygonQualityEvidence.push_back(evidence);
    evidence.objectIndex = 5;
    diagnostics.localPolygonQualityEvidence.push_back(evidence);
    fpt::writeDiagnosticsJson(argv[1], diagnostics);
    return 0;
}
'''.strip(),
        encoding="ascii",
    )
    compile_result = subprocess.run(
        [
            "g++",
            "-std=c++17",
            str(helper_source),
            str(ROOT / "src" / "io" / "DiagnosticsIO.cpp"),
            "-I",
            str(ROOT),
            "-o",
            str(helper_exe),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    assert compile_result.returncode == 0, (
        compile_result.stdout + compile_result.stderr
    )
    write_result = subprocess.run(
        [str(helper_exe), str(output)],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    assert write_result.returncode == 0, write_result.stdout + write_result.stderr

    quality = json.loads(output.read_text(encoding="utf-8"))[
        "localPolygonQuality"
    ]
    evidence = quality["candidateEvidence"]
    assert len(evidence) == 2
    assert evidence[0] == {
        "objectIndex": 3,
        "polygonId": 999,
        "type": "local_bulge",
        "window": {
            "minGridX": 12,
            "maxGridX": 18,
            "minGridY": 7,
            "maxGridY": 13,
        },
        "decision": "rejected",
        "rejectionReason": "stability_risk",
        "supportRatio": 0.625,
        "areaLossRatio": 0.125,
        "strongCoverageLossRatio": 0.01,
        "weakCoverageLossRatio": 0.02,
        "backgroundLeakageDelta": 0.03,
        "stabilityJaccard": 0.94,
    }
    assert evidence[1]["objectIndex"] == 4
    assert evidence[1]["polygonId"] == 999
    assert quality["totalEvidence"] == 3
    assert quality["storedEvidence"] == 2
    assert quality["truncatedEvidence"] == 1
    assert quality["postRepairFilteredCount"] == 1
    assert quality["rejected"] == 3
    assert quality["rejectedStability"] == 2
    assert quality["rejectedInvalidGeometry"] == 1
    assert quality["rejected"] == (
        quality["rejectedStability"] + quality["rejectedInvalidGeometry"]
    )
