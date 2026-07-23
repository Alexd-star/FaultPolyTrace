# -*- coding: utf-8 -*-
import sys
import json
from pathlib import Path

import pytest
from shapely.geometry import Polygon, box


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from 参数扰动稳定性验证 import (
    classify_three_way_consensus,
    compare_geometry_sets,
    main,
    write_outputs,
)


def test_identical_geometry_sets_are_stable():
    reference = [box(0, 0, 10, 2), box(20, 0, 30, 2)]
    candidate = [box(0, 0, 10, 2), box(20, 0, 30, 2)]

    result = compare_geometry_sets(reference, candidate)

    assert result["stable"] is True
    assert result["matched_reference_ratio"] == 1.0
    assert result["mean_matched_iou"] == 1.0
    assert result["split_reference_count"] == 0
    assert result["merged_candidate_count"] == 0


def test_split_and_merge_are_counted_separately():
    one_reference = [box(0, 0, 20, 4)]
    split_candidate = [box(0, 0, 9, 4), box(11, 0, 20, 4)]
    split_result = compare_geometry_sets(one_reference, split_candidate, overlap_fraction=0.20)

    assert split_result["split_reference_count"] == 1
    assert split_result["merged_candidate_count"] == 0
    assert split_result["stable"] is False

    two_references = [box(0, 0, 9, 4), box(11, 0, 20, 4)]
    merged_candidate = [box(0, 0, 20, 4)]
    merge_result = compare_geometry_sets(two_references, merged_candidate, overlap_fraction=0.20)

    assert merge_result["split_reference_count"] == 0
    assert merge_result["merged_candidate_count"] == 1
    assert merge_result["stable"] is False


def test_small_boundary_perturbation_remains_stable():
    reference = [box(0, 0, 100, 10)]
    candidate = [box(0.5, -0.2, 100.5, 10.2)]

    result = compare_geometry_sets(
        reference,
        candidate,
        min_match_iou=0.75,
        min_mean_iou=0.90,
    )

    assert result["stable"] is True
    assert result["mean_matched_iou"] > 0.90
    assert result["p95_relative_area_change"] < 0.10
    assert result["p95_normalized_boundary_hausdorff"] < 0.02


def test_thin_long_boundary_spike_fails_boundary_stability_gate():
    reference = [box(0, 0, 100, 10)]
    candidate = [
        Polygon(
            [
                (0, 0),
                (100, 0),
                (100, 4.95),
                (1000, 5),
                (100, 5.05),
                (100, 10),
                (0, 10),
                (0, 0),
            ]
        )
    ]

    result = compare_geometry_sets(reference, candidate)

    assert result["mean_matched_iou"] > 0.90
    assert result["p95_relative_area_change"] < 0.10
    assert result["p95_normalized_boundary_hausdorff"] > 0.50
    assert result["stable"] is False


def test_single_catastrophic_change_is_not_hidden_by_p95():
    reference = [box(index * 30, 0, index * 30 + 10, 2) for index in range(20)]
    candidate = list(reference)
    candidate[0] = box(0, 0, 20, 2)

    result = compare_geometry_sets(reference, candidate)

    assert result["p95_relative_area_change"] < 0.10
    assert result["p95_normalized_boundary_hausdorff"] < 0.10
    assert result["max_relative_area_change"] >= 1.0
    assert result["max_normalized_boundary_hausdorff"] > 0.45
    assert result["stable"] is False


def test_three_way_consensus_marks_common_and_variant_only_objects():
    balanced = [box(0, 0, 10, 2), box(20, 0, 30, 2), box(40, 0, 50, 2)]
    conservative = [box(0, 0, 10, 2), box(20, 0, 30, 2)]
    high_recall = [
        box(0, 0, 10, 2),
        box(20, 0, 30, 2),
        box(40, 0, 50, 2),
        box(60, 0, 70, 2),
    ]

    consensus = classify_three_way_consensus(
        balanced,
        conservative,
        high_recall,
        min_match_iou=0.75,
    )

    assert consensus["balanced_object_count"] == 3
    assert consensus["high_confidence_count"] == 2
    assert consensus["balanced_review_count"] == 1
    assert consensus["high_recall_only_count"] == 1


def test_three_way_consensus_requires_cross_preset_object_overlap():
    balanced = [box(0, 0, 10, 2)]
    conservative = [box(0, 0, 6, 2)]
    high_recall = [box(4, 0, 10, 2)]

    consensus = classify_three_way_consensus(
        balanced,
        conservative,
        high_recall,
        min_match_iou=0.50,
    )

    assert consensus["high_confidence_count"] == 0
    assert consensus["balanced_review_count"] == 1


def test_three_way_consensus_rejects_many_to_one_merge_as_high_confidence():
    balanced = [box(0, 0, 10, 2), box(10.5, 0, 20.5, 2)]
    merged = [box(0, 0, 20.5, 2)]

    consensus = classify_three_way_consensus(
        balanced,
        merged,
        merged,
        min_match_iou=0.40,
        overlap_fraction=0.20,
    )

    assert consensus["high_confidence_count"] == 0
    assert consensus["balanced_review_count"] == 2
    assert consensus["ambiguous_balanced_count"] == 2


def test_merge_detection_uses_reference_or_candidate_overlap_fraction():
    balanced = [box(index * 12, 0, index * 12 + 10, 2) for index in range(5)]
    merged = [box(0, 0, 58, 2)]

    comparison = compare_geometry_sets(
        balanced,
        merged,
        min_match_iou=0.10,
        overlap_fraction=0.20,
    )
    consensus = classify_three_way_consensus(
        balanced,
        merged,
        merged,
        min_match_iou=0.10,
        overlap_fraction=0.20,
    )

    assert comparison["merged_candidate_count"] == 1
    assert consensus["high_confidence_count"] == 0
    assert consensus["ambiguous_balanced_count"] == 5


def test_split_fragments_are_not_reported_as_preset_only_objects():
    balanced = [box(0, 0, 20, 2)]
    conservative = list(balanced)
    high_recall = [box(0, 0, 9, 2), box(11, 0, 20, 2)]

    consensus = classify_three_way_consensus(
        balanced,
        conservative,
        high_recall,
        min_match_iou=0.40,
        overlap_fraction=0.20,
    )

    assert consensus["high_recall_only_count"] == 0
    assert consensus["high_recall_ambiguous_fragment_count"] == 1


def test_cli_requires_at_least_one_candidate(tmp_path):
    with pytest.raises(SystemExit) as error:
        main(
            [
                "--baseline",
                f"balanced={tmp_path / 'baseline.csv'}",
                "--out-dir",
                str(tmp_path / "report"),
            ]
        )

    assert error.value.code == 2


def test_cli_rejects_incomplete_three_preset_arguments(tmp_path):
    with pytest.raises(SystemExit) as error:
        main(
            [
                "--baseline",
                f"balanced={tmp_path / 'baseline.csv'}",
                "--candidate",
                f"variant={tmp_path / 'variant.csv'}",
                "--conservative",
                str(tmp_path / "conservative.csv"),
                "--out-dir",
                str(tmp_path / "report"),
            ]
        )

    assert error.value.code == 2


def test_invalid_wkt_generates_failure_evidence_files(tmp_path):
    baseline = tmp_path / "baseline.csv"
    candidate = tmp_path / "candidate.csv"
    out_dir = tmp_path / "report"
    baseline.write_text(
        'id,status,wkt\n1,kept,"POLYGON((0 0,10 0,10 2,0 2,0 0))"\n',
        encoding="utf-8",
    )
    candidate.write_text(
        'id,status,wkt\n1,kept,"NOT_A_GEOMETRY"\n',
        encoding="utf-8",
    )

    exit_code = main(
        [
            "--baseline",
            f"balanced={baseline}",
            "--candidate",
            f"broken={candidate}",
            "--out-dir",
            str(out_dir),
        ]
    )

    assert exit_code == 3
    summary_path = out_dir / "参数扰动稳定性摘要.json"
    detail_path = out_dir / "参数扰动稳定性明细.csv"
    report_path = out_dir / "参数扰动稳定性报告.md"
    assert summary_path.is_file()
    assert detail_path.is_file()
    assert report_path.is_file()
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    assert summary["all_stable"] is False
    assert "error" in summary


def test_report_explains_candidate_side_failure_and_lists_object_keys(tmp_path):
    reference = [box(0, 0, 10, 2)]
    candidate = [box(0, 0, 10, 2), box(30, 0, 40, 2)]
    metrics = compare_geometry_sets(
        reference,
        candidate,
        reference_keys=["baseline-1"],
        candidate_keys=["variant-1", "variant-extra"],
    )

    write_outputs(
        tmp_path,
        "balanced",
        [{"name": "variant", "metrics": metrics}],
    )
    report = (tmp_path / "参数扰动稳定性报告.md").read_text(encoding="utf-8")

    assert "候选匹配率" in report
    assert "candidate_match_ratio" in metrics["failure_reasons"]
    assert "variant-extra" in metrics["unmatched_candidate_keys"]
    assert "variant-extra" in report
