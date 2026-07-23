# -*- coding: utf-8 -*-
import json
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from generalization_validation.runner import _evidence_opening


def test_public_data_list_cli_exposes_usage_boundaries():
    process = subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "管理公开泛化数据.py"), "list"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    assert process.returncode == 0, process.stderr
    assert "thebe" in process.stdout
    assert "平面验证角色" in process.stdout
    assert "边界" in process.stdout


def test_generate_cli_writes_balanced_manifest(tmp_path):
    output = tmp_path / "benchmark"
    process = subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts" / "运行泛化验证.py"),
            "generate",
            "--output", str(output),
            "--count", "8",
            "--width", "128",
            "--height", "96",
            "--seed", "17",
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    assert process.returncode == 0, process.stderr
    manifest = json.loads((output / "benchmark_manifest.json").read_text(encoding="utf-8"))
    assert manifest["count"] == 8
    assert len({case["family"] for case in manifest["cases"]}) == 8
    assert manifest["real_data_claim"] is False


def test_real_volume_converter_keeps_label_as_evaluation_only(tmp_path):
    seismic = np.zeros((20, 18, 17), dtype=np.float32)
    seismic[:10, :, :] = np.sin(np.linspace(0, 8, 17))
    seismic[10:, :, :] = np.cos(np.linspace(0, 8, 17))
    labels = np.zeros_like(seismic, dtype=np.uint8)
    labels[9:11, :, 8] = 1
    seismic_path = tmp_path / "seismic.npy"
    label_path = tmp_path / "labels.npy"
    np.save(seismic_path, seismic)
    np.save(label_path, labels)
    output = tmp_path / "converted"
    process = subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts" / "转换真实地震为平面属性.py"),
            "--seismic", str(seismic_path),
            "--label", str(label_path),
            "--sample-index", "8",
            "--output", str(output),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    assert process.returncode == 0, process.stderr
    manifest = json.loads((output / "conversion_manifest.json").read_text(encoding="utf-8"))
    assert manifest["label_used_as_input"] is False
    assert manifest["label_role"] == "evaluation_only"
    assert (output / "attribute.txt").exists()
    assert (output / "truth_masks.npz").exists()
    benchmark = json.loads((output / "benchmark_manifest.json").read_text(encoding="utf-8"))
    assert benchmark["benchmark_type"] == "real_planar_attribute_from_seismic_volume"
    assert benchmark["direct_planar_evidence"] is True
    assert benchmark["cases"][0]["path"] == "."


def test_report_opening_distinguishes_real_planar_from_controlled_truth():
    real = _evidence_opening({
        "benchmark_type": "real_planar_attribute_from_seismic_volume",
        "direct_planar_evidence": True,
    })
    controlled = _evidence_opening({
        "benchmark_type": "controlled_planar_fault_attribute",
        "direct_planar_evidence": True,
    })

    assert "真实地震体" in real
    assert "几何先生成" not in real
    assert "几何先生成" in controlled
