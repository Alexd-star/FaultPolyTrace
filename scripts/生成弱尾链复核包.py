#!/usr/bin/env python3
"""Generate full-map and per-tail figures for chain-level tail restoration."""

from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.image as mpimg
import matplotlib.pyplot as plt
import numpy as np

from 局部形态质量诊断 import read_geoeast_dat, read_polygons
from 生成弱连续性复核包 import (
    configure_font,
    draw_attribute,
    draw_polygons,
    local_extent,
    style_axis,
)


def parse_cells(value: str) -> list[tuple[int, int]]:
    cells: list[tuple[int, int]] = []
    for item in (value or "").split(";"):
        if not item:
            continue
        x_text, y_text = item.split(":", 1)
        cells.append((int(x_text), int(y_text)))
    return cells


def read_accepted(path: Path) -> list[dict[str, object]]:
    accepted: list[dict[str, object]] = []
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            if row.get("accepted") != "1":
                continue
            item: dict[str, object] = dict(row)
            item["cells"] = parse_cells(row.get("cells_grid_xy", ""))
            accepted.append(item)
    return accepted


def world_points(item: dict[str, object], xs: np.ndarray, ys: np.ndarray) -> np.ndarray:
    cells = item["cells"]
    return np.asarray(
        [(xs[x], ys[y]) for x, y in cells if 0 <= x < len(xs) and 0 <= y < len(ys)],
        dtype=float,
    )


def render_overview(
    output: Path,
    values: np.ndarray,
    extent: tuple[float, float, float, float],
    polygons: list[dict],
    tails: list[dict[str, object]],
    xs: np.ndarray,
    ys: np.ndarray,
) -> None:
    fig, ax = plt.subplots(figsize=(12.0, 5.8), dpi=260)
    draw_attribute(ax, values, extent)
    draw_polygons(ax, polygons, color="#0F4D92", width=0.42)
    for item in tails:
        points = world_points(item, xs, ys)
        if len(points):
            ax.scatter(points[:, 0], points[:, 1], s=3.2, color="#D55E00", zorder=5)
    style_axis(ax, extent)
    ax.set_title("链级端部恢复总览：蓝色为候选边界，橙色为恢复尾链", fontsize=10)
    fig.tight_layout()
    fig.savefig(output, bbox_inches="tight", facecolor="white")
    plt.close(fig)


def render_local(
    out_dir: Path,
    values: np.ndarray,
    extent: tuple[float, float, float, float],
    xs: np.ndarray,
    ys: np.ndarray,
    dx: float,
    dy: float,
    baseline: list[dict],
    candidate: list[dict],
    tails: list[dict[str, object]],
) -> list[Path]:
    paths: list[Path] = []
    for order, item in enumerate(tails, start=1):
        points = world_points(item, xs, ys)
        if len(points) < 2:
            continue
        view = local_extent(points, xs, ys, dx, dy, padding_cells=18)
        fig, axes = plt.subplots(1, 3, figsize=(12.4, 4.1), dpi=250)
        draw_attribute(axes[0], values, extent)
        axes[0].scatter(points[:, 0], points[:, 1], s=11, color="#D55E00", zorder=5)
        axes[0].set_title("a  属性与恢复尾链", loc="left", fontsize=9)
        draw_attribute(axes[1], values, extent)
        draw_polygons(axes[1], baseline, view=view, color="#3C5488", width=0.85)
        axes[1].set_title("b  基线局部结果", loc="left", fontsize=9)
        draw_attribute(axes[2], values, extent)
        draw_polygons(axes[2], candidate, view=view, color="#0F4D92", width=0.85)
        axes[2].scatter(points[:, 0], points[:, 1], s=7, color="#D55E00", zorder=5)
        axes[2].set_title("c  候选局部结果", loc="left", fontsize=9)
        for ax in axes:
            style_axis(ax, view)
        fig.suptitle(
            f"尾链 {item['candidate_id']} | {item['cell_count']} 格 | "
            f"长宽比 {float(item['aspect']):.2f} | 均值 {float(item['mean_value']):.3f} | "
            f"方向差 {float(item['alignment_angle']):.1f}°",
            fontsize=9.5,
            y=1.01,
        )
        fig.tight_layout()
        path = out_dir / f"尾链_{order:02d}_候选{int(item['candidate_id']):04d}.png"
        fig.savefig(path, bbox_inches="tight", facecolor="white")
        plt.close(fig)
        paths.append(path)
    return paths


def render_contact_sheet(paths: list[Path], output: Path, columns: int = 4) -> None:
    rows = max(1, math.ceil(len(paths) / columns))
    fig, axes = plt.subplots(rows, columns, figsize=(columns * 3.25, rows * 2.15), dpi=180)
    axes = np.asarray(axes).reshape(rows, columns)
    for index, ax in enumerate(axes.flat):
        ax.axis("off")
        if index < len(paths):
            ax.imshow(mpimg.imread(paths[index]))
            ax.set_title(paths[index].stem, fontsize=7, pad=2)
    fig.tight_layout(pad=0.35)
    fig.savefig(output, bbox_inches="tight", facecolor="white")
    plt.close(fig)


def write_report(output: Path, tails: list[dict[str, object]], paths: list[Path]) -> None:
    lines = [
        "# 链级端部恢复人工复核表",
        "",
        "尾链只有在与一个既有对象相接、整体细长、方向连续且属性支撑充分时才进入候选结果。多边形数量不是通过标准；每条尾链仍需核对是否属于同一断层轨迹。",
        "",
        "| 序号 | 候选 | 像元 | 长宽比 | 属性均值 | 属性峰值 | 方向差 | 图件 | 人工结论 |",
        "|---:|---:|---:|---:|---:|---:|---:|---|---|",
    ]
    for order, (item, path) in enumerate(zip(tails, paths), start=1):
        lines.append(
            f"| {order} | {item['candidate_id']} | {item['cell_count']} | "
            f"{float(item['aspect']):.2f} | {float(item['mean_value']):.3f} | "
            f"{float(item['max_value']):.3f} | {float(item['alignment_angle']):.1f}° | "
            f"[{path.name}](局部尾链复核/{path.name}) | 待复核 |"
        )
    lines.extend(
        [
            "",
            "## 通过标准",
            "",
            "- 尾链与主体在像元上相接，主方向延续自然。",
            "- 尾链宽度与主体相近，未横向吞入邻近轨迹。",
            "- 属性沿链连续或仅有短距离弱化，周围背景有清晰隔离。",
            "- 恢复后未形成异常鼓包、内环、自相交或对象间异常相交。",
        ]
    )
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    configure_font()
    parser = argparse.ArgumentParser()
    parser.add_argument("--dat", required=True)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--tail-evidence", required=True)
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    local_dir = out_dir / "局部尾链复核"
    local_dir.mkdir(parents=True, exist_ok=True)
    values, xs, ys, dx, dy, extent = read_geoeast_dat(args.dat)
    baseline = read_polygons(args.baseline)
    candidate = read_polygons(args.candidate)
    tails = read_accepted(Path(args.tail_evidence))
    render_overview(out_dir / "01_弱尾链恢复总览.png", values, extent, candidate, tails, xs, ys)
    paths = render_local(
        local_dir, values, extent, xs, ys, dx, dy, baseline, candidate, tails
    )
    render_contact_sheet(paths, out_dir / "02_全部弱尾链复核联系表.png")
    write_report(out_dir / "链级端部恢复人工复核表.md", tails, paths)
    (out_dir / "链级端部恢复摘要.json").write_text(
        json.dumps(
            {"status": "PENDING_HUMAN_REVIEW", "acceptedTailChains": len(tails)},
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print(out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
