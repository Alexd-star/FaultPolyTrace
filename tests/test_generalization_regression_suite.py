# -*- coding: utf-8 -*-
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from 泛化能力回归验证 import run_synthetic_generalization_suite


def test_synthetic_generalization_suite_all_core_cases_pass(tmp_path):
    report = run_synthetic_generalization_suite(tmp_path)

    assert report["pass"] is True
    assert report["case_count"] >= 25
    assert report["failed_cases"] == []
    assert report["generalization_level"] == "synthetic_multi_scenario_gate"
    assert report["dataset_specific_ids"] == []
    assert set(report["families"]) >= {
        "slender",
        "local_pouch",
        "long_wide_band",
        "branch_junction",
        "branch_split",
        "fallback_guard",
    }

    contract = report["geoeast_reuse_contract"]
    assert contract["reusable"] is True
    assert contract["integration_stage"] == "quality_gate_and_postprocess_experiment"
    assert contract["uses_dataset_specific_polygon_ids"] is False
    assert contract["requires_runtime_object_key"] is True
    assert contract["runtime_object_key"] == "polygon_id"
    assert contract["cpp_geoeast_parity_status"] == "not_verified_by_this_suite"
    assert "result_csv" in contract["inputs"]
    assert "summary_json" in contract["outputs"]
    assert "geoeast_runner_cpp_parity_verified" not in contract["promotion_gate"]

    cases = {case["case_id"]: case for case in report["cases"]}
    assert cases["slender_no_false_positive"]["pass"] is True
    assert cases["local_pouch_detected"]["observed_class"] == "局部鼓包"
    assert cases["branch_junction_not_pouch"]["observed_class"] == "分支交切复核"
    assert cases["two_supported_branches_split"]["decision_status"] == "accepted_precheck"
    assert cases["single_branch_rejected"]["decision_status"] == "rejected_not_enough_components"

    assert (tmp_path / "泛化能力回归验证摘要.json").exists()
    assert (tmp_path / "泛化能力回归验证报告.md").exists()
