# -*- coding: utf-8 -*-
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from 局部修形拓扑核验 import check_csv_topology


def _write_csv(path, wkts):
    with open(path, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["id", "status", "wkt"])
        writer.writeheader()
        for index, wkt in enumerate(wkts, start=1):
            writer.writerow({"id": index, "status": "kept", "wkt": wkt})


def test_check_csv_topology_counts_invalid_wkt(tmp_path):
    csv_path = tmp_path / "bad.csv"
    _write_csv(csv_path, ["POLYGON((0 0, 10 0, 10 10, 0 10))"])

    summary, details = check_csv_topology(csv_path)

    assert summary["kept_polygon_count"] == 1
    assert summary["parse_error_count"] == 1
    assert summary["invalid_geometry_count"] == 0
    assert details[0]["issue"] == "parse_error"


def test_check_csv_topology_counts_area_overlap(tmp_path):
    csv_path = tmp_path / "overlap.csv"
    _write_csv(
        csv_path,
        [
            "POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))",
            "POLYGON((5 5, 15 5, 15 15, 5 15, 5 5))",
        ],
    )

    summary, details = check_csv_topology(csv_path, area_tolerance=1e-9)

    assert summary["kept_polygon_count"] == 2
    assert summary["parse_error_count"] == 0
    assert summary["invalid_geometry_count"] == 0
    assert summary["polygon_intersection_count"] == 1
    assert any(detail["issue"] == "polygon_area_intersection" for detail in details)
