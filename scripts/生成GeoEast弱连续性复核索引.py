#!/usr/bin/env python3
"""Generate a GeoEast coordinate index from the actual BOMapGrid trace run."""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path
from statistics import median


def parse_path(value: str) -> list[tuple[float, float]]:
    points: list[tuple[float, float]] = []
    for item in (value or "").split(";"):
        if not item:
            continue
        x_text, y_text = item.split(":", 1)
        points.append((float(x_text), float(y_text)))
    return points


def infer_spacing(rows: list[dict[str, str]]) -> tuple[float, float]:
    dx_values: list[float] = []
    dy_values: list[float] = []
    for row in rows:
        grid_points = parse_path(row.get("path_grid_xy", ""))
        world_points = parse_path(row.get("path_world_xy", ""))
        for grid_a, grid_b, world_a, world_b in zip(
            grid_points, grid_points[1:], world_points, world_points[1:]
        ):
            grid_dx = grid_b[0] - grid_a[0]
            grid_dy = grid_b[1] - grid_a[1]
            if grid_dx:
                dx_values.append(abs((world_b[0] - world_a[0]) / grid_dx))
            if grid_dy:
                dy_values.append(abs((world_b[1] - world_a[1]) / grid_dy))
    if not dx_values or not dy_values:
        raise ValueError("无法从候选路径推导网格间距。")
    return median(dx_values), median(dy_values)


def review_priority(row: dict[str, str]) -> tuple[str, str]:
    support = float(row["support_ratio"])
    unsupported = int(row["max_unsupported_run"])
    turn = float(row["max_turn_angle"])
    margins = [
        float(row["competition_margin_a"]),
        float(row["competition_margin_b"]),
    ]
    reasons: list[str] = []
    if support < 0.65:
        reasons.append("弱属性支撑偏低")
    if unsupported >= 4:
        reasons.append("连续低支撑段较长")
    if turn > 50.0:
        reasons.append("路径转角偏大")
    if min(margins) < 0.18:
        reasons.append("存在较近竞争候选")
    return ("高", "；".join(reasons)) if reasons else ("常规", "")


def format_number(value: float) -> str:
    return f"{value:.3f}".rstrip("0").rstrip(".")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--candidates", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--padding-cells", type=float, default=18.0)
    parser.add_argument(
        "--baseline-layer",
        default="FaultPolyTrace_Baseline_Review_20260731",
    )
    parser.add_argument(
        "--candidate-layer",
        default="FaultPolyTrace_WeakContinuity_Review_20260731",
    )
    args = parser.parse_args()

    candidate_path = Path(args.candidates)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    with candidate_path.open("r", encoding="utf-8-sig", newline="") as stream:
        all_rows = list(csv.DictReader(stream))
    accepted = [row for row in all_rows if row.get("accepted") == "1"]
    dx, dy = infer_spacing(accepted)

    output_rows: list[dict[str, str]] = []
    for row in accepted:
        world_points = parse_path(row["path_world_xy"])
        if not world_points:
            world_points = [
                (float(row["endpoint_a_world_x"]), float(row["endpoint_a_world_y"])),
                (float(row["endpoint_b_world_x"]), float(row["endpoint_b_world_y"])),
            ]
        xs = [point[0] for point in world_points]
        ys = [point[1] for point in world_points]
        priority, priority_reason = review_priority(row)
        output_rows.append(
            {
                "candidate_id": row["candidate_id"],
                "review_priority": priority,
                "priority_reason": priority_reason,
                "center_x": format_number(float(row["center_world_x"])),
                "center_y": format_number(float(row["center_world_y"])),
                "endpoint_a_x": format_number(float(row["endpoint_a_world_x"])),
                "endpoint_a_y": format_number(float(row["endpoint_a_world_y"])),
                "endpoint_b_x": format_number(float(row["endpoint_b_world_x"])),
                "endpoint_b_y": format_number(float(row["endpoint_b_world_y"])),
                "view_xmin": format_number(min(xs) - args.padding_cells * dx),
                "view_xmax": format_number(max(xs) + args.padding_cells * dx),
                "view_ymin": format_number(min(ys) - args.padding_cells * dy),
                "view_ymax": format_number(max(ys) + args.padding_cells * dy),
                "support_ratio": row["support_ratio"],
                "mean_support": row["mean_support"],
                "max_unsupported_run": row["max_unsupported_run"],
                "competition_margin": format_number(
                    min(
                        float(row["competition_margin_a"]),
                        float(row["competition_margin_b"]),
                    )
                ),
                "manual_result": "待复核",
                "review_note": "",
                "screenshot_file": f"GeoEast_弱连续性候选_{int(row['candidate_id']):03d}.png",
            }
        )

    fieldnames = list(output_rows[0]) if output_rows else []
    csv_path = output_dir / "GeoEast工区底图复核坐标.csv"
    with csv_path.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(output_rows)

    high_count = sum(row["review_priority"] == "高" for row in output_rows)
    lines = [
        "# GeoEast 工区底图逐点复核说明",
        "",
        "本索引直接来自 GeoEast 当前 BOMapGrid 的实际运行结果。候选坐标按网格采样点中心计算，可与底图状态栏的 X/Y 坐标对应。",
        "",
        "## 图层设置",
        "",
        f"- 输入属性网格：保持显示。",
        f"- 正式基线层：`{args.baseline_layer}`，建议黄色细线。",
        f"- 弱连续性候选层：`{args.candidate_layer}`，建议青色细线。",
        "- 先单独切换两层确认差异，再同时叠加检查连接是否跨越其他断层。",
        "",
        "## 复核方法",
        "",
        "1. 先检查高优先级项，再检查其余项；优先级只决定复核顺序，不代表自动拒绝。",
        "2. 按中心坐标移动到候选位置，放大到单个属性点和多边形边界均清晰可见。",
        "3. 检查路径两端的切线方向、路径上的弱属性支撑、连续低支撑长度、竞争分支和连接后的局部宽度。",
        "4. 若候选跨过其他骨架、连接到错误分支、依赖长段无属性区或形成异常宽带，应判为拒绝。",
        "5. 若路径沿同一断层走向、存在连续或短间断弱属性支撑、未造成误合并且边界自然，可判为通过。",
        "6. 每项保存包含图层树、属性点、多边形边界和左下角 X/Y 坐标的截图。",
        "",
        "## 本轮数量",
        "",
        f"- 搜索候选：{len(all_rows)} 条。",
        f"- 算法门控接受：{len(output_rows)} 条。",
        f"- 高优先级人工复核：{high_count} 条。",
        f"- 网格间距：X={dx:g}，Y={dy:g}。",
        "",
        "最终是否晋级必须依据全部候选的工区底图复核结论，不能仅凭覆盖率提高作出判断。",
    ]
    (output_dir / "GeoEast工区底图复核说明.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8"
    )
    print(f"accepted={len(output_rows)} high_priority={high_count} dx={dx:g} dy={dy:g}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
