# -*- coding: utf-8 -*-
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from 冲顶候选后处理流水线 import build_pipeline_steps


def test_peak_candidate_pipeline_is_reproducible_and_path_driven(tmp_path):
    dat = tmp_path / "input.dat"
    baseline = tmp_path / "baseline.csv"
    out_dir = tmp_path / "peak_pipeline"

    steps = build_pipeline_steps(
        dat_path=dat,
        baseline_csv=baseline,
        out_dir=out_dir,
        python_executable="python",
        max_polygon_delta=25,
    )

    names = [step.name for step in steps]
    assert names == [
        "baseline_shape_diagnosis",
        "first_local_candidate_repair",
        "shape_after_first_repair",
        "branch_attribution_split",
        "shape_after_branch_split",
        "second_local_candidate_repair",
        "shape_after_second_repair",
        "final_candidate_box_refine",
        "promotion_gate",
        "pure_fault_polygon_figures",
    ]

    flattened = " ".join(" ".join(map(str, step.command)) for step in steps)
    assert str(dat) in flattened
    assert str(baseline) in flattened
    assert str(out_dir) in flattened
    assert "D:\\BaiduNetdiskDownload" not in flattened
    assert "acceptance_20260607_173017_official_root_antimerge" not in flattened

    final_csv = out_dir / "08_final_candidate_box_refine" / "local_candidate_repair_experiment.csv"
    promotion = out_dir / "09_promotion_gate"
    figures = out_dir / "10_pure_fault_polygon_view"
    assert str(final_csv) in " ".join(map(str, steps[8].command))
    assert str(promotion) in " ".join(map(str, steps[8].command))
    assert str(final_csv) in " ".join(map(str, steps[9].command))
    assert str(figures) in " ".join(map(str, steps[9].command))


def test_peak_candidate_pipeline_keeps_second_pass_limited_to_bulge_candidates(tmp_path):
    steps = build_pipeline_steps(
        dat_path=tmp_path / "input.dat",
        baseline_csv=tmp_path / "baseline.csv",
        out_dir=tmp_path / "peak_pipeline",
        python_executable="python",
    )

    second_repair = next(step for step in steps if step.name == "second_local_candidate_repair")
    assert "--classes" in second_repair.command
    classes = second_repair.command[second_repair.command.index("--classes") + 1]
    assert classes == "局部鼓包"
    assert "--support-buffer-cells" in second_repair.command
    buffer_cells = second_repair.command[second_repair.command.index("--support-buffer-cells") + 1]
    assert buffer_cells == "0.75"


def test_peak_candidate_pipeline_uses_promotion_gate_shape_diagnosis_settings(tmp_path):
    steps = build_pipeline_steps(
        dat_path=tmp_path / "input.dat",
        baseline_csv=tmp_path / "baseline.csv",
        out_dir=tmp_path / "peak_pipeline",
        python_executable="python",
    )

    shape_steps = [step for step in steps if "shape" in step.name and "diagnosis" in step.name or step.name.startswith("shape_after")]
    assert shape_steps
    for step in shape_steps:
        assert "--profile" in step.command
        assert step.command[step.command.index("--profile") + 1] == "repair"
        assert "--width-ratio" in step.command
        assert step.command[step.command.index("--width-ratio") + 1] == "2.35"
        assert "--window" in step.command
        assert step.command[step.command.index("--window") + 1] == "11"
        assert "--min-extra-cells" in step.command
        assert step.command[step.command.index("--min-extra-cells") + 1] == "24"
        assert "--max-figures" in step.command
        assert step.command[step.command.index("--max-figures") + 1] == "40"
