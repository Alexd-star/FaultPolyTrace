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


def score_quality(metrics, diagnostics):
    strong_coverage = float(metrics.get("strong_coverage", 0.0))
    weak_coverage = float(metrics.get("weak_coverage", 0.0))
    nonzero_coverage = float(metrics.get("nonzero_coverage", 0.0))
    strong_review = int(metrics.get("strong_review_components", 0))
    weak_review = int(metrics.get("weak_line_review_components", 0))
    high_risk = int(metrics.get("high_risk_merge_polygons", 0))
    middle_risk = int(metrics.get("middle_risk_merge_polygons", 0))
    small_count = int(metrics.get("small_polygon_count", 0))
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

    anti_merge_score = 25.0
    anti_merge_score -= min(8.0, high_risk * 0.38)
    anti_merge_score -= min(4.0, middle_risk * 0.07)
    anti_merge_score = clamp(anti_merge_score, 0.0, 25.0)

    granularity_score = 15.0
    small_ratio = small_count / polygon_count
    granularity_score -= min(7.0, small_ratio * 55.0)
    if polygon_count > 450:
        granularity_score -= min(6.0, (polygon_count - 450) / 80.0)
    if polygon_count < 80:
        granularity_score -= min(4.0, (80 - polygon_count) / 20.0)
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
            "granularity": round(granularity_score, 2),
            "topology": round(topology_score, 2),
            "performance": round(performance_score, 2),
        },
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
            "open_rings": open_rings,
            "self_intersections": self_intersections,
            "polygon_intersections": polygon_intersections,
            "trace_ms": trace_ms,
            "elapsed_ms": elapsed_ms,
        },
    }


def write_report(path, score, metrics_path, diagnostics_path):
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
            f"| 防黏连 | {s['anti_merge']:.2f} / 25 | 高/中风险疑似误合并越少越好 |",
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
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="按生产解释标准计算断层多边形综合质量评分")
    parser.add_argument("--metrics", required=True, help="顶尖质量诊断生成的 总体指标.json")
    parser.add_argument("--diagnostics", default="", help="运行诊断 JSON；默认尝试用 CSV.diagnostics.json")
    parser.add_argument("--csv", default="", help="结果 CSV，用于自动定位 diagnostics")
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

    score = score_quality(metrics, diagnostics)
    Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
    with open(args.out_json, "w", encoding="utf-8") as f:
        json.dump(score, f, ensure_ascii=False, indent=2)
    write_report(args.out_report, score, metrics_path, diagnostics_path)
    print(args.out_json)
    print(args.out_report)


if __name__ == "__main__":
    main()
