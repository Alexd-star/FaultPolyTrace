import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def _text(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def _source_paths(text: str) -> set[str]:
    return {
        match.replace("\\", "/")
        for match in re.findall(
            r"src[\\/](?:app|core|io)[\\/][A-Za-z0-9_]+\.cpp",
            text,
        )
    }


def _core_project_paths(text: str) -> set[str]:
    return {
        match.replace("\\", "/")
        for match in re.findall(
            r"src[\\/](?:core|io)[\\/][A-Za-z0-9_]+\.(?:cpp|h)",
            text,
        )
    }


def test_cli_and_address_sanitizer_compile_the_same_sources():
    release_sources = _source_paths(_text("scripts/build.ps1"))
    sanitizer_sources = _source_paths(_text("scripts/run_msvc_asan_validation.ps1"))

    assert release_sources
    assert release_sources == sanitizer_sources


def test_geoeast_qmake_and_visual_studio_projects_list_the_same_core_files():
    qmake_paths = _core_project_paths(
        _text("geoeast_plugin/bo_runner/FaultPolyTraceGeoEast.pro")
    )
    visual_studio_paths = _core_project_paths(
        _text("geoeast_plugin/bo_runner/FaultPolyTraceGeoEast.vcxproj")
    )

    assert qmake_paths
    assert qmake_paths == visual_studio_paths


def test_visual_studio_filter_file_covers_every_project_source_and_header():
    project_paths = _core_project_paths(
        _text("geoeast_plugin/bo_runner/FaultPolyTraceGeoEast.vcxproj")
    )
    filter_paths = _core_project_paths(
        _text("geoeast_plugin/bo_runner/FaultPolyTraceGeoEast.vcxproj.filters")
    )

    assert project_paths
    assert project_paths == filter_paths
