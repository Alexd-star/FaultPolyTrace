import argparse
import csv
import json
import os
import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib import font_manager
from matplotlib.path import Path as MplPath
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
    extent_edges = (float(xs.min() - 0.5 * dx), float(xs.max() + 0.5 * dx), float(ys.min() - 0.5 * dy), float(ys.max() + 0.5 * dy))
    extent_centers = (float(xs.min()), float(xs.max()), float(ys.min()), float(ys.max()))
    return values, xs, ys, dx, dy, extent_edges, extent_centers


def parse_wkt_polygon(wkt):
    match = re.search(r"POLYGON\s*\(\((.*)\)\)", wkt.strip(), re.IGNORECASE)
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


def read_polygons(csv_path):
    polygons = []
    with open(csv_path, "r", encoding="utf-8", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row.get("status", "").lower() != "kept":
                continue
            coords = parse_wkt_polygon(row.get("wkt", ""))
            if len(coords) >= 3:
                polygons.append(coords)
    return polygons


def rasterize_polygons(polygons, xs, ys):
    if len(xs) == 0 or len(ys) == 0:
        return np.zeros((len(ys), len(xs)), dtype=bool)
    dx = abs(xs[1] - xs[0]) if len(xs) > 1 else 1.0
    dy = abs(ys[1] - ys[0]) if len(ys) > 1 else 1.0
    xmin_edge = float(xs.min() - 0.5 * dx)
    ymax_edge = float(ys.max() + 0.5 * dy)
    image = Image.new("L", (len(xs), len(ys)), 0)
    draw = ImageDraw.Draw(image)
    for coords in polygons:
        pixel_coords = [
            ((x - xmin_edge) / dx, (ymax_edge - y) / dy)
            for x, y in coords
        ]
        if len(pixel_coords) >= 3:
            draw.polygon(pixel_coords, fill=1)
    return np.asarray(image, dtype=bool)[::-1, :]


def line_like_pixels(values, threshold):
    return np.isfinite(values) & (values >= threshold)


def draw_official_style(ax, values, extent_edges):
    ax.set_facecolor("white")
    weak = np.ma.masked_where(values <= 0, values)
    ax.imshow(weak, extent=extent_edges, origin="lower", cmap="gray_r", interpolation="nearest", vmin=0.30, vmax=1.0, alpha=0.55, aspect="equal")
    strong = np.ma.masked_where(values < 0.66, values)
    ax.imshow(strong, extent=extent_edges, origin="lower", cmap="cool", interpolation="nearest", vmin=0.66, vmax=1.0, alpha=0.78, aspect="equal")


def draw_polygons(ax, polygons, fill=False):
    for coords in polygons:
        arr = np.asarray(coords, dtype=float)
        if fill:
            ax.add_patch(PolygonPatch(arr, closed=True, facecolor="#ff2020", edgecolor="#e00000", linewidth=0.36, alpha=0.18))
        else:
            ax.plot(arr[:, 0], arr[:, 1], color="#e00000", linewidth=0.42, alpha=0.95)


def set_axes(ax, extent_edges):
    ax.set_xlim(extent_edges[0], extent_edges[1])
    ax.set_ylim(extent_edges[2], extent_edges[3])
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.tick_params(labelsize=7, colors="#606870")
    for spine in ax.spines.values():
        spine.set_color("#6a6a6a")
        spine.set_linewidth(0.7)


def evaluate_run(values, xs, ys, polygons, strong_threshold, weak_threshold):
    polygon_mask = rasterize_polygons(polygons, xs, ys)
    strong_mask = line_like_pixels(values, strong_threshold)
    weak_mask = line_like_pixels(values, weak_threshold)
    nonzero_mask = values > 0
    covered = values[polygon_mask]

    def ratio(a, b):
        denominator = int(np.count_nonzero(b))
        if denominator == 0:
            return 0.0
        return float(np.count_nonzero(a & b)) / denominator

    return {
        "polygon_cells": int(np.count_nonzero(polygon_mask)),
        "strong_threshold": strong_threshold,
        "weak_threshold": weak_threshold,
        "strong_coverage": ratio(polygon_mask, strong_mask),
        "weak_coverage": ratio(polygon_mask, weak_mask),
        "nonzero_coverage": ratio(polygon_mask, nonzero_mask),
        "polygon_mean_attribute": float(np.mean(covered)) if covered.size else 0.0,
        "polygon_high_value_ratio": float(np.count_nonzero(covered >= weak_threshold) / covered.size) if covered.size else 0.0,
    }


def render_run_figures(values, extent_edges, polygons, output_dir, name):
    output_dir.mkdir(parents=True, exist_ok=True)
    fig, axes = plt.subplots(1, 3, figsize=(16, 5.5), dpi=220)
    draw_official_style(axes[0], values, extent_edges)
    axes[0].set_title("原始属性：官方风格渲染")
    draw_official_style(axes[1], values, extent_edges)
    draw_polygons(axes[1], polygons, fill=False)
    axes[1].set_title(f"边界叠加：{len(polygons)} 个闭合多边形")
    draw_official_style(axes[2], values, extent_edges)
    draw_polygons(axes[2], polygons, fill=True)
    axes[2].set_title("半透明填充：检查多边形是否覆盖断层带")
    for ax in axes:
        set_axes(ax, extent_edges)
    fig.tight_layout()
    fig.savefig(output_dir / f"{name}_01_官方风格_边界_填充对比.png", bbox_inches="tight")
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(12, 5.8), dpi=240)
    draw_official_style(ax, values, extent_edges)
    draw_polygons(ax, polygons, fill=True)
    draw_polygons(ax, polygons, fill=False)
    set_axes(ax, extent_edges)
    ax.set_title(f"{name}：结果叠加细查图")
    fig.tight_layout()
    fig.savefig(output_dir / f"{name}_02_结果叠加细查图.png", bbox_inches="tight")
    plt.close(fig)


def main():
    configure_chinese_font()
    parser = argparse.ArgumentParser(description="生成断层多边形质量评估图和覆盖率指标")
    parser.add_argument("--dat", required=True)
    parser.add_argument("--runs-dir", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--include-regex", default="", help="只评估文件名匹配该正则表达式的结果")
    parser.add_argument("--strong-threshold", type=float, default=0.95)
    parser.add_argument("--weak-threshold", type=float, default=0.66)
    args = parser.parse_args()

    values, xs, ys, _, _, extent_edges, _ = read_geoeast_dat(args.dat)
    runs_dir = Path(args.runs_dir)
    out_dir = Path(args.out_dir)
    figures_dir = out_dir / "figures"
    metrics_dir = out_dir / "metrics"
    figures_dir.mkdir(parents=True, exist_ok=True)
    metrics_dir.mkdir(parents=True, exist_ok=True)

    summary = []
    include_pattern = re.compile(args.include_regex) if args.include_regex else None
    for csv_path in sorted(runs_dir.glob("*.csv")):
        if csv_path.name.endswith(".branches.csv") or csv_path.name.endswith(".topology.csv"):
            continue
        if csv_path.name.lower().startswith("run_manifest"):
            continue
        name = csv_path.stem
        if include_pattern and not include_pattern.search(name):
            continue
        polygons = read_polygons(csv_path)
        metrics = evaluate_run(values, xs, ys, polygons, args.strong_threshold, args.weak_threshold)
        diag_path = csv_path.with_suffix(csv_path.suffix + ".diagnostics.json")
        if diag_path.exists():
            with open(diag_path, "r", encoding="utf-8", errors="ignore") as f:
                diag = json.load(f)
            metrics.update({
                "polygons": diag["results"]["keptPolygonCount"],
                "traceMs": diag["timing"]["traceMs"],
                "elapsedMs": diag["timing"]["elapsedMs"],
                "openRings": diag["topology"]["openRings"],
                "selfIntersections": diag["topology"]["selfIntersectingPolygons"],
                "polygonIntersections": diag["topology"]["polygonIntersections"],
            })
        render_run_figures(values, extent_edges, polygons, figures_dir, name)
        metrics["name"] = name
        summary.append(metrics)

    fields = [
        "name", "polygons", "polygon_cells", "strong_coverage", "weak_coverage", "nonzero_coverage",
        "polygon_mean_attribute", "polygon_high_value_ratio", "traceMs", "elapsedMs",
        "openRings", "selfIntersections", "polygonIntersections"
    ]
    with open(metrics_dir / "质量评估汇总.csv", "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        for row in summary:
            writer.writerow(row)

    with open(metrics_dir / "质量评估汇总.json", "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)

    print(metrics_dir / "质量评估汇总.csv")
    print(figures_dir)


if __name__ == "__main__":
    main()
