from __future__ import annotations

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Rectangle

from figure_style import COLORS, arrow, canvas, label_box, phase_header, rule, save_all, setup_style


def _synthetic_field(nx: int = 180, ny: int = 100) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    x = np.linspace(-1, 1, nx)
    y = np.linspace(-0.6, 0.6, ny)
    xx, yy = np.meshgrid(x, y)
    c1 = 0.16 * np.sin(3.5 * xx)
    c2 = -0.42 * xx + 0.18
    field = np.exp(-((yy - c1) ** 2) / 0.005)
    field += 0.72 * np.exp(-((yy - c2) ** 2) / 0.009) * (xx > -0.55)
    field *= 0.83 + 0.17 * np.cos(7 * xx)
    return np.clip(field, 0, 1), xx, yy


def algorithm_pipeline() -> None:
    fig, ax = canvas(7.205, 4.35)
    phase_header(ax, 0.035, 0.30, 0.91, "I  响应提取", color="blue")
    phase_header(ax, 0.34, 0.63, 0.91, "II  结构追踪", color="teal")
    phase_header(ax, 0.67, 0.965, 0.91, "III  几何生成与检查", color="blue")
    stages = [
        (0.07, "网格解析", "坐标、尺寸\n属性统计", "blue"),
        (0.20, "强弱响应", "分位数种子\n受控弱分量", "blue"),
        (0.37, "骨架化", "中心结构\n端点与交点", "teal"),
        (0.50, "分支与补连", "长度/属性筛选\n距离/方向/支撑", "teal"),
        (0.69, "断层带重建", "骨架扩展\n属性包络", "teal"),
        (0.82, "闭合多边形", "防黏连\n边界矢量化", "blue"),
        (0.94, "保留结果", "面积过滤\n平滑/拓扑", "blue"),
    ]
    y = 0.59
    rule(ax, (0.07, y), (0.94, y), color="gray_dark", lw=0.9)
    for i, (x, title, note, color) in enumerate(stages, 1):
        ax.add_patch(Circle((x, y), 0.020, facecolor=COLORS["paper"], edgecolor=COLORS[color], lw=1.1, zorder=3))
        ax.text(x, y, str(i), ha="center", va="center", fontsize=5.7, weight="bold", color=COLORS[color])
        ax.text(x, y + 0.075, title, ha="center", fontsize=6.2, weight="bold", color=COLORS[color])
        ax.text(x, y - 0.075, note, ha="center", va="top", fontsize=5.3, color=COLORS["muted"], linespacing=1.25)

    ax.add_patch(Rectangle((0.035, 0.12), 0.93, 0.20, facecolor=COLORS["panel"], edgecolor=COLORS["line"], lw=0.55))
    rows = [
        (0.265, "覆盖保护", "强响应种子 · 受控弱响应 · 属性包络 · 平滑覆盖回退", "blue"),
        (0.205, "误合并抑制", "方向与路径支撑 · 根系归属 · 低值桥模拟切分 · 条件不足回退", "teal"),
        (0.145, "输出硬门槛", "闭合 · 无自交 · 无异常相交 · 面积与形态规则通过", "ink"),
    ]
    for yy, label, text, color in rows:
        ax.text(0.06, yy, label, fontsize=6.1, weight="bold", color=COLORS[color], va="center")
        rule(ax, (0.145, yy), (0.18, yy), color=color, lw=1.2)
        ax.text(0.19, yy, text, fontsize=5.8, color=COLORS["ink"], va="center")
    save_all(fig, "F09_Ridge-Belt核心算法总流程")


def segmentation_to_polygon() -> None:
    setup_style()
    field, xx, yy = _synthetic_field(180, 100)
    strong = field >= np.quantile(field, 0.86)
    weak = field >= np.quantile(field, 0.78)
    x = np.linspace(-1, 1, field.shape[1])
    fig, axes = plt.subplots(1, 5, figsize=(7.205, 2.45), constrained_layout=True)
    for ax in axes:
        ax.set_xticks([]); ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_visible(True); spine.set_linewidth(0.55); spine.set_color(COLORS["line"])
    axes[0].imshow(field, cmap="Greys", origin="lower", aspect="auto")
    axes[0].set_title("a  连续属性", loc="left", weight="bold")
    axes[1].imshow(weak, cmap="Greys", origin="lower", aspect="auto")
    axes[1].contour(strong.astype(float), [0.5], colors=[COLORS["blue"]], linewidths=0.7, origin="lower")
    axes[1].set_title("b  强种子与弱响应", loc="left", weight="bold")
    axes[2].plot(np.arange(180), 50 + 13 * np.sin(3.5 * x), color=COLORS["blue"], lw=1.25)
    axes[2].plot(np.arange(180), 64 - 35 * x, color=COLORS["blue"], lw=1.15)
    axes[2].set_xlim(0,179); axes[2].set_ylim(0,99); axes[2].set_title("c  中心骨架", loc="left", weight="bold")
    axes[3].imshow(field, cmap="Greys", origin="lower", aspect="auto", alpha=0.28)
    axes[3].contour(weak.astype(float), [0.5], colors=[COLORS["teal"]], linewidths=1.1, origin="lower")
    axes[3].set_title("d  属性约束断层带", loc="left", weight="bold")
    axes[4].contour(weak.astype(float), [0.5], colors=[COLORS["teal_dark"]], linewidths=1.15, origin="lower")
    axes[4].set_xlim(0,179); axes[4].set_ylim(0,99); axes[4].set_title("e  闭合矢量边界", loc="left", weight="bold")
    fig.text(0.50, 0.012, "处理逻辑由中心结构驱动，而不是直接照搬阈值斑块外缘", ha="center", fontsize=6.1, color=COLORS["muted"])
    save_all(fig, "F10_从属性响应到闭合多边形")


def weak_response_gate() -> None:
    setup_style()
    fig, axes = plt.subplots(1, 3, figsize=(7.205, 2.75), constrained_layout=True)
    x = np.linspace(0, 1, 180)
    response = 0.10 + 0.85 * np.exp(-((x - 0.25) ** 2) / 0.012) + 0.58 * np.exp(-((x - 0.68) ** 2) / 0.035)
    response *= 0.92 + 0.08 * np.cos(12 * x)
    axes[0].plot(x, response, color=COLORS["ink"], lw=1.0)
    axes[0].axhline(0.72, color=COLORS["blue"], ls="--", lw=0.8, label="强阈值")
    axes[0].axhline(0.48, color=COLORS["teal"], ls="--", lw=0.8, label="弱阈值")
    axes[0].fill_between(x, 0.48, response, where=response >= 0.48, color=COLORS["teal_light"], alpha=0.9)
    axes[0].set_ylim(0, 1.08); axes[0].set_xlabel("路径位置"); axes[0].set_ylabel("属性响应")
    axes[0].set_title("a  双阈值分离强种子与弱候选", loc="left", weight="bold")
    axes[0].legend(loc="upper right")

    ax = axes[1]
    ax.set_xlim(0,1); ax.set_ylim(0,1); ax.axis("off")
    ax.plot([0.08,0.34],[0.66,0.57],color=COLORS["blue"],lw=7,solid_capstyle="round")
    ax.plot([0.34,0.82],[0.57,0.36],color=COLORS["teal"],lw=5,solid_capstyle="round")
    ax.add_patch(Circle((0.34,0.57),0.025,facecolor=COLORS["paper"],edgecolor=COLORS["blue"],lw=0.9))
    ax.text(0.21,0.76,"强种子",ha="center",fontsize=6.2,weight="bold",color=COLORS["blue"])
    ax.text(0.62,0.55,"弱候选分量",ha="center",fontsize=6.2,weight="bold",color=COLORS["teal"])
    ax.text(0.50,0.18,"接触强种子邻域",ha="center",fontsize=5.8)
    ax.set_title("b  邻域接触建立延拓起点",loc="left",weight="bold")

    ax = axes[2]
    ax.set_xlim(0,1); ax.set_ylim(0,1); ax.axis("off")
    tests = [(0.74,"分量规模"),(0.56,"长宽比"),(0.38,"与强种子接触")]
    for y,label in tests:
        ax.add_patch(Circle((0.15,y),0.012,facecolor=COLORS["teal"],edgecolor="none"))
        ax.text(0.20,y,label,va="center",fontsize=6.0)
    rule(ax,(0.15,0.74),(0.15,0.38),color="teal",lw=0.8)
    label_box(ax,(0.54,0.58),"条件满足\n接纳弱分量",color="teal",fill="teal_light",fontsize=6.2,weight="bold")
    label_box(ax,(0.54,0.26),"条件不足\n保持背景",color="red",fill="paper",fontsize=6.0)
    arrow(ax,(0.30,0.56),(0.43,0.58),color="teal")
    ax.set_title("c  分量级形态门控",loc="left",weight="bold")
    save_all(fig, "F11_弱响应路径门控")


def branch_attribution() -> None:
    setup_style()
    fig, axes = plt.subplots(1, 2, figsize=(7.205, 3.05), gridspec_kw={"width_ratios":[1.12,0.88]}, constrained_layout=True)
    ax = axes[0]
    ax.set_xlim(0,1); ax.set_ylim(0,1); ax.axis("off")
    c=(0.50,0.50)
    pts=[(0.10,0.82),(0.11,0.20),(0.90,0.76),(0.90,0.23)]
    for i,p in enumerate(pts,1):
        ax.plot([c[0],p[0]],[c[1],p[1]],color=COLORS["blue"],lw=1.8)
        ax.add_patch(Circle(p,0.016,facecolor=COLORS["paper"],edgecolor=COLORS["blue"],lw=0.8))
        ax.text(p[0],p[1]+0.055,f"分支 {i}",ha="center",fontsize=5.8)
    ax.add_patch(Circle(c,0.026,facecolor=COLORS["amber_light"],edgecolor=COLORS["amber"],lw=0.9))
    ax.plot([0.12,0.88],[0.79,0.74],color=COLORS["teal"],lw=0.9,ls="--")
    ax.plot([0.12,0.88],[0.22,0.25],color=COLORS["teal"],lw=0.9,ls="--")
    ax.text(0.50,0.91,"小转角配对形成连续根系",ha="center",fontsize=6.2,weight="bold",color=COLORS["teal"])
    ax.text(0.50,0.08,"交点处仅在最大转角约束内配对",ha="center",fontsize=5.8,color=COLORS["muted"])
    ax.set_title("a  交点邻接方向配对",loc="left",weight="bold")

    ax=axes[1]
    matrix=np.array([[np.nan,18,64,79],[18,np.nan,72,21],[64,72,np.nan,16],[79,21,16,np.nan]])
    im=ax.imshow(matrix,cmap="Blues",vmin=0,vmax=90)
    for i in range(4):
        for j in range(4):
            if np.isfinite(matrix[i,j]):
                ax.text(j,i,f"{int(matrix[i,j])}°",ha="center",va="center",fontsize=5.7,
                        color="white" if matrix[i,j]>55 else COLORS["ink"])
    ax.set_xticks(range(4),["1","2","3","4"]); ax.set_yticks(range(4),["1","2","3","4"])
    ax.set_xlabel("候选分支"); ax.set_ylabel("当前分支")
    ax.set_title("b  转角代价与阈值",loc="left",weight="bold")
    ax.text(0.5,-0.20,"示意：优先选择转角较小且不超过阈值的组合",transform=ax.transAxes,ha="center",fontsize=5.6,color=COLORS["muted"])
    save_all(fig, "F12_分支归属与交切处理")


def root_antimerge() -> None:
    setup_style()
    fig, axes = plt.subplots(1, 4, figsize=(7.205, 2.45), constrained_layout=True)
    for ax in axes:
        ax.set_xlim(0,1); ax.set_ylim(0,1); ax.axis("off")
    axes[0].plot([0.08,0.43,0.92],[0.28,0.48,0.74],color=COLORS["teal"],lw=8,solid_capstyle="round")
    axes[0].plot([0.08,0.43,0.92],[0.70,0.52,0.26],color=COLORS["teal"],lw=8,solid_capstyle="round")
    axes[0].plot([0.41,0.57],[0.50,0.50],color=COLORS["red"],lw=5)
    axes[0].set_title("a  低值桥造成黏连",loc="left",weight="bold")

    axes[1].plot([0.08,0.43],[0.28,0.48],color=COLORS["teal"],lw=8,solid_capstyle="round")
    axes[1].plot([0.59,0.92],[0.58,0.74],color=COLORS["teal"],lw=8,solid_capstyle="round")
    axes[1].plot([0.08,0.43],[0.70,0.52],color=COLORS["teal"],lw=8,solid_capstyle="round")
    axes[1].plot([0.59,0.92],[0.42,0.26],color=COLORS["teal"],lw=8,solid_capstyle="round")
    axes[1].axvline(0.51,0.23,0.77,color=COLORS["amber"],ls="--",lw=1.0)
    axes[1].set_title("b  局部模拟切分",loc="left",weight="bold")

    ax=axes[2]
    y=np.array([0.72,0.50,0.28]); vals=np.array([0.08,0.42,0.46])
    ax.barh(y,vals,height=0.10,color=[COLORS["amber"],COLORS["blue_light"],COLORS["teal_light"]],edgecolor=COLORS["ink"],lw=0.5)
    ax.text(0.02,0.82,"桥属性",fontsize=5.7); ax.text(0.02,0.60,"主体 A 比例",fontsize=5.7); ax.text(0.02,0.38,"主体 B 比例",fontsize=5.7)
    ax.axvline(0.12,color=COLORS["red"],ls="--",lw=0.8)
    ax.set_xlim(0,0.55); ax.set_ylim(0.12,0.92); ax.set_xticks([]); ax.set_yticks([])
    ax.set_title("c  属性与主体规模判定",loc="left",weight="bold")

    label_box(axes[3],(0.50,0.67),"条件满足\n接受切分",color="teal",fill="teal_light",fontsize=6.2,weight="bold")
    label_box(axes[3],(0.50,0.31),"任一条件不足\n回退原结构",color="red",fill="paper",fontsize=6.0)
    arrow(axes[3],(0.50,0.55),(0.50,0.43),color="gray_dark")
    axes[3].set_title("d  可回退决策",loc="left",weight="bold")
    save_all(fig, "F13_根系防黏连机制")


def topology_gate() -> None:
    setup_style()
    fig, axes = plt.subplots(1, 4, figsize=(7.205, 2.35), constrained_layout=True)
    theta=np.linspace(0,2*np.pi,160)
    examples=["闭合", "开放环", "自相交", "异常相交"]
    for ax,title in zip(axes,examples):
        ax.set_xlim(-1.2,1.2);ax.set_ylim(-1.2,1.2);ax.set_aspect("equal");ax.axis("off")
        ax.set_title(title,loc="left",weight="bold")
    axes[0].plot(np.cos(theta),0.68*np.sin(theta),color=COLORS["teal"],lw=1.4)
    axes[0].text(0,-1.02,"通过",ha="center",fontsize=6.0,weight="bold",color=COLORS["teal"])
    t=np.linspace(0.3,2*np.pi-0.45,140);axes[1].plot(np.cos(t),0.68*np.sin(t),color=COLORS["red"],lw=1.4)
    axes[1].scatter([np.cos(t[0]),np.cos(t[-1])],[0.68*np.sin(t[0]),0.68*np.sin(t[-1])],s=10,color=COLORS["red"])
    tt=np.linspace(0,2*np.pi,180);axes[2].plot(np.sin(tt),np.sin(tt)*np.cos(tt),color=COLORS["red"],lw=1.4)
    axes[3].plot(np.cos(theta)-0.28,0.58*np.sin(theta),color=COLORS["red"],lw=1.1)
    axes[3].plot(np.cos(theta)+0.28,0.58*np.sin(theta),color=COLORS["amber"],lw=1.1)
    for ax in axes[1:]:ax.text(0,-1.02,"拒绝或回退",ha="center",fontsize=5.7,color=COLORS["red"])
    fig.text(0.5,0.012,"简化和平滑后重新检查；保留层只接收闭合、无自交且无异常相交的边界",
             ha="center",fontsize=6.1,color=COLORS["muted"])
    save_all(fig, "F14_多边形拓扑与覆盖门控")


def area_filter() -> None:
    setup_style()
    fig, ax = plt.subplots(figsize=(7.205,3.15),constrained_layout=True)
    rng=np.random.default_rng(17)
    area=np.r_[rng.uniform(15,118,34),rng.uniform(125,420,42)]
    elong=np.r_[rng.uniform(1.0,5.8,34),rng.uniform(1.0,6.0,42)]
    attr=np.r_[rng.uniform(0.45,0.98,34),rng.uniform(0.45,1.0,42)]
    adaptive=(area<120)&(elong>=2)&(attr>=0.72)
    direct=area>=120
    filtered=~(adaptive|direct)
    size=14+32*attr
    ax.scatter(area[filtered],elong[filtered],s=size[filtered],facecolor="none",edgecolor=COLORS["gray_dark"],lw=0.7,label="过滤")
    ax.scatter(area[adaptive],elong[adaptive],s=size[adaptive],color=COLORS["amber"],edgecolor=COLORS["ink"],lw=0.4,label="自适应保留")
    ax.scatter(area[direct],elong[direct],s=size[direct],color=COLORS["teal"],edgecolor=COLORS["ink"],lw=0.4,label="面积达标")
    ax.axvline(120,color=COLORS["blue"],ls="--",lw=0.9)
    ax.axhline(2,color=COLORS["amber"],ls=":",lw=0.8)
    ax.text(124,5.85,"面积阈值 120",fontsize=6.0,color=COLORS["blue"],va="top")
    ax.text(18,2.08,"长宽比门槛",fontsize=5.8,color=COLORS["amber"])
    ax.set_xlabel("多边形面积");ax.set_ylabel("长宽比")
    ax.set_xlim(0,440);ax.set_ylim(0.7,6.2)
    ax.set_title("面积为主门槛，形态与属性保护细长高响应小对象",loc="left",fontsize=7.5,weight="bold")
    ax.legend(loc="lower right",ncol=3)
    ax.grid(color=COLORS["gray"],lw=0.4,alpha=0.55)
    ax.spines["top"].set_visible(False);ax.spines["right"].set_visible(False)
    save_all(fig, "F15_面积阈值与自适应保留")


if __name__ == "__main__":
    algorithm_pipeline()
    segmentation_to_polygon()
    weak_response_gate()
    branch_attribution()
    root_antimerge()
    topology_gate()
    area_filter()
