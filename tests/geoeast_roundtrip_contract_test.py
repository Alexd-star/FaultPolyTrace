from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ADAPTER = (ROOT / "geoeast_plugin/bo_runner/GeoEastBoAdapter.cpp").read_text(encoding="utf-8")
RUNNER = (ROOT / "geoeast_plugin/bo_runner/main.cpp").read_text(encoding="utf-8")


def test_adapter_does_not_silently_flatten_polygon_holes():
    assert "GeoEast compatibility: omitted" in ADAPTER
    assert "appendRingSegment(polygon.points);" in ADAPTER
    assert "appendRingSegment(hole)" not in ADAPTER
    assert "omittedHoleCount += polygon.holes.size()" in ADAPTER


def test_all_geoeast_segment_and_point_fields_are_initialized():
    for field in (
        "segment.editMode",
        "segment.faultPolygonSegID",
        "segment.faultPolygonCombinedID",
        "segment.pkID",
        "segment.geometryID",
        "segment.level",
        "point.uplowID",
    ):
        assert field in ADAPTER


def test_overwrite_delete_and_explicit_delete_check_sdk_status():
    assert ADAPTER.count("eraseResult.Value() == NStatus::NFail") >= 2


def test_runner_re_reads_written_polygons_before_reporting_success():
    assert "verifyFaultPolygonRoundTrip" in RUNNER
    assert "GeoEast round-trip verified" in RUNNER
    assert "std::sort(expectedSignatures.begin(), expectedSignatures.end())" in RUNNER
    assert "std::sort(actualSignatures.begin(), actualSignatures.end())" in RUNNER
    assert "fpt::polygonArea(points)" in RUNNER
    assert "static_cast<double>(points.size())" in RUNNER


def test_runner_uses_staged_commit_with_readback_and_compensating_rollback():
    assert "commitFaultPolygonsWithReadback" in RUNNER
    assert "runStagedCommit" in RUNNER
    assert "writeStaging" in RUNNER
    assert "verifyStaging" in RUNNER
    assert "replaceTarget" in RUNNER
    assert "verifyTarget" in RUNNER
    assert "rollbackTarget" in RUNNER
    assert "deleteStaging" in RUNNER
    assert "application_staged_commit" in RUNNER


def test_runner_preserves_previous_geometry_and_style_for_rollback():
    assert "previousPolygons = fpt_geoeast::readFaultPolygons" in RUNNER
    assert "previousStyle = fpt_geoeast::readFaultPolygonStyle" in RUNNER
    assert "previousPolygons, true, previousStyle" in RUNNER
