import csv
import json
import os
import stat
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
    built_exe = ROOT / "build" / "faultpolytrace.exe"
    assert built_exe.exists()
    isolated_exe = (
        tmp_path_factory.mktemp("faultpolytrace_core")
        / "faultpolytrace.exe"
    )
    shutil.copy2(built_exe, isolated_exe)
    return isolated_exe


@pytest.fixture()
def small_grid(tmp_path):
    path = tmp_path / "grid.txt"
    path.write_text(
        "\n".join(
            (
                "5 5",
                "0 0",
                "1 1",
                "0 0 0 0 0",
                "0 1 0.7 0 0",
                "0 1 0.7 0.7 0",
                "0 0 0 0.7 0",
                "0 0 0 0 0",
            )
        ),
        encoding="ascii",
    )
    return path


def test_diagnostics_paths_are_utf8_in_chinese_directory(core_exe, tmp_path):
    chinese_dir = tmp_path / "中文路径"
    chinese_dir.mkdir()
    grid = chinese_dir / "输入网格.txt"
    output = chinese_dir / "结果.csv"
    diagnostics = chinese_dir / "诊断.json"
    grid.write_text(
        "\n".join(
            (
                "5 5",
                "0 0",
                "1 1",
                "0 0 0 0 0",
                "0 1 0.7 0 0",
                "0 1 0.7 0.7 0",
                "0 0 0 0.7 0",
                "0 0 0 0 0",
            )
        ),
        encoding="ascii",
    )

    result = subprocess.run(
        [
            str(core_exe),
            "--input",
            str(grid),
            "--input-format",
            "matrix",
            "--output",
            str(output),
            "--diagnostics",
            str(diagnostics),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )

    assert result.returncode == 0, result.stdout + result.stderr
    document = json.loads(diagnostics.read_text(encoding="utf-8"))
    assert "中文路径" in document["inputPath"]
    assert "中文路径" in document["outputPath"]


def test_cli_rejects_non_finite_weak_path_ratio(core_exe, small_grid, tmp_path):
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
            "--weak-path-support-ratio",
            "nan",
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode != 0
    assert "weakPathSupportRatio must be between 0 and 1" in result.stderr
    assert not output.exists()
    assert not diagnostics.exists()


@pytest.mark.parametrize(
    ("origin", "spacing", "expected"),
    (
        ("nan 0", "1 1", "Failed to read grid origin"),
        ("0 0", "inf 1", "Failed to read grid spacing"),
    ),
)
def test_cli_rejects_non_finite_grid_metadata(
    core_exe, tmp_path, origin, spacing, expected
):
    grid = tmp_path / "invalid_metadata.txt"
    grid.write_text(
        "\n".join(("2 2", origin, spacing, "1 0", "0 1")),
        encoding="ascii",
    )
    output = tmp_path / "must_not_exist.csv"
    result = subprocess.run(
        [
            str(core_exe),
            "--input",
            str(grid),
            "--input-format",
            "matrix",
            "--output",
            str(output),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode != 0
    assert expected in result.stderr
    assert not output.exists()


@pytest.mark.skipif(os.name != "nt", reason="Windows replacement semantics")
def test_atomic_output_failure_preserves_previous_result_and_cleans_temp(
    core_exe, small_grid, tmp_path
):
    output = tmp_path / "protected.csv"
    command = [
        str(core_exe),
        "--input",
        str(small_grid),
        "--input-format",
        "matrix",
        "--output",
        str(output),
        "--trace-mode",
        "region",
        "--threshold-mode",
        "fixed",
        "--threshold",
        "0.7",
        "--min-area",
        "1",
    ]
    first = subprocess.run(
        command, cwd=ROOT, capture_output=True, text=True, check=False
    )
    assert first.returncode == 0, first.stdout + first.stderr
    previous = output.read_bytes()

    os.chmod(output, stat.S_IREAD)
    try:
        second = subprocess.run(
            command, cwd=ROOT, capture_output=True, text=True, check=False
        )
        assert second.returncode != 0
        assert output.read_bytes() == previous
        assert not list(tmp_path.glob("protected.csv.fpt-tmp-*"))
    finally:
        os.chmod(output, stat.S_IWRITE | stat.S_IREAD)


def test_cli_writes_parseable_weak_path_diagnostics(
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
            "--weak-percentile",
            "0.7",
            "--weak-connect-radius",
            "1",
            "--weak-path-gate",
            "--weak-path-support-ratio",
            "0.5",
            "--weak-path-max-unsupported-run",
            "2",
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
    assert payload["parameters"]["weakPathGate"] is True
    assert payload["parameters"]["weakPathSupportRatio"] == 0.5
    assert payload["parameters"]["weakPathMaxUnsupportedRun"] == 2
    assert payload["weakPath"]["weakPathCandidateCells"] >= 0
    assert payload["weakPath"]["weakPathAcceptedCells"] >= 0
    assert payload["weakPath"]["weakPathRejectedCells"] >= 0
    assert payload["weakPath"]["weakPathCandidateComponents"] >= 0
    assert payload["weakPath"]["weakPathAcceptedComponents"] >= 0
    assert payload["weakPath"]["weakPathRejectedComponents"] >= 0
    assert payload["weakPath"]["weakPathSeedContactCells"] >= 0
    assert payload["weakPath"]["weakPathRootConflictRejectedCells"] >= 0
    assert payload["weakPath"]["weakPathPartiallyRejectedComponents"] >= 0
    assert payload["weakPath"]["weakPathEvidenceEvaluatedComponents"] >= 0
    assert payload["weakPath"]["weakPathEvidenceEvaluatedFragments"] >= 0
    assert payload["weakPath"]["weakPathAttributeRejectedFragments"] >= 0
    assert payload["weakPath"]["weakPathShapeRejectedFragments"] >= 0
    assert payload["weakPath"]["weakPathDirectionRejectedFragments"] >= 0


def test_cli_writes_parseable_branch_attribution_shadow_diagnostics(
    core_exe, small_grid, tmp_path
):
    output = tmp_path / "branch_shadow.csv"
    diagnostics = tmp_path / "branch_shadow.json"
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
            "ridge-belt",
            "--threshold-mode",
            "fixed",
            "--threshold",
            "0.6",
            "--branch-attribution-shadow",
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
    assert payload["parameters"]["branchAttributionShadow"] is True
    for field in (
        "branchAttributionJunctionClusters",
        "branchAttributionAcceptedPairs",
        "branchAttributionAmbiguousClusters",
        "branchAttributionUnmatchedEnds",
        "branchAttributionGreedyFallbackClusters",
        "branchAttributionPairingDifferingClusters",
        "branchAttributionDifferingClusters",
    ):
        assert payload["branchAttribution"][field] >= 0
    assert (
        payload["branchAttribution"][
            "branchAttributionAcceptedPairingDifferingClusters"
        ]
        == payload["branchAttribution"][
            "branchAttributionPairingDifferingClusters"
        ]
    )
    assert (
        payload["branchAttribution"]["branchAttributionAcceptedDifferingClusters"]
        == payload["branchAttribution"]["branchAttributionDifferingClusters"]
    )


def test_cli_writes_parseable_branch_attribution_difference_csv(
    core_exe, tmp_path
):
    junction_grid = tmp_path / "junction_grid.txt"
    rows = []
    for y in range(9):
        row = []
        for x in range(9):
            row.append("1" if x == 4 or y == 4 else "0")
        rows.append(" ".join(row))
    junction_grid.write_text(
        "\n".join(("9 9", "0 0", "1 1", *rows)),
        encoding="ascii",
    )
    output = tmp_path / "branch_difference.csv"
    diagnostics = tmp_path / "branch_difference.json"
    differences = tmp_path / "branch_attribution_differences.csv"
    result = subprocess.run(
        [
            str(core_exe),
            "--input",
            str(junction_grid),
            "--input-format",
            "matrix",
            "--output",
            str(output),
            "--diagnostics",
            str(diagnostics),
            "--trace-mode",
            "ridge-belt",
            "--threshold-mode",
            "fixed",
            "--threshold",
            "0.6",
            "--branch-attribution-shadow",
            "--branch-attribution-differences",
            str(differences),
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
    assert differences.exists()
    rows = list(csv.DictReader(differences.open(encoding="utf-8")))
    expected_columns = {
        "cluster_id",
        "center_grid_x",
        "center_grid_y",
        "legacy_pairs",
        "new_pairs",
        "differs_from_legacy",
        "best_cost",
        "alternative_cost",
        "confidence",
    }
    assert rows
    assert expected_columns.issubset(rows[0])


def test_branch_attribution_shadow_does_not_change_geometry_outputs(
    core_exe, tmp_path
):
    junction_grid = tmp_path / "shadow_invariant_grid.txt"
    rows = []
    for y in range(11):
        row = []
        for x in range(11):
            active = x == 5 or y == 5 or (x == y and 2 <= x <= 8)
            row.append("1" if active else "0")
        rows.append(" ".join(row))
    junction_grid.write_text(
        "\n".join(("11 11", "12.5 -7.5", "2 3", *rows)),
        encoding="ascii",
    )

    def run_shadow(enabled):
        prefix = "shadow_on" if enabled else "shadow_off"
        polygon = tmp_path / f"{prefix}.polygons.csv"
        branch = tmp_path / f"{prefix}.branches.csv"
        topology = tmp_path / f"{prefix}.topology.csv"
        diagnostics = tmp_path / f"{prefix}.json"
        command = [
            str(core_exe),
            "--input",
            str(junction_grid),
            "--input-format",
            "matrix",
            "--output",
            str(polygon),
            "--branches-output",
            str(branch),
            "--topology-output",
            str(topology),
            "--diagnostics",
            str(diagnostics),
            "--trace-mode",
            "ridge-belt",
            "--threshold-mode",
            "fixed",
            "--threshold",
            "0.6",
            "--min-area",
            "0",
            "--simplify-tolerance",
            "0",
            "--smooth-iterations",
            "0",
        ]
        if enabled:
            command.append("--branch-attribution-shadow")
        result = subprocess.run(
            command,
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        return polygon, branch, topology, json.loads(
            diagnostics.read_text(encoding="utf-8")
        )

    off_polygon, off_branch, off_topology, off_diagnostics = run_shadow(False)
    on_polygon, on_branch, on_topology, on_diagnostics = run_shadow(True)

    assert off_polygon.read_bytes() == on_polygon.read_bytes()
    assert off_branch.read_bytes() == on_branch.read_bytes()
    assert off_topology.read_bytes() == on_topology.read_bytes()
    assert {
        key: value
        for key, value in off_diagnostics["results"].items()
        if key != "elapsedMs"
    } == {
        key: value
        for key, value in on_diagnostics["results"].items()
        if key != "elapsedMs"
    }
    assert off_diagnostics["skeleton"] == on_diagnostics["skeleton"]
    assert off_diagnostics["topology"] == on_diagnostics["topology"]


def test_candidate_root_grouping_requires_root_anti_merge(
    core_exe,
    small_grid,
    tmp_path,
):
    output = tmp_path / "candidate.csv"
    result = subprocess.run(
        [
            str(core_exe),
            "--input",
            str(small_grid),
            "--input-format",
            "matrix",
            "--output",
            str(output),
            "--branch-attribution-candidate-groups",
            "--no-root-anti-merge",
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode != 0
    assert (
        "--branch-attribution-candidate-groups requires --root-anti-merge"
        in result.stderr
    )
    assert not output.exists()


def test_candidate_root_grouping_runs_as_explicit_experiment(
    core_exe,
    small_grid,
    tmp_path,
):
    output = tmp_path / "candidate.csv"
    diagnostics = tmp_path / "candidate.json"
    differences = tmp_path / "candidate-differences.csv"
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
            "--root-anti-merge",
            "--branch-attribution-candidate-groups",
            "--branch-attribution-differences",
            str(differences),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode == 0, result.stdout + result.stderr
    assert output.exists()
    assert differences.exists()
    document = json.loads(diagnostics.read_text(encoding="utf-8"))
    branch = document["branchAttribution"]
    assert document["parameters"]["branchAttributionCandidateGroups"] is True
    for field in (
        "branchAttributionCandidateGroupingAppliedClusters",
        "branchAttributionCandidateGroupingAmbiguousFallbackClusters",
        "branchAttributionCandidateGroupingLegacyFallbackClusters",
        "branchAttributionCandidateGroupingExcludedInternalBranches",
        "branchAttributionCandidateGroupingRootGroups",
    ):
        assert field in branch


def test_candidate_root_grouping_rejects_region_trace_mode(
    core_exe,
    small_grid,
    tmp_path,
):
    output = tmp_path / "candidate.csv"
    result = subprocess.run(
        [
            str(core_exe),
            "--input",
            str(small_grid),
            "--input-format",
            "matrix",
            "--output",
            str(output),
            "--trace-mode",
            "region",
            "--root-anti-merge",
            "--branch-attribution-candidate-groups",
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode != 0
    assert "requires --trace-mode ridge-belt" in result.stderr
    assert not output.exists()


@pytest.mark.parametrize(
    "conflicting_option",
    [
        "--output",
        "--diagnostics",
        "--branches-output",
        "--topology-output",
    ],
)
def test_branch_attribution_differences_rejects_output_path_collisions(
    core_exe,
    small_grid,
    tmp_path,
    conflicting_option,
):
    shared = tmp_path / "shared-output.dat"
    polygon = tmp_path / "polygons.csv"
    diagnostics = tmp_path / "diagnostics.json"
    branches = tmp_path / "branches.csv"
    topology = tmp_path / "topology.csv"
    option_paths = {
        "--output": polygon,
        "--diagnostics": diagnostics,
        "--branches-output": branches,
        "--topology-output": topology,
    }
    option_paths[conflicting_option] = shared
    result = subprocess.run(
        [
            str(core_exe),
            "--input",
            str(small_grid),
            "--input-format",
            "matrix",
            "--output",
            str(option_paths["--output"]),
            "--diagnostics",
            str(option_paths["--diagnostics"]),
            "--branches-output",
            str(option_paths["--branches-output"]),
            "--topology-output",
            str(option_paths["--topology-output"]),
            "--branch-attribution-shadow",
            "--branch-attribution-differences",
            str(shared),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode != 0
    assert "Output paths must be distinct" in result.stderr
    assert not shared.exists()


def test_outputs_cannot_overwrite_input_grid(core_exe, small_grid, tmp_path):
    original = small_grid.read_bytes()
    result = subprocess.run(
        [
            str(core_exe),
            "--input",
            str(small_grid),
            "--input-format",
            "matrix",
            "--output",
            str(small_grid),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode != 0
    assert "must not overwrite --input" in result.stderr
    assert small_grid.read_bytes() == original


def test_branch_attribution_differences_cannot_overwrite_input_grid(
    core_exe,
    small_grid,
    tmp_path,
):
    original = small_grid.read_bytes()
    output = tmp_path / "polygons.csv"
    result = subprocess.run(
        [
            str(core_exe),
            "--input",
            str(small_grid),
            "--input-format",
            "matrix",
            "--output",
            str(output),
            "--branch-attribution-shadow",
            "--branch-attribution-differences",
            str(small_grid),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode != 0
    assert "must not overwrite --input" in result.stderr
    assert small_grid.read_bytes() == original
    assert not output.exists()


def test_branch_attribution_difference_csv_exact_serialization(tmp_path):
    helper_source = tmp_path / "write_branch_diff.cpp"
    helper_exe = tmp_path / "write_branch_diff.exe"
    output = tmp_path / "branch_diff.csv"
    helper_source.write_text(
        r'''
#include "src/io/DiagnosticsIO.h"

int main(int argc, char** argv) {
    fpt::JunctionAttributionResult junction;
    junction.clusterId = 7;
    junction.centerGridX = 2.5;
    junction.centerGridY = 3.5;
    junction.centerWorldX = 106.25;
    junction.centerWorldY = -36.0;
    junction.minGridX = 2;
    junction.minGridY = 3;
    junction.maxGridX = 3;
    junction.maxGridY = 4;
    junction.legacyPairs = {
        {1, 3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
        {2, 3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    junction.pairs = {
        {1, 2, 0.5, 12.5, 0.125, 0.25, 0.375, 0.5}};
    junction.pairingDiffersFromLegacy = true;
    junction.differsFromLegacy = false;
    junction.acceptedPairingDiffersFromLegacy = true;
    junction.acceptedDiffersFromLegacy = false;
    junction.ambiguous = false;
    junction.usedExactMatching = true;
    junction.bestCost = 0.5;
    junction.confidence = 0.25;
    fpt::writeBranchAttributionDifferencesCsv(argv[1], {junction});
    return argc == 2 ? 0 : 1;
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

    assert output.read_text(encoding="utf-8").splitlines() == [
        (
            "cluster_id,center_grid_x,center_grid_y,center_world_x,center_world_y,"
            "min_grid_x,min_grid_y,max_grid_x,max_grid_y,"
            "legacy_pairs,new_pairs,legacy_unmatched,new_unmatched,"
            "pairing_differs_from_legacy,differs_from_legacy,"
            "ambiguous,used_exact_matching,best_cost,alternative_cost,confidence,"
            "turn_angle_degrees,angle_cost,attribute_cost,width_cost,background_cost,"
            "candidate_pairing_differs_from_legacy,candidate_differs_from_legacy,"
            "accepted_pairing_differs_from_legacy,accepted_differs_from_legacy"
        ),
        (
            "7,2.5,3.5,106.25,-36,2,3,3,4,1-3;2-3,1-2,,,1,0,0,1,"
            "0.5,,0.25,1-2:12.5,1-2:0.125,1-2:0.25,1-2:0.375,1-2:0.5,"
            "1,0,1,0"
        ),
    ]


def test_branch_attribution_diagnostics_separates_candidate_and_accepted_differences(
    tmp_path,
):
    helper_source = tmp_path / "write_branch_diagnostics.cpp"
    helper_exe = tmp_path / "write_branch_diagnostics.exe"
    output = tmp_path / "branch_diagnostics.json"
    helper_source.write_text(
        r'''
#include "src/io/DiagnosticsIO.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    fpt::RunDiagnostics diagnostics;
    diagnostics.branchAttributionCandidatePairingDifferingClusters = 9;
    diagnostics.branchAttributionCandidateDifferingClusters = 4;
    diagnostics.branchAttributionAcceptedPairingDifferingClusters = 8;
    diagnostics.branchAttributionAcceptedDifferingClusters = 3;
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
    assert write_result.returncode == 0, (
        write_result.stdout + write_result.stderr
    )

    branch = json.loads(output.read_text(encoding="utf-8"))[
        "branchAttribution"
    ]
    assert branch["branchAttributionPairingDifferingClusters"] == 9
    assert branch["branchAttributionDifferingClusters"] == 4
    assert branch["branchAttributionCandidatePairingDifferingClusters"] == 9
    assert branch["branchAttributionCandidateDifferingClusters"] == 4
    assert branch["branchAttributionAcceptedPairingDifferingClusters"] == 8
    assert branch["branchAttributionAcceptedDifferingClusters"] == 3
