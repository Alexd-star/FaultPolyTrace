# -*- coding: utf-8 -*-
import importlib.util
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))


def _load_module():
    path = ROOT / "scripts" / "运行复杂同域验证.py"
    spec = importlib.util.spec_from_file_location("complex_generalization", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_current_product_profile_does_not_override_released_defaults():
    module = _load_module()
    arguments = module._baseline_arguments("current_product")

    assert arguments == ("--input-format", "auto")
    assert "--no-local-anti-merge" not in arguments


def test_historical_profile_remains_reproducible_and_adaptive_mode_is_available():
    module = _load_module()
    historical = module._baseline_arguments("historical_214")

    assert "--no-local-anti-merge" in historical
    source = (ROOT / "scripts" / "运行复杂同域验证.py").read_text(encoding="utf-8")
    assert '"adaptive_boundary": ("--adaptive-boundary-apply",)' in source


def test_stability_summary_uses_worst_case_and_case_means():
    module = _load_module()
    summary = module._stability_summary({
        "stability": [
            {"minimum_iou_to_baseline": 0.80, "mean_iou_to_baseline": 0.90},
            {"minimum_iou_to_baseline": 0.70, "mean_iou_to_baseline": 0.86},
        ]
    })

    assert summary["case_count"] == 2
    assert summary["minimum_iou"] == 0.70
    assert summary["mean_minimum_iou"] == 0.75
    assert summary["mean_iou"] == 0.88
