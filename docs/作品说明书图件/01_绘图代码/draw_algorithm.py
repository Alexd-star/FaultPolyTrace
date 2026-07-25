from __future__ import annotations

import numpy as np
from matplotlib.patches import Circle, Polygon as MplPolygon

from figure_style import COLORS, arrow, box, canvas, horizontal_chain, save_all, setup_style, stage_band


def algorithm_pipeline() -> None:
    fig, ax = canvas(7.2, 4.35)
    stages = [
        "网格解析\n与统计", "分位数强种子\n弱响应受控延拓", "小尺度闭运算\n骨架化", "分支筛选\n受控补连", "属性约束\n断层带重建", "根系防黏连\n边界矢量化", "面积过滤\n平滑与拓扑验收"
    ]
    fills = ["panel", "blue_light", "paper", "paper", "teal_light", "teal_light", "panel"]
    horizontal_chain(ax, stages, y=0.49, box_height=0.22, fills=fills)
    ax.text(0.04, 0.84, "输入：二维平面断层属性网格", fontsize=7.5, weight="bold", color=COLORS["blue"])
    ax.text(0.96, 0.84, "输出：闭合断层多边形 + 结构化诊断", ha="right", fontsize=7.5, weight="bold", color=COLORS["teal"])
    ax.text(0.04, 0.27, "覆盖控制", fontsize=7, weight="bold", color=COLORS["blue"])
    ax.text(0.15, 0.27, "强响应种子、弱响应形态、路径属性支撑、覆盖保持回退", fontsize=6.5)
    ax.text(0.04, 0.18, "误合并控制", fontsize=7, weight="bold", color=COLORS["teal"])
    ax.text(0.15, 0.18, "方向一致性、背景隔离、根系归属、局部主体规模、可回退门控", fontsize=6.5)
    ax.text(0.04, 0.09, "正式输出门槛", fontsize=7, weight="bold", color=COLORS["ink"])
    ax.text(0.15, 0.09, "闭合、无自交、无异常相交、面积与形态规则通过", fontsize=6.5)
    save_all(fig, "F09_Ridge-Belt核心算法总流程")


def segmentation_to_polygon() -> None:
    setup_style()
    fig, axes = __import__("matplotlib.pyplot").pyplot.subplots(1, 5, figsize=(7.2, 2.35), constrained_layout=True)
    x = np.linspace(-1, 1, 140)
    y = np.linspace(-0.55, 0.55, 80)
    xx, yy = np.meshgrid(x, y)
    center = 0.16 * np.sin(3.4 * xx)
    main_response = np.exp(-((yy - center) ** 2) / 0.006) * (0.82 + 0.18 * np.cos(8 * xx))
    branch_response = 0.68 * np.exp(-((yy + 0.28 + 0.18 * xx) ** 2) / 0.008)
    field = main_response + branch_response
    mask = field > 0.50
    for ax in axes:
        ax.set_xticks([]); ax.set_yticks([])
        for spine in ax.spines.values(): spine.set_visible(True); spine.set_linewidth(0.6); spine.set_color(COLORS["line"])
    axes[0].imshow(field, cmap="gray_r", origin="lower", aspect="auto"); axes[0].set_title("a  属性响应", loc="left", weight="bold")
    axes[1].imshow(mask, cmap="gray_r", origin="lower", aspect="auto"); axes[1].set_title("b  强/弱响应掩膜", loc="left", weight="bold")
    axes[2].plot(np.arange(140), 40 + 12 * np.sin(3.4 * x), color=COLORS["blue"], lw=1.4)
    axes[2].plot(np.arange(140), 20 - 13 * x, color=COLORS["blue"], lw=1.4); axes[2].set_xlim(0, 139); axes[2].set_ylim(0, 79); axes[2].invert_yaxis(); axes[2].set_title("c  中心骨架", loc="left", weight="bold")
    axes[3].imshow(mask, cmap="Greys", origin="lower", aspect="auto", alpha=0.25)
    axes[3].plot(np.arange(140), 40 + 12 * np.sin(3.4 * x), color=COLORS["teal"], lw=3.8)
    axes[3].plot(np.arange(140), 20 - 13 * x, color=COLORS["teal"], lw=3.8); axes[3].set_xlim(0,139); axes[3].set_ylim(0,79); axes[3].invert_yaxis(); axes[3].set_title("d  断层带重建", loc="left", weight="bold")
    axes[4].contour(mask.astype(float), levels=[0.5], colors=[COLORS["teal"]], linewidths=1.2, origin="lower")
    axes[4].set_xlim(0,139); axes[4].set_ylim(0,79); axes[4].set_title("e  闭合多边形", loc="left", weight="bold")
    save_all(fig, "F10_从属性响应到闭合多边形")


def weak_response_gate() -> None:
    fig, ax = canvas(7.2, 4.0)
    box(ax, (0.05, 0.62), (0.17, 0.13), "弱响应候选分量", fill="blue_light", edge="blue", weight="bold")
    gates = [(0.32, 0.73, "接触强种子邻域"), (0.32, 0.54, "规模与长宽比合理"), (0.58, 0.73, "路径支撑比例足够"), (0.58, 0.54, "连续低支撑段不过长")]
    for x, y, text in gates:
        box(ax, (x, y - 0.055), (0.18, 0.11), text, fill="paper", edge="line")
    box(ax, (0.81, 0.62), (0.14, 0.13), "接纳并延拓", fill="teal_light", edge="teal", weight="bold")
    box(ax, (0.43, 0.20), (0.20, 0.11), "任一关键证据不足", fill="red_light", edge="red")
    box(ax, (0.74, 0.20), (0.17, 0.11), "拒绝 / 保持背景", fill="panel", edge="red", weight="bold")
    arrow(ax, (0.22, 0.685), (0.32, 0.685), color="blue")
    arrow(ax, (0.50, 0.68), (0.58, 0.68), color="blue")
    arrow(ax, (0.76, 0.685), (0.81, 0.685), color="teal")
    arrow(ax, (0.50, 0.54), (0.53, 0.31), color="red", dashed=True)
    arrow(ax, (0.63, 0.54), (0.53, 0.31), color="red", dashed=True)
    arrow(ax, (0.63, 0.255), (0.74, 0.255), color="red")
    ax.text(0.50, 0.90, "弱响应按形态和路径证据传播，不以全局降阈值替代判断", ha="center", fontsize=8, weight="bold")
    save_all(fig, "F11_弱响应路径门控")


def branch_attribution() -> None:
    fig, ax = canvas(7.2, 4.1)
    cx, cy = 0.33, 0.52
    branches = [(0.12,0.73),(0.12,0.31),(0.54,0.76),(0.55,0.28)]
    for i, p in enumerate(branches, 1):
        ax.plot([cx,p[0]],[cy,p[1]],color=COLORS["blue"],lw=2.0)
        ax.add_patch(Circle(p,0.018,facecolor=COLORS["paper"],edgecolor=COLORS["blue"],lw=1.0))
        ax.text(p[0],p[1]+0.055,f"分支 {i}",ha="center",fontsize=6.5)
    ax.add_patch(Circle((cx,cy),0.028,facecolor=COLORS["amber_light"],edgecolor=COLORS["amber"],lw=1.0))
    box(ax, (0.64,0.68),(0.28,0.10),"多像素方向窗口",fill="paper",edge="line")
    box(ax, (0.64,0.51),(0.28,0.10),"路径属性连续性",fill="paper",edge="line")
    box(ax, (0.64,0.34),(0.28,0.10),"候选配对代价与歧义标记",fill="paper",edge="line")
    box(ax, (0.64,0.15),(0.28,0.10),"明确归属 → 根系分组\n证据不足 → 保守回退",fill="teal_light",edge="teal",weight="bold")
    arrow(ax,(0.40,0.52),(0.64,0.73),color="amber")
    arrow(ax,(0.40,0.52),(0.64,0.56),color="amber")
    arrow(ax,(0.40,0.52),(0.64,0.39),color="amber")
    arrow(ax,(0.78,0.34),(0.78,0.25),color="teal")
    ax.text(0.31,0.11,"交切点局部几何",ha="center",fontsize=7,weight="bold",color=COLORS["muted"])
    save_all(fig, "F12_分支归属与交切处理")


def root_antimerge() -> None:
    setup_style()
    fig, axes = __import__("matplotlib.pyplot").pyplot.subplots(1, 3, figsize=(7.2, 2.55), constrained_layout=True)
    for ax in axes:
        ax.set_xlim(0,1);ax.set_ylim(0,1);ax.axis("off")
    for offset in (0.0, 0.18):
        axes[0].plot([0.08,0.40,0.92],[0.28+offset,0.43+offset,0.70+offset],color=COLORS["teal"],lw=9,solid_capstyle="round")
    axes[0].plot([0.43,0.56],[0.52,0.52],color=COLORS["red"],lw=5)
    axes[0].set_title("a  低支撑桥导致黏连",loc="left",weight="bold")
    axes[1].plot([0.08,0.40],[0.28,0.43],color=COLORS["teal"],lw=9,solid_capstyle="round")
    axes[1].plot([0.62,0.92],[0.58,0.70],color=COLORS["teal"],lw=9,solid_capstyle="round")
    axes[1].axvline(0.51,0.32,0.72,color=COLORS["amber"],ls="--",lw=1.3)
    axes[1].text(0.51,0.80,"模拟切分",ha="center",fontsize=7,color=COLORS["amber"])
    axes[1].set_title("b  局部候选切分",loc="left",weight="bold")
    box(axes[2],(0.08,0.64),(0.84,0.12),"桥属性接近背景",fill="paper",edge="line")
    box(axes[2],(0.08,0.44),(0.84,0.12),"两侧主体规模与比例通过",fill="paper",edge="line")
    box(axes[2],(0.08,0.20),(0.36,0.12),"接受切分",fill="teal_light",edge="teal",weight="bold")
    box(axes[2],(0.56,0.20),(0.36,0.12),"证据不足则回退",fill="panel",edge="red",weight="bold")
    axes[2].set_title("c  证据门控",loc="left",weight="bold")
    save_all(fig, "F13_根系防黏连机制")


def topology_gate() -> None:
    fig, ax = canvas(7.2, 4.0)
    box(ax,(0.05,0.60),(0.18,0.13),"候选多边形",fill="teal_light",edge="teal",weight="bold")
    gates=[("闭合环",0.31),("无自相交",0.49),("无异常相交",0.67)]
    for text,x in gates:
        box(ax,(x,0.60),(0.14,0.13),text,fill="paper",edge="line")
    box(ax,(0.86,0.60),(0.10,0.13),"正式层",fill="blue_light",edge="blue",weight="bold")
    for x0,x1 in [(0.23,0.31),(0.45,0.49),(0.63,0.67),(0.81,0.86)]: arrow(ax,(x0,0.665),(x1,0.665),color="blue")
    box(ax,(0.23,0.24),(0.26,0.12),"几何失败：拒绝并记录明细",fill="red_light",edge="red")
    box(ax,(0.58,0.24),(0.26,0.12),"覆盖损失：回退至可靠边界",fill="amber_light",edge="amber")
    arrow(ax,(0.56,0.60),(0.36,0.36),color="red",dashed=True)
    arrow(ax,(0.74,0.60),(0.71,0.36),color="amber",dashed=True)
    ax.text(0.50,0.89,"平滑和简化不能绕过拓扑与覆盖验收",ha="center",fontsize=8,weight="bold")
    save_all(fig,"F14_多边形拓扑与覆盖门控")


def area_filter() -> None:
    fig, ax = canvas(7.2, 4.0)
    box(ax,(0.05,0.62),(0.18,0.12),"闭合多边形",fill="teal_light",edge="teal",weight="bold")
    box(ax,(0.32,0.62),(0.19,0.12),"面积 ≥ 120",fill="paper",edge="line")
    box(ax,(0.63,0.68),(0.26,0.11),"直接保留",fill="blue_light",edge="blue",weight="bold")
    box(ax,(0.32,0.31),(0.19,0.12),"面积 < 120",fill="paper",edge="line")
    box(ax,(0.60,0.36),(0.32,0.12),"像元数 + 长宽比 + 平均属性\n+ 最大属性联合判断",fill="paper",edge="amber")
    box(ax,(0.60,0.13),(0.14,0.10),"形态明确\n自适应保留",fill="teal_light",edge="teal")
    box(ax,(0.78,0.13),(0.14,0.10),"证据不足\n标记过滤",fill="panel",edge="red")
    arrow(ax,(0.23,0.68),(0.32,0.68),color="blue")
    arrow(ax,(0.51,0.68),(0.63,0.735),color="blue")
    arrow(ax,(0.415,0.62),(0.415,0.43),color="amber",label="否",label_offset=(0.03,0))
    arrow(ax,(0.51,0.37),(0.60,0.42),color="amber")
    arrow(ax,(0.68,0.36),(0.67,0.23),color="teal")
    arrow(ax,(0.84,0.36),(0.85,0.23),color="red")
    ax.text(0.50,0.90,"面积是主门槛，形态与属性用于保护细长而可信的小对象",ha="center",fontsize=8,weight="bold")
    save_all(fig,"F15_面积阈值与自适应保留")


if __name__ == "__main__":
    algorithm_pipeline()
    segmentation_to_polygon()
    weak_response_gate()
    branch_attribution()
    root_antimerge()
    topology_gate()
    area_filter()
