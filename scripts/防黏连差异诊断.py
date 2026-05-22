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
from matplotlib.patches import Polygon as PolygonPatch
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
    return values, xs, ys, dx, dy


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


def make_polygon_items(polygons, values, xs, ys, dx, dy):
    height, width = values.shape
    items = []
    for poly in polygons:
        mask = rasterize_polygon(poly["coords"], width, height, xs, ys, dx, dy, poly.get("holes", []))
        ys_idx, xs_idx = np.where(mask)
        inside = values[mask]
        if inside.size:
            value_mean = float(np.mean(inside))
            value_max = float(np.max(inside))
            high_ratio = float(np.count_nonzero(inside >= 0.66) / inside.size)
        else:
            value_mean = 0.0
            value_max = 0.0
            high_ratio = 0.0
        item = dict(poly)
        item.update(
            {
                "mask": mask,
                "raster_cells": int(np.count_nonzero(mask)),
                "value_mean_raster": value_mean,
                "value_max_raster": value_max,
                "high_value_ratio": high_ratio,
                "min_x_index": int(xs_idx.min()) if xs_idx.size else 0,
                "max_x_index": int(xs_idx.max()) if xs_idx.size else 0,
                "min_y_index": int(ys_idx.min()) if ys_idx.size else 0,
                "max_y_index": int(ys_idx.max()) if ys_idx.size else 0,
            }
        )
        items.append(item)
    return items


def ratio(numerator, denominator):
    return float(numerator) / float(denominator) if denominator else 0.0


def summarize_mapping(base_items, candidate_items, values):
    rows = []
    candidate_source_rows = []
    candidate_used = set()
    for base in base_items:
        base_mask = base["mask"]
        base_cells = int(np.count_nonzero(base_mask))
        overlaps = []
        for cand in candidate_items:
            inter = int(np.count_nonzero(base_mask & cand["mask"]))
            if inter <= 0:
                continue
            overlaps.append((inter, cand))
        overlaps.sort(key=lambda item: item[0], reverse=True)

        cand_count = len(overlaps)
        covered_by_candidate = np.zeros_like(base_mask, dtype=bool)
        for _, cand in overlaps:
            covered_by_candidate |= cand["mask"]
            candidate_used.add(cand["id"])
            candidate_source_rows.append(
                {
                    "candidate_id": cand["id"],
                    "source_base_id": base["id"],
                    "intersection_cells": overlaps[0][0] if overlaps and cand["id"] == overlaps[0][1]["id"] else int(np.count_nonzero(base_mask & cand["mask"])),
                    "source_overlap_ratio": ratio(int(np.count_nonzero(base_mask & cand["mask"])), int(np.count_nonzero(cand["mask"]))),
                    "base_cover_ratio": ratio(int(np.count_nonzero(base_mask & cand["mask"])), base_cells),
                    "candidate_area": cand["area"],
                    "candidate_raster_cells": cand["raster_cells"],
                    "candidate_mean": cand["value_mean_raster"],
                    "candidate_high_ratio": cand["high_value_ratio"],
                }
            )

        lost_mask = base_mask & ~covered_by_candidate
        lost_values = values[lost_mask]
        row = {
            "base_id": base["id"],
            "base_area": base["area"],
            "base_raster_cells": base_cells,
            "candidate_parts": cand_count,
            "top_candidate_ids": ";".join(str(cand["id"]) for _, cand in overlaps[:8]),
            "covered_ratio": ratio(int(np.count_nonzero(base_mask & covered_by_candidate)), base_cells),
            "lost_cells": int(np.count_nonzero(lost_mask)),
            "lost_mean": float(np.mean(lost_values)) if lost_values.size else 0.0,
            "lost_max": float(np.max(lost_values)) if lost_values.size else 0.0,
            "lost_strong_cells": int(np.count_nonzero(lost_values >= 0.95)) if lost_values.size else 0,
            "lost_weak_cells": int(np.count_nonzero(lost_values >= 0.66)) if lost_values.size else 0,
            "base_mean": base["value_mean_raster"],
            "base_high_ratio": base["high_value_ratio"],
        }
        row["risk_hint"] = classify_split(row)
        rows.append(row)

    unmatched_candidates = [
        {
            "candidate_id": cand["id"],
            "candidate_area": cand["area"],
            "candidate_raster_cells": cand["raster_cells"],
            "candidate_mean": cand["value_mean_raster"],
            "candidate_high_ratio": cand["high_value_ratio"],
        }
        for cand in candidate_items
        if cand["id"] not in candidate_used
    ]
    rows.sort(key=lambda row: (row["candidate_parts"], row["lost_weak_cells"], row["lost_cells"]), reverse=True)
    return rows, candidate_source_rows, unmatched_candidates


def classify_split(row):
    if row["candidate_parts"] <= 1 and row["lost_cells"] <= 0:
        return "基本不变"
    if row["lost_strong_cells"] > 0:
        return "高风险：切掉强响应像元"
    if row["lost_weak_cells"] > 0:
        return "中风险：切掉弱响应像元"
    if row["candidate_parts"] >= 4:
        return "需复核：一个基线多边形被拆成多段"
    if row["candidate_parts"] >= 2:
        return "可能合理：轻度拆分且未损失高值响应"
    if row["lost_cells"] > 0 and row["lost_mean"] <= 0.05:
        return "可能合理：只剔除低值背景桥"
    return "需复核"


def write_csv(path, rows, fields):
    with open(path, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def world_extent_from_indices(xs, ys, min_x, max_x, min_y, max_y, pad):
    min_x = max(0, min_x - pad)
    max_x = min(len(xs) - 1, max_x + pad)
    min_y = max(0, min_y - pad)
    max_y = min(len(ys) - 1, max_y + pad)
    dx = abs(xs[1] - xs[0]) if len(xs) > 1 else 1.0
    dy = abs(ys[1] - ys[0]) if len(ys) > 1 else 1.0
    return (
        float(xs[min_x] - 0.5 * dx),
        float(xs[max_x] + 0.5 * dx),
        float(ys[min_y] - 0.5 * dy),
        float(ys[max_y] + 0.5 * dy),
    )


def draw_official_style(ax, values, extent_edges):
    ax.set_facecolor("white")
    weak = np.ma.masked_where(values <= 0, values)
    ax.imshow(
        weak,
        extent=extent_edges,
        origin="lower",
        cmap="gray_r",
        interpolation="nearest",
        vmin=0.30,
        vmax=1.0,
        alpha=0.52,
        aspect="equal",
    )
    strong = np.ma.masked_where(values < 0.66, values)
    ax.imshow(
        strong,
        extent=extent_edges,
        origin="lower",
        cmap="cool",
        interpolation="nearest",
        vmin=0.66,
        vmax=1.0,
        alpha=0.80,
        aspect="equal",
    )


def draw_polygon(ax, coords, color, width, alpha=1.0, fill=False):
    arr = np.asarray(coords, dtype=float)
    if fill:
        ax.add_patch(PolygonPatch(arr, closed=True, facecolor=color, edgecolor=color, linewidth=width, alpha=alpha))
    else:
        ax.plot(arr[:, 0], arr[:, 1], color=color, linewidth=width, alpha=alpha)


def render_split_figures(values, xs, ys, base_items, candidate_items, rows, out_dir, max_figures):
    figure_dir = out_dir / "局部复核图"
    figure_dir.mkdir(parents=True, exist_ok=True)
    base_by_id = {item["id"]: item for item in base_items}
    candidate_by_id = {item["id"]: item for item in candidate_items}
    interesting = [
        row
        for row in rows
        if row["candidate_parts"] >= 2 or row["lost_weak_cells"] > 0 or row["lost_cells"] >= 20
    ][:max_figures]

    for row in interesting:
        base = base_by_id[row["base_id"]]
        candidate_ids = [int(text) for text in str(row["top_candidate_ids"]).split(";") if text.strip()]
        candidates = [candidate_by_id[cid] for cid in candidate_ids if cid in candidate_by_id]
        if not candidates:
            continue

        min_x = min([base["min_x_index"]] + [cand["min_x_index"] for cand in candidates])
        max_x = max([base["max_x_index"]] + [cand["max_x_index"] for cand in candidates])
        min_y = min([base["min_y_index"]] + [cand["min_y_index"] for cand in candidates])
        max_y = max([base["max_y_index"]] + [cand["max_y_index"] for cand in candidates])
        extent = world_extent_from_indices(xs, ys, min_x, max_x, min_y, max_y, 18)

        fig, axes = plt.subplots(1, 3, figsize=(15, 5), dpi=220)
        for ax in axes:
            draw_official_style(ax, values, (float(xs.min()), float(xs.max()), float(ys.min()), float(ys.max())))
            ax.set_xlim(extent[0], extent[1])
            ax.set_ylim(extent[2], extent[3])
            ax.tick_params(labelsize=7, colors="#606870")
            for spine in ax.spines.values():
                spine.set_linewidth(0.7)
                spine.set_color("#6a6a6a")

        draw_polygon(axes[0], base["coords"], "#d00000", 0.9)
        axes[0].set_title(f"基线多边形 {base['id']}")

        for cand in candidates:
            draw_polygon(axes[1], cand["coords"], "#0072b2", 0.9)
        axes[1].set_title(f"候选拆分：{len(candidates)} 个主要片段")

        draw_polygon(axes[2], base["coords"], "#d00000", 0.85, alpha=0.85)
        for cand in candidates:
            draw_polygon(axes[2], cand["coords"], "#0072b2", 0.85, alpha=0.85)
        axes[2].set_title(row["risk_hint"])

        fig.suptitle(
            f"基线 {base['id']} -> 候选 {row['top_candidate_ids']}；"
            f"覆盖 {row['covered_ratio']:.2%}；丢失弱/强 {row['lost_weak_cells']}/{row['lost_strong_cells']}",
            fontsize=10,
        )
        fig.tight_layout()
        fig.savefig(figure_dir / f"基线{base['id']}_拆分复核.png", bbox_inches="tight")
        plt.close(fig)


def main():
    configure_chinese_font()
    parser = argparse.ArgumentParser(description="对比基线结果与防黏连候选结果，定位切分收益和副作用")
    parser.add_argument("--dat", required=True, help="GeoEast DAT 输入")
    parser.add_argument("--baseline-csv", required=True, help="稳定基线 CSV")
    parser.add_argument("--candidate-csv", required=True, help="候选 CSV")
    parser.add_argument("--out-dir", required=True, help="输出目录")
    parser.add_argument("--max-figures", type=int, default=30)
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    values, xs, ys, dx, dy = read_geoeast_dat(args.dat)
    base_polygons = read_polygons(args.baseline_csv)
    candidate_polygons = read_polygons(args.candidate_csv)
    base_items = make_polygon_items(base_polygons, values, xs, ys, dx, dy)
    candidate_items = make_polygon_items(candidate_polygons, values, xs, ys, dx, dy)
    split_rows, candidate_source_rows, unmatched_candidates = summarize_mapping(base_items, candidate_items, values)

    write_csv(
        out_dir / "基线到候选拆分映射.csv",
        split_rows,
        [
            "base_id",
            "base_area",
            "base_raster_cells",
            "candidate_parts",
            "top_candidate_ids",
            "covered_ratio",
            "lost_cells",
            "lost_mean",
            "lost_max",
            "lost_strong_cells",
            "lost_weak_cells",
            "base_mean",
            "base_high_ratio",
            "risk_hint",
        ],
    )
    write_csv(
        out_dir / "候选多边形来源映射.csv",
        sorted(candidate_source_rows, key=lambda row: (int(row["candidate_id"]), -float(row["source_overlap_ratio"]))),
        [
            "candidate_id",
            "source_base_id",
            "intersection_cells",
            "source_overlap_ratio",
            "base_cover_ratio",
            "candidate_area",
            "candidate_raster_cells",
            "candidate_mean",
            "candidate_high_ratio",
        ],
    )
    write_csv(
        out_dir / "无基线来源候选多边形.csv",
        unmatched_candidates,
        ["candidate_id", "candidate_area", "candidate_raster_cells", "candidate_mean", "candidate_high_ratio"],
    )
    render_split_figures(values, xs, ys, base_items, candidate_items, split_rows, out_dir, args.max_figures)

    changed = [row for row in split_rows if row["candidate_parts"] >= 2 or row["lost_cells"] > 0]
    high_value_loss = [row for row in split_rows if row["lost_strong_cells"] > 0]
    weak_value_loss = [row for row in split_rows if row["lost_weak_cells"] > 0]
    multi_part = [row for row in split_rows if row["candidate_parts"] >= 2]
    safe_splits = [
        row for row in multi_part
        if row["lost_strong_cells"] == 0 and row["lost_weak_cells"] == 0
    ]
    background_bridge_splits = [
        row for row in safe_splits
        if row["lost_cells"] > 0 and row["lost_mean"] <= 0.05
    ]
    lossless_splits = [row for row in safe_splits if row["lost_cells"] == 0]
    weak_loss_splits = [
        row for row in multi_part
        if row["lost_strong_cells"] == 0 and row["lost_weak_cells"] > 0
    ]
    lost_cells_total = int(sum(int(row["lost_cells"]) for row in changed))
    lost_weak_cells_total = int(sum(int(row["lost_weak_cells"]) for row in changed))
    lost_strong_cells_total = int(sum(int(row["lost_strong_cells"]) for row in changed))
    changed_covered_ratios = [float(row["covered_ratio"]) for row in changed]
    summary = {
        "baseline_csv": str(Path(args.baseline_csv)),
        "candidate_csv": str(Path(args.candidate_csv)),
        "baseline_polygon_count": len(base_items),
        "candidate_polygon_count": len(candidate_items),
        "changed_base_polygon_count": len(changed),
        "multi_part_base_polygon_count": len(multi_part),
        "strong_value_loss_base_polygon_count": len(high_value_loss),
        "weak_value_loss_base_polygon_count": len(weak_value_loss),
        "safe_split_base_polygon_count": len(safe_splits),
        "background_bridge_split_count": len(background_bridge_splits),
        "lossless_split_count": len(lossless_splits),
        "weak_loss_split_count": len(weak_loss_splits),
        "lost_cells_total": lost_cells_total,
        "lost_weak_cells_total": lost_weak_cells_total,
        "lost_strong_cells_total": lost_strong_cells_total,
        "min_changed_covered_ratio": min(changed_covered_ratios) if changed_covered_ratios else 1.0,
        "mean_changed_covered_ratio": (
            sum(changed_covered_ratios) / len(changed_covered_ratios)
            if changed_covered_ratios else 1.0
        ),
        "unmatched_candidate_count": len(unmatched_candidates),
    }
    (out_dir / "防黏连差异摘要.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    report = [
        "# 防黏连差异诊断报告",
        "",
        "## 1. 输入",
        "",
        f"- 稳定基线：`{summary['baseline_csv']}`",
        f"- 候选结果：`{summary['candidate_csv']}`",
        f"- 基线多边形数量：{summary['baseline_polygon_count']}",
        f"- 候选多边形数量：{summary['candidate_polygon_count']}",
        "",
        "## 2. 核心差异",
        "",
        f"- 发生覆盖变化的基线多边形：{summary['changed_base_polygon_count']}",
        f"- 被候选结果拆成多段的基线多边形：{summary['multi_part_base_polygon_count']}",
        f"- 切掉强响应像元的基线多边形：{summary['strong_value_loss_base_polygon_count']}",
        f"- 切掉弱响应像元的基线多边形：{summary['weak_value_loss_base_polygon_count']}",
        f"- 安全拆分的基线多边形：{summary['safe_split_base_polygon_count']}",
        f"- 只切掉低值背景桥的拆分：{summary['background_bridge_split_count']}",
        f"- 完全不损失覆盖的拆分：{summary['lossless_split_count']}",
        f"- 丢失弱响应像元总数：{summary['lost_weak_cells_total']}",
        f"- 丢失强响应像元总数：{summary['lost_strong_cells_total']}",
        f"- 发生变化对象的最低覆盖保持率：{summary['min_changed_covered_ratio']:.2%}",
        f"- 没有明显基线来源的候选多边形：{summary['unmatched_candidate_count']}",
        "",
        "## 3. 判断原则",
        "",
        "如果只切掉低值背景桥，且没有损失强弱响应像元，通常说明防黏连方向合理。"
        "如果切掉强响应或把一个连续断层切成很多小片段，则说明该参数或规则过激，不能进入默认主线。",
        "",
        "## 4. 输出文件",
        "",
        f"- 基线到候选拆分映射：`{out_dir / '基线到候选拆分映射.csv'}`",
        f"- 候选多边形来源映射：`{out_dir / '候选多边形来源映射.csv'}`",
        f"- 无基线来源候选多边形：`{out_dir / '无基线来源候选多边形.csv'}`",
        f"- 局部复核图：`{out_dir / '局部复核图'}`",
        f"- 摘要 JSON：`{out_dir / '防黏连差异摘要.json'}`",
    ]
    (out_dir / "防黏连差异诊断报告.md").write_text("\n".join(report) + "\n", encoding="utf-8")
    print(out_dir / "防黏连差异诊断报告.md")
    print(out_dir / "防黏连差异摘要.json")


if __name__ == "__main__":
    main()
