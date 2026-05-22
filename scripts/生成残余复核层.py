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
            if len(coords) >= 3:
                polygons.append(
                    {
                        "id": int(float(row.get("id", len(polygons) + 1) or 0)),
                        "area": float(row.get("area", 0.0) or 0.0),
                        "coords": coords,
                        "holes": holes,
                    }
                )
    return polygons


def rasterize_all(polygons, width, height, xs, ys, dx, dy):
    total = np.zeros((height, width), dtype=bool)
    id_map = np.zeros((height, width), dtype=np.int32)
    for poly in polygons:
        mask = rasterize_polygon(poly["coords"], width, height, xs, ys, dx, dy, poly.get("holes", []))
        total |= mask
        id_map[mask] = int(poly["id"])
    return total, id_map


def connected_components(mask, values, xs, ys):
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
                    nx, ny = x + ox, y + oy
                    if 0 <= nx < width and 0 <= ny < height and mask[ny, nx] and not visited[ny, nx]:
                        visited[ny, nx] = True
                        queue.append((nx, ny))

            arr = np.asarray(cells, dtype=int)
            cell_values = values[arr[:, 1], arr[:, 0]]
            min_x = int(arr[:, 0].min())
            max_x = int(arr[:, 0].max())
            min_y = int(arr[:, 1].min())
            max_y = int(arr[:, 1].max())
            bbox_width = max_x - min_x + 1
            bbox_height = max_y - min_y + 1
            aspect = max(bbox_width, bbox_height) / max(1, min(bbox_width, bbox_height))
            comp_mask = np.zeros_like(mask, dtype=bool)
            comp_mask[arr[:, 1], arr[:, 0]] = True
            components.append(
                {
                    "cells": cells,
                    "mask": comp_mask,
                    "cell_count": int(len(cells)),
                    "max_value": float(np.max(cell_values)),
                    "mean_value": float(np.mean(cell_values)),
                    "min_x_index": min_x,
                    "max_x_index": max_x,
                    "min_y_index": min_y,
                    "max_y_index": max_y,
                    "xmin": float(xs[min_x]),
                    "xmax": float(xs[max_x]),
                    "ymin": float(ys[min_y]),
                    "ymax": float(ys[max_y]),
                    "bbox_width_cells": int(bbox_width),
                    "bbox_height_cells": int(bbox_height),
                    "aspect": float(aspect),
                }
            )
    components.sort(key=lambda item: (item["cell_count"], item["max_value"]), reverse=True)
    return components


def nearest_covered_distance_cells(cells, covered_mask, max_radius):
    height, width = covered_mask.shape
    best_sq = float("inf")
    for x, y in cells:
        y0 = max(0, y - max_radius)
        y1 = min(height - 1, y + max_radius)
        x0 = max(0, x - max_radius)
        x1 = min(width - 1, x + max_radius)
        local = covered_mask[y0 : y1 + 1, x0 : x1 + 1]
        if not np.any(local):
            continue
        ys_local, xs_local = np.nonzero(local)
        dxs = xs_local + x0 - x
        dys = ys_local + y0 - y
        best_sq = min(best_sq, float(np.min(dxs * dxs + dys * dys)))
    if not math.isfinite(best_sq):
        return max_radius + 1.0
    return math.sqrt(best_sq)


def ordered_line_points(cells, xs, ys):
    if not cells:
        return []
    arr = np.asarray(cells, dtype=float)
    if len(cells) == 1:
        x, y = cells[0]
        return [(float(xs[x]), float(ys[y]))]
    centered = arr - np.mean(arr, axis=0)
    cov = centered.T @ centered / max(1, len(cells) - 1)
    _, vecs = np.linalg.eigh(cov)
    direction = vecs[:, -1]
    projection = centered @ direction
    order = np.argsort(projection)
    ordered = arr[order].astype(int)
    points = [(float(xs[int(x)]), float(ys[int(y)])) for x, y in ordered]
    deduped = []
    for point in points:
        if not deduped or point != deduped[-1]:
            deduped.append(point)
    return deduped


def line_wkt(points):
    if not points:
        return "LINESTRING EMPTY"
    if len(points) == 1:
        points = [points[0], points[0]]
    return "LINESTRING(" + ",".join(f"{x:.6f} {y:.6f}" for x, y in points) + ")"


def bbox_polygon_wkt(item, dx, dy):
    xmin = item["xmin"] - 0.5 * dx
    xmax = item["xmax"] + 0.5 * dx
    ymin = item["ymin"] - 0.5 * dy
    ymax = item["ymax"] + 0.5 * dy
    points = [(xmin, ymin), (xmax, ymin), (xmax, ymax), (xmin, ymax), (xmin, ymin)]
    return "POLYGON((" + ",".join(f"{x:.6f} {y:.6f}" for x, y in points) + "))"


def classify_candidate(kind, item, covered_mask, polygon_id_map, values, xs, ys, strong_threshold, weak_threshold, local_pad):
    height, width = values.shape
    x0 = max(0, item["min_x_index"] - local_pad)
    x1 = min(width - 1, item["max_x_index"] + local_pad)
    y0 = max(0, item["min_y_index"] - local_pad)
    y1 = min(height - 1, item["max_y_index"] + local_pad)
    local_values = values[y0 : y1 + 1, x0 : x1 + 1]
    local_covered = covered_mask[y0 : y1 + 1, x0 : x1 + 1]
    local_ids = polygon_id_map[y0 : y1 + 1, x0 : x1 + 1]
    local_strong = local_values >= strong_threshold
    local_weak = local_values >= weak_threshold

    touches_left = item["min_x_index"] == 0
    touches_right = item["max_x_index"] == width - 1
    touches_bottom = item["min_y_index"] == 0
    touches_top = item["max_y_index"] == height - 1
    touches_edge = touches_left or touches_right or touches_bottom or touches_top
    edge_names = []
    if touches_left:
        edge_names.append("左边界")
    if touches_right:
        edge_names.append("右边界")
    if touches_bottom:
        edge_names.append("下边界")
    if touches_top:
        edge_names.append("上边界")

    nearby_ids = sorted({int(v) for v in local_ids.flatten() if int(v) > 0})
    local_covered_strong = int(np.count_nonzero(local_covered & local_strong))
    local_covered_weak = int(np.count_nonzero(local_covered & local_weak))
    distance = nearest_covered_distance_cells(item["cells"], covered_mask, max_radius=max(12, local_pad))

    if touches_edge:
        review_type = "boundary_truncation"
        root_cause = "数据边界截断"
        recommendation = "边界复核；默认不外推，除非用户有外部解释依据。"
        priority = 3 if kind == "强响应" else 2
        include_in_review = True
    elif kind == "强响应":
        review_type = "strong_gap"
        root_cause = "强响应未覆盖"
        recommendation = "优先复核；若确认属于断层，应进入局部微补或参数复核。"
        priority = 1
        include_in_review = True
    elif distance <= 1.5 and item["cell_count"] <= 20 and item["aspect"] >= 3.0:
        review_type = "endpoint_micro_gap"
        root_cause = "断层端点轻微未包络"
        recommendation = "适合受控端点微补；不允许跨背景连接近邻断层。"
        priority = 2
        include_in_review = True
    elif distance <= 3.0 and local_covered_strong > 0:
        review_type = "local_envelope_gap"
        root_cause = "主断层附近的局部包络不足"
        recommendation = "适合局部补包络；补后必须重新检查拓扑和黏连风险。"
        priority = 2
        include_in_review = True
    elif kind == "弱响应" and item["aspect"] >= 3.0 and item["mean_value"] < strong_threshold:
        review_type = "weak_linear_residual"
        root_cause = "弱线性残余响应"
        recommendation = "进入残余复核层；不默认写入正式多边形层。"
        priority = 3
        include_in_review = True
    else:
        review_type = "noise_or_short_segment"
        root_cause = "孤立短段或噪声候选"
        recommendation = "仅保留诊断，不建议进入正式层。"
        priority = 4
        include_in_review = False

    line_points = ordered_line_points(item["cells"], xs, ys)
    return {
        "source_type": kind,
        "review_type": review_type,
        "root_cause": root_cause,
        "priority": priority,
        "include_in_review": include_in_review,
        "recommendation": recommendation,
        "cell_count": item["cell_count"],
        "max_value": item["max_value"],
        "mean_value": item["mean_value"],
        "aspect": item["aspect"],
        "bbox_width_cells": item["bbox_width_cells"],
        "bbox_height_cells": item["bbox_height_cells"],
        "xmin": item["xmin"],
        "xmax": item["xmax"],
        "ymin": item["ymin"],
        "ymax": item["ymax"],
        "touches_edge": touches_edge,
        "edge": "、".join(edge_names),
        "nearest_covered_distance_cells": distance,
        "nearby_polygon_ids": ",".join(str(v) for v in nearby_ids[:12]),
        "local_covered_strong_cells": local_covered_strong,
        "local_covered_weak_cells": local_covered_weak,
        "line_points": line_points,
        "mask": item["mask"],
    }


def draw_base(ax, values, extent_edges):
    ax.set_facecolor("white")
    weak = np.ma.masked_where(values <= 0, values)
    ax.imshow(weak, extent=extent_edges, origin="lower", cmap="gray_r", interpolation="nearest", vmin=0.30, vmax=1.0, alpha=0.45, aspect="equal")
    strong = np.ma.masked_where(values < 0.66, values)
    ax.imshow(strong, extent=extent_edges, origin="lower", cmap="cool", interpolation="nearest", vmin=0.66, vmax=1.0, alpha=0.70, aspect="equal")


def draw_polygons(ax, polygons, linewidth=0.32):
    for poly in polygons:
        arr = np.asarray(poly["coords"], dtype=float)
        ax.plot(arr[:, 0], arr[:, 1], color="#d00000", linewidth=linewidth, alpha=0.75)


def candidate_color(review_type):
    return {
        "strong_gap": "#ff8c00",
        "boundary_truncation": "#8a63d2",
        "endpoint_micro_gap": "#ffc400",
        "local_envelope_gap": "#00bf6f",
        "weak_linear_residual": "#00a6d6",
        "noise_or_short_segment": "#9aa0a6",
    }.get(review_type, "#00a6d6")


def draw_candidates(ax, candidates, linewidth=1.2):
    for item in candidates:
        points = item.get("line_points", [])
        if not points:
            continue
        arr = np.asarray(points, dtype=float)
        color = candidate_color(item["review_type"])
        ax.plot(arr[:, 0], arr[:, 1], color=color, linewidth=linewidth, alpha=0.98)
        ax.scatter(arr[0, 0], arr[0, 1], s=10, color=color, alpha=0.95)


def set_axes(ax, extent):
    ax.set_xlim(extent[0], extent[1])
    ax.set_ylim(extent[2], extent[3])
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.tick_params(labelsize=7, colors="#606870")


def local_extent(item, xs, ys, dx, dy, pad):
    x0 = max(0, int(item["xmin_index"]) - pad)
    x1 = min(len(xs) - 1, int(item["xmax_index"]) + pad)
    y0 = max(0, int(item["ymin_index"]) - pad)
    y1 = min(len(ys) - 1, int(item["ymax_index"]) + pad)
    return (
        float(xs[x0] - 0.5 * dx),
        float(xs[x1] + 0.5 * dx),
        float(ys[y0] - 0.5 * dy),
        float(ys[y1] + 0.5 * dy),
    )


def save_overview(values, extent_edges, polygons, candidates, out_path):
    fig, axes = plt.subplots(1, 2, figsize=(15.5, 6.0), dpi=220)
    for ax in axes:
        draw_base(ax, values, extent_edges)
        draw_polygons(ax, polygons)
        set_axes(ax, extent_edges)
    axes[0].set_title("正式结果层：红色闭合断层多边形")
    draw_candidates(axes[1], candidates, linewidth=1.05)
    axes[1].set_title("残余复核层：彩色线为未并入正式层的复核提示")
    fig.tight_layout()
    fig.savefig(out_path, bbox_inches="tight")
    plt.close(fig)


def save_local_figures(values, extent_edges, polygons, candidates, xs, ys, dx, dy, out_dir, limit):
    out_dir.mkdir(parents=True, exist_ok=True)
    generated = []
    for index, item in enumerate(candidates[:limit], start=1):
        points = item.get("line_points", [])
        if not points:
            continue
        item_for_extent = {
            "xmin_index": int(np.searchsorted(xs, item["xmin"], side="left")),
            "xmax_index": int(np.searchsorted(xs, item["xmax"], side="right")),
            "ymin_index": int(np.searchsorted(ys, item["ymin"], side="left")),
            "ymax_index": int(np.searchsorted(ys, item["ymax"], side="right")),
        }
        extent = local_extent(item_for_extent, xs, ys, dx, dy, 24)
        fig, axes = plt.subplots(1, 2, figsize=(10.5, 4.6), dpi=220)
        for ax in axes:
            draw_base(ax, values, extent_edges)
            draw_polygons(ax, polygons, linewidth=0.38)
            set_axes(ax, extent)
        axes[0].set_title("原始属性 + 正式多边形")
        draw_candidates(axes[1], [item], linewidth=2.1)
        axes[1].set_title(f"{item['root_cause']}：{item['cell_count']} 像元")
        fig.suptitle(f"复核候选 {index:02d}  priority={item['priority']}  max={item['max_value']:.3f}  mean={item['mean_value']:.3f}", fontsize=10)
        fig.tight_layout()
        path = out_dir / f"候选_{index:02d}_{item['review_type']}_cells{item['cell_count']}.png"
        fig.savefig(path, bbox_inches="tight")
        plt.close(fig)
        generated.append(path)
    return generated


def write_review_csv(path, candidates, dx, dy):
    fields = [
        "id",
        "status",
        "source_type",
        "review_type",
        "root_cause",
        "priority",
        "cell_count",
        "max_value",
        "mean_value",
        "aspect",
        "bbox_width_cells",
        "bbox_height_cells",
        "xmin",
        "xmax",
        "ymin",
        "ymax",
        "touches_edge",
        "edge",
        "nearest_covered_distance_cells",
        "nearby_polygon_ids",
        "local_covered_strong_cells",
        "local_covered_weak_cells",
        "recommendation",
        "line_wkt",
        "envelope_wkt",
    ]
    with open(path, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        for index, item in enumerate(candidates, start=1):
            row = dict(item)
            row["id"] = index
            row["status"] = "review"
            row["touches_edge"] = "yes" if item["touches_edge"] else "no"
            row["line_wkt"] = line_wkt(item["line_points"])
            row["envelope_wkt"] = bbox_polygon_wkt(item, dx, dy)
            writer.writerow(row)


def write_geojson(path, candidates):
    features = []
    for index, item in enumerate(candidates, start=1):
        points = item.get("line_points", [])
        if not points:
            continue
        if len(points) == 1:
            points = [points[0], points[0]]
        properties = {
            "id": index,
            "status": "review",
            "source_type": item["source_type"],
            "review_type": item["review_type"],
            "root_cause": item["root_cause"],
            "priority": item["priority"],
            "cell_count": item["cell_count"],
            "max_value": item["max_value"],
            "mean_value": item["mean_value"],
            "aspect": item["aspect"],
            "recommendation": item["recommendation"],
        }
        features.append(
            {
                "type": "Feature",
                "properties": properties,
                "geometry": {"type": "LineString", "coordinates": [[x, y] for x, y in points]},
            }
        )
    with open(path, "w", encoding="utf-8") as f:
        json.dump({"type": "FeatureCollection", "features": features}, f, ensure_ascii=False, indent=2)


def write_report(path, args, candidates, generated_figures, summary):
    counts = {}
    for item in candidates:
        counts[item["root_cause"]] = counts.get(item["root_cause"], 0) + 1

    lines = [
        "# 残余复核层报告",
        "",
        "## 1. 结论",
        "",
        f"- 输入 DAT：`{args.dat}`",
        f"- 正式结果 CSV：`{args.csv}`",
        f"- 复核候选数量：{len(candidates)}",
        f"- 强响应覆盖率：{summary['strong_coverage']:.4%}",
        f"- 弱响应覆盖率：{summary['weak_coverage']:.4%}",
        "",
        "本报告生成的是复核提示层，不会改变正式断层多边形结果。复核层用于提示解释人员检查边界截断、弱线性残余、端点轻微未包络和局部包络不足等位置，避免把弱碎片直接并入正式成果导致串珠化或误合并。",
        "",
        "## 2. 根因统计",
        "",
        "| 根因 | 数量 | 处理建议 |",
        "|---|---:|---|",
    ]
    advice = {
        "数据边界截断": "默认不外推，必要时结合外部解释依据。",
        "强响应未覆盖": "优先复核，必要时做局部微补。",
        "断层端点轻微未包络": "可做受控端点微补。",
        "主断层附近的局部包络不足": "可做局部补包络，并重新检查拓扑。",
        "弱线性残余响应": "作为复核层保留，不默认写入正式层。",
        "孤立短段或噪声候选": "继续过滤或仅保留诊断。",
    }
    for cause, count in sorted(counts.items(), key=lambda kv: (-kv[1], kv[0])):
        lines.append(f"| {cause} | {count} | {advice.get(cause, '复核后决定')} |")

    lines.extend(
        [
            "",
            "## 3. 输出文件",
            "",
            f"- 复核 CSV：`{Path(args.out_dir) / '残余复核层.csv'}`",
            f"- GeoJSON：`{Path(args.out_dir) / '残余复核层.geojson'}`",
            f"- 总览图：`{Path(args.out_dir) / '残余复核层总览.png'}`",
            f"- 局部图目录：`{Path(args.out_dir) / '局部复核图'}`",
            "",
            "## 4. 使用原则",
            "",
            "1. 正式断层多边形仍以 `result.csv` 为准，满足闭合、面积统计和 GeoEast 写入要求。",
            "2. `残余复核层.csv` 是质控提示，不应无条件写入正式多边形对象。",
            "3. 若某个复核候选被确认是真实断层，应优先通过局部微补或参数推荐重新运行，而不是手工扩大全部多边形。",
            "4. 若复核候选属于噪声或边界外推风险，应保留在报告中说明，不进入正式结果。",
            "",
            "## 5. 局部图清单",
            "",
        ]
    )
    for figure in generated_figures:
        lines.append(f"- `{figure}`")
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    configure_chinese_font()
    parser = argparse.ArgumentParser(description="生成正式结果之外的残余弱响应复核提示层")
    parser.add_argument("--dat", required=True, help="GeoEast DAT 网格文件")
    parser.add_argument("--csv", required=True, help="FaultPolyTrace 正式结果 CSV")
    parser.add_argument("--out-dir", required=True, help="输出目录")
    parser.add_argument("--strong-threshold", type=float, default=0.95)
    parser.add_argument("--weak-threshold", type=float, default=0.66)
    parser.add_argument("--min-review-cells", type=int, default=8)
    parser.add_argument("--min-line-aspect", type=float, default=3.0)
    parser.add_argument("--local-pad", type=int, default=12)
    parser.add_argument("--max-local-figures", type=int, default=12)
    parser.add_argument("--include-noise-candidates", action="store_true", help="同时输出孤立短段/噪声候选")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    values, xs, ys, dx, dy, extent_edges = read_geoeast_dat(args.dat)
    polygons = read_polygons(args.csv)
    if not polygons:
        raise RuntimeError(f"未读取到保留多边形：{args.csv}")

    height, width = values.shape
    covered_mask, polygon_id_map = rasterize_all(polygons, width, height, xs, ys, dx, dy)
    strong_mask = values >= args.strong_threshold
    weak_mask = values >= args.weak_threshold
    missed_strong = strong_mask & ~covered_mask
    missed_weak = weak_mask & ~covered_mask

    raw_components = []
    for item in connected_components(missed_strong, values, xs, ys):
        if item["cell_count"] >= args.min_review_cells:
            raw_components.append(("强响应", item))
    for item in connected_components(missed_weak, values, xs, ys):
        if item["cell_count"] >= args.min_review_cells and item["aspect"] >= args.min_line_aspect:
            raw_components.append(("弱响应", item))

    candidates = []
    for kind, item in raw_components:
        candidate = classify_candidate(kind, item, covered_mask, polygon_id_map, values, xs, ys, args.strong_threshold, args.weak_threshold, args.local_pad)
        if candidate["include_in_review"] or args.include_noise_candidates:
            candidates.append(candidate)

    candidates.sort(key=lambda item: (item["priority"], -item["max_value"], -item["cell_count"]))

    summary = {
        "strong_total_cells": int(np.count_nonzero(strong_mask)),
        "weak_total_cells": int(np.count_nonzero(weak_mask)),
        "strong_coverage": float(np.count_nonzero(covered_mask & strong_mask) / max(1, np.count_nonzero(strong_mask))),
        "weak_coverage": float(np.count_nonzero(covered_mask & weak_mask) / max(1, np.count_nonzero(weak_mask))),
        "candidate_count": len(candidates),
    }

    write_review_csv(out_dir / "残余复核层.csv", candidates, dx, dy)
    write_geojson(out_dir / "残余复核层.geojson", candidates)
    save_overview(values, extent_edges, polygons, candidates, out_dir / "残余复核层总览.png")
    generated_figures = save_local_figures(values, extent_edges, polygons, candidates, xs, ys, dx, dy, out_dir / "局部复核图", args.max_local_figures)
    with open(out_dir / "残余复核层摘要.json", "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)
    write_report(out_dir / "残余复核层报告.md", args, candidates, generated_figures, summary)

    print(out_dir / "残余复核层.csv")
    print(out_dir / "残余复核层.geojson")
    print(out_dir / "残余复核层报告.md")


if __name__ == "__main__":
    main()
