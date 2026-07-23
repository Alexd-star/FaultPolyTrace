# -*- coding: utf-8 -*-
import sys
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from 防黏连差异诊断 import (
    candidate_ids_for_review,
    is_changed_mapping_row,
    is_safe_split,
    read_polygons,
    summarize_global_mask_change,
    summarize_mapping,
    summarize_merge_relations,
)
from 综合质量评分 import score_split_benefit


def _item(object_id, mask):
    return {
        "id": object_id,
        "mask": np.asarray(mask, dtype=bool),
        "area": float(np.count_nonzero(mask)),
        "raster_cells": int(np.count_nonzero(mask)),
        "value_mean_raster": 1.0,
        "high_value_ratio": 1.0,
    }


def test_one_cell_boundary_touch_is_not_counted_as_split_part():
    base_mask = np.zeros((8, 12), dtype=bool)
    base_mask[2:6, 1:7] = True

    matching_mask = base_mask.copy()
    touching_mask = np.zeros_like(base_mask)
    touching_mask[5:8, 6:12] = True

    rows, source_rows, unmatched = summarize_mapping(
        [_item(1, base_mask)],
        [_item(10, matching_mask), _item(11, touching_mask)],
        np.ones_like(base_mask, dtype=float),
    )

    assert rows[0]["candidate_parts"] == 1
    assert rows[0]["top_candidate_ids"] == "10"
    assert [row["candidate_id"] for row in source_rows] == [10]
    assert [row["candidate_id"] for row in unmatched] == [11]


def test_two_substantial_fragments_are_counted_as_true_split():
    base_mask = np.zeros((6, 12), dtype=bool)
    base_mask[1:5, 1:11] = True

    left_mask = np.zeros_like(base_mask)
    left_mask[1:5, 1:6] = True
    right_mask = np.zeros_like(base_mask)
    right_mask[1:5, 6:11] = True

    rows, source_rows, unmatched = summarize_mapping(
        [_item(1, base_mask)],
        [_item(10, left_mask), _item(11, right_mask)],
        np.ones_like(base_mask, dtype=float),
    )

    assert rows[0]["candidate_parts"] == 2
    assert rows[0]["top_candidate_ids"] == "10;11"
    assert len(source_rows) == 2
    assert unmatched == []


def test_candidate_additions_are_reported_separately_from_baseline_losses():
    base_mask = np.zeros((6, 8), dtype=bool)
    base_mask[2:4, 2:6] = True
    expanded_mask = base_mask.copy()
    expanded_mask[1, 3] = True

    values = np.zeros_like(base_mask, dtype=float)
    values[base_mask] = 0.95

    rows, _, _ = summarize_mapping(
        [_item(1, base_mask)],
        [_item(10, expanded_mask)],
        values,
    )

    assert rows[0]["lost_cells"] == 0
    assert rows[0]["added_cells"] == 1
    assert rows[0]["added_mean"] == 0.0
    assert rows[0]["added_weak_cells"] == 0
    assert rows[0]["added_strong_cells"] == 0
    assert rows[0]["risk_hint"] == "需复核：新增低值背景包络"

    summary = summarize_global_mask_change(
        [_item(1, base_mask)],
        [_item(10, expanded_mask)],
        values,
    )
    assert summary["added_cells_total"] == 1
    assert summary["lost_cells_total"] == 0


def test_split_with_background_additions_is_not_classified_as_safe():
    row = {
        "candidate_parts": 2,
        "lost_strong_cells": 0,
        "lost_weak_cells": 0,
        "added_cells": 3,
    }

    assert is_safe_split(row) is False


def test_tiny_contained_fragment_is_not_a_substantial_split_part():
    base_mask = np.zeros((10, 20), dtype=bool)
    base_mask[2:8, 2:18] = True
    main_mask = base_mask.copy()
    tiny_mask = np.zeros_like(base_mask)
    tiny_mask[3:5, 3:5] = True

    rows, _, _ = summarize_mapping(
        [_item(1, base_mask)],
        [_item(10, main_mask), _item(11, tiny_mask)],
        np.ones_like(base_mask, dtype=float),
    )

    assert rows[0]["linked_candidate_count"] == 2
    assert rows[0]["candidate_parts"] == 1
    assert rows[0]["top_candidate_ids"] == "10"


def test_many_small_fragments_are_reported_as_overfragmentation():
    base_mask = np.ones((10, 10), dtype=bool)
    fragments = []
    for index in range(10):
        mask = np.zeros_like(base_mask)
        mask[index, :] = True
        fragments.append(_item(100 + index, mask))

    rows, _, _ = summarize_mapping(
        [_item(1, base_mask)],
        fragments,
        np.ones_like(base_mask, dtype=float),
    )

    row = rows[0]
    assert row["linked_candidate_count"] == 10
    assert row["candidate_parts"] == 0
    assert row["risk_hint"] == "需复核：存在多个小碎片"
    assert is_changed_mapping_row(row) is True
    assert is_safe_split(row) is False


def test_two_main_parts_with_small_fragments_are_not_safe():
    row = {
        "linked_candidate_count": 6,
        "candidate_parts": 2,
        "covered_ratio": 1.0,
        "min_base_part_ratio": 0.40,
        "lost_cells": 0,
        "lost_mean": 0.0,
        "lost_strong_cells": 0,
        "lost_weak_cells": 0,
        "added_cells": 0,
        "merged_candidate_parts": 0,
    }

    assert is_safe_split(row) is False


def test_review_uses_all_linked_candidates_when_small_fragments_exist():
    row = {
        "top_candidate_ids": "10;11",
        "linked_candidate_ids": "10;11;12;13",
    }

    assert candidate_ids_for_review(row) == [10, 11, 12, 13]


def test_low_coverage_or_overfragmented_split_is_not_safe():
    low_coverage = {
        "candidate_parts": 2,
        "covered_ratio": 0.10,
        "min_base_part_ratio": 0.05,
        "lost_cells": 90,
        "lost_mean": 0.0,
        "lost_strong_cells": 0,
        "lost_weak_cells": 0,
        "added_cells": 0,
        "merged_candidate_parts": 0,
    }
    overfragmented = dict(low_coverage)
    overfragmented.update(
        candidate_parts=5,
        covered_ratio=1.0,
        min_base_part_ratio=0.18,
        lost_cells=0,
    )

    assert is_safe_split(low_coverage) is False
    assert is_safe_split(overfragmented) is False


def test_many_to_one_merge_is_counted_without_false_added_envelope():
    left = np.zeros((6, 12), dtype=bool)
    left[1:5, 1:5] = True
    right = np.zeros_like(left)
    right[1:5, 7:11] = True
    merged = left | right

    rows, source_rows, _ = summarize_mapping(
        [_item(1, left), _item(2, right)],
        [_item(10, merged)],
        np.ones_like(left, dtype=float),
    )

    assert len(source_rows) == 2
    assert all(row["added_cells"] == 0 for row in rows)
    assert all(row["merged_candidate_parts"] == 1 for row in rows)
    assert all(is_safe_split(row) is False for row in rows)
    assert all(row["risk_hint"] == "高风险：候选合并多个基线对象" for row in rows)
    assert all(is_changed_mapping_row(row) is True for row in rows)
    merge_summary = summarize_merge_relations(source_rows)
    assert merge_summary["merged_candidate_count"] == 1
    assert merge_summary["merged_candidate_ids"] == [10]


def test_merge_count_is_penalized_by_quality_score():
    score, notes = score_split_benefit({"merged_candidate_count": 5})

    assert score < 0.0
    assert any("合并" in note for note in notes)


def test_overlap_fraction_outside_unit_interval_is_rejected():
    mask = np.ones((2, 2), dtype=bool)

    try:
        summarize_mapping(
            [_item(1, mask)],
            [_item(2, mask)],
            np.ones_like(mask, dtype=float),
            overlap_fraction=1.1,
        )
    except ValueError as error:
        assert "overlap_fraction" in str(error)
    else:
        raise AssertionError("Invalid overlap_fraction must be rejected.")


def test_duplicate_polygon_ids_are_rejected(tmp_path):
    csv_path = tmp_path / "duplicate.csv"
    csv_path.write_text(
        "id,status,area,perimeter,cell_count,mean_value,max_value,wkt\n"
        '7,kept,1,4,1,1,1,"POLYGON((0 0,1 0,1 1,0 1,0 0))"\n'
        '7,kept,1,4,1,1,1,"POLYGON((2 0,3 0,3 1,2 1,2 0))"\n',
        encoding="utf-8",
    )

    try:
        read_polygons(csv_path)
    except ValueError as error:
        assert "duplicate polygon id" in str(error).lower()
    else:
        raise AssertionError("Duplicate polygon ids must be rejected.")
