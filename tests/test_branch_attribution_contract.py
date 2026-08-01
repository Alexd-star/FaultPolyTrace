from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_branch_attribution_is_default_off_shadow_diagnostics():
    header = (ROOT / "src" / "core" / "TraceOptions.h").read_text(encoding="utf-8")
    implementation = (ROOT / "src" / "core" / "FaultTracer.cpp").read_text(
        encoding="utf-8"
    )

    assert "bool branchAttributionShadow = false;" in header
    assert "analyzeBranchAttribution" in implementation
    assert "options.branchAttributionShadow" in implementation


def test_branch_attribution_summary_is_exposed_without_replacing_formal_grouping():
    header = (ROOT / "src" / "core" / "TraceResult.h").read_text(encoding="utf-8")
    implementation = (ROOT / "src" / "core" / "FaultTracer.cpp").read_text(
        encoding="utf-8"
    )

    for field in (
        "branchAttributionJunctionClusters",
        "branchAttributionAcceptedPairs",
        "branchAttributionAmbiguousClusters",
        "branchAttributionUnmatchedEnds",
        "branchAttributionGreedyFallbackClusters",
        "branchAttributionPairingDifferingClusters",
        "branchAttributionDifferingClusters",
    ):
        assert field in header
        assert field in implementation

    assert "groupBranchesByRootTrace(keptBranchRefs" in implementation


def test_formal_grouping_and_shadow_audit_share_the_legacy_pairing_kernel():
    header = (ROOT / "src" / "core" / "BranchAttribution.h").read_text(
        encoding="utf-8"
    )
    attribution = (ROOT / "src" / "core" / "BranchAttribution.cpp").read_text(
        encoding="utf-8"
    )
    tracer = (ROOT / "src" / "core" / "FaultTracer.cpp").read_text(
        encoding="utf-8"
    )

    assert "computeLegacyGreedyBranchPairing" in header
    assert attribution.count("computeLegacyGreedyBranchPairing(") >= 2
    grouping = tracer[
        tracer.index("std::vector<BranchGroup> groupBranchesByRootTrace") :
        tracer.index("bool localGet")
    ]
    assert "computeLegacyGreedyBranchPairing" in grouping
    assert "bestAngle" not in grouping


def test_all_core_build_manifests_link_branch_attribution():
    manifests = (
        ROOT / "scripts" / "build.ps1",
        ROOT / "geoeast_plugin" / "bo_runner" / "FaultPolyTraceGeoEast.pro",
        ROOT / "geoeast_plugin" / "bo_runner" / "FaultPolyTraceGeoEast.vcxproj",
        ROOT
        / "geoeast_plugin"
        / "bo_runner"
        / "FaultPolyTraceGeoEast.vcxproj.filters",
    )

    for manifest in manifests:
        content = manifest.read_text(encoding="utf-8")
        assert "BranchAttribution.cpp" in content, manifest
        assert "BranchAttribution.h" in content or manifest.name == "build.ps1", manifest


def test_cpp_runner_executes_branch_attribution_tests():
    runner = (ROOT / "scripts" / "test_cpp_core.ps1").read_text(encoding="utf-8")

    assert "test_branch_attribution.cpp" in runner
    assert "BranchAttribution.cpp" in runner


def test_cli_and_diagnostics_expose_shadow_mode():
    main_cpp = (ROOT / "src" / "app" / "main.cpp").read_text(encoding="utf-8")
    diagnostics_h = (ROOT / "src" / "io" / "DiagnosticsIO.h").read_text(
        encoding="utf-8"
    )
    diagnostics_cpp = (ROOT / "src" / "io" / "DiagnosticsIO.cpp").read_text(
        encoding="utf-8"
    )

    assert "--branch-attribution-shadow" in main_cpp
    assert "--no-branch-attribution-shadow" in main_cpp
    assert "traceOptions.branchAttributionShadow" in main_cpp
    assert "cli.branchAttributionShadow" in main_cpp
    for field in (
        "branchAttributionShadow",
        "branchAttributionJunctionClusters",
        "branchAttributionAcceptedPairs",
        "branchAttributionAmbiguousClusters",
        "branchAttributionUnmatchedEnds",
        "branchAttributionGreedyFallbackClusters",
        "branchAttributionPairingDifferingClusters",
        "branchAttributionDifferingClusters",
    ):
        assert field in diagnostics_h
        assert f'\\"{field}\\"' in diagnostics_cpp
    for field in (
        "branchAttributionAcceptedPairingDifferingClusters",
        "branchAttributionAcceptedDifferingClusters",
    ):
        assert f'\\"{field}\\"' in diagnostics_cpp


def test_cli_can_write_auditable_branch_attribution_difference_csv():
    main_cpp = (ROOT / "src" / "app" / "main.cpp").read_text(encoding="utf-8")
    diagnostics_h = (ROOT / "src" / "io" / "DiagnosticsIO.h").read_text(
        encoding="utf-8"
    )
    diagnostics_cpp = (ROOT / "src" / "io" / "DiagnosticsIO.cpp").read_text(
        encoding="utf-8"
    )

    assert "--branch-attribution-differences" in main_cpp
    assert "branchAttributionDifferencesOutput" in main_cpp
    assert "writeBranchAttributionDifferencesCsv" in main_cpp
    assert "writeBranchAttributionDifferencesCsv" in diagnostics_h
    assert "differs_from_legacy" in diagnostics_cpp
    assert "pairing_differs_from_legacy" in diagnostics_cpp
    for column in (
        "legacy_pairs",
        "new_pairs",
        "angle_cost",
        "attribute_cost",
        "width_cost",
        "background_cost",
        "best_cost",
        "alternative_cost",
        "confidence",
        "candidate_pairing_differs_from_legacy",
        "candidate_differs_from_legacy",
        "accepted_pairing_differs_from_legacy",
        "accepted_differs_from_legacy",
    ):
        assert column in diagnostics_cpp


def test_candidate_root_grouping_is_default_off_and_only_feeds_root_anti_merge():
    header = (ROOT / "src" / "core" / "TraceOptions.h").read_text(
        encoding="utf-8"
    )
    implementation = (ROOT / "src" / "core" / "FaultTracer.cpp").read_text(
        encoding="utf-8"
    )
    main_cpp = (ROOT / "src" / "app" / "main.cpp").read_text(
        encoding="utf-8"
    )

    assert "bool branchAttributionCandidateGroups = false;" in header
    assert "--branch-attribution-candidate-groups" in main_cpp
    assert "--no-branch-attribution-candidate-groups" in main_cpp
    assert "computeCandidateRootGrouping" in implementation
    root_block = implementation[
        implementation.index("if (options.rootAntiMerge") :
        implementation.index("if (options.localAntiMerge)")
    ]
    assert "branchAttributionCandidateGroups" in root_block
    assert "computeCandidateRootGrouping" in root_block
