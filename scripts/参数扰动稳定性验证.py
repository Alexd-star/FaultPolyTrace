# -*- coding: utf-8 -*-
"""Compare polygon results across parameter perturbations and presets."""

import argparse
import csv
import json
import math
import sys
from pathlib import Path

import numpy as np
from shapely import wkt
from shapely.validation import explain_validity


def _ratio(numerator, denominator):
    return float(numerator) / float(denominator) if denominator else 1.0


def read_kept_records(csv_path):
    records = []
    with open(csv_path, "r", encoding="utf-8-sig", newline="") as stream:
        for line_number, row in enumerate(csv.DictReader(stream), start=2):
            if row.get("status", "kept").strip().lower() != "kept":
                continue
            text = row.get("wkt", "").strip()
            if not text:
                continue
            try:
                geometry = wkt.loads(text)
            except Exception as exc:
                raise ValueError(
                    f"{csv_path} 第 {line_number} 行 WKT 无法解析：{exc}"
                ) from exc
            if geometry.is_empty:
                continue
            if not geometry.is_valid:
                raise ValueError(
                    f"{csv_path} 第 {line_number} 行几何无效："
                    f"{explain_validity(geometry)}"
                )
            object_key = (
                row.get("id", "").strip()
                or row.get("polygon_id", "").strip()
                or f"row_{line_number}"
            )
            if geometry.geom_type == "Polygon":
                records.append({"key": object_key, "geometry": geometry})
            elif geometry.geom_type == "MultiPolygon":
                for part_index, part in enumerate(geometry.geoms, start=1):
                    records.append(
                        {
                            "key": f"{object_key}:part_{part_index}",
                            "geometry": part,
                        }
                    )
            else:
                raise ValueError(
                    f"{csv_path} 第 {line_number} 行不是 Polygon/MultiPolygon："
                    f"{geometry.geom_type}"
                )
    return records


def read_kept_geometries(csv_path):
    return [record["geometry"] for record in read_kept_records(csv_path)]


def _intersection_area(left, right):
    if not left.bounds or not right.bounds:
        return 0.0
    if (
        left.bounds[2] < right.bounds[0]
        or right.bounds[2] < left.bounds[0]
        or left.bounds[3] < right.bounds[1]
        or right.bounds[3] < left.bounds[1]
    ):
        return 0.0
    return float(left.intersection(right).area)


def _iou(left, right, intersection_area=None):
    intersection_area = _intersection_area(left, right) if intersection_area is None else intersection_area
    union = float(left.area) + float(right.area) - intersection_area
    return intersection_area / union if union > 0.0 else 0.0


def _percentile(values, percentile, default=0.0):
    return float(np.percentile(values, percentile)) if values else float(default)


def _normalized_boundary_hausdorff(left, right):
    left_bounds = left.bounds
    right_bounds = right.bounds
    left_diagonal = math.hypot(
        float(left_bounds[2] - left_bounds[0]),
        float(left_bounds[3] - left_bounds[1]),
    )
    right_diagonal = math.hypot(
        float(right_bounds[2] - right_bounds[0]),
        float(right_bounds[3] - right_bounds[1]),
    )
    scale = max(left_diagonal, right_diagonal, 1e-12)
    return float(left.boundary.hausdorff_distance(right.boundary)) / scale


def _overlap_graph(reference, candidate, overlap_fraction):
    overlaps = []
    reference_links = [set() for _ in reference]
    candidate_links = [set() for _ in candidate]
    for ref_index, ref_geometry in enumerate(reference):
        ref_area = max(float(ref_geometry.area), 1e-12)
        for candidate_index, candidate_geometry in enumerate(candidate):
            candidate_area = max(float(candidate_geometry.area), 1e-12)
            intersection = _intersection_area(ref_geometry, candidate_geometry)
            if intersection <= 0.0:
                continue
            overlaps.append(
                (
                    _iou(ref_geometry, candidate_geometry, intersection),
                    ref_index,
                    candidate_index,
                )
            )
            linked = (
                intersection / ref_area >= float(overlap_fraction)
                or intersection / candidate_area >= float(overlap_fraction)
            )
            if linked:
                reference_links[ref_index].add(candidate_index)
                candidate_links[candidate_index].add(ref_index)
    return overlaps, reference_links, candidate_links


def _greedy_iou_matches(overlaps, min_match_iou):
    matched_reference = set()
    matched_candidate = set()
    matches = []
    for iou, ref_index, candidate_index in sorted(overlaps, reverse=True):
        if iou < float(min_match_iou):
            break
        if ref_index in matched_reference or candidate_index in matched_candidate:
            continue
        matched_reference.add(ref_index)
        matched_candidate.add(candidate_index)
        matches.append((float(iou), ref_index, candidate_index))
    return matches


def compare_geometry_sets(
    reference,
    candidate,
    min_match_iou=0.50,
    min_match_ratio=0.95,
    min_mean_iou=0.85,
    max_p95_area_change=0.20,
    max_p95_normalized_boundary_hausdorff=0.05,
    max_single_relative_area_change=0.75,
    max_single_normalized_boundary_hausdorff=0.25,
    overlap_fraction=0.25,
    reference_keys=None,
    candidate_keys=None,
):
    reference = list(reference)
    candidate = list(candidate)
    reference_keys = list(reference_keys) if reference_keys is not None else [
        f"reference_{index + 1}" for index in range(len(reference))
    ]
    candidate_keys = list(candidate_keys) if candidate_keys is not None else [
        f"candidate_{index + 1}" for index in range(len(candidate))
    ]
    if len(reference_keys) != len(reference):
        raise ValueError("reference_keys 数量必须与 reference 几何数量一致。")
    if len(candidate_keys) != len(candidate):
        raise ValueError("candidate_keys 数量必须与 candidate 几何数量一致。")
    overlaps, reference_links, candidate_links = _overlap_graph(
        reference,
        candidate,
        overlap_fraction,
    )
    matches = _greedy_iou_matches(overlaps, min_match_iou)
    matched_ious = []
    relative_area_changes = []
    normalized_boundary_hausdorff = []
    matched_objects = []
    for iou, ref_index, candidate_index in matches:
        matched_ious.append(float(iou))
        ref_area = max(float(reference[ref_index].area), 1e-12)
        relative_area_changes.append(abs(float(candidate[candidate_index].area) - ref_area) / ref_area)
        normalized_boundary_hausdorff.append(
            _normalized_boundary_hausdorff(
                reference[ref_index],
                candidate[candidate_index],
            )
        )
        matched_objects.append(
            {
                "reference_key": reference_keys[ref_index],
                "candidate_key": candidate_keys[candidate_index],
                "iou": float(iou),
                "relative_area_change": float(relative_area_changes[-1]),
                "normalized_boundary_hausdorff": float(
                    normalized_boundary_hausdorff[-1]
                ),
            }
        )

    matched_reference_ratio = _ratio(len(matches), len(reference))
    matched_candidate_ratio = _ratio(len(matches), len(candidate))
    mean_iou = float(np.mean(matched_ious)) if matched_ious else 0.0
    p95_area_change = _percentile(relative_area_changes, 95.0, default=1.0)
    p95_boundary_hausdorff = _percentile(
        normalized_boundary_hausdorff,
        95.0,
        default=1.0,
    )
    max_area_change = max(relative_area_changes, default=1.0)
    max_boundary_hausdorff = max(normalized_boundary_hausdorff, default=1.0)
    split_count = sum(1 for links in reference_links if len(links) >= 2)
    merge_count = sum(1 for links in candidate_links if len(links) >= 2)
    failure_reasons = []
    if matched_reference_ratio < float(min_match_ratio):
        failure_reasons.append("reference_match_ratio")
    if matched_candidate_ratio < float(min_match_ratio):
        failure_reasons.append("candidate_match_ratio")
    if mean_iou < float(min_mean_iou):
        failure_reasons.append("mean_iou")
    if p95_area_change > float(max_p95_area_change):
        failure_reasons.append("p95_area_change")
    if p95_boundary_hausdorff > float(max_p95_normalized_boundary_hausdorff):
        failure_reasons.append("p95_boundary_hausdorff")
    if max_area_change > float(max_single_relative_area_change):
        failure_reasons.append("max_area_change")
    if max_boundary_hausdorff > float(max_single_normalized_boundary_hausdorff):
        failure_reasons.append("max_boundary_hausdorff")
    if split_count:
        failure_reasons.append("split")
    if merge_count:
        failure_reasons.append("merge")
    stable = not failure_reasons
    matched_reference_indices = {ref_index for _, ref_index, _ in matches}
    matched_candidate_indices = {candidate_index for _, _, candidate_index in matches}
    split_reference_indices = [
        index for index, links in enumerate(reference_links) if len(links) >= 2
    ]
    merged_candidate_indices = [
        index for index, links in enumerate(candidate_links) if len(links) >= 2
    ]

    return {
        "stable": bool(stable),
        "reference_count": len(reference),
        "candidate_count": len(candidate),
        "matched_count": len(matches),
        "matched_reference_ratio": matched_reference_ratio,
        "matched_candidate_ratio": matched_candidate_ratio,
        "mean_matched_iou": mean_iou,
        "p10_matched_iou": _percentile(matched_ious, 10.0),
        "median_relative_area_change": _percentile(relative_area_changes, 50.0),
        "p95_relative_area_change": p95_area_change,
        "p95_normalized_boundary_hausdorff": p95_boundary_hausdorff,
        "max_relative_area_change": float(max_area_change),
        "max_normalized_boundary_hausdorff": float(max_boundary_hausdorff),
        "split_reference_count": split_count,
        "merged_candidate_count": merge_count,
        "failure_reasons": failure_reasons,
        "matched_objects": matched_objects,
        "unmatched_reference_keys": [
            reference_keys[index]
            for index in range(len(reference))
            if index not in matched_reference_indices
        ],
        "unmatched_candidate_keys": [
            candidate_keys[index]
            for index in range(len(candidate))
            if index not in matched_candidate_indices
        ],
        "split_reference_keys": [
            reference_keys[index] for index in split_reference_indices
        ],
        "merged_candidate_keys": [
            candidate_keys[index] for index in merged_candidate_indices
        ],
        "thresholds": {
            "min_match_iou": float(min_match_iou),
            "min_match_ratio": float(min_match_ratio),
            "min_mean_iou": float(min_mean_iou),
            "max_p95_area_change": float(max_p95_area_change),
            "max_p95_normalized_boundary_hausdorff": float(
                max_p95_normalized_boundary_hausdorff
            ),
            "max_single_relative_area_change": float(max_single_relative_area_change),
            "max_single_normalized_boundary_hausdorff": float(
                max_single_normalized_boundary_hausdorff
            ),
            "overlap_fraction": float(overlap_fraction),
        },
    }


def _consensus_matches(reference, candidate, min_match_iou, overlap_fraction):
    overlaps, reference_links, candidate_links = _overlap_graph(
        reference,
        candidate,
        overlap_fraction,
    )
    matches = _greedy_iou_matches(overlaps, min_match_iou)
    matched_by_reference = {
        ref_index: candidate_index
        for _, ref_index, candidate_index in matches
    }
    matched_candidate = {
        candidate_index
        for _, _, candidate_index in matches
    }
    linked_candidate = {
        candidate_index
        for candidate_index, links in enumerate(candidate_links)
        if links
    }
    ambiguous_reference = set()
    for ref_index, links in enumerate(reference_links):
        if len(links) >= 2:
            ambiguous_reference.add(ref_index)
    for candidate_index, links in enumerate(candidate_links):
        if len(links) >= 2:
            ambiguous_reference.update(links)
    return (
        matched_by_reference,
        matched_candidate,
        linked_candidate,
        ambiguous_reference,
    )


def classify_three_way_consensus(
    balanced,
    conservative,
    high_recall,
    min_match_iou=0.50,
    overlap_fraction=0.25,
):
    balanced = list(balanced)
    conservative = list(conservative)
    high_recall = list(high_recall)
    (
        conservative_matches,
        conservative_matched,
        conservative_linked,
        conservative_ambiguous,
    ) = _consensus_matches(
        balanced,
        conservative,
        min_match_iou,
        overlap_fraction,
    )
    (
        high_recall_matches,
        high_recall_matched,
        high_recall_linked,
        high_recall_ambiguous,
    ) = _consensus_matches(
        balanced,
        high_recall,
        min_match_iou,
        overlap_fraction,
    )
    ambiguous_balanced = conservative_ambiguous | high_recall_ambiguous
    high_confidence = 0
    balanced_review = 0
    for balanced_index in range(len(balanced)):
        if (
            balanced_index in conservative_matches
            and balanced_index in high_recall_matches
            and balanced_index not in ambiguous_balanced
            and _iou(
                conservative[conservative_matches[balanced_index]],
                high_recall[high_recall_matches[balanced_index]],
            )
            >= float(min_match_iou)
        ):
            high_confidence += 1
        else:
            balanced_review += 1
    return {
        "balanced_object_count": len(balanced),
        "conservative_object_count": len(conservative),
        "high_recall_object_count": len(high_recall),
        "high_confidence_count": high_confidence,
        "balanced_review_count": balanced_review,
        "ambiguous_balanced_count": len(ambiguous_balanced),
        "high_recall_only_count": len(high_recall) - len(high_recall_linked),
        "conservative_only_count": len(conservative) - len(conservative_linked),
        "high_recall_ambiguous_fragment_count": len(
            high_recall_linked - high_recall_matched
        ),
        "conservative_ambiguous_fragment_count": len(
            conservative_linked - conservative_matched
        ),
        "min_match_iou": float(min_match_iou),
        "overlap_fraction": float(overlap_fraction),
    }


def write_outputs(out_dir, baseline_name, comparisons, consensus=None):
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    payload = {
        "baseline": baseline_name,
        "all_stable": bool(comparisons)
        and all(item["metrics"]["stable"] for item in comparisons),
        "comparisons": comparisons,
        "three_way_consensus": consensus,
        "evidence_boundary": (
            "This is parameter/preset consistency evidence. It does not use an "
            "expert geological reference and cannot prove geological correctness."
        ),
    }
    json_path = out_dir / "参数扰动稳定性摘要.json"
    json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

    csv_path = out_dir / "参数扰动稳定性明细.csv"
    fields = [
        "name",
        "stable",
        "reference_count",
        "candidate_count",
        "matched_reference_ratio",
        "matched_candidate_ratio",
        "mean_matched_iou",
        "p95_relative_area_change",
        "p95_normalized_boundary_hausdorff",
        "max_relative_area_change",
        "max_normalized_boundary_hausdorff",
        "split_reference_count",
        "merged_candidate_count",
        "failure_reasons",
    ]
    with open(csv_path, "w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for item in comparisons:
            row = {
                "name": item["name"],
                **{
                    key: item["metrics"][key]
                    for key in fields
                    if key not in {"name", "failure_reasons"}
                },
                "failure_reasons": ";".join(item["metrics"]["failure_reasons"]),
            }
            writer.writerow(row)

    report_path = out_dir / "参数扰动稳定性报告.md"
    lines = [
        "# 参数扰动稳定性报告",
        "",
        "本报告比较同一数据在小范围参数扰动或不同预设下的对象稳定性。它不使用专家标准答案，因此只能证明算法一致性，不能单独证明地质解释正确。",
        "",
        f"- 基线：`{baseline_name}`",
        f"- 总体稳定：{'是' if payload['all_stable'] else '否'}",
        "",
        "| 结果 | 稳定 | 对象数 | 基线/候选匹配率 | 平均 IoU | P95/最大面积变化 | P95/最大归一化边界 Hausdorff | split | merge |",
        "|---|---|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for item in comparisons:
        metrics = item["metrics"]
        lines.append(
            f"| {item['name']} | {'是' if metrics['stable'] else '否'} | "
            f"{metrics['candidate_count']} | "
            f"{metrics['matched_reference_ratio']:.2%}/"
            f"{metrics['matched_candidate_ratio']:.2%} | "
            f"{metrics['mean_matched_iou']:.4f} | "
            f"{metrics['p95_relative_area_change']:.2%}/{metrics['max_relative_area_change']:.2%} | "
            f"{metrics['p95_normalized_boundary_hausdorff']:.4f}/"
            f"{metrics['max_normalized_boundary_hausdorff']:.4f} | "
            f"{metrics['split_reference_count']} | {metrics['merged_candidate_count']} |"
        )
        if not metrics["stable"]:
            lines.extend(
                [
                    "",
                    f"### {item['name']} 失败定位",
                    "",
                    f"- 失败门禁：`{', '.join(metrics['failure_reasons'])}`",
                    f"- 未匹配基线对象：`{', '.join(metrics['unmatched_reference_keys'][:30]) or '无'}`",
                    f"- 未匹配候选对象：`{', '.join(metrics['unmatched_candidate_keys'][:30]) or '无'}`",
                    f"- split 基线对象：`{', '.join(metrics['split_reference_keys'][:30]) or '无'}`",
                    f"- merge 候选对象：`{', '.join(metrics['merged_candidate_keys'][:30]) or '无'}`",
                ]
            )
    if consensus:
        lines.extend(
            [
                "",
                "## 三预设一致性",
                "",
                f"- 三种预设共同支持的高置信对象：{consensus['high_confidence_count']}",
                f"- 均衡结果中需要复核的对象：{consensus['balanced_review_count']}",
                f"- 涉及 split/merge 歧义的均衡对象：{consensus['ambiguous_balanced_count']}",
                f"- 仅高召回预设出现的对象：{consensus['high_recall_only_count']}",
                f"- 仅保守预设出现的对象：{consensus['conservative_only_count']}",
                f"- 高召回预设中的 split/merge 歧义碎片：{consensus['high_recall_ambiguous_fragment_count']}",
                f"- 保守预设中的 split/merge 歧义碎片：{consensus['conservative_ambiguous_fragment_count']}",
            ]
        )
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return json_path, csv_path, report_path


def write_failure_outputs(out_dir, baseline_name, error):
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    message = str(error)
    payload = {
        "baseline": baseline_name,
        "all_stable": False,
        "comparisons": [],
        "three_way_consensus": None,
        "error": message,
        "evidence_boundary": (
            "The stability validation failed before comparison completed. "
            "This failure evidence does not prove geological correctness."
        ),
    }
    json_path = out_dir / "参数扰动稳定性摘要.json"
    json_path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    csv_path = out_dir / "参数扰动稳定性明细.csv"
    with open(csv_path, "w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=["name", "stable", "error"])
        writer.writeheader()
        writer.writerow(
            {
                "name": "validation_error",
                "stable": False,
                "error": message,
            }
        )
    report_path = out_dir / "参数扰动稳定性报告.md"
    report_path.write_text(
        "\n".join(
            [
                "# 参数扰动稳定性报告",
                "",
                "- 总体稳定：否",
                "- 验证状态：执行失败",
                "",
                "## 错误信息",
                "",
                message,
                "",
                "本次未形成可用于晋级判断的稳定性比较结果，必须先修复输入或几何错误。",
                "",
            ]
        ),
        encoding="utf-8",
    )
    return json_path, csv_path, report_path


def _parse_named_path(text):
    if "=" not in text:
        raise argparse.ArgumentTypeError("Use NAME=PATH")
    name, path = text.split("=", 1)
    if not name.strip() or not path.strip():
        raise argparse.ArgumentTypeError("Use NAME=PATH")
    return name.strip(), Path(path.strip())


def main(argv=None):
    parser = argparse.ArgumentParser(description="验证断层多边形结果对参数扰动和预设变化的稳定性")
    parser.add_argument("--baseline", required=True, type=_parse_named_path, help="NAME=CSV")
    parser.add_argument(
        "--candidate",
        action="append",
        required=True,
        type=_parse_named_path,
        help="NAME=CSV，可重复，至少提供一个",
    )
    parser.add_argument("--conservative", type=Path)
    parser.add_argument("--high-recall", type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    parser.add_argument("--min-match-iou", type=float, default=0.50)
    args = parser.parse_args(argv)
    if bool(args.conservative) != bool(args.high_recall):
        parser.error("--conservative 与 --high-recall 必须同时提供。")

    baseline_name, baseline_path = args.baseline
    record_cache = {}

    def load_records(path):
        cache_key = str(Path(path).resolve())
        if cache_key not in record_cache:
            record_cache[cache_key] = read_kept_records(path)
        return record_cache[cache_key]

    try:
        baseline_records = load_records(baseline_path)
        baseline = [record["geometry"] for record in baseline_records]
        baseline_keys = [record["key"] for record in baseline_records]
        comparisons = []
        for name, path in args.candidate:
            candidate_records = load_records(path)
            geometries = [record["geometry"] for record in candidate_records]
            comparisons.append(
                {
                    "name": name,
                    "path": str(path),
                    "metrics": compare_geometry_sets(
                        baseline,
                        geometries,
                        min_match_iou=args.min_match_iou,
                        reference_keys=baseline_keys,
                        candidate_keys=[
                            record["key"] for record in candidate_records
                        ],
                    ),
                }
            )

        consensus = None
        if args.conservative and args.high_recall:
            conservative_records = load_records(args.conservative)
            high_recall_records = load_records(args.high_recall)
            consensus = classify_three_way_consensus(
                baseline,
                [record["geometry"] for record in conservative_records],
                [record["geometry"] for record in high_recall_records],
                min_match_iou=args.min_match_iou,
            )
        paths = write_outputs(args.out_dir, baseline_name, comparisons, consensus)
        for path in paths:
            print(path)
        return 0 if all(item["metrics"]["stable"] for item in comparisons) else 2
    except Exception as exc:
        paths = write_failure_outputs(args.out_dir, baseline_name, exc)
        for path in paths:
            print(path)
        print(f"参数扰动稳定性验证失败：{exc}", file=sys.stderr)
        return 3


if __name__ == "__main__":
    raise SystemExit(main())
