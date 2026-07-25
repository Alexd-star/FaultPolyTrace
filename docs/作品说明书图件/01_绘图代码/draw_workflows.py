from __future__ import annotations

from figure_style import COLORS, arrow, box, canvas, horizontal_chain, save_all, stage_band


def standalone_workflow() -> None:
    fig, ax = canvas(7.2, 3.7)
    labels = ["选择数据源\n打开 DAT/文本", "检查原始网格\n确认坐标与响应", "选择预设或\n调整必要参数", "开始追踪\n后台执行", "查看原始/结果/叠加", "保存结果\n历史与图件"]
    fills = ["panel", "blue_light", "paper", "teal_light", "blue_light", "panel"]
    horizontal_chain(ax, labels, y=0.43, box_height=0.20, fills=fills)
    ax.text(0.04, 0.78, "前置条件", fontsize=7, weight="bold", color=COLORS["muted"])
    ax.text(0.04, 0.73, "输入可读、输出目录可写；未完成时界面给出下一步提示", fontsize=6.5, color=COLORS["ink"])
    ax.text(0.04, 0.22, "结果状态", fontsize=7, weight="bold", color=COLORS["muted"])
    ax.text(0.04, 0.17, "未运行：不显示处理后多边形  |  完成：显示对象数量、耗时与输出位置  |  失败：保留原因和修复入口", fontsize=6.5, color=COLORS["ink"])
    save_all(fig, "F05_独立软件全流程")


def geoeast_workflow() -> None:
    fig, ax = canvas(7.2, 4.0)
    labels = ["从 GeoEast\n打开插件", "自动识别根目录\n用户与当前工区", "检测连接并\n刷新 BOMapGrid", "选择输入网格\n命名输出对象", "执行统一核心\n写入 BOFaultPolygon", "刷新结果列表\n显示或删除对象"]
    fills = ["panel", "blue_light", "paper", "paper", "teal_light", "panel"]
    horizontal_chain(ax, labels, y=0.48, box_height=0.20, fills=fills)
    stage_band(ax, 0.03, 0.12, 0.45, 0.20, "自动化范围")
    ax.text(0.05, 0.21, "可检测：GeoEast 路径、执行程序、用户、当前工区、对象列表", fontsize=6.5, color=COLORS["ink"])
    ax.text(0.05, 0.155, "需确认：目标工区、输入对象、输出名称与数据库权限", fontsize=6.5, color=COLORS["ink"])
    stage_band(ax, 0.52, 0.12, 0.45, 0.20, "闭环证据")
    ax.text(0.54, 0.21, "读取 BOMapGrid → 运行 → 写入 → 重新读取 → 图层叠加", fontsize=6.5, color=COLORS["ink"])
    ax.text(0.54, 0.155, "日志同步记录核心耗时、总耗时、对象名称和失败原因", fontsize=6.5, color=COLORS["ink"])
    save_all(fig, "F06_GeoEast插件标准流程")


def layer_view_workflow() -> None:
    fig, ax = canvas(7.2, 4.1)
    box(ax, (0.05, 0.61), (0.20, 0.12), "输入网格选择", fill="paper", edge="blue")
    box(ax, (0.05, 0.29), (0.20, 0.12), "结果对象选择", fill="paper", edge="teal")
    box(ax, (0.35, 0.61), (0.20, 0.12), "原始网格图层：开启", fill="blue_light", edge="blue", weight="bold")
    box(ax, (0.35, 0.29), (0.20, 0.12), "结果多边形图层：开启", fill="teal_light", edge="teal", weight="bold")
    box(ax, (0.67, 0.44), (0.26, 0.16), "真实图层查看器\n平移、缩放、重置、导出", fill="panel", edge="ink", weight="bold")
    arrow(ax, (0.25, 0.67), (0.35, 0.67), color="blue")
    arrow(ax, (0.25, 0.35), (0.35, 0.35), color="teal")
    arrow(ax, (0.55, 0.67), (0.67, 0.55), color="blue")
    arrow(ax, (0.55, 0.35), (0.67, 0.49), color="teal")
    box(ax, (0.18, 0.08), (0.64, 0.09), "图层开关只控制显示；颜色、线宽和透明度不改变多边形坐标与面积", fill="paper", edge="amber")
    arrow(ax, (0.80, 0.44), (0.80, 0.17), color="amber", dashed=True)
    ax.text(0.50, 0.88, "选择对象与显示开关相互独立，可单层查看，也可同时叠加", ha="center", fontsize=8, weight="bold")
    save_all(fig, "F07_结果查看与图层叠加逻辑")


def guidance_states() -> None:
    fig, ax = canvas(7.2, 4.4)
    rows = [
        (0.73, "未选择输入", "禁用开始追踪", "提示选择外部文件或配置 GeoEast 数据源", "amber_light", "amber"),
        (0.54, "数据库信息不完整", "网格/结果列表不执行", "定位缺失字段并提供数据库配置与帮助入口", "amber_light", "amber"),
        (0.35, "已有输入但尚未计算", "结果图层保持空", "明确显示“尚未生成处理结果”，避免误解", "panel", "line"),
        (0.16, "执行失败", "保留日志和当前配置", "按输入、执行程序、权限或对象名称给出修复顺序", "red_light", "red"),
    ]
    ax.text(0.06, 0.92, "用户状态", fontsize=7, weight="bold", color=COLORS["muted"])
    ax.text(0.35, 0.92, "界面行为", fontsize=7, weight="bold", color=COLORS["muted"])
    ax.text(0.60, 0.92, "引导信息", fontsize=7, weight="bold", color=COLORS["muted"])
    for y, state, action, message, fill, edge in rows:
        box(ax, (0.04, y), (0.22, 0.11), state, fill=fill, edge=edge, weight="bold")
        box(ax, (0.32, y), (0.22, 0.11), action, fill="paper", edge="line")
        box(ax, (0.60, y), (0.35, 0.11), message, fill="paper", edge="line", fontsize=6.3)
        arrow(ax, (0.26, y + 0.055), (0.32, y + 0.055), color="ink")
        arrow(ax, (0.54, y + 0.055), (0.60, y + 0.055), color="ink")
    ax.text(0.50, 0.055, "原则：前置条件在动作发生前检查，错误信息同时说明原因、影响与下一步", ha="center", fontsize=7, color=COLORS["ink"])
    save_all(fig, "F08_前置条件与错误引导状态")


if __name__ == "__main__":
    standalone_workflow()
    geoeast_workflow()
    layer_view_workflow()
    guidance_states()
