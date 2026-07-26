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
TIFF_DIR = ROOT / "05_期刊TIFF"

COLORS = {
    "ink": "#20252A",
    "muted": "#5F6870",
    "line": "#A5ADB3",
    "paper": "#FFFFFF",
    "panel": "#F5F6F6",
    "blue": "#2D67A3",
    "blue_dark": "#234E78",
    "blue_light": "#DCE8F2",
    "teal": "#2F8F83",
    "teal_dark": "#236C64",
    "teal_light": "#DDEDEA",
    "amber": "#C9872B",
    "amber_light": "#F4E8D3",
    "red": "#A94A52",
    "red_light": "#F1DDDF",
    "gray": "#D5DADD",
    "gray_dark": "#737D85",
}


def setup_style() -> None:
    mpl.rcParams.update(
        {
            "font.family": "sans-serif",
            "font.sans-serif": ["Microsoft YaHei", "Noto Sans CJK SC", "Arial", "DejaVu Sans"],
            "font.size": 7,
            "axes.labelsize": 7,
            "axes.titlesize": 8,
            "xtick.labelsize": 6,
            "ytick.labelsize": 6,
            "legend.fontsize": 6,
            "axes.linewidth": 0.65,
            "lines.linewidth": 1.0,
            "patch.linewidth": 0.7,
            "axes.spines.top": False,
            "axes.spines.right": False,
            "legend.frameon": False,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
            "svg.fonttype": "none",
            "savefig.facecolor": "white",
            "savefig.transparent": False,
            "figure.facecolor": "white",
            "axes.unicode_minus": False,
        }
    )


def canvas(width: float = 7.205, height: float = 4.4):
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
    radius: float = 0.002,
    lw: float = 0.7,
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
    ax.add_patch(Rectangle((x, y), w, h, facecolor="none", edgecolor=COLORS[edge], linewidth=0.6))
    ax.text(x + 0.012, y + h - 0.024, title, ha="left", va="top", fontsize=6.2, weight="bold", color=COLORS["muted"])


def panel_title(ax, label: str, title: str, *, x: float = 0.0, y: float = 1.0) -> None:
    """Nature-style panel label followed by a short direct title."""
    ax.text(x, y, label, transform=ax.transAxes, ha="left", va="top", fontsize=8, weight="bold", color=COLORS["ink"])
    ax.text(x + 0.055, y, title, transform=ax.transAxes, ha="left", va="top", fontsize=7.2, weight="bold", color=COLORS["ink"])


def rule(ax, start: tuple[float, float], end: tuple[float, float], *, color: str = "line", lw: float = 0.7,
         dashed: bool = False, zorder: int = 1) -> None:
    ax.plot([start[0], end[0]], [start[1], end[1]], color=COLORS.get(color, color), lw=lw,
            ls="--" if dashed else "-", solid_capstyle="butt", zorder=zorder)


def label_box(ax, xy: tuple[float, float], text: str, *, color: str = "ink", fill: str = "paper",
              fontsize: float = 6.3, pad: float = 0.16, weight: str = "normal", ha: str = "center"):
    """Compact scientific annotation, intentionally not a UI-style card."""
    return ax.text(
        xy[0], xy[1], text, ha=ha, va="center", fontsize=fontsize, weight=weight,
        color=COLORS.get(color, color), linespacing=1.25,
        bbox={"boxstyle": f"square,pad={pad}", "fc": COLORS.get(fill, fill),
              "ec": COLORS.get(color, color), "lw": 0.6}, zorder=4,
    )


def phase_header(ax, x0: float, x1: float, y: float, label: str, *, color: str = "blue") -> None:
    rule(ax, (x0, y), (x1, y), color=color, lw=1.6)
    ax.text(x0, y + 0.024, label, ha="left", va="bottom", fontsize=6.4, weight="bold",
            color=COLORS.get(color, color))


def panel_label(ax, label: str, x: float = 0.0, y: float = 1.0):
    ax.text(x, y, label, transform=ax.transAxes, ha="left", va="top", fontsize=8, weight="bold", color=COLORS["ink"])


def save_all(fig, stem: str, *, dpi: int = 600, close: bool = True) -> None:
    VECTOR_DIR.mkdir(parents=True, exist_ok=True)
    PNG_DIR.mkdir(parents=True, exist_ok=True)
    QA_DIR.mkdir(parents=True, exist_ok=True)
    TIFF_DIR.mkdir(parents=True, exist_ok=True)
    svg = VECTOR_DIR / f"{stem}.svg"
    pdf = VECTOR_DIR / f"{stem}.pdf"
    png = PNG_DIR / f"{stem}.png"
    tiff = TIFF_DIR / f"{stem}.tiff"
    fig.savefig(svg, bbox_inches="tight", pad_inches=0.035)
    fig.savefig(pdf, bbox_inches="tight", pad_inches=0.035)
    fig.savefig(png, dpi=dpi, bbox_inches="tight", pad_inches=0.035)
    fig.savefig(tiff, dpi=dpi, bbox_inches="tight", pad_inches=0.035, pil_kwargs={"compression": "tiff_lzw"})
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
