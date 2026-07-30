# -*- coding: utf-8 -*-
"""Generate and evaluate official-complexity planar benchmark cases."""

import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from generalization_validation.complex_synthetic import generate_complex_case
from generalization_validation.runner import FORMAL_TRACE_ARGUMENTS, run_benchmark
from generalization_validation.synthetic import save_case


def _write_manifest(output, cases, width, height, seed):
    payload = {
        "schema_version": 1,
        "benchmark_type": "controlled_planar_fault_attribute_official_complexity",
        "evidence_level": "controlled_same_domain_complex_truth",
        "truth_independence": "geometry_is_generated_before_attribute_rendering",
        "real_data_claim": False,
        "direct_planar_evidence": True,
        "real_cross_survey_generalization_proven": False,
        "count": len(cases),
        "width": int(width),
        "height": int(height),
        "base_seed": int(seed),
        "cases": cases,
    }
    (output / "benchmark_manifest.json").write_text(
        json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return payload


def generate(output, count, width, height, seed):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    cases = []
    for index in range(int(count)):
        case_seed = int(seed) + index * 7919
        case = generate_complex_case(width, height, case_seed)
        case_id = f"case_{index + 1:02d}_mixed_network"
        save_case(case, output / case_id)
        cases.append({"id": case_id, "path": case_id, **case.metadata})
    manifest = _write_manifest(output, cases, width, height, seed)
    return manifest


def _sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _summary(payload):
    return payload["summary"]


def _compare(baseline, candidate, baseline_dir, candidate_dir):
    base = _summary(baseline)
    cand = _summary(candidate)
    metric_delta = {
        key: float(cand[key] - base[key])
        for key in (
            "pixel_iou_mean",
            "pixel_precision_mean",
            "pixel_recall_mean",
            "boundary_f1_mean",
            "centerline_corridor_f1_mean",
        )
    }
    output_hash_equal = True
    for case in baseline["cases"]:
        case_id = case["case_id"]
        base_csv = Path(baseline_dir) / case_id / "result.csv"
        candidate_csv = Path(candidate_dir) / case_id / "result.csv"
        output_hash_equal &= _sha256(base_csv) == _sha256(candidate_csv)
    checks = {
        "IoU不下降": metric_delta["pixel_iou_mean"] >= -0.002,
        "Precision不下降": metric_delta["pixel_precision_mean"] >= -0.002,
        "Recall不下降": metric_delta["pixel_recall_mean"] >= -0.002,
        "Boundary F1不下降": metric_delta["boundary_f1_mean"] >= -0.002,
        "中心线走廊F1不下降": metric_delta["centerline_corridor_f1_mean"] >= -0.002,
        "误合并不增加": cand["merge_count_total"] <= base["merge_count_total"],
        "误合并额外吞并对象不增加": cand["merge_excess_total"] <= base["merge_excess_total"],
        "过分割不增加": cand["split_count_total"] <= base["split_count_total"],
        "过分割额外结果不增加": cand["split_excess_total"] <= base["split_excess_total"],
        "漏对象不增加": cand["missed_object_count_total"] <= base["missed_object_count_total"],
        "非法几何为零": cand["invalid_polygon_count_total"] == 0,
        "异常相交为零": cand["polygon_intersection_count_total"] == 0,
        "处理耗时不过度增加": cand["elapsed_ms_max"] <= max(base["elapsed_ms_max"] * 2, base["elapsed_ms_max"] + 1000),
    }
    strict_improvement = (
        metric_delta["pixel_iou_mean"] > 0.002
        or metric_delta["pixel_recall_mean"] > 0.002
        or metric_delta["boundary_f1_mean"] > 0.002
        or cand["merge_count_total"] < base["merge_count_total"]
        or cand["merge_excess_total"] < base["merge_excess_total"]
        or cand["split_count_total"] < base["split_count_total"]
        or cand["split_excess_total"] < base["split_excess_total"]
        or cand["missed_object_count_total"] < base["missed_object_count_total"]
    )
    return {
        "pass": bool(all(checks.values()) and strict_improvement),
        "checks": checks,
        "strict_improvement": bool(strict_improvement),
        "output_hash_equal_to_baseline": bool(output_hash_equal),
        "baseline": base,
        "candidate": cand,
        "delta": metric_delta,
    }


def run(executable, dataset, output, modes, reuse_existing=False):
    executable = Path(executable)
    dataset = Path(dataset)
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    baseline_dir = output / "baseline"
    baseline = run_benchmark(
        dataset,
        executable,
        baseline_dir,
        trace_arguments=FORMAL_TRACE_ARGUMENTS,
        reuse_existing=reuse_existing,
    )
    mode_arguments = {
        "branch": ("--branch-attribution-candidate-groups",),
        "weak_path": ("--weak-path-gate",),
        "quality_shadow": ("--local-polygon-quality-shadow",),
    }
    comparisons = {}
    for mode in modes:
        if mode == "baseline":
            continue
        if mode not in mode_arguments:
            raise ValueError(f"Unknown mode: {mode}")
        candidate_dir = output / mode
        arguments = tuple(FORMAL_TRACE_ARGUMENTS) + mode_arguments[mode]
        candidate = run_benchmark(
            dataset,
            executable,
            candidate_dir,
            trace_arguments=arguments,
            reuse_existing=reuse_existing,
        )
        comparisons[mode] = _compare(baseline, candidate, baseline_dir, candidate_dir)
        comparisons[mode]["candidate_arguments"] = list(arguments)

    report = {
        "schema_version": 1,
        "benchmark": json.loads((dataset / "benchmark_manifest.json").read_text(encoding="utf-8")),
        "baseline_summary": baseline["summary"],
        "comparisons": comparisons,
        "promotion_policy": {
            "description": "候选必须保持主要质量指标、拓扑、对象粒度和耗时，并至少在一项指标上有明确改善。",
            "mean_metric_tolerance": 0.002,
            "production_default_unchanged": True,
        },
        "metric_interpretation": {
            "pixel_boundary_centerline_topology": "可按定义直接解释。",
            "object_overlap": "长路径真值与交点间结果片段的粒度不同，对象重叠数量仅作为同一基准内的A/B压力代理，不等同于经专家确认的地质误合并或过分割数量。",
        },
    }
    (output / "复杂同域验证摘要.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    lines = [
        "# 官方复杂度级同域验证报告",
        "",
        "本报告使用与赛题输入输出语义一致的平面断层属性网格。案例先生成复杂断层网络真值，再渲染非均匀响应和相关背景，算法参数保持正式基线不变。该数据是可控验证资料，不冒充其他真实工区的专家解释。",
        "",
        "## 基线结果",
        "",
        f"- 案例数：`{baseline['summary']['case_count']}`",
        f"- IoU：`{baseline['summary']['pixel_iou_mean']:.6f}`",
        f"- Recall：`{baseline['summary']['pixel_recall_mean']:.6f}`",
        f"- Boundary F1：`{baseline['summary']['boundary_f1_mean']:.6f}`",
        f"- 对象重叠代理（合并/拆分/未关联）：`{baseline['summary']['merge_count_total']}/{baseline['summary']['split_count_total']}/{baseline['summary']['missed_object_count_total']}`",
        f"- 对象重叠代理严重度（额外关联真值/额外关联结果）：`{baseline['summary']['merge_excess_total']}/{baseline['summary']['split_excess_total']}`",
        f"- 非法几何/异常相交：`{baseline['summary']['invalid_polygon_count_total']}/{baseline['summary']['polygon_intersection_count_total']}`",
        "",
        "## 候选A/B门控",
        "",
        "| 模式 | 是否通过 | 误合并变化 | IoU变化 | Recall变化 | 输出是否与基线完全相同 |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for mode, comparison in comparisons.items():
        base = comparison["baseline"]
        cand = comparison["candidate"]
        lines.append(
            f"| {mode} | {'是' if comparison['pass'] else '否'} | {cand['merge_count_total'] - base['merge_count_total']} | {comparison['delta']['pixel_iou_mean']:.6f} | {comparison['delta']['pixel_recall_mean']:.6f} | {'是' if comparison['output_hash_equal_to_baseline'] else '否'} |"
        )
    lines.extend(
        [
            "",
            "## 判定边界",
            "",
            "复杂度级可控基准用于暴露算法在密集网络、近邻平行、独立交叉、分支和弱缺口条件下的行为。长路径真值与交点间结果多边形的对象粒度不同，因此对象重叠数量只用于同一基准、同一规则下的候选A/B比较，不应直接表述为专家确认的地质错误数量。该基准不能替代独立真实工区的专家解释。任何候选模式即使局部指标改善，只要对象重叠代理、拓扑、覆盖或稳定性发生退化，也不替换正式214基线。",
        ]
    )
    (output / "复杂同域验证报告.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description="FaultPolyTrace官方复杂度级同域验证")
    sub = parser.add_subparsers(dest="command", required=True)
    gen = sub.add_parser("generate")
    gen.add_argument("--output", type=Path, required=True)
    gen.add_argument("--count", type=int, default=4)
    gen.add_argument("--width", type=int, default=1501)
    gen.add_argument("--height", type=int, default=701)
    gen.add_argument("--seed", type=int, default=20260730)
    run_parser = sub.add_parser("run")
    run_parser.add_argument("--dataset", type=Path, required=True)
    run_parser.add_argument("--exe", type=Path, default=ROOT / "build" / "faultpolytrace.exe")
    run_parser.add_argument("--output", type=Path, required=True)
    run_parser.add_argument(
        "--modes",
        default="branch,weak_path,quality_shadow",
        help="逗号分隔的候选模式：branch,weak_path,quality_shadow",
    )
    run_parser.add_argument("--reuse-existing", action="store_true", help="复用已存在的追踪输出，只重新计算评价指标")
    args = parser.parse_args()
    if args.command == "generate":
        result = generate(args.output, args.count, args.width, args.height, args.seed)
    else:
        result = run(
            args.exe,
            args.dataset,
            args.output,
            [item for item in args.modes.split(",") if item],
            reuse_existing=args.reuse_existing,
        )
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    raise SystemExit(main())
