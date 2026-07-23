# -*- coding: utf-8 -*-
"""Traceable peak-quality post-processing pipeline.

This script promotes the previously manual contest-peak experiment chain into
one reproducible workflow. It does not overwrite the C++ baseline CSV. Every
stage writes to its own directory so the result can be audited or rolled back.
"""

import argparse
import json
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent


@dataclass(frozen=True)
class PipelineStep:
    name: str
    command: list
    output_dir: Path
    primary_output: Path


def _script(name):
    return str(SCRIPT_DIR / name)


def _shape_csv(stage_dir):
    return stage_dir / "局部形态质量诊断.csv"


def _local_repair_csv(stage_dir):
    return stage_dir / "local_candidate_repair_experiment.csv"


def _branch_split_csv(stage_dir):
    return stage_dir / "branch_attribution_split_experiment.csv"


def _shape_diagnosis_command(py, dat_path, csv_path, out_dir, profile="repair"):
    return [
        py,
        _script("局部形态质量诊断.py"),
        "--dat",
        str(dat_path),
        "--csv",
        str(csv_path),
        "--out-dir",
        str(out_dir),
        "--profile",
        str(profile),
        "--width-ratio",
        "2.35",
        "--min-extra-cells",
        "24",
        "--window",
        "11",
        "--max-figures",
        "40",
    ]


def build_pipeline_steps(
    dat_path,
    baseline_csv,
    out_dir,
    python_executable=None,
    max_polygon_delta=25,
    figure_rows=4,
    figure_cols=6,
):
    """Return the exact command plan without executing it.

    The plan is path-driven. No official-data path, historical acceptance
    directory, or polygon id is embedded here; callers provide all inputs.
    """
    py = python_executable or sys.executable
    dat_path = Path(dat_path)
    baseline_csv = Path(baseline_csv)
    out_dir = Path(out_dir)

    shape0 = out_dir / "00_baseline_shape_diagnosis"
    repair1 = out_dir / "01_first_local_candidate_repair"
    shape1 = out_dir / "02_shape_after_first_repair"
    branch = out_dir / "03_branch_attribution_split"
    shape2 = out_dir / "04_shape_after_branch_split"
    repair2 = out_dir / "05_second_local_candidate_repair"
    shape3 = out_dir / "06_shape_after_second_repair"
    refine = out_dir / "08_final_candidate_box_refine"
    promotion = out_dir / "09_promotion_gate"
    figures = out_dir / "10_pure_fault_polygon_view"

    repair1_csv = _local_repair_csv(repair1)
    branch_csv = _branch_split_csv(branch)
    repair2_csv = _local_repair_csv(repair2)
    final_csv = _local_repair_csv(refine)

    return [
        PipelineStep(
            "baseline_shape_diagnosis",
            _shape_diagnosis_command(py, dat_path, baseline_csv, shape0),
            shape0,
            _shape_csv(shape0),
        ),
        PipelineStep(
            "first_local_candidate_repair",
            [
                py,
                _script("局部候选修形实验.py"),
                "--dat",
                str(dat_path),
                "--csv",
                str(baseline_csv),
                "--shape-candidates",
                str(_shape_csv(shape0)),
                "--out-dir",
                str(repair1),
                "--support-buffer-cells",
                "1.0",
            ],
            repair1,
            repair1_csv,
        ),
        PipelineStep(
            "shape_after_first_repair",
            _shape_diagnosis_command(py, dat_path, repair1_csv, shape1),
            shape1,
            _shape_csv(shape1),
        ),
        PipelineStep(
            "branch_attribution_split",
            [
                py,
                _script("分支归属拆分实验.py"),
                "--dat",
                str(dat_path),
                "--csv",
                str(repair1_csv),
                "--shape-candidates",
                str(_shape_csv(shape1)),
                "--out-dir",
                str(branch),
            ],
            branch,
            branch_csv,
        ),
        PipelineStep(
            "shape_after_branch_split",
            _shape_diagnosis_command(py, dat_path, branch_csv, shape2),
            shape2,
            _shape_csv(shape2),
        ),
        PipelineStep(
            "second_local_candidate_repair",
            [
                py,
                _script("局部候选修形实验.py"),
                "--dat",
                str(dat_path),
                "--csv",
                str(branch_csv),
                "--shape-candidates",
                str(_shape_csv(shape2)),
                "--out-dir",
                str(repair2),
                "--classes",
                "局部鼓包",
                "--support-buffer-cells",
                "0.75",
            ],
            repair2,
            repair2_csv,
        ),
        PipelineStep(
            "shape_after_second_repair",
            _shape_diagnosis_command(py, dat_path, repair2_csv, shape3),
            shape3,
            _shape_csv(shape3),
        ),
        PipelineStep(
            "final_candidate_box_refine",
            [
                py,
                _script("局部候选修形实验.py"),
                "--dat",
                str(dat_path),
                "--csv",
                str(repair2_csv),
                "--shape-candidates",
                str(_shape_csv(shape3)),
                "--out-dir",
                str(refine),
                "--max-area-gain-ratio",
                "0.05",
                "--max-inner-hole-area-cells",
                "120",
                "--max-inner-hole-count",
                "3",
            ],
            refine,
            final_csv,
        ),
        PipelineStep(
            "promotion_gate",
            [
                py,
                _script("冲顶实验晋级验收.py"),
                "--dat",
                str(dat_path),
                "--baseline-csv",
                str(baseline_csv),
                "--candidate-csv",
                str(final_csv),
                "--out-dir",
                str(promotion),
                "--max-polygon-delta",
                str(int(max_polygon_delta)),
            ],
            promotion,
            promotion / "冲顶实验晋级验收摘要.json",
        ),
        PipelineStep(
            "pure_fault_polygon_figures",
            [
                py,
                _script("生成纯断层多边形图.py"),
                "--dat",
                str(dat_path),
                "--csv",
                str(final_csv),
                "--out-dir",
                str(figures),
                "--rows",
                str(int(figure_rows)),
                "--cols",
                str(int(figure_cols)),
            ],
            figures,
            figures / "attribute_overlay" / "01_fault_polygons_overview_no_boxes.png",
        ),
    ]


def run_steps(steps, dry_run=False):
    records = []
    for index, step in enumerate(steps, start=1):
        step.output_dir.mkdir(parents=True, exist_ok=True)
        record = {
            "index": index,
            "name": step.name,
            "command": [str(part) for part in step.command],
            "output_dir": str(step.output_dir),
            "primary_output": str(step.primary_output),
            "status": "dry_run" if dry_run else "pending",
        }
        print(f"== {index}/{len(steps)} {step.name} ==")
        print(" ".join(record["command"]))
        if not dry_run:
            completed = subprocess.run(step.command, check=False)
            record["returncode"] = int(completed.returncode)
            record["status"] = "passed" if completed.returncode == 0 else "failed"
            if completed.returncode != 0:
                records.append(record)
                raise RuntimeError(f"Pipeline step failed: {step.name}")
        records.append(record)
    return records


def write_pipeline_report(out_dir, records, steps):
    out_dir = Path(out_dir)
    summary = {
        "pipeline": "peak_candidate_postprocess",
        "step_count": len(records),
        "pass": all(record["status"] in {"passed", "dry_run"} for record in records),
        "steps": records,
        "final_candidate_csv": str(steps[7].primary_output),
        "promotion_summary": str(steps[8].primary_output),
        "pure_figure_overview": str(steps[9].primary_output),
    }
    summary_path = out_dir / "冲顶候选后处理流水线摘要.json"
    report_path = out_dir / "冲顶候选后处理流水线报告.md"
    summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# 冲顶候选后处理流水线报告",
        "",
        "本流水线用于把手工冲顶实验整理为可复跑、可审计、可回退的后处理流程。它不会覆盖正式 C++ 输出，也不写死官方数据路径或历史验收目录。",
        "",
        "## 阶段记录",
        "",
        "| 序号 | 阶段 | 状态 | 主要输出 |",
        "|---:|---|---|---|",
    ]
    for record in records:
        lines.append(
            f"| {record['index']} | {record['name']} | {record['status']} | `{record['primary_output']}` |"
        )
    lines.extend(
        [
            "",
            "## 关键结果",
            "",
            f"- 最终候选 CSV：`{summary['final_candidate_csv']}`",
            f"- 晋级验收摘要：`{summary['promotion_summary']}`",
            f"- 无框总览图：`{summary['pure_figure_overview']}`",
            "",
            "## 使用口径",
            "",
            "该结果属于质量门控后的冲顶候选线。只有在覆盖率、拓扑、局部形态诊断、全图分块视觉复核和实际集成显示均通过后，才能考虑进入正式默认流程。",
        ]
    )
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return summary_path, report_path


def main():
    parser = argparse.ArgumentParser(description="运行可回溯冲顶候选后处理流水线")
    parser.add_argument("--dat", required=True, help="GeoEast DAT 或官方 test.dat")
    parser.add_argument("--baseline-csv", required=True, help="正式执行程序输出 CSV")
    parser.add_argument("--out-dir", required=True, help="流水线输出目录")
    parser.add_argument("--max-polygon-delta", type=int, default=25)
    parser.add_argument("--rows", type=int, default=4)
    parser.add_argument("--cols", type=int, default=6)
    parser.add_argument("--python", default=sys.executable)
    parser.add_argument("--dry-run", action="store_true", help="只打印命令并写报告，不执行")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    steps = build_pipeline_steps(
        dat_path=args.dat,
        baseline_csv=args.baseline_csv,
        out_dir=out_dir,
        python_executable=args.python,
        max_polygon_delta=args.max_polygon_delta,
        figure_rows=args.rows,
        figure_cols=args.cols,
    )
    records = run_steps(steps, dry_run=args.dry_run)
    summary_path, report_path = write_pipeline_report(out_dir, records, steps)
    print(summary_path)
    print(report_path)


if __name__ == "__main__":
    main()
