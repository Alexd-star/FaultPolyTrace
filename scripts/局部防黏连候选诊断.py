# -*- coding: utf-8 -*-
import argparse
import csv
import json
import os
import sys
from collections import deque
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib import font_manager
from matplotlib.patches import Polygon as PolygonPatch
import matplotlib.pyplot as plt
import numpy as np

from wkt_geometry import parse_wkt_polygon_with_holes, rasterize_polygon

csv.field_size_limit(min(sys.maxsize, 2_147_483_647))


def configure_chinese_font():
    for path in [r"C:\Windows\Fonts\msyh.ttc", r"C:\Windows\Fonts\simhei.ttf", r"C:\Windows\Fonts\simsun.ttc"]:
        if os.path.exists(path):
            font_manager.fontManager.addfont(path)
            name = font_manager.FontProperties(fname=path).get_name()
            plt.rcParams["font.sans-serif"] = [name, "DejaVu Sans"]
            plt.rcParams["axes.unicode_minus"] = False
            return


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
        raise RuntimeError(f"未读取到有效网格数据：{path}")

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
    polygons = {}
    with open(path, "r", encoding="utf-8", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row.get("status", "").lower() != "kept":
                continue
            coords, holes = parse_wkt_polygon_with_holes(row.get("wkt", ""))
            if len(coords) < 3:
                continue
            poly_id = int(float(row.get("id", len(polygons) + 1) or 0))
            polygons[poly_id] = {
                "id": poly_id,
                "area": float(row.get("area", 0.0) or 0.0),
                "cell_count": int(float(row.get("cell_count", 0) or 0)),
                "mean_value": float(row.get("mean_value", 0.0) or 0.0),
                "max_value": float(row.get("max_value", 0.0) or 0.0),
                "coords": coords,
                "holes": holes,
            }
    return polygons


def read_suspect_ids(path):
    if not path or not Path(path).exists():
        return {}, []
    rows = {}
    order = []
    with open(path, "r", encoding="utf-8-sig", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                poly_id = int(float(row.get("id", "")))
            except ValueError:
                continue
            rows[poly_id] = row
            order.append(poly_id)
    return rows, order


def connected_components(mask, min_cells=1):
    height, width = mask.shape
    visited = np.zeros_like(mask, dtype=bool)
    labels = np.zeros(mask.shape, dtype=np.int32)
    components = []
    offsets = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]
    label = 0
    for y0 in range(height):
        for x0 in range(width):
            if not mask[y0, x0] or visited[y0, x0]:
                continue
            label += 1
            queue = deque([(x0, y0)])
            visited[y0, x0] = True
            cells = []
            while queue:
                x, y = queue.popleft()
                labels[y, x] = label
                cells.append((x, y))
                for ox, oy in offsets:
                    nx, ny = x + ox, y + oy
                    if 0 <= nx < width and 0 <= ny < height and mask[ny, nx] and not visited[ny, nx]:
                        visited[ny, nx] = True
                        queue.append((nx, ny))
            if len(cells) >= min_cells:
                arr = np.asarray(cells, dtype=int)
                components.append(
                    {
                        "label": label,
                        "cells": cells,
                        "cell_count": int(len(cells)),
                        "min_x": int(arr[:, 0].min()),
                        "max_x": int(arr[:, 0].max()),
                        "min_y": int(arr[:, 1].min()),
                        "max_y": int(arr[:, 1].max()),
                    }
                )
    components.sort(key=lambda item: item["cell_count"], reverse=True)
    return components, labels


def find_bridge_cells(low_mask, labels, major_labels, radius):
    bridge = np.zeros_like(low_mask, dtype=bool)
    height, width = low_mask.shape
    major_labels = set(major_labels)
    for y in range(height):
        for x in range(width):
            if not low_mask[y, x]:
                continue
            touched = set()
            for yy in range(max(0, y - radius), min(height - 1, y + radius) + 1):
                for xx in range(max(0, x - radius), min(width - 1, x + radius) + 1):
                    label = int(labels[yy, xx])
                    if label in major_labels:
                        touched.add(label)
            if len(touched) >= 2:
                bridge[y, x] = True
    return bridge


def extent_from_mask(mask, xs, ys, pad):
    yy, xx = np.nonzero(mask)
    if len(xx) == 0:
        return None
    min_x = max(0, int(xx.min()) - pad)
    max_x = min(len(xs) - 1, int(xx.max()) + pad)
    min_y = max(0, int(yy.min()) - pad)
    max_y = min(len(ys) - 1, int(yy.max()) + pad)
    dx = abs(xs[1] - xs[0]) if len(xs) > 1 else 1.0
    dy = abs(ys[1] - ys[0]) if len(ys) > 1 else 1.0
    return (
        float(xs[min_x] - 0.5 * dx),
        float(xs[max_x] + 0.5 * dx),
        float(ys[min_y] - 0.5 * dy),
        float(ys[max_y] + 0.5 * dy),
    )


def draw_candidate(path, values, extent_edges, xs, ys, poly, poly_mask, low_bridge_mask, nonlow_labels, title):
    fig, axes = plt.subplots(1, 2, figsize=(13, 5.2), dpi=220)
    weak = np.ma.masked_where(values <= 0, values)
    strong = np.ma.masked_where(values < 0.66, values)
    local_extent = extent_from_mask(poly_mask, xs, ys, 18) or extent_edges
    arr = np.asarray(poly["coords"], dtype=float)
    for ax in axes:
        ax.set_facecolor("white")
        ax.imshow(weak, extent=extent_edges, origin="lower", cmap="gray_r", interpolation="nearest", vmin=0.30, vmax=1.0, alpha=0.42, aspect="equal")
        ax.imshow(strong, extent=extent_edges, origin="lower", cmap="cool", interpolation="nearest", vmin=0.66, vmax=1.0, alpha=0.78, aspect="equal")
        ax.add_patch(PolygonPatch(arr, closed=True, facecolor="#ff2a2a", edgecolor="#d00000", linewidth=0.9, alpha=0.10))
        ax.plot(arr[:, 0], arr[:, 1], color="#d00000", linewidth=0.65)
        ax.set_xlim(local_extent[0], local_extent[1])
        ax.set_ylim(local_extent[2], local_extent[3])
        ax.tick_params(labelsize=7, colors="#606870")
        ax.set_xlabel("X")
        ax.set_ylabel("Y")

    part_overlay = np.ma.masked_where(nonlow_labels <= 0, nonlow_labels.astype(float))
    axes[0].imshow(part_overlay, extent=extent_edges, origin="lower", cmap="tab20", interpolation="nearest", alpha=0.36, aspect="equal")
    axes[0].set_title("切掉低属性后形成的主体")

    bridge_overlay = np.ma.masked_where(~low_bridge_mask, low_bridge_mask.astype(float))
    axes[1].imshow(bridge_overlay, extent=extent_edges, origin="lower", cmap="autumn", interpolation="nearest", vmin=0, vmax=1, alpha=0.92, aspect="equal")
    axes[1].set_title("低属性桥接候选")

    fig.suptitle(title)
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)


def ratio(numerator, denominator):
    return float(numerator) / float(denominator) if denominator else 0.0


def main():
    configure_chinese_font()
    parser = argparse.ArgumentParser(description="生成局部防黏连候选诊断报告")
    parser.add_argument("--dat", required=True)
    parser.add_argument("--csv", required=True)
    parser.add_argument("--suspect-csv", default="")
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--strong-threshold", type=float, default=0.95)
    parser.add_argument("--weak-threshold", type=float, default=0.66)
    parser.add_argument("--low-threshold", type=float, default=0.05)
    parser.add_argument("--bridge-radius", type=int, default=2)
    parser.add_argument("--min-major-part-ratio", type=float, default=0.08)
    parser.add_argument("--min-major-part-cells", type=int, default=80)
    parser.add_argument("--min-bridge-cells", type=int, default=6)
    parser.add_argument("--max-figures", type=int, default=20)
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    figure_dir = out_dir / "局部防黏连候选图"
    out_dir.mkdir(parents=True, exist_ok=True)
    figure_dir.mkdir(parents=True, exist_ok=True)

    values, xs, ys, dx, dy, extent_edges = read_geoeast_dat(args.dat)
    polygons = read_polygons(args.csv)
    suspect_rows, suspect_order = read_suspect_ids(args.suspect_csv)
    if suspect_order:
        candidate_ids = [pid for pid in suspect_order if pid in polygons]
    else:
        candidate_ids = sorted(polygons.keys(), key=lambda pid: polygons[pid]["area"], reverse=True)[:80]

    rows = []
    figures = []
    height, width = values.shape
    for poly_id in candidate_ids:
        poly = polygons[poly_id]
        poly_mask = rasterize_polygon(poly["coords"], width, height, xs, ys, dx, dy, poly.get("holes", []))
        polygon_cells = int(np.count_nonzero(poly_mask))
        if polygon_cells <= 0:
            continue
        low_mask = poly_mask & (values <= args.low_threshold)
        weak_mask = poly_mask & (values >= args.weak_threshold)
        strong_mask = poly_mask & (values >= args.strong_threshold)
        nonlow_mask = poly_mask & (values > args.low_threshold)
        parts, labels = connected_components(nonlow_mask)
        min_major = max(args.min_major_part_cells, int(round(polygon_cells * args.min_major_part_ratio)))
        major_parts = [part for part in parts if part["cell_count"] >= min_major]
        bridge_mask = find_bridge_cells(low_mask, labels, [p["label"] for p in major_parts], args.bridge_radius)
        bridge_cells = int(np.count_nonzero(bridge_mask))
        strong_parts, _ = connected_components(strong_mask, min_cells=3)
        weak_parts, _ = connected_components(weak_mask, min_cells=3)
        crossing_like = len(strong_parts) >= 12 and len(weak_parts) >= 4 and len(major_parts) >= 2

        suspect = suspect_rows.get(poly_id, {})
        risk_level = suspect.get("risk_level", "")
        risk_score = suspect.get("risk_score", "")
        low_ratio = ratio(np.count_nonzero(low_mask), polygon_cells)
        recommendation = "保留观察"
        if crossing_like and bridge_cells >= args.min_bridge_cells:
            recommendation = "交切复合区保留复核"
        elif len(major_parts) >= 2 and bridge_cells >= args.min_bridge_cells and (risk_level == "高" or low_ratio >= 0.60):
            recommendation = "建议进入局部拆分候选"
        elif len(major_parts) >= 2 and bridge_cells >= args.min_bridge_cells:
            recommendation = "建议人工复核后决定是否拆分"

        row = {
            "id": poly_id,
            "area": poly["area"],
            "polygon_cells": polygon_cells,
            "risk_level": risk_level,
            "risk_score": risk_score,
            "low_value_ratio": low_ratio,
            "strong_component_count": len(strong_parts),
            "weak_component_count": len(weak_parts),
            "nonlow_part_count": len(parts),
            "major_part_count": len(major_parts),
            "largest_part_cells": major_parts[0]["cell_count"] if major_parts else 0,
            "second_part_cells": major_parts[1]["cell_count"] if len(major_parts) > 1 else 0,
            "bridge_cells": bridge_cells,
            "recommendation": recommendation,
        }
        rows.append(row)

        if recommendation != "保留观察" and len(figures) < args.max_figures:
            figure_path = figure_dir / f"候选_{len(figures) + 1:02d}_多边形{poly_id}.png"
            draw_candidate(
                figure_path,
                values,
                extent_edges,
                xs,
                ys,
                poly,
                poly_mask,
                bridge_mask,
                labels,
                f"多边形 {poly_id}: {recommendation}",
            )
            figures.append(figure_path)

    rows.sort(key=lambda r: (r["recommendation"] == "建议进入局部拆分候选", r["bridge_cells"], r["area"]), reverse=True)
    fields = [
        "id", "area", "polygon_cells", "risk_level", "risk_score", "low_value_ratio",
        "strong_component_count", "weak_component_count", "nonlow_part_count", "major_part_count",
        "largest_part_cells", "second_part_cells", "bridge_cells", "recommendation",
    ]
    with open(out_dir / "局部防黏连候选.csv", "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)

    split_candidates = [r for r in rows if r["recommendation"] == "建议进入局部拆分候选"]
    review_candidates = [r for r in rows if r["recommendation"] == "建议人工复核后决定是否拆分"]
    crossing_candidates = [r for r in rows if r["recommendation"] == "交切复合区保留复核"]
    summary = {
        "input_dat": str(Path(args.dat)),
        "input_csv": str(Path(args.csv)),
        "suspect_csv": str(Path(args.suspect_csv)) if args.suspect_csv else "",
        "checked_polygon_count": len(rows),
        "actionable_split_candidate_count": len(split_candidates),
        "manual_review_candidate_count": len(review_candidates),
        "crossing_review_candidate_count": len(crossing_candidates),
        "figure_count": len(figures),
        "actionable_split_candidate_ids": [int(r["id"]) for r in split_candidates],
        "manual_review_candidate_ids": [int(r["id"]) for r in review_candidates],
        "crossing_review_candidate_ids": [int(r["id"]) for r in crossing_candidates],
    }
    with open(out_dir / "局部防黏连候选摘要.json", "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)
    lines = [
        "# 局部防黏连候选诊断报告",
        "",
        "## 1. 诊断结论",
        "",
        f"- 输入数据：`{args.dat}`",
        f"- 结果文件：`{args.csv}`",
        f"- 疑似误合并表：`{args.suspect_csv}`" if args.suspect_csv else "- 疑似误合并表：未提供，按面积前 80 个多边形诊断",
        f"- 建议进入局部拆分候选：{len(split_candidates)} 个",
        f"- 建议人工复核后决定是否拆分：{len(review_candidates)} 个",
        f"- 交切复合区保留复核：{len(crossing_candidates)} 个",
        f"- 已生成局部图：{len(figures)} 张",
        "",
        "## 2. 判定逻辑",
        "",
        "本工具不直接修改正式结果。它先在疑似误合并多边形内部寻找低属性背景桥：如果切掉低属性像元后能形成两个以上有效主体，并且低属性桥接像元同时邻近多个主体，则认为该多边形存在局部拆分价值。对于强响应分支数量较多、弱响应段也较多的区域，优先标记为交切复合区保留复核，避免把真实断层交切关系错误拆散。该判断用于指导下一步算法拆分，不等同于直接判定结果错误。",
        "",
        "## 3. 优先复核对象",
        "",
        "| 多边形 | 面积 | 低属性比例 | 强响应段 | 主体数 | 桥接像元 | 建议 |",
        "|---:|---:|---:|---:|---:|---:|---|",
    ]
    for row in rows[:30]:
        lines.append(
            f"| {row['id']} | {row['area']:.1f} | {row['low_value_ratio']:.3f} | {row['strong_component_count']} | {row['major_part_count']} | {row['bridge_cells']} | {row['recommendation']} |"
        )
    lines.extend(
        [
            "",
            "## 4. 输出文件",
            "",
            f"- 候选表：`{out_dir / '局部防黏连候选.csv'}`",
            f"- 候选摘要：`{out_dir / '局部防黏连候选摘要.json'}`",
            f"- 局部图目录：`{figure_dir}`",
        ]
    )
    (out_dir / "局部防黏连候选诊断报告.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(out_dir / "局部防黏连候选诊断报告.md")
    print(out_dir / "局部防黏连候选.csv")


if __name__ == "__main__":
    main()
