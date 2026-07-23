# -*- coding: utf-8 -*-
import sys
from pathlib import Path

from shapely.geometry import box

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from 分支归属拆分实验 import split_polygon_by_support_components


def test_split_polygon_by_support_components_splits_two_supported_branches():
    branch_a = box(0, 0, 100, 8)
    branch_b = box(0, 30, 100, 38)
    bridge = box(45, 8, 55, 30)
    polygon = branch_a.union(branch_b).union(bridge)
    support = branch_a.union(branch_b)
    candidate = box(-5, -5, 105, 45)

    repaired, decision = split_polygon_by_support_components(
        polygon,
        candidate,
        support,
        min_component_area=100,
        min_retention_ratio=0.60,
    )

    assert decision["status"] == "accepted_precheck"
    assert len(list(repaired.geoms)) == 2
    assert repaired.intersection(bridge).area < bridge.area * 0.20


def test_split_polygon_by_support_components_falls_back_when_support_is_single_branch():
    polygon = box(0, 0, 100, 8)
    support = polygon.buffer(1)
    candidate = box(-5, -5, 105, 15)

    repaired, decision = split_polygon_by_support_components(
        polygon,
        candidate,
        support,
        min_component_area=100,
        min_retention_ratio=0.60,
    )

    assert decision["status"] == "rejected_not_enough_components"
    assert repaired.equals_exact(polygon, tolerance=1e-9)


def test_split_polygon_by_support_components_falls_back_when_area_loss_is_too_high():
    branch_a = box(0, 0, 100, 8)
    branch_b = box(0, 30, 100, 38)
    bridge = box(0, 8, 100, 30)
    polygon = branch_a.union(branch_b).union(bridge)
    support = branch_a.union(branch_b)
    candidate = box(-5, -5, 105, 45)

    repaired, decision = split_polygon_by_support_components(
        polygon,
        candidate,
        support,
        min_component_area=100,
        min_retention_ratio=0.80,
    )

    assert decision["status"] == "rejected_low_retention"
    assert repaired.equals_exact(polygon, tolerance=1e-9)
