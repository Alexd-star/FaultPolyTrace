import csv
from pathlib import Path
import sys

import numpy as np
from scipy.ndimage import binary_erosion, distance_transform_edt, label
from shapely import contains_xy, prepare
from shapely import wkt as shapely_wkt


try:
    csv.field_size_limit(min(sys.maxsize, 2**31 - 1))
except OverflowError:
    csv.field_size_limit(2**31 - 1)


def _safe_ratio(numerator, denominator):
    return float(numerator / denominator) if denominator else (1.0 if numerator == 0 else 0.0)


def connected_component_masks(binary, minimum_cells=3):
    binary = np.asarray(binary, dtype=bool)
    if binary.ndim != 2:
        raise ValueError("binary reference must be a two-dimensional mask.")
    components, count = label(binary, structure=np.ones((3, 3), dtype=np.uint8))
    masks = []
    for component_id in range(1, count + 1):
        mask = components == component_id
        if int(mask.sum()) >= int(minimum_cells):
            masks.append(mask)
    if not masks:
        return np.empty((0, *binary.shape), dtype=bool)
    return np.stack(masks).astype(bool)


def _read_prediction_geometries(csv_path):
    geometries = []
    with Path(csv_path).open("r", encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            if row.get("status", "kept").strip().lower() not in {"", "kept"}:
                continue
            text = row.get("wkt", "").strip()
            if text:
                geometries.append(shapely_wkt.loads(text))
    return geometries


def _rasterize_geometry(geometry, shape):
    height, width = shape
    if geometry.is_empty:
        return np.zeros(shape, dtype=bool)
    min_x, min_y, max_x, max_y = geometry.bounds
    x0 = max(0, int(np.floor(min_x)))
    y0 = max(0, int(np.floor(min_y)))
    x1 = min(width, int(np.ceil(max_x)))
    y1 = min(height, int(np.ceil(max_y)))
    result = np.zeros(shape, dtype=bool)
    if x0 >= x1 or y0 >= y1:
        return result
    xs = np.arange(x0, x1, dtype=np.float64) + 0.5
    ys = np.arange(y0, y1, dtype=np.float64) + 0.5
    xx, yy = np.meshgrid(xs, ys)
    prepare(geometry)
    result[y0:y1, x0:x1] = np.asarray(contains_xy(geometry, xx, yy), dtype=bool)
    return result


def _boundary(mask):
    mask = np.asarray(mask, dtype=bool)
    if not mask.any():
        return mask.copy()
    return mask & ~binary_erosion(mask, structure=np.ones((3, 3), dtype=bool), border_value=0)


def _boundary_metrics(predicted, truth, tolerance):
    pred_boundary = _boundary(predicted)
    truth_boundary = _boundary(truth)
    pred_count = int(pred_boundary.sum())
    truth_count = int(truth_boundary.sum())
    if pred_count == 0 or truth_count == 0:
        perfect = pred_count == truth_count
        return {"precision": float(perfect), "recall": float(perfect), "f1": float(perfect), "assd": 0.0 if perfect else None, "hd95": 0.0 if perfect else None}
    distance_to_truth = distance_transform_edt(~truth_boundary)
    distance_to_pred = distance_transform_edt(~pred_boundary)
    precision = float(np.mean(distance_to_truth[pred_boundary] <= tolerance))
    recall = float(np.mean(distance_to_pred[truth_boundary] <= tolerance))
    f1 = _safe_ratio(2.0 * precision * recall, precision + recall)
    pred_distances = distance_to_truth[pred_boundary]
    truth_distances = distance_to_pred[truth_boundary]
    all_distances = np.concatenate([pred_distances, truth_distances])
    return {
        "precision": precision,
        "recall": recall,
        "f1": f1,
        "assd": float(all_distances.mean()),
        "hd95": float(np.percentile(all_distances, 95.0)),
    }


def _object_metrics(predicted_masks, reference_masks):
    overlap = np.zeros((len(predicted_masks), len(reference_masks)), dtype=np.int64)
    if len(predicted_masks):
        prediction_labels = np.zeros(reference_masks[0].shape, dtype=np.int32)
        for pred_index, predicted in enumerate(predicted_masks, start=1):
            prediction_labels[np.asarray(predicted, dtype=bool)] = pred_index
        for truth_index, truth in enumerate(reference_masks):
            counts = np.bincount(
                prediction_labels[np.asarray(truth, dtype=bool)],
                minlength=len(predicted_masks) + 1,
            )
            overlap[:, truth_index] = counts[1 : len(predicted_masks) + 1]
    merge_count = 0
    merge_excess = 0
    max_references_per_prediction = 0
    for pred_index, predicted in enumerate(predicted_masks):
        linked = 0
        for truth_index, truth in enumerate(reference_masks):
            minimum = max(1, int(0.05 * np.count_nonzero(truth)))
            linked += int(overlap[pred_index, truth_index] >= minimum)
        merge_count += int(linked > 1)
        merge_excess += max(0, linked - 1)
        max_references_per_prediction = max(max_references_per_prediction, linked)
    split_count = 0
    split_excess = 0
    max_predictions_per_reference = 0
    detected_count = 0
    for truth_index, truth in enumerate(reference_masks):
        minimum = max(1, int(0.05 * np.count_nonzero(truth)))
        linked = int(np.count_nonzero(overlap[:, truth_index] >= minimum)) if len(predicted_masks) else 0
        split_count += int(linked > 1)
        split_excess += max(0, linked - 1)
        max_predictions_per_reference = max(max_predictions_per_reference, linked)
        detected_count += int(linked > 0)
    return {
        "reference_object_count": int(len(reference_masks)),
        "predicted_object_count": int(len(predicted_masks)),
        "detected_reference_count": int(detected_count),
        "missed_reference_count": int(len(reference_masks) - detected_count),
        "merge_count": int(merge_count),
        "merge_excess": int(merge_excess),
        "max_references_per_prediction": int(max_references_per_prediction),
        "split_count": int(split_count),
        "split_excess": int(split_excess),
        "max_predictions_per_reference": int(max_predictions_per_reference),
    }


def prediction_masks_from_csv(csv_path, shape):
    geometries = _read_prediction_geometries(csv_path)
    return geometries, [_rasterize_geometry(geometry, shape) for geometry in geometries]


def evaluate_prediction_csv(
    csv_path,
    reference_object_masks,
    boundary_tolerance=2.0,
    reference_centerline=None,
    centerline_tolerance=3.0,
):
    reference_masks = np.asarray(reference_object_masks, dtype=bool)
    if reference_masks.ndim != 3 or reference_masks.shape[0] == 0:
        raise ValueError("reference_object_masks must have shape (objects, height, width).")
    shape = reference_masks.shape[1:]
    geometries, predicted_masks = prediction_masks_from_csv(csv_path, shape)
    predicted = np.any(np.stack(predicted_masks), axis=0) if predicted_masks else np.zeros(shape, dtype=bool)
    truth = np.any(reference_masks, axis=0)
    true_positive = int(np.count_nonzero(predicted & truth))
    false_positive = int(np.count_nonzero(predicted & ~truth))
    false_negative = int(np.count_nonzero(~predicted & truth))
    precision = _safe_ratio(true_positive, true_positive + false_positive)
    recall = _safe_ratio(true_positive, true_positive + false_negative)
    pixel = {
        "precision": precision,
        "recall": recall,
        "f1": _safe_ratio(2.0 * precision * recall, precision + recall),
        "iou": _safe_ratio(true_positive, true_positive + false_positive + false_negative),
        "true_positive_cells": true_positive,
        "false_positive_cells": false_positive,
        "false_negative_cells": false_negative,
    }
    invalid = sum(int(not geometry.is_valid or geometry.is_empty) for geometry in geometries)
    intersections = 0
    for first in range(len(geometries)):
        for second in range(first + 1, len(geometries)):
            if geometries[first].intersection(geometries[second]).area > 1e-9:
                intersections += 1
    result = {
        "pixel": pixel,
        "boundary": _boundary_metrics(predicted, truth, float(boundary_tolerance)),
        "objects": _object_metrics(predicted_masks, reference_masks),
        "topology": {
            "invalid_polygon_count": int(invalid),
            "polygon_intersection_count": int(intersections),
        },
    }
    if reference_centerline is not None:
        centerline = np.asarray(reference_centerline, dtype=bool)
        if centerline.shape != shape:
            raise ValueError("reference_centerline shape does not match object masks.")
        coverage = float(np.mean(predicted[centerline])) if centerline.any() else 1.0
        corridor = distance_transform_edt(~centerline) <= float(centerline_tolerance)
        corridor_precision = (
            float(np.mean(corridor[predicted])) if predicted.any() else float(not centerline.any())
        )
        result["centerline"] = {
            "coverage": coverage,
            "corridor_precision": corridor_precision,
            "corridor_f1": _safe_ratio(
                2.0 * corridor_precision * coverage,
                corridor_precision + coverage,
            ),
            "corridor_tolerance_cells": float(centerline_tolerance),
            "reference_cells": int(centerline.sum()),
        }
    result["pixel"]["background_leakage_ratio"] = _safe_ratio(false_positive, int(predicted.sum()))
    return result
