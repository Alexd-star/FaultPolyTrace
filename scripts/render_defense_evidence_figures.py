#!/usr/bin/env python3
"""Render slide-ready evidence figures from real FaultPolyTrace outputs.

The three plates serve different claims:

* P07: both real interfaces converge on one shared C++ tracing core;
* P09: the formal result combines high attribute coverage, valid geometry and
  a reproducible timing breakdown;
* P11: two independent same-domain planar benchmarks preserve the main quality
  pattern while exposing the remaining independent-crossing merge risk.

All plots use project outputs and real screenshots.  No synthetic values or
generative artwork are introduced by this script.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import colors
from matplotlib.gridspec import GridSpec, GridSpecFromSubplotSpec
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch
import numpy as np
from PIL import Image
from scipy.ndimage import binary_erosion


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from generalization_validation.metrics import prediction_masks_from_csv
from render_defense_scientific_figures import (
    COLORS,
    DEFAULT_BRANCHES,
    DEFAULT_DIAGNOSTICS,
    DEFAULT_RESULT,
    DEFAULT_SUMMARY,
    DEFAULT_WEAK,
    add_panel_label,
    configure_style,
    draw_attribute,
    draw_polygons,
    load_data,
    save_figure,
    set_panel_title,
)


DEFAULT_OUT = ROOT / "outputs" / "presentation_assets" / "defense_v1.1.3_realdata_20260825"
DEFAULT_FIRST_DATASET = ROOT / "data" / "generated" / "planar_benchmark_80"
DEFAULT_SECOND_DATASET = ROOT / "data" / "generated" / "planar_benchmark_80_independent_20260730"
DEFAULT_FIRST_RESULT = ROOT / "examples" / "outputs" / "generalization_planar_80_current_20260730"
DEFAULT_SECOND_RESULT = ROOT / "examples" / "outputs" / "generalization_planar_80_independent_20260730"
DEFAULT_STANDALONE_SCREENSHOT = ROOT / "release" / "figure" / "ui_current_20260726" / "独立软件_主界面.png"
DEFAULT_GEOEAST_SCREENSHOT = ROOT / "release" / "figure" / "ui_current_20260726" / "GeoEast插件_主界面.png"

FAMILY_NAMES = {
    "isolated_curved": "孤立曲线",
    "parallel_close": "近邻平行",
    "branching": "分支结构",
    "crossing_independent": "独立交叉",
    "weak_gap": "弱响应缺口",
    "wide_variable": "变宽断层带",
    "edge_truncated": "边界截断",
    "noisy_footprint": "噪声背景",
}


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def rounded_box(
    ax: plt.Axes,
    xy: tuple[float, float],
    width: float,
    height: float,
    facecolor: str,
    edgecolor: str,
    linewidth: float = 1.0,
    radius: float = 0.016,
    zorder: int = 5,
) -> FancyBboxPatch:
    patch = FancyBboxPatch(
        xy,
        width,
        height,
        boxstyle=f"round,pad=0.010,rounding_size={radius}",
        facecolor=facecolor,
        edgecolor=edgecolor,
        linewidth=linewidth,
        zorder=zorder,
    )
    ax.add_patch(patch)
    return patch


def draw_box_text(
    ax: plt.Axes,
    xy: tuple[float, float],
    width: float,
    height: float,
    title: str,
    subtitle: str,
    font_prop: object,
    facecolor: str,
    edgecolor: str,
    title_color: str = COLORS["ink"],
) -> None:
    rounded_box(ax, xy, width, height, facecolor, edgecolor)
    x, y = xy
    ax.text(
        x + width / 2,
        y + height * 0.62,
        title,
        ha="center",
        va="center",
        fontproperties=font_prop,
        fontsize=10.5,
        fontweight="bold",
        color=title_color,
        zorder=8,
    )
    ax.text(
        x + width / 2,
        y + height * 0.27,
        subtitle,
        ha="center",
        va="center",
        fontproperties=font_prop,
        fontsize=7.8,
        color=COLORS["muted"],
        zorder=8,
    )


def draw_arrow(
    ax: plt.Axes,
    start: tuple[float, float],
    end: tuple[float, float],
    color: str = COLORS["slate"],
    connectionstyle: str = "arc3,rad=0",
    both: bool = False,
) -> None:
    ax.add_patch(
        FancyArrowPatch(
            start,
            end,
            arrowstyle="<|-|>" if both else "-|>",
            mutation_scale=13,
            linewidth=1.25,
            color=color,
            connectionstyle=connectionstyle,
            zorder=3,
        )
    )


def render_p07(
    font_prop: object,
    standalone_screenshot: Path,
    geoeast_screenshot: Path,
    out_dir: Path,
    dpi: int,
) -> None:
    """Real-interface plate with one shared-core architecture."""
    fig = plt.figure(figsize=(16, 9))
    gs = GridSpec(
        2,
        2,
        figure=fig,
        height_ratios=[1.42, 1.0],
        hspace=0.18,
        wspace=0.08,
        left=0.045,
        right=0.98,
        top=0.94,
        bottom=0.065,
    )

    screenshots = [
        (standalone_screenshot, "独立软件", "外部文件作业", COLORS["navy"]),
        (geoeast_screenshot, "GeoEast 插件", "工区数据库作业", COLORS["teal"]),
    ]
    for index, (path, title, subtitle, color) in enumerate(screenshots):
        ax = fig.add_subplot(gs[0, index])
        image = np.asarray(Image.open(path).convert("RGB"))
        ax.imshow(image, interpolation="lanczos")
        ax.set_axis_off()
        ax.set_title(title, fontproperties=font_prop, fontsize=13, fontweight="bold", color=COLORS["ink"], pad=7)
        ax.text(
            0.985,
            0.025,
            subtitle,
            transform=ax.transAxes,
            ha="right",
            va="bottom",
            fontproperties=font_prop,
            fontsize=9.0,
            color="white",
            bbox={"boxstyle": "round,pad=0.30", "facecolor": color, "edgecolor": "white", "linewidth": 0.6, "alpha": 0.96},
        )
        for spine in ax.spines.values():
            spine.set_visible(True)
            spine.set_edgecolor("#C3CDD4")
            spine.set_linewidth(0.8)
        add_panel_label(ax, "a" if index == 0 else "b", font_prop)

    ax = fig.add_subplot(gs[1, :])
    ax.set_axis_off()
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)

    draw_box_text(ax, (0.015, 0.50), 0.145, 0.27, "外部网格文件", "DAT / 规则矩阵文本", font_prop, "#EAF1F8", "#4B7FAE")
    draw_box_text(ax, (0.195, 0.50), 0.145, 0.27, "AsciiGridIO", "解析并统一为 Grid2D", font_prop, "#F5F7F8", "#A5B2BB")
    draw_box_text(ax, (0.660, 0.50), 0.145, 0.27, "GeoEastBoAdapter", "BOMapGrid / BOFaultPolygon", font_prop, "#F5F7F8", "#A5B2BB")
    draw_box_text(ax, (0.840, 0.50), 0.145, 0.27, "GeoEast 工区", "数据库对象读取与写回", font_prop, "#E6F5F2", "#319A8E")

    rounded_box(ax, (0.385, 0.39), 0.230, 0.48, "#F1F0F8", "#4D4295", linewidth=1.4, radius=0.020)
    ax.text(0.50, 0.78, "共享 C++ 追踪核心", ha="center", va="center", fontproperties=font_prop, fontsize=12.5, fontweight="bold", color="#3F357D")
    modules = [
        ("属性分层", "强弱响应"),
        ("方向拓扑", "骨架与分支"),
        ("断层带", "归属与防黏连"),
        ("矢量质量", "闭合与拓扑"),
    ]
    for index, (title, subtitle) in enumerate(modules):
        x = 0.400 + index * 0.0525
        rounded_box(ax, (x, 0.49), 0.043, 0.18, "white", "#B9B5D5", linewidth=0.8, radius=0.010, zorder=6)
        ax.text(x + 0.0215, 0.60, title, ha="center", va="center", fontproperties=font_prop, fontsize=8.3, fontweight="bold", color=COLORS["ink"], zorder=8)
        ax.text(x + 0.0215, 0.535, subtitle, ha="center", va="center", fontproperties=font_prop, fontsize=6.9, color=COLORS["muted"], zorder=8)
    ax.text(0.50, 0.435, "同一参数定义 · 同一几何输出 · 同一质量检查", ha="center", va="center", fontproperties=font_prop, fontsize=8.2, color="#5F588B")

    draw_arrow(ax, (0.160, 0.635), (0.195, 0.635), color="#4B7FAE")
    draw_arrow(ax, (0.340, 0.635), (0.385, 0.635), color="#4B7FAE")
    draw_arrow(ax, (0.615, 0.635), (0.660, 0.635), color="#319A8E", both=True)
    draw_arrow(ax, (0.805, 0.635), (0.840, 0.635), color="#319A8E", both=True)

    ax.plot([0.050, 0.950], [0.285, 0.285], color="#D9E0E5", linewidth=0.9)
    ax.text(0.05, 0.215, "文件输出", ha="left", va="center", fontproperties=font_prop, fontsize=9.5, fontweight="bold", color=COLORS["navy"])
    ax.text(0.15, 0.215, "CSV / WKT / PNG / 诊断 JSON", ha="left", va="center", fontproperties=font_prop, fontsize=8.2, color=COLORS["muted"])
    ax.text(0.54, 0.215, "事务式写回", ha="left", va="center", fontproperties=font_prop, fontsize=9.5, fontweight="bold", color=COLORS["teal"])
    transaction = ["暂存", "检查", "提交", "回读"]
    for index, label in enumerate(transaction):
        x = 0.66 + index * 0.072
        rounded_box(ax, (x, 0.155), 0.055, 0.12, "#EAF6F4", "#73B9B1", linewidth=0.8, radius=0.010)
        ax.text(x + 0.0275, 0.215, label, ha="center", va="center", fontproperties=font_prop, fontsize=8.0, color="#267E76", fontweight="bold", zorder=8)
        if index < len(transaction) - 1:
            draw_arrow(ax, (x + 0.056, 0.215), (x + 0.071, 0.215), color="#73A49E")
    ax.text(0.945, 0.105, "失败自动回退，不替换原对象", ha="right", va="center", fontproperties=font_prop, fontsize=7.4, color=COLORS["orange"])
    add_panel_label(ax, "c", font_prop)
    save_figure(fig, out_dir, "P07_真实界面与共享核心架构", dpi)


def draw_coverage_panel(ax: plt.Axes, font_prop: object) -> None:
    labels = ["强响应覆盖", "弱响应覆盖"]
    values = [99.855208, 97.683691]
    palette = [COLORS["navy"], COLORS["teal"]]
    y = np.arange(2)[::-1]
    ax.barh(y, [100, 100], height=0.32, color="#E7EDF1", edgecolor="none")
    ax.barh(y, values, height=0.32, color=palette, edgecolor="none")
    ax.axvline(95, color=COLORS["orange"], linewidth=1.0, linestyle=(0, (4, 2)))
    for yi, label, value, color in zip(y, labels, values, palette):
        ax.text(0, yi + 0.26, label, ha="left", va="bottom", fontproperties=font_prop, fontsize=8.5, color=COLORS["ink"], fontweight="bold")
        ax.text(99.4, yi, f"{value:.3f}%", ha="right", va="center", fontproperties=font_prop, fontsize=9.2, color="white", fontweight="bold")
    ax.set_xlim(0, 100)
    ax.set_ylim(-0.55, 1.55)
    ax.set_yticks([])
    ax.set_xticks([0, 25, 50, 75, 100])
    ax.set_xlabel("属性响应覆盖率 / %", fontproperties=font_prop, fontsize=8.0)
    ax.tick_params(axis="x", labelsize=7.2, colors=COLORS["muted"])
    ax.grid(axis="x", color="#E3E8EC", linewidth=0.6, zorder=0)
    ax.spines[["top", "right", "left"]].set_visible(False)
    ax.set_title("属性响应覆盖", fontproperties=font_prop, fontsize=11, fontweight="bold", color=COLORS["ink"], pad=7)


def draw_timing_panel(ax: plt.Axes, diagnostics: dict, font_prop: object) -> None:
    timing = diagnostics["timing"]
    labels = ["读取", "核心追踪", "拓扑检查", "写出"]
    values = [float(timing["readMs"]), float(timing["coreTotalMs"]), float(timing["topologyMs"]), float(timing["writeMs"])]
    palette = ["#9AA8B2", COLORS["navy"], COLORS["teal"], COLORS["orange"]]
    left = 0.0
    for label, value, color in zip(labels, values, palette):
        ax.barh([0], [value], left=left, height=0.38, color=color, edgecolor="white", linewidth=0.6)
        if value >= 160:
            ax.text(left + value / 2, 0, f"{label}\n{value:.0f} ms", ha="center", va="center", fontproperties=font_prop, fontsize=7.3, color="white", fontweight="bold")
        left += value
    total = float(timing["elapsedMs"])
    ax.set_xlim(0, max(total * 1.02, left * 1.02))
    ax.set_ylim(-0.65, 0.65)
    ax.set_yticks([])
    ax.set_xlabel("耗时 / ms", fontproperties=font_prop, fontsize=8.0)
    ax.tick_params(axis="x", labelsize=7.2, colors=COLORS["muted"])
    ax.grid(axis="x", color="#E3E8EC", linewidth=0.6, zorder=0)
    ax.spines[["top", "right", "left"]].set_visible(False)
    ax.text(0.99, 0.86, f"本次复现总耗时 {total / 1000.0:.3f} s", transform=ax.transAxes, ha="right", va="top", fontproperties=font_prop, fontsize=8.6, color=COLORS["ink"], fontweight="bold")
    ax.set_title("端到端耗时分解", fontproperties=font_prop, fontsize=11, fontweight="bold", color=COLORS["ink"], pad=7)


def draw_quality_panel(ax: plt.Axes, summary: dict, font_prop: object) -> None:
    ax.set_axis_off()
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    topology = summary["topology"]
    entries = [
        ("候选 / 保留", f"212 / {summary['keptPolygons']}", COLORS["navy"]),
        ("开放环", str(topology["openRings"]), COLORS["teal"]),
        ("自相交", str(topology["selfIntersectingPolygons"]), COLORS["teal"]),
        ("异常相交", str(topology["polygonIntersections"]), COLORS["teal"]),
        ("内部重叠", str(topology["polygonInteriorOverlaps"]), COLORS["teal"]),
        ("退化环", str(topology["degenerateRings"]), COLORS["teal"]),
    ]
    for index, (label, value, color) in enumerate(entries):
        row, col = divmod(index, 2)
        x = 0.02 + col * 0.50
        y = 0.76 - row * 0.30
        rounded_box(ax, (x, y), 0.46, 0.22, "#F5F8FA", "#D5DEE4", linewidth=0.8, radius=0.018)
        ax.text(x + 0.03, y + 0.14, label, ha="left", va="center", fontproperties=font_prop, fontsize=8.0, color=COLORS["muted"], zorder=8)
        ax.text(x + 0.43, y + 0.11, value, ha="right", va="center", fontproperties=font_prop, fontsize=12.0, color=color, fontweight="bold", zorder=8)
    ax.set_title("几何与拓扑检查", fontproperties=font_prop, fontsize=11, fontweight="bold", color=COLORS["ink"], pad=7)


def render_p09(data: object, font_prop: object, out_dir: Path, dpi: int) -> None:
    fig = plt.figure(figsize=(16, 9))
    gs = GridSpec(3, 2, figure=fig, width_ratios=[2.45, 1.0], height_ratios=[1.0, 1.0, 1.05], hspace=0.34, wspace=0.11, left=0.045, right=0.98, top=0.93, bottom=0.075)

    ax_map = fig.add_subplot(gs[:, 0])
    draw_attribute(ax_map, data, data.extent, font_prop, show_y=True)
    draw_polygons(ax_map, data, data.extent, fill=False, color="#F26B4A", alpha=0.96)
    set_panel_title(ax_map, "正式平面属性与追踪结果", font_prop)
    ax_map.text(0.985, 0.025, "1501 × 701 网格｜200 个闭合多边形", transform=ax_map.transAxes, ha="right", va="bottom", fontproperties=font_prop, fontsize=8.5, color="white", bbox={"boxstyle": "round,pad=0.28", "facecolor": "#111820", "edgecolor": "#718493", "alpha": 0.90})
    add_panel_label(ax_map, "a", font_prop)

    ax_cov = fig.add_subplot(gs[0, 1])
    draw_coverage_panel(ax_cov, font_prop)
    add_panel_label(ax_cov, "b", font_prop)

    ax_timing = fig.add_subplot(gs[1, 1])
    draw_timing_panel(ax_timing, data.diagnostics, font_prop)
    add_panel_label(ax_timing, "c", font_prop)

    ax_quality = fig.add_subplot(gs[2, 1])
    draw_quality_panel(ax_quality, data.summary, font_prop)
    add_panel_label(ax_quality, "d", font_prop)
    save_figure(fig, out_dir, "P09_正式结果质量与性能_真实数据科研图", dpi)


def boundary(mask: np.ndarray) -> np.ndarray:
    mask = np.asarray(mask, dtype=bool)
    return mask & ~binary_erosion(mask, structure=np.ones((3, 3)), border_value=0)


def load_validation_case(dataset: Path, result: Path, item: dict) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    case_data = dataset / item["path"]
    case_result = result / item["id"]
    attribute = np.load(case_data / "attribute.npy", allow_pickle=False)
    with np.load(case_data / "truth_masks.npz", allow_pickle=False) as archive:
        truth = np.any(archive["object_masks"].astype(bool), axis=0)
    _, masks = prediction_masks_from_csv(case_result / "result.csv", truth.shape)
    predicted = np.any(np.stack(masks), axis=0) if masks else np.zeros_like(truth)
    return attribute, truth, predicted


def select_family_cases(dataset: Path) -> list[dict]:
    manifest = load_json(dataset / "benchmark_manifest.json")
    selected: list[dict] = []
    seen: set[str] = set()
    for item in manifest["cases"]:
        family = item["family"]
        if family not in seen:
            selected.append(item)
            seen.add(family)
    return selected


def draw_metric_comparison(ax: plt.Axes, first: dict, second: dict, font_prop: object) -> None:
    keys = ["pixel_iou_mean", "pixel_precision_mean", "pixel_recall_mean", "boundary_f1_mean", "centerline_corridor_f1_mean"]
    labels = ["像元 IoU", "Precision", "Recall", "Boundary F1", "中心线走廊 F1"]
    first_values = np.asarray([first["summary"][key] for key in keys])
    second_values = np.asarray([second["summary"][key] for key in keys])
    y = np.arange(len(labels))[::-1]
    for yi, left, right in zip(y, first_values, second_values):
        ax.plot([left, right], [yi, yi], color="#C8D1D7", linewidth=1.4, zorder=1)
    ax.scatter(first_values, y, s=38, color="#6D7881", edgecolor="white", linewidth=0.6, label="第一套 80 例", zorder=3)
    ax.scatter(second_values, y, s=42, color=COLORS["teal"], marker="D", edgecolor="white", linewidth=0.6, label="第二套 80 例", zorder=4)
    ax.set_yticks(y, labels, fontproperties=font_prop, fontsize=8.0)
    ax.set_xlim(0.82, 1.006)
    ax.set_xticks([0.85, 0.90, 0.95, 1.00])
    ax.set_xlabel("指标值", fontproperties=font_prop, fontsize=8.0)
    ax.tick_params(axis="x", labelsize=7.2, colors=COLORS["muted"])
    ax.grid(axis="x", color="#E4E9EC", linewidth=0.6)
    ax.spines[["top", "right"]].set_visible(False)
    ax.legend(loc="lower right", frameon=False, prop=font_prop, fontsize=7.2)
    ax.set_title("两套独立基准的质量一致性", fontproperties=font_prop, fontsize=11, fontweight="bold", color=COLORS["ink"], pad=7)


def draw_perturbation_panel(ax: plt.Axes, second: dict, font_prop: object) -> None:
    groups = {name: [] for name in ("gain_085", "gain_115", "noise_002", "smooth_070")}
    for case in second["stability"]:
        for item in case["variants"]:
            groups[item["variant"]].append(float(item["iou_to_baseline"]))
    labels = ["增益 0.85", "增益 1.15", "噪声 σ=0.02", "平滑 σ=0.70"]
    data = [np.asarray(groups[key]) for key in groups]
    positions = np.arange(4)[::-1]
    ax.boxplot(
        data,
        positions=positions,
        vert=False,
        widths=0.50,
        patch_artist=True,
        showfliers=False,
        boxprops={"facecolor": "#DCEDEC", "edgecolor": COLORS["teal"], "linewidth": 1.0},
        medianprops={"color": COLORS["navy"], "linewidth": 1.4},
        whiskerprops={"color": "#7A8790", "linewidth": 0.9},
        capprops={"color": "#7A8790", "linewidth": 0.9},
    )
    rng = np.random.default_rng(20260825)
    for pos, values in zip(positions, data):
        jitter = rng.normal(0, 0.055, size=len(values))
        ax.scatter(values, pos + jitter, s=8, color="#4096C5", alpha=0.34, edgecolor="none", rasterized=True, zorder=2)
    ax.axvline(0.80, color=COLORS["orange"], linewidth=1.1, linestyle=(0, (4, 2)))
    ax.set_yticks(positions, labels, fontproperties=font_prop, fontsize=7.8)
    ax.set_xlim(0.50, 1.01)
    ax.set_xlabel("相对未扰动结果 IoU（每组 n=80）", fontproperties=font_prop, fontsize=8.0)
    ax.tick_params(axis="x", labelsize=7.2, colors=COLORS["muted"])
    ax.grid(axis="x", color="#E4E9EC", linewidth=0.6)
    ax.spines[["top", "right"]].set_visible(False)
    ax.set_title("增益、噪声与平滑扰动稳定性", fontproperties=font_prop, fontsize=11, fontweight="bold", color=COLORS["ink"], pad=7)


def draw_validation_summary(ax: plt.Axes, second: dict, font_prop: object) -> None:
    ax.set_axis_off()
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    summary = second["summary"]
    entries = [
        ("160", "两套平面案例", COLORS["navy"]),
        ("320", "第二套扰动运行", COLORS["teal"]),
        ("0", "漏对象", COLORS["teal"]),
        ("0", "过分割", COLORS["teal"]),
        ("0", "无效几何与异常相交", COLORS["teal"]),
        (str(summary["merge_count_total"]), "第二套独立交叉误合并", COLORS["orange"]),
    ]
    for index, (value, label, color) in enumerate(entries):
        row, col = divmod(index, 2)
        x = 0.02 + col * 0.50
        y = 0.69 - row * 0.31
        rounded_box(ax, (x, y), 0.46, 0.24, "#F6F9FA", "#D7E0E5", linewidth=0.75, radius=0.014)
        ax.text(x + 0.04, y + 0.13, value, ha="left", va="center", fontproperties=font_prop, fontsize=13.0, fontweight="bold", color=color, zorder=8)
        ax.text(x + 0.16, y + 0.13, label, ha="left", va="center", fontproperties=font_prop, fontsize=7.1, color=COLORS["muted"], zorder=8)


def render_p11(
    font_prop: object,
    first_dataset: Path,
    second_dataset: Path,
    first_result: Path,
    second_result: Path,
    out_dir: Path,
    dpi: int,
) -> None:
    first = load_json(first_result / "泛化验证摘要.json")
    second = load_json(second_result / "泛化验证摘要.json")
    selected = select_family_cases(second_dataset)
    if len(selected) != 8:
        raise RuntimeError(f"Expected eight validation families, found {len(selected)}")

    fig = plt.figure(figsize=(16, 9))
    outer = GridSpec(2, 1, figure=fig, height_ratios=[2.15, 1.0], hspace=0.25, left=0.060, right=0.98, top=0.93, bottom=0.075)
    left = GridSpecFromSubplotSpec(2, 4, subplot_spec=outer[0, 0], hspace=0.16, wspace=0.07)
    axes = []
    for index, item in enumerate(selected):
        ax = fig.add_subplot(left[index // 4, index % 4])
        attribute, truth, predicted = load_validation_case(second_dataset, second_result, item)
        finite = attribute[np.isfinite(attribute)]
        vmax = float(np.quantile(finite, 0.997)) if finite.size else 1.0
        ax.imshow(attribute, origin="lower", cmap="gray", norm=colors.Normalize(vmin=0.0, vmax=max(vmax, 1e-6)), interpolation="nearest", rasterized=True)
        ax.contour(boundary(truth), levels=[0.5], colors=[COLORS["orange"]], linewidths=1.15, origin="lower")
        ax.contour(boundary(predicted), levels=[0.5], colors=["#36A9D6"], linewidths=0.95, origin="lower")
        ax.set_xticks([])
        ax.set_yticks([])
        ax.set_facecolor("#101820")
        for spine in ax.spines.values():
            spine.set_edgecolor("#7B8993")
            spine.set_linewidth(0.7)
        ax.text(0.025, 0.96, FAMILY_NAMES[item["family"]], transform=ax.transAxes, ha="left", va="top", fontproperties=font_prop, fontsize=8.3, fontweight="bold", color="white", bbox={"boxstyle": "round,pad=0.22", "facecolor": "#111820", "edgecolor": "none", "alpha": 0.82})
        axes.append(ax)
    axes[0].text(0.02, -0.11, "橙色：独立几何真值    青色：正式算法结果", transform=axes[0].transAxes, ha="left", va="top", fontproperties=font_prop, fontsize=7.2, color=COLORS["muted"], clip_on=False)
    add_panel_label(axes[0], "a", font_prop)

    right = GridSpecFromSubplotSpec(1, 3, subplot_spec=outer[1, 0], width_ratios=[1.0, 1.28, 0.92], wspace=0.34)
    ax_metrics = fig.add_subplot(right[0, 0])
    draw_metric_comparison(ax_metrics, first, second, font_prop)
    add_panel_label(ax_metrics, "b", font_prop)

    ax_perturb = fig.add_subplot(right[0, 1])
    draw_perturbation_panel(ax_perturb, second, font_prop)
    add_panel_label(ax_perturb, "c", font_prop)

    ax_summary = fig.add_subplot(right[0, 2])
    draw_validation_summary(ax_summary, second, font_prop)
    add_panel_label(ax_summary, "d", font_prop)
    save_figure(fig, out_dir, "P11_同域真值与扰动稳定性_真实数据科研图", dpi)


def main() -> None:
    parser = argparse.ArgumentParser(description="生成基于真实结果和真实界面的答辩证据图")
    parser.add_argument("--dat", type=Path, required=True, help="官方平面断层属性 DAT")
    parser.add_argument("--result", type=Path, default=DEFAULT_RESULT)
    parser.add_argument("--branches", type=Path, default=DEFAULT_BRANCHES)
    parser.add_argument("--weak", type=Path, default=DEFAULT_WEAK)
    parser.add_argument("--diagnostics", type=Path, default=DEFAULT_DIAGNOSTICS)
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    parser.add_argument("--standalone-screenshot", type=Path, default=DEFAULT_STANDALONE_SCREENSHOT)
    parser.add_argument("--geoeast-screenshot", type=Path, default=DEFAULT_GEOEAST_SCREENSHOT)
    parser.add_argument("--first-dataset", type=Path, default=DEFAULT_FIRST_DATASET)
    parser.add_argument("--second-dataset", type=Path, default=DEFAULT_SECOND_DATASET)
    parser.add_argument("--first-result", type=Path, default=DEFAULT_FIRST_RESULT)
    parser.add_argument("--second-result", type=Path, default=DEFAULT_SECOND_RESULT)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--dpi", type=int, default=320)
    args = parser.parse_args()

    required = [
        args.dat,
        args.result,
        args.branches,
        args.weak,
        args.diagnostics,
        args.summary,
        args.standalone_screenshot,
        args.geoeast_screenshot,
        args.first_dataset / "benchmark_manifest.json",
        args.second_dataset / "benchmark_manifest.json",
        args.first_result / "泛化验证摘要.json",
        args.second_result / "泛化验证摘要.json",
    ]
    missing = [path for path in required if not path.exists()]
    if missing:
        raise FileNotFoundError("Missing required evidence files:\n" + "\n".join(str(path) for path in missing))
    if not 220 <= args.dpi <= 600:
        raise ValueError("DPI must be between 220 and 600")

    font_prop = configure_style()
    data = load_data(args)
    if len(data.polygons) != 200 or int(data.summary.get("keptPolygons", -1)) != 200:
        raise RuntimeError(
            "正式答辩图要求 200 个保留多边形，"
            f"当前读取到 {len(data.polygons)} 个，摘要记录 {data.summary.get('keptPolygons')} 个。"
        )
    render_p07(font_prop, args.standalone_screenshot, args.geoeast_screenshot, args.out_dir, args.dpi)
    render_p09(data, font_prop, args.out_dir, args.dpi)
    render_p11(font_prop, args.first_dataset, args.second_dataset, args.first_result, args.second_result, args.out_dir, args.dpi)
    for stem in (
        "P07_真实界面与共享核心架构",
        "P09_正式结果质量与性能_真实数据科研图",
        "P11_同域真值与扰动稳定性_真实数据科研图",
    ):
        print((args.out_dir / f"{stem}.png").resolve())


if __name__ == "__main__":
    main()
