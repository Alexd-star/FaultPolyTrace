import argparse
import csv
import json
import math
import os
import sys
from collections import deque
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib import font_manager
import matplotlib.pyplot as plt
import numpy as np
from shapely import wkt
from shapely.geometry import MultiPolygon, Polygon, box
from shapely.ops import unary_union

from wkt_geometry import parse_wkt_polygon_with_holes, rasterize_polygon

csv.field_size_limit(min(sys.maxsize, 2_147_483_647))


def configure_chinese_font():
    for path in (
        r"C:\Windows\Fonts\msyh.ttc",
        r"C:\Windows\Fonts\simhei.ttf",
        r"C:\Windows\Fonts\simsun.ttc",
    ):
        if os.path.exists(path):
            font_manager.fontManager.addfont(path)
            name = font_manager.FontProperties(fname=path).get_name()
            plt.rcParams["font.sans-serif"] = [name, "DejaVu Sans"]
            plt.rcParams["axes.unicode_minus"] = False
            return


def read_grid(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="ignore") as stream:
        for line in stream:
            parts = line.split()
            if len(parts) < 5 or line.lstrip().startswith("#"):
                continue
            rows.append(
                (int(float(parts[0])), int(float(parts[1])),
                 float(parts[2]), float(parts[3]), float(parts[4]))
            )
    if not rows:
        raise RuntimeError(f"No grid samples were read: {path}")
    line_keys = sorted({row[0] for row in rows})
    cmp_keys = sorted({row[1] for row in rows})
    line_index = {value: index for index, value in enumerate(line_keys)}
    cmp_index = {value: index for index, value in enumerate(cmp_keys)}
    values = np.zeros((len(cmp_keys), len(line_keys)), dtype=float)
    x_by_line = {}
    y_by_cmp = {}
    for line_no, cmp_no, x, y, value in rows:
        values[cmp_index[cmp_no], line_index[line_no]] = value
        x_by_line[line_no] = x
        y_by_cmp[cmp_no] = y
    xs = np.asarray([x_by_line[key] for key in line_keys], dtype=float)
    ys = np.asarray([y_by_cmp[key] for key in cmp_keys], dtype=float)
    dx = abs(float(xs[1] - xs[0])) if len(xs) > 1 else 1.0
    dy = abs(float(ys[1] - ys[0])) if len(ys) > 1 else 1.0
    extent = (
        float(xs.min() - dx / 2.0), float(xs.max() + dx / 2.0),
        float(ys.min() - dy / 2.0), float(ys.max() + dy / 2.0),
    )
    return values, xs, ys, dx, dy, extent


def read_result(path):
    with open(path, "r", encoding="utf-8-sig", errors="ignore", newline="") as stream:
        reader = csv.DictReader(stream)
        rows = list(reader)
        fields = list(reader.fieldnames or [])
    polygons = []
    raster_polygons = []
    for row in rows:
        if row.get("status", "").lower() != "kept":
            continue
        geometry = wkt.loads(row["wkt"])
        polygons.append((int(float(row["id"])), geometry))
        exterior, holes = parse_wkt_polygon_with_holes(row["wkt"])
        raster_polygons.append((exterior, holes))
    return rows, fields, polygons, raster_polygons


def rasterize_result(raster_polygons, xs, ys, dx, dy):
    covered = np.zeros((len(ys), len(xs)), dtype=bool)
    for exterior, holes in raster_polygons:
        covered |= rasterize_polygon(
            exterior, len(xs), len(ys), xs, ys, dx, dy, holes
        )
    return covered


def components(mask, values):
    height, width = mask.shape
    visited = np.zeros_like(mask, dtype=bool)
    offsets = (
        (-1, -1), (0, -1), (1, -1), (-1, 0),
        (1, 0), (-1, 1), (0, 1), (1, 1),
    )
    output = []
    for y0 in range(height):
        for x0 in range(width):
            if not mask[y0, x0] or visited[y0, x0]:
                continue
            queue = deque([(x0, y0)])
            visited[y0, x0] = True
            cells = []
            while queue:
                x, y = queue.popleft()
                cells.append((x, y))
                for ox, oy in offsets:
                    nx, ny = x + ox, y + oy
                    if (0 <= nx < width and 0 <= ny < height and
                            mask[ny, nx] and not visited[ny, nx]):
                        visited[ny, nx] = True
                        queue.append((nx, ny))
            array = np.asarray(cells, dtype=int)
            bbox_width = int(array[:, 0].max() - array[:, 0].min() + 1)
            bbox_height = int(array[:, 1].max() - array[:, 1].min() + 1)
            aspect = max(bbox_width, bbox_height) / max(1, min(bbox_width, bbox_height))
            sample = values[array[:, 1], array[:, 0]]
            output.append(
                {
                    "cells": cells,
                    "cell_count": len(cells),
                    "aspect": float(aspect),
                    "mean_value": float(np.mean(sample)),
                    "max_value": float(np.max(sample)),
                }
            )
    return output


def iter_polygons(geometry):
    if geometry.is_empty:
        return
    if isinstance(geometry, Polygon):
        yield geometry
    elif isinstance(geometry, MultiPolygon):
        yield from geometry.geoms
    else:
        for item in getattr(geometry, "geoms", []):
            if isinstance(item, Polygon):
                yield item


def build_candidate_geometry(item, xs, ys, dx, dy, base_union, grid_extent):
    raw = unary_union(
        [
            box(xs[x] - dx / 2.0, ys[y] - dy / 2.0,
                xs[x] + dx / 2.0, ys[y] + dy / 2.0)
            for x, y in item["cells"]
        ]
    )
    clearance = raw.distance(base_union)
    expansion = max(0.01, min(0.05 * min(dx, dy), 0.45 * clearance))
    candidate = raw.buffer(expansion, quad_segs=1, join_style=2)
    candidate = candidate.intersection(grid_extent).buffer(0)
    if candidate.intersects(base_union):
        candidate = candidate.difference(base_union.buffer(1e-5)).buffer(0)
    return candidate, float(clearance), float(expansion)


def ring_wkt(geometry):
    return wkt.dumps(geometry, rounding_precision=6, trim=True)


def write_result(path, rows, fields, additions):
    required = ["id", "status", "area", "perimeter", "cell_count", "mean_value", "max_value", "wkt"]
    for field in required:
        if field not in fields:
            fields.append(field)
    with open(path, "w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)
        writer.writerows(additions)


def draw_attribute(ax, values, extent):
    shown = np.ma.masked_where(values < 0.30, values)
    ax.imshow(
        shown, extent=extent, origin="lower", cmap="viridis_r",
        interpolation="nearest", vmin=0.30, vmax=1.0, aspect="equal",
    )


def draw_geometries(ax, geometries, color, linewidth, alpha=1.0):
    for geometry in geometries:
        for polygon in iter_polygons(geometry):
            x, y = polygon.exterior.xy
            ax.plot(x, y, color=color, linewidth=linewidth, alpha=alpha)
            for hole in polygon.interiors:
                hx, hy = hole.xy
                ax.plot(hx, hy, color=color, linewidth=linewidth, alpha=alpha)


def format_axes(ax, extent):
    ax.set_xlim(extent[0], extent[1])
    ax.set_ylim(extent[2], extent[3])
    ax.set_xlabel("X 坐标")
    ax.set_ylabel("Y 坐标")
    ax.ticklabel_format(style="plain", axis="both", useOffset=False)
    ax.grid(False)


def save_figures(out_dir, values, extent, base_geometries, additions):
    addition_geometries = [item["geometry"] for item in additions]
    all_geometries = base_geometries + addition_geometries

    fig, ax = plt.subplots(figsize=(18.5, 8.6), dpi=520)
    draw_attribute(ax, values, extent)
    draw_geometries(ax, all_geometries, "white", 1.05, 0.95)
    draw_geometries(ax, all_geometries, "#e2211c", 0.48, 1.0)
    format_axes(ax, extent)
    ax.set_title("断层多边形受控补追结果（属性叠加）", fontsize=19, pad=14)
    fig.tight_layout()
    fig.savefig(out_dir / "接收后结果叠加图.png", bbox_inches="tight", facecolor="white")
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(18.5, 8.6), dpi=520)
    draw_attribute(ax, values, extent)
    draw_geometries(ax, base_geometries, "#444444", 0.42, 0.62)
    draw_geometries(ax, addition_geometries, "white", 1.65, 1.0)
    draw_geometries(ax, addition_geometries, "#00a6d6", 0.95, 1.0)
    format_axes(ax, extent)
    ax.set_title("受控补追新增多边形位置", fontsize=19, pad=14)
    fig.tight_layout()
    fig.savefig(out_dir / "新增多边形差异图.png", bbox_inches="tight", facecolor="white")
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(18.5, 8.6), dpi=520)
    draw_geometries(ax, all_geometries, "#e2211c", 0.56, 1.0)
    format_axes(ax, extent)
    ax.set_title("断层多边形受控补追结果（矢量边界）", fontsize=19, pad=14)
    fig.tight_layout()
    fig.savefig(out_dir / "接收后纯边界图.png", bbox_inches="tight", facecolor="white")
    plt.close(fig)


def main():
    configure_chinese_font()
    parser = argparse.ArgumentParser(description="Promote confirmed weak linear residuals to closed polygon candidates")
    parser.add_argument("--dat", required=True)
    parser.add_argument("--base-csv", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--weak-threshold", type=float, default=0.66)
    parser.add_argument("--min-cells", type=int, default=8)
    parser.add_argument("--min-aspect", type=float, default=3.0)
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    values, xs, ys, dx, dy, extent = read_grid(args.dat)
    rows, fields, base, raster_polygons = read_result(args.base_csv)
    covered = rasterize_result(raster_polygons, xs, ys, dx, dy)
    missed = (values >= args.weak_threshold) & ~covered
    selected = [
        item for item in components(missed, values)
        if item["cell_count"] >= args.min_cells and item["aspect"] >= args.min_aspect
    ]
    base_geometries = [geometry for _, geometry in base]
    base_union = unary_union(base_geometries)
    grid_extent = box(extent[0], extent[2], extent[1], extent[3])
    next_id = max(int(float(row.get("id", 0) or 0)) for row in rows) + 1
    output_rows = []
    evidence = []
    figure_additions = []
    for component_index, item in enumerate(selected, start=1):
        geometry, clearance, expansion = build_candidate_geometry(
            item, xs, ys, dx, dy, base_union, grid_extent
        )
        pieces = [piece for piece in iter_polygons(geometry) if piece.area > 0.0]
        for piece_index, piece in enumerate(pieces, start=1):
            output_rows.append(
                {
                    "id": next_id,
                    "status": "kept",
                    "area": f"{piece.area:.6f}",
                    "perimeter": f"{piece.length:.6f}",
                    "cell_count": item["cell_count"],
                    "mean_value": f"{item['mean_value']:.6f}",
                    "max_value": f"{item['max_value']:.6f}",
                    "wkt": ring_wkt(piece),
                }
            )
            figure_additions.append({"geometry": piece})
            evidence.append(
                {
                    "component": component_index,
                    "piece": piece_index,
                    "polygon_id": next_id,
                    "cell_count": item["cell_count"],
                    "aspect": item["aspect"],
                    "mean_value": item["mean_value"],
                    "max_value": item["max_value"],
                    "base_clearance": clearance,
                    "boundary_expansion": expansion,
                    "area": piece.area,
                    "valid": piece.is_valid,
                }
            )
            next_id += 1

    output_csv = out_dir / "受控补追候选结果.csv"
    write_result(output_csv, rows, fields, output_rows)
    all_geometries = base_geometries + [item["geometry"] for item in figure_additions]
    combined = MultiPolygon(all_geometries)
    report = {
        "mode": "generic_weak_linear_residual_promotion",
        "base_kept_polygons": len(base_geometries),
        "selected_components": len(selected),
        "added_polygons": len(output_rows),
        "result_kept_polygons": len(all_geometries),
        "weak_threshold": args.weak_threshold,
        "minimum_cells": args.min_cells,
        "minimum_aspect": args.min_aspect,
        "combined_valid": combined.is_valid,
        "combined_area": combined.area,
        "evidence": evidence,
    }
    (out_dir / "受控补追候选报告.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    save_figures(out_dir, values, extent, base_geometries, figure_additions)
    print(output_csv)
    print(json.dumps({key: value for key, value in report.items() if key != "evidence"}, ensure_ascii=False))


if __name__ == "__main__":
    main()
