import argparse
import csv
import json
import math
import sys
from collections import deque
from pathlib import Path

import numpy as np

from wkt_geometry import parse_wkt_polygon_with_holes, rasterize_polygon

csv.field_size_limit(min(sys.maxsize, 2_147_483_647))


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


def bounded_window(item, width, height, pad):
    x0 = max(0, int(item["min_x_index"]) - pad)
    x1 = min(width - 1, int(item["max_x_index"]) + pad)
    y0 = max(0, int(item["min_y_index"]) - pad)
    y1 = min(height - 1, int(item["max_y_index"]) + pad)
    return x0, x1, y0, y1


def nearest_covered_distance_cells(cells, covered_mask, max_radius):
    height, width = covered_mask.shape
    covered_points = np.argwhere(covered_mask)
    if covered_points.size == 0:
        return max_radius + 1.0
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
        local_best = float(np.min(dxs * dxs + dys * dys))
        if local_best < best_sq:
            best_sq = local_best
    if not math.isfinite(best_sq):
        return max_radius + 1.0
    return math.sqrt(best_sq)


def component_orientation_deg(cells):
    if len(cells) < 2:
        return 0.0
    arr = np.asarray(cells, dtype=float)
    centered = arr - np.mean(arr, axis=0)
    cov = centered.T @ centered / max(1, len(cells) - 1)
    vals, vecs = np.linalg.eigh(cov)
    principal = vecs[:, int(np.argmax(vals))]
    angle = math.degrees(math.atan2(principal[1], principal[0]))
    if angle < -90:
        angle += 180
    if angle > 90:
        angle -= 180
    return float(angle)


def count_neighbor_covered(cells, covered_mask):
    height, width = covered_mask.shape
    count = 0
    offsets = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]
    for x, y in cells:
        for ox, oy in offsets:
            nx, ny = x + ox, y + oy
            if 0 <= nx < width and 0 <= ny < height and covered_mask[ny, nx]:
                count += 1
    return count


def classify_component(kind, item, values, covered_mask, polygon_id_map, strong_threshold, weak_threshold, pad):
    height, width = values.shape
    x0, x1, y0, y1 = bounded_window(item, width, height, pad)
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

    distance = nearest_covered_distance_cells(item["cells"], covered_mask, max_radius=max(12, pad))
    neighbor_covered = count_neighbor_covered(item["cells"], covered_mask)
    nearby_ids = sorted({int(v) for v in local_ids.flatten() if int(v) > 0})
    local_covered_strong = int(np.count_nonzero(local_covered & local_strong))
    local_covered_weak = int(np.count_nonzero(local_covered & local_weak))
    orientation = component_orientation_deg(item["cells"])

    if touches_edge:
        root_cause = "数据边界截断"
        action = "建议保留为边界复核项；默认不外推，除非用户启用边界外推或有外部解释依据。"
        risk = "低"
    elif distance <= 1.5 and item["cell_count"] <= 20 and len(nearby_ids) >= 1:
        if item["aspect"] >= 3.0:
            root_cause = "断层端点轻微未包络"
            action = "可用局部端点微补追踪处理；只允许贴近已有多边形补齐，不允许跨背景连接。"
            risk = "中低"
        else:
            root_cause = "交切或分支端部未包络"
            action = "应进入分支端部复核；若局部图确认是有效分支，可做小范围包络补偿。"
            risk = "中"
    elif distance <= 3.0 and local_covered_strong > 0:
        root_cause = "主断层附近的局部包络不足"
        action = "适合做受控局部补包络；必须保持拓扑 0 问题并复核相邻断层不被合并。"
        risk = "中"
    elif kind == "弱响应" and item["aspect"] >= 3.0 and item["mean_value"] < strong_threshold:
        root_cause = "弱线性残余响应"
        action = "建议输出到残余复核层，不默认并入正式层，避免碎片暴涨。"
        risk = "中低"
    else:
        root_cause = "孤立短段或噪声候选"
        action = "建议继续过滤或仅作为复核提示；不建议降低全局阈值。"
        risk = "低"

    row = {
        "类型": kind,
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
        "touches_edge": "是" if touches_edge else "否",
        "edge": "、".join(edge_names) if edge_names else "",
        "nearest_covered_distance_cells": distance,
        "neighbor_covered_count": neighbor_covered,
        "nearby_polygon_ids": ",".join(str(v) for v in nearby_ids[:12]),
        "local_covered_strong_cells": local_covered_strong,
        "local_covered_weak_cells": local_covered_weak,
        "orientation_deg": orientation,
        "root_cause": root_cause,
        "action": action,
        "risk": risk,
    }
    return row


def write_csv(path, rows, fields):
    with open(path, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def ratio(numerator, denominator):
    return float(numerator) / float(denominator) if denominator else 0.0


def main():
    parser = argparse.ArgumentParser(description="分析未覆盖断层响应的根因，生成可回溯质控表和报告")
    parser.add_argument("--dat", required=True, help="GeoEast DAT 网格文件")
    parser.add_argument("--csv", required=True, help="FaultPolyTrace 输出 CSV")
    parser.add_argument("--out-dir", required=True, help="输出目录")
    parser.add_argument("--strong-threshold", type=float, default=0.95)
    parser.add_argument("--weak-threshold", type=float, default=0.66)
    parser.add_argument("--min-review-cells", type=int, default=8)
    parser.add_argument("--min-line-aspect", type=float, default=3.0)
    parser.add_argument("--local-pad", type=int, default=12)
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    values, xs, ys, dx, dy = read_geoeast_dat(args.dat)
    polygons = read_polygons(args.csv)
    if not polygons:
        raise RuntimeError(f"未读取到保留多边形：{args.csv}")

    height, width = values.shape
    covered_mask, polygon_id_map = rasterize_all(polygons, width, height, xs, ys, dx, dy)
    strong_mask = values >= args.strong_threshold
    weak_mask = values >= args.weak_threshold
    missed_strong = strong_mask & ~covered_mask
    missed_weak = weak_mask & ~covered_mask

    strong_components = [
        c
        for c in connected_components(missed_strong, values, xs, ys)
        if c["cell_count"] >= args.min_review_cells
    ]
    weak_components = [
        c
        for c in connected_components(missed_weak, values, xs, ys)
        if c["cell_count"] >= args.min_review_cells and c["aspect"] >= args.min_line_aspect
    ]

    rows = []
    for item in strong_components:
        rows.append(classify_component("强响应", item, values, covered_mask, polygon_id_map, args.strong_threshold, args.weak_threshold, args.local_pad))
    for item in weak_components:
        rows.append(classify_component("弱响应", item, values, covered_mask, polygon_id_map, args.strong_threshold, args.weak_threshold, args.local_pad))

    fields = [
        "类型",
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
        "neighbor_covered_count",
        "nearby_polygon_ids",
        "local_covered_strong_cells",
        "local_covered_weak_cells",
        "orientation_deg",
        "root_cause",
        "risk",
        "action",
    ]
    write_csv(out_dir / "漏检根因诊断明细.csv", rows, fields)

    cause_counts = {}
    for row in rows:
        cause_counts[row["root_cause"]] = cause_counts.get(row["root_cause"], 0) + 1

    overall = {
        "input_dat": str(Path(args.dat)),
        "input_csv": str(Path(args.csv)),
        "grid": {"nx": width, "ny": height},
        "polygon_count": len(polygons),
        "strong_threshold": args.strong_threshold,
        "weak_threshold": args.weak_threshold,
        "strong_total_cells": int(np.count_nonzero(strong_mask)),
        "weak_total_cells": int(np.count_nonzero(weak_mask)),
        "strong_missed_cells": int(np.count_nonzero(missed_strong)),
        "weak_missed_cells": int(np.count_nonzero(missed_weak)),
        "strong_coverage": ratio(np.count_nonzero(covered_mask & strong_mask), np.count_nonzero(strong_mask)),
        "weak_coverage": ratio(np.count_nonzero(covered_mask & weak_mask), np.count_nonzero(weak_mask)),
        "strong_review_components": len(strong_components),
        "weak_line_review_components": len(weak_components),
        "root_cause_counts": cause_counts,
    }
    with open(out_dir / "漏检根因总体指标.json", "w", encoding="utf-8") as f:
        json.dump(overall, f, ensure_ascii=False, indent=2)

    lines = [
        "# 漏检根因诊断报告",
        "",
        "## 1. 诊断结论",
        "",
        f"- 输入数据：`{overall['input_dat']}`",
        f"- 结果文件：`{overall['input_csv']}`",
        f"- 输出多边形：{overall['polygon_count']} 个",
        f"- 强响应覆盖率：{overall['strong_coverage']:.4%}",
        f"- 弱响应覆盖率：{overall['weak_coverage']:.4%}",
        f"- 强响应需复核连通域：{overall['strong_review_components']} 个",
        f"- 弱线性需复核连通域：{overall['weak_line_review_components']} 个",
        "",
        "## 2. 根因分类统计",
        "",
        "| 根因 | 数量 |",
        "|---|---:|",
    ]
    for cause, count in sorted(cause_counts.items(), key=lambda kv: (-kv[1], kv[0])):
        lines.append(f"| {cause} | {count} |")
    if not cause_counts:
        lines.append("| 无需复核项 | 0 |")

    lines.extend(
        [
            "",
            "## 3. 复核明细",
            "",
            "| 类型 | 像元 | 最大值 | 形态比 | 边界 | 最近结果距离/格 | 根因 | 建议 |",
            "|---|---:|---:|---:|---|---:|---|---|",
        ]
    )
    for row in rows:
        edge = row["edge"] if row["edge"] else "否"
        lines.append(
            "| {kind} | {cells} | {maxv:.3f} | {aspect:.2f} | {edge} | {dist:.2f} | {cause} | {action} |".format(
                kind=row["类型"],
                cells=row["cell_count"],
                maxv=float(row["max_value"]),
                aspect=float(row["aspect"]),
                edge=edge,
                dist=float(row["nearest_covered_distance_cells"]),
                cause=row["root_cause"],
                action=row["action"],
            )
        )

    lines.extend(
        [
            "",
            "## 4. 对算法优化的约束",
            "",
            "1. 若复核项属于数据边界截断，默认主线不应外推，否则换数据后容易产生越界假断层。",
            "2. 若复核项属于端点或交切分支，应优先做局部微补，而不是降低全局阈值。",
            "3. 若复核项属于弱线性残余响应，推荐进入残余复核层，避免正式层碎片暴涨。",
            "4. 任一补追踪策略必须保持开放环、自相交、异常相交为 0，并保持多边形数量在稳定量级。",
            "",
            "## 5. 输出文件",
            "",
            f"- 明细表：`{out_dir / '漏检根因诊断明细.csv'}`",
            f"- 总体指标：`{out_dir / '漏检根因总体指标.json'}`",
        ]
    )
    (out_dir / "漏检根因诊断报告.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(out_dir / "漏检根因诊断报告.md")
    print(out_dir / "漏检根因诊断明细.csv")


if __name__ == "__main__":
    main()
