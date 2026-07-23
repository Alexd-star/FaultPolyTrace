# -*- coding: utf-8 -*-
import sys
from pathlib import Path

from shapely.geometry import Polygon, box

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from 局部候选修形实验 import (
    fill_inner_rings_inside_candidate,
    geometry_changed_meaningfully,
    refine_polygon_by_candidate_class,
    repair_polygon_inside_candidate,
    view_bounds_for_geometries_and_candidates,
)


def test_repair_polygon_inside_candidate_removes_pouch_but_preserves_main_band():
    main_band = box(0, 0, 100, 8)
    pouch = box(45, 8, 62, 28)
    polygon = main_band.union(pouch)
    support = main_band.buffer(0.5)
    candidate_box = box(40, 4, 68, 30)

    repaired = repair_polygon_inside_candidate(polygon, candidate_box, support)

    assert repaired.is_valid
    assert repaired.area < polygon.area - 250
    assert repaired.intersection(main_band).area >= main_band.area * 0.98
    assert repaired.intersection(pouch).area < pouch.area * 0.20


def test_repair_polygon_inside_candidate_keeps_original_when_support_would_delete_band():
    polygon = box(0, 0, 100, 8)
    support = box(0, 0, 10, 8)
    candidate_box = box(0, 0, 100, 8)

    repaired = repair_polygon_inside_candidate(
        polygon,
        candidate_box,
        support,
        min_candidate_retention=0.80,
    )

    assert repaired.equals_exact(polygon, tolerance=1e-9)


def test_view_bounds_for_geometries_and_candidates_uses_candidate_coordinate_order():
    polygon = box(100, 200, 300, 400)
    candidate = {"bbox": (250, 500, 350, 700)}

    bounds = view_bounds_for_geometries_and_candidates([polygon], [candidate], min_pad=0, pad_ratio=0)

    assert bounds == (100.0, 500.0, 200.0, 700.0)


def test_fill_inner_rings_inside_candidate_removes_small_local_hole():
    polygon = Polygon(
        [(0, 0), (100, 0), (100, 20), (0, 20), (0, 0)],
        holes=[[(42, 6), (56, 6), (56, 14), (42, 14), (42, 6)]],
    )
    candidate_box = box(38, 2, 62, 18)

    repaired = fill_inner_rings_inside_candidate(
        polygon,
        candidate_box,
        max_hole_area=150.0,
        max_hole_count=3,
    )

    assert repaired.is_valid
    assert len(repaired.interiors) == 0
    assert repaired.area > polygon.area


def test_fill_inner_rings_inside_candidate_keeps_large_or_outside_holes():
    polygon = Polygon(
        [(0, 0), (100, 0), (100, 40), (0, 40), (0, 0)],
        holes=[
            [(10, 10), (40, 10), (40, 30), (10, 30), (10, 10)],
            [(72, 12), (82, 12), (82, 22), (72, 22), (72, 12)],
        ],
    )
    candidate_box = box(68, 8, 86, 26)

    repaired = fill_inner_rings_inside_candidate(
        polygon,
        candidate_box,
        max_hole_area=150.0,
        max_hole_count=3,
    )

    assert repaired.is_valid
    assert len(repaired.interiors) == 1
    assert repaired.area > polygon.area
    assert repaired.area < box(0, 0, 100, 40).area


def test_refine_polygon_by_candidate_class_uses_inner_ring_cleanup_only_for_ring_candidates():
    polygon = Polygon(
        [(0, 0), (100, 0), (100, 20), (0, 20), (0, 0)],
        holes=[[(42, 6), (56, 6), (56, 14), (42, 14), (42, 6)]],
    )
    candidate = {"bbox": (38, 62, 2, 18), "shape_class": "内环复核"}
    support = box(0, 0, 100, 20)

    repaired = refine_polygon_by_candidate_class(polygon, candidate, support)

    assert repaired.is_valid
    assert len(repaired.interiors) == 0


def test_geometry_changed_meaningfully_rejects_noop_geometry():
    polygon = box(0, 0, 10, 10)

    assert not geometry_changed_meaningfully(polygon, box(0, 0, 10, 10))
    assert geometry_changed_meaningfully(polygon, box(0, 0, 11, 10))
