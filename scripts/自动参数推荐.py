import argparse
import csv
import json
import os
from collections import deque
from datetime import datetime
from pathlib import Path

import numpy as np


def read_geoeast_dat(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            text = line.strip()
            if not text or text.startswith("#"):
                continue
            parts = text.split()
            if len(parts) >= 5:
                rows.append((int(float(parts[0])), int(float(parts[1])), float(parts[2]), float(parts[3]), float(parts[4])))
    if not rows:
        raise RuntimeError(f"未读取到有效 GeoEast DAT 网格：{path}")

    arr = np.asarray(rows, dtype=float)
    line_keys = np.unique(arr[:, 0]).astype(int)
    cmp_keys = np.unique(arr[:, 1]).astype(int)
    line_index = {v: i for i, v in enumerate(line_keys)}
    cmp_index = {v: i for i, v in enumerate(cmp_keys)}
    values = np.zeros((len(cmp_keys), len(line_keys)), dtype=float)
    x_by_line = {}
    y_by_cmp = {}
    for line_no, cmp_no, x, y, value in rows:
        values[cmp_index[int(cmp_no)], line_index[int(line_no)]] = value
        x_by_line[int(line_no)] = x
        y_by_cmp[int(cmp_no)] = y
    xs = np.asarray([x_by_line[int(k)] for k in line_keys], dtype=float)
    ys = np.asarray([y_by_cmp[int(k)] for k in cmp_keys], dtype=float)
    dx = abs(xs[1] - xs[0]) if len(xs) > 1 else 1.0
    dy = abs(ys[1] - ys[0]) if len(ys) > 1 else 1.0
    return values, xs, ys, dx, dy


def connected_components(mask, values):
    height, width = mask.shape
    visited = np.zeros_like(mask, dtype=bool)
    offsets = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]
    components = []
    for y0 in range(height):
        for x0 in range(width):
            if not mask[y0, x0] or visited[y0, x0]:
                continue
            q = deque([(x0, y0)])
            visited[y0, x0] = True
            cells = []
            while q:
                x, y = q.popleft()
                cells.append((x, y))
                for ox, oy in offsets:
                    nx, ny = x + ox, y + oy
                    if 0 <= nx < width and 0 <= ny < height and mask[ny, nx] and not visited[ny, nx]:
                        visited[ny, nx] = True
                        q.append((nx, ny))
            arr = np.asarray(cells, dtype=int)
            min_x = int(arr[:, 0].min())
            max_x = int(arr[:, 0].max())
            min_y = int(arr[:, 1].min())
            max_y = int(arr[:, 1].max())
            bbox_width = max_x - min_x + 1
            bbox_height = max_y - min_y + 1
            aspect = max(bbox_width, bbox_height) / max(1, min(bbox_width, bbox_height))
            cell_values = values[arr[:, 1], arr[:, 0]]
            components.append(
                {
                    "cell_count": int(len(cells)),
                    "bbox_width": int(bbox_width),
                    "bbox_height": int(bbox_height),
                    "aspect": float(aspect),
                    "mean_value": float(np.mean(cell_values)),
                    "max_value": float(np.max(cell_values)),
                }
            )
    components.sort(key=lambda item: (item["cell_count"], item["max_value"]), reverse=True)
    return components


def percentile(values, p, default):
    if values.size == 0:
        return default
    index = int(np.floor(p * float(values.size - 1)))
    return float(np.partition(values, index)[index])


def clamp(value, low, high):
    return max(low, min(high, value))


def recommend(values, dx, dy):
    finite = values[np.isfinite(values)]
    nonzero = finite[finite > 0]
    total_cells = int(values.size)
    nonzero_ratio = float(nonzero.size / max(1, total_cells))
    p50 = percentile(finite, 0.50, 0.0)
    p66 = percentile(finite, 0.66, 0.0)
    p90 = percentile(finite, 0.90, 0.0)
    p95 = percentile(finite, 0.95, 0.0)
    p955 = percentile(finite, 0.955, 0.0)
    p965 = percentile(finite, 0.965, 0.0)
    p98 = percentile(finite, 0.98, 0.0)
    p985 = percentile(finite, 0.985, 0.0)
    p99 = percentile(finite, 0.99, 0.0)
    positive_p50 = percentile(nonzero, 0.50, 0.0)
    positive_p90 = percentile(nonzero, 0.90, 0.0)

    strong_threshold = p98 if p98 > 0.0 else max(0.50, positive_p90)
    weak_threshold = p965 if p965 > 0.0 and p965 < strong_threshold else max(0.35, positive_p50)
    high_mask = values >= strong_threshold
    weak_mask = values >= weak_threshold
    high_components = connected_components(high_mask, values)
    weak_components = connected_components(weak_mask, values)
    line_like_weak = [c for c in weak_components if c["cell_count"] >= 8 and c["aspect"] >= 3.0]
    high_sizes = [c["cell_count"] for c in high_components]
    weak_sizes = [c["cell_count"] for c in line_like_weak]
    median_high_size = float(np.median(high_sizes)) if high_sizes else 0.0
    median_weak_size = float(np.median(weak_sizes)) if weak_sizes else 0.0

    noise_score = 0
    reasons = []
    if nonzero_ratio > 0.12:
        noise_score += 1
        reasons.append("非零属性比例偏高，可能存在较多背景弱响应")
    tiny_high_ratio = float(sum(1 for c in high_components if c["cell_count"] <= 2) / max(1, len(high_components)))
    if nonzero_ratio > 0.08 and tiny_high_ratio > 0.55:
        noise_score += 1
        reasons.append("高值连通域以微小碎片为主，需要防止噪声进入正式结果")
    if p90 > 0 and p98 > 0 and (p98 - p90) < 0.08:
        noise_score += 1
        reasons.append("高分位属性差距较小，断层与背景分离度一般")
    if nonzero_ratio <= 0.08 and p98 > 0.0:
        reasons.append("非零属性呈稀疏线性分布，适合使用全网格分位数提取主断层响应")

    width_score = 0
    if line_like_weak:
        median_width = float(np.median([min(c["bbox_width"], c["bbox_height"]) for c in line_like_weak]))
        if median_width >= 14:
            width_score += 1
            reasons.append("弱响应线性带宽度偏宽，建议保留较宽断层带")
    else:
        median_width = 0.0

    base_belt = 3 + width_score
    if noise_score >= 2:
        base_belt = max(2, base_belt - 1)

    cell_area = abs(dx * dy)
    area_scale = max(cell_area, 1.0)
    auto_min_area = max(1.2 * area_scale, median_high_size * area_scale * 0.4, 80.0)
    auto_min_area = round(auto_min_area / 10.0) * 10.0

    balanced = {
        "trace-mode": "ridge-belt",
        "threshold-mode": "percentile",
        "percentile": 0.98 if noise_score < 2 else 0.985,
        "min-area": auto_min_area,
        "close-radius": 1,
        "weak-percentile": 0.965 if noise_score < 2 else 0.975,
        "weak-connect-radius": 1,
        "max-weak-component-cells": 20000,
        "min-weak-component-aspect": 3.0,
        "belt-radius": base_belt,
        "attribute-constrained-belt": True,
        "attribute-envelope-radius": 1,
        "post-belt-close-radius": 1,
        "min-branch-length": 4 if noise_score < 2 else 5,
        "min-junction-branch-length": 2,
        "min-junction-branch-mean": 0.94,
        "max-gap-distance": 12 if noise_score < 2 else 9,
        "max-gap-angle": 55,
        "min-gap-support-ratio": 0.25 if noise_score < 2 else 0.35,
        "separate-branch-belts": False,
        "root-anti-merge": False,
        "root-anti-merge-radius": 1,
        "root-anti-merge-support-ratio": 0.25,
        "root-anti-merge-min-part-cells": 0,
        "root-anti-merge-min-part-ratio": 0.12,
        "residual-trace": False,
        "simplify-tolerance": 2,
        "smooth-iterations": 1,
    }

    conservative = dict(balanced)
    conservative.update(
        {
            "percentile": min(0.995, balanced["percentile"] + 0.005),
            "weak-percentile": min(0.990, balanced["weak-percentile"] + 0.010),
            "min-area": max(balanced["min-area"], round(auto_min_area * 1.5 / 10.0) * 10.0),
            "belt-radius": max(2, base_belt - 1),
            "max-gap-distance": max(6, balanced["max-gap-distance"] - 3),
            "min-gap-support-ratio": min(0.60, balanced["min-gap-support-ratio"] + 0.10),
            "root-anti-merge": True,
        }
    )

    high_recall = dict(balanced)
    high_recall.update(
        {
            "percentile": max(0.940, balanced["percentile"] - 0.020),
            "weak-percentile": max(0.940, balanced["weak-percentile"] - 0.010),
            "min-area": max(20.0, round(auto_min_area * 0.65 / 10.0) * 10.0),
            "belt-radius": min(5, base_belt + 1),
            "max-gap-distance": min(16, balanced["max-gap-distance"] + 3),
            "min-gap-support-ratio": max(0.15, balanced["min-gap-support-ratio"] - 0.10),
            "residual-trace": True,
            "residual-trace-min-length": 6,
            "residual-trace-max-cells": 80,
        }
    )

    return {
        "grid": {
            "nx": int(values.shape[1]),
            "ny": int(values.shape[0]),
            "dx": float(dx),
            "dy": float(dy),
            "cell_area": float(cell_area),
        },
        "attribute": {
            "total_cells": total_cells,
            "nonzero_cells": int(nonzero.size),
            "nonzero_ratio": nonzero_ratio,
            "p50": p50,
            "p66": p66,
            "p90": p90,
            "p95": p95,
            "p955": p955,
            "p965": p965,
            "p98": p98,
            "p985": p985,
            "p99": p99,
            "positive_p50": positive_p50,
            "positive_p90": positive_p90,
        },
        "linearity": {
            "high_component_count": len(high_components),
            "weak_line_component_count": len(line_like_weak),
            "median_high_component_cells": median_high_size,
            "median_weak_line_cells": median_weak_size,
            "median_weak_line_width_cells": median_width,
            "tiny_high_component_ratio": tiny_high_ratio,
        },
        "diagnosis": {
            "noise_score": noise_score,
            "width_score": width_score,
            "reasons": reasons or ["属性分布清晰，默认均衡参数即可作为首选"],
        },
        "presets": {
            "稳健防误合并": conservative,
            "均衡推荐": balanced,
            "高召回复核": high_recall,
        },
    }


def write_ini(path, preset):
    lines = []
    for key, value in preset.items():
        if isinstance(value, bool):
            value = "true" if value else "false"
        lines.append(f"{key}={value}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def format_value(value):
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, float):
        text = f"{value:.6f}".rstrip("0").rstrip(".")
        return text if text else "0"
    return str(value)


def write_report(path, dat_path, result):
    lines = [
        "# 自动参数推荐报告",
        "",
        f"- 生成时间：{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        f"- 输入数据：`{dat_path}`",
        "",
        "## 1. 数据画像",
        "",
        "| 指标 | 数值 |",
        "|---|---:|",
        f"| 网格规模 | {result['grid']['nx']} x {result['grid']['ny']} |",
        f"| 网格间距 | {result['grid']['dx']:.3f} x {result['grid']['dy']:.3f} |",
        f"| 非零属性比例 | {result['attribute']['nonzero_ratio']:.4%} |",
        f"| 全网格属性 P96.5/P98/P98.5 | {result['attribute']['p965']:.4f} / {result['attribute']['p98']:.4f} / {result['attribute']['p985']:.4f} |",
        f"| 非零属性 P50/P90 | {result['attribute']['positive_p50']:.4f} / {result['attribute']['positive_p90']:.4f} |",
        f"| 高值连通域数量 | {result['linearity']['high_component_count']} |",
        f"| 弱线性连通域数量 | {result['linearity']['weak_line_component_count']} |",
        f"| 弱线性中位宽度 | {result['linearity']['median_weak_line_width_cells']:.2f} cells |",
        "",
        "## 2. 推荐判断",
        "",
    ]
    for reason in result["diagnosis"]["reasons"]:
        lines.append(f"- {reason}")
    lines.extend(
        [
            "",
            "## 3. 三档参数",
            "",
            "### 3.1 均衡推荐",
            "",
            "适合正式交付首跑，优先保持召回率、连贯性和拓扑稳定。",
            "",
            "```text",
        ]
    )
    for key, value in result["presets"]["均衡推荐"].items():
        lines.append(f"{key} = {format_value(value)}")
    lines.extend(["```", "", "### 3.2 稳健防误合并", "", "适合近邻断层多、误合并风险高的工区。该档会更保守，可能漏掉少量弱细线。", "", "```text"])
    for key, value in result["presets"]["稳健防误合并"].items():
        lines.append(f"{key} = {format_value(value)}")
    lines.extend(["```", "", "### 3.3 高召回复核", "", "适合先找全弱响应，再由质控图和面积阈值筛查。该档不建议直接作为最终正式提交。", "", "```text"])
    for key, value in result["presets"]["高召回复核"].items():
        lines.append(f"{key} = {format_value(value)}")
    lines.extend(
        [
            "```",
            "",
            "## 4. 使用建议",
            "",
            "1. 首先运行“均衡推荐”，检查强弱覆盖率、拓扑和疑似误合并候选。",
            "2. 若邻近断层被合并，运行“稳健防误合并”作对照，不直接牺牲主线召回率。",
            "3. 若弱细线漏检较多，运行“高召回复核”生成复核层，再由面积阈值和局部图判断是否进入正式结果。",
            "4. 三档参数都必须进入时间戳输出目录，保留诊断 JSON、质量图和复核报告。",
        ]
    )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="根据断层属性数据自动推荐 FaultPolyTrace 参数")
    parser.add_argument("--dat", required=True, help="GeoEast DAT 输入")
    parser.add_argument("--out-dir", default="", help="输出目录")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = Path(args.out_dir) if args.out_dir else repo_root / "examples" / "outputs" / f"auto_params_{timestamp}"
    out_dir.mkdir(parents=True, exist_ok=True)

    values, _, _, dx, dy = read_geoeast_dat(args.dat)
    result = recommend(values, dx, dy)
    (out_dir / "自动参数推荐.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    write_report(out_dir / "自动参数推荐报告.md", args.dat, result)
    for name, preset in result["presets"].items():
        write_ini(out_dir / f"{name}.ini", preset)

    with open(out_dir / "参数画像.csv", "w", encoding="utf-8-sig", newline="") as f:
        fields = ["category", "name", "value"]
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for category in ["grid", "attribute", "linearity", "diagnosis"]:
            for key, value in result[category].items():
                if isinstance(value, list):
                    value = "；".join(str(item) for item in value)
                writer.writerow({"category": category, "name": key, "value": value})

    print(out_dir / "自动参数推荐报告.md")
    print(out_dir / "自动参数推荐.json")


if __name__ == "__main__":
    main()
