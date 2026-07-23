from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_cli_exposes_experimental_weak_path_options():
    main_cpp = (ROOT / "src" / "app" / "main.cpp").read_text(encoding="utf-8")

    assert "--weak-path-gate" in main_cpp
    assert "--no-weak-path-gate" in main_cpp
    assert "--weak-path-support-ratio" in main_cpp
    assert "--weak-path-max-unsupported-run" in main_cpp
    assert "traceOptions.weakPathGate = cli.weakPathGate" in main_cpp


def test_diagnostics_record_weak_path_configuration_and_counts():
    header = (ROOT / "src" / "io" / "DiagnosticsIO.h").read_text(encoding="utf-8")
    implementation = (ROOT / "src" / "io" / "DiagnosticsIO.cpp").read_text(
        encoding="utf-8"
    )

    for field in (
        "weakPathGate",
        "weakPathSupportRatio",
        "weakPathMaxUnsupportedRun",
        "weakPathCandidateCells",
        "weakPathAcceptedCells",
        "weakPathRejectedCells",
        "weakPathSeedCells",
        "weakPathSeedContactCells",
        "weakPathRootConflictRejectedCells",
        "weakPathCandidateComponents",
        "weakPathAcceptedComponents",
        "weakPathRejectedComponents",
        "weakPathPartiallyRejectedComponents",
        "weakPathEvidenceEvaluatedFragments",
        "weakPathAttributeRejectedFragments",
        "weakPathShapeRejectedFragments",
        "weakPathDirectionRejectedFragments",
    ):
        assert field in header
        assert f'\\"{field}\\"' in implementation

    for field in (
        "weakPathEvidenceEvaluatedFragments",
        "weakPathAttributeRejectedFragments",
        "weakPathShapeRejectedFragments",
        "weakPathDirectionRejectedFragments",
    ):
        assert f'\\"{field}\\"' in implementation


def test_all_core_build_manifests_link_weak_path_support():
    manifests = (
        ROOT / "scripts" / "build.ps1",
        ROOT / "geoeast_plugin" / "bo_runner" / "FaultPolyTraceGeoEast.pro",
        ROOT / "geoeast_plugin" / "bo_runner" / "FaultPolyTraceGeoEast.vcxproj",
    )

    for manifest in manifests:
        content = manifest.read_text(encoding="utf-8")
        assert "WeakPathSupport.cpp" in content, manifest


def test_cpp_core_test_runner_builds_and_runs_weak_path_tests():
    runner = ROOT / "scripts" / "test_cpp_core.ps1"

    assert runner.exists()
    content = runner.read_text(encoding="utf-8")
    assert "test_weak_path_support.cpp" in content
    assert "test_weak_path_support_integration.cpp" in content
    assert "WeakPathSupport.cpp" in content
    assert "FaultTracer.cpp" in content
