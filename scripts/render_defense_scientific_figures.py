# -*- coding: utf-8 -*-
"""Render defense figures from the formal v1.1.3 grid and diagnostics.

The figures use one real spatial scene throughout the method narrative.  They
are intended for slide composition, so each figure is exported as a high-DPI
PNG and an editable SVG without an embedded slide title.
"""

from __future__ import annotations

import argparse
import csv
import json
from dataclasses import dataclass
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib import colors, patheffects
from matplotlib.gridspec import GridSpec, GridSpecFromSubplotSpec
from matplotlib.lines import Line2D
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Polygon as PolygonPatch
from matplotlib.ticker import FuncFormatter
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from scipy.ndimage import distance_transform_edt
from shapely import wkt
from shapely.geometry import box

from render_polygon122_comparison import (
    configure_chinese_font,
    read_all_polygons,
    read_attribute_dat,
)


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RUN = REPO_ROOT / "outputs" / "verification" / "submission_repro_v1.1.3"
DEFAULT_RESULT = DEFAULT_RUN / "FaultPolyTrace_Official_Result.csv"
DEFAULT_BRANCHES = DEFAULT_RUN / "FaultPolyTrace_Official_Result.branches.csv"
DEFAULT_WEAK = DEFAULT_RUN / "FaultPolyTrace_Official_Result.csv.weak-continuity.csv"
DEFAULT_DIAGNOSTICS = DEFAULT_RUN / "FaultPolyTrace_Official_Result.diagnostics.json"
DEFAULT_SUMMARY = DEFAULT_RUN / "复现结果摘要.json"
DEFAULT_OUT = (
    REPO_ROOT
    / "outputs"
    / "presentation_assets"
    / "defense_v1.1.3_realdata_20260825"
)


COLORS = {
    "navy": "#163A5F",
    "blue": "#3775BA",
    "teal": "#2B9A9A",
    "cyan": "#58C8C5",
    "pale_blue": "#D9EAF2",
    "orange": "#D9824B",
    "red": "#C4473A",
    "gold": "#F0C75E",
    "slate": "#6C7A89",
    "ink": "#17212B",
    "muted": "#617181",
    "paper": "#F7F9FA",
    "plate": "#111820",
    "grid": "#2A3743",
}

STRONG_THRESHOLD = 0.95
WEAK_THRESHOLD = 0.66
ACCEPTED_CANDIDATE_ID = 18
REJECTED_CANDIDATE_ID = 19


@dataclass
class BranchRecord:
    status: str
    geometry: object
    start_is_junction: bool
    end_is_junction: bool


@dataclass
class FigureData:
    values: np.ndarray
    extent: tuple[float, float, float, float]
    dx: float
    dy: float
    polygons: list[tuple[int, object]]
    polygon_rows: pd.DataFrame
    branches: list[BranchRecord]
    candidates: pd.DataFrame
    diagnostics: dict
    summary: dict

    def grid_to_world(self, x_index: float, y_index: float) -> tuple[float, float]:
        x = self.extent[0] + (x_index + 0.5) * self.dx
        y = self.extent[2] + (y_index + 0.5) * self.dy
        return x, y

    def view_from_grid(
        self,
        xmin: float,
        xmax: float,
        ymin: float,
        ymax: float,
    ) -> tuple[float, float, float, float]:
        x0, y0 = self.grid_to_world(xmin, ymin)
        x1, y1 = self.grid_to_world(xmax, ymax)
        return x0, x1, y0, y1


def configure_style() -> object:
    font_prop = configure_chinese_font()
    plt.rcParams.update(
        {
            "figure.facecolor": "white",
            "savefig.facecolor": "white",
            "axes.unicode_minus": False,
            "svg.fonttype": "none",
            "pdf.fonttype": 42,
            "axes.linewidth": 0.8,
            "font.size": 10,
        }
    )
    return font_prop


def read_branch_records(path: Path) -> list[BranchRecord]:
    csv.field_size_limit(2_147_483_647)
    records: list[BranchRecord] = []
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            records.append(
                BranchRecord(
                    status=row["status"].lower(),
                    geometry=wkt.loads(row["wkt"]),
                    start_is_junction=bool(int(row["start_is_junction"])),
                    end_is_junction=bool(int(row["end_is_junction"])),
                )
            )
    return records


def read_polygon_rows(path: Path) -> pd.DataFrame:
    csv.field_size_limit(2_147_483_647)
    rows = pd.read_csv(path)
    rows = rows[rows["status"].str.lower() == "kept"].copy()
    rows["geometry"] = rows["wkt"].map(wkt.loads)
    return rows


def parse_path(text: str, data: FigureData) -> np.ndarray:
    points = []
    for token in str(text).split(";"):
        if not token or ":" not in token:
            continue
        x_text, y_text = token.split(":", 1)
        points.append(data.grid_to_world(float(x_text), float(y_text)))
    return np.asarray(points, dtype=float)


def load_data(args: argparse.Namespace) -> FigureData:
    values, extent = read_attribute_dat(args.dat)
    dx = (extent[1] - extent[0]) / values.shape[1]
    dy = (extent[3] - extent[2]) / values.shape[0]
    polygons = read_all_polygons(args.result)
    polygon_rows = read_polygon_rows(args.result)
    branches = read_branch_records(args.branches)
    candidates = pd.read_csv(args.weak)
    with args.diagnostics.open("r", encoding="utf-8") as stream:
        diagnostics = json.load(stream)
    with args.summary.open("r", encoding="utf-8-sig") as stream:
        summary = json.load(stream)
    data = FigureData(
        values=values,
        extent=extent,
        dx=dx,
        dy=dy,
        polygons=polygons,
        polygon_rows=polygon_rows,
        branches=branches,
        candidates=candidates,
        diagnostics=diagnostics,
        summary=summary,
    )
    data.candidates["path_world"] = data.candidates["path_grid_xy"].map(
        lambda text: parse_path(text, data)
    )
    return data


def intersects_view(geometry: object, view: tuple[float, float, float, float]) -> bool:
    xmin, xmax, ymin, ymax = view
    gxmin, gymin, gxmax, gymax = geometry.bounds
    return gxmax >= xmin and gxmin <= xmax and gymax >= ymin and gymin <= ymax


def format_coordinate(value: float, _position: float) -> str:
    return f"{value / 1000.0:.1f}"


def style_map_axis(
    ax: plt.Axes,
    view: tuple[float, float, float, float],
    font_prop: object,
    dark: bool = True,
    show_y: bool = True,
) -> None:
    ax.set_xlim(view[0], view[1])
    ax.set_ylim(view[2], view[3])
    ax.set_aspect("equal", adjustable="box")
    # Tick labels and axis titles sit outside the dark data plate, on the white
    # figure canvas.  Keep them dark enough for projection and print.
    foreground = COLORS["muted"]
    spine = "#7D8F9C" if dark else "#A9B3BB"
    ax.tick_params(axis="both", colors=foreground, labelsize=8, width=0.6, length=2.5)
    ax.xaxis.set_major_formatter(FuncFormatter(format_coordinate))
    ax.yaxis.set_major_formatter(FuncFormatter(format_coordinate))
    for label in ax.get_xticklabels() + ax.get_yticklabels():
        label.set_fontproperties(font_prop)
    for side in ax.spines.values():
        side.set_color(spine)
        side.set_linewidth(0.7)
    ax.set_xlabel("X / km", fontproperties=font_prop, fontsize=8, color=foreground, labelpad=2)
    if show_y:
        ax.set_ylabel("Y / km", fontproperties=font_prop, fontsize=8, color=foreground, labelpad=2)
    else:
        ax.set_yticklabels([])
        ax.set_ylabel("")


def draw_attribute(
    ax: plt.Axes,
    data: FigureData,
    view: tuple[float, float, float, float],
    font_prop: object,
    show_y: bool = True,
    cmap: str = "viridis",
) -> object:
    masked = np.ma.masked_where(~np.isfinite(data.values) | (data.values <= 0), data.values)
    ax.set_facecolor(COLORS["plate"])
    image = ax.imshow(
        masked,
        extent=data.extent,
        origin="lower",
        cmap=cmap,
        norm=colors.Normalize(vmin=0.30, vmax=1.00, clip=True),
        interpolation="nearest",
        aspect="equal",
        rasterized=True,
        zorder=1,
    )
    style_map_axis(ax, view, font_prop, dark=True, show_y=show_y)
    return image


def draw_response_layers(ax: plt.Axes, data: FigureData) -> None:
    weak = np.ma.masked_where(
        ~np.isfinite(data.values)
        | (data.values < WEAK_THRESHOLD)
        | (data.values >= STRONG_THRESHOLD),
        np.ones_like(data.values),
    )
    strong = np.ma.masked_where(
        ~np.isfinite(data.values) | (data.values < STRONG_THRESHOLD),
        np.ones_like(data.values),
    )
    ax.imshow(
        weak,
        extent=data.extent,
        origin="lower",
        cmap=colors.ListedColormap(["#4FB6C2"]),
        vmin=0,
        vmax=1,
        interpolation="nearest",
        alpha=0.84,
        zorder=3,
    )
    ax.imshow(
        strong,
        extent=data.extent,
        origin="lower",
        cmap=colors.ListedColormap(["#F2D16B"]),
        vmin=0,
        vmax=1,
        interpolation="nearest",
        alpha=0.98,
        zorder=4,
    )


def draw_branches(
    ax: plt.Axes,
    data: FigureData,
    view: tuple[float, float, float, float],
    endpoints: bool = False,
    pruned: bool = True,
) -> None:
    for record in data.branches:
        if not intersects_view(record.geometry, view):
            continue
        if record.status == "pruned" and not pruned:
            continue
        coords = np.asarray(record.geometry.coords)
        if record.status == "kept":
            color = "#EAF4F8"
            width = 0.72
            alpha = 0.93
            linestyle = "-"
            zorder = 7
        else:
            color = "#7D8A94"
            width = 0.42
            alpha = 0.58
            linestyle = (0, (2, 2))
            zorder = 6
        ax.plot(
            coords[:, 0],
            coords[:, 1],
            color=color,
            linewidth=width,
            alpha=alpha,
            linestyle=linestyle,
            solid_capstyle="round",
            zorder=zorder,
        )
        if endpoints and record.status == "kept" and len(coords) >= 2:
            for point, junction in (
                (coords[0], record.start_is_junction),
                (coords[-1], record.end_is_junction),
            ):
                ax.scatter(
                    point[0],
                    point[1],
                    s=13 if junction else 8,
                    facecolor=COLORS["orange"] if junction else COLORS["teal"],
                    edgecolor="white",
                    linewidth=0.35,
                    zorder=10,
                )


def draw_polygon_geometry(
    ax: plt.Axes,
    geometry: object,
    edgecolor: str,
    facecolor: str | None = None,
    linewidth: float = 0.9,
    alpha: float = 1.0,
    zorder: int = 9,
) -> None:
    geoms = [geometry] if geometry.geom_type == "Polygon" else list(geometry.geoms)
    for polygon in geoms:
        outer = np.asarray(polygon.exterior.coords)
        if facecolor:
            ax.add_patch(
                PolygonPatch(
                    outer,
                    closed=True,
                    facecolor=facecolor,
                    edgecolor="none",
                    alpha=alpha,
                    zorder=zorder - 1,
                )
            )
        line = ax.plot(
            outer[:, 0],
            outer[:, 1],
            color=edgecolor,
            linewidth=linewidth,
            alpha=min(1.0, alpha + 0.15),
            solid_capstyle="round",
            solid_joinstyle="round",
            zorder=zorder,
        )[0]
        line.set_path_effects(
            [patheffects.Stroke(linewidth=linewidth + 0.8, foreground="#111820", alpha=0.75), patheffects.Normal()]
        )
        for ring in polygon.interiors:
            points = np.asarray(ring.coords)
            ax.plot(
                points[:, 0],
                points[:, 1],
                color=edgecolor,
                linewidth=max(0.45, linewidth * 0.75),
                zorder=zorder + 1,
            )


def draw_polygons(
    ax: plt.Axes,
    data: FigureData,
    view: tuple[float, float, float, float],
    fill: bool = False,
    color: str = "#F26B4A",
    alpha: float = 0.95,
    fill_alpha: float = 0.18,
) -> None:
    for _, geometry in data.polygons:
        if not intersects_view(geometry, view):
            continue
        draw_polygon_geometry(
            ax,
            geometry,
            edgecolor=color,
            facecolor="#4FB6C2" if fill else None,
            linewidth=0.88,
            alpha=fill_alpha if fill else alpha,
            zorder=11,
        )


def candidate_row(data: FigureData, candidate_id: int) -> pd.Series:
    rows = data.candidates[data.candidates["candidate_id"] == candidate_id]
    if len(rows) != 1:
        raise RuntimeError(f"弱路径候选 {candidate_id} 不唯一或不存在。")
    return rows.iloc[0]


def candidate_view(data: FigureData, row: pd.Series, pad_x: float = 72, pad_y: float = 56):
    cx = 0.5 * (float(row.endpoint_a_x) + float(row.endpoint_b_x))
    cy = 0.5 * (float(row.endpoint_a_y) + float(row.endpoint_b_y))
    return data.view_from_grid(cx - pad_x, cx + pad_x, cy - pad_y, cy + pad_y)


def draw_candidates(
    ax: plt.Axes,
    data: FigureData,
    view: tuple[float, float, float, float],
    highlight_ids: set[int] | None = None,
    only_highlighted: bool = False,
) -> None:
    view_box = box(view[0], view[2], view[1], view[3])
    for _, row in data.candidates.iterrows():
        path = row.path_world
        if path.size == 0:
            continue
        path_box = box(path[:, 0].min(), path[:, 1].min(), path[:, 0].max(), path[:, 1].max())
        if not view_box.intersects(path_box):
            continue
        accepted = bool(int(row.accepted))
        candidate_id = int(row.candidate_id)
        if only_highlighted and highlight_ids is not None and candidate_id not in highlight_ids:
            continue
        emphasized = highlight_ids is None or candidate_id in highlight_ids
        color = COLORS["teal"] if accepted else COLORS["orange"]
        linestyle = "-" if accepted else (0, (4, 2))
        linewidth = 2.3 if emphasized else 1.1
        alpha = 1.0 if emphasized else 0.5
        line = ax.plot(
            path[:, 0],
            path[:, 1],
            color=color,
            linewidth=linewidth,
            linestyle=linestyle,
            alpha=alpha,
            solid_capstyle="round",
            zorder=15,
        )[0]
        line.set_path_effects(
            [patheffects.Stroke(linewidth=linewidth + 1.4, foreground="#111820", alpha=0.9), patheffects.Normal()]
        )
        if emphasized:
            ax.scatter(
                [path[0, 0], path[-1, 0]],
                [path[0, 1], path[-1, 1]],
                s=22,
                facecolor=color,
                edgecolor="white",
                linewidth=0.7,
                zorder=17,
            )


def add_panel_label(ax: plt.Axes, label: str, font_prop: object, dark: bool = False) -> None:
    ax.text(
        -0.035,
        1.04,
        label,
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontproperties=font_prop,
        fontsize=16,
        fontweight="bold",
        color="white" if dark else COLORS["navy"],
        clip_on=False,
    )


def set_panel_title(ax: plt.Axes, title: str, font_prop: object, dark: bool = True) -> None:
    ax.set_title(
        title,
        fontproperties=font_prop,
        fontsize=11.5,
        color=COLORS["ink"],
        pad=8,
        fontweight="bold",
    )


def path_samples(data: FigureData, row: pd.Series) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    tokens = []
    for token in str(row.path_grid_xy).split(";"):
        if token and ":" in token:
            x_text, y_text = token.split(":", 1)
            tokens.append((int(float(x_text)), int(float(y_text))))
    xy = np.asarray(tokens, dtype=int)
    if len(xy) == 0:
        return np.array([]), np.array([]), np.empty((0, 2), dtype=int)
    sampled = data.values[xy[:, 1], xy[:, 0]]
    distance = np.zeros(len(xy), dtype=float)
    if len(xy) > 1:
        step = np.hypot(np.diff(xy[:, 0]) * data.dx, np.diff(xy[:, 1]) * data.dy)
        distance[1:] = np.cumsum(step)
    return distance, sampled, xy


def draw_path_profile(ax: plt.Axes, data: FigureData, row: pd.Series, font_prop: object) -> None:
    distance, sampled, _ = path_samples(data, row)
    ax.plot(distance, sampled, color=COLORS["navy"], linewidth=2.0, marker="o", markersize=3.4)
    ax.fill_between(distance, WEAK_THRESHOLD, sampled, where=sampled >= WEAK_THRESHOLD, color=COLORS["teal"], alpha=0.16)
    ax.axhline(STRONG_THRESHOLD, color=COLORS["gold"], linewidth=1.1, linestyle=(0, (4, 2)))
    ax.axhline(WEAK_THRESHOLD, color=COLORS["teal"], linewidth=1.1, linestyle=(0, (4, 2)))
    ax.set_ylim(0.25, 1.03)
    ax.set_xlabel("沿候选路径距离 / m", fontproperties=font_prop, fontsize=8.5)
    ax.set_ylabel("属性值", fontproperties=font_prop, fontsize=8.5)
    ax.tick_params(labelsize=7.5, colors=COLORS["muted"])
    ax.spines[["top", "right"]].set_visible(False)
    ax.grid(axis="y", color="#E3E8EC", linewidth=0.6)
    ax.text(0.99, 0.94, "强响应阈值", transform=ax.transAxes, ha="right", va="top", fontproperties=font_prop, fontsize=7.3, color="#96741E")
    ax.text(0.99, 0.54, "弱响应阈值", transform=ax.transAxes, ha="right", va="top", fontproperties=font_prop, fontsize=7.3, color=COLORS["teal"])


def draw_metric_cards(ax: plt.Axes, rows: list[tuple[str, str, str]], font_prop: object) -> None:
    ax.set_axis_off()
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    height = 0.78 / max(1, len(rows))
    for index, (label, value, state) in enumerate(rows):
        y = 0.91 - index * height
        color = COLORS["teal"] if state == "pass" else COLORS["orange"]
        ax.add_patch(
            FancyBboxPatch(
                (0.02, y - height * 0.72),
                0.96,
                height * 0.58,
                boxstyle="round,pad=0.012,rounding_size=0.018",
                facecolor="#F7FAFB",
                edgecolor="#D5DEE4",
                linewidth=0.8,
            )
        )
        ax.add_patch(
            FancyBboxPatch(
                (0.035, y - height * 0.61),
                0.018,
                height * 0.36,
                boxstyle="round,pad=0.002,rounding_size=0.006",
                facecolor=color,
                edgecolor="none",
            )
        )
        ax.text(0.075, y - height * 0.43, label, transform=ax.transAxes, ha="left", va="center", fontproperties=font_prop, fontsize=8.2, color=COLORS["ink"])
        ax.text(0.95, y - height * 0.43, value, transform=ax.transAxes, ha="right", va="center", fontproperties=font_prop, fontsize=8.3, color=color, fontweight="bold")


def save_figure(fig: plt.Figure, out_dir: Path, stem: str, dpi: int) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    # Preserve an exact 16:9 canvas.  Tight bounding boxes make every plate a
    # different aspect ratio and complicate PowerPoint placement.
    fig.savefig(out_dir / f"{stem}.png", dpi=dpi, facecolor="white")
    fig.savefig(out_dir / f"{stem}.svg", facecolor="white")
    plt.close(fig)


def add_stage_badge(ax: plt.Axes, number: int, label: str, color: str, font_prop: object) -> None:
    ax.text(
        0.018,
        0.955,
        f"{number:02d}  {label}",
        transform=ax.transAxes,
        ha="left",
        va="top",
        fontproperties=font_prop,
        fontsize=10.2,
        fontweight="bold",
        color="white",
        bbox={"boxstyle": "round,pad=0.30", "facecolor": color, "edgecolor": "white", "linewidth": 0.6, "alpha": 0.97},
        zorder=40,
    )


def add_candidate_label(ax: plt.Axes, row: pd.Series, text: str, color: str, font_prop: object) -> None:
    path = row.path_world
    if path.size == 0:
        return
    point = path[len(path) // 2]
    ax.annotate(
        text,
        xy=point,
        xytext=(16, 18),
        textcoords="offset points",
        ha="left",
        va="bottom",
        fontproperties=font_prop,
        fontsize=9.0,
        fontweight="bold",
        color=color,
        bbox={"boxstyle": "round,pad=0.24", "facecolor": "white", "edgecolor": color, "linewidth": 0.9, "alpha": 0.96},
        arrowprops={"arrowstyle": "-|>", "color": color, "linewidth": 1.0},
        zorder=42,
    )


def render_p02(data: FigureData, font_prop: object, out_dir: Path, dpi: int) -> None:
    """Official attribute overview with three source-traceable challenge regions."""
    rows = [
        candidate_row(data, ACCEPTED_CANDIDATE_ID),
        candidate_row(data, REJECTED_CANDIDATE_ID),
        candidate_row(data, 14),
    ]
    titles = ["弱响应与短距离缺口", "近邻分支与候选竞争", "交切位置与归属歧义"]
    colors_local = [COLORS["teal"], COLORS["orange"], COLORS["blue"]]
    views = [
        candidate_view(data, rows[0], pad_x=88, pad_y=58),
        candidate_view(data, rows[1], pad_x=88, pad_y=58),
        candidate_view(data, rows[2], pad_x=88, pad_y=58),
    ]

    fig = plt.figure(figsize=(16, 9))
    gs = GridSpec(
        2,
        3,
        figure=fig,
        height_ratios=[1.18, 1.0],
        hspace=0.22,
        wspace=0.10,
        left=0.05,
        right=0.98,
        top=0.93,
        bottom=0.075,
    )

    ax_overview = fig.add_subplot(gs[0, :])
    draw_attribute(ax_overview, data, data.extent, font_prop, show_y=True)
    set_panel_title(ax_overview, "正式平面断层属性与典型处理难点", font_prop)
    add_panel_label(ax_overview, "a", font_prop)
    roman = ["I", "II", "III"]
    for label, view, color in zip(roman, views, colors_local):
        ax_overview.add_patch(
            plt.Rectangle(
                (view[0], view[2]),
                view[1] - view[0],
                view[3] - view[2],
                fill=False,
                edgecolor=color,
                linewidth=1.45,
                zorder=25,
            )
        )
        ax_overview.text(
            view[0],
            view[3],
            f" {label} ",
            ha="left",
            va="bottom",
            fontproperties=font_prop,
            fontsize=8.2,
            fontweight="bold",
            color="white",
            bbox={"boxstyle": "round,pad=0.18", "facecolor": color, "edgecolor": "white", "linewidth": 0.45},
            zorder=28,
        )

    for index, (row, title, view, color) in enumerate(zip(rows, titles, views, colors_local)):
        ax = fig.add_subplot(gs[1, index])
        draw_attribute(ax, data, view, font_prop, show_y=index == 0)
        draw_candidates(
            ax,
            data,
            view,
            highlight_ids={int(row.candidate_id)},
            only_highlighted=True,
        )
        path = row.path_world
        if path.size:
            ax.scatter(
                [path[0, 0], path[-1, 0]],
                [path[0, 1], path[-1, 1]],
                s=28,
                facecolor=color,
                edgecolor="white",
                linewidth=0.8,
                zorder=22,
            )
        set_panel_title(ax, f"{roman[index]}  {title}", font_prop)
        add_panel_label(ax, chr(ord("b") + index), font_prop)
        ax.text(
            0.97,
            0.04,
            "实线：接受候选" if bool(int(row.accepted)) else "虚线：回退候选",
            transform=ax.transAxes,
            ha="right",
            va="bottom",
            fontproperties=font_prop,
            fontsize=7.8,
            color="white",
            bbox={"boxstyle": "round,pad=0.25", "facecolor": "#111820", "edgecolor": color, "alpha": 0.90},
            zorder=30,
        )

    save_figure(fig, out_dir, "P02_正式属性与典型难点", dpi)


def render_p03(data: FigureData, font_prop: object, out_dir: Path, dpi: int) -> None:
    # A compact vertical crop allows four large, physically undistorted panels
    # on a 16:9 slide while retaining the accepted and rejected path examples.
    view = data.view_from_grid(560, 770, 335, 445)
    accepted = candidate_row(data, ACCEPTED_CANDIDATE_ID)
    rejected = candidate_row(data, REJECTED_CANDIDATE_ID)
    fig = plt.figure(figsize=(16, 9), constrained_layout=False)
    gs = GridSpec(2, 2, figure=fig, hspace=0.23, wspace=0.10, left=0.055, right=0.98, top=0.94, bottom=0.075)
    axes = [fig.add_subplot(gs[index // 2, index % 2]) for index in range(4)]

    image = draw_attribute(axes[0], data, view, font_prop, show_y=True)
    set_panel_title(axes[0], "原始平面断层属性", font_prop)
    add_stage_badge(axes[0], 1, "属性响应", COLORS["navy"], font_prop)

    axes[1].set_facecolor(COLORS["plate"])
    style_map_axis(axes[1], view, font_prop, dark=True, show_y=True)
    draw_response_layers(axes[1], data)
    draw_branches(axes[1], data, view, endpoints=False, pruned=False)
    set_panel_title(axes[1], "强弱响应分层与方向骨架", font_prop)
    add_stage_badge(axes[1], 2, "响应分层", "#B0871B", font_prop)
    axes[1].text(
        0.98,
        0.035,
        "强响应  ≥ 0.95     弱响应  0.66–0.95     保留骨架",
        transform=axes[1].transAxes,
        ha="right",
        va="bottom",
        fontproperties=font_prop,
        fontsize=8.5,
        color="white",
        bbox={"boxstyle": "round,pad=0.28", "facecolor": "#111820", "edgecolor": "#718493", "alpha": 0.90},
        zorder=35,
    )

    draw_attribute(axes[2], data, view, font_prop, show_y=True, cmap="gray")
    draw_branches(axes[2], data, view, endpoints=False, pruned=True)
    draw_candidates(
        axes[2],
        data,
        view,
        highlight_ids={ACCEPTED_CANDIDATE_ID, REJECTED_CANDIDATE_ID},
        only_highlighted=True,
    )
    add_candidate_label(axes[2], accepted, "接受：方向与属性连续", COLORS["teal"], font_prop)
    add_candidate_label(axes[2], rejected, "拒绝：弱支撑中断", COLORS["orange"], font_prop)
    set_panel_title(axes[2], "分支归属与弱路径门控", font_prop)
    add_stage_badge(axes[2], 3, "候选判断", COLORS["orange"], font_prop)

    axes[3].set_facecolor("#F2F6F7")
    style_map_axis(axes[3], view, font_prop, dark=False, show_y=True)
    draw_polygons(axes[3], data, view, fill=True, color=COLORS["navy"], fill_alpha=0.34)
    set_panel_title(axes[3], "闭合多边形矢量结果", font_prop)
    add_stage_badge(axes[3], 4, "矢量输出", COLORS["teal"], font_prop)
    axes[3].text(
        0.98,
        0.035,
        "闭合边界  ·  面积筛选  ·  拓扑检查",
        transform=axes[3].transAxes,
        ha="right",
        va="bottom",
        fontproperties=font_prop,
        fontsize=9.0,
        fontweight="bold",
        color=COLORS["navy"],
        bbox={"boxstyle": "round,pad=0.28", "facecolor": "white", "edgecolor": "#AEBCC5", "alpha": 0.94},
        zorder=35,
    )

    for label, ax in zip("abcd", axes):
        add_panel_label(ax, label, font_prop, dark=False)

    cax = axes[0].inset_axes([0.18, 0.055, 0.58, 0.028])
    cb = fig.colorbar(image, cax=cax, orientation="horizontal")
    cb.ax.tick_params(labelsize=7.2, colors="white", length=1.8)
    cb.outline.set_edgecolor("#8799A7")
    save_figure(fig, out_dir, "P03_真实数据自动追踪总体流程", dpi)


def render_p04(data: FigureData, font_prop: object, out_dir: Path, dpi: int) -> None:
    row = candidate_row(data, ACCEPTED_CANDIDATE_ID)
    view = candidate_view(data, row, pad_x=82, pad_y=62)
    fig = plt.figure(figsize=(16, 9))
    gs = GridSpec(2, 3, figure=fig, height_ratios=[1.45, 1.0], hspace=0.18, wspace=0.10, left=0.05, right=0.98, top=0.92, bottom=0.085)
    top = [fig.add_subplot(gs[0, index]) for index in range(3)]

    draw_attribute(top[0], data, view, font_prop, show_y=True)
    draw_candidates(top[0], data, view, highlight_ids={ACCEPTED_CANDIDATE_ID})
    set_panel_title(top[0], "原始属性与弱响应缺口", font_prop)

    draw_attribute(top[1], data, view, font_prop, show_y=False)
    draw_response_layers(top[1], data)
    draw_branches(top[1], data, view, endpoints=True, pruned=False)
    set_panel_title(top[1], "双阈值响应与方向骨架", font_prop)

    draw_attribute(top[2], data, view, font_prop, show_y=False)
    draw_branches(top[2], data, view, endpoints=False, pruned=False)
    draw_candidates(top[2], data, view, highlight_ids={ACCEPTED_CANDIDATE_ID})
    draw_polygons(top[2], data, view, fill=True, color="#F06B4F")
    set_panel_title(top[2], "局部宽度恢复与闭合边界", font_prop)
    for label, ax in zip("abc", top):
        add_panel_label(ax, label, font_prop)

    ax_profile = fig.add_subplot(gs[1, 0])
    draw_path_profile(ax_profile, data, row, font_prop)
    ax_profile.set_title("路径属性支撑", fontproperties=font_prop, fontsize=10.5, color=COLORS["ink"], pad=6, fontweight="bold")
    add_panel_label(ax_profile, "d", font_prop)

    ax_width = fig.add_subplot(gs[1, 1])
    distance, _, xy = path_samples(data, row)
    weak_mask = np.isfinite(data.values) & (data.values >= WEAK_THRESHOLD)
    half_width = distance_transform_edt(weak_mask, sampling=(data.dy, data.dx))
    width = 2.0 * half_width[xy[:, 1], xy[:, 0]] if len(xy) else np.array([])
    ax_width.plot(distance, width, color=COLORS["teal"], linewidth=2.0, marker="s", markersize=3.1)
    ax_width.fill_between(distance, 0, width, color=COLORS["teal"], alpha=0.14)
    ax_width.set_xlabel("沿候选路径距离 / m", fontproperties=font_prop, fontsize=8.5)
    ax_width.set_ylabel("局部响应带宽 / m", fontproperties=font_prop, fontsize=8.5)
    ax_width.tick_params(labelsize=7.5, colors=COLORS["muted"])
    ax_width.spines[["top", "right"]].set_visible(False)
    ax_width.grid(axis="y", color="#E3E8EC", linewidth=0.6)
    ax_width.set_title("局部宽度估计", fontproperties=font_prop, fontsize=10.5, color=COLORS["ink"], pad=6, fontweight="bold")
    add_panel_label(ax_width, "e", font_prop)

    ax_gate = fig.add_subplot(gs[1, 2])
    gate_rows = [
        ("属性支撑率", f"{row.support_ratio:.2f} ≥ 0.65", "pass"),
        ("平均支撑", f"{row.mean_support:.2f} ≥ 0.28", "pass"),
        ("连续缺口", f"{int(row.max_unsupported_run)} ≤ 5", "pass"),
        ("最大转角", f"{row.max_turn_angle:.1f}° ≤ 50°", "pass"),
        ("路径绕行比", f"{row.path_length_ratio:.2f} ≤ 1.45", "pass"),
    ]
    draw_metric_cards(ax_gate, gate_rows, font_prop)
    ax_gate.set_title("弱路径接受条件", fontproperties=font_prop, fontsize=10.5, color=COLORS["ink"], pad=6, fontweight="bold")
    add_panel_label(ax_gate, "f", font_prop)
    save_figure(fig, out_dir, "P04_真实数据强弱响应协同提取", dpi)


def draw_candidate_detail(
    ax: plt.Axes,
    data: FigureData,
    row: pd.Series,
    font_prop: object,
    title: str,
    accepted: bool,
) -> None:
    view = candidate_view(data, row, pad_x=68, pad_y=52)
    draw_attribute(ax, data, view, font_prop, show_y=True)
    draw_branches(ax, data, view, endpoints=True, pruned=True)
    draw_candidates(ax, data, view, highlight_ids={int(row.candidate_id)})
    draw_polygons(ax, data, view, fill=False, color="#E8EEF2", alpha=0.72)
    set_panel_title(ax, title, font_prop)
    color = COLORS["teal"] if accepted else COLORS["orange"]
    label = "接受" if accepted else "拒绝"
    ax.text(
        0.025,
        0.965,
        label,
        transform=ax.transAxes,
        ha="left",
        va="top",
        fontproperties=font_prop,
        fontsize=10,
        fontweight="bold",
        color="white",
        bbox={"boxstyle": "round,pad=0.28", "facecolor": color, "edgecolor": "none", "alpha": 0.96},
        zorder=30,
    )


def render_p05(data: FigureData, font_prop: object, out_dir: Path, dpi: int) -> None:
    accepted = candidate_row(data, ACCEPTED_CANDIDATE_ID)
    rejected = candidate_row(data, REJECTED_CANDIDATE_ID)
    fig = plt.figure(figsize=(16, 9))
    gs = GridSpec(2, 2, figure=fig, height_ratios=[2.05, 1.0], hspace=0.16, wspace=0.09, left=0.055, right=0.98, top=0.92, bottom=0.085)
    ax_acc = fig.add_subplot(gs[0, 0])
    ax_rej = fig.add_subplot(gs[0, 1])
    draw_candidate_detail(ax_acc, data, accepted, font_prop, "方向连续且属性支撑充分", True)
    draw_candidate_detail(ax_rej, data, rejected, font_prop, "连续缺口与曲率约束不满足", False)
    add_panel_label(ax_acc, "a", font_prop)
    add_panel_label(ax_rej, "b", font_prop)

    metrics = [
        ("属性支撑率", "support_ratio", 0.65, "higher", "{:.2f}"),
        ("最大连续缺口 / 格", "max_unsupported_run", 5.0, "lower", "{:.0f}"),
        ("最大转角 / °", "max_turn_angle", 50.0, "lower", "{:.1f}"),
        ("路径绕行比", "path_length_ratio", 1.45, "lower", "{:.2f}"),
    ]
    sub = GridSpecFromSubplotSpec(1, 4, subplot_spec=gs[1, :], wspace=0.30)
    for index, (label, field, threshold, direction, fmt) in enumerate(metrics):
        ax = fig.add_subplot(sub[0, index])
        values = [float(accepted[field]), float(rejected[field])]
        normalized = [value / threshold for value in values]
        bars = ax.barh([1, 0], normalized, height=0.46, color=[COLORS["teal"], COLORS["orange"]], alpha=0.92)
        ax.axvline(1.0, color=COLORS["navy"], linewidth=1.15, linestyle=(0, (3, 2)))
        for bar, value in zip(bars, values):
            ax.text(
                min(bar.get_width(), max(1.18, max(normalized) * 1.08)) + 0.03,
                bar.get_y() + bar.get_height() / 2,
                fmt.format(value),
                va="center",
                ha="left",
                fontproperties=font_prop,
                fontsize=8.2,
                color=COLORS["ink"],
            )
        ax.set_yticks([1, 0], ["接受路径", "拒绝路径"], fontproperties=font_prop, fontsize=8)
        ax.set_xticks([])
        ax.set_xlim(0, max(1.35, max(normalized) * 1.24))
        ax.set_title(label, fontproperties=font_prop, fontsize=9.2, color=COLORS["ink"], pad=7, fontweight="bold")
        ax.spines[:].set_visible(False)
        ax.grid(False)
        note = "越高越优" if direction == "higher" else "越低越优"
        ax.text(1.0, -0.36, f"虚线为门限，{note}", transform=ax.transAxes, ha="right", va="top", fontproperties=font_prop, fontsize=7.1, color=COLORS["muted"])
        add_panel_label(ax, chr(ord("c") + index), font_prop)
    save_figure(fig, out_dir, "P05_真实数据分支归属与防误合并", dpi)


def select_representative_polygon(data: FigureData) -> pd.Series:
    rows = data.polygon_rows.copy()
    rows["bbox_ratio"] = rows["geometry"].map(
        lambda geom: max(
            (geom.bounds[2] - geom.bounds[0]) / max(geom.bounds[3] - geom.bounds[1], 1e-9),
            (geom.bounds[3] - geom.bounds[1]) / max(geom.bounds[2] - geom.bounds[0], 1e-9),
        )
    )
    candidates = rows[(rows["area"] >= rows["area"].quantile(0.45)) & (rows["area"] <= rows["area"].quantile(0.80))]
    return candidates.sort_values(["bbox_ratio", "area"], ascending=[False, False]).iloc[0]


def draw_direction_arrows(ax: plt.Axes, geometry: object, color: str) -> None:
    ring = geometry.exterior if geometry.geom_type == "Polygon" else list(geometry.geoms)[0].exterior
    coords = np.asarray(ring.coords)
    if len(coords) < 12:
        return
    for fraction in (0.16, 0.46, 0.76):
        index = min(len(coords) - 3, max(1, int(fraction * (len(coords) - 1))))
        start = coords[index]
        end = coords[index + 2]
        ax.annotate(
            "",
            xy=end,
            xytext=start,
            arrowprops={"arrowstyle": "-|>", "color": color, "linewidth": 1.2, "mutation_scale": 11},
            zorder=20,
        )


def render_p06(data: FigureData, font_prop: object, out_dir: Path, dpi: int) -> None:
    representative = select_representative_polygon(data)
    geometry = representative.geometry
    gx0, gy0, gx1, gy1 = geometry.bounds
    pad_x = max(120.0, 0.18 * (gx1 - gx0))
    pad_y = max(120.0, 0.25 * (gy1 - gy0))
    local_view = (gx0 - pad_x, gx1 + pad_x, gy0 - pad_y, gy1 + pad_y)
    fig = plt.figure(figsize=(16, 9))
    gs = GridSpec(2, 3, figure=fig, width_ratios=[1.35, 1.35, 0.95], height_ratios=[1.2, 1.0], hspace=0.18, wspace=0.12, left=0.045, right=0.98, top=0.92, bottom=0.07)

    ax_map = fig.add_subplot(gs[:, 0:2])
    draw_attribute(ax_map, data, data.extent, font_prop, show_y=True)
    draw_polygons(ax_map, data, data.extent, fill=False, color="#F16B4D", alpha=0.95)
    set_panel_title(ax_map, "正式结果全图几何检查", font_prop)
    add_panel_label(ax_map, "a", font_prop)
    ax_map.add_patch(
        plt.Rectangle(
            (local_view[0], local_view[2]),
            local_view[1] - local_view[0],
            local_view[3] - local_view[2],
            fill=False,
            edgecolor=COLORS["cyan"],
            linewidth=1.3,
            zorder=25,
        )
    )

    ax_summary = fig.add_subplot(gs[0, 2])
    topology = data.summary["topology"]
    summary_rows = [
        ("闭合多边形", f"{data.summary['keptPolygons']} / {data.summary['keptPolygons']}", "pass"),
        ("开放环", str(topology["openRings"]), "pass"),
        ("自相交", str(topology["selfIntersectingPolygons"]), "pass"),
        ("异常相交", str(topology["polygonIntersections"]), "pass"),
        ("包含或内部重叠", str(topology["polygonContainments"] + topology["polygonInteriorOverlaps"]), "pass"),
        ("退化环与零长度边", str(topology["degenerateRings"] + topology["zeroLengthEdges"]), "pass"),
    ]
    draw_metric_cards(ax_summary, summary_rows, font_prop)
    ax_summary.set_title("逐对象几何诊断", fontproperties=font_prop, fontsize=11, color=COLORS["ink"], pad=6, fontweight="bold")
    add_panel_label(ax_summary, "b", font_prop)

    ax_local = fig.add_subplot(gs[1, 2])
    draw_attribute(ax_local, data, local_view, font_prop, show_y=True)
    draw_polygon_geometry(ax_local, geometry, edgecolor=COLORS["cyan"], facecolor="#58C8C5", linewidth=1.8, alpha=0.20, zorder=14)
    draw_direction_arrows(ax_local, geometry, "white")
    ax_local.text(
        0.03,
        0.05,
        f"面积 {float(representative.area):,.0f} m²\n边界首尾闭合，方向一致",
        transform=ax_local.transAxes,
        ha="left",
        va="bottom",
        fontproperties=font_prop,
        fontsize=8.4,
        color="white",
        bbox={"boxstyle": "round,pad=0.35", "facecolor": "#111820", "edgecolor": "#7190A5", "alpha": 0.88},
        zorder=30,
    )
    set_panel_title(ax_local, "代表性多边形局部复核", font_prop)
    add_panel_label(ax_local, "c", font_prop)
    save_figure(fig, out_dir, "P06_真实结果多边形几何质量检查", dpi)


def render_p08(data: FigureData, font_prop: object, out_dir: Path, dpi: int) -> None:
    """Same-extent comparison of input, vector boundaries and final overlay."""
    fig = plt.figure(figsize=(16, 9))
    gs = GridSpec(
        2,
        2,
        figure=fig,
        hspace=0.22,
        wspace=0.09,
        left=0.05,
        right=0.98,
        top=0.93,
        bottom=0.075,
    )

    axes = [fig.add_subplot(gs[0, 0]), fig.add_subplot(gs[0, 1]), fig.add_subplot(gs[1, 0])]
    draw_attribute(axes[0], data, data.extent, font_prop, show_y=True)
    set_panel_title(axes[0], "原始平面断层属性", font_prop)

    axes[1].set_facecolor("#F2F6F7")
    style_map_axis(axes[1], data.extent, font_prop, dark=False, show_y=False)
    draw_polygons(axes[1], data, data.extent, fill=True, color=COLORS["navy"], alpha=0.98, fill_alpha=0.28)
    set_panel_title(axes[1], "闭合多边形纯矢量结果", font_prop)

    draw_attribute(axes[2], data, data.extent, font_prop, show_y=False)
    draw_polygons(axes[2], data, data.extent, fill=False, color="#F26B4A", alpha=0.98)
    set_panel_title(axes[2], "属性与多边形叠加复核", font_prop)

    for label, ax in zip("abc", axes):
        add_panel_label(ax, label, font_prop)

    ax_metrics = fig.add_subplot(gs[1, 1])
    ax_metrics.set_axis_off()
    ax_metrics.set_xlim(0, 1)
    ax_metrics.set_ylim(0, 1)
    topology = data.summary["topology"]
    metrics = [
        ("网格规模", "1501 × 701", COLORS["navy"]),
        ("候选 / 保留", f"212 / {data.summary['keptPolygons']}", COLORS["teal"]),
        ("强响应覆盖", "99.855%", COLORS["navy"]),
        ("弱响应覆盖", "97.684%", COLORS["teal"]),
        (
            "几何异常",
            str(
                topology["openRings"]
                + topology["selfIntersectingPolygons"]
                + topology["polygonIntersections"]
                + topology["polygonContainments"]
                + topology["polygonInteriorOverlaps"]
            ),
            COLORS["teal"],
        ),
    ]
    card_width = 0.445
    card_height = 0.245
    positions = [(0.03, 0.66), (0.525, 0.66), (0.03, 0.36), (0.525, 0.36), (0.03, 0.06)]
    for index, (label, value, color) in enumerate(metrics):
        x, y = positions[index]
        width = 0.94 if index == len(metrics) - 1 else card_width
        ax_metrics.add_patch(
            FancyBboxPatch(
                (x, y),
                width,
                card_height,
                boxstyle="round,pad=0.012,rounding_size=0.018",
                facecolor="#F7FAFB",
                edgecolor="#D5DEE4",
                linewidth=0.8,
            )
        )
        ax_metrics.text(
            x + 0.018,
            y + card_height * 0.70,
            label,
            ha="left",
            va="center",
            fontproperties=font_prop,
            fontsize=8.2,
            color=COLORS["muted"],
        )
        ax_metrics.text(
            x + 0.018,
            y + card_height * 0.30,
            value,
            ha="left",
            va="center",
            fontproperties=font_prop,
            fontsize=17.5,
            fontweight="bold",
            color=color,
        )
    add_panel_label(ax_metrics, "d", font_prop)
    save_figure(fig, out_dir, "P08_正式输入与结果同尺度对照", dpi)


def draw_summary_metric(ax: plt.Axes, y: float, value: str, label: str, font_prop: object, color: str) -> None:
    ax.text(0.08, y, value, transform=ax.transAxes, ha="left", va="center", fontproperties=font_prop, fontsize=22, fontweight="bold", color=color)
    ax.text(0.08, y - 0.075, label, transform=ax.transAxes, ha="left", va="top", fontproperties=font_prop, fontsize=9, color=COLORS["muted"])


def render_p12(data: FigureData, font_prop: object, out_dir: Path, dpi: int) -> None:
    accepted = candidate_row(data, ACCEPTED_CANDIDATE_ID)
    rejected = candidate_row(data, REJECTED_CANDIDATE_ID)
    ambiguous = candidate_row(data, 14)
    fig = plt.figure(figsize=(16, 9))
    gs = GridSpec(2, 4, figure=fig, width_ratios=[1.0, 1.0, 1.0, 0.82], height_ratios=[2.25, 0.85], hspace=0.14, wspace=0.10, left=0.04, right=0.98, top=0.92, bottom=0.07)

    ax_map = fig.add_subplot(gs[0, 0:3])
    draw_attribute(ax_map, data, data.extent, font_prop, show_y=True)
    draw_polygons(ax_map, data, data.extent, fill=False, color="#F26B4A", alpha=0.95)
    set_panel_title(ax_map, "平面断层属性与自动追踪结果", font_prop)
    add_panel_label(ax_map, "a", font_prop)

    ax_metrics = fig.add_subplot(gs[0, 3])
    ax_metrics.set_axis_off()
    ax_metrics.set_xlim(0, 1)
    ax_metrics.set_ylim(0, 1)
    ax_metrics.add_patch(
        FancyBboxPatch((0.02, 0.03), 0.96, 0.94, boxstyle="round,pad=0.02,rounding_size=0.03", facecolor="#F7F9FA", edgecolor="#D5DEE4", linewidth=0.9)
    )
    draw_summary_metric(ax_metrics, 0.86, "1501 × 701", "平面属性网格", font_prop, COLORS["navy"])
    draw_summary_metric(ax_metrics, 0.66, "212 → 200", "候选生成与面积筛选", font_prop, COLORS["teal"])
    draw_summary_metric(ax_metrics, 0.46, "99.855%", "强响应覆盖率", font_prop, COLORS["navy"])
    draw_summary_metric(ax_metrics, 0.26, "97.684%", "弱响应覆盖率", font_prop, COLORS["teal"])
    ax_metrics.text(0.08, 0.10, "开放环 0  ·  自相交 0\n异常相交 0  ·  内部重叠 0", transform=ax_metrics.transAxes, ha="left", va="center", fontproperties=font_prop, fontsize=9.5, color=COLORS["ink"], linespacing=1.7)
    add_panel_label(ax_metrics, "b", font_prop)

    rows = [
        (accepted, "弱响应连续路径", True),
        (rejected, "低支撑候选自动拒绝", False),
        (ambiguous, "交点归属歧义自动回退", False),
    ]
    for index, (row, title, accepted_state) in enumerate(rows):
        ax = fig.add_subplot(gs[1, index])
        view = candidate_view(data, row, pad_x=54, pad_y=42)
        draw_attribute(ax, data, view, font_prop, show_y=index == 0)
        draw_branches(ax, data, view, endpoints=True, pruned=True)
        draw_candidates(ax, data, view, highlight_ids={int(row.candidate_id)})
        set_panel_title(ax, title, font_prop)
        ax.text(0.03, 0.05, "保留" if accepted_state else "回退", transform=ax.transAxes, ha="left", va="bottom", fontproperties=font_prop, fontsize=8.5, fontweight="bold", color="white", bbox={"boxstyle": "round,pad=0.25", "facecolor": COLORS["teal"] if accepted_state else COLORS["orange"], "edgecolor": "none", "alpha": 0.94})
        add_panel_label(ax, chr(ord("c") + index), font_prop)

    ax_note = fig.add_subplot(gs[1, 3])
    ax_note.set_axis_off()
    ax_note.text(0.05, 0.78, "独立软件", transform=ax_note.transAxes, fontproperties=font_prop, fontsize=12, fontweight="bold", color=COLORS["navy"])
    ax_note.text(0.05, 0.57, "GeoEast 插件", transform=ax_note.transAxes, fontproperties=font_prop, fontsize=12, fontweight="bold", color=COLORS["teal"])
    ax_note.add_patch(FancyArrowPatch((0.10, 0.45), (0.82, 0.45), transform=ax_note.transAxes, arrowstyle="-|>", mutation_scale=16, linewidth=2.0, color=COLORS["slate"]))
    ax_note.text(0.46, 0.32, "共享 C++ 追踪核心", transform=ax_note.transAxes, ha="center", va="center", fontproperties=font_prop, fontsize=9.5, color=COLORS["ink"], fontweight="bold")
    ax_note.text(0.46, 0.14, "统一参数、统一几何、统一质量检查", transform=ax_note.transAxes, ha="center", va="center", fontproperties=font_prop, fontsize=8.2, color=COLORS["muted"])
    add_panel_label(ax_note, "f", font_prop)
    save_figure(fig, out_dir, "P12_真实数据项目总结主视觉", dpi)


def main() -> None:
    parser = argparse.ArgumentParser(description="生成基于正式真实数据的答辩科研图件")
    parser.add_argument("--dat", type=Path, required=True, help="官方平面断层属性 DAT")
    parser.add_argument("--result", type=Path, default=DEFAULT_RESULT)
    parser.add_argument("--branches", type=Path, default=DEFAULT_BRANCHES)
    parser.add_argument("--weak", type=Path, default=DEFAULT_WEAK)
    parser.add_argument("--diagnostics", type=Path, default=DEFAULT_DIAGNOSTICS)
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--dpi", type=int, default=320)
    args = parser.parse_args()
    if not 220 <= args.dpi <= 600:
        raise ValueError("DPI 应位于 220 至 600 之间。")
    for path in (args.dat, args.result, args.branches, args.weak, args.diagnostics, args.summary):
        if not path.exists():
            raise FileNotFoundError(path)
    font_prop = configure_style()
    data = load_data(args)
    if len(data.polygons) != 200:
        raise RuntimeError(f"正式结果应为 200 个保留多边形，当前读取到 {len(data.polygons)} 个。")
    render_p02(data, font_prop, args.out_dir, args.dpi)
    render_p03(data, font_prop, args.out_dir, args.dpi)
    render_p04(data, font_prop, args.out_dir, args.dpi)
    render_p05(data, font_prop, args.out_dir, args.dpi)
    render_p06(data, font_prop, args.out_dir, args.dpi)
    render_p08(data, font_prop, args.out_dir, args.dpi)
    render_p12(data, font_prop, args.out_dir, args.dpi)
    for path in sorted(args.out_dir.glob("P*.png")):
        print(path)


if __name__ == "__main__":
    main()
