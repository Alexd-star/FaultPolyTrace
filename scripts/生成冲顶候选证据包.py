# -*- coding: utf-8 -*-
"""Build a review-ready evidence package for the peak candidate line."""

import argparse
import json
import shutil
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class IntegrationContext:
    integration_dir: Path
    manifest_path: Path
    manifest: dict
    promotion_path: Path
    promotion: dict


def _read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def _resolve_from(base_dir, value):
    path = Path(value)
    if path.is_absolute():
        return path
    direct = base_dir / path
    if direct.exists():
        return direct
    return path


def load_integration_context(integration_dir):
    integration_dir = Path(integration_dir)
    manifest_path = integration_dir / "冲顶候选集成入口清单.json"
    if not manifest_path.exists():
        raise FileNotFoundError(f"缺少冲顶候选集成入口清单：{manifest_path}")
    manifest = _read_json(manifest_path)
    promotion_path = _resolve_from(integration_dir, manifest.get("promotion_summary", ""))
    if not promotion_path.exists():
        fallback = integration_dir / "01_peak_candidate_pipeline" / "09_promotion_gate" / "冲顶实验晋级验收摘要.json"
        if fallback.exists():
            promotion_path = fallback
        else:
            raise FileNotFoundError(f"缺少晋级门控摘要：{promotion_path}")
    promotion = _read_json(promotion_path)
    return IntegrationContext(
        integration_dir=integration_dir,
        manifest_path=manifest_path,
        manifest=manifest,
        promotion_path=promotion_path,
        promotion=promotion,
    )


def _copy_if_exists(source, destination):
    source = Path(source)
    destination = Path(destination)
    if not source.exists():
        return False
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    return True


def _percent(value):
    try:
        return f"{float(value) * 100.0:.6f}%"
    except (TypeError, ValueError):
        return ""


def _candidate_files(context):
    manifest = context.manifest
    integration_dir = context.integration_dir
    final_candidate = _resolve_from(integration_dir, manifest.get("final_candidate_csv", ""))
    overview = _resolve_from(integration_dir, manifest.get("pure_figure_overview", ""))
    contact_sheet = overview.parent / "02_fault_polygons_tiles_contact_sheet_no_boxes.png"
    pipeline_report = integration_dir / "01_peak_candidate_pipeline" / "冲顶候选后处理流水线报告.md"
    integration_report = integration_dir / "冲顶候选集成入口说明.md"
    return {
        "final_candidate": final_candidate,
        "overview": overview,
        "contact_sheet": contact_sheet,
        "pipeline_report": pipeline_report,
        "integration_report": integration_report,
    }


def build_evidence_pack(context, out_dir):
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    files = _candidate_files(context)
    promotion = context.promotion
    baseline = promotion.get("baseline", {})
    candidate = promotion.get("candidate", {})
    decision = promotion.get("decision", {})

    copied = []
    copied.append(
        {
            "name": "最终候选 CSV",
            "source": str(files["final_candidate"]),
            "target": str(out_dir / "01_候选结果" / files["final_candidate"].name),
            "copied": _copy_if_exists(files["final_candidate"], out_dir / "01_候选结果" / files["final_candidate"].name),
        }
    )
    copied.append(
        {
            "name": "晋级门控摘要",
            "source": str(context.promotion_path),
            "target": str(out_dir / "02_晋级门控" / context.promotion_path.name),
            "copied": _copy_if_exists(context.promotion_path, out_dir / "02_晋级门控" / context.promotion_path.name),
        }
    )
    copied.append(
        {
            "name": "集成入口清单",
            "source": str(context.manifest_path),
            "target": str(out_dir / "02_晋级门控" / context.manifest_path.name),
            "copied": _copy_if_exists(context.manifest_path, out_dir / "02_晋级门控" / context.manifest_path.name),
        }
    )
    for key in ("overview", "contact_sheet"):
        source = files[key]
        copied.append(
            {
                "name": "无框图件",
                "source": str(source),
                "target": str(out_dir / "03_无框图件" / source.name),
                "copied": _copy_if_exists(source, out_dir / "03_无框图件" / source.name),
            }
        )
    for key in ("pipeline_report", "integration_report"):
        source = files[key]
        copied.append(
            {
                "name": "过程报告",
                "source": str(source),
                "target": str(out_dir / "04_过程报告" / source.name),
                "copied": _copy_if_exists(source, out_dir / "04_过程报告" / source.name),
            }
        )

    summary = {
        "source_integration_dir": str(context.integration_dir),
        "mode": context.manifest.get("mode", ""),
        "integration_boundary": context.manifest.get("integration_boundary", ""),
        "decision_pass": bool(decision.get("pass", False)),
        "baseline": baseline,
        "candidate": candidate,
        "decision": decision,
        "copied_items": copied,
    }
    (out_dir / "关键指标摘要.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )

    lines = [
        "# 冲顶候选证据索引",
        "",
        f"来源目录：`{context.integration_dir}`",
        "",
        "## 1. 使用边界",
        "",
        "本证据包用于复核冲顶候选线。候选线不是 GeoEast 或 C++ 默认正式输出，不能把该候选结果表述为已经替换正式默认流程。",
        "",
        f"- 集成模式：`{summary['mode']}`",
        f"- 边界标识：`{summary['integration_boundary']}`",
        f"- 晋级门控：`{'PASS' if summary['decision_pass'] else 'FAIL'}`",
        "",
        "## 2. 关键指标",
        "",
        "| 指标 | 正式基线 | 冲顶候选 |",
        "|---|---:|---:|",
        f"| 保留多边形 | {baseline.get('kept_polygons', '')} | {candidate.get('kept_polygons', '')} |",
        f"| 强响应覆盖率 | {_percent(baseline.get('strong_coverage'))} | {_percent(candidate.get('strong_coverage'))} |",
        f"| 弱响应覆盖率 | {_percent(baseline.get('weak_coverage'))} | {_percent(candidate.get('weak_coverage'))} |",
        f"| 开放环 | {baseline.get('open_rings', '')} | {candidate.get('open_rings', '')} |",
        f"| 自相交 | {baseline.get('self_intersections', '')} | {candidate.get('self_intersections', '')} |",
        f"| 多边形相交 | {baseline.get('polygon_intersections', '')} | {candidate.get('polygon_intersections', '')} |",
        f"| 局部形态候选 | {baseline.get('local_shape_candidates', '')} | {candidate.get('local_shape_candidates', '')} |",
        f"| 高优先级候选 | {baseline.get('high_priority_local_shape_candidates', '')} | {candidate.get('high_priority_local_shape_candidates', '')} |",
        "",
        "## 3. 文件清单",
        "",
        "| 文件 | 状态 | 来源 |",
        "|---|---|---|",
    ]
    for item in copied:
        lines.append(f"| {item['name']} | {'已复制' if item['copied'] else '缺失'} | `{item['source']}` |")
    lines.extend(
        [
            "",
            "## 4. 答辩口径",
            "",
            "可以表述为：正式 C++ 基线稳定，冲顶候选线在保持覆盖率和拓扑不退化的前提下降低了局部形态候选；该候选结果已形成可复跑证据包，后续进入默认流程前仍需要完成等价迁移和实际写入显示验证。",
        ]
    )
    (out_dir / "冲顶候选证据索引.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return summary


def main():
    parser = argparse.ArgumentParser(description="生成冲顶候选证据包")
    parser.add_argument("--integration-dir", required=True, help="冲顶候选集成入口输出目录")
    parser.add_argument("--out-dir", required=True, help="证据包输出目录")
    args = parser.parse_args()

    context = load_integration_context(args.integration_dir)
    summary = build_evidence_pack(context, args.out_dir)
    print(Path(args.out_dir) / "冲顶候选证据索引.md")
    print(json.dumps({"decision_pass": summary["decision_pass"]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
