# -*- coding: utf-8 -*-
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_peak_candidate_wrapper_is_customer_path_driven():
    script_path = ROOT / "scripts" / "运行冲顶候选集成入口.ps1"
    assert script_path.read_bytes().startswith(b"\xef\xbb\xbf")
    script = script_path.read_text(encoding="utf-8-sig")

    assert "冲顶候选集成入口.py" in script
    assert "FPT_OFFICIAL_DAT" in script
    assert "D:\\BaiduNetdiskDownload" not in script
    assert "C:\\Users\\" not in script
    assert "-BuildCore" in script
    assert "-SkipCore" in script
    assert "-DryRun" in script
    assert "candidate_not_default_geoeast_or_cpp" in script


def test_release_package_includes_peak_candidate_wrapper():
    package_script = (ROOT / "scripts" / "打包发布包.ps1").read_text(encoding="utf-8-sig")
    assert "运行冲顶候选集成入口.ps1" in package_script
