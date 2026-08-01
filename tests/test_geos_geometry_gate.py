from __future__ import annotations

import csv
import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "validate_geometry_geos.py"


def write_csv(path: Path, wkts: list[str]) -> None:
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["id", "status", "area", "perimeter", "cell_count", "mean_value", "max_value", "wkt"])
        for index, geometry_wkt in enumerate(wkts, start=1):
            writer.writerow([index, "kept", 1, 4, 1, 1, 1, geometry_wkt])


def run_gate(path: Path, report: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--input",
            str(path),
            "--report",
            str(report),
            "--expected-count",
            "2",
        ],
        text=True,
        capture_output=True,
        check=False,
    )


def test_geos_gate_accepts_disjoint_valid_polygons(tmp_path: Path) -> None:
    source = tmp_path / "valid.csv"
    report = tmp_path / "valid.json"
    write_csv(
        source,
        [
            "POLYGON((0 0,2 0,2 2,0 2,0 0))",
            "POLYGON((3 0,5 0,5 2,3 2,3 0))",
        ],
    )
    result = run_gate(source, report)
    assert result.returncode == 0, result.stderr + result.stdout
    assert json.loads(report.read_text(encoding="utf-8"))["status"] == "PASS"


def test_geos_gate_rejects_contained_polygon_set(tmp_path: Path) -> None:
    source = tmp_path / "contained.csv"
    report = tmp_path / "contained.json"
    write_csv(
        source,
        [
            "POLYGON((0 0,10 0,10 10,0 10,0 0))",
            "POLYGON((2 2,4 2,4 4,2 4,2 2))",
        ],
    )
    result = run_gate(source, report)
    assert result.returncode == 2
    payload = json.loads(report.read_text(encoding="utf-8"))
    assert payload["status"] == "FAIL"
    assert not payload["multiPolygonValid"]
