from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from PIL import Image

from figure_style import COLORS, save_all, setup_style


REPO = Path(__file__).resolve().parents[3]


def quality_overview() -> None:
    setup_style()
    fig, axes = plt.subplots(1, 3, figsize=(7.2, 2.7), constrained_layout=True)
    cover_names = ["强响应覆盖率", "弱响应覆盖率"]
    cover = [99.845866, 97.149367]
    y = np.arange(2)
    axes[0].hlines(y, 95, cover, color=COLORS["gray"], lw=3)
    axes[0].plot(cover, y, "o", color=COLORS["blue"], ms=5)
    for yi, value in zip(y, cover): axes[0].text(value + 0.12, yi, f"{value:.3f}%", va="center", fontsize=6.5)
    axes[0].set_xlim(95, 100.4); axes[0].set_yticks(y, cover_names); axes[0].set_xlabel("覆盖率（%）"); axes[0].set_title("a  属性覆盖", loc="left", weight="bold")
    axes[0].spines["left"].set_visible(False); axes[0].tick_params(axis="y", length=0)

    topo_names = ["开放环", "自相交", "异常相交"]
    axes[1].barh(np.arange(3), [0, 0, 0], color=COLORS["teal"], edgecolor=COLORS["ink"])
    axes[1].scatter([0,0,0], np.arange(3), color=COLORS["teal"], s=28, zorder=3)
    for yi in range(3): axes[1].text(0.02, yi, "0", va="center", fontsize=7, weight="bold")
    axes[1].set_xlim(-0.03, 0.15); axes[1].set_yticks(np.arange(3), topo_names); axes[1].set_xticks([0]); axes[1].set_xlabel("问题数量"); axes[1].set_title("b  闭合拓扑", loc="left", weight="bold")
    axes[1].spines["left"].set_visible(False); axes[1].tick_params(axis="y", length=0)

    names = ["生成对象", "保留对象", "面积过滤"]
    values = [232, 214, 18]
    colors = [COLORS["gray"], COLORS["teal"], COLORS["amber"]]
    axes[2].barh(np.arange(3), values, color=colors, edgecolor=COLORS["ink"], linewidth=0.6)
    for yi, value in enumerate(values): axes[2].text(value + 4, yi, str(value), va="center", fontsize=6.5)
    axes[2].set_xlim(0, 255); axes[2].set_yticks(np.arange(3), names); axes[2].set_xlabel("多边形数量"); axes[2].set_title("c  对象粒度", loc="left", weight="bold")
    axes[2].spines["left"].set_visible(False); axes[2].tick_params(axis="y", length=0)
    for ax in axes: ax.invert_yaxis(); ax.grid(False)
    save_all(fig, "F16_官方数据质量指标总览")


def performance_breakdown() -> None:
    setup_style()
    fig, ax = plt.subplots(figsize=(7.2, 2.75), constrained_layout=True)
    stages = ["输入读取", "核心追踪", "拓扑检查", "结果写出"]
    values = np.array([536, 630, 173, 400])
    y = np.arange(len(stages))
    ax.barh(y, values, color=[COLORS["gray"], COLORS["blue"], COLORS["teal"], COLORS["gray"]], edgecolor=COLORS["ink"], linewidth=0.6)
    for yi, value in zip(y, values):
        ax.text(value + 14, yi, f"{value} ms", va="center", fontsize=6.5)
    ax.set_yticks(y, stages); ax.invert_yaxis(); ax.set_xlabel("耗时（ms）"); ax.set_xlim(0, 700)
    ax.set_title("输入数据完整运行的分阶段耗时", loc="left", fontsize=8, weight="bold")
    ax.text(0.99, 0.05, "总耗时 1740 ms（1.74 s）\n处理时限 60 s", transform=ax.transAxes, ha="right", va="bottom", fontsize=7, color=COLORS["muted"])
    ax.axvline(630, color=COLORS["blue"], lw=0.7, ls="--")
    ax.grid(False); ax.spines["left"].set_visible(False); ax.tick_params(axis="y", length=0)
    save_all(fig, "F17_官方数据耗时分解")


def verification_matrix() -> None:
    setup_style()
    rows = ["算法一致性检验", "独立软件界面检查", "GeoEast 界面检查", "参数一致性核对", "C++ 核心验证", "安装器运行检查", "插件部署检查"]
    columns = ["算法", "输入输出", "界面", "GeoEast", "发布"]
    matrix = np.array([
        [1,1,1,1,1], [0,1,1,0,1], [0,1,1,1,1], [1,1,1,1,1], [1,0,0,0,0], [1,1,1,0,1], [0,1,1,1,1]
    ])
    notes = ["180 项", "209/209", "135/135", "465/465", "5 项", "正常完成", "组件完整"]
    fig, ax = plt.subplots(figsize=(7.2, 3.5), constrained_layout=True)
    cmap = __import__("matplotlib.colors").colors.ListedColormap(["#F0F2F3", COLORS["blue"]])
    ax.imshow(matrix, cmap=cmap, vmin=0, vmax=1, aspect="auto")
    ax.set_xticks(np.arange(len(columns)), columns); ax.set_yticks(np.arange(len(rows)), rows)
    ax.tick_params(top=True, bottom=False, labeltop=True, labelbottom=False, length=0)
    for i in range(matrix.shape[0]):
        for j in range(matrix.shape[1]):
            ax.text(j, i, "●" if matrix[i,j] else "—", ha="center", va="center",
                    color="white" if matrix[i,j] else COLORS["muted"], fontsize=7)
        ax.text(1.03, i, notes[i], transform=ax.get_yaxis_transform(), ha="left", va="center", fontsize=6.5, color=COLORS["ink"])
    ax.set_title("自动化验证覆盖范围与运行结果", loc="left", fontsize=8, weight="bold", pad=8)
    for spine in ax.spines.values(): spine.set_visible(False)
    save_all(fig, "F18_自动化验证与交付核验矩阵")


def scoring_traceability() -> None:
    setup_style()
    rows = ["GeoEast 正确读写与集成", "追踪正确、边界自然", "面积阈值过滤", "原理、结果与文档", "核心处理小于 1 分钟", "内存合理、运行稳定"]
    cols = ["代码", "自动化验证", "输入数据", "界面图件", "复现资料"]
    matrix = np.array([
        [1,1,1,1,1], [1,1,1,1,1], [1,1,1,1,1], [1,1,1,1,1], [1,1,1,1,1], [1,1,1,0,1]
    ])
    fig, ax = plt.subplots(figsize=(7.2, 3.4), constrained_layout=True)
    cmap = __import__("matplotlib.colors").colors.ListedColormap(["#F0F2F3", COLORS["teal"]])
    ax.imshow(matrix, cmap=cmap, vmin=0, vmax=1, aspect="auto")
    ax.set_xticks(np.arange(len(cols)), cols); ax.set_yticks(np.arange(len(rows)), rows)
    ax.tick_params(top=True, bottom=False, labeltop=True, labelbottom=False, length=0)
    for i in range(matrix.shape[0]):
        for j in range(matrix.shape[1]):
            ax.text(j, i, "●" if matrix[i,j] else "不适用", ha="center", va="center",
                    color="white" if matrix[i,j] else COLORS["muted"], fontsize=5.4, weight="bold" if matrix[i,j] else "normal")
    ax.set_title("功能要求与验证资料对应关系", loc="left", fontsize=8, weight="bold", pad=8)
    for spine in ax.spines.values(): spine.set_visible(False)
    save_all(fig, "F19_功能要求与验证资料矩阵")


def official_evidence_plate() -> None:
    setup_style()
    original = REPO / "examples" / "outputs" / "official" / "figures" / "01_original_fault_attribute.png"
    overlay = REPO / "examples" / "outputs" / "official" / "figures" / "02_faultpolytrace_overlay.png"
    if not original.exists() or not overlay.exists():
        raise FileNotFoundError("请先生成 examples/outputs/official/figures 下的官方数据图件")
    fig, axes = plt.subplots(1, 2, figsize=(7.2, 3.15), constrained_layout=True)
    for ax, path, title in zip(axes, [original, overlay], ["a  输入平面断层属性", "b  保留多边形叠加结果"]):
        ax.imshow(Image.open(path))
        ax.set_title(title, loc="left", fontsize=8, weight="bold")
        ax.axis("off")
    save_all(fig, "F20_官方数据输入与结果对照图")


if __name__ == "__main__":
    quality_overview()
    performance_breakdown()
    verification_matrix()
    scoring_traceability()
    official_evidence_plate()
