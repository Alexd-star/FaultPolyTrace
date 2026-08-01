# -*- coding: utf-8 -*-
"""Render high-resolution overview figures for the 200 and 214 results."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib.lines import Line2D
from matplotlib.patches import Patch
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap, Normalize
from matplotlib.ticker import FuncFormatter
import numpy as np
from shapely import contains_xy, prepare
from shapely.ops import unary_union

from render_polygon122_comparison import (
    REPO_ROOT,
    apply_axes,
    configure_chinese_font,
    draw_boundary,
    read_all_polygons,
    read_attribute_dat,
)


DEFAULT_DELIVERY = (
    REPO_ROOT / "outputs" / "evidence" / "geoeast_delivery_20260731_v12_and_214"
)
DEFAULT_RESULT_OUT = DEFAULT_DELIVERY / "result_overviews_highres"


def result_stats(polygons):
    area = sum(geometry.area for _, geometry in polygons)
    perimeter = sum(geometry.length for _, geometry in polygons)
    hole_count = sum(len(geometry.interiors) for _, geometry in polygons if geometry.geom_type == "Polygon")
    hole_area = 0.0
    for _, geometry in polygons:
        if geometry.geom_type != "Polygon":
            continue
        for ring in geometry.interiors:
            from shapely.geometry import Polygon

            hole_area += Polygon(ring).area
    return {
        "polygon_count": len(polygons),
        "net_area": float(area),
        "perimeter": float(perimeter),
        "hole_count": hole_count,
        "hole_area": float(hole_area),
    }


def format_coordinate(value, _position):
    return f"{value:,.0f}"


def style_full_map(ax, view, font_prop):
    apply_axes(ax, view, font_prop, tick_size=8)
    ax.xaxis.set_major_formatter(FuncFormatter(format_coordinate))
    ax.yaxis.set_major_formatter(FuncFormatter(format_coordinate))


def draw_result_boundaries(ax, polygons, color, white_halo=True, width=0.42):
    if white_halo:
        for _, geometry in polygons:
            draw_boundary(ax, geometry, "#ffffff", linewidth=width + 0.75, alpha=0.90, zorder=6)
    for _, geometry in polygons:
        draw_boundary(ax, geometry, color, linewidth=width, alpha=0.96, zorder=8)


def draw_attribute_high_contrast(ax, values, extent):
    """Render the valid attribute range without changing the underlying data."""

    import numpy as np

    masked = np.ma.masked_where(~np.isfinite(values) | (values <= 0), values)
    ax.set_facecolor("#ffffff")
    return ax.imshow(
        masked,
        extent=extent,
        origin="lower",
        cmap="viridis",
        norm=Normalize(vmin=0.30, vmax=1.00, clip=True),
        interpolation="nearest",
        aspect="equal",
        alpha=1.0,
    )


def add_attribute_colorbar(fig, ax, image, font_prop):
    colorbar = fig.colorbar(image, ax=ax, fraction=0.018, pad=0.018, shrink=0.78)
    colorbar.set_label("断层属性值（有效范围 0.30–1.00）", fontproperties=font_prop, fontsize=9)
    colorbar.ax.tick_params(labelsize=8, length=2)
    for label in colorbar.ax.get_yticklabels():
        label.set_fontproperties(font_prop)
        label.set_fontsize(8)


def add_stats_box(ax, stats, label, color, font_prop):
    text = (
        f"结果：{label}\n"
        f"闭合多边形：{stats['polygon_count']} 个\n"
        f"几何面积：{stats['net_area']:,.0f}\n"
        f"内环：{stats['hole_count']} 个"
    )
    ax.text(
        0.012,
        0.985,
        text,
        transform=ax.transAxes,
        va="top",
        ha="left",
        fontproperties=font_prop,
        fontsize=9,
        color=color,
        bbox={"boxstyle": "round,pad=0.35", "facecolor": "white", "edgecolor": color, "alpha": 0.92},
        zorder=20,
    )


def save_figure(fig, out_dir, stem, vector=True):
    fig.savefig(out_dir / f"{stem}.png", dpi=500, bbox_inches="tight", facecolor="white")
    if vector:
        fig.savefig(out_dir / f"{stem}.svg", bbox_inches="tight", facecolor="white")
        fig.savefig(out_dir / f"{stem}.pdf", bbox_inches="tight", facecolor="white")
    plt.close(fig)


def render_single_result(out_dir, values, extent, polygons, label, stem, color, font_prop):
    view = extent
    stats = result_stats(polygons)

    fig, ax = plt.subplots(figsize=(19, 9.2), dpi=220)
    ax.set_facecolor("white")
    draw_result_boundaries(ax, polygons, color, white_halo=False, width=0.48)
    style_full_map(ax, view, font_prop)
    ax.set_title(f"{label}：断层多边形纯边界总览", fontproperties=font_prop, fontsize=16, pad=12)
    add_stats_box(ax, stats, label, color, font_prop)
    save_figure(fig, out_dir, f"{stem}_pure_polygon_overview_highres")

    fig, ax = plt.subplots(figsize=(19, 9.2), dpi=220)
    attribute_image = draw_attribute_high_contrast(ax, values, extent)
    draw_result_boundaries(ax, polygons, color, white_halo=False, width=0.38)
    style_full_map(ax, view, font_prop)
    ax.set_title(f"{label}：叠加原始平面断层属性网格", fontproperties=font_prop, fontsize=16, pad=12)
    add_stats_box(ax, stats, label, color, font_prop)
    add_attribute_colorbar(fig, ax, attribute_image, font_prop)
    save_figure(fig, out_dir, f"{stem}_overlay_attribute_highres")
    return stats


def render_comparison(out_dir, values, extent, baseline, candidate, font_prop, baseline_stats, candidate_stats):
    view = extent
    fig, axes = plt.subplots(1, 2, figsize=(22, 9.5), dpi=220)
    baseline_image = draw_attribute_high_contrast(axes[0], values, extent)
    draw_result_boundaries(axes[0], baseline, "#1558a6", white_halo=False, width=0.36)
    style_full_map(axes[0], view, font_prop)
    axes[0].set_title("214 正式基线结果", fontproperties=font_prop, fontsize=15, pad=10)
    add_stats_box(axes[0], baseline_stats, "214 基线", "#1558a6", font_prop)

    candidate_image = draw_attribute_high_contrast(axes[1], values, extent)
    draw_result_boundaries(axes[1], candidate, "#d62728", white_halo=True, width=0.38)
    style_full_map(axes[1], view, font_prop)
    axes[1].set_title("v12 候选结果（200 个）", fontproperties=font_prop, fontsize=15, pad=10)
    add_stats_box(axes[1], candidate_stats, "v12 候选", "#d62728", font_prop)
    add_attribute_colorbar(fig, axes[1], candidate_image, font_prop)

    fig.suptitle(
        "214 正式基线与 v12 候选结果同坐标对比",
        fontproperties=font_prop,
        fontsize=17,
        color="#111827",
    )
    save_figure(fig, out_dir, "baseline_214_vs_v12_200_same_extent_comparison_highres")

    fig, ax = plt.subplots(figsize=(19, 9.2), dpi=220)
    attribute_image = draw_attribute_high_contrast(ax, values, extent)
    draw_result_boundaries(ax, baseline, "#1558a6", white_halo=False, width=0.34)
    draw_result_boundaries(ax, candidate, "#d62728", white_halo=True, width=0.38)
    style_full_map(ax, view, font_prop)
    ax.set_title("214 基线与 v12 候选边界叠加差异", fontproperties=font_prop, fontsize=16, pad=12)
    ax.legend(
        handles=[
            Line2D([0], [0], color="#1558a6", linewidth=1.6, label="214 正式基线（214 个）"),
            Line2D([0], [0], color="#d62728", linewidth=1.6, label="v12 候选（200 个）"),
        ],
        loc="lower right",
        prop=font_prop,
        framealpha=0.92,
    )
    add_attribute_colorbar(fig, ax, attribute_image, font_prop)
    save_figure(fig, out_dir, "baseline_214_v12_200_boundary_overlay_highres")


def render_attribute_only(out_dir, values, extent, font_prop):
    fig, ax = plt.subplots(figsize=(19, 9.2), dpi=220)
    attribute_image = draw_attribute_high_contrast(ax, values, extent)
    style_full_map(ax, extent, font_prop)
    ax.set_title("原始平面断层属性网格：增强色阶显示", fontproperties=font_prop, fontsize=16, pad=12)
    ax.text(
        0.012,
        0.985,
        "仅调整显示色阶，不改变原始属性值",
        transform=ax.transAxes,
        va="top",
        ha="left",
        fontproperties=font_prop,
        fontsize=9,
        color="#1f2937",
        bbox={"boxstyle": "round,pad=0.35", "facecolor": "white", "edgecolor": "#4f46e5", "alpha": 0.92},
    )
    add_attribute_colorbar(fig, ax, attribute_image, font_prop)
    save_figure(fig, out_dir, "original_attribute_grid_contrast_highres")


def render_improvement_difference(out_dir, values, extent, baseline, candidate, font_prop):
    baseline_union = unary_union([geometry for _, geometry in baseline])
    candidate_union = unary_union([geometry for _, geometry in candidate])
    common_geometry = baseline_union.intersection(candidate_union)
    baseline_only_geometry = baseline_union.difference(candidate_union)
    candidate_only_geometry = candidate_union.difference(baseline_union)
    union_area = baseline_union.union(candidate_union).area

    height, width = values.shape
    dx = (extent[1] - extent[0]) / width
    dy = (extent[3] - extent[2]) / height
    xs = np.linspace(extent[0] + 0.5 * dx, extent[1] - 0.5 * dx, width)
    ys = np.linspace(extent[2] + 0.5 * dy, extent[3] - 0.5 * dy, height)
    grid_x, grid_y = np.meshgrid(xs, ys)
    prepare(baseline_union)
    prepare(candidate_union)
    baseline_mask = contains_xy(baseline_union, grid_x, grid_y)
    candidate_mask = contains_xy(candidate_union, grid_x, grid_y)
    baseline_only_mask = baseline_mask & ~candidate_mask
    candidate_only_mask = candidate_mask & ~baseline_mask

    fig, ax = plt.subplots(figsize=(19, 9.2), dpi=220)
    attribute_image = draw_attribute_high_contrast(ax, values, extent)
    draw_result_boundaries(ax, baseline, "#9ca3af", white_halo=False, width=0.20)
    draw_result_boundaries(ax, candidate, "#6b7280", white_halo=False, width=0.20)

    baseline_layer = np.ma.masked_where(~baseline_only_mask, np.ones_like(values, dtype=float))
    candidate_layer = np.ma.masked_where(~candidate_only_mask, np.ones_like(values, dtype=float))
    ax.imshow(
        baseline_layer,
        extent=extent,
        origin="lower",
        cmap=ListedColormap(["#1f5fff"]),
        vmin=0.0,
        vmax=1.0,
        interpolation="nearest",
        aspect="equal",
        alpha=0.95,
        zorder=11,
    )
    ax.imshow(
        candidate_layer,
        extent=extent,
        origin="lower",
        cmap=ListedColormap(["#ef233c"]),
        vmin=0.0,
        vmax=1.0,
        interpolation="nearest",
        aspect="equal",
        alpha=0.95,
        zorder=12,
    )

    style_full_map(ax, extent, font_prop)
    ax.set_title(
        "214 正式基线与 v12 200 候选：覆盖改进差异叠加图",
        fontproperties=font_prop,
        fontsize=16,
        pad=12,
    )
    ax.legend(
        handles=[
            Patch(facecolor="#ef233c", edgecolor="#b91c1c", label="200 相对 214 新增覆盖"),
            Patch(facecolor="#1f5fff", edgecolor="#1e40af", label="214 独有覆盖"),
            Line2D([0], [0], color="#6b7280", linewidth=1.0, label="两套结果共同边界（灰色）"),
        ],
        loc="lower right",
        prop=font_prop,
        framealpha=0.94,
    )
    geometry_iou = common_geometry.area / union_area if union_area > 0 else 0.0
    summary = (
        f"200 新增几何面积：{candidate_only_geometry.area:,.0f}\n"
        f"214 独有几何面积：{baseline_only_geometry.area:,.0f}\n"
        f"共同几何面积：{common_geometry.area:,.0f}\n"
        f"几何覆盖 IoU：{geometry_iou * 100:.2f}%\n"
        f"着色网格：新增 {int(candidate_only_mask.sum()):,} 格，独有 {int(baseline_only_mask.sum()):,} 格"
    )
    ax.text(
        0.012,
        0.985,
        summary,
        transform=ax.transAxes,
        va="top",
        ha="left",
        fontproperties=font_prop,
        fontsize=9,
        color="#111827",
        bbox={"boxstyle": "round,pad=0.38", "facecolor": "white", "edgecolor": "#4b5563", "alpha": 0.94},
        zorder=20,
    )
    ax.text(
        0.012,
        0.015,
        "颜色按 10 m 网格中心归属显示；面积按原始矢量几何精确计算",
        transform=ax.transAxes,
        va="bottom",
        ha="left",
        fontproperties=font_prop,
        fontsize=8.5,
        color="#374151",
        bbox={"boxstyle": "round,pad=0.25", "facecolor": "white", "edgecolor": "#9ca3af", "alpha": 0.90},
        zorder=20,
    )
    add_attribute_colorbar(fig, ax, attribute_image, font_prop)
    save_figure(fig, out_dir, "baseline_214_v12_200_improvement_difference_overlay_highres")
    return {
        "candidate_added_vector_area": float(candidate_only_geometry.area),
        "baseline_only_vector_area": float(baseline_only_geometry.area),
        "common_vector_area": float(common_geometry.area),
        "geometry_iou": float(geometry_iou),
        "candidate_added_grid_cells": int(candidate_only_mask.sum()),
        "baseline_only_grid_cells": int(baseline_only_mask.sum()),
        "grid_cell_area": float(dx * dy),
    }


def main():
    parser = argparse.ArgumentParser(description="生成 200 与 214 结果的高清总览图")
    parser.add_argument("--dat", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_DELIVERY / "post_change_cli_baseline.csv")
    parser.add_argument("--candidate", type=Path, default=DEFAULT_DELIVERY / "post_change_cli_v12.csv")
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_RESULT_OUT)
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    font_prop = configure_chinese_font()
    values, extent = read_attribute_dat(args.dat)
    baseline = read_all_polygons(args.baseline)
    candidate = read_all_polygons(args.candidate)
    baseline_stats = render_single_result(
        args.out_dir, values, extent, baseline, "214 正式基线", "baseline_214", "#1558a6", font_prop
    )
    candidate_stats = render_single_result(
        args.out_dir, values, extent, candidate, "v12 候选（200 个）", "v12_200", "#d62728", font_prop
    )
    render_attribute_only(args.out_dir, values, extent, font_prop)
    render_comparison(args.out_dir, values, extent, baseline, candidate, font_prop, baseline_stats, candidate_stats)
    difference_stats = render_improvement_difference(
        args.out_dir, values, extent, baseline, candidate, font_prop
    )
    manifest = {
        "data": str(args.dat),
        "baseline": str(args.baseline),
        "candidate": str(args.candidate),
        "baseline_stats": baseline_stats,
        "candidate_stats": candidate_stats,
        "difference_stats": difference_stats,
        "outputs": sorted(path.name for path in args.out_dir.iterdir() if path.is_file()),
    }
    (args.out_dir / "result_overviews_manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(f"输出目录：{args.out_dir}")
    for path in sorted(args.out_dir.iterdir()):
        if path.is_file():
            print(path)


if __name__ == "__main__":
    main()
