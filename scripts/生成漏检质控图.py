import argparse
import csv
import os
import re
import sys
from collections import deque
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
    return values, xs, ys, dx, dy, extent_edges


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


def read_polygons(path):
    polygons = []
    with open(path, "r", encoding="utf-8", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row.get("status", "").lower() != "kept":
                continue
            coords = parse_wkt_polygon(row.get("wkt", ""))
            if len(coords) >= 3:
                polygons.append(coords)
    return polygons


def rasterize_polygons(polygons, xs, ys):
    dx = abs(xs[1] - xs[0]) if len(xs) > 1 else 1.0
    dy = abs(ys[1] - ys[0]) if len(ys) > 1 else 1.0
    xmin_edge = float(xs.min() - 0.5 * dx)
    ymax_edge = float(ys.max() + 0.5 * dy)
    image = Image.new("L", (len(xs), len(ys)), 0)
    draw = ImageDraw.Draw(image)
    for coords in polygons:
        pixel_coords = [((x - xmin_edge) / dx, (ymax_edge - y) / dy) for x, y in coords]
        if len(pixel_coords) >= 3:
            draw.polygon(pixel_coords, fill=1)
    return np.asarray(image, dtype=bool)[::-1, :]


def connected_components(mask, values, xs, ys):
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
                for dx, dy in offsets:
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h and mask[ny, nx] and not visited[ny, nx]:
                        visited[ny, nx] = True
                        q.append((nx, ny))
            arr = np.asarray(cells, dtype=int)
            cell_values = values[arr[:, 1], arr[:, 0]]
            min_x = int(arr[:, 0].min())
            max_x = int(arr[:, 0].max())
            min_y = int(arr[:, 1].min())
            max_y = int(arr[:, 1].max())
            width = max_x - min_x + 1
            height = max_y - min_y + 1
            aspect = max(width, height) / max(1, min(width, height))
            components.append({
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
                "bbox_width_cells": int(width),
                "bbox_height_cells": int(height),
                "aspect": float(aspect),
            })
    components.sort(key=lambda item: (item["cell_count"], item["max_value"]), reverse=True)
    return components


def draw_base(ax, values, extent_edges):
    ax.set_facecolor("white")
    weak = np.ma.masked_where(values <= 0, values)
    ax.imshow(weak, extent=extent_edges, origin="lower", cmap="gray_r", interpolation="nearest", vmin=0.30, vmax=1.0, alpha=0.45, aspect="equal")
    strong = np.ma.masked_where(values < 0.66, values)
    ax.imshow(strong, extent=extent_edges, origin="lower", cmap="cool", interpolation="nearest", vmin=0.66, vmax=1.0, alpha=0.65, aspect="equal")


def draw_polygons(ax, polygons):
    for coords in polygons:
        arr = np.asarray(coords, dtype=float)
        ax.add_patch(PolygonPatch(arr, closed=True, facecolor="#ff2020", edgecolor="#d00000", linewidth=0.28, alpha=0.13))


def draw_mask(ax, mask, extent_edges, color_cmap, alpha):
    overlay = np.ma.masked_where(~mask, mask.astype(float))
    ax.imshow(overlay, extent=extent_edges, origin="lower", cmap=color_cmap, interpolation="nearest", alpha=alpha, aspect="equal", vmin=0, vmax=1)


def set_axes(ax, extent_edges):
    ax.set_xlim(extent_edges[0], extent_edges[1])
    ax.set_ylim(extent_edges[2], extent_edges[3])
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.tick_params(labelsize=7, colors="#606870")


def main():
    configure_chinese_font()
    parser = argparse.ArgumentParser(description="生成断层多边形漏检质控图")
    parser.add_argument("--dat", required=True)
    parser.add_argument("--csv", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--strong-threshold", type=float, default=0.95)
    parser.add_argument("--weak-threshold", type=float, default=0.66)
    parser.add_argument("--min-review-cells", type=int, default=8)
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    values, xs, ys, _, _, extent_edges = read_geoeast_dat(args.dat)
    polygons = read_polygons(args.csv)
    poly_mask = rasterize_polygons(polygons, xs, ys)
    strong_mask = values >= args.strong_threshold
    weak_mask = values >= args.weak_threshold
    missed_strong = strong_mask & ~poly_mask
    missed_weak = weak_mask & ~poly_mask

    strong_components = connected_components(missed_strong, values, xs, ys)
    weak_components = connected_components(missed_weak, values, xs, ys)

    def write_components(path, components):
        fields = ["rank", "cell_count", "max_value", "mean_value", "aspect", "bbox_width_cells", "bbox_height_cells", "xmin", "xmax", "ymin", "ymax"]
        with open(path, "w", encoding="utf-8-sig", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            for i, item in enumerate(components, start=1):
                row = dict(item)
                row["rank"] = i
                writer.writerow({k: row.get(k, "") for k in fields})

    write_components(out_dir / "强响应未覆盖连通域.csv", strong_components)
    write_components(out_dir / "弱响应未覆盖连通域.csv", weak_components)

    review_strong = np.zeros_like(missed_strong)
    for item in strong_components:
        if item["cell_count"] >= args.min_review_cells:
            review_strong[item["min_y_index"]:item["max_y_index"] + 1, item["min_x_index"]:item["max_x_index"] + 1] |= missed_strong[item["min_y_index"]:item["max_y_index"] + 1, item["min_x_index"]:item["max_x_index"] + 1]

    review_weak = np.zeros_like(missed_weak)
    for item in weak_components:
        if item["cell_count"] >= args.min_review_cells and item["aspect"] >= 3.0:
            review_weak[item["min_y_index"]:item["max_y_index"] + 1, item["min_x_index"]:item["max_x_index"] + 1] |= missed_weak[item["min_y_index"]:item["max_y_index"] + 1, item["min_x_index"]:item["max_x_index"] + 1]

    fig, axes = plt.subplots(1, 3, figsize=(16, 5.5), dpi=220)
    draw_base(axes[0], values, extent_edges)
    draw_polygons(axes[0], polygons)
    axes[0].set_title("结果覆盖：红色填充为断层多边形")
    draw_base(axes[1], values, extent_edges)
    draw_polygons(axes[1], polygons)
    draw_mask(axes[1], review_strong, extent_edges, "autumn", 0.95)
    axes[1].set_title("强响应未覆盖质控：黄红色为需复核区域")
    draw_base(axes[2], values, extent_edges)
    draw_polygons(axes[2], polygons)
    draw_mask(axes[2], review_weak, extent_edges, "winter", 0.95)
    axes[2].set_title("弱线性响应未覆盖质控：蓝绿色为需复核区域")
    for ax in axes:
        set_axes(ax, extent_edges)
    fig.tight_layout()
    fig.savefig(out_dir / "漏检质控总览.png", bbox_inches="tight")
    plt.close(fig)

    strong_total = int(np.count_nonzero(strong_mask))
    weak_total = int(np.count_nonzero(weak_mask))
    with open(out_dir / "漏检质控摘要.txt", "w", encoding="utf-8") as f:
        f.write("断层多边形漏检质控摘要\n")
        f.write(f"输入结果: {args.csv}\n")
        f.write(f"多边形数量: {len(polygons)}\n")
        f.write(f"强响应阈值: {args.strong_threshold}\n")
        f.write(f"弱响应阈值: {args.weak_threshold}\n")
        f.write(f"强响应总像元: {strong_total}\n")
        f.write(f"强响应未覆盖像元: {int(np.count_nonzero(missed_strong))}\n")
        f.write(f"强响应覆盖率: {1.0 - np.count_nonzero(missed_strong) / max(1, strong_total):.6f}\n")
        f.write(f"强响应未覆盖连通域数: {len(strong_components)}\n")
        f.write(f"强响应需复核连通域数(cell_count>={args.min_review_cells}): {sum(1 for c in strong_components if c['cell_count'] >= args.min_review_cells)}\n")
        f.write(f"弱响应总像元: {weak_total}\n")
        f.write(f"弱响应未覆盖像元: {int(np.count_nonzero(missed_weak))}\n")
        f.write(f"弱响应覆盖率: {1.0 - np.count_nonzero(missed_weak) / max(1, weak_total):.6f}\n")
        f.write(f"弱响应未覆盖连通域数: {len(weak_components)}\n")
        f.write(f"弱响应需复核线性连通域数(cell_count>={args.min_review_cells}, aspect>=3): {sum(1 for c in weak_components if c['cell_count'] >= args.min_review_cells and c['aspect'] >= 3.0)}\n")

    print(out_dir / "漏检质控总览.png")
    print(out_dir / "漏检质控摘要.txt")


if __name__ == "__main__":
    main()
