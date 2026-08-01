# -*- coding: utf-8 -*-
"""Render the three final, high-resolution PNG figures for submission."""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib import patheffects
from matplotlib.colors import Normalize
from matplotlib.lines import Line2D
from matplotlib.ticker import FuncFormatter
import matplotlib.pyplot as plt
import numpy as np

from render_polygon122_comparison import (
    configure_chinese_font,
    read_all_polygons,
    read_attribute_dat,
)


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RESULT = (
    REPO_ROOT
    / "packaging"
    / "submission"
    / "04_复现与验证"
    / "正式官方结果"
    / "FaultPolyTrace_Official_Result.csv"
)
DEFAULT_OUT = REPO_ROOT / "outputs" / "evidence" / "final_submission_pngs"

ATTRIBUTE_TITLE = "平面断层属性数据"
OVERLAY_TITLE = "断层多边形自动追踪结果（属性叠加）"
BOUNDARY_TITLE = "断层多边形自动追踪结果（矢量边界）"
BOUNDARY_COLOR = "#c62828"


def iter_polygons(geometry):
    if geometry.geom_type == "Polygon":
        yield geometry
    elif geometry.geom_type == "MultiPolygon":
        yield from geometry.geoms


def format_coordinate(value, _position):
    return f"{value:,.0f}"


def style_axes(ax, extent, font_prop):
    ax.set_xlim(extent[0], extent[1])
    ax.set_ylim(extent[2], extent[3])
    ax.set_aspect("equal", adjustable="box")
    ax.set_xlabel("X 坐标", fontproperties=font_prop, fontsize=11)
    ax.set_ylabel("Y 坐标", fontproperties=font_prop, fontsize=11)
    ax.xaxis.set_major_formatter(FuncFormatter(format_coordinate))
    ax.yaxis.set_major_formatter(FuncFormatter(format_coordinate))
    ax.tick_params(axis="both", which="major", labelsize=9.5, length=3.2,
                   width=0.7, colors="#374151")
    for label in ax.get_xticklabels() + ax.get_yticklabels():
        label.set_fontproperties(font_prop)
    for spine in ax.spines.values():
        spine.set_color("#6b7280")
        spine.set_linewidth(0.8)
    ax.grid(False)


def draw_attribute(ax, values, extent):
    masked = np.ma.masked_where(~np.isfinite(values) | (values <= 0.0), values)
    ax.set_facecolor("#ffffff")
    return ax.imshow(
        masked,
        extent=extent,
        origin="lower",
        cmap="viridis_r",
        norm=Normalize(vmin=0.30, vmax=1.00, clip=True),
        interpolation="nearest",
        aspect="equal",
        alpha=1.0,
        rasterized=True,
        zorder=1,
    )


def draw_boundaries(ax, polygons, linewidth, halo):
    effects = None
    if halo:
        effects = [
            patheffects.Stroke(linewidth=linewidth + 1.25, foreground="white", alpha=0.96),
            patheffects.Normal(),
        ]
    for _, geometry in polygons:
        for polygon in iter_polygons(geometry):
            rings = [polygon.exterior, *polygon.interiors]
            for ring in rings:
                coordinates = np.asarray(ring.coords)
                (line,) = ax.plot(
                    coordinates[:, 0],
                    coordinates[:, 1],
                    color=BOUNDARY_COLOR,
                    linewidth=linewidth,
                    solid_capstyle="round",
                    solid_joinstyle="round",
                    antialiased=True,
                    alpha=0.98,
                    zorder=5,
                )
                if effects:
                    line.set_path_effects(effects)


def add_colorbar(fig, ax, image, font_prop):
    colorbar = fig.colorbar(image, ax=ax, fraction=0.018, pad=0.018, shrink=0.82)
    colorbar.set_label("断层属性值", fontproperties=font_prop, fontsize=10)
    colorbar.ax.tick_params(labelsize=9, length=2.5, width=0.7)
    for label in colorbar.ax.get_yticklabels():
        label.set_fontproperties(font_prop)
    colorbar.outline.set_linewidth(0.7)
    colorbar.outline.set_edgecolor("#6b7280")


def add_boundary_legend(ax, font_prop):
    legend = ax.legend(
        handles=[
            Line2D([0], [0], color=BOUNDARY_COLOR, linewidth=1.8,
                   label="自动追踪多边形边界")
        ],
        loc="lower right",
        prop=font_prop,
        fontsize=9.5,
        frameon=True,
        framealpha=0.94,
        facecolor="white",
        edgecolor="#9ca3af",
        borderpad=0.55,
        handlelength=2.8,
    )
    legend.get_frame().set_linewidth(0.7)


def save_png(fig, path, dpi):
    fig.savefig(
        path,
        format="png",
        dpi=dpi,
        bbox_inches="tight",
        pad_inches=0.08,
        facecolor="white",
        edgecolor="none",
        pil_kwargs={"compress_level": 6},
    )
    plt.close(fig)


def render_attribute(values, extent, font_prop, out_dir, dpi):
    fig, ax = plt.subplots(figsize=(20.0, 9.35), dpi=160)
    image = draw_attribute(ax, values, extent)
    style_axes(ax, extent, font_prop)
    ax.set_title(ATTRIBUTE_TITLE, fontproperties=font_prop, fontsize=18, pad=13,
                 color="#111827")
    add_colorbar(fig, ax, image, font_prop)
    save_png(fig, out_dir / "原始属性图.png", dpi)


def render_overlay(values, extent, polygons, font_prop, out_dir, dpi):
    fig, ax = plt.subplots(figsize=(20.0, 9.35), dpi=160)
    image = draw_attribute(ax, values, extent)
    draw_boundaries(ax, polygons, linewidth=0.72, halo=True)
    style_axes(ax, extent, font_prop)
    ax.set_title(OVERLAY_TITLE, fontproperties=font_prop, fontsize=18, pad=13,
                 color="#111827")
    add_boundary_legend(ax, font_prop)
    add_colorbar(fig, ax, image, font_prop)
    save_png(fig, out_dir / "最佳结果叠加图.png", dpi)


def render_boundaries(extent, polygons, font_prop, out_dir, dpi):
    fig, ax = plt.subplots(figsize=(20.0, 9.35), dpi=160)
    ax.set_facecolor("white")
    draw_boundaries(ax, polygons, linewidth=0.88, halo=False)
    style_axes(ax, extent, font_prop)
    ax.set_title(BOUNDARY_TITLE, fontproperties=font_prop, fontsize=18, pad=13,
                 color="#111827")
    add_boundary_legend(ax, font_prop)
    save_png(fig, out_dir / "最佳结果纯边界图.png", dpi)


def main():
    parser = argparse.ArgumentParser(description="生成正式提交用三张高清 PNG")
    parser.add_argument(
        "--dat",
        type=Path,
        required=True,
        help="输入平面断层属性 DAT 文件",
    )
    parser.add_argument("--result", type=Path, default=DEFAULT_RESULT)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--dpi", type=int, default=560)
    args = parser.parse_args()
    if args.dpi < 300 or args.dpi > 800:
        raise ValueError("DPI 必须位于 300 至 800 之间。")
    args.out_dir.mkdir(parents=True, exist_ok=True)
    font_prop = configure_chinese_font()
    values, extent = read_attribute_dat(args.dat)
    polygons = read_all_polygons(args.result)
    if len(polygons) != 200:
        raise RuntimeError(f"正式结果应包含 200 个保留多边形，当前读取到 {len(polygons)} 个。")
    render_attribute(values, extent, font_prop, args.out_dir, args.dpi)
    render_overlay(values, extent, polygons, font_prop, args.out_dir, args.dpi)
    render_boundaries(extent, polygons, font_prop, args.out_dir, args.dpi)
    for name in ("原始属性图.png", "最佳结果叠加图.png", "最佳结果纯边界图.png"):
        print(args.out_dir / name)


if __name__ == "__main__":
    main()
