# -*- coding: utf-8 -*-
"""Generate clean fault-polygon overview and tiled figures without review boxes."""

import argparse
import math
import os
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib import font_manager
import matplotlib.image as mpimg
from matplotlib.patches import Polygon as PolygonPatch
import matplotlib.pyplot as plt
import numpy as np

from 全图分块视觉复核 import build_tile_windows, index_window_to_world, bounds_overlap, polygon_bbox
from 局部形态质量诊断 import read_geoeast_dat, read_polygons


def configure_chinese_font():
    for path in [r"C:\Windows\Fonts\msyh.ttc", r"C:\Windows\Fonts\simhei.ttf", r"C:\Windows\Fonts\simsun.ttc"]:
        if os.path.exists(path):
            font_manager.fontManager.addfont(path)
            font_name = font_manager.FontProperties(fname=path).get_name()
            plt.rcParams["font.sans-serif"] = [font_name, "DejaVu Sans"]
            plt.rcParams["axes.unicode_minus"] = False
            return


def draw_background(ax, values, extent_edges, mode):
    ax.set_facecolor("white")
    if mode == "attribute":
        weak = np.ma.masked_where(values <= 0, values)
        ax.imshow(
            weak,
            extent=extent_edges,
            origin="lower",
            cmap="viridis",
            interpolation="nearest",
            vmin=0.0,
            vmax=1.0,
            alpha=0.68,
            aspect="equal",
        )
    ax.tick_params(labelsize=7, colors="#4f5963", length=2)
    ax.set_xlabel("X", fontsize=8)
    ax.set_ylabel("Y", fontsize=8)


def draw_polygons(ax, polygons, view_bbox, linewidth=0.55, color="#005BBB"):
    count = 0
    for polygon in polygons:
        bbox = polygon_bbox(polygon)
        if not bounds_overlap(bbox, view_bbox):
            continue
        coords = np.asarray(polygon["coords"], dtype=float)
        ax.add_patch(
            PolygonPatch(
                coords,
                closed=True,
                facecolor="none",
                edgecolor=color,
                linewidth=linewidth,
                alpha=0.96,
            )
        )
        for hole in polygon["holes"]:
            if len(hole) < 3:
                continue
            hole_coords = np.asarray(hole, dtype=float)
            ax.plot(hole_coords[:, 0], hole_coords[:, 1], color=color, linewidth=max(0.35, linewidth * 0.72), linestyle="--", alpha=0.92)
        count += 1
    return count


def draw_overview(path, values, extent_edges, polygons, mode):
    fig, ax = plt.subplots(figsize=(12.0, 6.1), dpi=260)
    draw_background(ax, values, extent_edges, mode)
    draw_polygons(ax, polygons, extent_edges, linewidth=0.42)
    ax.set_xlim(extent_edges[0], extent_edges[1])
    ax.set_ylim(extent_edges[2], extent_edges[3])
    title = "断层多边形总览（无候选框）" if mode == "polygon" else "断层多边形叠加总览（无候选框）"
    ax.set_title(title, fontsize=11)
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close(fig)


def draw_tile(path, values, extent_edges, xs, ys, dx, dy, polygons, tile, mode):
    bbox = index_window_to_world(tile, xs, ys, dx, dy)
    fig, ax = plt.subplots(figsize=(8.2, 5.2), dpi=240)
    draw_background(ax, values, extent_edges, mode)
    polygon_count = draw_polygons(ax, polygons, bbox, linewidth=0.78)
    ax.set_xlim(bbox[0], bbox[1])
    ax.set_ylim(bbox[2], bbox[3])
    ax.set_title(f"{tile['tile_id']} 断层多边形切分图：polygons={polygon_count}", fontsize=10)
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight", facecolor="white")
    plt.close(fig)


def draw_contact_sheet(tile_paths, output_path, columns=6):
    tile_paths = [Path(path) for path in tile_paths]
    rows = int(math.ceil(len(tile_paths) / max(1, columns)))
    fig, axes = plt.subplots(rows, columns, figsize=(columns * 2.15, rows * 1.45), dpi=220)
    axes = np.asarray(axes).reshape(rows, columns)
    for index, ax in enumerate(axes.flat):
        ax.set_axis_off()
        if index >= len(tile_paths):
            continue
        image = mpimg.imread(tile_paths[index])
        ax.imshow(image)
        ax.set_title(tile_paths[index].stem.replace("_tile", ""), fontsize=7, pad=2)
    fig.patch.set_facecolor("white")
    fig.tight_layout(pad=0.45)
    fig.savefig(output_path, bbox_inches="tight", facecolor="white")
    plt.close(fig)


def generate(dat_path, csv_path, out_dir, cols=6, rows=4):
    configure_chinese_font()
    values, xs, ys, dx, dy, extent_edges = read_geoeast_dat(dat_path)
    polygons = read_polygons(csv_path)
    tiles = build_tile_windows(width=len(xs), height=len(ys), cols=cols, rows=rows)
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    outputs = {}
    for mode, prefix in [("attribute", "attribute_overlay"), ("polygon", "polygon_only")]:
        mode_dir = out_dir / prefix
        tiles_dir = mode_dir / "tiles"
        mode_dir.mkdir(parents=True, exist_ok=True)
        tiles_dir.mkdir(parents=True, exist_ok=True)
        overview = mode_dir / "01_fault_polygons_overview_no_boxes.png"
        draw_overview(overview, values, extent_edges, polygons, mode)
        tile_paths = []
        for tile in tiles:
            tile_path = tiles_dir / f"{tile['tile_id']}_tile.png"
            draw_tile(tile_path, values, extent_edges, xs, ys, dx, dy, polygons, tile, mode)
            tile_paths.append(tile_path)
        sheet = mode_dir / "02_fault_polygons_tiles_contact_sheet_no_boxes.png"
        draw_contact_sheet(tile_paths, sheet, columns=cols)
        outputs[prefix] = {"overview": str(overview), "tiles_contact_sheet": str(sheet), "tiles_dir": str(tiles_dir)}

    summary = out_dir / "纯断层多边形图件说明.md"
    summary.write_text(
        "\n".join(
            [
                "# 纯断层多边形图件说明",
                "",
                "本目录只展示断层多边形结果，不包含候选框、黄色框、风险框、分块边框或其他复核标记。",
                "",
                "## 图件",
                "",
                f"- 属性背景叠加总览：`{outputs['attribute_overlay']['overview']}`",
                f"- 属性背景叠加切分拼图：`{outputs['attribute_overlay']['tiles_contact_sheet']}`",
                f"- 纯白底多边形总览：`{outputs['polygon_only']['overview']}`",
                f"- 纯白底多边形切分拼图：`{outputs['polygon_only']['tiles_contact_sheet']}`",
            ]
        )
        + "\n",
        encoding="utf-8",
    )
    return outputs


def main():
    parser = argparse.ArgumentParser(description="生成无候选框的纯断层多边形总览图和切分图")
    parser.add_argument("--dat", required=True)
    parser.add_argument("--csv", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--cols", type=int, default=6)
    parser.add_argument("--rows", type=int, default=4)
    args = parser.parse_args()
    outputs = generate(args.dat, args.csv, args.out_dir, cols=args.cols, rows=args.rows)
    for key, value in outputs.items():
        print(key)
        print(value["overview"])
        print(value["tiles_contact_sheet"])


if __name__ == "__main__":
    main()
