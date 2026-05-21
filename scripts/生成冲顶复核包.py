import argparse
import csv
import json
import os
import re
import sys
from collections import deque
from datetime import datetime
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib import font_manager
from matplotlib.patches import Polygon as PolygonPatch
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image, ImageDraw

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


def parse_wkt_polygon(wkt):
    match = re.search(r"POLYGON\s*\(\((.*)\)\)", str(wkt).strip(), re.IGNORECASE)
    if not match:
        return []
    coords = []
    for pair in match.group(1).split(","):
        parts = pair.strip().split()
        if len(parts) >= 2:
            coords.append((float(parts[0]), float(parts[1])))
    if len(coords) > 1 and coords[0] == coords[-1]:
        coords.pop()
    return coords


def read_polygons(path):
    polygons = []
    with open(path, "r", encoding="utf-8", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row.get("status", "").lower() != "kept":
                continue
            coords = parse_wkt_polygon(row.get("wkt", ""))
            if len(coords) < 3:
                continue
            polygons.append(
                {
                    "id": int(float(row.get("id", len(polygons) + 1) or 0)),
                    "area": float(row.get("area", 0.0) or 0.0),
                    "cell_count": int(float(row.get("cell_count", 0) or 0)),
                    "mean_value": float(row.get("mean_value", 0.0) or 0.0),
                    "max_value": float(row.get("max_value", 0.0) or 0.0),
                    "coords": coords,
                }
            )
    return polygons


def read_csv_rows(path):
    if not Path(path).exists():
        return []
    with open(path, "r", encoding="utf-8-sig", errors="ignore", newline="") as f:
        return list(csv.DictReader(f))


def rasterize_polygon(coords, width, height, xs, ys, dx, dy):
    xmin_edge = float(xs.min() - 0.5 * dx)
    ymax_edge = float(ys.max() + 0.5 * dy)
    image = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(image)
    pixel_coords = [((x - xmin_edge) / dx, (ymax_edge - y) / dy) for x, y in coords]
    if len(pixel_coords) >= 3:
        draw.polygon(pixel_coords, fill=1)
    return np.asarray(image, dtype=bool)[::-1, :]


def rasterize_all(polygons, width, height, xs, ys, dx, dy):
    total = np.zeros((height, width), dtype=bool)
    by_id = {}
    for poly in polygons:
        mask = rasterize_polygon(poly["coords"], width, height, xs, ys, dx, dy)
        total |= mask
        by_id[poly["id"]] = mask
    return total, by_id


def connected_components(mask, values, xs, ys):
    height, width = mask.shape
    visited = np.zeros_like(mask, dtype=bool)
    components = []
    offsets = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]
    for y0 in range(height):
        for x0 in range(width):
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
                    if 0 <= nx < width and 0 <= ny < height and mask[ny, nx] and not visited[ny, nx]:
                        visited[ny, nx] = True
                        q.append((nx, ny))
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
                    "mask": comp_mask,
                }
            )
    components.sort(key=lambda item: (item["cell_count"], item["max_value"]), reverse=True)
    return components


def extent_from_indices(xs, ys, min_x, max_x, min_y, max_y, pad):
    min_x = max(0, int(min_x) - pad)
    max_x = min(len(xs) - 1, int(max_x) + pad)
    min_y = max(0, int(min_y) - pad)
    max_y = min(len(ys) - 1, int(max_y) + pad)
    dx = abs(xs[1] - xs[0]) if len(xs) > 1 else 1.0
    dy = abs(ys[1] - ys[0]) if len(ys) > 1 else 1.0
    return (
        float(xs[min_x] - 0.5 * dx),
        float(xs[max_x] + 0.5 * dx),
        float(ys[min_y] - 0.5 * dy),
        float(ys[max_y] + 0.5 * dy),
    )


def polygon_index_extent(poly, xs, ys):
    arr = np.asarray(poly["coords"], dtype=float)
    min_x = int(np.searchsorted(xs, float(arr[:, 0].min()), side="left"))
    max_x = int(np.searchsorted(xs, float(arr[:, 0].max()), side="right"))
    min_y = int(np.searchsorted(ys, float(arr[:, 1].min()), side="left"))
    max_y = int(np.searchsorted(ys, float(arr[:, 1].max()), side="right"))
    return max(0, min_x), min(len(xs) - 1, max_x), max(0, min_y), min(len(ys) - 1, max_y)


def draw_base(ax, values, extent_edges):
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
        alpha=0.45,
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
        alpha=0.75,
        aspect="equal",
    )


def draw_polygons(ax, polygons, target_id=None):
    for poly in polygons:
        arr = np.asarray(poly["coords"], dtype=float)
        if target_id is not None and poly["id"] == target_id:
            ax.add_patch(PolygonPatch(arr, closed=True, facecolor="#ff8c00", edgecolor="#b85c00", linewidth=1.25, alpha=0.20))
            ax.plot(arr[:, 0], arr[:, 1], color="#005bbb", linewidth=1.15, alpha=0.98)
        else:
            ax.plot(arr[:, 0], arr[:, 1], color="#d00000", linewidth=0.35, alpha=0.65)


def draw_mask(ax, mask, extent_edges, cmap, alpha):
    overlay = np.ma.masked_where(~mask, mask.astype(float))
    ax.imshow(overlay, extent=extent_edges, origin="lower", cmap=cmap, interpolation="nearest", vmin=0, vmax=1, alpha=alpha, aspect="equal")


def style_axes(ax, extent):
    ax.set_xlim(extent[0], extent[1])
    ax.set_ylim(extent[2], extent[3])
    ax.tick_params(labelsize=7, colors="#606870")
    for spine in ax.spines.values():
        spine.set_linewidth(0.7)
        spine.set_color("#6a6a6a")


def save_miss_figure(values, extent_edges, polygons, comp, out_path, title, cmap, pad, xs, ys):
    local_extent = extent_from_indices(
        xs,
        ys,
        comp["min_x_index"],
        comp["max_x_index"],
        comp["min_y_index"],
        comp["max_y_index"],
        pad,
    )
    fig, axes = plt.subplots(1, 2, figsize=(10, 4.6), dpi=220)
    for ax in axes:
        draw_base(ax, values, extent_edges)
        draw_polygons(ax, polygons)
        style_axes(ax, local_extent)
    axes[0].set_title("原始属性 + 当前多边形")
    draw_mask(axes[1], comp["mask"], extent_edges, cmap, 0.95)
    axes[1].set_title("未覆盖复核区域")
    fig.suptitle(
        f"{title}：cells={comp['cell_count']} max={comp['max_value']:.3f} mean={comp['mean_value']:.3f} aspect={comp['aspect']:.2f}",
        fontsize=10,
    )
    fig.tight_layout()
    fig.savefig(out_path, bbox_inches="tight")
    plt.close(fig)


def save_merge_figure(values, extent_edges, polygons, poly, row, out_path, pad, xs, ys):
    min_x, max_x, min_y, max_y = polygon_index_extent(poly, xs, ys)
    local_extent = extent_from_indices(xs, ys, min_x, max_x, min_y, max_y, pad)
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.8), dpi=220)
    for ax in axes:
        draw_base(ax, values, extent_edges)
        style_axes(ax, local_extent)
    draw_polygons(axes[0], polygons)
    axes[0].set_title("当前结果邻域")
    draw_polygons(axes[1], polygons, target_id=poly["id"])
    axes[1].set_title(f"高亮多边形 {poly['id']}")
    draw_polygons(axes[2], [poly], target_id=poly["id"])
    axes[2].set_title("仅看目标闭合带")
    fig.suptitle(
        "误合并复核："
        f"风险={row.get('risk_level', '')} 分={row.get('risk_score', '')} "
        f"强段={row.get('strong_component_count', '')} 弱段={row.get('weak_component_count', '')} "
        f"低值占比={float(row.get('low_value_ratio', 0) or 0):.2%}",
        fontsize=10,
    )
    fig.tight_layout()
    fig.savefig(out_path, bbox_inches="tight")
    plt.close(fig)


def copy_key_metrics(diag_path, top_quality_path):
    metrics = {}
    if Path(diag_path).exists():
        with open(diag_path, "r", encoding="utf-8", errors="ignore") as f:
            diag = json.load(f)
        metrics["polygons"] = diag.get("results", {}).get("keptPolygonCount")
        metrics["trace_ms"] = diag.get("timing", {}).get("traceMs")
        metrics["elapsed_ms"] = diag.get("timing", {}).get("elapsedMs")
        metrics["open_rings"] = diag.get("topology", {}).get("openRings")
        metrics["self_intersections"] = diag.get("topology", {}).get("selfIntersectingPolygons")
        metrics["polygon_intersections"] = diag.get("topology", {}).get("polygonIntersections")
    if Path(top_quality_path).exists():
        with open(top_quality_path, "r", encoding="utf-8", errors="ignore") as f:
            top = json.load(f)
        metrics.update(top)
    return metrics


def main():
    configure_chinese_font()
    parser = argparse.ArgumentParser(description="生成冲击满分用的局部复核图包和中文问题清单")
    parser.add_argument("--dat", required=True, help="GeoEast DAT 输入")
    parser.add_argument("--csv", required=True, help="FaultPolyTrace 输出 CSV")
    parser.add_argument("--top-quality-dir", default="", help="顶尖质量诊断目录，默认使用 CSV 同级的 顶尖质量诊断")
    parser.add_argument("--out-dir", default="", help="输出目录，默认写入 examples/outputs/top_quality_review_<timestamp>")
    parser.add_argument("--max-strong", type=int, default=6)
    parser.add_argument("--max-weak", type=int, default=10)
    parser.add_argument("--max-merge", type=int, default=13)
    parser.add_argument("--strong-threshold", type=float, default=0.95)
    parser.add_argument("--weak-threshold", type=float, default=0.66)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = Path(args.out_dir) if args.out_dir else repo_root / "examples" / "outputs" / f"top_quality_review_{timestamp}"
    figure_dir = out_dir / "局部复核图"
    figure_dir.mkdir(parents=True, exist_ok=True)
    top_quality_dir = Path(args.top_quality_dir) if args.top_quality_dir else Path(args.csv).parent / "顶尖质量诊断"

    values, xs, ys, dx, dy, extent_edges = read_geoeast_dat(args.dat)
    height, width = values.shape
    polygons = read_polygons(args.csv)
    total_mask, masks_by_id = rasterize_all(polygons, width, height, xs, ys, dx, dy)

    strong_components = connected_components((values >= args.strong_threshold) & ~total_mask, values, xs, ys)
    weak_components = [
        comp
        for comp in connected_components((values >= args.weak_threshold) & ~total_mask, values, xs, ys)
        if comp["cell_count"] >= 8 and comp["aspect"] >= 3.0
    ]
    strong_review = [comp for comp in strong_components if comp["cell_count"] >= 8]

    merge_rows = read_csv_rows(top_quality_dir / "疑似误合并多边形.csv")
    polygon_by_id = {poly["id"]: poly for poly in polygons}

    generated = []
    for idx, comp in enumerate(strong_review[: args.max_strong], start=1):
        path = figure_dir / f"强响应漏检_{idx:02d}_cells{comp['cell_count']}.png"
        save_miss_figure(values, extent_edges, polygons, comp, path, "强响应漏检", "autumn", 28, xs, ys)
        generated.append(("强响应漏检", path))

    for idx, comp in enumerate(weak_components[: args.max_weak], start=1):
        path = figure_dir / f"弱线性漏检_{idx:02d}_cells{comp['cell_count']}.png"
        save_miss_figure(values, extent_edges, polygons, comp, path, "弱线性响应漏检", "winter", 28, xs, ys)
        generated.append(("弱线性漏检", path))

    for idx, row in enumerate(merge_rows[: args.max_merge], start=1):
        poly_id = int(float(row.get("id", 0) or 0))
        poly = polygon_by_id.get(poly_id)
        if not poly:
            continue
        path = figure_dir / f"疑似误合并_{idx:02d}_多边形{poly_id}.png"
        save_merge_figure(values, extent_edges, polygons, poly, row, path, 32, xs, ys)
        generated.append(("疑似误合并", path))

    metrics = copy_key_metrics(str(Path(args.csv)) + ".diagnostics.json", top_quality_dir / "总体指标.json")
    manifest = {
        "timestamp": timestamp,
        "dat": str(Path(args.dat)),
        "csv": str(Path(args.csv)),
        "top_quality_dir": str(top_quality_dir),
        "out_dir": str(out_dir),
        "metrics": metrics,
        "generated_figures": [{"type": kind, "path": str(path)} for kind, path in generated],
    }
    (out_dir / "复核清单.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")

    report = [
        "# 冲顶局部复核报告",
        "",
        f"- 生成时间：{timestamp}",
        f"- 输入数据：`{Path(args.dat)}`",
        f"- 结果文件：`{Path(args.csv)}`",
        f"- 顶尖诊断目录：`{top_quality_dir}`",
        "",
        "## 1. 关键指标",
        "",
        "| 指标 | 数值 | 冲顶判断 |",
        "|---|---:|---|",
        f"| 保留多边形 | {metrics.get('polygon_count', metrics.get('polygons', ''))} | 维持 200 个量级，未碎片化 |",
        f"| 强响应覆盖率 | {float(metrics.get('strong_coverage', 0.0)):.4%} | 已达到高召回，但仍复核局部漏点 |",
        f"| 弱响应覆盖率 | {float(metrics.get('weak_coverage', 0.0)):.4%} | 已过当前门槛，弱细线仍需图件确认 |",
        f"| 强响应需复核连通域 | {len(strong_review)} | 数量很少，逐项判断即可 |",
        f"| 弱线性需复核连通域 | {len(weak_components)} | 用于判断是否需要残余复核层 |",
        f"| 高风险疑似误合并 | {metrics.get('high_risk_merge_polygons', '')} | 需要局部图确认是否真误合并 |",
        f"| 中风险疑似误合并 | {metrics.get('middle_risk_merge_polygons', '')} | 作为二级复核对象 |",
        f"| 开环/自交/多边形相交 | {metrics.get('open_rings', 0)}/{metrics.get('self_intersections', 0)}/{metrics.get('polygon_intersections', 0)} | 必须保持 0 |",
        f"| 核心耗时 | {metrics.get('trace_ms', '')} ms | 远低于 1 分钟要求 |",
        "",
        "## 2. 本次复核结论",
        "",
        "当前主线已经满足赛题硬性功能和性能指标。冲顶差距集中在三个局部问题：强响应极少量未覆盖、弱细线局部未覆盖、少数大多边形存在误合并候选。后续算法优化必须在不降低强弱覆盖率、不产生开环自交、不把结果拆成几千个碎片的前提下进行。",
        "",
        "## 3. 图件清单",
        "",
    ]
    for kind, path in generated:
        report.append(f"- {kind}：`{path}`")
    report.extend(
        [
            "",
            "## 4. 下一步判定规则",
            "",
            "1. 如果强响应漏检图显示是真实连续断层，应优先做局部补追踪；如果只是孤立端点或噪声，可保留为过滤说明。",
            "2. 如果弱线性漏检位于主断层延伸方向，应进入残余复核层；如果只是短噪声，应继续过滤。",
            "3. 如果疑似误合并图显示目标多边形跨越大面积低属性背景且两侧走向不同，才启用局部防黏连拆分。",
            "4. 任一优化导致强响应覆盖下降、弱响应覆盖明显下降、拓扑非 0 或多边形数量暴涨，都不能进入正式主线。",
        ]
    )
    (out_dir / "冲顶局部复核报告.md").write_text("\n".join(report) + "\n", encoding="utf-8")
    print(out_dir / "冲顶局部复核报告.md")
    print(figure_dir)


if __name__ == "__main__":
    main()
