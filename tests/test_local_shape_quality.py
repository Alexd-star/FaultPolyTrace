# -*- coding: utf-8 -*-
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from 局部形态质量诊断 import (
    apply_polygon_context,
    build_summary,
    classify_inner_ring_context,
    detect_local_bulges,
)


def test_detects_local_bulge_on_slender_fault_band():
    mask = np.zeros((40, 120), dtype=bool)
    mask[18:22, 5:115] = True
    mask[12:28, 54:70] = True

    candidates = detect_local_bulges(mask, window=9, width_ratio=2.2, min_extra_cells=30)

    assert len(candidates) == 1
    candidate = candidates[0]
    assert 50 <= candidate["min_x"] <= 58
    assert 66 <= candidate["max_x"] <= 74
    assert candidate["extra_cells"] >= 100
    assert candidate["local_width"] >= 14
    assert candidate["shape_class"] == "局部鼓包"
    assert candidate["review_priority"] == "high"


def test_does_not_flag_uniform_slender_fault_band():
    mask = np.zeros((40, 120), dtype=bool)
    mask[18:22, 5:115] = True

    candidates = detect_local_bulges(mask, window=9, width_ratio=2.2, min_extra_cells=30)

    assert candidates == []


def test_long_wide_section_is_labeled_as_wide_band_review_not_pouch():
    mask = np.zeros((40, 140), dtype=bool)
    mask[18:22, 5:135] = True
    mask[14:26, 30:110] = True

    candidates = detect_local_bulges(mask, window=9, width_ratio=2.2, min_extra_cells=30)

    assert len(candidates) == 1
    candidate = candidates[0]
    assert candidate["bin_count"] >= 70
    assert candidate["shape_class"] == "长段宽带复核"
    assert candidate["review_priority"] == "medium"


def test_branch_junction_is_labeled_as_branch_intersection_review_not_pouch():
    mask = np.zeros((80, 140), dtype=bool)
    mask[38:42, 10:125] = True
    for i in range(55):
        x = 65 + i
        upper_y = 40 - int(i * 0.45)
        lower_y = 40 + int(i * 0.40)
        mask[max(0, upper_y - 2) : min(80, upper_y + 2), max(0, x - 1) : min(140, x + 2)] = True
        mask[max(0, lower_y - 2) : min(80, lower_y + 2), max(0, x - 1) : min(140, x + 2)] = True

    candidates = detect_local_bulges(mask, window=9, width_ratio=2.2, min_extra_cells=30)

    assert len(candidates) == 1
    candidate = candidates[0]
    assert candidate["shape_class"] == "分支交切复核"
    assert candidate["review_priority"] == "medium"
    assert candidate["branch_arm_count"] >= 3


def test_branch_junction_repair_profile_keeps_raw_actionable_bulge():
    mask = np.zeros((80, 140), dtype=bool)
    mask[38:42, 10:125] = True
    for i in range(55):
        x = 65 + i
        upper_y = 40 - int(i * 0.45)
        lower_y = 40 + int(i * 0.40)
        mask[max(0, upper_y - 2) : min(80, upper_y + 2), max(0, x - 1) : min(140, x + 2)] = True
        mask[max(0, lower_y - 2) : min(80, lower_y + 2), max(0, x - 1) : min(140, x + 2)] = True

    candidates = detect_local_bulges(
        mask,
        window=9,
        width_ratio=2.2,
        min_extra_cells=30,
        context_aware=False,
    )

    assert len(candidates) == 1
    candidate = candidates[0]
    assert candidate["shape_class"] == "局部鼓包"
    assert candidate["review_priority"] == "high"
    assert candidate["branch_arm_count"] >= 3


def test_two_arm_small_endpoint_widening_is_medium_review_not_high_pouch():
    mask = np.zeros((70, 150), dtype=bool)
    mask[34:38, 10:100] = True
    for i in range(42):
        x = 82 + i
        y = 36 + int(i * 0.42)
        mask[max(0, y - 2) : min(70, y + 2), max(0, x - 1) : min(150, x + 2)] = True
    mask[30:42, 78:91] = True

    candidates = detect_local_bulges(mask, window=9, width_ratio=2.2, min_extra_cells=30)

    assert len(candidates) == 1
    candidate = candidates[0]
    assert candidate["branch_arm_count"] >= 2
    assert candidate["shape_class"] == "双臂端点复核"
    assert candidate["review_priority"] == "medium"


def test_supported_inner_ring_is_medium_clamped_gap_review():
    shape_class, priority = classify_inner_ring_context(
        hole_cells=55,
        support_ratio=0.72,
        aspect_ratio=3.5,
    )

    assert shape_class == "双线夹持内环复核"
    assert priority == "medium"


def test_unsupported_large_inner_ring_remains_high_review():
    shape_class, priority = classify_inner_ring_context(
        hole_cells=55,
        support_ratio=0.18,
        aspect_ratio=3.5,
    )

    assert shape_class == "内环复核"
    assert priority == "high"


def test_build_summary_counts_classes_and_priorities():
    records = [
        {"type": "局部鼓包", "shape_class": "局部鼓包", "review_priority": "high"},
        {"type": "局部鼓包", "shape_class": "长段宽带复核", "review_priority": "medium"},
        {"type": "内环复核", "shape_class": "内环复核", "review_priority": "medium"},
    ]

    summary = build_summary(records)

    assert summary["candidate_count"] == 3
    assert summary["type_counts"]["局部鼓包"] == 2
    assert summary["shape_class_counts"]["长段宽带复核"] == 1
    assert summary["priority_counts"]["high"] == 1


def test_polygon_context_marks_large_holed_polygon_as_branch_attribution_review():
    candidate = {
        "shape_class": "局部鼓包",
        "review_priority": "high",
        "width_ratio": 8.0,
        "extra_cells": 120,
        "span_ratio": 0.18,
    }
    polygon = {
        "area": 500000.0,
        "holes": [[(0, 0), (1, 0), (1, 1), (0, 1)]],
    }

    updated = apply_polygon_context(candidate, polygon)

    assert updated["shape_class"] == "疑似分支归属错误"
    assert updated["review_priority"] == "high"
