from __future__ import annotations

from figure_style import COLORS, arrow, box, canvas, horizontal_chain, save_all, stage_band


def packaging_flow() -> None:
    fig, ax = canvas(7.2, 4.1)
    labels = ["同一 Git 提交", "构建 CLI / 桌面软件\nLauncher / Runner / 插件", "自动测试与哈希核对", "生成安装器\n便携 ZIP / 插件包", "异机与中文路径烟测", "形成唯一交付清单"]
    fills = ["panel", "blue_light", "paper", "teal_light", "paper", "panel"]
    horizontal_chain(ax, labels, y=0.50, box_height=0.20, fills=fills)
    stage_band(ax, 0.04, 0.13, 0.42, 0.20, "一致性约束")
    ax.text(0.06,0.22,"源码提交、编译时间、Build ID、EXE 哈希和包哈希可追溯",fontsize=6.5)
    ax.text(0.06,0.16,"禁止混入旧版 EXE、个人配置、开发缓存和历史发布目录",fontsize=6.5)
    stage_band(ax, 0.54, 0.13, 0.42, 0.20, "客户机约束")
    ax.text(0.56,0.22,"安装目录不写配置；用户数据进入 LocalAppData / 文档目录",fontsize=6.5)
    ax.text(0.56,0.16,"Qt、VC、平台插件、示例、模板和帮助文档完整随包",fontsize=6.5)
    save_all(fig,"F21_Windows产品化构建与交付流程")


def geoeast_roundtrip() -> None:
    fig, ax = canvas(7.2, 4.15)
    box(ax,(0.05,0.59),(0.18,0.12),"当前工区\nBOMapGrid",fill="panel",edge="ink")
    box(ax,(0.31,0.59),(0.18,0.12),"BO Runner\n读取并转换 Grid2D",fill="blue_light",edge="blue")
    box(ax,(0.57,0.59),(0.18,0.12),"统一追踪核心\n生成正式多边形",fill="teal_light",edge="teal")
    box(ax,(0.79,0.59),(0.16,0.12),"写入\nBOFaultPolygon",fill="panel",edge="teal")
    for x0,x1 in [(0.23,0.31),(0.49,0.57),(0.75,0.79)]:arrow(ax,(x0,0.65),(x1,0.65),color="blue")
    box(ax,(0.64,0.24),(0.24,0.11),"重新读取对象列表",fill="paper",edge="line")
    box(ax,(0.36,0.24),(0.20,0.11),"插件真实图层查看",fill="blue_light",edge="blue")
    box(ax,(0.08,0.24),(0.20,0.11),"工区底图 / 平面成图",fill="paper",edge="line")
    arrow(ax,(0.87,0.59),(0.76,0.35),color="teal")
    arrow(ax,(0.64,0.295),(0.56,0.295),color="teal")
    arrow(ax,(0.36,0.295),(0.28,0.295),color="teal")
    ax.text(0.50,0.89,"数据库闭环以“读取—计算—写入—重新读取—显示”为完成条件",ha="center",fontsize=8,weight="bold")
    ax.text(0.50,0.09,"结果删除只作用于明确选中的 BOFaultPolygon，并在执行前二次确认",ha="center",fontsize=7,color=COLORS["muted"])
    save_all(fig,"F22_GeoEast数据库读写闭环")


def display_scope() -> None:
    fig, ax = canvas(7.2, 4.05)
    stage_band(ax,0.04,0.13,0.42,0.72,"显示属性（即时重绘）")
    for y,text in [(0.65,"颜色"),(0.48,"线宽"),(0.31,"透明度")]:
        box(ax,(0.10,y),(0.30,0.10),text,fill="blue_light" if y==0.65 else "paper",edge="blue")
    stage_band(ax,0.54,0.13,0.42,0.72,"计算与几何属性（需重新追踪）")
    for y,text in [(0.65,"阈值、连接与断层带参数"),(0.48,"面积阈值与自适应过滤"),(0.31,"简化、平滑与拓扑验收")]:
        box(ax,(0.60,y),(0.30,0.10),text,fill="teal_light" if y==0.65 else "paper",edge="teal")
    ax.text(0.25,0.19,"不改变 WKT、面积、数量或数据库对象",ha="center",fontsize=6.5,color=COLORS["muted"])
    ax.text(0.75,0.19,"可能改变边界坐标、对象数量和统计结果",ha="center",fontsize=6.5,color=COLORS["muted"])
    ax.text(0.50,0.92,"线条属性可修改，但显示调整与算法参数严格分离",ha="center",fontsize=8,weight="bold")
    save_all(fig,"F23_显示属性与几何结果作用范围")


if __name__ == "__main__":
    packaging_flow()
    geoeast_roundtrip()
    display_scope()

