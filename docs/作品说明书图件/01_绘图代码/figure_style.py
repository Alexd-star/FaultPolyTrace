from __future__ import annotations

from pathlib import Path
from typing import Iterable, Sequence

import matplotlib as mpl
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch, Rectangle
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
VECTOR_DIR = ROOT / "02_矢量图"
PNG_DIR = ROOT / "03_高清PNG"
QA_DIR = ROOT / "04_灰度与审阅"

COLORS = {
    "ink": "#20272E",
    "muted": "#5C6873",
    "line": "#8C969E",
    "paper": "#FFFFFF",
    "panel": "#F4F6F7",
    "blue": "#0F4D92",
    "blue_light": "#D9E7F5",
    "teal": "#2A9D8F",
    "teal_light": "#D8EFEB",
    "amber": "#D58B2A",
    "amber_light": "#F6E7CC",
    "red": "#B23A48",
    "red_light": "#F4DADD",
    "gray": "#CBD1D6",
}


def setup_style() -> None:
    mpl.rcParams.update(
        {
            "font.family": "sans-serif",
            "font.sans-serif": ["Microsoft YaHei", "SimHei", "Arial", "DejaVu Sans"],
            "font.size": 7,
            "axes.labelsize": 7,
            "axes.titlesize": 8,
            "xtick.labelsize": 6,
            "ytick.labelsize": 6,
            "legend.fontsize": 6,
            "axes.linewidth": 0.7,
            "lines.linewidth": 1.2,
            "patch.linewidth": 0.8,
            "axes.spines.top": False,
            "axes.spines.right": False,
            "legend.frameon": False,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
            "svg.fonttype": "none",
            "savefig.facecolor": "white",
            "figure.facecolor": "white",
            "axes.unicode_minus": False,
        }
    )


def canvas(width: float = 7.2, height: float = 4.4):
    setup_style()
    fig, ax = plt.subplots(figsize=(width, height), constrained_layout=True)
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    return fig, ax


def box(
    ax,
    xy: tuple[float, float],
    wh: tuple[float, float],
    text: str,
    *,
    fill: str = "paper",
    edge: str = "line",
    text_color: str = "ink",
    fontsize: float = 7,
    weight: str = "normal",
    radius: float = 0.012,
    lw: float = 0.8,
    zorder: int = 2,
):
    x, y = xy
    w, h = wh
    patch = FancyBboxPatch(
        (x, y),
        w,
        h,
        boxstyle=f"round,pad=0.006,rounding_size={radius}",
        facecolor=COLORS.get(fill, fill),
        edgecolor=COLORS.get(edge, edge),
        linewidth=lw,
        zorder=zorder,
    )
    ax.add_patch(patch)
    ax.text(
        x + w / 2,
        y + h / 2,
        text,
        ha="center",
        va="center",
        fontsize=fontsize,
        color=COLORS.get(text_color, text_color),
        weight=weight,
        linespacing=1.35,
        zorder=zorder + 1,
    )
    return patch


def arrow(
    ax,
    start: tuple[float, float],
    end: tuple[float, float],
    *,
    color: str = "ink",
    lw: float = 0.9,
    dashed: bool = False,
    label: str | None = None,
    label_offset: tuple[float, float] = (0.0, 0.0),
):
    patch = FancyArrowPatch(
        start,
        end,
        arrowstyle="-|>",
        mutation_scale=8,
        linewidth=lw,
        color=COLORS.get(color, color),
        linestyle="--" if dashed else "-",
        shrinkA=2,
        shrinkB=2,
        zorder=1,
    )
    ax.add_patch(patch)
    if label:
        mx = (start[0] + end[0]) / 2 + label_offset[0]
        my = (start[1] + end[1]) / 2 + label_offset[1]
        ax.text(mx, my, label, ha="center", va="center", fontsize=6, color=COLORS["muted"], backgroundcolor="white")
    return patch


def stage_band(ax, x: float, y: float, w: float, h: float, title: str, *, edge: str = "gray"):
    ax.add_patch(Rectangle((x, y), w, h, facecolor="none", edgecolor=COLORS[edge], linewidth=0.7, linestyle="--"))
    ax.text(x + 0.012, y + h - 0.025, title, ha="left", va="top", fontsize=6.5, weight="bold", color=COLORS["muted"])


def panel_label(ax, label: str, x: float = 0.0, y: float = 1.0):
    ax.text(x, y, label, transform=ax.transAxes, ha="left", va="top", fontsize=8, weight="bold", color=COLORS["ink"])


def save_all(fig, stem: str, *, dpi: int = 300, close: bool = True) -> None:
    VECTOR_DIR.mkdir(parents=True, exist_ok=True)
    PNG_DIR.mkdir(parents=True, exist_ok=True)
    QA_DIR.mkdir(parents=True, exist_ok=True)
    svg = VECTOR_DIR / f"{stem}.svg"
    pdf = VECTOR_DIR / f"{stem}.pdf"
    png = PNG_DIR / f"{stem}.png"
    fig.savefig(svg, bbox_inches="tight", pad_inches=0.035)
    fig.savefig(pdf, bbox_inches="tight", pad_inches=0.035)
    fig.savefig(png, dpi=dpi, bbox_inches="tight", pad_inches=0.035)
    with Image.open(png) as image:
        image.convert("L").save(QA_DIR / f"{stem}_gray.png")
    if close:
        plt.close(fig)


def horizontal_chain(ax, labels: Sequence[str], *, y: float = 0.42, x0: float = 0.04, x1: float = 0.96,
                     box_height: float = 0.16, fills: Iterable[str] | None = None):
    fills_list = list(fills or ["paper"] * len(labels))
    n = len(labels)
    gap = 0.028
    width = (x1 - x0 - gap * (n - 1)) / n
    for i, label in enumerate(labels):
        x = x0 + i * (width + gap)
        box(ax, (x, y), (width, box_height), label, fill=fills_list[i], edge="line")
        if i < n - 1:
            arrow(ax, (x + width, y + box_height / 2), (x + width + gap, y + box_height / 2), color="blue")

