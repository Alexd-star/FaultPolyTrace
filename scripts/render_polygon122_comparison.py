# -*- coding: utf-8 -*-
"""Render a Chinese, high-resolution comparison for v12 polygon 122.

The figure compares the spatially corresponding object in the 214 baseline
(polygon 134) and the v12 candidate (polygon 122).  It is intentionally a
review figure rather than a scoring figure: the raw attribute grid remains
visible, both runs use the same world-coordinate window, and the small added
connection that changes the topology is highlighted separately.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib import font_manager
from matplotlib.lines import Line2D
from matplotlib.patches import Polygon as PolygonPatch
import matplotlib.pyplot as plt
import numpy as np
from shapely import wkt
from shapely.geometry import Polygon


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RUN = (
    REPO_ROOT
    / "outputs"
    / "evidence"
    / "weak_continuity_candidate_20260731_v12_tail_strict_final"
)
DEFAULT_OUT = (
    REPO_ROOT
    / "outputs"
    / "evidence"
    / "geoeast_delivery_20260731_v12_and_214"
    / "polygon_122_review_cn"
)


def configure_chinese_font():
    """Use an explicit installed CJK font for every text artist."""

    candidates = [
        r"C:\Windows\Fonts\msyh.ttc",
        r"C:\Windows\Fonts\simhei.ttf",
        r"C:\Windows\Fonts\simsun.ttc",
        r"C:\Windows\Fonts\NotoSansSC-VF.ttf",
    ]
    for path in candidates:
        if os.path.exists(path):
            font_manager.fontManager.addfont(path)
            prop = font_manager.FontProperties(fname=path)
            name = prop.get_name()
            plt.rcParams["font.family"] = name
            plt.rcParams["font.sans-serif"] = [name]
            plt.rcParams["axes.unicode_minus"] = False
            return prop
    prop = font_manager.FontProperties(family="DejaVu Sans")
    plt.rcParams["axes.unicode_minus"] = False
    return prop


def read_attribute_dat(path: Path):
    rows = []
    with path.open("r", encoding="utf-8", errors="ignore") as stream:
        for line in stream:
            text = line.strip()
            if not text or text.startswith("#"):
                continue
            fields = text.split()
            if len(fields) < 5:
                continue
            rows.append(
                (
                    int(float(fields[0])),
                    int(float(fields[1])),
                    float(fields[2]),
                    float(fields[3]),
                    float(fields[4]),
                )
            )
    if not rows:
        raise RuntimeError(f"未读取到有效属性网格：{path}")

    arr = np.asarray(rows, dtype=float)
    line_keys = np.unique(arr[:, 0]).astype(int)
    cmp_keys = np.unique(arr[:, 1]).astype(int)
    line_index = {value: index for index, value in enumerate(line_keys)}
    cmp_index = {value: index for index, value in enumerate(cmp_keys)}
    values = np.full((len(cmp_keys), len(line_keys)), np.nan, dtype=float)
    x_by_line = {}
    y_by_cmp = {}
    for line_no, cmp_no, x, y, value in rows:
        values[cmp_index[int(cmp_no)], line_index[int(line_no)]] = value
        x_by_line[int(line_no)] = x
        y_by_cmp[int(cmp_no)] = y

    xs = np.asarray([x_by_line[int(key)] for key in line_keys], dtype=float)
    ys = np.asarray([y_by_cmp[int(key)] for key in cmp_keys], dtype=float)
    dx = abs(xs[1] - xs[0]) if len(xs) > 1 else 1.0
    dy = abs(ys[1] - ys[0]) if len(ys) > 1 else 1.0
    extent = (
        float(xs.min() - 0.5 * dx),
        float(xs.max() + 0.5 * dx),
        float(ys.min() - 0.5 * dy),
        float(ys.max() + 0.5 * dy),
    )
    return values, extent


def read_polygon(csv_path: Path, polygon_id: int):
    csv.field_size_limit(2_147_483_647)
    with csv_path.open("r", encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            if row.get("status", "").lower() != "kept":
                continue
            if int(row["id"]) == polygon_id:
                return wkt.loads(row["wkt"]), row
    raise RuntimeError(f"结果文件中未找到保留多边形：{csv_path} / {polygon_id}")


def read_all_polygons(csv_path: Path):
    csv.field_size_limit(2_147_483_647)
    result = []
    with csv_path.open("r", encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            if row.get("status", "").lower() != "kept":
                continue
            result.append((int(row["id"]), wkt.loads(row["wkt"])))
    return result


def iter_polygons(geometry):
    if geometry.geom_type == "Polygon":
        yield geometry
    elif geometry.geom_type == "MultiPolygon":
        yield from geometry.geoms


def bbox(geometry):
    return geometry.bounds


def intersects_view(geometry, view):
    xmin, xmax, ymin, ymax = view
    gxmin, gymin, gxmax, gymax = geometry.bounds
    return not (gxmax < xmin or gxmin > xmax or gymax < ymin or gymin > ymax)


def draw_attribute(ax, values, extent):
    masked = np.ma.masked_where(~np.isfinite(values) | (values <= 0), values)
    ax.set_facecolor("#fbfbfb")
    ax.imshow(
        masked,
        extent=extent,
        origin="lower",
        cmap="magma",
        vmin=0.0,
        vmax=1.0,
        interpolation="nearest",
        aspect="equal",
        alpha=0.93,
    )


def draw_boundary(ax, geometry, color, linewidth=1.2, linestyle="-", alpha=1.0, zorder=5):
    for polygon in iter_polygons(geometry):
        outer = np.asarray(polygon.exterior.coords)
        ax.plot(
            outer[:, 0],
            outer[:, 1],
            color=color,
            linewidth=linewidth,
            linestyle=linestyle,
            alpha=alpha,
            zorder=zorder,
        )
        for ring in polygon.interiors:
            points = np.asarray(ring.coords)
            ax.plot(
                points[:, 0],
                points[:, 1],
                color=color,
                linewidth=max(0.55, linewidth * 0.76),
                linestyle="--",
                alpha=alpha,
                zorder=zorder + 1,
            )


def draw_context(ax, all_polygons, target, view, color="#9aa3ad", linewidth=0.34):
    for _, geometry in all_polygons:
        if geometry.equals(target) or not intersects_view(geometry, view):
            continue
        draw_boundary(ax, geometry, color=color, linewidth=linewidth, alpha=0.45, zorder=3)


def shade_geometry(ax, geometry, facecolor, edgecolor, alpha=0.18, hatch=None, zorder=2):
    for polygon in iter_polygons(geometry):
        outer = np.asarray(polygon.exterior.coords)
        ax.add_patch(
            PolygonPatch(
                outer,
                closed=True,
                facecolor=facecolor,
                edgecolor=edgecolor,
                linewidth=0.55,
                alpha=alpha,
                hatch=hatch,
                zorder=zorder,
            )
        )


def apply_axes(ax, view, font_prop, tick_size=8):
    ax.set_xlim(view[0], view[1])
    ax.set_ylim(view[2], view[3])
    ax.set_aspect("equal", adjustable="box")
    ax.set_xlabel("X 坐标", fontproperties=font_prop, fontsize=9)
    ax.set_ylabel("Y 坐标", fontproperties=font_prop, fontsize=9)
    ax.tick_params(labelsize=tick_size, length=3, colors="#404040")
    for label in ax.get_xticklabels() + ax.get_yticklabels():
        label.set_fontproperties(font_prop)
        label.set_fontsize(tick_size)
    for spine in ax.spines.values():
        spine.set_color("#555555")
        spine.set_linewidth(0.7)


def add_title(ax, text, font_prop, fontsize=11):
    ax.set_title(text, fontproperties=font_prop, fontsize=fontsize, pad=8, color="#1f2933")


def add_annotation(ax, xy, text, xytext, font_prop, color="#9c2f2f", fontsize=9):
    ax.annotate(
        text,
        xy=xy,
        xytext=xytext,
        textcoords="data",
        fontproperties=font_prop,
        fontsize=fontsize,
        color=color,
        arrowprops={"arrowstyle": "->", "color": color, "linewidth": 1.0},
        bbox={"boxstyle": "round,pad=0.28", "facecolor": "white", "edgecolor": color, "alpha": 0.92},
        zorder=20,
    )


def metrics(v12, baseline):
    hole_area = sum(Polygon(ring).area for ring in v12.interiors)
    outer_area = sum(Polygon(polygon.exterior).area for polygon in iter_polygons(v12))
    added = v12.difference(baseline)
    overlap = v12.intersection(baseline).area
    union = v12.union(baseline).area
    return {
        "v12_id": 122,
        "baseline_id": 134,
        "v12_area": float(v12.area),
        "v12_outer_envelope_area": float(outer_area),
        "baseline_area": float(baseline.area),
        "v12_hole_count": len(v12.interiors),
        "v12_hole_area": float(hole_area),
        "v12_hole_ratio_percent": float(hole_area / outer_area * 100.0),
        "added_connection_area": float(added.area),
        "intersection_area": float(overlap),
        "iou": float(overlap / union),
        "added_connection_bounds": [float(value) for value in added.bounds],
        "added_connection_centroid": [float(added.centroid.x), float(added.centroid.y)],
    }


def render_overview(out_dir, values, extent, v12, baseline, all_v12, all_baseline, font_prop, stats):
    xmin, ymin, xmax, ymax = v12.bounds
    padding_x = 0.035 * (xmax - xmin)
    padding_y = 0.08 * (ymax - ymin)
    view = (xmin - padding_x, xmax + padding_x, ymin - padding_y, ymax + padding_y)

    fig, axes = plt.subplots(1, 2, figsize=(20, 8.2), dpi=220, constrained_layout=True)
    for ax in axes:
        draw_attribute(ax, values, extent)
        ax.set_facecolor("white")
        draw_context(ax, all_baseline if ax is axes[0] else all_v12, baseline if ax is axes[0] else v12, view)

    draw_boundary(axes[0], baseline, "#1558a6", linewidth=1.65)
    add_title(axes[0], "214 基线：polygon 134（无内环）", font_prop)
    draw_boundary(axes[1], v12, "#d97706", linewidth=1.65)
    hole = Polygon(v12.interiors[0]) if len(v12.interiors) else None
    if hole is not None:
        shade_geometry(axes[1], hole, "#ffffff", "#7c3aed", alpha=0.50, hatch="///", zorder=4)
        draw_boundary(axes[1], hole, "#7c3aed", linewidth=1.25, linestyle="--", zorder=8)
    add_title(axes[1], "v12 候选：polygon 122（含一个大内环）", font_prop)

    for ax in axes:
        apply_axes(ax, view, font_prop)
    axes[1].text(
        view[0] + 0.02 * (view[1] - view[0]),
        view[3] - 0.08 * (view[3] - view[2]),
        f"外环包络面积：{stats['v12_outer_envelope_area']:,.0f}\n"
        f"几何面积（扣除内环）：{stats['v12_area']:,.0f}\n"
        f"内环面积：{stats['v12_hole_area']:,.0f}\n"
        f"内环占外环包络：{stats['v12_hole_ratio_percent']:.1f}%",
        fontproperties=font_prop,
        fontsize=9,
        color="#5b21b6",
        bbox={"boxstyle": "round,pad=0.35", "facecolor": "white", "edgecolor": "#7c3aed", "alpha": 0.90},
        zorder=20,
    )
    fig.suptitle(
        "polygon 122 与 214 基线对应对象对比（同坐标、同尺度）",
        fontproperties=font_prop,
        fontsize=16,
        color="#111827",
    )
    fig.savefig(out_dir / "polygon122_vs_214_overview_cn.png", dpi=600, bbox_inches="tight", facecolor="white")
    fig.savefig(out_dir / "polygon122_vs_214_overview_cn.svg", bbox_inches="tight", facecolor="white")
    fig.savefig(out_dir / "polygon122_vs_214_overview_cn.pdf", bbox_inches="tight", facecolor="white")
    plt.close(fig)


def render_comparison(out_dir, values, extent, v12, baseline, all_v12, all_baseline, font_prop, stats):
    xmin, ymin, xmax, ymax = v12.bounds
    padding_x = 0.035 * (xmax - xmin)
    padding_y = 0.08 * (ymax - ymin)
    view = (xmin - padding_x, xmax + padding_x, ymin - padding_y, ymax + padding_y)
    added = v12.difference(baseline)
    hole = Polygon(v12.interiors[0]) if len(v12.interiors) else None

    fig, axes = plt.subplots(2, 2, figsize=(18, 14), dpi=220, constrained_layout=True)
    axes = axes.ravel()

    draw_attribute(axes[0], values, extent)
    draw_context(axes[0], all_baseline, baseline, view)
    draw_boundary(axes[0], baseline, "#1558a6", linewidth=1.75)
    add_title(axes[0], "（a）214 基线 polygon 134", font_prop)
    axes[0].text(
        view[0] + 0.02 * (view[1] - view[0]),
        view[3] - 0.08 * (view[3] - view[2]),
        f"面积：{stats['baseline_area']:,.0f}\n内环：无",
        fontproperties=font_prop,
        fontsize=9,
        color="#1558a6",
        bbox={"boxstyle": "round,pad=0.28", "facecolor": "white", "edgecolor": "#1558a6", "alpha": 0.9},
    )

    draw_attribute(axes[1], values, extent)
    draw_context(axes[1], all_v12, v12, view)
    draw_boundary(axes[1], v12, "#d97706", linewidth=1.75)
    if hole is not None:
        shade_geometry(axes[1], hole, "#ffffff", "#7c3aed", alpha=0.52, hatch="///", zorder=4)
        draw_boundary(axes[1], hole, "#7c3aed", linewidth=1.30, linestyle="--", zorder=8)
    add_title(axes[1], "（b）v12 候选 polygon 122", font_prop)
    axes[1].text(
        view[0] + 0.02 * (view[1] - view[0]),
        view[3] - 0.08 * (view[3] - view[2]),
        f"几何面积：{stats['v12_area']:,.0f}\n"
        f"内环：{stats['v12_hole_area']:,.0f}\n"
        f"内环占外环包络：{stats['v12_hole_ratio_percent']:.1f}%",
        fontproperties=font_prop,
        fontsize=9,
        color="#7c3aed",
        bbox={"boxstyle": "round,pad=0.28", "facecolor": "white", "edgecolor": "#7c3aed", "alpha": 0.9},
    )

    draw_attribute(axes[2], values, extent)
    draw_boundary(axes[2], baseline, "#1558a6", linewidth=1.05, alpha=0.95)
    draw_boundary(axes[2], v12, "#d97706", linewidth=1.10, alpha=0.95)
    shade_geometry(axes[2], added, "#dc2626", "#b91c1c", alpha=0.42, hatch="\\\\", zorder=6)
    if hole is not None:
        draw_boundary(axes[2], hole, "#7c3aed", linewidth=1.05, linestyle="--", zorder=8)
    add_title(axes[2], "（c）同坐标叠加：新增连接与内环位置", font_prop)
    add_annotation(
        axes[2],
        (stats["added_connection_centroid"][0], stats["added_connection_centroid"][1]),
        f"v12 新增局部连接\n面积 {stats['added_connection_area']:,.0f}",
        (stats["added_connection_centroid"][0] + 440, stats["added_connection_centroid"][1] - 120),
        font_prop,
    )
    axes[2].legend(
        handles=[
            Line2D([0], [0], color="#1558a6", linewidth=1.6, label="214 基线边界"),
            Line2D([0], [0], color="#d97706", linewidth=1.6, label="v12 外环边界"),
            Line2D([0], [0], color="#7c3aed", linewidth=1.3, linestyle="--", label="v12 内环边界"),
            Line2D([0], [0], color="#dc2626", linewidth=6, alpha=0.42, label="v12 新增局部连接"),
        ],
        loc="lower right",
        prop=font_prop,
        framealpha=0.92,
    )

    bridge_xmin, bridge_ymin, bridge_xmax, bridge_ymax = added.bounds
    bridge_pad_x = 0.75 * max(bridge_xmax - bridge_xmin, 100.0)
    bridge_pad_y = 0.85 * max(bridge_ymax - bridge_ymin, 100.0)
    bridge_view = (bridge_xmin - bridge_pad_x, bridge_xmax + bridge_pad_x, bridge_ymin - bridge_pad_y, bridge_ymax + bridge_pad_y)
    draw_attribute(axes[3], values, extent)
    draw_boundary(axes[3], baseline, "#1558a6", linewidth=1.35)
    draw_boundary(axes[3], v12, "#d97706", linewidth=1.35)
    shade_geometry(axes[3], added, "#dc2626", "#b91c1c", alpha=0.48, hatch="\\\\", zorder=6)
    if hole is not None:
        draw_boundary(axes[3], hole, "#7c3aed", linewidth=1.15, linestyle="--", zorder=8)
    add_title(axes[3], "（d）新增连接局部放大", font_prop)
    add_annotation(
        axes[3],
        (stats["added_connection_centroid"][0], stats["added_connection_centroid"][1]),
        "这段连接使边界闭合\n并形成大内环",
        (bridge_view[0] + 0.06 * (bridge_view[1] - bridge_view[0]), bridge_view[3] - 0.17 * (bridge_view[3] - bridge_view[2])),
        font_prop,
    )

    for index, ax in enumerate(axes):
        apply_axes(ax, bridge_view if index == 3 else view, font_prop)
    fig.suptitle(
        "polygon 122 与 214 基线的局部质量对比",
        fontproperties=font_prop,
        fontsize=17,
        color="#111827",
    )
    fig.savefig(out_dir / "polygon122_vs_214_comparison_cn.png", dpi=600, bbox_inches="tight", facecolor="white")
    fig.savefig(out_dir / "polygon122_vs_214_comparison_cn.svg", bbox_inches="tight", facecolor="white")
    fig.savefig(out_dir / "polygon122_vs_214_comparison_cn.pdf", bbox_inches="tight", facecolor="white")
    plt.close(fig)


def render_hole_detail(out_dir, values, extent, v12, baseline, font_prop, stats):
    xmin, ymin, xmax, ymax = v12.bounds
    view = (xmin - 220, xmax + 220, ymin - 180, ymax + 180)
    hole = Polygon(v12.interiors[0]) if len(v12.interiors) else None

    fig, ax = plt.subplots(figsize=(17, 8.8), dpi=240, constrained_layout=True)
    draw_attribute(ax, values, extent)
    draw_boundary(ax, baseline, "#1558a6", linewidth=1.25)
    draw_boundary(ax, v12, "#d97706", linewidth=1.55)
    if hole is not None:
        shade_geometry(ax, hole, "#ffffff", "#7c3aed", alpha=0.58, hatch="///", zorder=4)
        draw_boundary(ax, hole, "#7c3aed", linewidth=1.35, linestyle="--", zorder=8)
        add_annotation(
            ax,
            (hole.centroid.x, hole.centroid.y),
            f"大内环\n面积 {stats['v12_hole_area']:,.0f}\n"
            f"占外环包络 {stats['v12_hole_ratio_percent']:.1f}%",
            (hole.centroid.x - 1900, hole.centroid.y + 670),
            font_prop,
            color="#5b21b6",
            fontsize=10,
        )
    add_annotation(
        ax,
        (stats["added_connection_centroid"][0], stats["added_connection_centroid"][1]),
        f"新增连接\n面积 {stats['added_connection_area']:,.0f}",
        (stats["added_connection_centroid"][0] - 1450, stats["added_connection_centroid"][1] - 520),
        font_prop,
        color="#991b1b",
        fontsize=10,
    )
    apply_axes(ax, view, font_prop, tick_size=9)
    ax.legend(
        handles=[
            Line2D([0], [0], color="#1558a6", linewidth=1.6, label="214 基线 polygon 134"),
            Line2D([0], [0], color="#d97706", linewidth=1.6, label="v12 外环 polygon 122"),
            Line2D([0], [0], color="#7c3aed", linewidth=1.35, linestyle="--", label="v12 内环"),
        ],
        loc="upper right",
        prop=font_prop,
        framealpha=0.92,
    )
    ax.set_title("polygon 122 内环与 214 基线边界复核图", fontproperties=font_prop, fontsize=15, pad=10)
    fig.savefig(out_dir / "v12_polygon_122_hole_detail_cn.png", dpi=600, bbox_inches="tight", facecolor="white")
    fig.savefig(out_dir / "v12_polygon_122_hole_detail_cn.svg", bbox_inches="tight", facecolor="white")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description="生成 polygon 122 与 214 基线的中文高清对比图")
    parser.add_argument("--dat", type=Path, required=True)
    parser.add_argument("--run-dir", type=Path, default=DEFAULT_RUN)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    font_prop = configure_chinese_font()
    values, extent = read_attribute_dat(args.dat)
    v12, _ = read_polygon(args.run_dir / "weak_continuity_candidate.csv", 122)
    baseline, _ = read_polygon(args.run_dir / "baseline_214.csv", 134)
    all_v12 = read_all_polygons(args.run_dir / "weak_continuity_candidate.csv")
    all_baseline = read_all_polygons(args.run_dir / "baseline_214.csv")
    stats = metrics(v12, baseline)

    render_overview(args.out_dir, values, extent, v12, baseline, all_v12, all_baseline, font_prop, stats)
    render_comparison(args.out_dir, values, extent, v12, baseline, all_v12, all_baseline, font_prop, stats)
    render_hole_detail(args.out_dir, values, extent, v12, baseline, font_prop, stats)
    (args.out_dir / "polygon122_vs_214_metrics.json").write_text(
        json.dumps(stats, ensure_ascii=False, indent=2), encoding="utf-8"
    )

    print(f"输出目录：{args.out_dir}")
    for path in sorted(args.out_dir.glob("polygon122_vs_214*")):
        print(path)
    print(args.out_dir / "v12_polygon_122_hole_detail_cn.png")


if __name__ == "__main__":
    main()
