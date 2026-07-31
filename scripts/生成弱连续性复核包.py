# -*- coding: utf-8 -*-
"""Generate reproducible A/B and per-connection review figures.

The script never changes tracing results. It converts constrained weak-path
evidence into full-map, tile, and local figures for human promotion review.
"""

import argparse
import csv
import json
import math
import os
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib import font_manager
from matplotlib.patches import Polygon as PolygonPatch
import matplotlib.image as mpimg
import matplotlib.pyplot as plt
import numpy as np

from 局部形态质量诊断 import read_geoeast_dat, read_polygons
from wkt_geometry import rasterize_polygon

csv.field_size_limit(min(sys.maxsize, 2_147_483_647))


def configure_font():
    for path in [r"C:\Windows\Fonts\msyh.ttc", r"C:\Windows\Fonts\simhei.ttf"]:
        if os.path.exists(path):
            font_manager.fontManager.addfont(path)
            plt.rcParams["font.sans-serif"] = [font_manager.FontProperties(fname=path).get_name(), "DejaVu Sans"]
            plt.rcParams["axes.unicode_minus"] = False
            break


def polygon_bbox(polygon):
    points = np.asarray(polygon["coords"], dtype=float)
    return points[:, 0].min(), points[:, 0].max(), points[:, 1].min(), points[:, 1].max()


def overlaps(a, b):
    return not (a[1] < b[0] or a[0] > b[1] or a[3] < b[2] or a[2] > b[3])


def draw_attribute(ax, values, extent):
    layer = np.ma.masked_where(values <= 0, values)
    ax.imshow(layer, extent=extent, origin="lower", cmap="viridis", interpolation="nearest",
              vmin=0.0, vmax=1.0, aspect="equal", alpha=0.82)
    ax.set_facecolor("white")


def draw_polygons(ax, polygons, view=None, color="#0F4D92", width=0.7, fill=False):
    count = 0
    for polygon in polygons:
        if view and not overlaps(polygon_bbox(polygon), view):
            continue
        points = np.asarray(polygon["coords"], dtype=float)
        face = color if fill else "none"
        alpha = 0.10 if fill else 0.96
        ax.add_patch(PolygonPatch(points, closed=True, facecolor=face, edgecolor=color,
                                  linewidth=width, alpha=alpha))
        for hole in polygon.get("holes", []):
            hole = np.asarray(hole, dtype=float)
            ax.plot(hole[:, 0], hole[:, 1], color=color, linewidth=max(0.35, width * 0.7), linestyle="--")
        count += 1
    return count


def style_axis(ax, extent, label=True):
    ax.set_xlim(extent[0], extent[1])
    ax.set_ylim(extent[2], extent[3])
    if label:
        ax.set_xlabel("X", fontsize=8)
        ax.set_ylabel("Y", fontsize=8)
    ax.tick_params(labelsize=7, length=2, colors="#333333")
    for spine in ax.spines.values():
        spine.set_color("#555555")
        spine.set_linewidth(0.65)


def save_overview_bundle(fig, base_path, png_dpi=600):
    """Save one review overview as zoomable vector and 12k-class raster files."""
    fig.savefig(base_path.with_suffix(".png"), dpi=png_dpi, bbox_inches="tight", facecolor="white")
    fig.savefig(base_path.with_suffix(".svg"), bbox_inches="tight", facecolor="white")
    fig.savefig(base_path.with_suffix(".pdf"), bbox_inches="tight", facecolor="white")


def parse_path(text):
    points = []
    for item in (text or "").split(";"):
        if not item:
            continue
        x, y = item.split(":", 1)
        points.append((int(x), int(y)))
    return points


def read_candidates(path):
    candidates = []
    with open(path, "r", encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            row["path"] = parse_path(row.get("path_grid_xy", ""))
            row["accepted"] = row.get("accepted") == "1"
            for endpoint in ("a", "b"):
                for axis in ("x", "y"):
                    legacy_key = f"endpoint_{endpoint}_{axis}"
                    grid_key = f"endpoint_{endpoint}_grid_{axis}"
                    if legacy_key not in row and grid_key in row:
                        row[legacy_key] = row[grid_key]
            candidates.append(row)
    return candidates


def path_world(candidate, xs, ys):
    valid = [(x, y) for x, y in candidate["path"] if 0 <= x < len(xs) and 0 <= y < len(ys)]
    return np.asarray([(xs[x], ys[y]) for x, y in valid], dtype=float)


def render_overviews(out_dir, values, extent, baseline, candidate, accepted, xs, ys):
    fig, axes = plt.subplots(1, 3, figsize=(20.0, 6.6), dpi=240)
    draw_attribute(axes[0], values, extent)
    axes[0].set_title("a  平面断层属性", loc="left", fontsize=10)
    draw_attribute(axes[1], values, extent)
    draw_polygons(axes[1], baseline, color="#3C5488", width=0.42)
    axes[1].set_title(f"b  基线结果（{len(baseline)} 个）", loc="left", fontsize=10)
    draw_attribute(axes[2], values, extent)
    draw_polygons(axes[2], candidate, color="#0F4D92", width=0.42)
    axes[2].set_title(f"c  弱连续性候选（{len(candidate)} 个）", loc="left", fontsize=10)
    for ax in axes:
        style_axis(ax, extent)
    fig.tight_layout(w_pad=1.2)
    save_overview_bundle(fig, out_dir / "01_全图属性_基线_候选对比")
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(20.0, 9.5), dpi=260)
    draw_attribute(ax, values, extent)
    draw_polygons(ax, baseline, color="#6A6A6A", width=0.40)
    draw_polygons(ax, candidate, color="#0F4D92", width=0.36)
    for item in accepted:
        points = path_world(item, xs, ys)
        if len(points) >= 2:
            ax.plot(points[:, 0], points[:, 1], color="#D55E00", linewidth=0.75, alpha=0.95)
    style_axis(ax, extent)
    ax.set_title("候选变化总览：灰色为基线，蓝色为候选，橙色为受约束弱路径", fontsize=10)
    fig.tight_layout()
    save_overview_bundle(fig, out_dir / "02_全图新增弱路径差异")
    plt.close(fig)

    for name, polygons in [("03_基线纯多边形总览", baseline), ("04_候选纯多边形总览", candidate)]:
        fig, ax = plt.subplots(figsize=(20.0, 9.5), dpi=260)
        draw_polygons(ax, polygons, color="#0F4D92", width=0.48)
        style_axis(ax, extent)
        ax.set_facecolor("white")
        fig.tight_layout()
        save_overview_bundle(fig, out_dir / name)
        plt.close(fig)

    fig, ax = plt.subplots(figsize=(20.0, 9.5), dpi=260)
    draw_attribute(ax, values, extent)
    draw_polygons(ax, candidate, color="#FFFFFF", width=1.05)
    draw_polygons(ax, candidate, color="#B2182B", width=0.48)
    style_axis(ax, extent)
    fig.tight_layout()
    save_overview_bundle(fig, out_dir / "05_候选多边形叠加原始属性网格高清总览")
    plt.close(fig)


def local_extent(points, xs, ys, dx, dy, padding_cells=18):
    xmin = points[:, 0].min() - padding_cells * dx
    xmax = points[:, 0].max() + padding_cells * dx
    ymin = points[:, 1].min() - padding_cells * dy
    ymax = points[:, 1].max() + padding_cells * dy
    return xmin, xmax, ymin, ymax


def render_local_figures(out_dir, values, extent, xs, ys, dx, dy, baseline, candidate, accepted):
    paths = []
    for order, item in enumerate(accepted, start=1):
        points = path_world(item, xs, ys)
        if len(points) < 2:
            continue
        view = local_extent(points, xs, ys, dx, dy)
        fig, axes = plt.subplots(1, 3, figsize=(12.4, 4.1), dpi=250)
        draw_attribute(axes[0], values, extent)
        axes[0].plot(points[:, 0], points[:, 1], color="#D55E00", linewidth=1.15)
        axes[0].scatter(points[[0, -1], 0], points[[0, -1], 1], s=14, color="#111111", zorder=5)
        axes[0].set_title("a  属性与最小代价路径", loc="left", fontsize=9)
        draw_attribute(axes[1], values, extent)
        draw_polygons(axes[1], baseline, view=view, color="#3C5488", width=0.85)
        axes[1].set_title("b  基线局部结果", loc="left", fontsize=9)
        draw_attribute(axes[2], values, extent)
        draw_polygons(axes[2], candidate, view=view, color="#0F4D92", width=0.85)
        axes[2].plot(points[:, 0], points[:, 1], color="#D55E00", linewidth=0.9)
        axes[2].set_title("c  候选局部结果", loc="left", fontsize=9)
        for ax in axes:
            style_axis(ax, view)
        fig.suptitle(
            f"连接 {item['candidate_id']} | 距离 {float(item['endpoint_distance']):.1f} | "
            f"支撑率 {float(item['support_ratio']):.2f} | 最大空白 {item['max_unsupported_run']} 格",
            fontsize=9.5, y=1.01)
        fig.tight_layout()
        path = out_dir / f"连接_{order:02d}_候选{int(item['candidate_id']):03d}.png"
        fig.savefig(path, bbox_inches="tight", facecolor="white")
        plt.close(fig)
        paths.append(path)
    return paths


def render_contact_sheet(paths, output, columns=4):
    rows = max(1, math.ceil(len(paths) / columns))
    fig, axes = plt.subplots(rows, columns, figsize=(columns * 3.2, rows * 2.1), dpi=180)
    axes = np.asarray(axes).reshape(rows, columns)
    for index, ax in enumerate(axes.flat):
        ax.axis("off")
        if index < len(paths):
            ax.imshow(mpimg.imread(paths[index]))
            ax.set_title(paths[index].stem, fontsize=7, pad=2)
    fig.tight_layout(pad=0.35)
    fig.savefig(output, bbox_inches="tight", facecolor="white")
    plt.close(fig)


def rasterize(polygons, xs, ys, dx, dy):
    mask = np.zeros((len(ys), len(xs)), dtype=bool)
    for polygon in polygons:
        mask |= rasterize_polygon(polygon["coords"], len(xs), len(ys), xs, ys, dx, dy, polygon.get("holes", []))
    return mask


def metrics(values, mask, strong=0.95, weak=0.66):
    def coverage(threshold):
        target = values >= threshold
        return float(np.count_nonzero(mask & target) / max(1, np.count_nonzero(target)))
    covered = values[mask]
    return {
        "polygon_cells": int(np.count_nonzero(mask)),
        "strong_coverage": coverage(strong),
        "weak_coverage": coverage(weak),
        "nonzero_coverage": float(np.count_nonzero(mask & (values > 0)) / max(1, np.count_nonzero(values > 0))),
        "background_cells": int(np.count_nonzero(mask & (values <= 0))),
        "polygon_mean_attribute": float(np.mean(covered)) if covered.size else 0.0,
    }


def write_reports(out_dir, baseline, candidate, accepted, all_candidates, values, xs, ys, dx, dy):
    baseline_metrics = metrics(values, rasterize(baseline, xs, ys, dx, dy))
    candidate_metrics = metrics(values, rasterize(candidate, xs, ys, dx, dy))
    summary = {
        "status": "PENDING_HUMAN_REVIEW",
        "baseline_polygons": len(baseline),
        "candidate_polygons": len(candidate),
        "accepted_connections": len(accepted),
        "evaluated_candidates": len(all_candidates),
        "baseline": baseline_metrics,
        "candidate": candidate_metrics,
        "delta": {key: candidate_metrics[key] - baseline_metrics[key] for key in baseline_metrics},
    }
    (out_dir / "弱连续性A_B摘要.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# 受约束弱断层连续追踪人工复核报告",
        "",
        "本轮结果是候选实验，不自动替换正式基线。晋级前必须逐图确认新增连接具有方向连续性和弱属性支撑，且没有把相邻独立断层错误合并。",
        "",
        "## A/B 指标",
        "",
        f"| 指标 | 工区基线（{len(baseline)} 个） | 弱连续性候选 | 变化 |",
        "|---|---:|---:|---:|",
        f"| 保留多边形 | {len(baseline)} | {len(candidate)} | {len(candidate)-len(baseline):+d} |",
        f"| 强响应覆盖率 | {baseline_metrics['strong_coverage']:.6%} | {candidate_metrics['strong_coverage']:.6%} | {candidate_metrics['strong_coverage']-baseline_metrics['strong_coverage']:+.6%} |",
        f"| 弱响应覆盖率 | {baseline_metrics['weak_coverage']:.6%} | {candidate_metrics['weak_coverage']:.6%} | {candidate_metrics['weak_coverage']-baseline_metrics['weak_coverage']:+.6%} |",
        f"| 非零属性覆盖率 | {baseline_metrics['nonzero_coverage']:.6%} | {candidate_metrics['nonzero_coverage']:.6%} | {candidate_metrics['nonzero_coverage']-baseline_metrics['nonzero_coverage']:+.6%} |",
        f"| 零属性背景像元 | {baseline_metrics['background_cells']} | {candidate_metrics['background_cells']} | {candidate_metrics['background_cells']-baseline_metrics['background_cells']:+d} |",
        "",
        "## 逐连接人工结论",
        "",
        "| 序号 | 候选 | 端点A | 端点B | 距离 | 支撑率 | 平均支撑 | 最大空白 | 转角 | 图件 | 人工结论 |",
        "|---:|---:|---|---|---:|---:|---:|---:|---:|---|---|",
    ]
    for order, item in enumerate(accepted, start=1):
        image = f"局部连接复核/连接_{order:02d}_候选{int(item['candidate_id']):03d}.png"
        lines.append(
            f"| {order} | {item['candidate_id']} | ({item['endpoint_a_x']},{item['endpoint_a_y']}) | "
            f"({item['endpoint_b_x']},{item['endpoint_b_y']}) | {float(item['endpoint_distance']):.2f} | "
            f"{float(item['support_ratio']):.3f} | {float(item['mean_support']):.3f} | "
            f"{item['max_unsupported_run']} | {float(item['max_turn_angle']):.1f}° | [{Path(image).name}]({image}) | 待复核 |"
        )
    lines.extend([
        "",
        "## 晋级门控",
        "",
        f"- [ ] {len(accepted)} 条新增连接已逐张复核，方向、属性和局部形态均合理。",
        "- [ ] 全图 24 个分块已逐张复核，无新增误合并、异常短桥或不自然宽带。",
        "- [ ] 强、弱响应覆盖不下降，零属性背景泄漏未异常增加。",
        "- [ ] 开放环、自相交和多边形异常相交均为 0。",
        "- [ ] 处理耗时低于 1 分钟，重复运行结果确定一致。",
        "",
        "只有全部项目通过，候选参数才可考虑晋级；否则继续保留当前正式基线并收紧对应门控。",
    ])
    (out_dir / "人工复核与晋级报告.md").write_text("\n".join(lines), encoding="utf-8")


def write_geoeast_review_coordinates(out_dir, accepted, xs, ys, dx, dy):
    fields = [
        "candidate_id", "center_x", "center_y",
        "endpoint_a_x", "endpoint_a_y", "endpoint_b_x", "endpoint_b_y",
        "view_xmin", "view_xmax", "view_ymin", "view_ymax",
        "endpoint_distance_cells", "support_ratio", "mean_support",
        "max_unsupported_run", "manual_result", "review_note",
    ]
    rows = []
    for item in accepted:
        points = path_world(item, xs, ys)
        if len(points) < 2:
            continue
        endpoint_a_x = item.get("endpoint_a_x", item.get("endpoint_a_grid_x"))
        endpoint_a_y = item.get("endpoint_a_y", item.get("endpoint_a_grid_y"))
        endpoint_b_x = item.get("endpoint_b_x", item.get("endpoint_b_grid_x"))
        endpoint_b_y = item.get("endpoint_b_y", item.get("endpoint_b_grid_y"))
        padding_x = 18.0 * dx
        padding_y = 18.0 * dy
        rows.append({
            "candidate_id": int(item["candidate_id"]),
            "center_x": float(np.mean(points[:, 0])),
            "center_y": float(np.mean(points[:, 1])),
            "endpoint_a_x": float(xs[int(endpoint_a_x)]),
            "endpoint_a_y": float(ys[int(endpoint_a_y)]),
            "endpoint_b_x": float(xs[int(endpoint_b_x)]),
            "endpoint_b_y": float(ys[int(endpoint_b_y)]),
            "view_xmin": float(points[:, 0].min() - padding_x),
            "view_xmax": float(points[:, 0].max() + padding_x),
            "view_ymin": float(points[:, 1].min() - padding_y),
            "view_ymax": float(points[:, 1].max() + padding_y),
            "endpoint_distance_cells": float(item["endpoint_distance"]),
            "support_ratio": float(item["support_ratio"]),
            "mean_support": float(item["mean_support"]),
            "max_unsupported_run": int(item["max_unsupported_run"]),
            "manual_result": "待复核",
            "review_note": "",
        })

    csv_path = out_dir / "GeoEast工区底图复核坐标.csv"
    with open(csv_path, "w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    lines = [
        "# GeoEast 工区底图复核坐标索引",
        "",
        "GeoEast 工区底图是本轮人工复核的优先环境。底图能够同时显示断层属性和多边形边界，状态栏给出真实 X/Y 坐标，放大后可直接检查像元级支撑、端点方向、邻近分支和边界包络。静态三联图用于归档，不替代工区内交互复核。",
        "",
        "## 推荐操作",
        "",
        "1. 在同一工区同时保留基线对象和弱连续性候选对象，使用不同名称，禁止覆盖基线。",
        "2. 勾选输入断层属性网格，并将候选多边形设置为高对比边界色；必要时同时显示基线层作 A/B 比较。",
        "3. 按下表中心坐标定位，将视图放大到单个网格点清晰可见。",
        "4. 检查端点之间是否存在连续或点状弱属性、方向是否一致、是否靠近竞争分支、连接后是否形成异常宽带。",
        "5. 在 CSV 的 `manual_result` 和 `review_note` 中记录“通过、拒绝、待讨论”及原因。",
        "",
        "## 坐标表",
        "",
        "| 候选 | 中心 X | 中心 Y | 端点 A | 端点 B | 建议查看范围 |",
        "|---:|---:|---:|---|---|---|",
    ]
    for row in rows:
        lines.append(
            f"| {row['candidate_id']} | {row['center_x']:.2f} | {row['center_y']:.2f} | "
            f"({row['endpoint_a_x']:.2f}, {row['endpoint_a_y']:.2f}) | "
            f"({row['endpoint_b_x']:.2f}, {row['endpoint_b_y']:.2f}) | "
            f"X={row['view_xmin']:.2f}~{row['view_xmax']:.2f}; "
            f"Y={row['view_ymin']:.2f}~{row['view_ymax']:.2f} |"
        )
    (out_dir / "GeoEast工区底图复核说明.md").write_text("\n".join(lines), encoding="utf-8")


def main():
    configure_font()
    parser = argparse.ArgumentParser(description="生成受约束弱连续性A/B与逐连接复核图")
    parser.add_argument("--dat", required=True)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--candidate-evidence", required=True)
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    local_dir = out_dir / "局部连接复核"
    out_dir.mkdir(parents=True, exist_ok=True)
    local_dir.mkdir(parents=True, exist_ok=True)
    values, xs, ys, dx, dy, extent = read_geoeast_dat(args.dat)
    baseline = read_polygons(args.baseline)
    candidate = read_polygons(args.candidate)
    all_candidates = read_candidates(args.candidate_evidence)
    accepted = [item for item in all_candidates if item["accepted"]]
    if values.size == 0 or not np.any(np.isfinite(values)):
        raise RuntimeError("输入属性网格为空，停止生成复核图。")
    if not baseline or not candidate:
        raise RuntimeError("基线或候选多边形为空，停止生成复核图。")
    if not accepted:
        raise RuntimeError("没有可复核的弱连续性候选，停止生成复核图。")

    render_overviews(out_dir, values, extent, baseline, candidate, accepted, xs, ys)
    local_paths = render_local_figures(local_dir, values, extent, xs, ys, dx, dy, baseline, candidate, accepted)
    render_contact_sheet(local_paths, out_dir / "06_全部新增连接复核联系表.png")
    write_geoeast_review_coordinates(out_dir, accepted, xs, ys, dx, dy)
    write_reports(out_dir, baseline, candidate, accepted, all_candidates, values, xs, ys, dx, dy)
    print(out_dir)
    print(f"accepted_connections={len(accepted)}")


if __name__ == "__main__":
    main()
