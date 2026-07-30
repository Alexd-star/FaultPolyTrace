# -*- coding: utf-8 -*-
import json
import sys
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from generalization_validation.synthetic import FAMILIES, generate_case, save_case
from generalization_validation.complex_synthetic import generate_complex_case
from generalization_validation.metrics import _object_metrics


def test_all_benchmark_families_generate_independent_truth():
    for index, family in enumerate(FAMILIES):
        case = generate_case(width=160, height=112, seed=100 + index, family=family)
        assert case.attribute.shape == (112, 160)
        assert case.centerline_mask.shape == case.attribute.shape
        assert case.object_masks.ndim == 3
        assert case.object_masks.shape[1:] == case.attribute.shape
        assert np.isfinite(case.attribute).all()
        assert 0.0 <= float(case.attribute.min()) <= float(case.attribute.max()) <= 1.0
        assert int(case.centerline_mask.sum()) > 20
        assert int(case.object_masks.sum()) > int(case.centerline_mask.sum())
        assert case.metadata["family"] == family
        assert case.metadata["truth_source"] == "generator_geometry_before_attribute_rendering"


def test_generation_is_deterministic_and_semisynthetic_background_is_recorded(tmp_path):
    first = generate_case(144, 96, seed=42, family="weak_gap")
    second = generate_case(144, 96, seed=42, family="weak_gap")
    assert np.array_equal(first.attribute, second.attribute)
    assert np.array_equal(first.object_masks, second.object_masks)

    background = np.linspace(0.0, 1.0, 144 * 96, dtype=np.float32).reshape(96, 144)
    semi = generate_case(144, 96, seed=42, family="weak_gap", background=background)
    assert not np.array_equal(first.attribute, semi.attribute)
    assert semi.metadata["background_mode"] == "external_real_or_user_supplied"

    case_dir = save_case(semi, tmp_path / "case_0001")
    assert (case_dir / "attribute.txt").exists()
    assert (case_dir / "truth_masks.npz").exists()
    assert (case_dir / "truth.geojson").exists()
    metadata = json.loads((case_dir / "metadata.json").read_text(encoding="utf-8"))
    assert metadata["seed"] == 42


def test_official_like_complex_case_is_sparse_deterministic_and_has_independent_truth():
    first = generate_complex_case(600, 300, seed=20260732)
    second = generate_complex_case(600, 300, seed=20260732)

    assert np.array_equal(first.attribute, second.attribute)
    assert np.array_equal(first.object_masks, second.object_masks)
    assert first.object_masks.shape[0] == 26
    assert first.metadata["dataset_specific_rule"] is False
    assert first.metadata["truth_source"] == "geometry_is_generated_before_attribute_rendering"

    calibrated = generate_complex_case(1501, 701, seed=20260732)
    positive_ratio = float(np.mean(calibrated.attribute > 0.0))
    truth_ratio = float(np.mean(np.any(calibrated.object_masks, axis=0)))
    assert 0.02 <= positive_ratio <= 0.08
    assert 0.08 <= truth_ratio <= 0.30


def test_object_metrics_report_merge_and_split_severity_not_only_event_count():
    reference = np.zeros((3, 20, 20), dtype=bool)
    reference[0, 2:6, 2:6] = True
    reference[1, 2:6, 8:12] = True
    reference[2, 2:6, 14:18] = True
    merged = np.any(reference, axis=0)

    metrics = _object_metrics([merged], reference)

    assert metrics["merge_count"] == 1
    assert metrics["merge_excess"] == 2
    assert metrics["max_references_per_prediction"] == 3
