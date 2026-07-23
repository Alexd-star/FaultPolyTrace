# -*- coding: utf-8 -*-
"""Generate auditable local figures for branch-attribution differences.

The input CSV is produced by the C++ shadow diagnostics. This script never
changes formal polygon geometry; it only turns connectivity differences into
reviewable image evidence and an indexed candidate table.
"""

import argparse
import csv
import math
import os
from pathlib import Path
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager
import numpy as np
from PIL import Image, ImageDraw
from shapely import wkt

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from 局部形态质量诊断 import read_geoeast_dat


OLD_COLOR = "#B24745"
NEW_COLOR = "#145DA0"
NEUTRAL_COLOR = "#7A7A7A"


def configure_style():
    font_candidates = (
        r"C:\Windows\Fonts\msyh.ttc",
        r"C:\Windows\Fonts\simsun.ttc",
        r"C:\Windows\Fonts\simhei.ttf",
    )
    for path in font_candidates:
        if os.path.exists(path):
            font_manager.fontManager.addfont(path)
            name = font_manager.FontProperties(fname=path).get_name()
            plt.rcParams["font.sans-serif"] = [name, "DejaVu Sans"]
            break
    plt.rcParams.update(
        {
            "font.size": 7,
            "axes.linewidth": 0.7,
            "axes.spines.top": False,
            "axes.spines.right": False,
            "xtick.major.width": 0.6,
            "ytick.major.width": 0.6,
            "xtick.major.size": 2.5,
            "ytick.major.size": 2.5,
            "svg.fonttype": "none",
            "pdf.fonttype": 42,
            "axes.unicode_minus": False,
        }
    )


def _as_float(value, default=0.0):
    try:
        return float(value)
    except (TypeError, ValueError):
        return float(default)


def _as_int(value, default=0):
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return int(default)


def branch_ids_from_pairs(text):
    branch_ids = set()
    for item in str(text or "").split(";"):
        item = item.strip()
        if not item or "-" not in item:
            continue
        first, second = item.split("-", 1)
        branch_ids.add(_as_int(first, -1))
        branch_ids.add(_as_int(second, -1))
    branch_ids.discard(-1)
    return branch_ids


def _pair_set(text):
    pairs = set()
    for item in str(text or "").split(";"):
        item = item.strip()
        if not item or "-" not in item:
            continue
        first, second = item.split("-", 1)
        a = _as_int(first, -1)
        b = _as_int(second, -1)
        if a >= 0 and b >= 0:
            pairs.add(tuple(sorted((a, b))))
    return pairs


def _flag(row, field, fallback_field=None):
    value = row.get(field)
    if value is None and fallback_field:
        value = row.get(fallback_field)
    return str(value or "0").strip() == "1"


def describe_difference_candidate(row):
    if _flag(row, "ambiguous"):
        return "根系连通歧义候选（不进入候选分组）"
    if _flag(
        row,
        "accepted_differs_from_legacy",
        "differs_from_legacy",
    ):
        return "根系连通差异候选（可进入受控实验）"
    return "根系连通差异候选（仅供复核）"


def rank_difference_rows(rows):
    ranked = []
    for raw in rows:
        if not _flag(
            raw,
            "candidate_differs_from_legacy",
            "differs_from_legacy",
        ):
            continue
        row = dict(raw)
        confidence = _as_float(row.get("confidence"), math.inf)
        span_x = _as_int(row.get("max_grid_x")) - _as_int(
            row.get("min_grid_x")
        )
        span_y = _as_int(row.get("max_grid_y")) - _as_int(
            row.get("min_grid_y")
        )
        span = max(span_x, span_y)
        pair_delta = len(
            _pair_set(row.get("legacy_pairs"))
            ^ _pair_set(row.get("new_pairs"))
        )
        ambiguous = _flag(row, "ambiguous")
        if ambiguous or confidence <= 0.05 or span >= 12 or pair_delta >= 6:
            priority = "high"
        else:
            priority = "medium"
        row["review_priority"] = priority
        row["_rank_key"] = (
            0 if ambiguous else 1,
            confidence,
            -span,
            -pair_delta,
            _as_int(row.get("cluster_id")),
        )
        ranked.append(row)
    ranked.sort(key=lambda item: item["_rank_key"])
    return ranked


def read_difference_rows(path):
    with open(path, "r", encoding="utf-8-sig", newline="") as stream:
        return rank_difference_rows(list(csv.DictReader(stream)))


def read_branches(path):
    branches = {}
    with open(path, "r", encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            branch_id = _as_int(row.get("id"), -1)
            if branch_id < 0:
                continue
            geometry = wkt.loads(row.get("wkt", "LINESTRING EMPTY"))
            if geometry.is_empty:
                continue
            coords = np.asarray(geometry.coords, dtype=float)
            branches[branch_id] = {
                "id": branch_id,
                "status": row.get("status", ""),
                "coords": coords,
                "bbox": (
                    float(coords[:, 0].min()),
                    float(coords[:, 0].max()),
                    float(coords[:, 1].min()),
                    float(coords[:, 1].max()),
                ),
            }
    return branches


def _bbox_overlaps(first, second):
    return not (
        first[1] < second[0]
        or first[0] > second[1]
        or first[3] < second[2]
        or first[2] > second[3]
    )


def _candidate_extent(row, xs, ys, dx, dy, padding_cells):
    min_x = max(0, _as_int(row.get("min_grid_x")))
    max_x = min(len(xs) - 1, _as_int(row.get("max_grid_x")))
    min_y = max(0, _as_int(row.get("min_grid_y")))
    max_y = min(len(ys) - 1, _as_int(row.get("max_grid_y")))
    span = max(max_x - min_x + 1, max_y - min_y + 1)
    pad = max(int(padding_cells), min(40, span // 2 + 6))
    x0 = max(0, min_x - pad)
    x1 = min(len(xs) - 1, max_x + pad)
    y0 = max(0, min_y - pad)
    y1 = min(len(ys) - 1, max_y + pad)
    world_extent = (
        float(xs[x0] - 0.5 * dx),
        float(xs[x1] + 0.5 * dx),
        float(ys[y0] - 0.5 * dy),
        float(ys[y1] + 0.5 * dy),
    )
    return (x0, x1, y0, y1), world_extent


def _draw_branch_label(ax, branch, center_x, center_y, color):
    coords = branch["coords"]
    distance = np.square(coords[:, 0] - center_x) + np.square(
        coords[:, 1] - center_y
    )
    max_distance = float(distance.max())
    target_distance = 0.55 * max_distance
    point = coords[int(np.argmin(np.abs(distance - target_distance)))]
    ax.text(
        point[0],
        point[1],
        str(branch["id"]),
        color=color,
        fontsize=5.8,
        ha="center",
        va="center",
        bbox={
            "facecolor": "white",
            "edgecolor": "none",
            "alpha": 0.78,
            "pad": 0.35,
        },
        zorder=6,
    )


def _draw_panel(
    ax,
    values,
    extent_edges,
    local_extent,
    nearby_branches,
    highlighted_ids,
    label_ids,
    center,
    color,
    linestyle,
    panel_label,
    title,
):
    weak = np.ma.masked_where(values <= 0.0, values)
    image = ax.imshow(
        weak,
        extent=extent_edges,
        origin="lower",
        cmap="cividis",
        interpolation="nearest",
        vmin=0.0,
        vmax=1.0,
        aspect="equal",
        rasterized=True,
    )
    for branch in nearby_branches:
        coords = branch["coords"]
        ax.plot(
            coords[:, 0],
            coords[:, 1],
            color=NEUTRAL_COLOR,
            linewidth=0.45,
            alpha=0.50,
            zorder=2,
        )
    for branch in nearby_branches:
        if branch["id"] not in highlighted_ids:
            continue
        coords = branch["coords"]
        ax.plot(
            coords[:, 0],
            coords[:, 1],
            color=color,
            linewidth=1.25,
            linestyle=linestyle,
            alpha=0.98,
            zorder=4,
        )
        if branch["id"] in label_ids:
            _draw_branch_label(
                ax,
                branch,
                center[0],
                center[1],
                color,
            )
    ax.plot(
        center[0],
        center[1],
        marker="+",
        markersize=5.0,
        markeredgewidth=0.9,
        color="#111111",
        zorder=7,
    )
    ax.set_xlim(local_extent[0], local_extent[1])
    ax.set_ylim(local_extent[2], local_extent[3])
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.tick_params(labelsize=6)
    ax.text(
        0.01,
        0.99,
        panel_label,
        transform=ax.transAxes,
        ha="left",
        va="top",
        fontsize=8,
        fontweight="bold",
        color="#111111",
    )
    ax.set_title(title, fontsize=7.2, pad=3)
    return image


def draw_difference_figure(
    row,
    values,
    xs,
    ys,
    dx,
    dy,
    extent_edges,
    branches,
    output_base,
    padding_cells=14,
    export_vectors=False,
):
    _, local_extent = _candidate_extent(
        row, xs, ys, dx, dy, padding_cells
    )
    center = (
        _as_float(row.get("center_world_x")),
        _as_float(row.get("center_world_y")),
    )
    nearby = [
        branch
        for branch in branches.values()
        if _bbox_overlaps(branch["bbox"], local_extent)
    ]
    legacy_ids = branch_ids_from_pairs(row.get("legacy_pairs"))
    new_ids = branch_ids_from_pairs(row.get("new_pairs"))
    legacy_ids.update(
        _as_int(item, -1)
        for item in str(row.get("legacy_unmatched", "")).split(";")
        if item
    )
    new_ids.update(
        _as_int(item, -1)
        for item in str(row.get("new_unmatched", "")).split(";")
        if item
    )
    legacy_ids.discard(-1)
    new_ids.discard(-1)

    fig, axes = plt.subplots(
        1,
        2,
        figsize=(7.2, 3.2),
        sharex=True,
        sharey=True,
    )
    fig.subplots_adjust(
        left=0.075,
        right=0.90,
        bottom=0.16,
        top=0.82,
        wspace=0.18,
    )
    image = _draw_panel(
        axes[0],
        values,
        extent_edges,
        local_extent,
        nearby,
        legacy_ids,
        new_ids,
        center,
        OLD_COLOR,
        "--",
        "a",
        "现行单像元贪心归属",
    )
    _draw_panel(
        axes[1],
        values,
        extent_edges,
        local_extent,
        nearby,
        new_ids,
        new_ids,
        center,
        NEW_COLOR,
        "-",
        "b",
        "多尺度证据归属候选",
    )
    confidence = _as_float(row.get("confidence"), math.inf)
    fig.suptitle(
        "交点簇 {cluster}：{description}，置信差 {confidence:.4f}".format(
            cluster=row.get("cluster_id", ""),
            description=describe_difference_candidate(row),
            confidence=confidence,
        ),
        fontsize=8,
    )
    colorbar_axis = fig.add_axes((0.92, 0.16, 0.018, 0.66))
    colorbar = fig.colorbar(image, cax=colorbar_axis)
    colorbar.set_label("断层属性值", fontsize=6.5)
    colorbar.ax.tick_params(labelsize=6, width=0.5, length=2)
    output_base.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(
        output_base.with_suffix(".png"),
        dpi=300,
        facecolor="white",
    )
    if export_vectors:
        fig.savefig(output_base.with_suffix(".svg"), facecolor="white")
        fig.savefig(output_base.with_suffix(".pdf"), facecolor="white")
    plt.close(fig)


def write_contact_sheets(image_paths, output_dir, columns=4, rows=5):
    output_dir.mkdir(parents=True, exist_ok=True)
    per_page = columns * rows
    for page_index in range(0, len(image_paths), per_page):
        page_paths = image_paths[page_index : page_index + per_page]
        thumbnails = []
        for path in page_paths:
            image = Image.open(path).convert("RGB")
            image.thumbnail((900, 400))
            thumbnails.append((path, image.copy()))
            image.close()
        cell_width = max(image.width for _, image in thumbnails)
        cell_height = max(image.height for _, image in thumbnails) + 28
        sheet = Image.new(
            "RGB",
            (columns * cell_width, rows * cell_height),
            "white",
        )
        draw = ImageDraw.Draw(sheet)
        for index, (path, image) in enumerate(thumbnails):
            col = index % columns
            row = index // columns
            x = col * cell_width
            y = row * cell_height
            sheet.paste(image, (x, y))
            draw.text((x + 6, y + image.height + 4), path.stem, fill="black")
        page_number = page_index // per_page + 1
        sheet.save(
            output_dir / f"分支归属差异接触表_{page_number:02d}.png",
            dpi=(300, 300),
        )


def write_index(rows, output_path):
    fields = [
        "rank",
        "cluster_id",
        "review_priority",
        "center_grid_x",
        "center_grid_y",
        "center_world_x",
        "center_world_y",
        "legacy_pairs",
        "new_pairs",
        "legacy_unmatched",
        "new_unmatched",
        "pairing_differs_from_legacy",
        "differs_from_legacy",
        "candidate_pairing_differs_from_legacy",
        "candidate_differs_from_legacy",
        "accepted_pairing_differs_from_legacy",
        "accepted_differs_from_legacy",
        "ambiguous",
        "best_cost",
        "alternative_cost",
        "confidence",
        "figure",
    ]
    with open(output_path, "w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for rank, row in enumerate(rows, start=1):
            clean = {field: row.get(field, "") for field in fields}
            clean["rank"] = rank
            writer.writerow(clean)


def main():
    configure_style()
    parser = argparse.ArgumentParser(
        description="生成旧贪心与多尺度分支归属差异的局部科研诊断图"
    )
    parser.add_argument("--dat", required=True, help="平面断层属性 DAT")
    parser.add_argument("--differences", required=True, help="交点差异 CSV")
    parser.add_argument("--branches", required=True, help="分支结果 CSV")
    parser.add_argument("--out-dir", required=True, help="证据输出目录")
    parser.add_argument(
        "--max-figures",
        type=int,
        default=0,
        help="最多生成图数，0 表示全部根系连通差异",
    )
    parser.add_argument(
        "--vector-limit",
        type=int,
        default=40,
        help="前多少张同时输出 SVG/PDF",
    )
    parser.add_argument(
        "--padding-cells",
        type=int,
        default=14,
        help="候选窗口周边扩展网格数",
    )
    args = parser.parse_args()

    values, xs, ys, dx, dy, extent_edges = read_geoeast_dat(args.dat)
    rows = read_difference_rows(args.differences)
    if args.max_figures > 0:
        rows = rows[: args.max_figures]
    branches = read_branches(args.branches)
    output_dir = Path(args.out_dir)
    figure_dir = output_dir / "局部分支归属差异图"
    figure_dir.mkdir(parents=True, exist_ok=True)

    image_paths = []
    for index, row in enumerate(rows, start=1):
        output_base = figure_dir / (
            f"{index:03d}_cluster_{_as_int(row.get('cluster_id')):04d}"
        )
        draw_difference_figure(
            row,
            values,
            xs,
            ys,
            dx,
            dy,
            extent_edges,
            branches,
            output_base,
            padding_cells=args.padding_cells,
            export_vectors=index <= max(0, args.vector_limit),
        )
        image_path = output_base.with_suffix(".png")
        image_paths.append(image_path)
        row["figure"] = str(image_path)

    write_index(rows, output_dir / "分支归属差异复核索引.csv")
    if image_paths:
        write_contact_sheets(
            image_paths,
            output_dir / "接触表",
        )
    print(output_dir / "分支归属差异复核索引.csv")
    print(figure_dir)


if __name__ == "__main__":
    main()
