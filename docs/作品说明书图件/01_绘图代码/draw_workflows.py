from __future__ import annotations

import numpy as np
from matplotlib.patches import Circle, Rectangle

from figure_style import COLORS, arrow, canvas, label_box, phase_header, rule, save_all


def _field(nx: int = 180, ny: int = 90) -> np.ndarray:
    x = np.linspace(-1, 1, nx)
    y = np.linspace(-0.55, 0.55, ny)
    xx, yy = np.meshgrid(x, y)
    c = 0.15 * np.sin(3.6 * xx)
    branch = -0.34 * xx + 0.16
    return np.clip(np.exp(-((yy - c) ** 2) / 0.005) + 0.7 * np.exp(-((yy - branch) ** 2) / 0.009), 0, 1)


def _timeline(ax, xs: list[float], y: float, labels: list[str], notes: list[str], colors: list[str]) -> None:
    rule(ax, (xs[0], y), (xs[-1], y), color="gray_dark", lw=0.8)
    for i, (x, label, note, color) in enumerate(zip(xs, labels, notes, colors), 1):
        ax.add_patch(Circle((x, y), 0.020, facecolor=COLORS["paper"], edgecolor=COLORS[color], lw=1.2, zorder=3))
        ax.text(x, y, str(i), ha="center", va="center", fontsize=5.8, weight="bold", color=COLORS[color])
        ax.text(x, y + 0.075, label, ha="center", va="bottom", fontsize=6.4, weight="bold", color=COLORS[color])
        ax.text(x, y - 0.075, note, ha="center", va="top", fontsize=5.5, color=COLORS["muted"], linespacing=1.25)


def standalone_workflow() -> None:
    fig, ax = canvas(7.205, 4.05)
    phase_header(ax, 0.04, 0.29, 0.91, "准备", color="blue")
    phase_header(ax, 0.33, 0.58, 0.91, "计算", color="teal")
    phase_header(ax, 0.62, 0.96, 0.91, "复核与交付", color="blue")
    xs = [0.09, 0.25, 0.41, 0.57, 0.73, 0.91]
    _timeline(
        ax, xs, 0.58,
        ["选择数据", "检查网格", "确认参数", "开始追踪", "查看图层", "保存结果"],
        ["DAT/文本\n或 GeoEast", "坐标、范围\n与属性", "预设模板\n或必要调整", "后台执行\n显示进度", "原始/结果\n自由叠加", "文件、历史\n与图件"],
        ["blue", "blue", "teal", "teal", "blue", "blue"],
    )
    ax.add_patch(Rectangle((0.04, 0.13), 0.92, 0.18, facecolor=COLORS["panel"], edgecolor=COLORS["line"], lw=0.6))
    ax.text(0.06, 0.265, "状态反馈贯穿全流程", fontsize=6.3, weight="bold", color=COLORS["ink"])
    states = [
        (0.08, "未就绪", "指出缺少的输入或配置", "amber"),
        (0.35, "执行中", "进度、阶段与耗时持续更新", "blue"),
        (0.64, "已完成", "对象数量、输出位置与图层可用", "teal"),
    ]
    for x, title, note, color in states:
        ax.add_patch(Circle((x, 0.185), 0.010, facecolor=COLORS[color], edgecolor="none"))
        ax.text(x + 0.025, 0.205, title, fontsize=6.1, weight="bold", color=COLORS[color], ha="left")
        ax.text(x + 0.025, 0.165, note, fontsize=5.5, color=COLORS["muted"], ha="left")
    save_all(fig, "F05_独立软件全流程")


def geoeast_workflow() -> None:
    fig, ax = canvas(7.205, 4.2)
    center = (0.51, 0.52)
    nodes = [
        (0.18, 0.69, "1  打开插件", "宿主传入当前工区"),
        (0.43, 0.80, "2  自动检测", "根目录、用户、执行程序"),
        (0.73, 0.72, "3  刷新对象", "列出 BOMapGrid"),
        (0.79, 0.42, "4  执行追踪", "读取、计算、写入"),
        (0.52, 0.24, "5  重新读取", "刷新 BOFaultPolygon"),
        (0.20, 0.34, "6  显示或管理", "叠加、导出、删除"),
    ]
    for i, (x, y, title, note) in enumerate(nodes):
        color = "teal" if i in (3, 4) else "blue"
        ax.add_patch(Circle((x, y), 0.032, facecolor=COLORS["paper"], edgecolor=COLORS[color], lw=1.2, zorder=3))
        ax.text(x, y - 0.003, str(i + 1), ha="center", va="center", fontsize=6.2, weight="bold", color=COLORS[color])
        ax.text(x, y + 0.067, title[3:], ha="center", fontsize=6.4, weight="bold", color=COLORS[color])
        ax.text(x, y - 0.075, note, ha="center", fontsize=5.4, color=COLORS["muted"])
    for a, b in zip(nodes, nodes[1:] + nodes[:1]):
        arrow(ax, (a[0], a[1]), (b[0], b[1]), color="teal" if a == nodes[3] else "blue", lw=0.8)
    ax.add_patch(Circle(center, 0.105, facecolor=COLORS["panel"], edgecolor=COLORS["ink"], lw=0.7))
    ax.text(center[0], center[1] + 0.025, "GeoEast", ha="center", fontsize=7.3, weight="bold")
    ax.text(center[0], center[1] - 0.025, "数据库作业闭环", ha="center", fontsize=6.0, color=COLORS["muted"])
    label_box(ax, (0.50, 0.08), "完成判据：写入后能够重新读取，并在工区中显示同一对象", color="teal", fontsize=6.2)
    save_all(fig, "F06_GeoEast插件标准流程")


def layer_view_workflow() -> None:
    fig, axes = __import__("matplotlib.pyplot").pyplot.subplots(1, 3, figsize=(7.205, 2.7), constrained_layout=True)
    field = _field()
    mask = field > 0.50
    titles = ["a  原始网格层", "b  结果多边形层", "c  叠加查看"]
    for ax, title in zip(axes, titles):
        ax.set_title(title, loc="left", fontsize=7.2, weight="bold")
        ax.set_xticks([]); ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_visible(True); spine.set_linewidth(0.6); spine.set_color(COLORS["line"])
    axes[0].imshow(field, origin="lower", cmap="Greys", aspect="auto")
    axes[0].text(0.02, 0.04, "开关：显示 / 隐藏", transform=axes[0].transAxes, fontsize=5.7,
                 color=COLORS["blue"], bbox={"fc":"white","ec":"none","alpha":0.85,"pad":1.5})
    axes[1].contour(mask.astype(float), levels=[0.5], colors=[COLORS["teal"]], linewidths=1.1, origin="lower")
    axes[1].text(0.02, 0.04, "颜色、线宽、透明度可调", transform=axes[1].transAxes, fontsize=5.7,
                 color=COLORS["teal_dark"], bbox={"fc":"white","ec":"none","alpha":0.85,"pad":1.5})
    axes[2].imshow(field, origin="lower", cmap="Greys", aspect="auto")
    axes[2].contour(mask.astype(float), levels=[0.5], colors=[COLORS["amber"]], linewidths=1.15, origin="lower")
    axes[2].text(0.02, 0.04, "平移 · 缩放 · 重置 · 导出", transform=axes[2].transAxes, fontsize=5.7,
                 color=COLORS["ink"], bbox={"fc":"white","ec":"none","alpha":0.85,"pad":1.5})
    fig.text(0.5, 0.015, "对象选择决定显示内容；图层开关只控制可见性；显示样式不修改多边形坐标与面积",
             ha="center", fontsize=6.2, color=COLORS["muted"])
    save_all(fig, "F07_结果查看与图层叠加逻辑")


def guidance_states() -> None:
    fig, ax = canvas(7.205, 4.3)
    ax.text(0.04, 0.91, "用户动作", fontsize=6.2, weight="bold", color=COLORS["muted"])
    ax.text(0.31, 0.91, "前置条件判断", fontsize=6.2, weight="bold", color=COLORS["muted"])
    ax.text(0.66, 0.91, "界面反馈与下一步", fontsize=6.2, weight="bold", color=COLORS["muted"])
    rows = [
        (0.74, "开始追踪", "输入已选择且可读？", "执行计算 / 引导选择输入", "blue"),
        (0.56, "刷新数据库对象", "数据库配置与权限完整？", "刷新列表 / 打开配置帮助", "teal"),
        (0.38, "显示结果层", "已选择可用结果？", "立即重绘 / 说明尚无结果", "blue"),
        (0.20, "删除结果", "对象明确且二次确认？", "执行删除 / 取消并保持现状", "red"),
    ]
    for y, action, condition, result, color in rows:
        ax.text(0.05, y, action, fontsize=6.6, weight="bold", color=COLORS[color], va="center")
        arrow(ax, (0.20, y), (0.30, y), color=color, lw=0.8)
        diamond = np.array([[0.44, y + 0.065], [0.58, y], [0.44, y - 0.065], [0.30, y]])
        ax.fill(diamond[:,0], diamond[:,1], facecolor=COLORS["paper"], edgecolor=COLORS[color], lw=0.8)
        ax.text(0.44, y, condition, ha="center", va="center", fontsize=5.5, color=COLORS["ink"])
        arrow(ax, (0.58, y), (0.65, y), color=color, lw=0.8)
        ax.add_patch(Rectangle((0.66, y - 0.055), 0.30, 0.11, facecolor=COLORS["panel"],
                               edgecolor=COLORS["line"], lw=0.55))
        ax.text(0.81, y, result, ha="center", va="center", fontsize=5.8, color=COLORS["ink"])
    label_box(ax, (0.50, 0.075), "提示内容包括缺失条件、操作影响和处理方法", color="amber", fontsize=6.3)
    save_all(fig, "F08_前置条件与错误引导状态")


if __name__ == "__main__":
    standalone_workflow()
    geoeast_workflow()
    layer_view_workflow()
    guidance_states()
