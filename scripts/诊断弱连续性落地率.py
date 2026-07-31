#!/usr/bin/env python3
"""Measure whether accepted weak paths survive into final polygon objects."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

import numpy as np

from 局部形态质量诊断 import read_geoeast_dat, read_polygons
from wkt_geometry import rasterize_polygon


def read_candidates(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return [row for row in csv.DictReader(stream) if row.get("accepted") == "1"]


def parse_path(value: str) -> list[tuple[int, int]]:
    points: list[tuple[int, int]] = []
    for item in (value or "").split(";"):
        if not item:
            continue
        x_text, y_text = item.split(":", 1)
        points.append((int(x_text), int(y_text)))
    return points


def polygon_object_map(
    polygons: list[dict], xs: np.ndarray, ys: np.ndarray, dx: float, dy: float
) -> np.ndarray:
    object_map = np.zeros((len(ys), len(xs)), dtype=np.int32)
    for polygon in polygons:
        mask = rasterize_polygon(
            polygon["coords"],
            len(xs),
            len(ys),
            xs,
            ys,
            dx,
            dy,
            polygon.get("holes", []),
        )
        object_map[mask] = int(polygon["id"])
    return object_map


def object_id_at(object_map: np.ndarray, x: int, y: int) -> int:
    if x < 0 or y < 0 or y >= object_map.shape[0] or x >= object_map.shape[1]:
        return 0
    return int(object_map[y, x])


def nearest_object_id(
    object_map: np.ndarray, x: int, y: int, max_radius: int = 3
) -> int:
    """Resolve endpoints that fall on a rasterized polygon boundary."""
    direct = object_id_at(object_map, x, y)
    if direct > 0:
        return direct
    for radius in range(1, max_radius + 1):
        matches: list[tuple[int, int]] = []
        for oy in range(-radius, radius + 1):
            for ox in range(-radius, radius + 1):
                if max(abs(ox), abs(oy)) != radius:
                    continue
                object_id = object_id_at(object_map, x + ox, y + oy)
                if object_id > 0:
                    matches.append((ox * ox + oy * oy, object_id))
        if matches:
            matches.sort()
            return matches[0][1]
    return 0


def overlapping_objects(
    source_map: np.ndarray, target_map: np.ndarray, target_id: int
) -> list[tuple[int, int]]:
    if target_id <= 0:
        return []
    ids, counts = np.unique(source_map[target_map == target_id], return_counts=True)
    pairs = [
        (int(object_id), int(count))
        for object_id, count in zip(ids, counts)
        if int(object_id) > 0
    ]
    return sorted(pairs, key=lambda item: (-item[1], item[0]))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dat", required=True)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--candidates", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    _, xs, ys, dx, dy, _ = read_geoeast_dat(args.dat)
    baseline_polygons = read_polygons(args.baseline)
    candidate_polygons = read_polygons(args.candidate)
    baseline_map = polygon_object_map(baseline_polygons, xs, ys, dx, dy)
    candidate_map = polygon_object_map(candidate_polygons, xs, ys, dx, dy)
    candidate_holes = {
        int(polygon["id"]): len(polygon.get("holes", []))
        for polygon in candidate_polygons
    }
    accepted = read_candidates(Path(args.candidates))

    rows: list[dict[str, object]] = []
    for candidate in accepted:
        ax = int(candidate.get("endpoint_a_grid_x", candidate.get("endpoint_a_x", 0)))
        ay = int(candidate.get("endpoint_a_grid_y", candidate.get("endpoint_a_y", 0)))
        bx = int(candidate.get("endpoint_b_grid_x", candidate.get("endpoint_b_x", 0)))
        by = int(candidate.get("endpoint_b_grid_y", candidate.get("endpoint_b_y", 0)))
        path = parse_path(candidate.get("path_grid_xy", ""))
        baseline_a = nearest_object_id(baseline_map, ax, ay)
        baseline_b = nearest_object_id(baseline_map, bx, by)
        final_a = nearest_object_id(candidate_map, ax, ay)
        final_b = nearest_object_id(candidate_map, bx, by)
        covered = sum(object_id_at(candidate_map, x, y) > 0 for x, y in path)
        path_ratio = covered / len(path) if path else 0.0
        baseline_same = baseline_a > 0 and baseline_a == baseline_b
        final_same = final_a > 0 and final_a == final_b
        overlaps = overlapping_objects(baseline_map, candidate_map, final_a)
        overlap_ids = [object_id for object_id, _ in overlaps]
        endpoint_ids = {object_id for object_id in (baseline_a, baseline_b) if object_id > 0}
        additional_ids = [object_id for object_id in overlap_ids if object_id not in endpoint_ids]
        if baseline_same:
            outcome = "baseline_already_same_object"
        elif final_same and path_ratio >= 0.80:
            outcome = "landed_and_merged"
        elif final_same:
            outcome = "same_object_but_path_partially_clipped"
        elif final_a == 0 or final_b == 0:
            outcome = "endpoint_not_covered"
        else:
            outcome = "accepted_but_not_merged"
        rows.append(
            {
                "candidate_id": int(candidate["candidate_id"]),
                "baseline_object_a": baseline_a,
                "baseline_object_b": baseline_b,
                "baseline_same_object": int(baseline_same),
                "candidate_object_a": final_a,
                "candidate_object_b": final_b,
                "candidate_same_object": int(final_same),
                "overlapping_baseline_objects": ";".join(
                    f"{object_id}:{cells}" for object_id, cells in overlaps
                ),
                "overlapping_baseline_object_count": len(overlaps),
                "additional_baseline_objects": ";".join(map(str, additional_ids)),
                "additional_baseline_object_count": len(additional_ids),
                "attribution_review_required": int(len(additional_ids) > 0),
                "candidate_internal_ring_count": candidate_holes.get(final_a, 0),
                "path_cells": len(path),
                "covered_path_cells": covered,
                "path_coverage_ratio": path_ratio,
                "outcome": outcome,
            }
        )

    csv_path = output_dir / "弱连续性终态落地明细.csv"
    with csv_path.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]) if rows else [])
        writer.writeheader()
        writer.writerows(rows)

    counts: dict[str, int] = {}
    for row in rows:
        outcome = str(row["outcome"])
        counts[outcome] = counts.get(outcome, 0) + 1
    summary = {
        "acceptedConnections": len(rows),
        "outcomeCounts": counts,
        "meanPathCoverageRatio": (
            float(np.mean([float(row["path_coverage_ratio"]) for row in rows]))
            if rows
            else 0.0
        ),
        "fullyCoveredConnections": sum(
            float(row["path_coverage_ratio"]) >= 0.999 for row in rows
        ),
        "connectionsRequiringAttributionReview": sum(
            int(row["attribution_review_required"]) for row in rows
        ),
        "connectionsWithInternalRings": sum(
            int(row["candidate_internal_ring_count"]) > 0 for row in rows
        ),
        "baselineObjectCount": int(np.unique(baseline_map[baseline_map > 0]).size),
        "candidateObjectCount": int(np.unique(candidate_map[candidate_map > 0]).size),
    }
    (output_dir / "弱连续性终态落地摘要.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(summary, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
