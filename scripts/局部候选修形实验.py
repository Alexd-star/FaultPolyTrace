# -*- coding: utf-8 -*-
"""Experimental local repair for morphology candidates.

The script is conservative by design. It creates an experimental CSV instead
of overwriting the accepted baseline. Candidate boxes are repaired only when
attribute-supported geometry can remove a local pouch without deleting the
main fault belt.
"""

import argparse
import csv
import json
import math
import os
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib import font_manager
from matplotlib.patches import Polygon as PolygonPatch, Rectangle
import matplotlib.pyplot as plt
import numpy as np
from shapely.geometry import GeometryCollection, MultiPolygon, Polygon, box
from shapely.ops import unary_union

from 局部形态质量诊断 import read_geoeast_dat
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


def repair_polygon_inside_candidate(polygon, candidate_box, support_geometry, min_candidate_retention=0.20):
    """Repair only the polygon portion inside one candidate box.

    The part outside the candidate box is preserved exactly. The inside part is
    clipped to attribute-supported geometry. If clipping would erase too much
    of the candidate-box portion, the original polygon is returned.
    """
    if polygon.is_empty or candidate_box.is_empty or support_geometry.is_empty:
        return polygon
    if not polygon.intersects(candidate_box):
        return polygon

    outside = polygon.difference(candidate_box)
    inside = polygon.intersection(candidate_box)
    if inside.is_empty:
        return polygon

    trimmed_inside = inside.intersection(support_geometry)
    if trimmed_inside.is_empty:
        return polygon

    retention = trimmed_inside.area / max(1e-9, inside.area)
    if retention < min_candidate_retention:
        return polygon

    repaired = unary_union([outside, trimmed_inside]).buffer(0)
    if repaired.is_empty or not repaired.is_valid:
        return polygon
    return repaired


def fill_inner_rings_inside_candidate(polygon, candidate_box, max_hole_area, max_hole_count=3):
    """Fill only small inner rings located inside a local candidate box.

    This is a local cleanup gate for thin fault belts. It does not fill large
    holes or holes outside the candidate box, because those may represent a
    real split response or a separate interpretation object.
    """
    if polygon.is_empty or candidate_box.is_empty or not isinstance(polygon, Polygon):
        return polygon
    if not polygon.interiors:
        return polygon

    kept_holes = []
    filled_count = 0
    for ring in polygon.interiors:
        hole_coords = list(ring.coords)
        hole_polygon = Polygon(hole_coords)
        should_fill = (
            hole_polygon.is_valid
            and not hole_polygon.is_empty
            and hole_polygon.area <= float(max_hole_area)
            and candidate_box.contains(hole_polygon.representative_point())
            and filled_count < int(max_hole_count)
        )
        if should_fill:
            filled_count += 1
            continue
        kept_holes.append(hole_coords)

    if filled_count <= 0:
        return polygon

    repaired = Polygon(list(polygon.exterior.coords), kept_holes).buffer(0)
    if repaired.is_empty or not repaired.is_valid:
        return polygon
    return repaired


def refine_polygon_by_candidate_class(
    polygon,
    candidate,
    support_geometry,
    min_candidate_retention=0.20,
    max_inner_hole_area=250000.0,
    max_inner_hole_count=3,
):
    """Apply the safest local action for one morphology candidate class.

    Candidate boxes are stored as (xmin, xmax, ymin, ymax). The function keeps
    all edits local to the box and returns the original geometry when the class
    is better left as review-only.
    """
    bbox = candidate.get("bbox")
    if not bbox:
        return polygon
    candidate_box = box(float(bbox[0]), float(bbox[2]), float(bbox[1]), float(bbox[3]))
    shape_class = str(candidate.get("shape_class", ""))

    if "内环" in shape_class:
        return fill_inner_rings_inside_candidate(
            polygon,
            candidate_box,
            max_hole_area=max_inner_hole_area,
            max_hole_count=max_inner_hole_count,
        )

    if shape_class in {"局部鼓包", "长段宽带复核", "双臂端点复核", "疑似分支归属错误"}:
        return repair_polygon_inside_candidate(
            polygon,
            candidate_box,
            support_geometry,
            min_candidate_retention=min_candidate_retention,
        )

    return polygon


def geometry_changed_meaningfully(original_geometry, repaired_geometry, min_change_area=1e-6):
    if original_geometry.is_empty or repaired_geometry.is_empty:
        return False
    try:
        return float(original_geometry.symmetric_difference(repaired_geometry).area) > float(min_change_area)
    except Exception:
        return False


def shapely_from_rings(coords, holes):
    polygon = Polygon(coords, holes)
    if not polygon.is_valid:
        polygon = polygon.buffer(0)
    return polygon


def iter_polygons(geometry):
    if geometry.is_empty:
        return []
    if isinstance(geometry, Polygon):
        return [geometry]
    if isinstance(geometry, MultiPolygon):
        return list(geometry.geoms)
    if isinstance(geometry, GeometryCollection):
        return [g for g in geometry.geoms if isinstance(g, Polygon) and not g.is_empty]
    return []


def polygon_to_wkt(polygon):
    exterior = list(polygon.exterior.coords)
    holes = [list(ring.coords) for ring in polygon.interiors]

    def ring_text(coords):
        return ", ".join(f"{float(x):.6f} {float(y):.6f}" for x, y in coords)

    rings = [f"({ring_text(exterior)})"]
    rings.extend(f"({ring_text(hole)})" for hole in holes if len(hole) >= 4)
    return "POLYGON(" + ", ".join(rings) + ")"


def read_result_rows(path):
    rows = []
    with open(path, "r", encoding="utf-8-sig", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        fieldnames = reader.fieldnames or []
        for row in reader:
            rows.append(dict(row))
    return fieldnames, rows


def read_shape_candidates(path, allowed_classes):
    grouped = {}
    if not path or not Path(path).exists():
        return grouped
    with open(path, "r", encoding="utf-8-sig", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            shape_class = row.get("shape_class", "")
            if allowed_classes and shape_class not in allowed_classes:
                continue
            try:
                polygon_id = int(float(row.get("polygon_id", 0)))
                bbox = (
                    float(row.get("xmin", 0.0)),
                    float(row.get("xmax", 0.0)),
                    float(row.get("ymin", 0.0)),
                    float(row.get("ymax", 0.0)),
                )
            except ValueError:
                continue
            grouped.setdefault(polygon_id, []).append({**row, "bbox": bbox})
    return grouped


def cell_box(xs, ys, dx, dy, x_index, y_index):
    x = float(xs[x_index])
    y = float(ys[y_index])
    return box(x - 0.5 * dx, y - 0.5 * dy, x + 0.5 * dx, y + 0.5 * dy)


def build_support_geometry(values, xs, ys, dx, dy, bbox, threshold, buffer_cells):
    x0 = max(0, int(np.searchsorted(xs, bbox[0] - dx, side="left")))
    x1 = min(len(xs), int(np.searchsorted(xs, bbox[1] + dx, side="right")))
    y0 = max(0, int(np.searchsorted(ys, bbox[2] - dy, side="left")))
    y1 = min(len(ys), int(np.searchsorted(ys, bbox[3] + dy, side="right")))

    cells = []
    for y in range(y0, y1):
        for x in range(x0, x1):
            value = values[y, x]
            if np.isfinite(value) and value >= threshold:
                cells.append(cell_box(xs, ys, dx, dy, x, y))
    if not cells:
        return GeometryCollection()
    support = unary_union(cells)
    buffer_distance = max(dx, dy) * max(0.0, float(buffer_cells))
    if buffer_distance > 0:
        support = support.buffer(buffer_distance, join_style=2)
    return support.buffer(0)


def geometry_mask(geometry, width, height, xs, ys, dx, dy):
    mask = np.zeros((height, width), dtype=bool)
    for polygon in iter_polygons(geometry):
        coords = [(float(x), float(y)) for x, y in polygon.exterior.coords[:-1]]
        holes = [[(float(x), float(y)) for x, y in ring.coords[:-1]] for ring in polygon.interiors]
        if len(coords) >= 3:
            mask |= rasterize_polygon(coords, width, height, xs, ys, dx, dy, holes)
    return mask


def summarize_geometry(geometry, values, xs, ys, dx, dy):
    height, width = values.shape
    mask = geometry_mask(geometry, width, height, xs, ys, dx, dy)
    covered = values[mask]
    return {
        "area": float(geometry.area),
        "perimeter": float(geometry.length),
        "cell_count": int(np.count_nonzero(mask)),
        "mean_value": float(np.mean(covered)) if covered.size else 0.0,
        "max_value": float(np.max(covered)) if covered.size else 0.0,
        "mask": mask,
    }


def total_mask_from_rows(rows, values, xs, ys, dx, dy):
    height, width = values.shape
    total = np.zeros((height, width), dtype=bool)
    for row in rows:
        if str(row.get("status", "")).lower() != "kept":
            continue
        coords, holes = parse_wkt_polygon_with_holes(row.get("wkt", ""))
        if len(coords) >= 3:
            total |= rasterize_polygon(coords, width, height, xs, ys, dx, dy, holes)
    return total


def coverage_metrics(values, mask, strong_threshold=0.95, weak_threshold=0.66):
    strong = values >= strong_threshold
    weak = values >= weak_threshold
    nonzero = values > 0

    def ratio(target):
        denom = int(np.count_nonzero(target))
        return float(np.count_nonzero(mask & target)) / denom if denom else 0.0

    return {
        "strong_coverage": ratio(strong),
        "weak_coverage": ratio(weak),
        "nonzero_coverage": ratio(nonzero),
        "covered_cells": int(np.count_nonzero(mask)),
    }


def draw_repair_figure(path, values, extent_edges, original_geometry, repaired_geometry, candidates, title):
    fig, axes = plt.subplots(1, 2, figsize=(11.0, 4.6), dpi=220)
    for ax, geom, name, color in [
        (axes[0], original_geometry, "修形前", "#005BBB"),
        (axes[1], repaired_geometry, "修形后", "#007A3D"),
    ]:
        weak = np.ma.masked_where(values <= 0, values)
        ax.imshow(weak, extent=extent_edges, origin="lower", cmap="viridis", interpolation="nearest", vmin=0, vmax=1, alpha=0.72, aspect="equal")
        for polygon in iter_polygons(geom):
            arr = np.asarray(polygon.exterior.coords, dtype=float)
            ax.add_patch(PolygonPatch(arr, closed=True, facecolor="none", edgecolor=color, linewidth=0.9))
            for hole in polygon.interiors:
                h = np.asarray(hole.coords, dtype=float)
                ax.plot(h[:, 0], h[:, 1], color=color, linewidth=0.55, linestyle="--")
        for candidate in candidates:
            b = candidate["bbox"]
            ax.add_patch(Rectangle((b[0], b[2]), b[1] - b[0], b[3] - b[2], fill=False, edgecolor="#ffb000", linewidth=1.3))
        ax.set_title(name)
        ax.tick_params(labelsize=7)
    xmin, xmax, ymin, ymax = view_bounds_for_geometries_and_candidates(
        [original_geometry, repaired_geometry],
        candidates,
        min_pad=100.0,
        pad_ratio=0.45,
    )
    for ax in axes:
        ax.set_xlim(xmin, xmax)
        ax.set_ylim(ymin, ymax)
        ax.set_xlabel("X")
        ax.set_ylabel("Y")
    fig.suptitle(title)
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)


def view_bounds_for_geometries_and_candidates(geometries, candidates, min_pad=100.0, pad_ratio=0.45):
    """Return plot bounds as xmin, xmax, ymin, ymax.

    Shapely geometry bounds are (xmin, ymin, xmax, ymax), while candidate CSV
    boxes are stored as (xmin, xmax, ymin, ymax). Keeping this conversion in
    one function prevents review figures from zooming to a wrong coordinate
    range.
    """
    xmin_values = []
    xmax_values = []
    ymin_values = []
    ymax_values = []
    for geometry in geometries:
        if geometry.is_empty:
            continue
        xmin, ymin, xmax, ymax = geometry.bounds
        xmin_values.append(float(xmin))
        xmax_values.append(float(xmax))
        ymin_values.append(float(ymin))
        ymax_values.append(float(ymax))
    for candidate in candidates:
        xmin, xmax, ymin, ymax = candidate["bbox"]
        xmin_values.append(float(xmin))
        xmax_values.append(float(xmax))
        ymin_values.append(float(ymin))
        ymax_values.append(float(ymax))
    if not xmin_values:
        return (0.0, 1.0, 0.0, 1.0)
    xmin = min(xmin_values)
    xmax = max(xmax_values)
    ymin = min(ymin_values)
    ymax = max(ymax_values)
    pad_x = max(float(min_pad), (xmax - xmin) * float(pad_ratio))
    pad_y = max(float(min_pad), (ymax - ymin) * float(pad_ratio))
    return (xmin - pad_x, xmax + pad_x, ymin - pad_y, ymax + pad_y)


def main():
    configure_chinese_font()
    parser = argparse.ArgumentParser(description="候选框内局部修形实验，不覆盖正式基线")
    parser.add_argument("--dat", required=True)
    parser.add_argument("--csv", required=True)
    parser.add_argument("--shape-candidates", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--support-threshold", type=float, default=0.66)
    parser.add_argument("--support-buffer-cells", type=float, default=2.0)
    parser.add_argument("--min-candidate-retention", type=float, default=0.20)
    parser.add_argument("--max-area-loss-ratio", type=float, default=0.18)
    parser.add_argument("--max-area-gain-ratio", type=float, default=0.05)
    parser.add_argument("--max-inner-hole-area-cells", type=float, default=120.0)
    parser.add_argument("--max-inner-hole-count", type=int, default=3)
    parser.add_argument("--min-strong-coverage-delta", type=float, default=-0.0002)
    parser.add_argument(
        "--classes",
        default="疑似分支归属错误,局部鼓包,长段宽带复核,双臂端点复核,内环复核,属性支撑内环复核",
        help="逗号分隔的候选类别。默认对三类候选做实验。",
    )
    args = parser.parse_args()

    values, xs, ys, dx, dy, extent_edges = read_geoeast_dat(args.dat)
    out_dir = Path(args.out_dir)
    figures_dir = out_dir / "局部修形对比图"
    out_dir.mkdir(parents=True, exist_ok=True)
    figures_dir.mkdir(parents=True, exist_ok=True)

    fieldnames, rows = read_result_rows(args.csv)
    allowed_classes = {c.strip() for c in args.classes.split(",") if c.strip()}
    candidates_by_polygon = read_shape_candidates(args.shape_candidates, allowed_classes)

    baseline_mask = total_mask_from_rows(rows, values, xs, ys, dx, dy)
    baseline_metrics = coverage_metrics(values, baseline_mask)
    repaired_rows = []
    repair_records = []
    next_id = 1

    for row in rows:
        if str(row.get("status", "")).lower() != "kept":
            repaired_rows.append(row)
            continue

        try:
            original_id = int(float(row.get("id", "0")))
        except ValueError:
            original_id = 0

        coords, holes = parse_wkt_polygon_with_holes(row.get("wkt", ""))
        if len(coords) < 3 or original_id not in candidates_by_polygon:
            new_row = dict(row)
            new_row["id"] = str(next_id)
            next_id += 1
            repaired_rows.append(new_row)
            continue

        original_geometry = shapely_from_rings(coords, holes)
        repaired_geometry = original_geometry
        support_union = []
        for candidate in candidates_by_polygon[original_id]:
            candidate_geometry = box(candidate["bbox"][0], candidate["bbox"][2], candidate["bbox"][1], candidate["bbox"][3])
            support_geometry = build_support_geometry(
                values,
                xs,
                ys,
                dx,
                dy,
                candidate["bbox"],
                args.support_threshold,
                args.support_buffer_cells,
            )
            support_union.append(support_geometry)
            repaired_geometry = refine_polygon_by_candidate_class(
                repaired_geometry,
                candidate,
                support_geometry,
                min_candidate_retention=args.min_candidate_retention,
                max_inner_hole_area=float(args.max_inner_hole_area_cells) * float(dx) * float(dy),
                max_inner_hole_count=args.max_inner_hole_count,
            )

        original_area = float(original_geometry.area)
        repaired_area = float(repaired_geometry.area)
        area_loss_ratio = (original_area - repaired_area) / max(1e-9, original_area)
        area_gain_ratio = (repaired_area - original_area) / max(1e-9, original_area)

        candidate_rows = candidates_by_polygon[original_id]
        accepted = (
            repaired_geometry.is_valid
            and not repaired_geometry.is_empty
            and geometry_changed_meaningfully(original_geometry, repaired_geometry, min_change_area=max(1e-6, float(dx) * float(dy) * 0.25))
            and area_loss_ratio <= args.max_area_loss_ratio
            and area_gain_ratio <= args.max_area_gain_ratio
        )

        if accepted:
            repair_records.append(
                {
                    "polygon_id": original_id,
                    "candidate_count": len(candidate_rows),
                    "shape_classes": ";".join(sorted({c.get("shape_class", "") for c in candidate_rows})),
                    "original_area": original_area,
                    "repaired_area": repaired_area,
                    "area_loss_ratio": area_loss_ratio,
                    "area_gain_ratio": area_gain_ratio,
                    "status": "accepted_precheck",
                }
            )
            draw_repair_figure(
                figures_dir / f"polygon_{original_id}_repair_preview.png",
                values,
                extent_edges,
                original_geometry,
                repaired_geometry,
                candidate_rows,
                f"polygon {original_id} 候选框内局部修形实验",
            )
        else:
            repair_records.append(
                {
                    "polygon_id": original_id,
                    "candidate_count": len(candidate_rows),
                    "shape_classes": ";".join(sorted({c.get("shape_class", "") for c in candidate_rows})),
                    "original_area": original_area,
                    "repaired_area": repaired_area,
                    "area_loss_ratio": area_loss_ratio,
                    "area_gain_ratio": area_gain_ratio,
                    "status": "rejected_precheck",
                }
            )
            repaired_geometry = original_geometry

        for part in iter_polygons(repaired_geometry):
            summary = summarize_geometry(part, values, xs, ys, dx, dy)
            new_row = dict(row)
            new_row["id"] = str(next_id)
            new_row["status"] = "kept"
            new_row["area"] = f"{summary['area']:.6f}"
            new_row["perimeter"] = f"{summary['perimeter']:.6f}"
            new_row["cell_count"] = str(summary["cell_count"])
            new_row["mean_value"] = f"{summary['mean_value']:.6f}"
            new_row["max_value"] = f"{summary['max_value']:.6f}"
            new_row["wkt"] = polygon_to_wkt(part)
            next_id += 1
            repaired_rows.append(new_row)

    repaired_mask = total_mask_from_rows(repaired_rows, values, xs, ys, dx, dy)
    repaired_metrics = coverage_metrics(values, repaired_mask)
    strong_delta = repaired_metrics["strong_coverage"] - baseline_metrics["strong_coverage"]
    if strong_delta < args.min_strong_coverage_delta:
        for record in repair_records:
            if record["status"] == "accepted_precheck":
                record["status"] = "rejected_global_strong_coverage"
        repaired_rows = []
        next_id = 1
        for row in rows:
            new_row = dict(row)
            if str(new_row.get("status", "")).lower() == "kept":
                new_row["id"] = str(next_id)
                next_id += 1
            repaired_rows.append(new_row)
        repaired_mask = total_mask_from_rows(repaired_rows, values, xs, ys, dx, dy)
        repaired_metrics = coverage_metrics(values, repaired_mask)

    output_csv = out_dir / "local_candidate_repair_experiment.csv"
    output_fields = list(fieldnames)
    if not output_fields:
        output_fields = ["id", "status", "area", "perimeter", "cell_count", "mean_value", "max_value", "wkt"]
    with open(output_csv, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=output_fields, extrasaction="ignore")
        writer.writeheader()
        for row in repaired_rows:
            writer.writerow(row)

    records_csv = out_dir / "局部修形实验明细.csv"
    with open(records_csv, "w", encoding="utf-8-sig", newline="") as f:
        fields = ["polygon_id", "candidate_count", "shape_classes", "original_area", "repaired_area", "area_loss_ratio", "area_gain_ratio", "status"]
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for record in repair_records:
            writer.writerow(record)

    summary = {
        "input_csv": str(Path(args.csv)),
        "output_csv": str(output_csv),
        "candidate_polygon_count": len(candidates_by_polygon),
        "accepted_precheck_count": sum(1 for r in repair_records if r["status"] == "accepted_precheck"),
        "rejected_count": sum(1 for r in repair_records if r["status"] != "accepted_precheck"),
        "baseline_metrics": baseline_metrics,
        "repaired_metrics": repaired_metrics,
        "strong_coverage_delta": repaired_metrics["strong_coverage"] - baseline_metrics["strong_coverage"],
        "weak_coverage_delta": repaired_metrics["weak_coverage"] - baseline_metrics["weak_coverage"],
        "nonzero_coverage_delta": repaired_metrics["nonzero_coverage"] - baseline_metrics["nonzero_coverage"],
    }
    summary_path = out_dir / "局部修形实验摘要.json"
    summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    report = [
        "# 局部候选修形实验报告",
        "",
        "本实验只处理局部形态候选框，不覆盖当前正式基线。候选框外的几何保持不动，候选框内仅保留有属性支撑的局部包络。若修形导致强响应覆盖率下降超过阈值或几何预检查失败，则自动回退。",
        "",
        "## 关键指标",
        "",
        f"- 候选 polygon 数量：{len(candidates_by_polygon)}",
        f"- 预检查接受数量：{summary['accepted_precheck_count']}",
        f"- 回退/拒绝数量：{summary['rejected_count']}",
        f"- 强响应覆盖率变化：{summary['strong_coverage_delta']:.6%}",
        f"- 弱响应覆盖率变化：{summary['weak_coverage_delta']:.6%}",
        f"- 非零响应覆盖率变化：{summary['nonzero_coverage_delta']:.6%}",
        "",
        "## 输出",
        "",
        f"- 实验 CSV：`{output_csv}`",
        f"- 修形明细：`{records_csv}`",
        f"- 修形对比图：`{figures_dir}`",
        f"- 摘要 JSON：`{summary_path}`",
    ]
    (out_dir / "局部候选修形实验报告.md").write_text("\n".join(report) + "\n", encoding="utf-8")

    print(output_csv)
    print(summary_path)
    print(out_dir / "局部候选修形实验报告.md")


if __name__ == "__main__":
    main()
