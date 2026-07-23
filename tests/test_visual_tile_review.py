# -*- coding: utf-8 -*-
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from 全图分块视觉复核 import build_tile_windows, draw_contact_sheet


def test_build_tile_windows_covers_complete_grid_without_gaps():
    tiles = build_tile_windows(width=11, height=7, cols=3, rows=2)

    assert len(tiles) == 6
    assert tiles[0]["tile_id"] == "R1C1"
    assert tiles[-1]["tile_id"] == "R2C3"

    covered = set()
    for tile in tiles:
        for y in range(tile["y0"], tile["y1"]):
            for x in range(tile["x0"], tile["x1"]):
                assert (x, y) not in covered
                covered.add((x, y))

    assert covered == {(x, y) for y in range(7) for x in range(11)}


def test_build_tile_windows_rejects_invalid_grid():
    try:
        build_tile_windows(width=0, height=7, cols=3, rows=2)
    except ValueError as exc:
        assert "width" in str(exc)
    else:
        raise AssertionError("expected ValueError")


def test_draw_contact_sheet_writes_all_tiles(tmp_path):
    tile_paths = []
    for index in range(4):
        path = tmp_path / f"R{index // 2 + 1}C{index % 2 + 1}_tile.png"
        plt.imsave(path, np.full((8, 12, 3), index / 4.0))
        tile_paths.append(path)

    output = tmp_path / "tiles_contact_sheet.png"
    draw_contact_sheet(tile_paths, output, columns=2)

    assert output.exists()
    assert output.stat().st_size > 0
