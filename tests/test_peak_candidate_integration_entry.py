# -*- coding: utf-8 -*-
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from 冲顶候选集成入口 import (  # noqa: E402
    build_formal_baseline_command,
    build_integration_plan,
    write_manifest,
)


def test_formal_baseline_command_is_path_driven_and_uses_official_quality_preset(tmp_path):
    dat = tmp_path / "input.dat"
    baseline = tmp_path / "out" / "official_root_antimerge.csv"
    exe = tmp_path / "faultpolytrace.exe"

    command = build_formal_baseline_command(
        core_exe=exe,
        dat_path=dat,
        baseline_csv=baseline,
        input_format="geoeast-dat",
    )

    flattened = " ".join(map(str, command))
    assert str(dat) in flattened
    assert str(baseline) in flattened
    assert "D:\\BaiduNetdiskDownload" not in flattened
    assert "acceptance_20260607_173017_official_root_antimerge" not in flattened

    assert command[0] == str(exe)
    assert command[command.index("--input") + 1] == str(dat)
    assert command[command.index("--input-format") + 1] == "geoeast-dat"
    assert command[command.index("--output") + 1] == str(baseline)
    assert command[command.index("--percentile") + 1] == "0.98"
    assert command[command.index("--min-area") + 1] == "120"
    assert "--root-anti-merge" in command
    assert "--no-separate-branch-belts" in command
    assert "--no-residual-trace" in command
    assert "--diagnostics" in command
    assert command[command.index("--diagnostics") + 1] == str(baseline) + ".diagnostics.json"
    assert "--branches-output" in command
    assert "--topology-output" in command


def test_integration_plan_keeps_formal_and_candidate_outputs_separate(tmp_path):
    plan = build_integration_plan(
        dat_path=tmp_path / "input.dat",
        out_dir=tmp_path / "integrated",
        core_exe=tmp_path / "faultpolytrace.exe",
        python_executable="python",
        max_polygon_delta=25,
        rows=4,
        cols=6,
    )

    assert plan.mode == "controlled_postprocess"
    assert plan.formal_baseline_csv == tmp_path / "integrated" / "00_formal_cpp_baseline" / "official_root_antimerge.csv"
    assert plan.candidate_output_dir == tmp_path / "integrated" / "01_peak_candidate_pipeline"
    assert plan.final_candidate_csv == (
        tmp_path
        / "integrated"
        / "01_peak_candidate_pipeline"
        / "08_final_candidate_box_refine"
        / "local_candidate_repair_experiment.csv"
    )
    assert plan.final_candidate_csv != plan.formal_baseline_csv
    assert plan.promotion_summary == (
        tmp_path / "integrated" / "01_peak_candidate_pipeline" / "09_promotion_gate" / "冲顶实验晋级验收摘要.json"
    )
    assert len(plan.pipeline_steps) == 10
    assert str(plan.formal_baseline_csv) in " ".join(map(str, plan.pipeline_steps[0].command))


def test_manifest_records_review_boundary_and_traceable_outputs(tmp_path):
    plan = build_integration_plan(
        dat_path=tmp_path / "input.dat",
        out_dir=tmp_path / "integrated",
        core_exe=tmp_path / "faultpolytrace.exe",
        python_executable="python",
    )

    manifest_path = write_manifest(
        plan,
        status="dry_run",
        formal_returncode=None,
        pipeline_summary_path=tmp_path / "integrated" / "01_peak_candidate_pipeline" / "冲顶候选后处理流水线摘要.json",
    )

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert manifest["mode"] == "controlled_postprocess"
    assert manifest["status"] == "dry_run"
    assert manifest["formal_baseline_csv"] == str(plan.formal_baseline_csv)
    assert manifest["final_candidate_csv"] == str(plan.final_candidate_csv)
    assert manifest["promotion_summary"] == str(plan.promotion_summary)
    assert manifest["integration_boundary"] == "candidate_not_default_geoeast_or_cpp"
    assert manifest["requires_manual_review"] is True
    assert "GeoEast" in manifest["notes"]
