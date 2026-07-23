# -*- coding: utf-8 -*-
import sys
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from quality_evidence_metrics import evaluate_proxy_quality, evaluate_reference_quality
from 冲顶实验晋级验收 import decide_promotion


def test_proxy_quality_distinguishes_coverage_from_background_leakage():
    values = np.zeros((9, 13), dtype=float)
    values[4, 2:11] = 0.98

    narrow_mask = np.zeros_like(values, dtype=bool)
    narrow_mask[4, 2:11] = True
    wide_mask = np.zeros_like(values, dtype=bool)
    wide_mask[1:8, 1:12] = True

    narrow = evaluate_proxy_quality(values, narrow_mask)
    wide = evaluate_proxy_quality(values, wide_mask)

    assert narrow["strong_response_coverage_proxy"] == 1.0
    assert wide["strong_response_coverage_proxy"] == 1.0
    assert narrow["background_leakage_ratio_proxy"] == 0.0
    assert wide["background_leakage_ratio_proxy"] > 0.80
    assert wide["supported_inside_ratio_proxy"] < narrow["supported_inside_ratio_proxy"]


def test_proxy_quality_detects_low_support_bridge_between_supported_regions():
    values = np.zeros((11, 21), dtype=float)
    values[3:8, 1:7] = 0.98
    values[3:8, 14:20] = 0.98
    result_mask = np.zeros_like(values, dtype=bool)
    result_mask[3:8, 1:20] = True

    metrics = evaluate_proxy_quality(values, result_mask, bridge_radius=8)

    assert metrics["low_support_bridge_cell_count_proxy"] > 0
    assert metrics["low_support_bridge_ratio_proxy"] > 0.0
    assert metrics["metric_semantics"]["uses_expert_ground_truth"] is False
    assert metrics["metric_semantics"]["classification"] == "attribute_response_proxy"


def _promotion_record(**overrides):
    record = {
        "strong_coverage": 0.99846,
        "weak_coverage": 0.97149,
        "background_leakage_ratio_proxy": 0.08,
        "unsupported_inside_ratio_proxy": 0.12,
        "low_support_bridge_ratio_proxy": 0.01,
        "kept_polygons": 214,
        "open_rings": 0,
        "self_intersections": 0,
        "polygon_intersections": 0,
        "local_shape_candidates": 29,
        "high_priority_local_shape_candidates": 3,
    }
    record.update(overrides)
    return record


def test_promotion_rejects_background_leakage_regression_with_equal_coverage():
    baseline = _promotion_record()
    candidate = _promotion_record(
        kept_polygons=220,
        local_shape_candidates=8,
        high_priority_local_shape_candidates=0,
        background_leakage_ratio_proxy=0.11,
    )

    decision = decide_promotion(baseline, candidate, proxy_tolerance=1e-9)

    assert decision["pass"] is False
    failed = {item["name"] for item in decision["checks"] if not item["pass"]}
    assert "背景泄漏代理指标不退化" in failed


def test_promotion_accepts_proxy_improvement_when_other_gates_pass():
    baseline = _promotion_record()
    candidate = _promotion_record(
        kept_polygons=225,
        local_shape_candidates=8,
        high_priority_local_shape_candidates=0,
        background_leakage_ratio_proxy=0.05,
        unsupported_inside_ratio_proxy=0.09,
        low_support_bridge_ratio_proxy=0.005,
    )

    decision = decide_promotion(baseline, candidate)

    assert decision["pass"] is True
    assert decision["summary"]["background_leakage_delta_proxy"] < 0.0
    assert decision["summary"]["unsupported_inside_delta_proxy"] < 0.0
    assert decision["summary"]["low_support_bridge_delta_proxy"] < 0.0


def test_reference_quality_is_perfect_for_identical_masks():
    reference = np.zeros((32, 48), dtype=bool)
    reference[14:18, 4:44] = True

    metrics = evaluate_reference_quality(reference, reference, boundary_tolerance=1)

    for name in ("precision", "recall", "iou", "boundary_f1", "cldice"):
        assert metrics[name] == 1.0
    assert metrics["split_excess_count"] == 0
    assert metrics["merge_excess_count"] == 0
    assert metrics["metric_semantics"]["uses_reference_mask"] is True


def test_reference_quality_exposes_boundary_shift_and_background_overreach():
    reference = np.zeros((30, 50), dtype=bool)
    reference[14:17, 5:45] = True
    shifted = np.zeros_like(reference)
    shifted[15:18, 5:45] = True
    wide = np.zeros_like(reference)
    wide[9:22, 2:48] = True

    shifted_metrics = evaluate_reference_quality(reference, shifted, boundary_tolerance=1)
    wide_metrics = evaluate_reference_quality(reference, wide, boundary_tolerance=1)

    assert shifted_metrics["boundary_f1"] > 0.90
    assert shifted_metrics["iou"] < 1.0
    assert wide_metrics["recall"] == 1.0
    assert wide_metrics["precision"] < 0.30
    assert wide_metrics["boundary_f1"] < shifted_metrics["boundary_f1"]


def test_reference_quality_counts_object_splits_and_merges():
    reference = np.zeros((40, 60), dtype=bool)
    reference[8:14, 5:25] = True
    reference[25:31, 35:55] = True

    split_prediction = reference.copy()
    split_prediction[8:14, 14:16] = False
    merge_prediction = reference.copy()
    merge_prediction[12:27, 22:38] = True

    split_metrics = evaluate_reference_quality(reference, split_prediction)
    merge_metrics = evaluate_reference_quality(reference, merge_prediction)

    assert split_metrics["split_excess_count"] == 1
    assert split_metrics["merge_excess_count"] == 0
    assert merge_metrics["merge_excess_count"] == 1


def test_promotion_rejects_reference_boundary_or_object_regression_when_available():
    baseline = _promotion_record(
        reference_iou=0.92,
        reference_boundary_f1=0.90,
        reference_cldice=0.95,
        reference_split_excess_count=0,
        reference_merge_excess_count=0,
    )
    candidate = _promotion_record(
        local_shape_candidates=8,
        high_priority_local_shape_candidates=0,
        reference_iou=0.93,
        reference_boundary_f1=0.84,
        reference_cldice=0.95,
        reference_split_excess_count=0,
        reference_merge_excess_count=1,
    )

    decision = decide_promotion(baseline, candidate)

    assert decision["pass"] is False
    failed = {item["name"] for item in decision["checks"] if not item["pass"]}
    assert "参考边界 F1 不退化" in failed
    assert "参考对象误合并不增加" in failed
