from dataclasses import dataclass
import json
from pathlib import Path

import numpy as np
from PIL import Image
from scipy.ndimage import distance_transform_edt, gaussian_filter
from shapely.geometry import box, mapping
from shapely.ops import unary_union

from .attributes import robust_normalize
from .io import write_ascii_grid


FAMILIES = (
    "isolated_curved",
    "parallel_close",
    "branching",
    "crossing_independent",
    "weak_gap",
    "wide_variable",
    "edge_truncated",
    "noisy_footprint",
)


@dataclass
class GeneratedCase:
    attribute: np.ndarray
    centerline_mask: np.ndarray
    object_masks: np.ndarray
    metadata: dict


def _bezier(points, count=420):
    p0, p1, p2, p3 = (np.asarray(point, dtype=float) for point in points)
    t = np.linspace(0.0, 1.0, count)[:, None]
    return (
        ((1.0 - t) ** 3) * p0
        + 3.0 * ((1.0 - t) ** 2) * t * p1
        + 3.0 * (1.0 - t) * (t**2) * p2
        + (t**3) * p3
    )


def _line(points, count=420):
    first, second = (np.asarray(point, dtype=float) for point in points)
    t = np.linspace(0.0, 1.0, count)[:, None]
    return first * (1.0 - t) + second * t


def _rasterize_paths(paths, width, height):
    mask = np.zeros((height, width), dtype=bool)
    for path in paths:
        points = np.asarray(path)
        xs = np.rint(points[:, 0]).astype(int)
        ys = np.rint(points[:, 1]).astype(int)
        valid = (xs >= 0) & (xs < width) & (ys >= 0) & (ys < height)
        mask[ys[valid], xs[valid]] = True
    return mask


def _family_paths(width, height, family, rng):
    mid = height * 0.5
    wobble = float(rng.uniform(-0.09, 0.09) * height)
    curve = _bezier(
        [(12, mid + wobble), (width * 0.32, height * 0.25), (width * 0.66, height * 0.77), (width - 12, mid - wobble)]
    )
    if family == "isolated_curved" or family in {"weak_gap", "wide_variable", "noisy_footprint"}:
        return [[curve]]
    if family == "parallel_close":
        offset = max(10.0, height * 0.13)
        return [[curve + (0.0, -offset * 0.5)], [curve + (0.0, offset * 0.5)]]
    if family == "branching":
        junction = (width * 0.48, mid)
        trunk = _line([(8, mid), junction], 260)
        upper = _bezier([junction, (width * 0.62, mid - 4), (width * 0.78, height * 0.23), (width - 8, height * 0.18)], 260)
        lower = _bezier([junction, (width * 0.62, mid + 4), (width * 0.78, height * 0.77), (width - 8, height * 0.82)], 260)
        return [[trunk, upper, lower]]
    if family == "crossing_independent":
        first = _line([(8, height * 0.18), (width - 8, height * 0.82)])
        second = _line([(8, height * 0.82), (width - 8, height * 0.18)])
        return [[first], [second]]
    if family == "edge_truncated":
        return [[_bezier([(-20, height * 0.28), (width * 0.3, height * 0.18), (width * 0.7, height * 0.82), (width + 20, height * 0.72)])]]
    raise KeyError(f"Unsupported benchmark family: {family}")


def _attribute_from_lines(line_masks, family, rng):
    height, width = line_masks[0].shape
    signal = np.zeros((height, width), dtype=np.float32)
    for index, line_mask in enumerate(line_masks):
        distance = distance_transform_edt(~line_mask)
        sigma = 1.7 + 0.25 * index
        signal = np.maximum(signal, np.exp(-0.5 * (distance / sigma) ** 2).astype(np.float32))
    if family == "weak_gap":
        x0, x1 = int(width * 0.43), int(width * 0.57)
        signal[:, x0:x1] *= 0.42
    if family == "wide_variable":
        combined = np.any(np.stack(line_masks), axis=0)
        central = combined.copy()
        central[:, : int(width * 0.38)] = False
        central[:, int(width * 0.68) :] = False
        distance = distance_transform_edt(~central)
        signal = np.maximum(signal, 0.82 * np.exp(-0.5 * (distance / 4.4) ** 2))
    noise = gaussian_filter(rng.normal(0.0, 1.0, signal.shape), sigma=1.25)
    noise = robust_normalize(noise) * 0.10
    signal = 0.90 * signal + noise
    if family == "noisy_footprint":
        x = np.arange(width, dtype=float)[None, :]
        footprint = 0.09 * (0.5 + 0.5 * np.cos(2.0 * np.pi * x / 13.0))
        signal = signal + footprint
        for _ in range(18):
            cy = int(rng.integers(0, height))
            cx = int(rng.integers(0, width))
            signal[max(0, cy - 1) : cy + 2, max(0, cx - 1) : cx + 2] += float(rng.uniform(0.08, 0.20))
    return robust_normalize(signal)


def generate_case(width=256, height=192, seed=0, family="isolated_curved", background=None):
    if family not in FAMILIES:
        raise KeyError(f"Unknown family {family}; expected one of {FAMILIES}.")
    if width < 96 or height < 72:
        raise ValueError("Benchmark grids must be at least 96 x 72 cells.")
    rng = np.random.default_rng(int(seed))
    objects = _family_paths(width, height, family, rng)
    line_masks = [_rasterize_paths(paths, width, height) for paths in objects]
    truth_radius = 3.25
    object_masks = np.stack([distance_transform_edt(~mask) <= truth_radius for mask in line_masks])
    attribute = _attribute_from_lines(line_masks, family, rng)
    background_mode = "procedural_noise_only"
    if background is not None:
        external = np.asarray(background, dtype=np.float32)
        if external.shape != attribute.shape:
            raise ValueError(f"Background shape {external.shape} does not match grid {attribute.shape}.")
        attribute = robust_normalize(0.34 * robust_normalize(external) + 0.82 * attribute)
        background_mode = "external_real_or_user_supplied"
    metadata = {
        "schema_version": 1,
        "seed": int(seed),
        "family": family,
        "width": int(width),
        "height": int(height),
        "object_count": int(len(objects)),
        "truth_radius_cells": truth_radius,
        "truth_source": "generator_geometry_before_attribute_rendering",
        "background_mode": background_mode,
        "dataset_specific_rule": False,
    }
    return GeneratedCase(
        attribute=attribute.astype(np.float32),
        centerline_mask=np.any(np.stack(line_masks), axis=0),
        object_masks=object_masks.astype(bool),
        metadata=metadata,
    )


def _mask_geometry(mask):
    rectangles = []
    for y, row in enumerate(np.asarray(mask, dtype=bool)):
        padded = np.pad(row.astype(np.int8), (1, 1))
        changes = np.diff(padded)
        starts = np.flatnonzero(changes == 1)
        ends = np.flatnonzero(changes == -1)
        rectangles.extend(box(float(x0), float(y), float(x1), float(y + 1)) for x0, x1 in zip(starts, ends))
    return unary_union(rectangles) if rectangles else None


def object_masks_geojson(object_masks):
    features = []
    for index, mask in enumerate(np.asarray(object_masks), start=1):
        geometry = _mask_geometry(mask)
        if geometry is None or geometry.is_empty:
            continue
        features.append(
            {
                "type": "Feature",
                "properties": {"object_id": index},
                "geometry": mapping(geometry),
            }
        )
    return {"type": "FeatureCollection", "features": features}


def save_case(case, output_dir):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    write_ascii_grid(output_dir / "attribute.txt", case.attribute)
    np.save(output_dir / "attribute.npy", case.attribute, allow_pickle=False)
    np.savez_compressed(
        output_dir / "truth_masks.npz",
        object_masks=case.object_masks.astype(np.uint8),
        centerline_mask=case.centerline_mask.astype(np.uint8),
    )
    Image.fromarray((case.attribute * 255.0).astype(np.uint8)).save(output_dir / "attribute.png")
    Image.fromarray((np.any(case.object_masks, axis=0) * 255).astype(np.uint8)).save(output_dir / "truth_mask.png")
    (output_dir / "truth.geojson").write_text(
        json.dumps(object_masks_geojson(case.object_masks), ensure_ascii=False, indent=2), encoding="utf-8"
    )
    (output_dir / "metadata.json").write_text(
        json.dumps(case.metadata, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return output_dir

