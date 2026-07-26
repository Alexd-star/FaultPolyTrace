from __future__ import annotations

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Rectangle

from figure_style import COLORS, arrow, canvas, label_box, phase_header, rule, save_all, setup_style


def packaging_flow() -> None:
    fig, ax = canvas(7.205, 4.15)
    phase_header(ax, 0.04, 0.31, 0.91, "来源可追溯", color="blue")
    phase_header(ax, 0.35, 0.65, 0.91, "统一构建与验证", color="teal")
    phase_header(ax, 0.69, 0.96, 0.91, "客户机交付", color="blue")
    xs=[0.08,0.23,0.40,0.55,0.72,0.89]
    labels=[
        ("Git 提交","源码唯一来源"),("版本清单","Build ID / 时间"),("编译","CLI · GUI · 插件"),
        ("自动验证","回归 · 哈希 · 运行检查"),("打包","安装器 · ZIP · 插件包"),("异机复核","中文路径与依赖")]
    rule(ax,(xs[0],0.61),(xs[-1],0.61),color="gray_dark",lw=0.9)
    for i,(x,(title,note)) in enumerate(zip(xs,labels),1):
        color="teal" if i in (3,4) else "blue"
        ax.add_patch(Circle((x,0.61),0.019,facecolor=COLORS["paper"],edgecolor=COLORS[color],lw=1.1,zorder=3))
        ax.text(x,0.61,str(i),ha="center",va="center",fontsize=5.6,weight="bold",color=COLORS[color])
        ax.text(x,0.69,title,ha="center",fontsize=6.2,weight="bold",color=COLORS[color])
        ax.text(x,0.51,note,ha="center",fontsize=5.4,color=COLORS["muted"])

    ax.add_patch(Rectangle((0.04,0.13),0.43,0.20,facecolor=COLORS["panel"],edgecolor=COLORS["line"],lw=0.55))
    ax.text(0.06,0.285,"纳入交付包",fontsize=6.2,weight="bold")
    checks=["可执行程序与运行库","帮助、模板与许可","唯一构建清单与 SHA256"]
    for k,text in enumerate(checks):
        yy=0.245-k*0.050;ax.add_patch(Circle((0.07,yy),0.008,facecolor=COLORS["teal"],edgecolor="none"));ax.text(0.09,yy,text,va="center",fontsize=5.7)
    ax.add_patch(Rectangle((0.53,0.13),0.43,0.20,facecolor=COLORS["panel"],edgecolor=COLORS["line"],lw=0.55))
    ax.text(0.55,0.285,"交付包排除内容",fontsize=6.2,weight="bold")
    rejects=["个人路径与本机配置","缓存、临时文件与旧版 EXE","无法追溯的混合构建产物"]
    for k,text in enumerate(rejects):
        yy=0.245-k*0.050;ax.text(0.56,yy,"×",va="center",fontsize=7,color=COLORS["red"],weight="bold");ax.text(0.59,yy,text,va="center",fontsize=5.7)
    save_all(fig,"F21_Windows产品化构建与交付流程")


def geoeast_roundtrip() -> None:
    fig, ax = canvas(7.205, 4.25)
    center=(0.50,0.52)
    ax.add_patch(Circle(center,0.115,facecolor=COLORS["panel"],edgecolor=COLORS["ink"],lw=0.7))
    ax.text(center[0],center[1]+0.025,"GeoEast 工区",ha="center",fontsize=7.0,weight="bold")
    ax.text(center[0],center[1]-0.028,"数据库上下文",ha="center",fontsize=5.8,color=COLORS["muted"])
    nodes=[
        (0.18,0.70,"读取 BOMapGrid","网格与坐标"),(0.49,0.82,"转换 Grid2D","统一内部契约"),
        (0.80,0.69,"执行追踪核心","闭合多边形"),(0.82,0.34,"写入 BOFaultPolygon","对象名与边界段"),
        (0.50,0.20,"重新读取列表","确认对象存在"),(0.18,0.34,"显示与管理","叠加、导出、删除")]
    for i,(x,y,title,note) in enumerate(nodes):
        color="teal" if i in (2,3,4) else "blue"
        ax.add_patch(Circle((x,y),0.024,facecolor=COLORS["paper"],edgecolor=COLORS[color],lw=1.0,zorder=3))
        ax.text(x,y,str(i+1),ha="center",va="center",fontsize=5.6,weight="bold",color=COLORS[color])
        ax.text(x,y+0.060,title,ha="center",fontsize=6.1,weight="bold",color=COLORS[color])
        ax.text(x,y-0.060,note,ha="center",fontsize=5.3,color=COLORS["muted"])
    for a,b in zip(nodes,nodes[1:]+nodes[:1]):
        arrow(ax,(a[0],a[1]),(b[0],b[1]),color="teal" if a[0]>0.7 or a[1]<0.3 else "blue",lw=0.8)
    label_box(ax,(0.50,0.065),"闭环完成条件：写入结果能够重新读取，并与选定网格在同一坐标系显示",color="teal",fontsize=6.2)
    save_all(fig,"F22_GeoEast数据库读写闭环")


def display_scope() -> None:
    setup_style()
    fig, axes = plt.subplots(1,2,figsize=(7.205,2.85),constrained_layout=True)
    t=np.linspace(0,1,180);y=0.50+0.13*np.sin(5.4*t)
    ax=axes[0]
    ax.plot(t,y,color=COLORS["teal"],lw=1.0,label="原显示")
    ax.plot(t,y,color=COLORS["amber"],lw=2.8,alpha=0.55,label="修改线宽/颜色/透明度")
    ax.set_xlim(0,1);ax.set_ylim(0.25,0.75);ax.set_xticks([]);ax.set_yticks([])
    ax.set_title("a  显示属性：即时重绘",loc="left",weight="bold")
    ax.legend(loc="lower center",ncol=1)
    ax.text(0.5,0.08,"WKT、面积和对象数量保持不变",transform=ax.transAxes,ha="center",fontsize=5.8,color=COLORS["muted"])
    for spine in ax.spines.values():spine.set_visible(True);spine.set_color(COLORS["line"]);spine.set_linewidth(0.55)

    ax=axes[1]
    ax.plot(t,y,color=COLORS["gray_dark"],lw=1.0,label="原边界")
    y2=0.50+0.17*np.sin(5.4*t)+0.025*np.sin(14*t)
    ax.plot(t,y2,color=COLORS["blue"],lw=1.3,label="重新追踪后边界")
    ax.fill_between(t,y,y2,color=COLORS["blue_light"],alpha=0.8)
    ax.set_xlim(0,1);ax.set_ylim(0.25,0.78);ax.set_xticks([]);ax.set_yticks([])
    ax.set_title("b  计算属性：重新追踪",loc="left",weight="bold")
    ax.legend(loc="lower center",ncol=1)
    ax.text(0.5,0.08,"阈值、带宽、面积过滤或平滑可能改变几何",transform=ax.transAxes,ha="center",fontsize=5.8,color=COLORS["muted"])
    for spine in ax.spines.values():spine.set_visible(True);spine.set_color(COLORS["line"]);spine.set_linewidth(0.55)
    fig.text(0.5,0.012,"界面把“看起来怎样”和“几何是什么”分成两组控制，避免显示操作误改计算结果",ha="center",fontsize=6.1,color=COLORS["muted"])
    save_all(fig,"F23_显示属性与几何结果作用范围")


if __name__ == "__main__":
    packaging_flow()
    geoeast_roundtrip()
    display_scope()
