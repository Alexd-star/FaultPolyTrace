# -*- coding: utf-8 -*-
"""Generate full-map and tiled visual review figures for fault polygons.

This script is intentionally evidence-oriented: it does not change algorithm
outputs. It creates a reproducible image pack so the whole map can be inspected
tile by tile, including original fault attributes, result polygons, and local
shape candidates.
"""

import argparse
import csv
import math
import os
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib import font_manager
import matplotlib.image as mpimg
from matplotlib.patches import Polygon as PolygonPatch, Rectangle
import matplotlib.pyplot as plt
import numpy as np

from 局部形态质量诊断 import read_geoeast_dat, read_polygons


def configure_chinese_font():
    for path in [r"C:\Windows\Fonts\msyh.ttc", r"C:\Windows\Fonts\simhei.ttf", r"C:\Windows\Fonts\simsun.ttc"]:
        if os.path.exists(path):
            font_manager.fontManager.addfont(path)
            font_name = font_manager.FontProperties(fname=path).get_name()
            plt.rcParams["font.sans-serif"] = [font_name, "DejaVu Sans"]
            plt.rcParams["axes.unicode_minus"] = False
            return


def build_tile_windows(width, height, cols, rows):
    if width <= 0:
        raise ValueError("width must be positive")
    if height <= 0:
        raise ValueError("height must be positive")
    if cols <= 0 or rows <= 0:
        raise ValueError("cols and rows must be positive")

    x_edges = [int(round(i * width / cols)) for i in range(cols + 1)]
    y_edges = [int(round(i * height / rows)) for i in range(rows + 1)]
    x_edges[0] = 0
    x_edges[-1] = width
    y_edges[0] = 0
    y_edges[-1] = height

    tiles = []
    for row in range(rows):
        for col in range(cols):
            tiles.append(
                {
                    "tile_id": f"R{row + 1}C{col + 1}",
                    "row": row + 1,
                    "col": col + 1,
                    "x0": x_edges[col],
                    "x1": x_edges[col + 1],
                    "y0": y_edges[row],
                    "y1": y_edges[row + 1],
                }
            )
    return tiles


def index_window_to_world(tile, xs, ys, dx, dy):
    x0 = max(0, min(len(xs) - 1, tile["x0"]))
    x1 = max(0, min(len(xs), tile["x1"]))
    y0 = max(0, min(len(ys) - 1, tile["y0"]))
    y1 = max(0, min(len(ys), tile["y1"]))
    return (
        float(xs[x0] - 0.5 * dx),
        float(xs[x1 - 1] + 0.5 * dx),
        float(ys[y0] - 0.5 * dy),
        float(ys[y1 - 1] + 0.5 * dy),
    )


def bounds_overlap(a, b):
    return not (a[1] < b[0] or a[0] > b[1] or a[3] < b[2] or a[2] > b[3])


def polygon_bbox(polygon):
    coords = np.asarray(polygon["coords"], dtype=float)
    return (float(coords[:, 0].min()), float(coords[:, 0].max()), float(coords[:, 1].min()), float(coords[:, 1].max()))


def read_shape_candidates(path):
    if not path or not Path(path).exists():
        return []
    candidates = []
    with open(path, "r", encoding="utf-8-sig", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                candidates.append(
                    {
                        "type": row.get("type", ""),
                        "shape_class": row.get("shape_class", ""),
                        "review_priority": row.get("review_priority", ""),
                        "polygon_id": row.get("polygon_id", ""),
                        "bbox": (
                            float(row.get("xmin", 0.0)),
                            float(row.get("xmax", 0.0)),
                            float(row.get("ymin", 0.0)),
                            float(row.get("ymax", 0.0)),
                        ),
                    }
                )
            except ValueError:
                continue
    return candidates


def setup_axis(ax, values, extent_edges):
    ax.set_facecolor("white")
    weak = np.ma.masked_where(values <= 0, values)
    ax.imshow(
        weak,
        extent=extent_edges,
        origin="lower",
        cmap="viridis",
        interpolation="nearest",
        vmin=0.0,
        vmax=1.0,
        alpha=0.74,
        aspect="equal",
    )
    ax.tick_params(labelsize=7, colors="#4f5963", length=2)
    ax.set_xlabel("X", fontsize=8)
    ax.set_ylabel("Y", fontsize=8)


def draw_polygons(ax, polygons, view_bbox, linewidth=0.55):
    count = 0
    for polygon in polygons:
        bbox = polygon_bbox(polygon)
        if not bounds_overlap(bbox, view_bbox):
            continue
        coords = np.asarray(polygon["coords"], dtype=float)
        patch = PolygonPatch(coords, closed=True, facecolor="none", edgecolor="#005BBB", linewidth=linewidth, alpha=0.95)
        ax.add_patch(patch)
        for hole in polygon["holes"]:
            if len(hole) < 3:
                continue
            hole_coords = np.asarray(hole, dtype=float)
            ax.plot(hole_coords[:, 0], hole_coords[:, 1], color="#005BBB", linewidth=max(0.35, linewidth * 0.75), linestyle="--", alpha=0.90)
        count += 1
    return count


def draw_candidates(ax, candidates, view_bbox):
    count = 0
    for candidate in candidates:
        bbox = candidate["bbox"]
        if not bounds_overlap(bbox, view_bbox):
            continue
        color = "#d46a00" if candidate.get("shape_class") == "疑似分支归属错误" else "#ffb000"
        ax.add_patch(
            Rectangle(
                (bbox[0], bbox[2]),
                bbox[1] - bbox[0],
                bbox[3] - bbox[2],
                fill=False,
                edgecolor=color,
                linewidth=1.5,
            )
        )
        count += 1
    return count


def draw_overview(path, values, extent_edges, xs, ys, dx, dy, polygons, candidates, tiles):
    fig, ax = plt.subplots(figsize=(12.0, 6.1), dpi=240)
    setup_axis(ax, values, extent_edges)
    draw_polygons(ax, polygons, extent_edges, linewidth=0.40)
    draw_candidates(ax, candidates, extent_edges)
    for tile in tiles:
        bbox = index_window_to_world(tile, xs, ys, dx, dy)
        ax.add_patch(Rectangle((bbox[0], bbox[2]), bbox[1] - bbox[0], bbox[3] - bbox[2], fill=False, edgecolor="#222222", linewidth=0.45, alpha=0.70))
        ax.text((bbox[0] + bbox[1]) / 2, (bbox[2] + bbox[3]) / 2, tile["tile_id"], ha="center", va="center", fontsize=6.5, color="#111111")
    ax.set_xlim(extent_edges[0], extent_edges[1])
    ax.set_ylim(extent_edges[2], extent_edges[3])
    ax.set_title("FaultPolyTrace 全图视觉复核总览（白底、分块编号、候选框）", fontsize=11)
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)


def draw_tile(path, values, extent_edges, xs, ys, dx, dy, polygons, candidates, tile):
    bbox = index_window_to_world(tile, xs, ys, dx, dy)
    fig, ax = plt.subplots(figsize=(8.2, 5.2), dpi=240)
    setup_axis(ax, values, extent_edges)
    polygon_count = draw_polygons(ax, polygons, bbox, linewidth=0.80)
    candidate_count = draw_candidates(ax, candidates, bbox)
    ax.set_xlim(bbox[0], bbox[1])
    ax.set_ylim(bbox[2], bbox[3])
    ax.set_title(
        f"{tile['tile_id']} 分块复核：polygons={polygon_count}, shape candidates={candidate_count}",
        fontsize=10,
    )
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)


def draw_contact_sheet(tile_paths, output_path, columns=6):
    """Create a compact contact sheet for all review tiles.

    The individual tile files remain the source of detailed inspection; this
    sheet is an index image that lets reviewers see whether every block was
    generated and where the remaining candidate boxes are concentrated.
    """
    tile_paths = [Path(path) for path in tile_paths]
    if not tile_paths:
        raise ValueError("tile_paths must not be empty")
    columns = max(1, int(columns))
    rows = int(math.ceil(len(tile_paths) / columns))

    fig, axes = plt.subplots(rows, columns, figsize=(columns * 2.15, rows * 1.45), dpi=220)
    axes = np.asarray(axes).reshape(rows, columns)
    for index, ax in enumerate(axes.flat):
        ax.set_axis_off()
        if index >= len(tile_paths):
            continue
        tile_path = tile_paths[index]
        image = mpimg.imread(tile_path)
        ax.imshow(image)
        ax.set_title(tile_path.stem.replace("_tile", ""), fontsize=7, pad=2)
    fig.patch.set_facecolor("white")
    fig.tight_layout(pad=0.45)
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, bbox_inches="tight", facecolor="white")
    plt.close(fig)


def write_review_template(path, tiles, candidates):
    lines = [
        "# 全图分块视觉复核报告",
        "",
        "本报告用于记录人工逐图观察结果。每个分块都覆盖官方 test.dat 的一个固定范围，观察重点包括：该覆盖的断层响应是否覆盖、是否存在邻近断层误合并、局部鼓包/内环是否明显、边界是否自然、是否存在数据边界截断或弱响应复核候选。",
        "",
        f"- 分块数量：{len(tiles)}",
        f"- 局部形态候选数量：{len(candidates)}",
        "",
        "## 逐块观察记录",
        "",
        "| 分块 | 图件 | 初步观察 | 风险等级 | 后续处理建议 |",
        "|---|---|---|---|---|",
    ]
    for tile in tiles:
        image_name = f"{tile['tile_id']}_tile.png"
        lines.append(f"| {tile['tile_id']} | tiles/{image_name} | 待观察 | 待定 | 待定 |")
    lines.extend(
        [
            "",
            "## 图例",
            "",
            "- 原始属性响应：蓝绿黄色渐变，数值越高越接近强断层响应。",
            "- 结果多边形：深蓝色闭合边界。",
            "- 局部形态候选：橙色或深橙色矩形框；深橙色优先检查疑似分支归属错误。",
        ]
    )
    path.write_text("\n".join(lines), encoding="utf-8")


def main():
    configure_chinese_font()
    parser = argparse.ArgumentParser(description="生成断层多边形全图分块视觉复核图")
    parser.add_argument("--dat", required=True, help="GeoEast DAT 输入")
    parser.add_argument("--csv", required=True, help="断层多边形结果 CSV")
    parser.add_argument("--shape-candidates", default="", help="局部形态质量诊断 CSV")
    parser.add_argument("--out-dir", required=True, help="输出目录")
    parser.add_argument("--cols", type=int, default=6, help="横向分块数")
    parser.add_argument("--rows", type=int, default=4, help="纵向分块数")
    args = parser.parse_args()

    values, xs, ys, dx, dy, extent_edges = read_geoeast_dat(args.dat)
    polygons = read_polygons(args.csv)
    candidates = read_shape_candidates(args.shape_candidates)
    tiles = build_tile_windows(width=len(xs), height=len(ys), cols=args.cols, rows=args.rows)

    out_dir = Path(args.out_dir)
    tiles_dir = out_dir / "tiles"
    out_dir.mkdir(parents=True, exist_ok=True)
    tiles_dir.mkdir(parents=True, exist_ok=True)

    draw_overview(out_dir / "00_full_overview_with_tiles.png", values, extent_edges, xs, ys, dx, dy, polygons, candidates, tiles)
    tile_paths = []
    for tile in tiles:
        tile_path = tiles_dir / f"{tile['tile_id']}_tile.png"
        draw_tile(tile_path, values, extent_edges, xs, ys, dx, dy, polygons, candidates, tile)
        tile_paths.append(tile_path)
    contact_sheet = out_dir / "tiles_contact_sheet.png"
    draw_contact_sheet(tile_paths, contact_sheet, columns=args.cols)
    write_review_template(out_dir / "全图分块视觉复核报告.md", tiles, candidates)
    print(out_dir / "00_full_overview_with_tiles.png")
    print(contact_sheet)
    print(tiles_dir)
    print(out_dir / "全图分块视觉复核报告.md")


if __name__ == "__main__":
    main()
