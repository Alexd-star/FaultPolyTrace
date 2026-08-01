# -*- coding: utf-8 -*-
"""Render deterministic A/B review figures for two polygon result files."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib.lines import Line2D
import matplotlib.pyplot as plt
import numpy as np
from shapely.ops import unary_union

from render_polygon122_comparison import (
    apply_axes,
    configure_chinese_font,
    draw_attribute,
    draw_boundary,
    intersects_view,
    read_all_polygons,
    read_attribute_dat,
    shade_geometry,
)


BASELINE_COLOR = "#1558A6"
CANDIDATE_COLOR = "#D62728"
BASELINE_ONLY_COLOR = "#F59E0B"
CANDIDATE_ONLY_COLOR = "#00A6A6"
CONTEXT_COLOR = "#9CA3AF"


def polygon_map(polygons):
    return {polygon_id: geometry for polygon_id, geometry in polygons}


def changed_polygon_ids(baseline, candidate, tolerance=1e-8):
    baseline_by_id = polygon_map(baseline)
    candidate_by_id = polygon_map(candidate)
    changed = []
    for polygon_id in sorted(set(baseline_by_id) | set(candidate_by_id)):
        first = baseline_by_id.get(polygon_id)
        second = candidate_by_id.get(polygon_id)
        if first is None or second is None or not first.equals_exact(second, tolerance):
            changed.append(polygon_id)
    return changed


def draw_visible_boundaries(ax, polygons, view, color, width, alpha=0.95):
    for _, geometry in polygons:
        if intersects_view(geometry, view):
            draw_boundary(
                ax,
                geometry,
                color,
                linewidth=width,
                alpha=alpha,
                zorder=8,
            )


def style_axes(ax, view, font_prop, tick_size=7):
    apply_axes(ax, view, font_prop, tick_size=tick_size)
    ax.ticklabel_format(style="plain", axis="both", useOffset=False)


def add_legend(ax, font_prop, location="lower right"):
    ax.legend(
        handles=[
            Line2D([0], [0], color=BASELINE_COLOR, linewidth=1.5,
                   label="正式基线"),
            Line2D([0], [0], color=CANDIDATE_COLOR, linewidth=1.5,
                   label="修形候选"),
        ],
        loc=location,
        prop=font_prop,
        framealpha=0.94,
    )


def render_full_overview(
    values,
    extent,
    baseline,
    candidate,
    changed_ids,
    font_prop,
    output_path,
    dpi,
):
    fig, ax = plt.subplots(figsize=(20, 9.6), dpi=180)
    draw_attribute(ax, values, extent)
    draw_visible_boundaries(ax, baseline, extent, BASELINE_COLOR, 0.32, 0.80)
    draw_visible_boundaries(ax, candidate, extent, CANDIDATE_COLOR, 0.36, 0.92)
    style_axes(ax, extent, font_prop, tick_size=8)
    ax.set_title(
        "断层多边形自适应边界修形 A/B 总览",
        fontproperties=font_prop,
        fontsize=16,
        pad=12,
    )
    ax.text(
        0.012,
        0.985,
        f"基线 {len(baseline)} 个 · 候选 {len(candidate)} 个 · 变化对象 {len(changed_ids)} 个",
        transform=ax.transAxes,
        va="top",
        ha="left",
        fontproperties=font_prop,
        fontsize=9,
        color="#1F2937",
        bbox={
            "boxstyle": "round,pad=0.32",
            "facecolor": "white",
            "edgecolor": "#6B7280",
            "alpha": 0.94,
        },
        zorder=20,
    )
    add_legend(ax, font_prop)
    fig.savefig(output_path, dpi=dpi, bbox_inches="tight", facecolor="white")
    plt.close(fig)


def render_difference_overview(
    values,
    extent,
    baseline,
    candidate,
    font_prop,
    output_path,
    dpi,
):
    baseline_union = unary_union([geometry for _, geometry in baseline])
    candidate_union = unary_union([geometry for _, geometry in candidate])
    baseline_only = baseline_union.difference(candidate_union)
    candidate_only = candidate_union.difference(baseline_union)
    common = baseline_union.intersection(candidate_union)

    fig, ax = plt.subplots(figsize=(20, 9.6), dpi=180)
    draw_attribute(ax, values, extent)
    shade_geometry(
        ax, baseline_only, BASELINE_ONLY_COLOR, BASELINE_ONLY_COLOR,
        alpha=0.82, zorder=9,
    )
    shade_geometry(
        ax, candidate_only, CANDIDATE_ONLY_COLOR, CANDIDATE_ONLY_COLOR,
        alpha=0.88, zorder=10,
    )
    draw_visible_boundaries(ax, baseline, extent, BASELINE_COLOR, 0.22, 0.55)
    draw_visible_boundaries(ax, candidate, extent, CANDIDATE_COLOR, 0.24, 0.65)
    style_axes(ax, extent, font_prop, tick_size=8)
    union_area = baseline_union.union(candidate_union).area
    iou = common.area / union_area if union_area > 0.0 else 1.0
    ax.set_title(
        "自适应边界修形覆盖差异总览",
        fontproperties=font_prop,
        fontsize=16,
        pad=12,
    )
    ax.text(
        0.012,
        0.985,
        (
            f"候选新增面积 {candidate_only.area:,.0f} m²\n"
            f"候选收缩面积 {baseline_only.area:,.0f} m²\n"
            f"几何覆盖 IoU {iou:.4%}"
        ),
        transform=ax.transAxes,
        va="top",
        ha="left",
        fontproperties=font_prop,
        fontsize=9,
        color="#111827",
        bbox={
            "boxstyle": "round,pad=0.34",
            "facecolor": "white",
            "edgecolor": "#4B5563",
            "alpha": 0.95,
        },
        zorder=20,
    )
    ax.legend(
        handles=[
            Line2D([0], [0], color=BASELINE_ONLY_COLOR, linewidth=4,
                   label="基线独有（候选收缩）"),
            Line2D([0], [0], color=CANDIDATE_ONLY_COLOR, linewidth=4,
                   label="候选新增"),
        ],
        loc="lower right",
        prop=font_prop,
        framealpha=0.94,
    )
    fig.savefig(output_path, dpi=dpi, bbox_inches="tight", facecolor="white")
    plt.close(fig)
    return {
        "baseline_only_area": float(baseline_only.area),
        "candidate_only_area": float(candidate_only.area),
        "common_area": float(common.area),
        "geometry_iou": float(iou),
    }


def render_tiles(
    values,
    extent,
    baseline,
    candidate,
    font_prop,
    output_dir,
    columns,
    rows,
    dpi,
):
    output_dir.mkdir(parents=True, exist_ok=True)
    xmin, xmax, ymin, ymax = extent
    tile_width = (xmax - xmin) / columns
    tile_height = (ymax - ymin) / rows
    paths = []
    index = 1
    for row in range(rows - 1, -1, -1):
        for column in range(columns):
            view = (
                xmin + column * tile_width,
                xmin + (column + 1) * tile_width,
                ymin + row * tile_height,
                ymin + (row + 1) * tile_height,
            )
            fig, ax = plt.subplots(figsize=(12, 7.2), dpi=150)
            draw_attribute(ax, values, extent)
            draw_visible_boundaries(
                ax, baseline, view, BASELINE_COLOR, 0.52, 0.75)
            draw_visible_boundaries(
                ax, candidate, view, CANDIDATE_COLOR, 0.58, 0.92)
            style_axes(ax, view, font_prop, tick_size=7)
            ax.set_title(
                f"A/B 分块复核 {index:02d}（第 {rows - row} 行，第 {column + 1} 列）",
                fontproperties=font_prop,
                fontsize=12,
                pad=8,
            )
            add_legend(ax, font_prop)
            path = output_dir / f"分块_{index:02d}.png"
            fig.savefig(path, dpi=dpi, bbox_inches="tight", facecolor="white")
            plt.close(fig)
            paths.append(path)
            index += 1
    return paths


def expanded_view(first, second, extent):
    geometries = [geometry for geometry in (first, second) if geometry is not None]
    union = unary_union(geometries)
    min_x, min_y, max_x, max_y = union.bounds
    span_x = max(max_x - min_x, 1.0)
    span_y = max(max_y - min_y, 1.0)
    padding = max(40.0, 0.18 * max(span_x, span_y))
    return (
        max(extent[0], min_x - padding),
        min(extent[1], max_x + padding),
        max(extent[2], min_y - padding),
        min(extent[3], max_y + padding),
    )


def draw_local_context(ax, polygons, view, target_id):
    for polygon_id, geometry in polygons:
        if polygon_id != target_id and intersects_view(geometry, view):
            draw_boundary(
                ax, geometry, CONTEXT_COLOR,
                linewidth=0.32, alpha=0.38, zorder=3,
            )


def render_changed_objects(
    values,
    extent,
    baseline,
    candidate,
    changed_ids,
    font_prop,
    output_dir,
    dpi,
):
    output_dir.mkdir(parents=True, exist_ok=True)
    baseline_by_id = polygon_map(baseline)
    candidate_by_id = polygon_map(candidate)
    paths = []
    for ordinal, polygon_id in enumerate(changed_ids, start=1):
        first = baseline_by_id.get(polygon_id)
        second = candidate_by_id.get(polygon_id)
        view = expanded_view(first, second, extent)
        view_aspect = (view[1] - view[0]) / max(view[3] - view[2], 1e-9)
        panel_axis_height = 5.3 / max(view_aspect, 0.25)
        figure_height = min(7.0, max(3.0, panel_axis_height + 1.65))
        fig, axes = plt.subplots(
            1, 3, figsize=(18, figure_height), dpi=150,
            layout="constrained",
        )
        for ax in axes:
            draw_attribute(ax, values, extent)
            style_axes(ax, view, font_prop, tick_size=6.5)

        draw_local_context(axes[0], baseline, view, polygon_id)
        if first is not None:
            draw_boundary(
                axes[0], first, BASELINE_COLOR,
                linewidth=1.0, alpha=0.98, zorder=9,
            )
        axes[0].set_title("a  正式基线", fontproperties=font_prop, fontsize=11)

        draw_local_context(axes[1], candidate, view, polygon_id)
        if second is not None:
            draw_boundary(
                axes[1], second, CANDIDATE_COLOR,
                linewidth=1.0, alpha=0.98, zorder=9,
            )
        axes[1].set_title("b  修形候选", fontproperties=font_prop, fontsize=11)

        draw_local_context(axes[2], candidate, view, polygon_id)
        if first is not None and second is not None:
            shade_geometry(
                axes[2], first.difference(second),
                BASELINE_ONLY_COLOR, BASELINE_ONLY_COLOR,
                alpha=0.78, zorder=8,
            )
            shade_geometry(
                axes[2], second.difference(first),
                CANDIDATE_ONLY_COLOR, CANDIDATE_ONLY_COLOR,
                alpha=0.84, zorder=9,
            )
            draw_boundary(
                axes[2], first, BASELINE_COLOR,
                linewidth=0.42, alpha=0.60, zorder=10,
            )
            draw_boundary(
                axes[2], second, CANDIDATE_COLOR,
                linewidth=0.46, alpha=0.72, zorder=11,
            )
        axes[2].set_title(
            "c  覆盖差异（橙色收缩，青色新增）",
            fontproperties=font_prop,
            fontsize=11,
        )
        fig.suptitle(
            f"变化对象 {ordinal:02d} · Polygon {polygon_id}",
            fontproperties=font_prop,
            fontsize=13,
        )
        path = output_dir / f"变化_{ordinal:02d}_polygon_{polygon_id}.png"
        fig.savefig(path, dpi=dpi, bbox_inches="tight", facecolor="white")
        plt.close(fig)
        paths.append(path)
    return paths


def main():
    parser = argparse.ArgumentParser(
        description="生成断层多边形候选相对正式基线的高清 A/B 复核包"
    )
    parser.add_argument("--dat", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--tile-columns", type=int, default=6)
    parser.add_argument("--tile-rows", type=int, default=4)
    parser.add_argument("--dpi", type=int, default=300)
    args = parser.parse_args()
    if args.tile_columns < 1 or args.tile_rows < 1 or args.dpi < 100:
        parser.error("分块数量必须为正数，DPI 不得低于 100。")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    font_prop = configure_chinese_font()
    values, extent = read_attribute_dat(args.dat)
    baseline = read_all_polygons(args.baseline)
    candidate = read_all_polygons(args.candidate)
    changed_ids = changed_polygon_ids(baseline, candidate)

    overview_path = args.out_dir / "01_自适应边界修形_AB总览.png"
    difference_path = args.out_dir / "02_自适应边界修形_覆盖差异总览.png"
    render_full_overview(
        values, extent, baseline, candidate, changed_ids,
        font_prop, overview_path, args.dpi,
    )
    difference = render_difference_overview(
        values, extent, baseline, candidate,
        font_prop, difference_path, args.dpi,
    )
    tile_paths = render_tiles(
        values, extent, baseline, candidate, font_prop,
        args.out_dir / "03_全图24分块复核",
        args.tile_columns, args.tile_rows, args.dpi,
    )
    local_paths = render_changed_objects(
        values, extent, baseline, candidate, changed_ids, font_prop,
        args.out_dir / "04_全部变化对象局部复核", args.dpi,
    )

    manifest = {
        "data": str(args.dat.resolve()),
        "baseline": str(args.baseline.resolve()),
        "candidate": str(args.candidate.resolve()),
        "baseline_polygon_count": len(baseline),
        "candidate_polygon_count": len(candidate),
        "changed_polygon_count": len(changed_ids),
        "changed_polygon_ids": changed_ids,
        "tile_count": len(tile_paths),
        "local_figure_count": len(local_paths),
        "difference_metrics": difference,
        "outputs": {
            "overview": str(overview_path.resolve()),
            "difference_overview": str(difference_path.resolve()),
            "tiles": [str(path.resolve()) for path in tile_paths],
            "changed_objects": [str(path.resolve()) for path in local_paths],
        },
    }
    manifest_path = args.out_dir / "复核包清单.json"
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    print(manifest_path)


if __name__ == "__main__":
    main()
