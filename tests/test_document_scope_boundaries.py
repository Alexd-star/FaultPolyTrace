# -*- coding: utf-8 -*-
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _read(relative_path):
    return (ROOT / relative_path).read_text(encoding="utf-8")


def test_core_documents_focus_on_contest_scope():
    algorithm_doc = _read("docs/08_算法说明.md")
    architecture_doc = _read("docs/05_项目架构说明.md")
    delivery_doc = _read("docs/17_项目交付说明.md")

    required_current_scope = "平面断层属性网格"
    forbidden_terms = [
        "三" + "维",
        "fault " + "surface",
        "fault " + "surfaces",
        "fault " + "seal",
        "horizon " + "intersection",
        "层位" + "求交",
        "断层" + "封堵",
        "储层" + "模拟",
        "数值" + "模拟",
        "远" + "期",
        "路" + "线图",
    ]

    for text in [algorithm_doc, architecture_doc, delivery_doc]:
        assert required_current_scope in text
        for term in forbidden_terms:
            assert term not in text

    assert "赛题要求" in delivery_doc
    assert "闭合断层多边形" in algorithm_doc
    assert "GeoEast" in architecture_doc


def test_operation_documents_match_current_release_identity():
    operation_guide = _read("docs/用户操作手册.md")
    enterprise_guide = _read("docs/企业级用户操作手册与产品说明书.md")

    assert "Polygons: 212 total, 200 kept" in operation_guide
    assert "Polygons: 232 total, 214 kept" not in operation_guide
    assert "防黏连增强 / 正式处理" in operation_guide
    assert "文档版本：V1.1.3" in enterprise_guide
    assert "产品版本：FaultPolyTrace V1.1.3" in enterprise_guide
