from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_inno_setup_installs_verified_package_and_supports_uninstall():
    iss = (ROOT / "packaging/windows/FaultPolyTrace.iss").read_text(encoding="utf-8")
    assert "{#PackageSource}\\*" in iss
    assert "Uninstallable=yes" in iss
    assert "ArchitecturesInstallIn64BitMode=x64compatible" in iss
    assert "{autoprograms}\\FaultPolyTrace" in iss
    assert "{autodesktop}\\FaultPolyTrace" in iss
    assert "FaultPolyTrace.exe" in iss
    assert "docs\\用户操作手册.md" in iss
    assert "FaultPolyTrace中文操作文档_V1.0.pdf" not in iss
    assert 'Excludes: "src\\*,standalone_app\\*,scripts\\*' in iss
    assert "examples\\outputs\\official\\perf_*" in iss
    assert "examples\\outputs\\official\\test_*" in iss
    assert "examples\\outputs\\standalone_acceptance_*" in iss


def test_installer_build_script_has_no_developer_specific_source_path():
    script = (ROOT / "scripts/构建Windows安装器.ps1").read_text(encoding="utf-8")
    assert "Get-Command iscc" in script
    assert "PackageDir" in script
    assert "Get-FileHash -Algorithm SHA256" in script
    assert "buildId" in script
    assert "packageContentSha256" in script
    assert "[System.IO.Path]::IsPathRooted($OutputDir)" in script
    assert "$OutputDir = (Resolve-Path -LiteralPath $OutputDir).Path" in script
    assert "C:\\Users\\" not in script


def test_installer_smoke_test_covers_install_run_identity_and_uninstall():
    script = (ROOT / "scripts/TestWindowsInstaller.ps1").read_text(encoding="utf-8-sig")
    for required in (
        "/VERYSILENT",
        "installedIdentity.buildId",
        "keptPolygonCount -ne 200",
        "topology.openRings",
        "ExpectedCsvSha256",
        "unins000.exe",
    ):
        assert required in script


def test_standalone_gui_acceptance_waits_for_the_complete_output_set():
    script = (ROOT / "scripts/验收独立软件发布包.ps1").read_text(encoding="utf-8-sig")
    assert "$expectedOutputs = @($csv, $geojson, $diagnostics, $branches, $topology)" in script
    assert "$ready = @($expectedOutputs | Where-Object" in script
    assert "Assert-OutputFile -Path $geojson" in script
