# -*- coding: utf-8 -*-
"""Controlled entry point for the peak-quality post-processing candidate.

This module intentionally keeps two outputs separate:

1. the formal C++ baseline CSV produced by ``faultpolytrace.exe``;
2. the gated peak candidate CSV produced by the Python post-processing
   pipeline.

It is an integration bridge for validation and packaging. It does not silently
replace the default C++ or GeoEast output line.
"""

import argparse
import json
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

from 冲顶候选后处理流水线 import build_pipeline_steps, run_steps, write_pipeline_report


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent


@dataclass(frozen=True)
class IntegrationPlan:
    mode: str
    dat_path: Path
    out_dir: Path
    core_exe: Path
    input_format: str
    formal_baseline_dir: Path
    formal_baseline_csv: Path
    candidate_output_dir: Path
    final_candidate_csv: Path
    promotion_summary: Path
    pure_figure_overview: Path
    manifest_path: Path
    formal_command: list
    pipeline_steps: list


def _as_path(value):
    return Path(value).expanduser()


def build_formal_baseline_command(core_exe, dat_path, baseline_csv, input_format="geoeast-dat"):
    """Build the formal contest-quality C++ command without executing it."""
    core_exe = _as_path(core_exe)
    dat_path = _as_path(dat_path)
    baseline_csv = _as_path(baseline_csv)
    return [
        str(core_exe),
        "--input",
        str(dat_path),
        "--input-format",
        str(input_format),
        "--output",
        str(baseline_csv),
        "--diagnostics",
        str(baseline_csv) + ".diagnostics.json",
        "--branches-output",
        str(baseline_csv) + ".branches.csv",
        "--topology-output",
        str(baseline_csv) + ".topology.csv",
        "--trace-mode",
        "ridge-belt",
        "--threshold-mode",
        "percentile",
        "--percentile",
        "0.98",
        "--close-radius",
        "1",
        "--weak-percentile",
        "0.965",
        "--weak-connect-radius",
        "1",
        "--max-weak-component-cells",
        "20000",
        "--min-weak-component-aspect",
        "3",
        "--belt-radius",
        "3",
        "--attribute-constrained-belt",
        "--attribute-envelope-radius",
        "1",
        "--post-belt-close-radius",
        "1",
        "--min-branch-length",
        "4",
        "--min-junction-branch-length",
        "2",
        "--min-junction-branch-mean",
        "0.94",
        "--max-gap-distance",
        "12",
        "--max-gap-angle",
        "55",
        "--min-gap-support-ratio",
        "0.25",
        "--no-separate-branch-belts",
        "--root-anti-merge",
        "--root-anti-merge-support-ratio",
        "0.25",
        "--root-anti-merge-min-part-cells",
        "0",
        "--root-anti-merge-min-part-ratio",
        "0.12",
        "--root-anti-merge-weak-guard-threshold",
        "0.66",
        "--root-anti-merge-weak-guard-radius",
        "0",
        "--residual-percentile",
        "0.955",
        "--residual-fill-radius",
        "4",
        "--residual-edge-fill-radius",
        "2",
        "--residual-edge-fill-min-value-ratio",
        "0.97",
        "--residual-edge-fill-max-patch-span",
        "5",
        "--residual-min-cells",
        "2",
        "--residual-trace-min-length",
        "6",
        "--residual-trace-max-cells",
        "80",
        "--residual-covered-guard-radius",
        "0",
        "--no-residual-trace",
        "--min-area",
        "120",
        "--simplify-tolerance",
        "2",
        "--smooth-iterations",
        "1",
    ]


def build_integration_plan(
    dat_path,
    out_dir,
    core_exe=None,
    baseline_csv=None,
    input_format="geoeast-dat",
    python_executable=None,
    max_polygon_delta=25,
    rows=4,
    cols=6,
):
    """Create a fully traceable formal-baseline plus candidate-pipeline plan."""
    dat_path = _as_path(dat_path)
    out_dir = _as_path(out_dir)
    core_exe = _as_path(core_exe or (REPO_ROOT / "build" / "faultpolytrace.exe"))

    formal_dir = out_dir / "00_formal_cpp_baseline"
    formal_csv = _as_path(baseline_csv) if baseline_csv else formal_dir / "official_root_antimerge.csv"
    candidate_dir = out_dir / "01_peak_candidate_pipeline"

    formal_command = build_formal_baseline_command(
        core_exe=core_exe,
        dat_path=dat_path,
        baseline_csv=formal_csv,
        input_format=input_format,
    )
    pipeline_steps = build_pipeline_steps(
        dat_path=dat_path,
        baseline_csv=formal_csv,
        out_dir=candidate_dir,
        python_executable=python_executable or sys.executable,
        max_polygon_delta=max_polygon_delta,
        figure_rows=rows,
        figure_cols=cols,
    )

    return IntegrationPlan(
        mode="controlled_postprocess",
        dat_path=dat_path,
        out_dir=out_dir,
        core_exe=core_exe,
        input_format=str(input_format),
        formal_baseline_dir=formal_dir,
        formal_baseline_csv=formal_csv,
        candidate_output_dir=candidate_dir,
        final_candidate_csv=pipeline_steps[7].primary_output,
        promotion_summary=pipeline_steps[8].primary_output,
        pure_figure_overview=pipeline_steps[9].primary_output,
        manifest_path=out_dir / "冲顶候选集成入口清单.json",
        formal_command=formal_command,
        pipeline_steps=pipeline_steps,
    )


def _command_record(command):
    return [str(part) for part in command]


def write_manifest(plan, status, formal_returncode=None, pipeline_summary_path=None):
    """Write a machine-readable manifest plus a short Chinese report."""
    plan.out_dir.mkdir(parents=True, exist_ok=True)
    manifest = {
        "mode": plan.mode,
        "status": status,
        "integration_boundary": "candidate_not_default_geoeast_or_cpp",
        "requires_manual_review": True,
        "dat_path": str(plan.dat_path),
        "core_exe": str(plan.core_exe),
        "input_format": plan.input_format,
        "formal_baseline_csv": str(plan.formal_baseline_csv),
        "formal_diagnostics": str(plan.formal_baseline_csv) + ".diagnostics.json",
        "formal_branches": str(plan.formal_baseline_csv) + ".branches.csv",
        "formal_topology": str(plan.formal_baseline_csv) + ".topology.csv",
        "candidate_output_dir": str(plan.candidate_output_dir),
        "final_candidate_csv": str(plan.final_candidate_csv),
        "promotion_summary": str(plan.promotion_summary),
        "pure_figure_overview": str(plan.pure_figure_overview),
        "pipeline_summary": str(pipeline_summary_path) if pipeline_summary_path else "",
        "formal_returncode": formal_returncode,
        "formal_command": _command_record(plan.formal_command),
        "pipeline_step_count": len(plan.pipeline_steps),
        "pipeline_commands": [
            {"name": step.name, "command": _command_record(step.command)}
            for step in plan.pipeline_steps
        ],
        "notes": (
            "GeoEast 与 C++ 正式默认输出仍以 formal_baseline_csv 为准；"
            "final_candidate_csv 是经过门控的冲顶候选结果，必须在覆盖率、拓扑、全图分块视觉复核和实际写入显示都通过后，"
            "才允许作为新的正式默认流程。"
        ),
    }
    plan.manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")

    report_path = plan.out_dir / "冲顶候选集成入口说明.md"
    lines = [
        "# 冲顶候选集成入口说明",
        "",
        "本入口用于把正式执行程序输出和冲顶候选后处理流水线串成一条可复跑、可审计、可回退的验证链路。",
        "它不会覆盖正式 C++ 输出，也不会把候选结果悄悄写成 GeoEast 默认结果。",
        "",
        "## 输出边界",
        "",
        f"- 正式基线 CSV：`{plan.formal_baseline_csv}`",
        f"- 冲顶候选 CSV：`{plan.final_candidate_csv}`",
        f"- 晋级门控摘要：`{plan.promotion_summary}`",
        f"- 清单文件：`{plan.manifest_path}`",
        "",
        "## 状态",
        "",
        f"- 当前状态：`{status}`",
        f"- 正式执行程序返回码：`{formal_returncode}`",
        "",
        "## 使用口径",
        "",
        "候选结果只能作为受控优化线进行复核。若要进入独立软件或 GeoEast 默认流程，需要再完成等价实现、官方数据复验、写入显示验证和人工分块复核。",
    ]
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return plan.manifest_path


def _print_plan(plan):
    print("== formal_cpp_baseline ==")
    print(" ".join(_command_record(plan.formal_command)))
    for index, step in enumerate(plan.pipeline_steps, start=1):
        print(f"== pipeline {index}/{len(plan.pipeline_steps)} {step.name} ==")
        print(" ".join(_command_record(step.command)))


def run_integration(plan, dry_run=False, skip_core=False):
    """Run the integration plan and return the manifest path."""
    plan.formal_baseline_dir.mkdir(parents=True, exist_ok=True)
    plan.candidate_output_dir.mkdir(parents=True, exist_ok=True)
    _print_plan(plan)

    if dry_run:
        return write_manifest(plan, status="dry_run")

    if not plan.dat_path.exists():
        raise FileNotFoundError(f"Input DAT not found: {plan.dat_path}")

    formal_returncode = None
    if skip_core:
        if not plan.formal_baseline_csv.exists():
            raise FileNotFoundError(f"Baseline CSV not found for --skip-core: {plan.formal_baseline_csv}")
    else:
        if not plan.core_exe.exists():
            raise FileNotFoundError(f"Core executable not found: {plan.core_exe}")
        completed = subprocess.run(plan.formal_command, check=False)
        formal_returncode = int(completed.returncode)
        if completed.returncode != 0:
            write_manifest(plan, status="formal_failed", formal_returncode=formal_returncode)
            raise RuntimeError("Formal C++ baseline command failed.")

    records = run_steps(plan.pipeline_steps, dry_run=False)
    summary_path, _report_path = write_pipeline_report(plan.candidate_output_dir, records, plan.pipeline_steps)
    return write_manifest(
        plan,
        status="passed",
        formal_returncode=formal_returncode,
        pipeline_summary_path=summary_path,
    )


def main():
    parser = argparse.ArgumentParser(description="运行正式基线加冲顶候选后处理的受控集成入口")
    parser.add_argument("--dat", required=True, help="GeoEast DAT 或官方 test.dat")
    parser.add_argument("--out-dir", required=True, help="集成入口输出目录")
    parser.add_argument("--core-exe", default=str(REPO_ROOT / "build" / "faultpolytrace.exe"))
    parser.add_argument("--baseline-csv", default="", help="可选：已有正式基线 CSV；不填则写入输出目录")
    parser.add_argument("--input-format", default="geoeast-dat")
    parser.add_argument("--python", default=sys.executable)
    parser.add_argument("--max-polygon-delta", type=int, default=25)
    parser.add_argument("--rows", type=int, default=4)
    parser.add_argument("--cols", type=int, default=6)
    parser.add_argument("--skip-core", action="store_true", help="跳过正式 C++ 执行，复用 --baseline-csv 或默认基线 CSV")
    parser.add_argument("--dry-run", action="store_true", help="只写清单并打印命令，不执行算法")
    args = parser.parse_args()

    plan = build_integration_plan(
        dat_path=args.dat,
        out_dir=args.out_dir,
        core_exe=args.core_exe,
        baseline_csv=args.baseline_csv or None,
        input_format=args.input_format,
        python_executable=args.python,
        max_polygon_delta=args.max_polygon_delta,
        rows=args.rows,
        cols=args.cols,
    )
    manifest_path = run_integration(plan, dry_run=args.dry_run, skip_core=args.skip_core)
    print(manifest_path)


if __name__ == "__main__":
    main()
