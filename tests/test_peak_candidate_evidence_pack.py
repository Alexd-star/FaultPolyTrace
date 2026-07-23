# -*- coding: utf-8 -*-
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from 生成冲顶候选证据包 import build_evidence_pack, load_integration_context  # noqa: E402


def _write(path, text="x"):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def test_peak_candidate_evidence_pack_summarizes_metrics_and_boundary(tmp_path):
    integration = tmp_path / "integration"
    candidate_csv = integration / "01_peak_candidate_pipeline" / "08_final_candidate_box_refine" / "local_candidate_repair_experiment.csv"
    promotion = integration / "01_peak_candidate_pipeline" / "09_promotion_gate" / "冲顶实验晋级验收摘要.json"
    overview = integration / "01_peak_candidate_pipeline" / "10_pure_fault_polygon_view" / "attribute_overlay" / "01_fault_polygons_overview_no_boxes.png"
    contact_sheet = integration / "01_peak_candidate_pipeline" / "10_pure_fault_polygon_view" / "attribute_overlay" / "02_fault_polygons_tiles_contact_sheet_no_boxes.png"

    _write(candidate_csv, "polygon_id,kept\n1,1\n")
    _write(overview, "png")
    _write(contact_sheet, "png")
    promotion.parent.mkdir(parents=True, exist_ok=True)
    promotion.write_text(
        json.dumps(
            {
                "baseline": {"kept_polygons": 214, "strong_coverage": 0.998, "weak_coverage": 0.971},
                "candidate": {
                    "kept_polygons": 230,
                    "strong_coverage": 0.998,
                    "weak_coverage": 0.971,
                    "open_rings": 0,
                    "self_intersections": 0,
                    "polygon_intersections": 0,
                    "local_shape_candidates": 5,
                    "high_priority_local_shape_candidates": 0,
                },
                "decision": {"pass": True, "summary": {"kept_polygon_delta": 16}},
            },
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )
    (integration / "冲顶候选集成入口清单.json").write_text(
        json.dumps(
            {
                "mode": "controlled_postprocess",
                "integration_boundary": "candidate_not_default_geoeast_or_cpp",
                "formal_baseline_csv": "formal.csv",
                "final_candidate_csv": str(candidate_csv),
                "promotion_summary": str(promotion),
                "pure_figure_overview": str(overview),
            },
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )

    context = load_integration_context(integration)
    out_dir = tmp_path / "pack"
    summary = build_evidence_pack(context, out_dir)

    assert summary["decision_pass"] is True
    assert summary["candidate"]["kept_polygons"] == 230
    assert summary["integration_boundary"] == "candidate_not_default_geoeast_or_cpp"
    assert (out_dir / "冲顶候选证据索引.md").exists()
    assert (out_dir / "关键指标摘要.json").exists()
    assert (out_dir / "01_候选结果" / "local_candidate_repair_experiment.csv").exists()
    assert (out_dir / "02_晋级门控" / "冲顶实验晋级验收摘要.json").exists()
    assert (out_dir / "03_无框图件" / "01_fault_polygons_overview_no_boxes.png").exists()
    assert (out_dir / "03_无框图件" / "02_fault_polygons_tiles_contact_sheet_no_boxes.png").exists()

    index_text = (out_dir / "冲顶候选证据索引.md").read_text(encoding="utf-8")
    assert "候选线不是 GeoEast 或 C++ 默认正式输出" in index_text
    assert "230" in index_text
    assert "5" in index_text


def test_release_package_includes_peak_candidate_evidence_pack_script():
    package_script = (ROOT / "scripts" / "打包发布包.ps1").read_text(encoding="utf-8-sig")
    assert "生成冲顶候选证据包.py" in package_script
