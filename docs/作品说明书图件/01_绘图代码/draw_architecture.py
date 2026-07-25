from __future__ import annotations

from figure_style import COLORS, arrow, box, canvas, save_all, stage_band


def system_architecture() -> None:
    fig, ax = canvas(7.2, 4.35)
    stage_band(ax, 0.02, 0.09, 0.16, 0.82, "输入")
    stage_band(ax, 0.21, 0.09, 0.20, 0.82, "产品入口")
    stage_band(ax, 0.44, 0.09, 0.29, 0.82, "统一 C++ 核心")
    stage_band(ax, 0.76, 0.09, 0.22, 0.82, "输出与复核")

    box(ax, (0.045, 0.61), (0.11, 0.12), "外部 DAT\n矩阵文本", fill="panel", edge="ink")
    box(ax, (0.045, 0.29), (0.11, 0.12), "GeoEast\nBOMapGrid", fill="panel", edge="ink")
    box(ax, (0.245, 0.61), (0.12, 0.12), "独立桌面软件", fill="blue_light", edge="blue", weight="bold")
    box(ax, (0.245, 0.29), (0.12, 0.12), "GeoEast 插件\nLauncher + Runner", fill="blue_light", edge="blue", weight="bold")
    box(ax, (0.48, 0.64), (0.20, 0.10), "输入解析与统一网格", fill="paper", edge="blue")
    box(ax, (0.48, 0.48), (0.20, 0.10), "Ridge-Belt 结构追踪", fill="teal_light", edge="teal", weight="bold")
    box(ax, (0.48, 0.32), (0.20, 0.10), "多边形生成与面积过滤", fill="paper", edge="teal")
    box(ax, (0.48, 0.16), (0.20, 0.10), "拓扑、覆盖与耗时诊断", fill="paper", edge="ink")
    box(ax, (0.79, 0.65), (0.16, 0.10), "CSV / WKT\nJSON / PNG", fill="panel", edge="ink")
    box(ax, (0.79, 0.46), (0.16, 0.10), "BOFaultPolygon", fill="panel", edge="ink")
    box(ax, (0.79, 0.27), (0.16, 0.10), "原始 / 结果 / 叠加\n真实图层查看", fill="teal_light", edge="teal")
    box(ax, (0.79, 0.12), (0.16, 0.08), "历史记录与复现证据", fill="panel", edge="ink")

    arrow(ax, (0.155, 0.67), (0.245, 0.67), color="blue")
    arrow(ax, (0.155, 0.35), (0.245, 0.35), color="blue")
    arrow(ax, (0.365, 0.67), (0.48, 0.69), color="blue")
    arrow(ax, (0.365, 0.35), (0.48, 0.69), color="blue")
    for y0, y1 in [(0.64, 0.58), (0.48, 0.42), (0.32, 0.26)]:
        arrow(ax, (0.58, y0), (0.58, y1), color="teal")
    arrow(ax, (0.68, 0.37), (0.79, 0.70), color="ink")
    arrow(ax, (0.68, 0.37), (0.79, 0.51), color="ink")
    arrow(ax, (0.68, 0.21), (0.79, 0.32), color="ink")
    arrow(ax, (0.68, 0.21), (0.79, 0.16), color="ink")
    save_all(fig, "F01_产品总体架构")


def layered_architecture() -> None:
    fig, ax = canvas(7.2, 4.4)
    layers = [
        (0.74, "用户交互层", "独立软件窗口  |  GeoEast Launcher  |  菜单、工具栏、真实图层查看", "blue_light", "blue"),
        (0.55, "应用服务层", "参数校验  |  任务调度  |  历史记录  |  结果管理  |  错误引导", "paper", "line"),
        (0.36, "数据适配层", "AsciiGridIO  |  DiagnosticsIO  |  GeoEastBoAdapter", "paper", "line"),
        (0.17, "核心算法层", "Segmentation  |  SkeletonGraph  |  FaultTracer  |  Polygon  |  Topology", "teal_light", "teal"),
    ]
    for y, title, detail, fill, edge in layers:
        box(ax, (0.08, y), (0.84, 0.13), "", fill=fill, edge=edge, radius=0.004)
        ax.text(0.11, y + 0.085, title, fontsize=8, weight="bold", color=COLORS[edge if edge in COLORS else "ink"], va="center")
        ax.text(0.11, y + 0.042, detail, fontsize=6.5, color=COLORS["ink"], va="center")
    for y0, y1 in [(0.74, 0.68), (0.55, 0.49), (0.36, 0.30)]:
        arrow(ax, (0.50, y0), (0.50, y1), color="ink", label="稳定接口", label_offset=(0.09, 0))
    ax.text(0.50, 0.06, "入口可以独立演进，核心算法和参数语义保持单一来源", ha="center", va="center", fontsize=7, color=COLORS["muted"])
    save_all(fig, "F02_代码分层与职责")


def decoupled_deployment() -> None:
    fig, ax = canvas(7.2, 4.3)
    stage_band(ax, 0.03, 0.12, 0.42, 0.76, "独立产品包")
    stage_band(ax, 0.55, 0.12, 0.42, 0.76, "GeoEast 集成包")
    box(ax, (0.08, 0.66), (0.32, 0.10), "FaultPolyTrace.exe", fill="blue_light", edge="blue", weight="bold")
    box(ax, (0.08, 0.47), (0.32, 0.10), "独立窗口代码 + 本地配置\n历史记录 + 外部文件流程", fill="paper", edge="line")
    box(ax, (0.08, 0.28), (0.32, 0.10), "Qt / VC 运行库 + 帮助文档\n安装器与便携版", fill="paper", edge="line")
    box(ax, (0.60, 0.66), (0.32, 0.10), "GeoEast Ribbon 入口", fill="blue_light", edge="blue", weight="bold")
    box(ax, (0.60, 0.47), (0.32, 0.10), "Mainframe Plugin + Launcher\n宿主上下文与工区交互", fill="paper", edge="line")
    box(ax, (0.60, 0.28), (0.32, 0.10), "BO Runner + GeoEast SDK\nBOMapGrid / BOFaultPolygon", fill="paper", edge="line")
    for x in (0.24, 0.76):
        arrow(ax, (x, 0.66), (x, 0.57), color="blue")
        arrow(ax, (x, 0.47), (x, 0.38), color="blue")
    box(ax, (0.36, 0.025), (0.28, 0.08), "共享：C++ 核心 + 参数模型 + 输出语义", fill="teal_light", edge="teal", weight="bold")
    arrow(ax, (0.24, 0.28), (0.43, 0.105), color="teal")
    arrow(ax, (0.76, 0.28), (0.57, 0.105), color="teal")
    ax.text(0.50, 0.92, "界面与部署解耦，算法实现不复制", ha="center", fontsize=8, weight="bold", color=COLORS["ink"])
    save_all(fig, "F03_独立软件与插件解耦部署")


def data_contracts() -> None:
    fig, ax = canvas(7.2, 4.25)
    box(ax, (0.04, 0.64), (0.17, 0.13), "GeoEast DAT\n规则矩阵文本", fill="panel", edge="ink")
    box(ax, (0.04, 0.29), (0.17, 0.13), "GeoEast BOMapGrid", fill="panel", edge="ink")
    box(ax, (0.32, 0.46), (0.24, 0.15), "Grid2D\n坐标、尺寸、连续属性值", fill="blue_light", edge="blue", weight="bold")
    box(ax, (0.67, 0.64), (0.26, 0.13), "Polygon + WKT\n面积、周长、属性统计", fill="teal_light", edge="teal")
    box(ax, (0.67, 0.29), (0.26, 0.13), "BOFaultPolygon\n工区对象名称与边界段", fill="teal_light", edge="teal")
    box(ax, (0.36, 0.13), (0.16, 0.09), "统一参数模型", fill="paper", edge="amber")
    arrow(ax, (0.21, 0.705), (0.32, 0.55), color="blue", label="AsciiGridIO", label_offset=(0, 0.035))
    arrow(ax, (0.21, 0.355), (0.32, 0.51), color="blue", label="GeoEastBoAdapter", label_offset=(0, -0.035))
    arrow(ax, (0.56, 0.55), (0.67, 0.705), color="teal", label="文件输出", label_offset=(0, 0.035))
    arrow(ax, (0.56, 0.51), (0.67, 0.355), color="teal", label="数据库写入", label_offset=(0, -0.035))
    arrow(ax, (0.44, 0.22), (0.44, 0.46), color="amber", dashed=True, label="同一参数语义", label_offset=(0.09, 0))
    ax.text(0.50, 0.90, "双数据源在内部网格处汇合，双输出共享同一多边形几何", ha="center", fontsize=8, weight="bold")
    save_all(fig, "F04_双数据源与输出契约")


if __name__ == "__main__":
    system_architecture()
    layered_architecture()
    decoupled_deployment()
    data_contracts()

