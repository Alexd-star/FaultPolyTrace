import argparse
import json
import math
from pathlib import Path


def clamp(value, low, high):
    return max(low, min(high, value))


def load_json(path):
    with open(path, "r", encoding="utf-8-sig", errors="replace") as f:
        return json.load(f)


def find_diagnostics(csv_path):
    candidate = Path(str(csv_path) + ".diagnostics.json")
    return candidate if candidate.exists() else None


def load_optional_json(path):
    if not path:
        return {}
    candidate = Path(path)
    if not candidate.exists():
        return {}
    return load_json(candidate)


def score_local_candidate_risk(local_summary):
    if not local_summary:
        return 0.0, []
    actionable = int(local_summary.get("actionable_split_candidate_count", 0))
    manual = int(local_summary.get("manual_review_candidate_count", 0))
    score = 0.0
    score -= min(2.5, actionable * 0.65)
    score -= min(0.8, manual * 0.20)
    notes = []
    if actionable:
        ids = local_summary.get("actionable_split_candidate_ids", [])
        suffix = f"：{ids}" if ids else ""
        notes.append(f"剩余可行动低值桥候选 {actionable} 个{suffix}")
    if manual:
        notes.append(f"剩余人工复核低值桥候选 {manual} 个")
    return round(clamp(score, -3.0, 0.0), 2), notes


def score_split_benefit(split_summary):
    if not split_summary:
        return 0.0, []
    multi_part = int(split_summary.get("multi_part_base_polygon_count", 0))
    safe_split = int(split_summary.get("safe_split_base_polygon_count", 0))
    bridge_split = int(split_summary.get("background_bridge_split_count", 0))
    lossless_split = int(split_summary.get("lossless_split_count", 0))
    unmatched = int(split_summary.get("unmatched_candidate_count", 0))
    strong_loss_objects = int(split_summary.get("strong_value_loss_base_polygon_count", 0))
    weak_loss_objects = int(split_summary.get("weak_value_loss_base_polygon_count", 0))
    lost_strong_cells = int(split_summary.get("lost_strong_cells_total", 0))
    lost_weak_cells = int(split_summary.get("lost_weak_cells_total", 0))
    min_cover = float(split_summary.get("min_changed_covered_ratio", 1.0))

    score = 0.0
    score += min(2.0, bridge_split * 0.55)
    score += min(1.0, lossless_split * 0.35)
    score += min(1.0, max(0, safe_split - bridge_split - lossless_split) * 0.25)
    score -= min(4.0, strong_loss_objects * 1.5 + lost_strong_cells * 0.08)
    score -= min(2.0, weak_loss_objects * 0.45 + lost_weak_cells * 0.01)
    score -= min(2.0, unmatched * 0.5)
    if min_cover < 0.88:
        score -= min(2.0, (0.88 - min_cover) * 10.0)
    if multi_part > 0 and safe_split == 0:
        score -= 1.0

    notes = []
    if bridge_split:
        notes.append(f"安全切除低值背景桥 {bridge_split} 处")
    if lossless_split:
        notes.append(f"无覆盖损失拆分 {lossless_split} 处")
    if lost_strong_cells:
        notes.append(f"损失强响应像元 {lost_strong_cells} 个")
    if lost_weak_cells:
        notes.append(f"损失弱响应像元 {lost_weak_cells} 个")
    if unmatched:
        notes.append(f"无基线来源候选 {unmatched} 个")
    return round(clamp(score, -5.0, 4.0), 2), notes


def score_quality(metrics, diagnostics, split_summary=None, local_summary=None):
    strong_coverage = float(metrics.get("strong_coverage", 0.0))
    weak_coverage = float(metrics.get("weak_coverage", 0.0))
    nonzero_coverage = float(metrics.get("nonzero_coverage", 0.0))
    strong_review = int(metrics.get("strong_review_components", 0))
    weak_review = int(metrics.get("weak_line_review_components", 0))
    high_risk = int(metrics.get("high_risk_merge_polygons", 0))
    middle_risk = int(metrics.get("middle_risk_merge_polygons", 0))
    small_count = int(metrics.get("small_polygon_count", 0))
    background_ratio = float(metrics.get("boundary_background_ratio", 0.0))
    large_background_count = int(metrics.get("large_background_polygon_count", 0))
    polygon_count = max(1, int(metrics.get("polygon_count", 0)))

    topology = diagnostics.get("topology", {}) if diagnostics else {}
    timing = diagnostics.get("timing", {}) if diagnostics else {}
    open_rings = int(topology.get("openRings", 0))
    self_intersections = int(topology.get("selfIntersectingPolygons", 0))
    polygon_intersections = int(topology.get("polygonIntersections", 0))
    trace_ms = float(timing.get("traceMs", 0.0))
    elapsed_ms = float(timing.get("elapsedMs", 0.0))

    gates = []
    if open_rings != 0 or self_intersections != 0 or polygon_intersections != 0:
        gates.append("拓扑未通过")
    if trace_ms > 60000.0:
        gates.append("核心耗时超过 1 分钟")
    if strong_coverage < 0.99:
        gates.append("强响应覆盖率低于 99%")
    if weak_coverage < 0.955:
        gates.append("弱响应覆盖率低于 95.5%")

    coverage_score = 0.0
    coverage_score += 20.0 * clamp((strong_coverage - 0.985) / 0.011, 0.0, 1.0)
    coverage_score += 10.0 * clamp((weak_coverage - 0.945) / 0.024, 0.0, 1.0)
    coverage_score += 5.0 * clamp((nonzero_coverage - 0.90) / 0.045, 0.0, 1.0)
    coverage_score -= min(3.0, strong_review * 0.75)
    coverage_score -= min(2.5, weak_review * 0.15)
    coverage_score = clamp(coverage_score, 0.0, 35.0)

    split_benefit_score, split_notes = score_split_benefit(split_summary)
    local_candidate_score, local_candidate_notes = score_local_candidate_risk(local_summary)

    anti_merge_score = 25.0
    anti_merge_score -= min(8.0, high_risk * 0.38)
    anti_merge_score -= min(4.0, middle_risk * 0.07)
    anti_merge_score += split_benefit_score
    anti_merge_score += local_candidate_score
    anti_merge_score = clamp(anti_merge_score, 0.0, 25.0)

    granularity_score = 15.0
    small_ratio = small_count / polygon_count
    granularity_score -= min(7.0, small_ratio * 55.0)
    if polygon_count > 450:
        granularity_score -= min(6.0, (polygon_count - 450) / 80.0)
    if polygon_count < 80:
        granularity_score -= min(4.0, (80 - polygon_count) / 20.0)
    if background_ratio > 0.58:
        granularity_score -= min(2.0, (background_ratio - 0.58) * 10.0)
    granularity_score -= min(1.5, large_background_count * 0.08)
    granularity_score = clamp(granularity_score, 0.0, 15.0)

    topology_score = 15.0
    topology_score -= open_rings * 5.0
    topology_score -= self_intersections * 6.0
    topology_score -= polygon_intersections * 4.0
    topology_score = clamp(topology_score, 0.0, 15.0)

    performance_score = 10.0
    if trace_ms > 0.0:
        performance_score -= max(0.0, (trace_ms - 1000.0) / 2500.0)
    if elapsed_ms > 0.0:
        performance_score -= max(0.0, (elapsed_ms - 5000.0) / 5000.0)
    performance_score = clamp(performance_score, 0.0, 10.0)

    total = coverage_score + anti_merge_score + granularity_score + topology_score + performance_score
    if gates:
        total = min(total, 69.0)

    if total >= 92.0 and not gates:
        level = "冲顶优秀"
    elif total >= 82.0 and not gates:
        level = "正式交付"
    elif total >= 72.0:
        level = "可复核使用"
    else:
        level = "研发实验"

    return {
        "total_score": round(total, 2),
        "level": level,
        "gates_passed": not gates,
        "failed_gates": gates,
        "subscores": {
            "coverage": round(coverage_score, 2),
            "anti_merge": round(anti_merge_score, 2),
            "split_benefit": round(split_benefit_score, 2),
            "local_candidate_risk": round(local_candidate_score, 2),
            "granularity": round(granularity_score, 2),
            "topology": round(topology_score, 2),
            "performance": round(performance_score, 2),
        },
        "split_benefit_notes": split_notes,
        "local_candidate_notes": local_candidate_notes,
        "metrics": {
            "polygon_count": polygon_count,
            "strong_coverage": strong_coverage,
            "weak_coverage": weak_coverage,
            "nonzero_coverage": nonzero_coverage,
            "strong_review_components": strong_review,
            "weak_line_review_components": weak_review,
            "high_risk_merge_polygons": high_risk,
            "middle_risk_merge_polygons": middle_risk,
            "small_polygon_count": small_count,
            "boundary_background_ratio": background_ratio,
            "large_background_polygon_count": large_background_count,
            "open_rings": open_rings,
            "self_intersections": self_intersections,
            "polygon_intersections": polygon_intersections,
            "trace_ms": trace_ms,
            "elapsed_ms": elapsed_ms,
        },
    }


def write_report(path, score, metrics_path, diagnostics_path, split_summary_path=None, local_summary_path=None):
    m = score["metrics"]
    s = score["subscores"]
    lines = [
        "# 综合质量评分报告",
        "",
        "## 1. 结论",
        "",
        f"- 综合评分：{score['total_score']:.2f} / 100",
        f"- 质量等级：{score['level']}",
        f"- 门禁状态：{'通过' if score['gates_passed'] else '未通过'}",
    ]
    if score["failed_gates"]:
        lines.append(f"- 未通过门禁：{'；'.join(score['failed_gates'])}")
    lines.extend(
        [
            "",
            "## 2. 分项评分",
            "",
            "| 分项 | 分值 | 说明 |",
            "|---|---:|---|",
            f"| 覆盖与漏检 | {s['coverage']:.2f} / 35 | 强/弱/非零响应覆盖率，并扣除未覆盖复核连通域 |",
            f"| 防黏连 | {s['anti_merge']:.2f} / 25 | 高/中风险疑似误合并越少越好；安全拆掉低值背景桥可获得收益修正 |",
            f"| 安全拆分收益修正 | {s.get('split_benefit', 0.0):+.2f} | 只切背景桥且不损失强响应时加分；切掉强/弱响应或产生无来源候选时扣分 |",
            f"| 可行动低值桥风险修正 | {s.get('local_candidate_risk', 0.0):+.2f} | 仍存在可明确切开的低值桥候选时扣分，避免把粗略高风险计数当作唯一依据 |",
            f"| 粒度与碎片 | {s['granularity']:.2f} / 15 | 不固定多边形数量，但惩罚小碎片比例和极端数量 |",
            f"| 拓扑合法 | {s['topology']:.2f} / 15 | 开放环、自相交、异常相交必须趋近 0 |",
            f"| 性能 | {s['performance']:.2f} / 10 | 赛题要求核心 1 分钟内，工程目标远低于 5 秒 |",
            "",
            "## 3. 核心指标",
            "",
            "| 指标 | 数值 |",
            "|---|---:|",
            f"| 保留多边形数量 | {m['polygon_count']} |",
            f"| 强响应覆盖率 | {m['strong_coverage']:.4%} |",
            f"| 弱响应覆盖率 | {m['weak_coverage']:.4%} |",
            f"| 非零响应覆盖率 | {m['nonzero_coverage']:.4%} |",
            f"| 强响应复核连通域 | {m['strong_review_components']} |",
            f"| 弱线性复核连通域 | {m['weak_line_review_components']} |",
            f"| 高风险疑似误合并 | {m['high_risk_merge_polygons']} |",
            f"| 中风险疑似误合并 | {m['middle_risk_merge_polygons']} |",
            f"| 小碎片多边形 | {m['small_polygon_count']} |",
            f"| 远背景包络比例 | {m.get('boundary_background_ratio', 0.0):.4%} |",
            f"| 高远背景包络多边形 | {m.get('large_background_polygon_count', 0)} |",
            f"| 开放环 / 自相交 / 异常相交 | {m['open_rings']} / {m['self_intersections']} / {m['polygon_intersections']} |",
            f"| 核心追踪耗时 | {m['trace_ms']:.0f} ms |",
            f"| 总耗时 | {m['elapsed_ms']:.0f} ms |",
            "",
            "## 4. 评分原则",
            "",
            "本评分不把某个固定多边形数量作为唯一目标。生产解释更关心：该覆盖的断层响应是否覆盖，近邻但不同脉络是否被错误合并，结果是否闭合且拓扑合法，边界是否自然，输出粒度是否便于解释人员使用。",
            "",
            "## 5. 输入文件",
            "",
            f"- 质量指标：`{metrics_path}`",
        ]
    )
    if diagnostics_path:
        lines.append(f"- 运行诊断：`{diagnostics_path}`")
    if split_summary_path:
        lines.append(f"- 防黏连差异摘要：`{split_summary_path}`")
    if local_summary_path:
        lines.append(f"- 局部防黏连候选摘要：`{local_summary_path}`")
    if score.get("split_benefit_notes"):
        lines.extend(["", "## 6. 防黏连差异要点", ""])
        lines.extend(f"- {note}" for note in score["split_benefit_notes"])
    if score.get("local_candidate_notes"):
        lines.extend(["", "## 7. 可行动低值桥候选", ""])
        lines.extend(f"- {note}" for note in score["local_candidate_notes"])
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="按生产解释标准计算断层多边形综合质量评分")
    parser.add_argument("--metrics", required=True, help="顶尖质量诊断生成的 总体指标.json")
    parser.add_argument("--diagnostics", default="", help="运行诊断 JSON；默认尝试用 CSV.diagnostics.json")
    parser.add_argument("--csv", default="", help="结果 CSV，用于自动定位 diagnostics")
    parser.add_argument("--split-summary", default="", help="防黏连差异摘要 JSON；用于评价安全拆分收益")
    parser.add_argument("--local-summary", default="", help="局部防黏连候选摘要 JSON；用于评价剩余可行动低值桥候选")
    parser.add_argument("--out-json", required=True)
    parser.add_argument("--out-report", required=True)
    args = parser.parse_args()

    metrics_path = Path(args.metrics)
    metrics = load_json(metrics_path)
    diagnostics_path = Path(args.diagnostics) if args.diagnostics else None
    if diagnostics_path and not diagnostics_path.exists():
        diagnostics_path = None
    if diagnostics_path is None and args.csv:
        diagnostics_path = find_diagnostics(args.csv)
    diagnostics = load_json(diagnostics_path) if diagnostics_path else {}
    split_summary = load_optional_json(args.split_summary)
    local_summary = load_optional_json(args.local_summary)

    score = score_quality(metrics, diagnostics, split_summary, local_summary)
    Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
    with open(args.out_json, "w", encoding="utf-8") as f:
        json.dump(score, f, ensure_ascii=False, indent=2)
    write_report(
        args.out_report,
        score,
        metrics_path,
        diagnostics_path,
        args.split_summary if split_summary else None,
        args.local_summary if local_summary else None,
    )
    print(args.out_json)
    print(args.out_report)


if __name__ == "__main__":
    main()
