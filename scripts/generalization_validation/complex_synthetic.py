"""Official-complexity planar fault-attribute benchmark generator.

The benchmark keeps the contest input/output contract while making the scenes
denser and less separable than the small morphology unit cases. Geometry is
generated first and the attribute response is rendered afterwards, so the
reference masks remain independent of the tracing implementation.
"""

from dataclasses import dataclass

import numpy as np
from scipy.ndimage import distance_transform_edt, gaussian_filter, gaussian_filter1d

from .attributes import robust_normalize


@dataclass
class ComplexCase:
    attribute: np.ndarray
    centerline_mask: np.ndarray
    object_masks: np.ndarray
    metadata: dict


def _path(x0, x1, y0, slope, curvature, phase, width, rng, count=760):
    x = np.linspace(float(x0), float(x1), count)
    span = max(abs(x1 - x0), 1.0)
    t = (x - x0) / span
    y = (
        y0
        + slope * (t - 0.5) * span
        + curvature * np.sin(2.0 * np.pi * (t * rng.uniform(0.65, 1.35) + phase))
        + 0.35 * curvature * np.sin(5.0 * np.pi * (t + phase * 0.7))
    )
    y += gaussian_filter1d(rng.normal(0.0, max(width, 1.0) * 0.8, count), sigma=35.0, mode="nearest")
    return np.column_stack((x, y))


def _line(p0, p1, count=760):
    t = np.linspace(0.0, 1.0, count)[:, None]
    return np.asarray(p0, dtype=float) * (1.0 - t) + np.asarray(p1, dtype=float) * t


def _rasterize(paths, width, height):
    mask = np.zeros((height, width), dtype=bool)
    for points in paths:
        xy = np.rint(np.asarray(points)).astype(int)
        valid = (
            (xy[:, 0] >= 0)
            & (xy[:, 0] < width)
            & (xy[:, 1] >= 0)
            & (xy[:, 1] < height)
        )
        mask[xy[valid, 1], xy[valid, 0]] = True
    return mask


def _add_crossing(objects, width, height, rng):
    cx = float(rng.uniform(width * 0.22, width * 0.78))
    cy = float(rng.uniform(height * 0.22, height * 0.78))
    half = float(rng.uniform(height * 0.20, height * 0.38))
    tilt = float(rng.uniform(-0.14, 0.14))
    objects.append([_line((cx - half * 1.7, cy - half), (cx + half * 1.7, cy + half + tilt * width))])
    objects.append([_line((cx - half * 1.7, cy + half), (cx + half * 1.7, cy - half + tilt * width))])


def _add_branch(objects, width, height, rng):
    cx = float(rng.uniform(width * 0.35, width * 0.62))
    cy = float(rng.uniform(height * 0.28, height * 0.72))
    trunk = _line((max(0.0, cx - width * 0.34), cy), (cx, cy), 380)
    branch_span = width * 0.34
    branch_amp = float(rng.uniform(height * 0.10, height * 0.22))
    upper = _path(cx, min(width, cx + branch_span), cy, -branch_amp / branch_span, branch_amp * 0.42, 0.1, 3.0, rng, 380)
    lower = _path(cx, min(width, cx + branch_span), cy, branch_amp / branch_span, branch_amp * 0.42, 0.7, 3.0, rng, 380)
    objects.append([trunk, upper, lower])


def generate_complex_case(width=1501, height=701, seed=0):
    if width < 512 or height < 256:
        raise ValueError("Complex benchmark requires a large planar grid.")
    rng = np.random.default_rng(int(seed))
    objects = []
    object_widths = []

    # A dense set of nonstationary, curved bands creates realistic competition
    # between nearby roots without using any dataset-specific coordinates.
    for _ in range(12):
        width_cells = float(rng.uniform(1.6, 3.2))
        objects.append(
            [
                _path(
                    -width * rng.uniform(0.02, 0.10),
                    width * rng.uniform(1.00, 1.10),
                    rng.uniform(height * 0.04, height * 0.96),
                    rng.uniform(-0.22, 0.22),
                    rng.uniform(12.0, 115.0),
                    rng.uniform(0.0, 1.0),
                    width_cells,
                    rng,
                )
            ]
        )
        object_widths.append(width_cells)

    # Close parallel pairs exercise anti-merge decisions separately from true
    # crossing pairs.
    for _ in range(3):
        base_y = float(rng.uniform(height * 0.12, height * 0.88))
        separation = float(rng.uniform(8.0, 22.0))
        for offset in (-separation * 0.5, separation * 0.5):
            objects.append(
                [
                    _path(
                        -width * 0.04,
                        width * 1.04,
                        base_y + offset,
                        rng.uniform(-0.12, 0.12),
                        rng.uniform(18.0, 85.0),
                        rng.uniform(0.0, 1.0),
                        float(rng.uniform(1.6, 3.0)),
                        rng,
                    )
                ]
            )
            object_widths.append(float(rng.uniform(1.6, 3.0)))

    for _ in range(3):
        _add_crossing(objects, width, height, rng)
        object_widths.extend([float(rng.uniform(1.6, 3.0)), float(rng.uniform(1.6, 3.0))])

    for _ in range(2):
        _add_branch(objects, width, height, rng)
        object_widths.append(float(rng.uniform(1.6, 3.0)))

    line_masks = [_rasterize(paths, width, height) for paths in objects]
    object_masks = []
    centerline = np.zeros((height, width), dtype=bool)
    signal = np.zeros((height, width), dtype=np.float32)
    positive_support = np.zeros((height, width), dtype=bool)
    weak_gap_count = 0

    xgrid = np.arange(width, dtype=np.float32)[None, :]
    for index, line_mask in enumerate(line_masks):
        centerline |= line_mask
        radius = object_widths[index] if index < len(object_widths) else float(rng.uniform(2.5, 5.0))
        distance = distance_transform_edt(~line_mask)
        object_mask = distance <= radius
        object_masks.append(object_mask)
        sigma = max(1.15, radius * float(rng.uniform(0.65, 0.95)))
        response = np.exp(-0.5 * (distance / sigma) ** 2).astype(np.float32)
        amplitude = float(rng.uniform(0.88, 1.0))
        modulation = 0.92 + 0.08 * np.sin(xgrid / rng.uniform(55.0, 145.0) + rng.uniform(0.0, 6.0))
        support = distance <= 1.0 if index % 6 == 0 else line_mask
        if index % 7 == 0:
            gap_start = int(rng.uniform(width * 0.32, width * 0.70))
            gap_span = int(rng.uniform(width * 0.008, width * 0.035))
            response[:, gap_start : gap_start + gap_span] *= float(rng.uniform(0.28, 0.52))
            weak_gap_count += 1
        signal = np.maximum(signal, amplitude * response * modulation)
        positive_support |= support

    # Correlated background, low-frequency texture, and anisotropic stripes are
    # deliberately added after the geometry is fixed to prevent truth leakage.
    low_noise = robust_normalize(gaussian_filter(rng.normal(size=(height, width)), sigma=(7.0, 17.0)))
    fine_noise = robust_normalize(gaussian_filter(rng.normal(size=(height, width)), sigma=(1.2, 3.5)))
    modulated = np.clip(signal * (0.97 + 0.03 * low_noise) + 0.01 * fine_noise, 0.0, 1.0)
    attribute = np.zeros((height, width), dtype=np.float32)
    attribute[positive_support] = 0.30 + 0.70 * np.power(modulated[positive_support], 0.35)
    sparse_noise = (low_noise >= np.quantile(low_noise, 0.9885)) & ~positive_support
    attribute[sparse_noise] = 0.45 + 0.38 * fine_noise[sparse_noise]
    metadata = {
        "schema_version": 1,
        "seed": int(seed),
        "family": "official_complexity_mixed_network",
        "width": int(width),
        "height": int(height),
        "object_count": int(len(object_masks)),
        "truth_radius_cells": "object-specific variable band width",
        "truth_source": "geometry_is_generated_before_attribute_rendering",
        "background_mode": "sparse_zero_background_with_correlated_noise",
        "attribute_distribution_calibration": "contest_aggregate_sparsity_only_no_coordinates_or_object_ids",
        "dataset_specific_rule": False,
        "complexity": {
            "dense_curved_objects": 12,
            "close_parallel_pairs": 3,
            "independent_crossing_pairs": 3,
            "branch_groups": 2,
            "weak_gap_objects": weak_gap_count,
        },
    }
    return ComplexCase(
        attribute=attribute.astype(np.float32),
        centerline_mask=centerline,
        object_masks=np.asarray(object_masks, dtype=bool),
        metadata=metadata,
    )
