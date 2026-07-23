# -*- coding: utf-8 -*-
"""Local morphology diagnostics for fault-polygon quality review.

The topology gate can prove that a polygon is closed and valid, but it cannot
detect interpretation-level defects such as local pouch-like bulges, inner
rings in a thin fault belt, or an over-wide envelope around a branch junction.
This script adds a local shape gate without changing the official result CSV.
"""

import argparse
import csv
import json
import math
import os
import sys
from collections import deque
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib import font_manager
from matplotlib.patches import Polygon as PolygonPatch, Rectangle
import matplotlib.pyplot as plt
import numpy as np

from wkt_geometry import parse_wkt_polygon_with_holes, rasterize_polygon

csv.field_size_limit(min(sys.maxsize, 2_147_483_647))


def configure_chinese_font():
    for path in [r"C:\Windows\Fonts\msyh.ttc", r"C:\Windows\Fonts\simhei.ttf", r"C:\Windows\Fonts\simsun.ttc"]:
        if os.path.exists(path):
            font_manager.fontManager.addfont(path)
            font_name = font_manager.FontProperties(fname=path).get_name()
            plt.rcParams["font.sans-serif"] = [font_name, "DejaVu Sans"]
            plt.rcParams["axes.unicode_minus"] = False
            return


def _connected_components(mask, min_cells=1):
    height, width = mask.shape
    visited = np.zeros_like(mask, dtype=bool)
    components = []
    offsets = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]
    for y0 in range(height):
        for x0 in range(width):
            if not mask[y0, x0] or visited[y0, x0]:
                continue
            queue = deque([(x0, y0)])
            visited[y0, x0] = True
            cells = []
            while queue:
                x, y = queue.popleft()
                cells.append((x, y))
                for ox, oy in offsets:
                    nx = x + ox
                    ny = y + oy
                    if 0 <= nx < width and 0 <= ny < height and mask[ny, nx] and not visited[ny, nx]:
                        visited[ny, nx] = True
                        queue.append((nx, ny))
            if len(cells) >= min_cells:
                arr = np.asarray(cells, dtype=int)
                components.append(
                    {
                        "cells": cells,
                        "cell_count": int(len(cells)),
                        "min_x": int(arr[:, 0].min()),
                        "max_x": int(arr[:, 0].max()),
                        "min_y": int(arr[:, 1].min()),
                        "max_y": int(arr[:, 1].max()),
                    }
                )
    components.sort(key=lambda item: item["cell_count"], reverse=True)
    return components


def _principal_axis(points):
    centered = points - points.mean(axis=0, keepdims=True)
    if len(points) < 3:
        return np.asarray([1.0, 0.0]), np.asarray([0.0, 1.0]), centered
    covariance = centered.T @ centered / max(1, len(points) - 1)
    eigenvalues, eigenvectors = np.linalg.eigh(covariance)
    axis = eigenvectors[:, int(np.argmax(eigenvalues))]
    axis = axis / max(1e-12, np.linalg.norm(axis))
    normal = np.asarray([-axis[1], axis[0]], dtype=float)
    return axis, normal, centered


def _rolling_median(values, radius):
    if radius <= 0:
        return values.copy()
    out = np.zeros_like(values, dtype=float)
    for i in range(len(values)):
        lo = max(0, i - radius)
        hi = min(len(values), i + radius + 1)
        out[i] = float(np.median(values[lo:hi]))
    return out


def classify_bulge_candidate(candidate, total_bins):
    """Classify a width anomaly so reports do not mix different defect types."""
    span_ratio = float(candidate["bin_count"]) / max(1.0, float(total_bins))
    width_ratio = float(candidate["width_ratio"])
    extra_cells = int(candidate["extra_cells"])

    if span_ratio >= 0.45:
        shape_class = "长段宽带复核"
        priority = "medium" if width_ratio >= 2.2 or extra_cells >= 80 else "low"
    elif width_ratio >= 3.0 or extra_cells >= 80:
        shape_class = "局部鼓包"
        priority = "high"
    else:
        shape_class = "局部包络复核"
        priority = "medium"

    return shape_class, priority, span_ratio


def classify_inner_ring_context(hole_cells, support_ratio=0.0, aspect_ratio=1.0):
    """Classify an inner ring by attribute support and morphology context.

    In a fault belt, an inner ring is not automatically an error. A long,
    narrow gap clamped by strong responses on both sides is more consistent
    with a double-line fault belt or local split response than with a false
    pouch. A large ring with weak surrounding support remains high priority.
    """
    cells = int(hole_cells or 0)
    support = float(support_ratio or 0.0)
    aspect = float(aspect_ratio or 1.0)

    if support >= 0.60 and aspect >= 2.5:
        return "双线夹持内环复核", "medium"
    if support >= 0.45 and aspect >= 1.8 and cells < 120:
        return "属性支撑内环复核", "medium"
    if cells >= 24 and support < 0.35:
        return "内环复核", "high"
    return "内环复核", "medium"


def _count_boundary_branch_arms(mask, component, boundary_width=2, min_arm_cells=2):
    """Count independent mask arms crossing the candidate component boundary.

    A simple width anomaly is not necessarily a bad pouch. At real branch or
    intersection zones, several line-like responses enter and leave the local
    wide area. Counting boundary-contacting response clusters gives the
    diagnostic gate a topology cue without changing the traced polygons.
    """
    mask = np.asarray(mask, dtype=bool)
    height, width = mask.shape
    min_x = max(0, int(component["min_x"]))
    max_x = min(width - 1, int(component["max_x"]))
    min_y = max(0, int(component["min_y"]))
    max_y = min(height - 1, int(component["max_y"]))
    if min_x > max_x or min_y > max_y:
        return 0

    local = np.zeros_like(mask, dtype=bool)
    local[min_y : max_y + 1, min_x : max_x + 1] = mask[min_y : max_y + 1, min_x : max_x + 1]
    yy, xx = np.nonzero(local)
    if len(xx) == 0:
        return 0

    boundary = np.zeros_like(mask, dtype=bool)
    bw = max(1, int(boundary_width))
    on_boundary = (
        (xx <= min_x + bw)
        | (xx >= max_x - bw)
        | (yy <= min_y + bw)
        | (yy >= max_y - bw)
    )
    boundary[yy[on_boundary], xx[on_boundary]] = True
    internal_count = len(_connected_components(boundary, min_cells=min_arm_cells))

    # Also count response arms immediately outside the candidate box. A filled
    # endpoint widening can hide the incoming and outgoing arms if we only look
    # at cells inside the box.
    pad = max(2, int(boundary_width) * 3)
    ex_min_x = max(0, min_x - pad)
    ex_max_x = min(width - 1, max_x + pad)
    ex_min_y = max(0, min_y - pad)
    ex_max_y = min(height - 1, max_y + pad)
    exterior = np.zeros_like(mask, dtype=bool)
    exterior[ex_min_y : ex_max_y + 1, ex_min_x : ex_max_x + 1] = mask[ex_min_y : ex_max_y + 1, ex_min_x : ex_max_x + 1]
    exterior[min_y : max_y + 1, min_x : max_x + 1] = False
    exterior_count = len(_connected_components(exterior, min_cells=min_arm_cells))
    return max(internal_count, exterior_count)


def _suppress_adjacent_endpoint_duplicates(candidates, max_gap=4):
    """Remove duplicate sub-candidates produced by one endpoint widening."""
    kept = []
    for candidate in candidates:
        is_duplicate = False
        for existing in kept:
            close_x = candidate["min_x"] <= existing["max_x"] + max_gap and existing["min_x"] <= candidate["max_x"] + max_gap
            close_y = candidate["min_y"] <= existing["max_y"] + max_gap and existing["min_y"] <= candidate["max_y"] + max_gap
            endpoint_like = (
                existing.get("shape_class") == "双臂端点复核"
                or candidate.get("shape_class") == "双臂端点复核"
            )
            if close_x and close_y and endpoint_like:
                is_duplicate = True
                existing["extra_cells"] = int(existing.get("extra_cells", 0)) + int(candidate.get("extra_cells", 0))
                existing["min_x"] = min(int(existing["min_x"]), int(candidate["min_x"]))
                existing["max_x"] = max(int(existing["max_x"]), int(candidate["max_x"]))
                existing["min_y"] = min(int(existing["min_y"]), int(candidate["min_y"]))
                existing["max_y"] = max(int(existing["max_y"]), int(candidate["max_y"]))
                existing["bin_count"] = int(existing.get("bin_count", 0)) + int(candidate.get("bin_count", 0))
                break
        if not is_duplicate:
            kept.append(candidate)
    return kept


def apply_polygon_context(candidate, polygon):
    """Upgrade candidate labels when polygon-level context implies branch risk."""
    updated = dict(candidate)
    hole_count = len(polygon.get("holes", []))
    polygon_area = float(polygon.get("area", 0.0) or 0.0)
    width_ratio = float(updated.get("width_ratio", 0.0) or 0.0)
    span_ratio = float(updated.get("span_ratio", 0.0) or 0.0)
    extra_cells = int(updated.get("extra_cells", 0) or 0)

    if (
        hole_count > 0
        and polygon_area >= 100000.0
        and width_ratio >= 6.0
        and span_ratio < 0.50
        and extra_cells >= 80
    ):
        updated["shape_class"] = "疑似分支归属错误"
        updated["review_priority"] = "high"
    return updated


def _dilate_mask(mask, radius):
    mask = np.asarray(mask, dtype=bool)
    radius = int(max(0, radius))
    if radius <= 0:
        return mask.copy()
    height, width = mask.shape
    out = mask.copy()
    yy, xx = np.nonzero(mask)
    for dy0 in range(-radius, radius + 1):
        for dx0 in range(-radius, radius + 1):
            if dx0 * dx0 + dy0 * dy0 > radius * radius:
                continue
            ny = yy + dy0
            nx = xx + dx0
            valid = (nx >= 0) & (nx < width) & (ny >= 0) & (ny < height)
            out[ny[valid], nx[valid]] = True
    return out


def _hole_context_metrics(hole_mask, polygon_mask, values, support_threshold=0.66, ring_radius=2):
    """Return support ratio and aspect ratio for an inner ring.

    The support ratio is measured only in the immediate polygon-side collar
    around the hole, because the question is whether the hole is clamped by
    real fault-attribute response rather than surrounded by low-value fill.
    """
    hole_mask = np.asarray(hole_mask, dtype=bool)
    yy, xx = np.nonzero(hole_mask)
    if len(xx) == 0:
        return 0.0, 1.0

    width = int(xx.max() - xx.min() + 1)
    height = int(yy.max() - yy.min() + 1)
    aspect_ratio = float(max(width, height)) / max(1.0, float(min(width, height)))

    collar = _dilate_mask(hole_mask, ring_radius) & ~hole_mask & np.asarray(polygon_mask, dtype=bool)
    collar_cells = int(np.count_nonzero(collar))
    if collar_cells <= 0:
        return 0.0, aspect_ratio
    support_cells = int(np.count_nonzero(np.asarray(values)[collar] >= float(support_threshold)))
    support_ratio = float(support_cells) / float(collar_cells)
    return support_ratio, aspect_ratio


def detect_local_bulges(
    mask,
    window=11,
    width_ratio=2.4,
    min_extra_cells=24,
    min_baseline_width=2.0,
    context_aware=True,
):
    """Detect local pouch-like bulges in one rasterized polygon mask.

    The method estimates a dominant trace direction with PCA, projects polygon
    cells along that direction, and compares local cross-trace width with a
    robust baseline. It intentionally returns candidates, not edits.
    """
    mask = np.asarray(mask, dtype=bool)
    yy, xx = np.nonzero(mask)
    if len(xx) < max(8, min_extra_cells):
        return []

    points = np.column_stack([xx.astype(float), yy.astype(float)])
    axis, normal, centered = _principal_axis(points)
    along = centered @ axis
    across = centered @ normal
    along_min = float(along.min())
    bins = np.floor(along - along_min).astype(int)
    bin_count = int(bins.max()) + 1
    if bin_count < 3:
        return []

    width_by_bin = np.zeros(bin_count, dtype=float)
    count_by_bin = np.zeros(bin_count, dtype=int)
    for b in range(bin_count):
        values = across[bins == b]
        if values.size:
            width_by_bin[b] = float(values.max() - values.min() + 1.0)
            count_by_bin[b] = int(values.size)

    occupied = count_by_bin > 0
    if np.count_nonzero(occupied) < 3:
        return []

    occupied_widths = width_by_bin[occupied]
    lower_pool = np.sort(occupied_widths)[: max(1, int(math.ceil(len(occupied_widths) * 0.65)))]
    global_baseline = max(float(np.median(lower_pool)), float(min_baseline_width))
    local_baseline = _rolling_median(width_by_bin, max(1, window // 2))
    local_baseline[local_baseline <= 0] = global_baseline
    # A local median can be polluted by the bulge we are trying to detect.
    # Use the lower of local and robust global baselines so pouch-like outliers
    # are compared against the ordinary belt width, not against themselves.
    baseline_by_bin = np.minimum(local_baseline, global_baseline)
    baseline_by_bin[baseline_by_bin <= 0] = global_baseline

    suspect_bins = occupied & (width_by_bin >= baseline_by_bin * float(width_ratio))
    if not np.any(suspect_bins):
        return []

    suspect_mask = np.zeros_like(mask, dtype=bool)
    bin_by_cell = {}
    for x, y, b in zip(xx, yy, bins):
        bin_by_cell[(int(x), int(y))] = int(b)
        if suspect_bins[b]:
            suspect_mask[y, x] = True

    candidates = []
    for component in _connected_components(suspect_mask, min_cells=1):
        component_cells = component["cells"]
        component_bins = []
        for x, y in component_cells:
            if (x, y) in bin_by_cell:
                component_bins.append(bin_by_cell[(x, y)])
        if not component_bins:
            continue
        unique_bins = sorted(set(component_bins))
        edge_distance_ratio = float(min(unique_bins[0], max(0, bin_count - 1 - unique_bins[-1]))) / max(1.0, float(bin_count))
        expected_cells = sum(float(baseline_by_bin[b]) for b in unique_bins)
        actual_cells = len(component_cells)
        extra_cells = int(round(actual_cells - expected_cells))
        if extra_cells < min_extra_cells:
            continue
        local_width = float(max(width_by_bin[b] for b in unique_bins))
        baseline_width = float(np.median([baseline_by_bin[b] for b in unique_bins]))
        branch_arm_count = _count_boundary_branch_arms(mask, component)
        shape_class, review_priority, span_ratio = classify_bulge_candidate(
            {
                "bin_count": len(unique_bins),
                "width_ratio": local_width / max(1e-9, baseline_width),
                "extra_cells": extra_cells,
            },
            bin_count,
        )
        component.update(
            {
                "baseline_width": baseline_width,
                "local_width": local_width,
                "width_ratio": float(local_width / max(1e-9, baseline_width)),
                "extra_cells": extra_cells,
                "bin_count": int(len(unique_bins)),
                "span_ratio": span_ratio,
                "edge_distance_ratio": edge_distance_ratio,
                "shape_class": shape_class,
                "review_priority": review_priority,
                "branch_arm_count": branch_arm_count,
            }
        )
        if (
            context_aware
            and
            branch_arm_count >= 3
            and component["span_ratio"] < 0.45
            and component["width_ratio"] >= float(width_ratio)
        ):
            component["shape_class"] = "分支交切复核"
            component["review_priority"] = "medium"
        elif (
            context_aware
            and
            branch_arm_count == 2
            and component["span_ratio"] < 0.35
            and component["edge_distance_ratio"] <= (1.0 / 3.0)
            and component["width_ratio"] >= float(width_ratio)
        ):
            component["shape_class"] = "双臂端点复核"
            component["review_priority"] = "medium"
        candidates.append(component)

    candidates.sort(key=lambda item: (item["extra_cells"], item["width_ratio"]), reverse=True)
    candidates = _suppress_adjacent_endpoint_duplicates(candidates)
    candidates.sort(key=lambda item: (item["extra_cells"], item["width_ratio"]), reverse=True)
    return candidates


def read_geoeast_dat(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            text = line.strip()
            if not text or text.startswith("#"):
                continue
            parts = text.split()
            if len(parts) < 5:
                continue
            rows.append((int(float(parts[0])), int(float(parts[1])), float(parts[2]), float(parts[3]), float(parts[4])))
    if not rows:
        raise RuntimeError(f"未读取到有效 GeoEast DAT 网格：{path}")

    arr = np.asarray(rows, dtype=float)
    line_keys = np.unique(arr[:, 0]).astype(int)
    cmp_keys = np.unique(arr[:, 1]).astype(int)
    line_index = {v: i for i, v in enumerate(line_keys)}
    cmp_index = {v: i for i, v in enumerate(cmp_keys)}
    values = np.zeros((len(cmp_keys), len(line_keys)), dtype=float)
    x_by_line = {}
    y_by_cmp = {}
    for line_no, cmp_no, x, y, value in rows:
        values[cmp_index[int(cmp_no)], line_index[int(line_no)]] = value
        x_by_line[int(line_no)] = x
        y_by_cmp[int(cmp_no)] = y
    xs = np.asarray([x_by_line[int(k)] for k in line_keys], dtype=float)
    ys = np.asarray([y_by_cmp[int(k)] for k in cmp_keys], dtype=float)
    dx = abs(xs[1] - xs[0]) if len(xs) > 1 else 1.0
    dy = abs(ys[1] - ys[0]) if len(ys) > 1 else 1.0
    extent_edges = (
        float(xs.min() - 0.5 * dx),
        float(xs.max() + 0.5 * dx),
        float(ys.min() - 0.5 * dy),
        float(ys.max() + 0.5 * dy),
    )
    return values, xs, ys, dx, dy, extent_edges


def read_polygons(path):
    polygons = []
    with open(path, "r", encoding="utf-8", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row.get("status", "").lower() != "kept":
                continue
            coords, holes = parse_wkt_polygon_with_holes(row.get("wkt", ""))
            if len(coords) < 3:
                continue
            polygons.append(
                {
                    "id": int(float(row.get("id", len(polygons) + 1) or 0)),
                    "area": float(row.get("area", 0.0) or 0.0),
                    "perimeter": float(row.get("perimeter", 0.0) or 0.0),
                    "cell_count": int(float(row.get("cell_count", 0) or 0)),
                    "mean_value": float(row.get("mean_value", 0.0) or 0.0),
                    "max_value": float(row.get("max_value", 0.0) or 0.0),
                    "coords": coords,
                    "holes": holes,
                }
            )
    return polygons


def world_bbox(candidate, xs, ys, dx, dy):
    min_x = max(0, candidate["min_x"])
    max_x = min(len(xs) - 1, candidate["max_x"])
    min_y = max(0, candidate["min_y"])
    max_y = min(len(ys) - 1, candidate["max_y"])
    return (
        float(xs[min_x] - 0.5 * dx),
        float(xs[max_x] + 0.5 * dx),
        float(ys[min_y] - 0.5 * dy),
        float(ys[max_y] + 0.5 * dy),
    )


def draw_candidate(path, values, extent_edges, xs, ys, dx, dy, polygon, candidate, polygon_mask):
    bbox = world_bbox(candidate, xs, ys, dx, dy)
    pad_x = max(120.0, (bbox[1] - bbox[0]) * 2.0)
    pad_y = max(120.0, (bbox[3] - bbox[2]) * 2.0)
    local_extent = (bbox[0] - pad_x, bbox[1] + pad_x, bbox[2] - pad_y, bbox[3] + pad_y)

    fig, ax = plt.subplots(figsize=(7.2, 5.6), dpi=220)
    ax.set_facecolor("white")
    weak = np.ma.masked_where(values <= 0, values)
    strong = np.ma.masked_where(values < 0.66, values)
    ax.imshow(weak, extent=extent_edges, origin="lower", cmap="gray_r", interpolation="nearest", vmin=0.30, vmax=1.0, alpha=0.45, aspect="equal")
    ax.imshow(strong, extent=extent_edges, origin="lower", cmap="cool", interpolation="nearest", vmin=0.66, vmax=1.0, alpha=0.78, aspect="equal")

    arr = np.asarray(polygon["coords"], dtype=float)
    ax.add_patch(PolygonPatch(arr, closed=True, facecolor="#ff2a2a", edgecolor="#d00000", linewidth=0.8, alpha=0.12))
    ax.plot(arr[:, 0], arr[:, 1], color="#d00000", linewidth=0.6)
    for hole in polygon["holes"]:
        hole_arr = np.asarray(hole, dtype=float)
        ax.plot(hole_arr[:, 0], hole_arr[:, 1], color="#d00000", linewidth=0.55, linestyle="--")

    ax.add_patch(
        Rectangle(
            (bbox[0], bbox[2]),
            bbox[1] - bbox[0],
            bbox[3] - bbox[2],
            fill=False,
            edgecolor="#ffb000",
            linewidth=2.0,
        )
    )
    ax.set_xlim(local_extent[0], local_extent[1])
    ax.set_ylim(local_extent[2], local_extent[3])
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.tick_params(labelsize=7, colors="#606870")
    shape_class = candidate.get("shape_class", "局部鼓包")
    priority = candidate.get("review_priority", "medium")
    if "support_ratio" in candidate or "aspect_ratio" in candidate:
        ax.set_title(
            f"polygon {polygon['id']} {shape_class}（{priority}）：支撑 {float(candidate.get('support_ratio', 0.0)):.2f}，长宽比 {float(candidate.get('aspect_ratio', 1.0)):.2f}，cells={candidate['extra_cells']}"
        )
    else:
        ax.set_title(
            f"polygon {polygon['id']} {shape_class}（{priority}）：宽度比 {candidate['width_ratio']:.2f}，extra={candidate['extra_cells']}"
        )
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)


def diagnose(
    polygons,
    values,
    xs,
    ys,
    dx,
    dy,
    width_ratio,
    min_extra_cells,
    window,
    min_hole_area_cells=6,
    context_aware=True,
):
    records = []
    width = len(xs)
    height = len(ys)
    for polygon in polygons:
        polygon_mask = rasterize_polygon(polygon["coords"], width, height, xs, ys, dx, dy, polygon["holes"])
        bulges = detect_local_bulges(
            polygon_mask,
            window=window,
            width_ratio=width_ratio,
            min_extra_cells=min_extra_cells,
            context_aware=context_aware,
        )
        for index, candidate in enumerate(bulges, start=1):
            candidate = apply_polygon_context(candidate, polygon)
            bbox = world_bbox(candidate, xs, ys, dx, dy)
            records.append(
                {
                    "type": "局部鼓包",
                    "polygon_id": polygon["id"],
                    "candidate_index": index,
                    "polygon_area": polygon["area"],
                    "polygon_cell_count": polygon["cell_count"],
                    "polygon_mean_value": polygon["mean_value"],
                    "polygon_max_value": polygon["max_value"],
                    "hole_count": len(polygon["holes"]),
                    "extra_cells": candidate["extra_cells"],
                    "local_width": candidate["local_width"],
                    "baseline_width": candidate["baseline_width"],
                    "width_ratio": candidate["width_ratio"],
                    "span_ratio": candidate.get("span_ratio", 0.0),
                    "branch_arm_count": candidate.get("branch_arm_count", 0),
                    "shape_class": candidate.get("shape_class", "局部鼓包"),
                    "review_priority": candidate.get("review_priority", "medium"),
                    "min_x_index": candidate["min_x"],
                    "max_x_index": candidate["max_x"],
                    "min_y_index": candidate["min_y"],
                    "max_y_index": candidate["max_y"],
                    "xmin": bbox[0],
                    "xmax": bbox[1],
                    "ymin": bbox[2],
                    "ymax": bbox[3],
                    "_polygon": polygon,
                    "_candidate": candidate,
                    "_mask": polygon_mask,
                }
            )

        if polygon["holes"]:
            for index, hole in enumerate(polygon["holes"], start=1):
                hole_mask = rasterize_polygon(hole, width, height, xs, ys, dx, dy, [])
                hole_cells = int(np.count_nonzero(hole_mask))
                if hole_cells >= min_hole_area_cells:
                    yy, xx = np.nonzero(hole_mask)
                    support_ratio, aspect_ratio = _hole_context_metrics(hole_mask, polygon_mask, values)
                    shape_class, review_priority = classify_inner_ring_context(
                        hole_cells=hole_cells,
                        support_ratio=support_ratio,
                        aspect_ratio=aspect_ratio,
                    )
                    bbox = (
                        float(xs[int(xx.min())] - 0.5 * dx),
                        float(xs[int(xx.max())] + 0.5 * dx),
                        float(ys[int(yy.min())] - 0.5 * dy),
                        float(ys[int(yy.max())] + 0.5 * dy),
                    )
                    records.append(
                        {
                            "type": "内环复核",
                            "polygon_id": polygon["id"],
                            "candidate_index": index,
                            "polygon_area": polygon["area"],
                            "polygon_cell_count": polygon["cell_count"],
                            "polygon_mean_value": polygon["mean_value"],
                            "polygon_max_value": polygon["max_value"],
                            "hole_count": len(polygon["holes"]),
                            "extra_cells": hole_cells,
                            "local_width": 0.0,
                            "baseline_width": 0.0,
                            "width_ratio": 0.0,
                            "span_ratio": 0.0,
                            "aspect_ratio": aspect_ratio,
                            "branch_arm_count": 0,
                            "shape_class": shape_class,
                            "review_priority": review_priority,
                            "support_ratio": support_ratio,
                            "min_x_index": int(xx.min()),
                            "max_x_index": int(xx.max()),
                            "min_y_index": int(yy.min()),
                            "max_y_index": int(yy.max()),
                            "xmin": bbox[0],
                            "xmax": bbox[1],
                            "ymin": bbox[2],
                            "ymax": bbox[3],
                            "_polygon": polygon,
                            "_candidate": {
                                "min_x": int(xx.min()),
                                "max_x": int(xx.max()),
                                "min_y": int(yy.min()),
                                "max_y": int(yy.max()),
                                "width_ratio": 0.0,
                                "extra_cells": hole_cells,
                                "shape_class": shape_class,
                                "review_priority": review_priority,
                                "support_ratio": support_ratio,
                                "aspect_ratio": aspect_ratio,
                            },
                            "_mask": polygon_mask,
                        }
                    )
    records.sort(key=lambda item: (item["type"] != "局部鼓包", -float(item["width_ratio"]), -int(item["extra_cells"])))
    return records


def build_summary(records):
    def count_by(key):
        counts = {}
        for record in records:
            value = str(record.get(key, "未分类") or "未分类")
            counts[value] = counts.get(value, 0) + 1
        return dict(sorted(counts.items(), key=lambda item: item[0]))

    high_priority = [r for r in records if str(r.get("review_priority", "")).lower() == "high"]
    return {
        "candidate_count": len(records),
        "high_priority_count": len(high_priority),
        "type_counts": count_by("type"),
        "shape_class_counts": count_by("shape_class"),
        "priority_counts": count_by("review_priority"),
    }


def write_outputs(records, values, extent_edges, xs, ys, dx, dy, output_dir, max_figures):
    output_dir.mkdir(parents=True, exist_ok=True)
    figures_dir = output_dir / "局部形态复核图"
    figures_dir.mkdir(parents=True, exist_ok=True)

    fields = [
        "type",
        "polygon_id",
        "candidate_index",
        "polygon_area",
        "polygon_cell_count",
        "polygon_mean_value",
        "polygon_max_value",
        "hole_count",
        "extra_cells",
        "local_width",
        "baseline_width",
        "width_ratio",
        "span_ratio",
        "edge_distance_ratio",
        "support_ratio",
        "aspect_ratio",
        "branch_arm_count",
        "shape_class",
        "review_priority",
        "min_x_index",
        "max_x_index",
        "min_y_index",
        "max_y_index",
        "xmin",
        "xmax",
        "ymin",
        "ymax",
        "figure",
    ]
    clean_records = []
    for i, record in enumerate(records, start=1):
        clean = {key: record.get(key, "") for key in fields if key != "figure"}
        figure = ""
        if i <= max_figures:
            figure_path = figures_dir / f"{i:02d}_{record['type']}_polygon_{record['polygon_id']}.png"
            draw_candidate(figure_path, values, extent_edges, xs, ys, dx, dy, record["_polygon"], record["_candidate"], record["_mask"])
            figure = str(figure_path)
        clean["figure"] = figure
        clean_records.append(clean)

    csv_path = output_dir / "局部形态质量诊断.csv"
    with open(csv_path, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in clean_records:
            writer.writerow(row)

    json_path = output_dir / "局部形态质量诊断.json"
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(clean_records, f, ensure_ascii=False, indent=2)

    summary = build_summary(clean_records)
    summary_path = output_dir / "局部形态质量摘要.json"
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)

    bulges = [r for r in clean_records if r["type"] == "局部鼓包"]
    holes = [r for r in clean_records if r["type"] == "内环复核"]
    high_priority = [r for r in clean_records if r["review_priority"] == "high"]
    md_path = output_dir / "局部形态质量诊断报告.md"
    lines = [
        "# 局部形态质量诊断报告",
        "",
        "本报告用于补充拓扑诊断。拓扑合法只能说明多边形闭合、无自交，不代表局部形态一定符合断层解释习惯。",
        "",
        f"- 局部鼓包候选：{len(bulges)}",
        f"- 内环复核候选：{len(holes)}",
        f"- 高优先级复核候选：{len(high_priority)}",
        f"- 候选总数：{len(clean_records)}",
        "",
        "## 重点候选",
        "",
        "| 类型 | 形态类别 | 优先级 | Polygon | extra_cells | 宽度比 | 内环数 | X范围 | Y范围 |",
        "|---|---|---|---:|---:|---:|---:|---|---|",
    ]
    for row in clean_records[:20]:
        lines.append(
            f"| {row['type']} | {row['shape_class']} | {row['review_priority']} | {row['polygon_id']} | {int(row['extra_cells'])} | {float(row['width_ratio']):.2f} | {row['hole_count']} | "
            f"{float(row['xmin']):.1f}-{float(row['xmax']):.1f} | {float(row['ymin']):.1f}-{float(row['ymax']):.1f} |"
        )
    lines.extend(
        [
            "",
            "## 使用建议",
            "",
            "这些候选不等同于必须删除的错误。它们表示需要进一步判断的局部形态风险：若候选区域跨过低值背景、连接不同根系或形成囊状宽面，应进入局部瘦身/分裂候选；若候选区域确实对应真实断层交切或宽断层带，则可保留。",
        ]
    )
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return csv_path, json_path, md_path


def main():
    configure_chinese_font()
    parser = argparse.ArgumentParser(description="诊断断层多边形的局部鼓包、内环和形态异常候选")
    parser.add_argument("--dat", required=True, help="官方或工区 GeoEast DAT 文件")
    parser.add_argument("--csv", required=True, help="断层多边形结果 CSV")
    parser.add_argument("--out-dir", required=True, help="输出目录")
    parser.add_argument("--width-ratio", type=float, default=2.4, help="局部宽度超过基准宽度的倍数")
    parser.add_argument("--min-extra-cells", type=int, default=24, help="局部鼓包至少多出的栅格数")
    parser.add_argument("--window", type=int, default=11, help="局部宽度滚动窗口")
    parser.add_argument("--max-figures", type=int, default=24, help="最多输出多少张局部复核图")
    parser.add_argument(
        "--profile",
        choices=["review", "repair"],
        default="review",
        help="review 为最终复核口径；repair 保留原始可行动几何候选供局部修形流水线使用。",
    )
    args = parser.parse_args()

    values, xs, ys, dx, dy, extent_edges = read_geoeast_dat(args.dat)
    polygons = read_polygons(args.csv)
    records = diagnose(
        polygons,
        values,
        xs,
        ys,
        dx,
        dy,
        width_ratio=args.width_ratio,
        min_extra_cells=args.min_extra_cells,
        window=args.window,
        context_aware=args.profile == "review",
    )
    csv_path, json_path, md_path = write_outputs(records, values, extent_edges, xs, ys, dx, dy, Path(args.out_dir), args.max_figures)
    print(csv_path)
    print(json_path)
    print(md_path)


if __name__ == "__main__":
    main()
