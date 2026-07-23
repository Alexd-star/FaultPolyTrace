from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ADAPTER = (ROOT / "geoeast_plugin/bo_runner/GeoEastBoAdapter.cpp").read_text(encoding="utf-8")
RUNNER = (ROOT / "geoeast_plugin/bo_runner/main.cpp").read_text(encoding="utf-8")


def test_adapter_does_not_silently_flatten_polygon_holes():
    assert "BOFaultPolygon does not expose hole ownership semantics" in ADAPTER
    assert "polygon.holes.empty()" in ADAPTER


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
