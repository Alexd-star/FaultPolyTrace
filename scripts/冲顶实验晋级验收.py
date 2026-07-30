# -*- coding: utf-8 -*-
"""Promotion gate for contest peak-quality experiment results.

This gate compares a candidate CSV against a baseline CSV under the same
official planar fault-attribute grid. It does not edit geometry; it only
decides whether the candidate has enough evidence to be considered for the
formal line.
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np

from quality_evidence_metrics import evaluate_proxy_quality
from wkt_geometry import rasterize_polygon
from 局部修形拓扑核验 import check_csv_topology
from 局部形态质量诊断 import (
    build_summary,
    configure_chinese_font,
    diagnose,
    read_geoeast_dat,
    read_polygons,
    write_outputs as write_shape_outputs,
)


def _ratio(numerator, denominator):
    return float(numerator) / float(denominator) if denominator else 1.0


def _polygon_mask(polygons, width, height, xs, ys, dx, dy):
    mask = np.zeros((height, width), dtype=bool)
    for polygon in polygons:
        mask |= rasterize_polygon(polygon["coords"], width, height, xs, ys, dx, dy, polygon.get("holes", []))
    return mask


def evaluate_result(dat_path, csv_path, shape_out_dir=None):
    values, xs, ys, dx, dy, extent_edges = read_geoeast_dat(dat_path)
    polygons = read_polygons(csv_path)
    width = len(xs)
    height = len(ys)
    mask = _polygon_mask(polygons, width, height, xs, ys, dx, dy)
    proxy_metrics = evaluate_proxy_quality(values, mask)
    topology_summary, _ = check_csv_topology(csv_path)

    shape_records = diagnose(
        polygons,
        values,
        xs,
        ys,
        dx,
        dy,
        width_ratio=2.35,
        min_extra_cells=24,
        window=11,
    )
    clean_shape_records = [
        {key: value for key, value in record.items() if not key.startswith("_")}
        for record in shape_records
    ]
    shape_summary = build_summary(clean_shape_records)
    if shape_out_dir is not None:
        write_shape_outputs(shape_records, values, extent_edges, xs, ys, dx, dy, Path(shape_out_dir), max_figures=40)

    topology_parse_or_empty = int(topology_summary["parse_error_count"]) + int(topology_summary["empty_or_non_polygon_count"])
    result = {
        "csv": str(csv_path),
        "kept_polygons": int(len(polygons)),
        "strong_coverage": proxy_metrics["strong_response_coverage_proxy"],
        "weak_coverage": proxy_metrics["weak_response_coverage_proxy"],
        "open_rings": topology_parse_or_empty,
        "self_intersections": int(topology_summary["invalid_geometry_count"]),
        "polygon_intersections": int(topology_summary["polygon_intersection_count"]),
        "topology_parse_or_empty": topology_parse_or_empty,
        "topology_invalid_geometry": int(topology_summary["invalid_geometry_count"]),
        "topology_area_intersections": int(topology_summary["polygon_intersection_count"]),
        "local_shape_candidates": int(shape_summary["candidate_count"]),
        "high_priority_local_shape_candidates": int(shape_summary["high_priority_count"]),
        "shape_class_counts": shape_summary.get("shape_class_counts", {}),
        "priority_counts": shape_summary.get("priority_counts", {}),
    }
    result.update(proxy_metrics)
    return result


def _check(name, passed, detail):
    return {"name": name, "pass": bool(passed), "detail": detail}


def _optional_lower_is_better_check(baseline, candidate, key, name, tolerance):
    if key not in baseline or key not in candidate:
        return _check(name, True, "旧结果未提供该代理指标，本次不作为阻断项")
    delta = float(candidate[key]) - float(baseline[key])
    return _check(name, delta <= tolerance, f"变化 {delta:.6%}")


def _optional_higher_is_better_check(baseline, candidate, key, name, tolerance):
    if key not in baseline or key not in candidate:
        return _check(name, True, "未同时提供参考真值指标，本次不作为阻断项")
    delta = float(candidate[key]) - float(baseline[key])
    return _check(name, delta >= -tolerance, f"变化 {delta:.6%}")


def _optional_count_not_increase_check(baseline, candidate, key, name):
    if key not in baseline or key not in candidate:
        return _check(name, True, "未同时提供参考对象指标，本次不作为阻断项")
    delta = int(candidate[key]) - int(baseline[key])
    return _check(name, delta <= 0, f"变化 {delta}")


def decide_promotion(
    baseline,
    candidate,
    max_polygon_delta=25,
    coverage_tolerance=1e-9,
    proxy_tolerance=1e-9,
):
    strong_delta = float(candidate["strong_coverage"]) - float(baseline["strong_coverage"])
    weak_delta = float(candidate["weak_coverage"]) - float(baseline["weak_coverage"])
    polygon_delta = int(candidate["kept_polygons"]) - int(baseline["kept_polygons"])
    local_shape_delta = int(candidate["local_shape_candidates"]) - int(baseline["local_shape_candidates"])
    high_shape_delta = int(candidate["high_priority_local_shape_candidates"]) - int(baseline["high_priority_local_shape_candidates"])
    background_delta = float(candidate.get("background_leakage_ratio_proxy", 0.0)) - float(
        baseline.get("background_leakage_ratio_proxy", 0.0)
    )
    unsupported_delta = float(candidate.get("unsupported_inside_ratio_proxy", 0.0)) - float(
        baseline.get("unsupported_inside_ratio_proxy", 0.0)
    )
    bridge_delta = float(candidate.get("low_support_bridge_ratio_proxy", 0.0)) - float(
        baseline.get("low_support_bridge_ratio_proxy", 0.0)
    )
    topology_zero = (
        int(candidate["open_rings"]) == 0
        and int(candidate["self_intersections"]) == 0
        and int(candidate["polygon_intersections"]) == 0
    )
    strict_improvements = [
        strong_delta > coverage_tolerance,
        weak_delta > coverage_tolerance,
        local_shape_delta < 0,
        high_shape_delta < 0,
    ]
    for key, delta in (
        ("background_leakage_ratio_proxy", background_delta),
        ("unsupported_inside_ratio_proxy", unsupported_delta),
        ("low_support_bridge_ratio_proxy", bridge_delta),
    ):
        if key in baseline and key in candidate:
            strict_improvements.append(delta < -proxy_tolerance)
    for key in ("reference_iou", "reference_boundary_f1", "reference_cldice"):
        if key in baseline and key in candidate:
            strict_improvements.append(
                float(candidate[key]) - float(baseline[key]) > coverage_tolerance
            )
    for key in ("reference_split_excess_count", "reference_merge_excess_count"):
        if key in baseline and key in candidate:
            strict_improvements.append(int(candidate[key]) - int(baseline[key]) < 0)
    has_strict_improvement = any(strict_improvements)

    checks = [
        _check("强响应覆盖率不退化", strong_delta >= -coverage_tolerance, f"变化 {strong_delta:.6%}"),
        _check("弱响应覆盖率不退化", weak_delta >= -coverage_tolerance, f"变化 {weak_delta:.6%}"),
        _check("拓扑零问题", topology_zero, f"开放环/自交/相交 = {candidate['open_rings']}/{candidate['self_intersections']}/{candidate['polygon_intersections']}"),
        _check("多边形数量稳定", abs(polygon_delta) <= max_polygon_delta, f"变化 {polygon_delta}，阈值 {max_polygon_delta}"),
        _check("局部形态候选不增加", local_shape_delta <= 0, f"变化 {local_shape_delta}"),
        _check("高优先级局部形态候选不增加", high_shape_delta <= 0, f"变化 {high_shape_delta}"),
        _optional_lower_is_better_check(
            baseline,
            candidate,
            "background_leakage_ratio_proxy",
            "背景泄漏代理指标不退化",
            proxy_tolerance,
        ),
        _optional_lower_is_better_check(
            baseline,
            candidate,
            "unsupported_inside_ratio_proxy",
            "多边形内不支撑比例不退化",
            proxy_tolerance,
        ),
        _optional_lower_is_better_check(
            baseline,
            candidate,
            "low_support_bridge_ratio_proxy",
            "低支撑桥代理指标不退化",
            proxy_tolerance,
        ),
        _optional_higher_is_better_check(
            baseline, candidate, "reference_iou", "参考区域 IoU 不退化", coverage_tolerance
        ),
        _optional_higher_is_better_check(
            baseline, candidate, "reference_boundary_f1", "参考边界 F1 不退化", coverage_tolerance
        ),
        _optional_higher_is_better_check(
            baseline, candidate, "reference_cldice", "参考中心线 clDice 不退化", coverage_tolerance
        ),
        _optional_count_not_increase_check(
            baseline, candidate, "reference_split_excess_count", "参考对象过分割不增加"
        ),
        _optional_count_not_increase_check(
            baseline, candidate, "reference_merge_excess_count", "参考对象误合并不增加"
        ),
        _check(
            "至少一项质量指标严格改善",
            has_strict_improvement,
            "覆盖、形态、泄漏、边界、中心线或对象粒度至少一项必须优于基线",
        ),
    ]
    return {
        "pass": all(item["pass"] for item in checks),
        "checks": checks,
        "summary": {
            "strong_coverage_delta": strong_delta,
            "weak_coverage_delta": weak_delta,
            "kept_polygon_delta": polygon_delta,
            "local_shape_candidate_delta": local_shape_delta,
            "high_priority_local_shape_delta": high_shape_delta,
            "background_leakage_delta_proxy": background_delta,
            "unsupported_inside_delta_proxy": unsupported_delta,
            "low_support_bridge_delta_proxy": bridge_delta,
            "reference_iou_delta": float(candidate.get("reference_iou", 0.0)) - float(baseline.get("reference_iou", 0.0)),
            "reference_boundary_f1_delta": float(candidate.get("reference_boundary_f1", 0.0)) - float(baseline.get("reference_boundary_f1", 0.0)),
            "reference_cldice_delta": float(candidate.get("reference_cldice", 0.0)) - float(baseline.get("reference_cldice", 0.0)),
            "reference_split_excess_delta": int(candidate.get("reference_split_excess_count", 0)) - int(baseline.get("reference_split_excess_count", 0)),
            "reference_merge_excess_delta": int(candidate.get("reference_merge_excess_count", 0)) - int(baseline.get("reference_merge_excess_count", 0)),
        },
    }


def write_report(out_dir, baseline, candidate, decision):
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    summary_path = out_dir / "冲顶实验晋级验收摘要.json"
    report_path = out_dir / "冲顶实验晋级验收报告.md"
    payload = {
        "baseline": baseline,
        "candidate": candidate,
        "decision": decision,
    }
    summary_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# 冲顶实验晋级验收报告",
        "",
        "## 1. 结论",
        "",
        f"- 晋级判断：{'通过' if decision['pass'] else '未通过'}",
        "- 本报告只评价平面断层属性网格到闭合断层多边形这条赛题主线。",
        "- 候选结果通过本门禁后，仍需完成 GeoEast 写入显示验证，才能替换正式默认线。",
        "",
        "## 2. 基线与候选指标",
        "",
        "| 指标 | 基线 | 候选 | 变化 |",
        "|---|---:|---:|---:|",
        f"| 保留多边形 | {baseline['kept_polygons']} | {candidate['kept_polygons']} | {decision['summary']['kept_polygon_delta']} |",
        f"| 强响应覆盖率 | {baseline['strong_coverage']:.4%} | {candidate['strong_coverage']:.4%} | {decision['summary']['strong_coverage_delta']:.6%} |",
        f"| 弱响应覆盖率 | {baseline['weak_coverage']:.4%} | {candidate['weak_coverage']:.4%} | {decision['summary']['weak_coverage_delta']:.6%} |",
        f"| 拓扑 | {baseline['open_rings']}/{baseline['self_intersections']}/{baseline['polygon_intersections']} | {candidate['open_rings']}/{candidate['self_intersections']}/{candidate['polygon_intersections']} | - |",
        f"| 局部形态候选 | {baseline['local_shape_candidates']} | {candidate['local_shape_candidates']} | {decision['summary']['local_shape_candidate_delta']} |",
        f"| 高优先级局部形态候选 | {baseline['high_priority_local_shape_candidates']} | {candidate['high_priority_local_shape_candidates']} | {decision['summary']['high_priority_local_shape_delta']} |",
        f"| 背景泄漏代理比例 | {baseline.get('background_leakage_ratio_proxy', 0.0):.4%} | {candidate.get('background_leakage_ratio_proxy', 0.0):.4%} | {decision['summary']['background_leakage_delta_proxy']:.6%} |",
        f"| 多边形内不支撑比例 | {baseline.get('unsupported_inside_ratio_proxy', 0.0):.4%} | {candidate.get('unsupported_inside_ratio_proxy', 0.0):.4%} | {decision['summary']['unsupported_inside_delta_proxy']:.6%} |",
        f"| 低支撑桥代理比例 | {baseline.get('low_support_bridge_ratio_proxy', 0.0):.4%} | {candidate.get('low_support_bridge_ratio_proxy', 0.0):.4%} | {decision['summary']['low_support_bridge_delta_proxy']:.6%} |",
        "",
        "> 强弱覆盖率、背景泄漏和低支撑桥均为输入属性响应代理指标，不使用专家标准答案，不能单独证明地质解释正确性。",
        "",
        "## 3. 门禁明细",
        "",
        "| 门禁 | 结果 | 说明 |",
        "|---|---|---|",
    ]
    for item in decision["checks"]:
        lines.append(f"| {item['name']} | {'通过' if item['pass'] else '未通过'} | {item['detail']} |")
    lines.extend(
        [
            "",
            "## 4. 后续动作",
            "",
            "1. 若本报告通过，可把候选结果作为冲顶实验最佳线保留。",
            "2. 正式替换 GeoEast 默认线前，必须完成官方数据复验、泛化回归、图件逐块复核和 GeoEast 写入显示验证。",
            "3. 若任一门禁失败，不得进入正式默认线，应回到候选分析报告定位原因。",
        ]
    )
    report_path.write_text("\n".join(lines), encoding="utf-8")
    return summary_path, report_path


def main(argv=None):
    configure_chinese_font()
    parser = argparse.ArgumentParser(description="比较冲顶实验候选与正式基线，判断是否满足晋级门禁")
    parser.add_argument("--dat", required=True, help="官方 test.dat 或等价平面断层属性网格")
    parser.add_argument("--baseline-csv", required=True, help="正式基线 CSV")
    parser.add_argument("--candidate-csv", required=True, help="候选实验 CSV")
    parser.add_argument("--out-dir", required=True, help="输出目录")
    parser.add_argument("--max-polygon-delta", type=int, default=25)
    args = parser.parse_args(argv)

    out_dir = Path(args.out_dir)
    baseline_shape_dir = out_dir / "baseline_shape_diagnosis"
    candidate_shape_dir = out_dir / "candidate_shape_diagnosis"
    baseline = evaluate_result(args.dat, args.baseline_csv, baseline_shape_dir)
    candidate = evaluate_result(args.dat, args.candidate_csv, candidate_shape_dir)
    decision = decide_promotion(baseline, candidate, max_polygon_delta=args.max_polygon_delta)
    summary_path, report_path = write_report(out_dir, baseline, candidate, decision)
    print(summary_path)
    print(report_path)
    return 0 if decision["pass"] else 2


if __name__ == "__main__":
    sys.exit(main())
