#!/usr/bin/env python3
"""Cross-check a FaultPolyTrace polygon CSV with GEOS during release validation."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any

import shapely
from shapely import wkt
from shapely.geometry import MultiPolygon, Polygon
from shapely.validation import explain_validity


def load_polygons(path: Path, kept_only: bool) -> list[tuple[int, Polygon]]:
    polygons: list[tuple[int, Polygon]] = []
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        required = {"id", "status", "wkt"}
        missing = required.difference(reader.fieldnames or ())
        if missing:
            raise ValueError(f"missing CSV columns: {', '.join(sorted(missing))}")
        for row in reader:
            if kept_only and row["status"].strip().lower() != "kept":
                continue
            geometry = wkt.loads(row["wkt"])
            if geometry.geom_type != "Polygon":
                raise ValueError(
                    f"polygon {row['id']} has unsupported geometry type {geometry.geom_type}"
                )
            polygons.append((int(row["id"]), geometry))
    return polygons


def validate(polygons: list[tuple[int, Polygon]], expected_count: int | None) -> dict[str, Any]:
    invalid: list[dict[str, Any]] = []
    empty_ids: list[int] = []
    non_positive_area_ids: list[int] = []
    for polygon_id, geometry in polygons:
        if geometry.is_empty:
            empty_ids.append(polygon_id)
        if geometry.area <= 0.0:
            non_positive_area_ids.append(polygon_id)
        if not geometry.is_valid:
            invalid.append(
                {"polygonId": polygon_id, "reason": explain_validity(geometry)}
            )

    collection_valid = False
    collection_reason = "empty polygon collection"
    if polygons:
        collection = MultiPolygon([geometry for _, geometry in polygons])
        collection_valid = collection.is_valid
        collection_reason = explain_validity(collection)

    count_matches = expected_count is None or len(polygons) == expected_count
    passed = (
        count_matches
        and not invalid
        and not empty_ids
        and not non_positive_area_ids
        and collection_valid
    )
    return {
        "format": "FaultPolyTrace.GeosGeometryValidation",
        "schemaVersion": 1,
        "status": "PASS" if passed else "FAIL",
        "engine": {"shapely": shapely.__version__, "geos": shapely.geos_version_string},
        "polygonCount": len(polygons),
        "expectedPolygonCount": expected_count,
        "countMatches": count_matches,
        "invalidPolygonCount": len(invalid),
        "invalidPolygons": invalid,
        "emptyPolygonIds": empty_ids,
        "nonPositiveAreaPolygonIds": non_positive_area_ids,
        "multiPolygonValid": collection_valid,
        "multiPolygonValidityReason": collection_reason,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Use Shapely/GEOS to validate FaultPolyTrace polygon CSV output."
    )
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--expected-count", type=int)
    parser.add_argument("--all", action="store_true", help="validate filtered rows too")
    args = parser.parse_args()

    polygons = load_polygons(args.input, kept_only=not args.all)
    report = validate(polygons, args.expected_count)
    report["input"] = str(args.input.resolve())
    payload = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(payload, encoding="utf-8")
    print(payload, end="")
    return 0 if report["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
