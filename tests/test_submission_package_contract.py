from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8-sig")


def test_submission_source_uses_git_archive_only() -> None:
    script = read("scripts/构建提交代码包.ps1")
    assert "git -C $repoRoot archive" in script
    assert "Copy-Tree -Source $repoRoot" not in script
    assert "源码包异常膨胀" in script
    for forbidden in ("outputs", "build", "release", ".git", ".pytest_tmp"):
        assert f'"{forbidden}"' in script


def test_submission_requires_clean_matching_build() -> None:
    script = read("scripts/构建提交代码包.ps1")
    assert "status --porcelain --untracked-files=all" in script
    assert "$manifestData.gitHead -ne $gitHead" in script
    assert "$manifestData.gitDirty" in script


def test_submission_keeps_desktop_and_plugin_packages_decoupled() -> None:
    script = read("scripts/构建提交代码包.ps1")
    assert 'Join-Path $releaseDir "geoeast_plugin"' in script
    assert 'Join-Path $releaseDir "qt_launcher"' in script
    assert 'Join-Path $pluginRoot "Launcher"' in script
    assert 'Join-Path $pluginRoot "BO_Runner"' in script
    assert 'Join-Path $pluginRoot "Mainframe_Plugin"' in script


def test_submission_hash_verifier_parses_generated_format() -> None:
    builder = read("scripts/构建提交代码包.ps1")
    verifier = read("packaging/submission/04_复现与验证/核验提交包哈希.ps1")
    assert '$lines.Add("$hash *$relative")' in builder
    assert "^([0-9A-Fa-f]{64}) [ *](.+)$" in verifier
    assert "INVALID $line" in verifier


def test_submission_templates_have_no_frozen_old_build_identity() -> None:
    template_root = ROOT / "packaging" / "submission"
    text = "\n".join(
        path.read_text(encoding="utf-8-sig")
        for path in template_root.rglob("*")
        if path.is_file() and path.suffix.lower() in {".md", ".ps1", ".json"}
    )
    assert "6069b21083eb17a13a8bf3e14d394d4082ae2157" not in text
    assert "21ED2ED2FD5E8F3C" not in text
    assert "FaultPolyTrace_Portable_v1.0.1" not in text
    assert "{{BUILD_ID}}" in text
    assert "{{GIT_HEAD}}" in text


def test_unique_manifest_uses_portable_paths() -> None:
    script = read("scripts/GenerateBuildManifest.ps1")
    assert "portablePath" in script
    assert "path = $item.portablePath" in script
    assert "path = $file.FullName" not in script
