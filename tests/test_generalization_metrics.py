# -*- coding: utf-8 -*-
import csv
import sys
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from generalization_validation.metrics import (
    _rasterize_geometry,
    connected_component_masks,
    evaluate_prediction_csv,
)
from shapely import wkt as shapely_wkt


def _write_csv(path, wkts):
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=["id", "status", "wkt"])
        writer.writeheader()
        for index, wkt in enumerate(wkts, start=1):
            writer.writerow({"id": index, "status": "kept", "wkt": wkt})


def test_perfect_polygon_prediction_scores_near_one(tmp_path):
    truth = np.zeros((32, 40), dtype=bool)
    truth[8:20, 10:28] = True
    objects = truth[None, :, :]
    csv_path = tmp_path / "result.csv"
    _write_csv(csv_path, ["POLYGON((10 8,28 8,28 20,10 20,10 8))"])

    result = evaluate_prediction_csv(csv_path, objects, boundary_tolerance=1.5)
    assert result["pixel"]["iou"] > 0.99
    assert result["boundary"]["f1"] > 0.99
    assert result["objects"]["merge_count"] == 0
    assert result["objects"]["split_count"] == 0
    assert result["topology"]["invalid_polygon_count"] == 0


def test_object_metrics_detect_merge(tmp_path):
    objects = np.zeros((2, 30, 48), dtype=bool)
    objects[0, 8:18, 5:17] = True
    objects[1, 8:18, 29:41] = True
    csv_path = tmp_path / "merged.csv"
    _write_csv(csv_path, ["POLYGON((5 8,41 8,41 18,5 18,5 8))"])

    result = evaluate_prediction_csv(csv_path, objects, boundary_tolerance=1.5)
    assert result["objects"]["merge_count"] == 1
    assert result["objects"]["reference_object_count"] == 2
    assert result["objects"]["predicted_object_count"] == 1


def test_rasterization_clips_to_geometry_bounds_without_changing_cells():
    geometry = shapely_wkt.loads("POLYGON((10 8,28 8,28 20,10 20,10 8))")
    mask = _rasterize_geometry(geometry, (300, 500))

    assert mask.shape == (300, 500)
    assert int(mask.sum()) == 12 * 18
    assert mask[8:20, 10:28].all()
    assert not mask[:8].any()
    assert not mask[:, :10].any()


def test_centerline_corridor_metrics_distinguish_coverage_from_band_width(tmp_path):
    centerline = np.zeros((40, 60), dtype=bool)
    centerline[20, 10:50] = True
    objects = centerline[None, :, :]
    csv_path = tmp_path / "band.csv"
    _write_csv(csv_path, ["POLYGON((10 17,50 17,50 24,10 24,10 17))"])

    result = evaluate_prediction_csv(
        csv_path,
        objects,
        boundary_tolerance=2.0,
        reference_centerline=centerline,
        centerline_tolerance=3.0,
    )

    assert result["centerline"]["coverage"] == 1.0
    assert result["centerline"]["corridor_precision"] > 0.99
    assert result["centerline"]["corridor_f1"] > 0.99


def test_connected_component_masks_keep_separate_reference_objects():
    truth = np.zeros((30, 40), dtype=bool)
    truth[5:10, 6] = True
    truth[18:24, 31] = True
    truth[2, 2] = True

    masks = connected_component_masks(truth, minimum_cells=3)

    assert masks.shape == (2, 30, 40)
    assert sorted(int(mask.sum()) for mask in masks) == [5, 6]
