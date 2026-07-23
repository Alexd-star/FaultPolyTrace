# -*- coding: utf-8 -*-
"""Synthetic generalization checks for local morphology experiments.

The official test.dat result is still the primary competition evidence. This
script complements it with small, interpretable synthetic cases so we can see
whether the local quality gates behave by principle instead of by dataset-
specific polygon ids.
"""

import argparse
import json
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from shapely.geometry import box

from 局部形态质量诊断 import configure_chinese_font, detect_local_bulges
from 分支归属拆分实验 import split_polygon_by_support_components


def _case_result(case_id, name, passed, **extra):
    return {
        "case_id": case_id,
        "name": name,
        "pass": bool(passed),
        "source": "synthetic",
        **extra,
    }


def _save_mask(path, mask, title):
    fig, ax = plt.subplots(figsize=(5.2, 3.0), dpi=180)
    ax.imshow(mask, origin="lower", cmap="gray_r", interpolation="nearest")
    ax.set_title(title, fontsize=10)
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.tick_params(labelsize=7)
    fig.tight_layout()
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)


def _add_sparse_noise(mask, seed=0, count=0):
    if count <= 0:
        return mask
    rng = np.random.default_rng(seed)
    height, width = mask.shape
    for _ in range(int(count)):
        x = int(rng.integers(0, width))
        y = int(rng.integers(0, height))
        mask[y, x] = True
    return mask


def _slender_band_mask(width=120, height=40, thickness=4, x0=5, x1=None, y=None, noise=0, seed=0):
    mask = np.zeros((height, width), dtype=bool)
    x1 = width - 5 if x1 is None else x1
    y = height // 2 if y is None else y
    half = max(1, thickness // 2)
    mask[max(0, y - half) : min(height, y - half + thickness), x0:x1] = True
    _add_sparse_noise(mask, seed=seed, count=noise)
    return mask


def _pouch_mask(width=120, height=40, thickness=4, pouch_width=16, pouch_height=16, center_x=None, center_y=None):
    mask = _slender_band_mask(width=width, height=height, thickness=thickness)
    center_x = width // 2 if center_x is None else center_x
    center_y = height // 2 if center_y is None else center_y
    px0 = max(0, center_x - pouch_width // 2)
    px1 = min(width, center_x + pouch_width // 2)
    py0 = max(0, center_y - pouch_height // 2)
    py1 = min(height, center_y + pouch_height // 2)
    mask[py0:py1, px0:px1] = True
    return mask


def _wide_band_mask(width=140, height=40, thickness=4, wide_height=12, span_ratio=0.58):
    mask = np.zeros((height, width), dtype=bool)
    y = height // 2
    mask[y - thickness // 2 : y - thickness // 2 + thickness, 5 : width - 5] = True
    span = int(width * span_ratio)
    x0 = max(5, (width - span) // 2)
    x1 = min(width - 5, x0 + span)
    mask[max(0, y - wide_height // 2) : min(height, y + wide_height // 2), x0:x1] = True
    return mask


def _branch_junction_mask(width=140, height=80, trunk_thickness=4, arm_length=55, upper_slope=0.45, lower_slope=0.40):
    mask = np.zeros((height, width), dtype=bool)
    center_y = height // 2
    start_x = width // 2 - 5
    mask[center_y - trunk_thickness // 2 : center_y - trunk_thickness // 2 + trunk_thickness, 10 : width - 15] = True
    for i in range(arm_length):
        x = start_x + i
        upper_y = center_y - int(i * upper_slope)
        lower_y = center_y + int(i * lower_slope)
        mask[max(0, upper_y - 2) : min(height, upper_y + 2), max(0, x - 1) : min(width, x + 2)] = True
        mask[max(0, lower_y - 2) : min(height, lower_y + 2), max(0, x - 1) : min(width, x + 2)] = True
    return mask


def _first_class(candidates):
    if not candidates:
        return "", "", 0
    first = candidates[0]
    return (
        first.get("shape_class", ""),
        first.get("review_priority", ""),
        int(first.get("branch_arm_count", 0) or 0),
    )


def _run_shape_cases(figures_dir):
    cases = []

    mask = _slender_band_mask()
    _save_mask(figures_dir / "01_slender_no_false_positive.png", mask, "细长断层带：不应误报")
    candidates = detect_local_bulges(mask, window=9, width_ratio=2.2, min_extra_cells=30)
    cases.append(
        _case_result(
            "slender_no_false_positive",
            "细长断层带不应产生局部鼓包误报",
            candidates == [],
            family="slender",
            observed_count=len(candidates),
        )
    )

    mask = _pouch_mask()
    _save_mask(figures_dir / "02_local_pouch_detected.png", mask, "局部囊状鼓包：应识别为高风险")
    candidates = detect_local_bulges(mask, window=9, width_ratio=2.2, min_extra_cells=30)
    shape_class, priority, arm_count = _first_class(candidates)
    cases.append(
        _case_result(
            "local_pouch_detected",
            "局部囊状鼓包应识别为局部鼓包",
            len(candidates) == 1 and shape_class == "局部鼓包" and priority == "high",
            family="local_pouch",
            observed_count=len(candidates),
            observed_class=shape_class,
            observed_priority=priority,
            branch_arm_count=arm_count,
        )
    )

    mask = _wide_band_mask()
    _save_mask(figures_dir / "03_long_wide_band_review.png", mask, "长段宽断层带：应进入复核而非强裁")
    candidates = detect_local_bulges(mask, window=9, width_ratio=2.2, min_extra_cells=30)
    shape_class, priority, arm_count = _first_class(candidates)
    cases.append(
        _case_result(
            "long_wide_band_review",
            "长段宽断层带应标记为长段宽带复核",
            len(candidates) == 1 and shape_class == "长段宽带复核" and priority == "medium",
            family="long_wide_band",
            observed_count=len(candidates),
            observed_class=shape_class,
            observed_priority=priority,
            branch_arm_count=arm_count,
        )
    )

    mask = _branch_junction_mask()
    _save_mask(figures_dir / "04_branch_junction_not_pouch.png", mask, "Y 字分支交切：不应误判为囊状鼓包")
    candidates = detect_local_bulges(mask, window=9, width_ratio=2.2, min_extra_cells=30)
    shape_class, priority, arm_count = _first_class(candidates)
    cases.append(
        _case_result(
            "branch_junction_not_pouch",
            "Y 字分支交切应标记为分支交切复核",
            len(candidates) == 1 and shape_class == "分支交切复核" and priority == "medium" and arm_count >= 3,
            family="branch_junction",
            observed_count=len(candidates),
            observed_class=shape_class,
            observed_priority=priority,
            branch_arm_count=arm_count,
        )
    )

    return cases


def _shape_variant_case(case_id, name, family, mask, expected_class, expected_priority, figures_dir, figure_prefix):
    _save_mask(figures_dir / f"{figure_prefix}_{case_id}.png", mask, name)
    candidates = detect_local_bulges(mask, window=9, width_ratio=2.2, min_extra_cells=30)
    shape_class, priority, arm_count = _first_class(candidates)
    if expected_class == "":
        passed = candidates == []
    else:
        passed = len(candidates) == 1 and shape_class == expected_class and priority == expected_priority
        if expected_class == "分支交切复核":
            passed = passed and arm_count >= 3
    return _case_result(
        case_id,
        name,
        passed,
        family=family,
        observed_count=len(candidates),
        observed_class=shape_class,
        observed_priority=priority,
        branch_arm_count=arm_count,
    )


def _run_shape_variant_cases(figures_dir):
    cases = []

    slender_variants = [
        {"width": 100, "height": 38, "thickness": 3, "noise": 0},
        {"width": 150, "height": 48, "thickness": 4, "noise": 4, "seed": 1},
        {"width": 180, "height": 54, "thickness": 5, "noise": 6, "seed": 2},
        {"width": 130, "height": 42, "thickness": 6, "noise": 0},
    ]
    for index, params in enumerate(slender_variants, start=1):
        cases.append(
            _shape_variant_case(
                f"slender_variant_{index}",
                f"细长断层带泛化变体 {index}",
                "slender",
                _slender_band_mask(**params),
                "",
                "",
                figures_dir,
                "10",
            )
        )

    pouch_variants = [
        {"width": 120, "height": 42, "thickness": 4, "pouch_width": 14, "pouch_height": 18},
        {"width": 150, "height": 50, "thickness": 4, "pouch_width": 18, "pouch_height": 20, "center_x": 82},
        {"width": 135, "height": 44, "thickness": 5, "pouch_width": 16, "pouch_height": 18, "center_x": 58},
        {"width": 180, "height": 54, "thickness": 5, "pouch_width": 20, "pouch_height": 24, "center_x": 96},
    ]
    for index, params in enumerate(pouch_variants, start=1):
        cases.append(
            _shape_variant_case(
                f"local_pouch_variant_{index}",
                f"局部囊状鼓包泛化变体 {index}",
                "local_pouch",
                _pouch_mask(**params),
                "局部鼓包",
                "high",
                figures_dir,
                "20",
            )
        )

    wide_variants = [
        {"width": 140, "height": 44, "thickness": 4, "wide_height": 12, "span_ratio": 0.62},
        {"width": 180, "height": 56, "thickness": 5, "wide_height": 16, "span_ratio": 0.58},
        {"width": 160, "height": 48, "thickness": 4, "wide_height": 14, "span_ratio": 0.60},
        {"width": 200, "height": 58, "thickness": 5, "wide_height": 16, "span_ratio": 0.55},
    ]
    for index, params in enumerate(wide_variants, start=1):
        cases.append(
            _shape_variant_case(
                f"long_wide_band_variant_{index}",
                f"长段宽断层带泛化变体 {index}",
                "long_wide_band",
                _wide_band_mask(**params),
                "长段宽带复核",
                "medium",
                figures_dir,
                "30",
            )
        )

    branch_variants = [
        {"width": 140, "height": 80, "arm_length": 55, "upper_slope": 0.45, "lower_slope": 0.40},
        {"width": 160, "height": 90, "arm_length": 65, "upper_slope": 0.35, "lower_slope": 0.48},
        {"width": 180, "height": 95, "arm_length": 70, "upper_slope": 0.52, "lower_slope": 0.36},
        {"width": 150, "height": 88, "arm_length": 58, "upper_slope": 0.42, "lower_slope": 0.42},
    ]
    for index, params in enumerate(branch_variants, start=1):
        cases.append(
            _shape_variant_case(
                f"branch_junction_variant_{index}",
                f"Y 字分支交切泛化变体 {index}",
                "branch_junction",
                _branch_junction_mask(**params),
                "分支交切复核",
                "medium",
                figures_dir,
                "40",
            )
        )

    return cases


def _run_split_cases():
    cases = []

    branch_a = box(0, 0, 100, 8)
    branch_b = box(0, 30, 100, 38)
    bridge = box(45, 8, 55, 30)
    polygon = branch_a.union(branch_b).union(bridge)
    support = branch_a.union(branch_b)
    candidate = box(-5, -5, 105, 45)
    repaired, decision = split_polygon_by_support_components(
        polygon,
        candidate,
        support,
        min_component_area=100,
        min_retention_ratio=0.60,
    )
    cases.append(
        _case_result(
            "two_supported_branches_split",
            "两个有支撑分支之间的低支撑桥应允许拆分",
            decision["status"] == "accepted_precheck" and len(list(repaired.geoms)) == 2,
            family="branch_split",
            decision_status=decision["status"],
            component_count=decision["component_count"],
            retention_ratio=decision["retention_ratio"],
        )
    )

    polygon = box(0, 0, 100, 8)
    support = polygon.buffer(1)
    candidate = box(-5, -5, 105, 15)
    repaired, decision = split_polygon_by_support_components(
        polygon,
        candidate,
        support,
        min_component_area=100,
        min_retention_ratio=0.60,
    )
    cases.append(
        _case_result(
            "single_branch_rejected",
            "单一连续分支证据不足时必须回退",
            decision["status"] == "rejected_not_enough_components" and repaired.equals_exact(polygon, tolerance=1e-9),
            family="fallback_guard",
            decision_status=decision["status"],
            component_count=decision["component_count"],
            retention_ratio=decision["retention_ratio"],
        )
    )

    branch_a = box(0, 0, 100, 8)
    branch_b = box(0, 30, 100, 38)
    bridge = box(0, 8, 100, 30)
    polygon = branch_a.union(branch_b).union(bridge)
    support = branch_a.union(branch_b)
    candidate = box(-5, -5, 105, 45)
    repaired, decision = split_polygon_by_support_components(
        polygon,
        candidate,
        support,
        min_component_area=100,
        min_retention_ratio=0.80,
    )
    cases.append(
        _case_result(
            "low_retention_rejected",
            "拆分会删除过多候选框面积时必须回退",
            decision["status"] == "rejected_low_retention" and repaired.equals_exact(polygon, tolerance=1e-9),
            family="fallback_guard",
            decision_status=decision["status"],
            component_count=decision["component_count"],
            retention_ratio=decision["retention_ratio"],
        )
    )

    return cases


def _run_split_variant_cases():
    cases = []

    split_variants = [
        {"gap": 18, "bridge_width": 8, "branch_length": 90},
        {"gap": 24, "bridge_width": 10, "branch_length": 120},
        {"gap": 30, "bridge_width": 12, "branch_length": 140},
    ]
    for index, params in enumerate(split_variants, start=1):
        gap = params["gap"]
        bridge_width = params["bridge_width"]
        branch_length = params["branch_length"]
        branch_a = box(0, 0, branch_length, 8)
        branch_b = box(0, 8 + gap, branch_length, 16 + gap)
        bridge_x0 = branch_length / 2 - bridge_width / 2
        bridge_x1 = branch_length / 2 + bridge_width / 2
        bridge = box(bridge_x0, 8, bridge_x1, 8 + gap)
        polygon = branch_a.union(branch_b).union(bridge)
        support = branch_a.union(branch_b)
        candidate = box(-5, -5, branch_length + 5, gap + 25)
        repaired, decision = split_polygon_by_support_components(
            polygon,
            candidate,
            support,
            min_component_area=100,
            min_retention_ratio=0.58,
        )
        cases.append(
            _case_result(
                f"two_supported_branches_split_variant_{index}",
                f"双分支低支撑桥拆分泛化变体 {index}",
                decision["status"] == "accepted_precheck" and decision["component_count"] == 2,
                family="branch_split",
                decision_status=decision["status"],
                component_count=decision["component_count"],
                retention_ratio=decision["retention_ratio"],
            )
        )

    fallback_variants = [
        {"height": 8, "buffer": 1.0, "min_retention": 0.60},
        {"height": 10, "buffer": 1.5, "min_retention": 0.60},
        {"height": 12, "buffer": 2.0, "min_retention": 0.60},
    ]
    for index, params in enumerate(fallback_variants, start=1):
        height = params["height"]
        polygon = box(0, 0, 120, height)
        support = polygon.buffer(params["buffer"])
        candidate = box(-5, -5, 125, height + 8)
        repaired, decision = split_polygon_by_support_components(
            polygon,
            candidate,
            support,
            min_component_area=100,
            min_retention_ratio=params["min_retention"],
        )
        cases.append(
            _case_result(
                f"single_branch_rejected_variant_{index}",
                f"单分支持续响应拒绝拆分泛化变体 {index}",
                decision["status"] == "rejected_not_enough_components" and repaired.equals_exact(polygon, tolerance=1e-9),
                family="fallback_guard",
                decision_status=decision["status"],
                component_count=decision["component_count"],
                retention_ratio=decision["retention_ratio"],
            )
        )

    return cases


def _write_report(out_dir, report):
    json_path = out_dir / "泛化能力回归验证摘要.json"
    json_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# 泛化能力回归验证报告",
        "",
        "本报告用于回答实验方法是否具备复用基础。它不替代官方 `test.dat` 验收，而是用可解释的合成场景验证核心门控是否按地质形态原则工作。",
        "",
        f"- 场景数量：{report['case_count']}",
        f"- 通过数量：{report['passed_case_count']}",
        f"- 失败数量：{len(report['failed_cases'])}",
        f"- 场景族：{', '.join(report['families'])}",
        f"- 数据集专用 ID：{'无' if not report['dataset_specific_ids'] else ', '.join(report['dataset_specific_ids'])}",
        f"- 总体结论：{'通过' if report['pass'] else '未通过'}",
        "",
        "## GeoEast 复用契约",
        "",
        "| 项目 | 内容 |",
        "|---|---|",
        f"| 可复用 | {report['geoeast_reuse_contract']['reusable']} |",
        f"| 集成阶段 | {report['geoeast_reuse_contract']['integration_stage']} |",
        f"| 是否使用数据集专用 polygon 编号 | {report['geoeast_reuse_contract']['uses_dataset_specific_polygon_ids']} |",
        f"| 是否需要运行时对象关联键 | {report['geoeast_reuse_contract']['requires_runtime_object_key']} |",
        f"| 运行时对象关联键 | {report['geoeast_reuse_contract']['runtime_object_key']} |",
        f"| C++/GeoEast 等价状态 | {report['geoeast_reuse_contract']['cpp_geoeast_parity_status']} |",
        f"| 输入 | {', '.join(report['geoeast_reuse_contract']['inputs'])} |",
        f"| 输出 | {', '.join(report['geoeast_reuse_contract']['outputs'])} |",
        f"| 晋级门禁 | {', '.join(report['geoeast_reuse_contract']['promotion_gate'])} |",
        "",
        "## 场景明细",
        "",
        "| 场景 | 场景族 | 名称 | 结果 | 观测类别/决策 | 说明 |",
        "|---|---|---|---|---|---|",
    ]
    for case in report["cases"]:
        observed = case.get("observed_class") or case.get("decision_status") or "-"
        lines.append(
            f"| `{case['case_id']}` | {case.get('family', '')} | {case['name']} | {'PASS' if case['pass'] else 'FAIL'} | {observed} | {case.get('observed_priority', '')} |"
        )
    lines.extend(
        [
            "",
            "## 解释",
            "",
            "若这些合成场景全部通过，说明当前局部形态诊断和分支归属拆分并非依赖某个固定数据集专用 polygon 编号，而是依据形态、属性支撑、候选框内保留比例和分支臂数量等可迁移证据工作。实验运行仍需要 polygon_id 作为候选记录与结果对象之间的唯一关联键，这不等于针对某个固定编号编写规则。当前结论是“具备可复用实验门禁基础”，不是“已经完成全部产品化泛化”。真正进入 GeoEast 正式主线前，还必须完成 C++ 等价实现、官方数据指标不退化验证、GeoEast runner 写入/查看验证，以及更多真实工区回归。",
        ]
    )
    md_path = out_dir / "泛化能力回归验证报告.md"
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def run_synthetic_generalization_suite(out_dir):
    configure_chinese_font()
    out_dir = Path(out_dir)
    figures_dir = out_dir / "figures"
    out_dir.mkdir(parents=True, exist_ok=True)
    figures_dir.mkdir(parents=True, exist_ok=True)

    cases = []
    cases.extend(_run_shape_cases(figures_dir))
    cases.extend(_run_shape_variant_cases(figures_dir))
    cases.extend(_run_split_cases())
    cases.extend(_run_split_variant_cases())
    failed = [case["case_id"] for case in cases if not case["pass"]]
    families = sorted({case.get("family", "unknown") for case in cases})
    report = {
        "pass": len(failed) == 0,
        "case_count": len(cases),
        "passed_case_count": sum(1 for case in cases if case["pass"]),
        "failed_cases": failed,
        "families": families,
        "family_counts": {family: sum(1 for case in cases if case.get("family") == family) for family in families},
        "dataset_specific_ids": [],
        "generalization_level": "synthetic_multi_scenario_gate",
        "geoeast_reuse_contract": {
            "reusable": True,
            "integration_stage": "quality_gate_and_postprocess_experiment",
            "uses_dataset_specific_polygon_ids": False,
            "requires_runtime_object_key": True,
            "runtime_object_key": "polygon_id",
            "cpp_geoeast_parity_status": "not_verified_by_this_suite",
            "inputs": ["dat", "result_csv", "shape_candidates"],
            "outputs": ["summary_json", "report_md", "figures_dir", "diagnostic_csv"],
            "promotion_gate": [
                "official_data_metrics_not_regressed",
                "topology_parse_invalid_intersection_all_zero",
                "synthetic_generalization_cases_all_pass",
                "cpp_geoeast_parity_must_be_verified_separately",
            ],
        },
        "cases": cases,
        "figures_dir": str(figures_dir),
    }
    _write_report(out_dir, report)
    return report


def main():
    configure_chinese_font()
    parser = argparse.ArgumentParser(description="运行断层多边形局部质量门控的合成泛化回归验证")
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args()
    report = run_synthetic_generalization_suite(Path(args.out_dir))
    print(Path(args.out_dir) / "泛化能力回归验证摘要.json")
    print(Path(args.out_dir) / "泛化能力回归验证报告.md")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
