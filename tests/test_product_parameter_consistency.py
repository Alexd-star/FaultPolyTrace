import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _text(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def _constant(name: str) -> str:
    source = _text("src/core/ProductDefaults.h")
    match = re.search(
        rf"inline constexpr (?:double|int|bool) {re.escape(name)} = ([^;]+);",
        source,
    )
    assert match, f"missing product default: {name}"
    return match.group(1).strip()


def test_all_product_surfaces_use_the_shared_profile():
    cli = _text("src/app/main.cpp")
    runner = _text("geoeast_plugin/bo_runner/main.cpp")
    shared_ui = _text("src/ui/TraceParameterModel.h")
    call = "product_defaults::applyVisibleProductionDefaults"
    assert call in cli
    assert call in runner
    assert call in shared_ui


def test_released_help_defaults_match_the_profile():
    cli = _text("src/app/main.cpp")
    expected_fragments = {
        "threshold": f"Fixed threshold value. Default: {_constant('threshold')}",
        "percentile": f"Default: {_constant('percentile')}",
        "minArea": f"Minimum polygon area. Default: {_constant('minArea').removesuffix('.0')}",
        "weakPercentile": f"Default: {_constant('weakPercentile')}",
        "beltRadius": f"Default: {_constant('beltRadius')}",
        "maxGapDistance": f"Default: {_constant('maxGapDistance')}",
        "maxGapAngle": f"Default: {_constant('maxGapAngle').removesuffix('.0')}",
    }
    for name, fragment in expected_fragments.items():
        assert fragment in cli, f"CLI help drift for {name}: {fragment}"


def test_advanced_shadow_flags_are_symmetric_in_cli_and_geoeast_runner():
    cli = _text("src/app/main.cpp")
    runner = _text("geoeast_plugin/bo_runner/main.cpp")
    flags = [
        "--skeleton-graph-v2-attribution-shadow",
        "--multi-scale-evidence-shadow",
        "--adaptive-boundary-shadow",
        "--weak-continuity-physical-metric",
        "--weak-continuity-global-matching",
    ]
    for flag in flags:
        assert flag in cli, f"CLI is missing {flag}"
        assert flag in runner, f"GeoEast runner is missing {flag}"


def test_all_weak_continuity_passes_use_the_shared_option_builder():
    tracer = _text("src/core/FaultTracer.cpp")
    assert tracer.count("makeWeakContinuityOptions(options)") >= 3
    required_assignments = [
        "usePhysicalMetric = options.weakContinuityUsePhysicalMetric",
        "useGlobalMatching = options.weakContinuityUseGlobalMatching",
        "noConnectCost = options.weakContinuityNoConnectCost",
        "maxExactMatchingEndpoints =",
    ]
    for assignment in required_assignments:
        assert assignment in tracer
