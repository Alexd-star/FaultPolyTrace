# -*- coding: utf-8 -*-
"""WKT polygon helpers used by quality-control scripts.

The official CSV output uses standard WKT.  A fault polygon can be written as
POLYGON((outer),(hole1),(hole2)); quality metrics must subtract inner rings
instead of treating the exterior envelope as fully covered.
"""

import re

import numpy as np
from PIL import Image, ImageDraw


def parse_wkt_polygon_rings(wkt):
    text = str(wkt or "").strip()
    match = re.match(r"^\s*POLYGON\s*\((.*)\)\s*$", text, re.IGNORECASE | re.DOTALL)
    if not match:
        return []

    body = match.group(1)
    rings_text = []
    depth = 0
    start = None
    for index, char in enumerate(body):
        if char == "(":
            if depth == 0:
                start = index + 1
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0 and start is not None:
                rings_text.append(body[start:index])
                start = None

    rings = []
    for ring_text in rings_text:
        coords = []
        for pair in ring_text.split(","):
            parts = pair.strip().split()
            if len(parts) >= 2:
                coords.append((float(parts[0]), float(parts[1])))
        if len(coords) > 1 and coords[0] == coords[-1]:
            coords.pop()
        if len(coords) >= 3:
            rings.append(coords)
    return rings


def parse_wkt_polygon(wkt):
    rings = parse_wkt_polygon_rings(wkt)
    return rings[0] if rings else []


def parse_wkt_polygon_with_holes(wkt):
    rings = parse_wkt_polygon_rings(wkt)
    if not rings:
        return [], []
    return rings[0], rings[1:]


def ring_to_pixel_coords(coords, xs, ys, dx, dy):
    xmin_edge = float(xs.min() - 0.5 * dx)
    ymax_edge = float(ys.max() + 0.5 * dy)
    return [((x - xmin_edge) / dx, (ymax_edge - y) / dy) for x, y in coords]


def rasterize_polygon(coords, width, height, xs, ys, dx, dy, holes=None):
    image = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(image)
    exterior_pixels = ring_to_pixel_coords(coords, xs, ys, dx, dy)
    if len(exterior_pixels) >= 3:
        draw.polygon(exterior_pixels, fill=1)
    for hole in holes or []:
        hole_pixels = ring_to_pixel_coords(hole, xs, ys, dx, dy)
        if len(hole_pixels) >= 3:
            draw.polygon(hole_pixels, fill=0)
    return np.asarray(image, dtype=bool)[::-1, :]

