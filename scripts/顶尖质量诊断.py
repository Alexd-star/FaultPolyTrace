# -*- coding: utf-8 -*-
import argparse
import csv
import json
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
                        "id": int(float(row.get("id", len(polygons) + 1))),
                        "area": float(row.get("area", 0.0) or 0.0),
                        "perimeter": float(row.get("perimeter", 0.0) or 0.0),
                        "cell_count": int(float(row.get("cell_count", 0) or 0)),
                        "mean_value_reported": float(row.get("mean_value", 0.0) or 0.0),
                        "max_value_reported": float(row.get("max_value", 0.0) or 0.0),
                        "coords": coords,
                        "holes": holes,
                    }
                )
    return polygons


def connected_components(mask, values=None, xs=None, ys=None):
    h, w = mask.shape
    visited = np.zeros_like(mask, dtype=bool)
    components = []
    offsets = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]
    for y0 in range(h):
        for x0 in range(w):
            if not mask[y0, x0] or visited[y0, x0]:
                continue
            q = deque([(x0, y0)])
            visited[y0, x0] = True
            cells = []
            while q:
                x, y = q.popleft()
                cells.append((x, y))
                for ox, oy in offsets:
                    nx, ny = x + ox, y + oy
                    if 0 <= nx < w and 0 <= ny < h and mask[ny, nx] and not visited[ny, nx]:
                        visited[ny, nx] = True
                        q.append((nx, ny))

            arr = np.asarray(cells, dtype=int)
            min_x = int(arr[:, 0].min())
            max_x = int(arr[:, 0].max())
            min_y = int(arr[:, 1].min())
            max_y = int(arr[:, 1].max())
            bbox_width = max_x - min_x + 1
            bbox_height = max_y - min_y + 1
            aspect = max(bbox_width, bbox_height) / max(1, min(bbox_width, bbox_height))
            item = {
                "cell_count": int(len(cells)),
                "min_x_index": min_x,
                "max_x_index": max_x,
                "min_y_index": min_y,
                "max_y_index": max_y,
                "bbox_width_cells": int(bbox_width),
                "bbox_height_cells": int(bbox_height),
                "aspect": float(aspect),
            }
            if values is not None:
                cell_values = values[arr[:, 1], arr[:, 0]]
                item["max_value"] = float(np.max(cell_values))
                item["mean_value"] = float(np.mean(cell_values))
            if xs is not None and ys is not None:
                item["xmin"] = float(xs[min_x])
                item["xmax"] = float(xs[max_x])
                item["ymin"] = float(ys[min_y])
                item["ymax"] = float(ys[max_y])
            components.append(item)
    components.sort(key=lambda item: (item["cell_count"], item.get("max_value", 0.0)), reverse=True)
    return components


def ratio(numerator, denominator):
    return float(numerator) / float(denominator) if denominator else 0.0


def distance_to_mask(mask):
    h, w = mask.shape
    distances = np.full(mask.shape, -1, dtype=np.int32)
    q = deque()
    ys, xs = np.nonzero(mask)
    for y, x in zip(ys, xs):
        distances[y, x] = 0
        q.append((x, y))

    offsets = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]
    while q:
        x, y = q.popleft()
        next_distance = int(distances[y, x]) + 1
        for ox, oy in offsets:
            nx, ny = x + ox, y + oy
            if 0 <= nx < w and 0 <= ny < h and distances[ny, nx] < 0:
                distances[ny, nx] = next_distance
                q.append((nx, ny))
    return distances


def summarize_polygon(poly, mask, values, distance_to_weak, strong_threshold, weak_threshold, low_threshold, median_area, background_fit_radius):
    inside = values[mask]
    covered_cells = int(np.count_nonzero(mask))
    if covered_cells == 0:
        return None

    strong_inside = mask & (values >= strong_threshold)
    weak_inside = mask & (values >= weak_threshold)
    low_inside = mask & (values <= low_threshold)
    far_low_inside = low_inside & (distance_to_weak > background_fit_radius)
    strong_components = [c for c in connected_components(strong_inside) if c["cell_count"] >= 3]
    weak_components = [c for c in connected_components(weak_inside) if c["cell_count"] >= 3]

    low_ratio = ratio(np.count_nonzero(low_inside), covered_cells)
    far_low_ratio = ratio(np.count_nonzero(far_low_inside), covered_cells)
    strong_ratio = ratio(np.count_nonzero(strong_inside), covered_cells)
    weak_ratio = ratio(np.count_nonzero(weak_inside), covered_cells)
    mean_value = float(np.mean(inside)) if inside.size else 0.0
    max_value = float(np.max(inside)) if inside.size else 0.0
    coord_arr = np.asarray(poly["coords"], dtype=float)
    bbox_width = float(coord_arr[:, 0].max() - coord_arr[:, 0].min()) if coord_arr.size else 0.0
    bbox_height = float(coord_arr[:, 1].max() - coord_arr[:, 1].min()) if coord_arr.size else 0.0
    bbox_aspect = max(bbox_width, bbox_height) / max(1e-9, min(bbox_width, bbox_height)) if bbox_width > 0.0 and bbox_height > 0.0 else 0.0

    supported_low_ratio = max(0.0, low_ratio - far_low_ratio)
    risk_score = 0
    reasons = []
    if len(strong_components) >= 4 and far_low_ratio >= 0.08 and bbox_aspect <= 4.0:
        risk_score += 3
        reasons.append("远离弱响应的低属性背景跨接多个强响应段，且整体形态不够细长")
    elif len(strong_components) >= 4 and low_ratio >= 0.55 and supported_low_ratio >= 0.50:
        reasons.append("低值区主要贴近弱响应带，按断层带包络处理，不直接判为误合并")
    if len(strong_components) >= 8 and bbox_aspect <= 6.0:
        if far_low_ratio >= 0.03:
            risk_score += 1
            reasons.append("强响应段数量较多，且存在少量远背景低值区，需局部复核")
        else:
            reasons.append("强响应段数量较多，但低值区受弱响应支撑，倾向于连续断层带或交切复合区")
    if len(weak_components) >= 4 and bbox_aspect <= 4.0 and far_low_ratio >= 0.03:
        risk_score += 1
        reasons.append("弱响应连通段较多且形态偏面状")
    if mean_value < weak_threshold * 0.32 and poly["area"] >= median_area * 2.0 and far_low_ratio >= 0.08:
        risk_score += 1
        reasons.append("大面积多边形平均属性偏低，且远背景包络偏多")
    if strong_ratio < 0.12 and poly["area"] >= median_area * 2.0 and far_low_ratio >= 0.08:
        risk_score += 1
        reasons.append("强响应面积占比偏低，且远背景包络偏多")
    if poly["area"] >= median_area * 5.0 and bbox_aspect <= 4.0 and far_low_ratio >= 0.08:
        risk_score += 1
        reasons.append("面积显著偏大且不够细长，同时远背景包络偏多")

    return {
        "id": poly["id"],
        "area": poly["area"],
        "perimeter": poly["perimeter"],
        "bbox_aspect": bbox_aspect,
        "reported_cell_count": poly["cell_count"],
        "raster_cell_count": covered_cells,
        "mean_value": mean_value,
        "max_value": max_value,
        "strong_cell_ratio": strong_ratio,
        "weak_cell_ratio": weak_ratio,
        "low_value_ratio": low_ratio,
        "far_low_background_ratio": far_low_ratio,
        "strong_component_count": len(strong_components),
        "weak_component_count": len(weak_components),
        "risk_score": risk_score,
        "risk_level": "高" if risk_score >= 5 else ("中" if risk_score >= 3 else "低"),
        "reason": "；".join(reasons) if reasons else "未触发明显误合并风险规则；多强响应段可能是长断层沿走向强弱变化",
    }


def write_csv(path, rows, fields):
    with open(path, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def main():
    parser = argparse.ArgumentParser(description="生成断层多边形顶尖质量诊断报告")
    parser.add_argument("--dat", required=True, help="官方或工区 GeoEast DAT 文件")
    parser.add_argument("--csv", required=True, help="FaultPolyTrace 输出 CSV")
    parser.add_argument("--out-dir", required=True, help="诊断输出目录")
    parser.add_argument("--strong-threshold", type=float, default=0.95)
    parser.add_argument("--weak-threshold", type=float, default=0.66)
    parser.add_argument("--low-threshold", type=float, default=0.05)
    parser.add_argument("--min-review-cells", type=int, default=8)
    parser.add_argument("--min-line-aspect", type=float, default=3.0)
    parser.add_argument("--background-fit-radius", type=int, default=2, help="多边形内距离弱响应超过该格数的低值背景计为远背景")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    values, xs, ys, dx, dy = read_geoeast_dat(args.dat)
    polygons = read_polygons(args.csv)
    if not polygons:
        raise RuntimeError(f"未读取到保留多边形：{args.csv}")

    height, width = values.shape
    total_mask = np.zeros_like(values, dtype=bool)
    polygon_rows = []
    areas = [p["area"] for p in polygons if p["area"] > 0.0]
    median_area = float(np.median(areas)) if areas else 0.0
    weak_mask = values >= args.weak_threshold
    distance_to_weak = distance_to_mask(weak_mask)

    for poly in polygons:
        mask = rasterize_polygon(poly["coords"], width, height, xs, ys, dx, dy, poly.get("holes", []))
        total_mask |= mask
        row = summarize_polygon(
            poly,
            mask,
            values,
            distance_to_weak,
            args.strong_threshold,
            args.weak_threshold,
            args.low_threshold,
            median_area,
            args.background_fit_radius,
        )
        if row:
            polygon_rows.append(row)

    strong_mask = values >= args.strong_threshold
    nonzero_mask = values > 0
    missed_strong = strong_mask & ~total_mask
    missed_weak = weak_mask & ~total_mask

    strong_components = [
        c for c in connected_components(missed_strong, values, xs, ys)
        if c["cell_count"] >= args.min_review_cells
    ]
    weak_components = [
        c for c in connected_components(missed_weak, values, xs, ys)
        if c["cell_count"] >= args.min_review_cells and c["aspect"] >= args.min_line_aspect
    ]

    high_risk = [r for r in polygon_rows if r["risk_level"] == "高"]
    middle_risk = [r for r in polygon_rows if r["risk_level"] == "中"]
    small_polygons = [r for r in polygon_rows if r["area"] < median_area * 0.25] if median_area else []
    large_background_polygons = [
        r for r in polygon_rows
        if r["far_low_background_ratio"] >= 0.60 and r["weak_cell_ratio"] < 0.30
    ]
    weighted_covered_cells = sum(r["raster_cell_count"] for r in polygon_rows)
    boundary_background_ratio = (
        sum(r["far_low_background_ratio"] * r["raster_cell_count"] for r in polygon_rows) / weighted_covered_cells
        if weighted_covered_cells else 0.0
    )

    overall = {
        "input_dat": str(Path(args.dat)),
        "input_csv": str(Path(args.csv)),
        "grid": {"nx": width, "ny": height},
        "polygon_count": len(polygons),
        "median_area": median_area,
        "strong_threshold": args.strong_threshold,
        "weak_threshold": args.weak_threshold,
        "low_threshold": args.low_threshold,
        "strong_total_cells": int(np.count_nonzero(strong_mask)),
        "weak_total_cells": int(np.count_nonzero(weak_mask)),
        "nonzero_total_cells": int(np.count_nonzero(nonzero_mask)),
        "covered_cells": int(np.count_nonzero(total_mask)),
        "strong_coverage": ratio(np.count_nonzero(total_mask & strong_mask), np.count_nonzero(strong_mask)),
        "weak_coverage": ratio(np.count_nonzero(total_mask & weak_mask), np.count_nonzero(weak_mask)),
        "nonzero_coverage": ratio(np.count_nonzero(total_mask & nonzero_mask), np.count_nonzero(nonzero_mask)),
        "strong_review_components": len(strong_components),
        "weak_line_review_components": len(weak_components),
        "high_risk_merge_polygons": len(high_risk),
        "middle_risk_merge_polygons": len(middle_risk),
        "small_polygon_count": len(small_polygons),
        "boundary_background_ratio": boundary_background_ratio,
        "large_background_polygon_count": len(large_background_polygons),
        "background_fit_radius": args.background_fit_radius,
    }

    write_csv(
        out_dir / "逐多边形质量诊断.csv",
        polygon_rows,
        [
            "id", "area", "perimeter", "reported_cell_count", "raster_cell_count",
            "bbox_aspect", "mean_value", "max_value", "strong_cell_ratio", "weak_cell_ratio", "low_value_ratio",
            "far_low_background_ratio", "strong_component_count", "weak_component_count", "risk_score", "risk_level", "reason",
        ],
    )
    write_csv(
        out_dir / "疑似误合并多边形.csv",
        sorted(high_risk + middle_risk, key=lambda r: (r["risk_score"], r["area"]), reverse=True),
        [
            "id", "area", "bbox_aspect", "mean_value", "max_value", "strong_cell_ratio", "weak_cell_ratio", "low_value_ratio",
            "far_low_background_ratio", "strong_component_count", "weak_component_count", "risk_score", "risk_level", "reason",
        ],
    )
    write_csv(
        out_dir / "强响应未覆盖复核连通域.csv",
        strong_components,
        [
            "cell_count", "max_value", "mean_value", "aspect", "bbox_width_cells", "bbox_height_cells",
            "xmin", "xmax", "ymin", "ymax",
        ],
    )
    write_csv(
        out_dir / "弱响应未覆盖线性连通域.csv",
        weak_components,
        [
            "cell_count", "max_value", "mean_value", "aspect", "bbox_width_cells", "bbox_height_cells",
            "xmin", "xmax", "ymin", "ymax",
        ],
    )

    with open(out_dir / "总体指标.json", "w", encoding="utf-8") as f:
        json.dump(overall, f, ensure_ascii=False, indent=2)

    report_lines = [
        "# 顶尖质量诊断报告",
        "",
        "## 1. 输入",
        "",
        f"- DAT：`{overall['input_dat']}`",
        f"- CSV：`{overall['input_csv']}`",
        f"- 网格：{width} x {height}",
        "",
        "## 2. 总体指标",
        "",
        "| 指标 | 数值 |",
        "|---|---:|",
        f"| 保留多边形数量 | {overall['polygon_count']} |",
        f"| 多边形覆盖像元 | {overall['covered_cells']} |",
        f"| 强响应覆盖率 | {overall['strong_coverage']:.4%} |",
        f"| 弱响应覆盖率 | {overall['weak_coverage']:.4%} |",
        f"| 非零响应覆盖率 | {overall['nonzero_coverage']:.4%} |",
        f"| 强响应需复核连通域 | {overall['strong_review_components']} |",
        f"| 弱响应线性需复核连通域 | {overall['weak_line_review_components']} |",
        f"| 高风险疑似误合并多边形 | {overall['high_risk_merge_polygons']} |",
        f"| 中风险疑似误合并多边形 | {overall['middle_risk_merge_polygons']} |",
        f"| 小碎片多边形数量 | {overall['small_polygon_count']} |",
        f"| 远背景包络比例 | {overall['boundary_background_ratio']:.4%} |",
        f"| 高远背景包络多边形 | {overall['large_background_polygon_count']} |",
        "",
        "## 3. 诊断解释",
        "",
        "本报告不直接判定某个多边形一定错误，而是把需要人工或算法进一步检查的位置列出来。误合并复核规则经过收紧：只有当一个多边形同时表现出多个强响应段、较高低属性背景比例，并且整体形态不够细长时，才会被标为中高风险。单纯长断层沿走向强弱变化不会直接视为误合并。漏检风险通常表现为：强响应或细长弱响应连通域没有被任何正式多边形覆盖。远背景包络比例用于衡量结果是否包入过多远离弱响应的 0 值背景，它不是单独扣分依据，但可辅助判断边界是否过宽、是否存在低值桥黏连。",
        "",
        "## 4. 输出文件",
        "",
        f"- 逐多边形质量诊断：`{out_dir / '逐多边形质量诊断.csv'}`",
        f"- 疑似误合并多边形：`{out_dir / '疑似误合并多边形.csv'}`",
        f"- 强响应未覆盖复核连通域：`{out_dir / '强响应未覆盖复核连通域.csv'}`",
        f"- 弱响应未覆盖线性连通域：`{out_dir / '弱响应未覆盖线性连通域.csv'}`",
        f"- 总体指标 JSON：`{out_dir / '总体指标.json'}`",
        "",
        "## 5. 下一步使用方法",
        "",
        "1. 先查看强响应未覆盖复核连通域，若数量较多，说明主断层召回不足，应优先处理漏检。",
        "2. 再查看疑似误合并多边形，按风险分从高到低检查是否跨越低属性背景。",
        "3. 若误合并风险集中在少数多边形，下一步应做局部拆分，而不是全局调低融合强度。",
        "4. 若小碎片数量上升，说明高召回策略过激，应收紧残余追踪或分支分开输出。",
    ]
    (out_dir / "顶尖质量诊断报告.md").write_text("\n".join(report_lines) + "\n", encoding="utf-8")

    print(out_dir / "顶尖质量诊断报告.md")
    print(out_dir / "总体指标.json")


if __name__ == "__main__":
    main()
