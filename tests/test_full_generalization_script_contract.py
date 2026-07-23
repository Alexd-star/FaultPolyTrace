# -*- coding: utf-8 -*-
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_full_generalization_script_is_portable_and_keeps_evidence_layers_separate():
    script = ROOT / "scripts" / "运行完整泛化验证.ps1"
    assert script.exists()
    text = script.read_text(encoding="utf-8-sig")
    assert "运行泛化验证.py" in text
    assert "planar_benchmark" in text
    assert "cracks_auxiliary" in text
    assert "DownloadCracks" in text
    assert "ThebeSeismic" in text
    assert "ThebeLabel" in text
    assert "转换真实地震为平面属性.py" in text
    assert "真实平面泛化已证明" not in text
    assert "C:\\Users\\26312" not in text
    assert "D:\\BaiduNetdiskDownload" not in text
