# -*- coding: utf-8 -*-
"""Experimental branch-attribution splitting for local morphology candidates.

This script is intentionally conservative. It only targets candidates already
classified as high-priority branch-attribution risks and writes an experimental
CSV instead of replacing the accepted baseline.
"""

import argparse
import csv
import json
import os
import sys
from pathlib import Path

from shapely.geometry import GeometryCollection, MultiPolygon, Polygon, box
from shapely.ops import unary_union

from 局部候选修形实验 import (
    build_support_geometry,
    configure_chinese_font,
    coverage_metrics,
    draw_repair_figure,
    iter_polygons,
    polygon_to_wkt,
    read_result_rows,
    shapely_from_rings,
    summarize_geometry,
    total_mask_from_rows,
)
from 局部形态质量诊断 import read_geoeast_dat
from wkt_geometry import parse_wkt_polygon_with_holes

csv.field_size_limit(min(sys.maxsize, 2_147_483_647))


def _polygon_components(geometry, min_area=0.0):
    parts = []
    if geometry.is_empty:
        return parts
    if isinstance(geometry, Polygon):
        parts = [geometry]
    elif isinstance(geometry, MultiPolygon):
        parts = list(geometry.geoms)
    elif isinstance(geometry, GeometryCollection):
        parts = [part for part in geometry.geoms if isinstance(part, Polygon)]
    return [part for part in parts if not part.is_empty and part.area >= float(min_area)]


def split_polygon_by_support_components(
    polygon,
    candidate_box,
    support_geometry,
    min_component_area=400.0,
    min_retention_ratio=0.45,
):
    """Split candidate-box geometry only when support proves multiple branches.

    The part outside the candidate box is preserved. The part inside the box is
    replaced by sufficiently large support components. When evidence is weak,
    the original polygon is returned with a rejection status.
    """
    decision = {
        "status": "rejected_no_intersection",
        "component_count": 0,
        "retention_ratio": 1.0,
        "original_inside_area": 0.0,
        "kept_inside_area": 0.0,
    }
    if polygon.is_empty or candidate_box.is_empty or support_geometry.is_empty:
        decision["status"] = "rejected_empty_input"
        return polygon, decision
    if not polygon.intersects(candidate_box):
        return polygon, decision

    inside = polygon.intersection(candidate_box)
    outside = polygon.difference(candidate_box)
    if inside.is_empty:
        decision["status"] = "rejected_empty_inside"
        return polygon, decision

    supported_inside = inside.intersection(support_geometry).buffer(0)
    components = _polygon_components(supported_inside, min_area=min_component_area)
    decision["component_count"] = len(components)
    decision["original_inside_area"] = float(inside.area)

    if len(components) < 2:
        decision["status"] = "rejected_not_enough_components"
        return polygon, decision

    kept_inside = unary_union(components).buffer(0)
    kept_area = float(kept_inside.area)
    retention = kept_area / max(1e-9, float(inside.area))
    decision["kept_inside_area"] = kept_area
    decision["retention_ratio"] = retention
    if retention < float(min_retention_ratio):
        decision["status"] = "rejected_low_retention"
        return polygon, decision

    repaired = unary_union([outside, kept_inside]).buffer(0)
    if repaired.is_empty or not repaired.is_valid:
        decision["status"] = "rejected_invalid_result"
        return polygon, decision

    decision["status"] = "accepted_precheck"
    return repaired, decision


def read_branch_candidates(path):
    grouped = {}
    with open(path, "r", encoding="utf-8-sig", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row.get("shape_class", "") != "疑似分支归属错误":
                continue
            if str(row.get("review_priority", "")).lower() != "high":
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


def _write_result_csv(path, fieldnames, rows):
    output_fields = list(fieldnames or [])
    if not output_fields:
        output_fields = ["id", "status", "area", "perimeter", "cell_count", "mean_value", "max_value", "wkt"]
    with open(path, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=output_fields, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def run_experiment(args):
    values, xs, ys, dx, dy, extent_edges = read_geoeast_dat(args.dat)
    out_dir = Path(args.out_dir)
    figures_dir = out_dir / "分支归属拆分对比图"
    out_dir.mkdir(parents=True, exist_ok=True)
    figures_dir.mkdir(parents=True, exist_ok=True)

    fieldnames, rows = read_result_rows(args.csv)
    candidates_by_polygon = read_branch_candidates(args.shape_candidates)
    baseline_mask = total_mask_from_rows(rows, values, xs, ys, dx, dy)
    baseline_metrics = coverage_metrics(values, baseline_mask)

    repaired_rows = []
    records = []
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
        candidate_rows = candidates_by_polygon[original_id]
        decisions = []
        for candidate in candidate_rows:
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
            repaired_geometry, decision = split_polygon_by_support_components(
                repaired_geometry,
                candidate_geometry,
                support_geometry,
                min_component_area=args.min_component_area,
                min_retention_ratio=args.min_retention_ratio,
            )
            decisions.append(decision)

        status = "accepted_precheck" if any(d["status"] == "accepted_precheck" for d in decisions) else ";".join(sorted({d["status"] for d in decisions}))
        area_loss_ratio = (float(original_geometry.area) - float(repaired_geometry.area)) / max(1e-9, float(original_geometry.area))
        records.append(
            {
                "polygon_id": original_id,
                "candidate_count": len(candidate_rows),
                "status": status,
                "component_counts": ";".join(str(d["component_count"]) for d in decisions),
                "retention_ratios": ";".join(f"{d['retention_ratio']:.6f}" for d in decisions),
                "original_area": float(original_geometry.area),
                "repaired_area": float(repaired_geometry.area),
                "area_loss_ratio": area_loss_ratio,
            }
        )

        if status == "accepted_precheck":
            draw_repair_figure(
                figures_dir / f"polygon_{original_id}_branch_split_preview.png",
                values,
                extent_edges,
                original_geometry,
                repaired_geometry,
                candidate_rows,
                f"polygon {original_id} 分支归属拆分实验",
            )
        else:
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
    weak_delta = repaired_metrics["weak_coverage"] - baseline_metrics["weak_coverage"]

    if strong_delta < args.min_strong_coverage_delta or weak_delta < args.min_weak_coverage_delta:
        for record in records:
            if record["status"] == "accepted_precheck":
                record["status"] = "rejected_global_coverage"
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

    output_csv = out_dir / "branch_attribution_split_experiment.csv"
    _write_result_csv(output_csv, fieldnames, repaired_rows)

    records_csv = out_dir / "分支归属拆分实验明细.csv"
    fields = ["polygon_id", "candidate_count", "status", "component_counts", "retention_ratios", "original_area", "repaired_area", "area_loss_ratio"]
    with open(records_csv, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for record in records:
            writer.writerow(record)

    summary = {
        "input_csv": str(Path(args.csv)),
        "output_csv": str(output_csv),
        "candidate_polygon_count": len(candidates_by_polygon),
        "accepted_precheck_count": sum(1 for r in records if r["status"] == "accepted_precheck"),
        "rejected_count": sum(1 for r in records if r["status"] != "accepted_precheck"),
        "baseline_metrics": baseline_metrics,
        "repaired_metrics": repaired_metrics,
        "strong_coverage_delta": repaired_metrics["strong_coverage"] - baseline_metrics["strong_coverage"],
        "weak_coverage_delta": repaired_metrics["weak_coverage"] - baseline_metrics["weak_coverage"],
        "nonzero_coverage_delta": repaired_metrics["nonzero_coverage"] - baseline_metrics["nonzero_coverage"],
    }
    summary_path = out_dir / "分支归属拆分实验摘要.json"
    summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    report_path = out_dir / "分支归属拆分实验报告.md"
    lines = [
        "# 分支归属拆分实验报告",
        "",
        "本实验只处理已被局部形态诊断标记为 `疑似分支归属错误` 且优先级为 high 的候选。普通局部鼓包、边界截断和弱响应残余不参与本轮拆分。",
        "",
        "## 依据",
        "",
        "断层多边形生产应用要求结果既要覆盖断层带，又不能把邻近但不同脉络的断层合成大面。因此本实验只在候选框内存在两个以上足够大的属性支撑分支时尝试拆分，证据不足则回退。",
        "",
        "## 指标",
        "",
        f"- 候选 polygon 数量：{summary['candidate_polygon_count']}",
        f"- 预检查接受数量：{summary['accepted_precheck_count']}",
        f"- 回退/拒绝数量：{summary['rejected_count']}",
        f"- 强响应覆盖率变化：{summary['strong_coverage_delta']:.6%}",
        f"- 弱响应覆盖率变化：{summary['weak_coverage_delta']:.6%}",
        f"- 非零响应覆盖率变化：{summary['nonzero_coverage_delta']:.6%}",
        "",
        "## 输出",
        "",
        f"- 实验 CSV：`{output_csv}`",
        f"- 明细 CSV：`{records_csv}`",
        f"- 摘要 JSON：`{summary_path}`",
        f"- 对比图目录：`{figures_dir}`",
    ]
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    return output_csv, summary_path, report_path


def main():
    configure_chinese_font()
    parser = argparse.ArgumentParser(description="高优先级分支归属候选的局部拆分实验")
    parser.add_argument("--dat", required=True)
    parser.add_argument("--csv", required=True)
    parser.add_argument("--shape-candidates", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--support-threshold", type=float, default=0.66)
    parser.add_argument("--support-buffer-cells", type=float, default=0.75)
    parser.add_argument("--min-component-area", type=float, default=400.0)
    parser.add_argument("--min-retention-ratio", type=float, default=0.45)
    parser.add_argument("--min-strong-coverage-delta", type=float, default=-0.0002)
    parser.add_argument("--min-weak-coverage-delta", type=float, default=-0.001)
    args = parser.parse_args()

    output_csv, summary_path, report_path = run_experiment(args)
    print(output_csv)
    print(summary_path)
    print(report_path)


if __name__ == "__main__":
    main()
