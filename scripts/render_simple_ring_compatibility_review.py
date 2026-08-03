# -*- coding: utf-8 -*-
"""Render the GeoEast simple-ring compatibility A/B review pack."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib.lines import Line2D
from matplotlib.patches import Patch, Polygon as PolygonPatch
import matplotlib.pyplot as plt
from matplotlib.colors import Normalize
from matplotlib.ticker import FuncFormatter
import numpy as np
from shapely import wkt
from shapely.geometry import Polygon

from render_polygon122_comparison import (
    apply_axes,
    configure_chinese_font,
    draw_boundary,
    read_attribute_dat,
)


def read_kept(path: Path):
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return {
            int(row["id"]): wkt.loads(row["wkt"])
            for row in csv.DictReader(stream)
            if row.get("status") == "kept"
        }


def attribute_image(ax, values, extent):
    masked = np.ma.masked_where(~np.isfinite(values) | (values <= 0), values)
    return ax.imshow(
        masked,
        extent=extent,
        origin="lower",
        cmap="viridis_r",
        norm=Normalize(vmin=0.30, vmax=1.00, clip=True),
        interpolation="nearest",
        aspect="equal",
    )


def format_coordinate(value, _position):
    return f"{value:,.0f}"


def style(ax, view, font):
    apply_axes(ax, view, font, tick_size=8)
    ax.xaxis.set_major_formatter(FuncFormatter(format_coordinate))
    ax.yaxis.set_major_formatter(FuncFormatter(format_coordinate))


def add_colorbar(fig, ax, image, font):
    colorbar = fig.colorbar(image, ax=ax, fraction=0.020, pad=0.016, shrink=0.82)
    colorbar.set_label("断层属性值", fontproperties=font, fontsize=9)
    for label in colorbar.ax.get_yticklabels():
        label.set_fontproperties(font)
        label.set_fontsize(8)


def geometry_bounds(geometry, padding=140.0):
    min_x, min_y, max_x, max_y = geometry.bounds
    span_x = max(1.0, max_x - min_x)
    span_y = max(1.0, max_y - min_y)
    pad_x = max(padding, span_x * 0.10)
    pad_y = max(padding, span_y * 0.18)
    return min_x - pad_x, max_x + pad_x, min_y - pad_y, max_y + pad_y


def fill_exterior(ax, geometry, color, alpha):
    coordinates = np.asarray(geometry.exterior.coords)
    ax.add_patch(
        PolygonPatch(
            coordinates,
            closed=True,
            facecolor=color,
            edgecolor="none",
            alpha=alpha,
            zorder=4,
        )
    )


def render_local(out_dir, object_id, old, new, values, extent, font):
    combined = old.union(new)
    view = geometry_bounds(combined)
    removed = old.difference(new)
    added = new.difference(old)
    holes = [Polygon(ring) for ring in old.interiors]

    fig, axes = plt.subplots(1, 3, figsize=(21, 7.2), dpi=220)
    for ax in axes:
        image = attribute_image(ax, values, extent)
        style(ax, view, font)

    fill_exterior(axes[0], old, "#67c8dd", 0.42)
    draw_boundary(axes[0], old, "#d62828", linewidth=1.25, alpha=1.0, zorder=8)
    axes[0].set_title(
        "原 DAT 的 GeoEast 外环显示\n内环区域会被填充",
        fontproperties=font,
        fontsize=13,
    )

    fill_exterior(axes[1], new, "#67c8dd", 0.35)
    draw_boundary(axes[1], new, "#d62828", linewidth=1.25, alpha=1.0, zorder=8)
    axes[1].set_title(
        "简单环兼容结果\n闭合外环直接表达",
        fontproperties=font,
        fontsize=13,
    )

    draw_boundary(axes[2], old, "#6b7280", linewidth=0.85, alpha=0.75, zorder=6)
    draw_boundary(axes[2], new, "#d62828", linewidth=1.20, alpha=1.0, zorder=8)
    if not removed.is_empty:
        for geometry in getattr(removed, "geoms", [removed]):
            if geometry.geom_type == "Polygon":
                fill_exterior(axes[2], geometry, "#f59e0b", 0.82)
    if not added.is_empty:
        for geometry in getattr(added, "geoms", [added]):
            if geometry.geom_type == "Polygon":
                fill_exterior(axes[2], geometry, "#22c55e", 0.72)
    axes[2].set_title(
        "几何差异\n橙色为最小开槽，绿色为小孔填补",
        fontproperties=font,
        fontsize=13,
    )

    handles = [
        Line2D([0], [0], color="#d62828", linewidth=1.6, label="兼容结果边界"),
        Patch(facecolor="#f59e0b", alpha=0.82, label="开槽移除"),
        Patch(facecolor="#22c55e", alpha=0.72, label="小孔填补"),
    ]
    axes[2].legend(handles=handles, loc="lower right", prop=font, framealpha=0.94)
    add_colorbar(fig, axes[2], image, font)
    fig.suptitle(
        f"对象 {object_id}：GeoEast 简单环兼容复核",
        fontproperties=font,
        fontsize=16,
        y=0.99,
    )
    fig.tight_layout(rect=(0.0, 0.0, 1.0, 0.96))
    fig.savefig(
        out_dir / f"对象_{object_id}_简单环兼容复核.png",
        dpi=420,
        bbox_inches="tight",
        facecolor="white",
    )
    plt.close(fig)

    return {
        "id": object_id,
        "old_holes": len(old.interiors),
        "new_holes": len(new.interiors),
        "old_area": old.area,
        "new_area": new.area,
        "symmetric_difference_area": old.symmetric_difference(new).area,
        "removed_area": removed.area,
        "added_area": added.area,
        "old_hole_area": sum(hole.area for hole in holes),
    }


def render_full(out_dir, candidate, values, extent, font):
    fig, ax = plt.subplots(figsize=(19, 9.2), dpi=220)
    image = attribute_image(ax, values, extent)
    for geometry in candidate.values():
        draw_boundary(ax, geometry, "#d62828", linewidth=0.42, alpha=0.98, zorder=8)
    style(ax, extent, font)
    ax.set_title(
        "断层多边形自动追踪结果（GeoEast 简单环兼容）",
        fontproperties=font,
        fontsize=16,
        pad=12,
    )
    ax.legend(
        handles=[Line2D([0], [0], color="#d62828", linewidth=1.6, label="闭合断层多边形边界")],
        loc="lower right",
        prop=font,
        framealpha=0.94,
    )
    add_colorbar(fig, ax, image, font)
    fig.savefig(
        out_dir / "全图_简单环兼容结果_属性叠加.png",
        dpi=500,
        bbox_inches="tight",
        facecolor="white",
    )
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dat", required=True, type=Path)
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--candidate", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    font = configure_chinese_font()
    values, extent = read_attribute_dat(args.dat)
    baseline = read_kept(args.baseline)
    candidate = read_kept(args.candidate)
    if set(baseline) != set(candidate):
        raise RuntimeError("Baseline and candidate object IDs do not match.")

    changed = []
    for object_id in sorted(baseline):
        old = baseline[object_id]
        new = candidate[object_id]
        if old.symmetric_difference(new).area <= 1e-4:
            continue
        changed.append(
            render_local(
                args.out_dir,
                object_id,
                old,
                new,
                values,
                extent,
                font,
            )
        )
    render_full(args.out_dir, candidate, values, extent, font)

    import json

    (args.out_dir / "简单环兼容差异.json").write_text(
        json.dumps(changed, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    print(args.out_dir)


if __name__ == "__main__":
    main()
