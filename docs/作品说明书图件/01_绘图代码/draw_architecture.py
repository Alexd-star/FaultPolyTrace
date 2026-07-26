from __future__ import annotations

import numpy as np
from matplotlib.patches import Circle, Rectangle

from figure_style import COLORS, arrow, canvas, label_box, phase_header, rule, save_all


def _fault_field(nx: int = 180, ny: int = 96) -> np.ndarray:
    x = np.linspace(-1.0, 1.0, nx)
    y = np.linspace(-0.7, 0.7, ny)
    xx, yy = np.meshgrid(x, y)
    c1 = 0.18 * np.sin(3.0 * xx) - 0.10
    c2 = -0.38 * xx + 0.24
    a = np.exp(-((yy - c1) ** 2) / 0.006)
    b = 0.72 * np.exp(-((yy - c2) ** 2) / 0.008) * (xx > -0.45)
    return np.clip(a + b, 0, 1)


def _schema(ax, x: float, y: float, title: str, fields: list[str], color: str) -> None:
    width, row_h = 0.20, 0.055
    ax.text(x, y + row_h * (len(fields) + 1) + 0.018, title, fontsize=6.8, weight="bold",
            color=COLORS[color], ha="left", va="bottom")
    for i, field in enumerate(fields):
        yy = y + row_h * (len(fields) - i - 1)
        ax.add_patch(Rectangle((x, yy), width, row_h, facecolor=COLORS["paper"],
                               edgecolor=COLORS["line"], linewidth=0.55))
        ax.text(x + 0.012, yy + row_h / 2, field, ha="left", va="center", fontsize=5.9,
                color=COLORS["ink"])


def system_architecture() -> None:
    fig, ax = canvas(7.205, 4.35)
    phase_header(ax, 0.03, 0.24, 0.91, "数据入口", color="blue")
    phase_header(ax, 0.30, 0.68, 0.91, "统一计算主线", color="teal")
    phase_header(ax, 0.75, 0.97, 0.91, "结果交付与复核", color="blue")

    # Two independent product entrances.
    for y, title, sub, color in [
        (0.67, "外部文件", "DAT / 规则矩阵", "blue"),
        (0.34, "GeoEast 工区", "BOMapGrid", "teal"),
    ]:
        ax.add_patch(Rectangle((0.04, y - 0.075), 0.17, 0.15, facecolor=COLORS["paper"],
                               edgecolor=COLORS[color], linewidth=0.9))
        ax.text(0.055, y + 0.020, title, fontsize=7.0, weight="bold", color=COLORS[color], va="center")
        ax.text(0.055, y - 0.028, sub, fontsize=5.9, color=COLORS["muted"], va="center")
        ax.add_patch(Circle((0.222, y), 0.012, facecolor=COLORS[color], edgecolor="none"))

    field = _fault_field()
    ax.imshow(field, extent=(0.32, 0.51, 0.55, 0.78), origin="lower", cmap="Greys", aspect="auto", zorder=0)
    ax.contour(field, levels=[0.52], extent=(0.32, 0.51, 0.55, 0.78), colors=[COLORS["blue"]], linewidths=0.8)
    ax.text(0.32, 0.805, "a  网格解析与响应提取", fontsize=6.4, weight="bold", ha="left")

    x = np.linspace(0.0, 1.0, 150)
    y1 = 0.405 + 0.045 * np.sin(5.2 * x)
    ax.plot(0.33 + 0.17 * x, y1, color=COLORS["blue"], lw=1.6)
    ax.plot(0.39 + 0.10 * x, 0.49 - 0.13 * x, color=COLORS["blue"], lw=1.3)
    ax.scatter([0.33, 0.50, 0.39, 0.49], [y1[0], y1[-1], 0.49, 0.36], s=9,
               facecolor=COLORS["paper"], edgecolor=COLORS["blue"], linewidth=0.7, zorder=3)
    ax.text(0.32, 0.505, "b  骨架图与分支归属", fontsize=6.4, weight="bold", ha="left")

    theta = np.linspace(0, 2 * np.pi, 180)
    rr = 1 + 0.10 * np.sin(3 * theta)
    px = 0.60 + 0.075 * rr * np.cos(theta)
    py = 0.47 + 0.18 * rr * np.sin(theta)
    ax.fill(px, py, facecolor=COLORS["teal_light"], edgecolor=COLORS["teal"], lw=1.0)
    ax.plot([0.54, 0.66], [0.35, 0.59], color=COLORS["teal_dark"], lw=0.9)
    ax.text(0.535, 0.705, "c  断层带重建与闭合矢量化", fontsize=6.4, weight="bold", ha="left")

    arrow(ax, (0.234, 0.67), (0.315, 0.67), color="blue", lw=1.0)
    arrow(ax, (0.234, 0.34), (0.315, 0.61), color="teal", lw=1.0)
    arrow(ax, (0.515, 0.66), (0.545, 0.55), color="teal", lw=1.0)

    rule(ax, (0.71, 0.20), (0.71, 0.80), color="line", lw=0.7)
    outputs = [
        (0.72, "文件成果", "CSV/WKT、诊断 JSON、PNG", "blue"),
        (0.50, "数据库对象", "BOFaultPolygon", "teal"),
        (0.28, "复核资料", "真实图层、历史记录、哈希", "ink"),
    ]
    for y, title, sub, color in outputs:
        ax.add_patch(Circle((0.71, y), 0.010, facecolor=COLORS[color], edgecolor="none", zorder=3))
        ax.text(0.75, y + 0.025, title, fontsize=6.8, weight="bold", color=COLORS[color], ha="left")
        ax.text(0.75, y - 0.025, sub, fontsize=5.8, color=COLORS["muted"], ha="left")
    arrow(ax, (0.675, 0.47), (0.705, 0.50), color="teal")

    ax.text(0.50, 0.075, "两个产品入口只负责环境适配；同一 C++ 核心决定几何、统计与质量结果",
            ha="center", va="center", fontsize=6.8, color=COLORS["ink"])
    save_all(fig, "F01_产品总体架构")


def layered_architecture() -> None:
    fig, ax = canvas(7.205, 4.25)
    labels = [
        ("用户交互层", "独立桌面软件  ·  GeoEast Launcher  ·  菜单/工具栏/真实图层查看", "blue", 0.72),
        ("应用服务层", "任务编排  ·  参数校验  ·  历史记录  ·  结果索引  ·  错误引导", "ink", 0.53),
        ("数据适配层", "AsciiGridIO  ·  DiagnosticsIO  ·  GeoEastBoAdapter", "ink", 0.34),
        ("核心算法层", "Segmentation  ·  SkeletonGraph  ·  FaultTracer  ·  Polygon  ·  Topology", "teal", 0.15),
    ]
    ax.text(0.05, 0.93, "运行时调用方向", fontsize=6.2, color=COLORS["muted"], ha="left")
    arrow(ax, (0.155, 0.86), (0.155, 0.12), color="gray_dark", lw=0.8)
    for idx, (title, modules, color, y) in enumerate(labels):
        fill = "blue_light" if idx == 0 else "teal_light" if idx == 3 else "panel"
        ax.add_patch(Rectangle((0.21, y), 0.73, 0.115, facecolor=COLORS[fill],
                               edgecolor=COLORS[color], linewidth=0.75))
        ax.text(0.235, y + 0.074, title, fontsize=7.0, weight="bold", color=COLORS[color], va="center")
        ax.text(0.405, y + 0.074, modules, fontsize=5.9, color=COLORS["ink"], va="center")
        ax.text(0.235, y + 0.028, ["交互与展示", "业务编排", "格式转换", "唯一计算来源"][idx],
                fontsize=5.6, color=COLORS["muted"], va="center")
        ax.add_patch(Circle((0.155, y + 0.057), 0.012, facecolor=COLORS[color], edgecolor=COLORS["paper"], lw=0.6))
        rule(ax, (0.167, y + 0.057), (0.21, y + 0.057), color=color, lw=0.8)
    ax.text(0.96, 0.07, "依赖方向单向向下，算法与数据语义不在界面层复制", ha="right", fontsize=6.5,
            color=COLORS["muted"])
    save_all(fig, "F02_代码分层与职责")


def decoupled_deployment() -> None:
    fig, ax = canvas(7.205, 4.25)
    phase_header(ax, 0.04, 0.46, 0.91, "独立软件产品边界", color="blue")
    phase_header(ax, 0.54, 0.96, 0.91, "GeoEast 集成产品边界", color="teal")

    def branch(x0: float, color: str, items: list[tuple[str, str]]) -> None:
        rule(ax, (x0 + 0.04, 0.77), (x0 + 0.04, 0.28), color=color, lw=1.2)
        for y, (title, note) in zip([0.73, 0.55, 0.37], items):
            ax.add_patch(Circle((x0 + 0.04, y), 0.011, facecolor=COLORS[color], edgecolor="none"))
            ax.text(x0 + 0.075, y + 0.020, title, fontsize=6.8, weight="bold", color=COLORS[color], ha="left")
            ax.text(x0 + 0.075, y - 0.025, note, fontsize=5.7, color=COLORS["muted"], ha="left")

    branch(0.05, "blue", [
        ("FaultPolyTrace.exe", "独立窗口与本地任务服务"),
        ("客户机运行资源", "Qt/VC、平台插件、帮助与示例"),
        ("安装器与便携包", "配置写入用户目录，不依赖开发机路径"),
    ])
    branch(0.55, "teal", [
        ("GeoEast Ribbon 入口", "由宿主加载并传递当前工区上下文"),
        ("Launcher 与 BO Runner", "数据库选择、任务调度与状态反馈"),
        ("GeoEast SDK 适配", "BOMapGrid 读取与 BOFaultPolygon 写入"),
    ])

    ax.add_patch(Rectangle((0.26, 0.08), 0.48, 0.12, facecolor=COLORS["panel"],
                           edgecolor=COLORS["ink"], linewidth=0.8))
    ax.text(0.50, 0.155, "共享计算资产", fontsize=6.4, color=COLORS["muted"], ha="center")
    ax.text(0.50, 0.115, "C++ 核心  ·  参数模型  ·  多边形输出语义  ·  自动化验证", fontsize=6.8,
            weight="bold", color=COLORS["ink"], ha="center")
    arrow(ax, (0.30, 0.28), (0.40, 0.20), color="blue")
    arrow(ax, (0.70, 0.28), (0.60, 0.20), color="teal")
    ax.text(0.50, 0.95, "部署单元彼此独立，计算能力保持单一来源", fontsize=7.3, weight="bold", ha="center")
    save_all(fig, "F03_独立软件与插件解耦部署")


def data_contracts() -> None:
    fig, ax = canvas(7.205, 4.3)
    _schema(ax, 0.04, 0.48, "外部文件输入", ["nx, ny", "x/y 坐标", "attribute[]"], "blue")
    _schema(ax, 0.04, 0.16, "GeoEast 输入", ["BOMapGrid", "工区/对象名", "数据库上下文"], "teal")

    ax.add_patch(Rectangle((0.37, 0.25), 0.25, 0.48, facecolor=COLORS["panel"],
                           edgecolor=COLORS["ink"], linewidth=0.8))
    ax.text(0.495, 0.675, "内部数据约定", fontsize=7.2, weight="bold", ha="center")
    field = _fault_field(120, 70)
    ax.imshow(field, extent=(0.40, 0.59, 0.39, 0.61), origin="lower", cmap="Greys", aspect="auto")
    ax.text(0.495, 0.345, "Grid2D", fontsize=7.0, weight="bold", ha="center", color=COLORS["blue_dark"])
    ax.text(0.495, 0.305, "统一坐标、尺寸与连续属性", fontsize=5.8, ha="center", color=COLORS["muted"])

    _schema(ax, 0.76, 0.48, "文件输出", ["Polygon/WKT", "面积与属性统计", "诊断/拓扑/图件"], "blue")
    _schema(ax, 0.76, 0.16, "GeoEast 输出", ["BOFaultPolygon", "边界段坐标", "工区对象索引"], "teal")
    arrow(ax, (0.25, 0.61), (0.37, 0.56), color="blue", label="解析", label_offset=(0, 0.035))
    arrow(ax, (0.25, 0.29), (0.37, 0.42), color="teal", label="适配", label_offset=(0, -0.035))
    arrow(ax, (0.62, 0.56), (0.76, 0.61), color="blue", label="序列化", label_offset=(0, 0.035))
    arrow(ax, (0.62, 0.42), (0.76, 0.29), color="teal", label="写入", label_offset=(0, -0.035))
    label_box(ax, (0.50, 0.11), "同一参数模型与同一多边形几何", color="amber", fill="paper", fontsize=6.2)
    ax.text(0.50, 0.92, "不同数据源在 Grid2D 处汇合，不同输出由同一几何结果派生", ha="center",
            fontsize=7.2, weight="bold")
    save_all(fig, "F04_双数据源与输出约定")


if __name__ == "__main__":
    system_architecture()
    layered_architecture()
    decoupled_deployment()
    data_contracts()
