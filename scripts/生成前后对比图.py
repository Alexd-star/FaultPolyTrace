import argparse
import csv
import os
import re

import matplotlib

matplotlib.use("Agg")
from matplotlib import font_manager
import matplotlib.pyplot as plt
import numpy as np

from wkt_geometry import parse_wkt_polygon_with_holes


def configure_chinese_font():
    candidates = [
        r"C:\Windows\Fonts\msyh.ttc",
        r"C:\Windows\Fonts\simhei.ttf",
        r"C:\Windows\Fonts\simsun.ttc",
    ]
    for path in candidates:
        if os.path.exists(path):
            font_manager.fontManager.addfont(path)
            font_name = font_manager.FontProperties(fname=path).get_name()
            plt.rcParams["font.sans-serif"] = [font_name, "DejaVu Sans"]
            plt.rcParams["axes.unicode_minus"] = False
            return


def read_official_dat(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            text = line.strip()
            if not text or text.startswith("#"):
                continue
            parts = text.split()
            if len(parts) < 5:
                continue
            rows.append(
                (
                    int(float(parts[0])),
                    int(float(parts[1])),
                    float(parts[2]),
                    float(parts[3]),
                    float(parts[4]),
                )
            )
    if not rows:
        raise RuntimeError(f"未读取到有效网格数据：{path}")

    arr = np.asarray(rows, dtype=float)
    lines = np.unique(arr[:, 0]).astype(int)
    cmps = np.unique(arr[:, 1]).astype(int)
    line_index = {v: i for i, v in enumerate(lines)}
    cmp_index = {v: i for i, v in enumerate(cmps)}

    values = np.full((len(lines), len(cmps)), np.nan, dtype=float)
    xs = np.full_like(values, np.nan)
    ys = np.full_like(values, np.nan)
    for line, cmp_no, x, y, value in rows:
        i = line_index[line]
        j = cmp_index[cmp_no]
        values[i, j] = value
        xs[i, j] = x
        ys[i, j] = y

    extent = (
        float(np.nanmin(xs)),
        float(np.nanmax(xs)),
        float(np.nanmin(ys)),
        float(np.nanmax(ys)),
    )
    return values.T, extent


def read_polygons(csv_path):
    polygons = []
    with open(csv_path, "r", encoding="utf-8", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row.get("status", "").lower() != "kept":
                continue
            coords, holes = parse_wkt_polygon_with_holes(row.get("wkt", ""))
            if coords:
                polygons.append({"coords": coords, "holes": holes})
    return polygons


def add_coordinate_grid(ax, extent):
    xmin, xmax, ymin, ymax = extent
    for x in np.linspace(xmin, xmax, 6):
        ax.axvline(x, color="#d6e2f0", linewidth=0.55, zorder=0)
    for y in np.linspace(ymin, ymax, 6):
        ax.axhline(y, color="#d6e2f0", linewidth=0.55, zorder=0)
    ax.set_xlim(xmin, xmax)
    ax.set_ylim(ymin, ymax)
    ax.tick_params(labelsize=7, colors="#6c737a")
    for spine in ax.spines.values():
        spine.set_color("#666666")
        spine.set_linewidth(0.8)


def draw_polygons(ax, polygons, color="#e60000", linewidth=0.62):
    for poly in polygons:
        coords = poly["coords"]
        if len(coords) < 3:
            continue
        xs = [p[0] for p in coords]
        ys = [p[1] for p in coords]
        ax.plot(xs, ys, color=color, linewidth=linewidth, alpha=0.95, zorder=3)
        for hole in poly.get("holes", []):
            hx = [p[0] for p in hole]
            hy = [p[1] for p in hole]
            ax.plot(hx, hy, color=color, linewidth=max(0.24, linewidth * 0.72), alpha=0.72, zorder=3, linestyle="--")


def render_compare(values, extent, polygons, output_path, title=True):
    finite = values[np.isfinite(values)]
    vmin = float(np.percentile(finite, 1.0))
    vmax = float(np.percentile(finite, 99.5))

    fig, axes = plt.subplots(1, 2, figsize=(13.5, 6.0), dpi=220)
    for ax in axes:
        ax.imshow(
            values,
            extent=extent,
            origin="lower",
            cmap="gray_r",
            interpolation="nearest",
            aspect="equal",
            vmin=vmin,
            vmax=vmax,
            zorder=1,
        )
        add_coordinate_grid(ax, extent)
        ax.set_xlabel("X", fontsize=8)
        ax.set_ylabel("Y", fontsize=8)

    if title:
        axes[0].set_title("处理前：平面断层属性", fontsize=11)
        axes[1].set_title(f"处理后：自动追踪断层多边形（{len(polygons)} 个）", fontsize=11)

    draw_polygons(axes[1], polygons)
    fig.tight_layout(w_pad=1.8)
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)


def render_single_overlay(values, extent, polygons, output_path):
    finite = values[np.isfinite(values)]
    fig, ax = plt.subplots(figsize=(9.5, 7.0), dpi=240)
    ax.imshow(
        values,
        extent=extent,
        origin="lower",
        cmap="gray_r",
        interpolation="nearest",
        aspect="equal",
        vmin=float(np.percentile(finite, 1.0)),
        vmax=float(np.percentile(finite, 99.5)),
        zorder=1,
    )
    add_coordinate_grid(ax, extent)
    draw_polygons(ax, polygons, linewidth=0.55)
    ax.set_title(f"FaultPolyTrace 结果叠加图：{len(polygons)} 个闭合多边形", fontsize=11)
    ax.set_xlabel("X", fontsize=8)
    ax.set_ylabel("Y", fontsize=8)
    fig.tight_layout()
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)


def main():
    configure_chinese_font()

    parser = argparse.ArgumentParser(description="生成赛题7前后对比验收图")
    parser.add_argument("--dat", required=True, help="官方 test.dat 路径")
    parser.add_argument("--csv", required=True, help="FaultPolyTrace 输出 CSV 路径")
    parser.add_argument("--out-dir", required=True, help="输出图片目录")
    args = parser.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    values, extent = read_official_dat(args.dat)
    polygons = read_polygons(args.csv)

    compare_png = os.path.join(args.out_dir, "03_前后对比图_原始属性_红色追踪结果.png")
    overlay_png = os.path.join(args.out_dir, "04_结果叠加图_红色断层多边形.png")
    render_compare(values, extent, polygons, compare_png)
    render_single_overlay(values, extent, polygons, overlay_png)

    print(f"前后对比图: {compare_png}")
    print(f"结果叠加图: {overlay_png}")
    print(f"网格尺寸: {values.shape[1]} x {values.shape[0]}")
    print(f"绘制多边形数量: {len(polygons)}")


if __name__ == "__main__":
    main()
