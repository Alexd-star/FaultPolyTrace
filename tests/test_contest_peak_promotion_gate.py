# -*- coding: utf-8 -*-
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from 冲顶实验晋级验收 import decide_promotion


def test_promotion_passes_when_candidate_improves_shape_without_regression():
    baseline = {
        "strong_coverage": 0.99846,
        "weak_coverage": 0.97149,
        "kept_polygons": 214,
        "open_rings": 0,
        "self_intersections": 0,
        "polygon_intersections": 0,
        "local_shape_candidates": 29,
        "high_priority_local_shape_candidates": 3,
    }
    candidate = {
        "strong_coverage": 0.99846,
        "weak_coverage": 0.97149,
        "kept_polygons": 228,
        "open_rings": 0,
        "self_intersections": 0,
        "polygon_intersections": 0,
        "local_shape_candidates": 8,
        "high_priority_local_shape_candidates": 0,
    }

    decision = decide_promotion(baseline, candidate)

    assert decision["pass"] is True
    assert decision["summary"]["high_priority_local_shape_delta"] == -3
    assert decision["summary"]["local_shape_candidate_delta"] == -21
    assert all(item["pass"] for item in decision["checks"])


def test_promotion_rejects_coverage_regression_even_if_shape_improves():
    baseline = {
        "strong_coverage": 0.99846,
        "weak_coverage": 0.97149,
        "kept_polygons": 214,
        "open_rings": 0,
        "self_intersections": 0,
        "polygon_intersections": 0,
        "local_shape_candidates": 29,
        "high_priority_local_shape_candidates": 3,
    }
    candidate = {
        "strong_coverage": 0.99000,
        "weak_coverage": 0.96000,
        "kept_polygons": 225,
        "open_rings": 0,
        "self_intersections": 0,
        "polygon_intersections": 0,
        "local_shape_candidates": 8,
        "high_priority_local_shape_candidates": 0,
    }

    decision = decide_promotion(baseline, candidate)

    assert decision["pass"] is False
    failed = {item["name"] for item in decision["checks"] if not item["pass"]}
    assert "强响应覆盖率不退化" in failed
    assert "弱响应覆盖率不退化" in failed


def test_promotion_rejects_topology_or_high_priority_shape_risk():
    baseline = {
        "strong_coverage": 0.99846,
        "weak_coverage": 0.97149,
        "kept_polygons": 214,
        "open_rings": 0,
        "self_intersections": 0,
        "polygon_intersections": 0,
        "local_shape_candidates": 29,
        "high_priority_local_shape_candidates": 3,
    }
    candidate = {
        "strong_coverage": 0.99846,
        "weak_coverage": 0.97149,
        "kept_polygons": 228,
        "open_rings": 0,
        "self_intersections": 1,
        "polygon_intersections": 0,
        "local_shape_candidates": 8,
        "high_priority_local_shape_candidates": 4,
    }

    decision = decide_promotion(baseline, candidate)

    assert decision["pass"] is False
    failed = {item["name"] for item in decision["checks"] if not item["pass"]}
    assert "拓扑零问题" in failed
    assert "高优先级局部形态候选不增加" in failed
