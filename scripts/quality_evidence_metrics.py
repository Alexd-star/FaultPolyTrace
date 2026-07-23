# -*- coding: utf-8 -*-
"""Quality evidence metrics that do not require expert ground truth.

These metrics are attribute-response proxies. They describe how a result mask
relates to the input planar fault attribute, but they must not be reported as
expert-reference precision, recall, or geological correctness.
"""

import numpy as np
from scipy import ndimage


def _ratio(numerator, denominator):
    return float(numerator) / float(denominator) if denominator else 1.0


def _harmonic_mean(first, second, empty_is_perfect=False):
    denominator = float(first) + float(second)
    if denominator == 0.0:
        return 1.0 if empty_is_perfect else 0.0
    return 2.0 * float(first) * float(second) / denominator


def _sample_offset(mask, dy, dx):
    """Return values sampled at (y + dy, x + dx) without wraparound."""
    out = np.zeros_like(mask, dtype=bool)
    height, width = mask.shape
    src_y0 = max(0, dy)
    src_y1 = min(height, height + dy)
    src_x0 = max(0, dx)
    src_x1 = min(width, width + dx)
    dst_y0 = max(0, -dy)
    dst_y1 = min(height, height - dy)
    dst_x0 = max(0, -dx)
    dst_x1 = min(width, width - dx)
    if src_y0 < src_y1 and src_x0 < src_x1:
        out[dst_y0:dst_y1, dst_x0:dst_x1] = mask[src_y0:src_y1, src_x0:src_x1]
    return out


def low_support_bridge_mask(
    values,
    result_mask,
    support_threshold=0.66,
    background_threshold=0.05,
    bridge_radius=4,
):
    """Find low-attribute cells lying between supported result cells.

    A candidate cell is counted when it is inside the result, has a low
    attribute value, and supported result cells occur on opposite sides along
    at least one of four principal direction pairs. This is a conservative
    bridge-risk proxy, not a proof of erroneous geological merging.
    """
    values = np.asarray(values, dtype=float)
    result_mask = np.asarray(result_mask, dtype=bool)
    if values.shape != result_mask.shape:
        raise ValueError("values and result_mask must have identical shapes")

    finite = np.isfinite(values)
    inside = result_mask & finite
    supported = inside & (values >= float(support_threshold))
    low_candidate = inside & (values <= float(background_threshold))
    radius = max(0, int(bridge_radius))
    if radius <= 0 or not np.any(low_candidate):
        return np.zeros_like(result_mask, dtype=bool)

    bridge = np.zeros_like(result_mask, dtype=bool)
    for dy, dx in ((0, 1), (1, 0), (1, 1), (1, -1)):
        positive = np.zeros_like(result_mask, dtype=bool)
        negative = np.zeros_like(result_mask, dtype=bool)
        for distance in range(1, radius + 1):
            positive |= _sample_offset(supported, dy * distance, dx * distance)
            negative |= _sample_offset(supported, -dy * distance, -dx * distance)
        bridge |= positive & negative
    return low_candidate & bridge


def evaluate_proxy_quality(
    values,
    result_mask,
    strong_threshold=0.95,
    weak_threshold=0.66,
    background_threshold=0.05,
    bridge_radius=4,
):
    """Evaluate coverage, support, leakage, and bridge-risk proxy metrics."""
    values = np.asarray(values, dtype=float)
    result_mask = np.asarray(result_mask, dtype=bool)
    if values.ndim != 2 or result_mask.ndim != 2:
        raise ValueError("values and result_mask must be two-dimensional")
    if values.shape != result_mask.shape:
        raise ValueError("values and result_mask must have identical shapes")

    finite = np.isfinite(values)
    inside = result_mask & finite
    strong = finite & (values >= float(strong_threshold))
    weak = finite & (values >= float(weak_threshold))
    supported_inside = inside & weak
    unsupported_inside = inside & ~weak
    background_inside = inside & (values <= float(background_threshold))
    bridge = low_support_bridge_mask(
        values,
        result_mask,
        support_threshold=weak_threshold,
        background_threshold=background_threshold,
        bridge_radius=bridge_radius,
    )

    inside_count = int(np.count_nonzero(inside))
    metrics = {
        "strong_response_coverage_proxy": _ratio(np.count_nonzero(inside & strong), np.count_nonzero(strong)),
        "weak_response_coverage_proxy": _ratio(np.count_nonzero(inside & weak), np.count_nonzero(weak)),
        "result_cell_count": inside_count,
        "supported_inside_cell_count": int(np.count_nonzero(supported_inside)),
        "unsupported_inside_cell_count": int(np.count_nonzero(unsupported_inside)),
        "background_inside_cell_count": int(np.count_nonzero(background_inside)),
        "low_support_bridge_cell_count_proxy": int(np.count_nonzero(bridge)),
        "supported_inside_ratio_proxy": _ratio(np.count_nonzero(supported_inside), inside_count),
        "unsupported_inside_ratio_proxy": _ratio(np.count_nonzero(unsupported_inside), inside_count),
        "background_leakage_ratio_proxy": _ratio(np.count_nonzero(background_inside), inside_count),
        "low_support_bridge_ratio_proxy": _ratio(np.count_nonzero(bridge), inside_count),
        "thresholds": {
            "strong": float(strong_threshold),
            "weak": float(weak_threshold),
            "background": float(background_threshold),
            "bridge_radius": int(bridge_radius),
        },
        "metric_semantics": {
            "classification": "attribute_response_proxy",
            "uses_expert_ground_truth": False,
            "can_prove_geological_correctness": False,
            "intended_use": "candidate_comparison_and_risk_gating",
        },
    }
    return metrics


def _binary_boundary(mask):
    structure = np.ones((3, 3), dtype=bool)
    return mask & ~ndimage.binary_erosion(mask, structure=structure, border_value=0)


def _morphological_skeleton(mask):
    """Return a deterministic binary morphological skeleton."""
    structure = ndimage.generate_binary_structure(2, 1)
    current = np.asarray(mask, dtype=bool).copy()
    skeleton = np.zeros_like(current)
    while np.any(current):
        eroded = ndimage.binary_erosion(current, structure=structure)
        opened = ndimage.binary_dilation(eroded, structure=structure)
        skeleton |= current & ~opened
        current = eroded
    return skeleton


def _matched_ratio(source, target, tolerance):
    source_count = int(np.count_nonzero(source))
    if source_count == 0:
        return 1.0 if not np.any(target) else 0.0
    if not np.any(target):
        return 0.0
    distance = ndimage.distance_transform_edt(~target)
    return _ratio(np.count_nonzero(source & (distance <= tolerance)), source_count)


def _object_errors(reference, prediction):
    structure = np.ones((3, 3), dtype=bool)
    reference_labels, reference_count = ndimage.label(reference, structure=structure)
    prediction_labels, prediction_count = ndimage.label(prediction, structure=structure)

    split_excess = 0
    for object_id in range(1, reference_count + 1):
        overlaps = np.unique(prediction_labels[reference_labels == object_id])
        overlap_count = int(np.count_nonzero(overlaps))
        split_excess += max(0, overlap_count - 1)

    merge_excess = 0
    for object_id in range(1, prediction_count + 1):
        overlaps = np.unique(reference_labels[prediction_labels == object_id])
        overlap_count = int(np.count_nonzero(overlaps))
        merge_excess += max(0, overlap_count - 1)

    return reference_count, prediction_count, split_excess, merge_excess


def evaluate_reference_quality(reference_mask, prediction_mask, boundary_tolerance=1.0):
    """Evaluate a prediction against a binary synthetic or expert reference.

    Unlike :func:`evaluate_proxy_quality`, this function requires an explicit
    reference mask. It is suitable for controlled synthetic truth and manually
    reviewed local annotations, but must not be presented as geological truth
    when its reference is only synthetic.
    """
    reference = np.asarray(reference_mask, dtype=bool)
    prediction = np.asarray(prediction_mask, dtype=bool)
    if reference.ndim != 2 or prediction.ndim != 2:
        raise ValueError("reference_mask and prediction_mask must be two-dimensional")
    if reference.shape != prediction.shape:
        raise ValueError("reference_mask and prediction_mask must have identical shapes")
    tolerance = float(boundary_tolerance)
    if not np.isfinite(tolerance) or tolerance < 0.0:
        raise ValueError("boundary_tolerance must be finite and non-negative")

    intersection = int(np.count_nonzero(reference & prediction))
    reference_count = int(np.count_nonzero(reference))
    prediction_count = int(np.count_nonzero(prediction))
    union = int(np.count_nonzero(reference | prediction))

    reference_boundary = _binary_boundary(reference)
    prediction_boundary = _binary_boundary(prediction)
    boundary_precision = _matched_ratio(prediction_boundary, reference_boundary, tolerance)
    boundary_recall = _matched_ratio(reference_boundary, prediction_boundary, tolerance)
    boundary_f1 = _harmonic_mean(
        boundary_precision,
        boundary_recall,
        empty_is_perfect=not np.any(reference_boundary) and not np.any(prediction_boundary),
    )

    reference_skeleton = _morphological_skeleton(reference)
    prediction_skeleton = _morphological_skeleton(prediction)
    topology_precision = _ratio(np.count_nonzero(prediction_skeleton & reference), np.count_nonzero(prediction_skeleton))
    topology_recall = _ratio(np.count_nonzero(reference_skeleton & prediction), np.count_nonzero(reference_skeleton))
    cldice = _harmonic_mean(
        topology_precision,
        topology_recall,
        empty_is_perfect=not np.any(reference) and not np.any(prediction),
    )

    ref_objects, pred_objects, split_excess, merge_excess = _object_errors(reference, prediction)
    return {
        "precision": _ratio(intersection, prediction_count),
        "recall": _ratio(intersection, reference_count),
        "iou": _ratio(intersection, union),
        "boundary_precision": boundary_precision,
        "boundary_recall": boundary_recall,
        "boundary_f1": boundary_f1,
        "cldice": cldice,
        "reference_object_count": int(ref_objects),
        "prediction_object_count": int(pred_objects),
        "split_excess_count": int(split_excess),
        "merge_excess_count": int(merge_excess),
        "boundary_tolerance_cells": tolerance,
        "metric_semantics": {
            "classification": "reference_based_segmentation_and_topology",
            "uses_reference_mask": True,
            "reference_scope_must_be_declared": True,
            "intended_use": "synthetic_truth_and_expert_local_annotation_gate",
        },
    }
