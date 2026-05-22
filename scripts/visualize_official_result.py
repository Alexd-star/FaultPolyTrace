import argparse
import csv
import os
import re

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from wkt_geometry import parse_wkt_polygon_with_holes


def read_official_dat(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            text = line.strip()
            if not text or text.startswith("#"):
                continue
            parts = text.split()
            if len(parts) < 5:
                continue
            rows.append(
                (
                    int(float(parts[0])),
                    int(float(parts[1])),
                    float(parts[2]),
                    float(parts[3]),
                    float(parts[4]),
                )
            )
    if not rows:
        raise RuntimeError(f"No numeric rows found in {path}")

    arr = np.asarray(rows, dtype=float)
    lines = np.unique(arr[:, 0]).astype(int)
    cmps = np.unique(arr[:, 1]).astype(int)
    line_index = {v: i for i, v in enumerate(lines)}
    cmp_index = {v: i for i, v in enumerate(cmps)}

    values = np.full((len(lines), len(cmps)), np.nan, dtype=float)
    xs = np.full_like(values, np.nan)
    ys = np.full_like(values, np.nan)

    for line, cmp_no, x, y, value in rows:
        i = line_index[line]
        j = cmp_index[cmp_no]
        values[i, j] = value
        xs[i, j] = x
        ys[i, j] = y

    extent = (
        float(np.nanmin(xs)),
        float(np.nanmax(xs)),
        float(np.nanmin(ys)),
        float(np.nanmax(ys)),
    )
    return values.T, extent


def read_polygons(csv_path, max_polygons=None):
    polygons = []
    with open(csv_path, "r", encoding="utf-8", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row.get("status", "").lower() != "kept":
                continue
            coords, holes = parse_wkt_polygon_with_holes(row.get("wkt", ""))
            if coords:
                polygons.append({"coords": coords, "holes": holes})
            if max_polygons and len(polygons) >= max_polygons:
                break
    return polygons


def draw_attribute(values, extent, output_path):
    fig, ax = plt.subplots(figsize=(12, 7), dpi=220)
    image = ax.imshow(
        values,
        extent=extent,
        origin="lower",
        cmap="magma",
        interpolation="nearest",
        aspect="auto",
    )
    ax.set_title("Original planar fault attribute: test7_fault_attribute_bo")
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    fig.colorbar(image, ax=ax, shrink=0.85, label="Fault attribute")
    ax.grid(False)
    fig.tight_layout()
    fig.savefig(output_path)
    plt.close(fig)


def draw_overlay(values, extent, polygons, output_path):
    fig, ax = plt.subplots(figsize=(12, 7), dpi=220)
    image = ax.imshow(
        values,
        extent=extent,
        origin="lower",
        cmap="gray_r",
        interpolation="nearest",
        aspect="auto",
        vmin=0.0,
        vmax=max(1.0, float(np.nanmax(values))),
    )
    for poly in polygons:
        coords = poly["coords"]
        xs = [p[0] for p in coords]
        ys = [p[1] for p in coords]
        ax.plot(xs, ys, color="#ffcc00", linewidth=0.55, alpha=0.9)
        for hole in poly.get("holes", []):
            hx = [p[0] for p in hole]
            hy = [p[1] for p in hole]
            ax.plot(hx, hy, color="#ffcc00", linewidth=0.40, alpha=0.72, linestyle="--")
    ax.set_title(f"FaultPolyTrace overlay: {len(polygons)} closed polygons")
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    fig.colorbar(image, ax=ax, shrink=0.85, label="Fault attribute")
    ax.grid(False)
    fig.tight_layout()
    fig.savefig(output_path)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(
        description="Render official problem-7 attribute and FaultPolyTrace polygon overlay."
    )
    parser.add_argument("--dat", required=True, help="Official test.dat path")
    parser.add_argument("--csv", required=True, help="FaultPolyTrace polygon CSV path")
    parser.add_argument("--out-dir", required=True, help="Directory for PNG outputs")
    parser.add_argument("--max-polygons", type=int, default=0, help="Optional polygon limit")
    args = parser.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    values, extent = read_official_dat(args.dat)
    polygons = read_polygons(args.csv, args.max_polygons or None)

    attribute_png = os.path.join(args.out_dir, "01_original_fault_attribute.png")
    overlay_png = os.path.join(args.out_dir, "02_faultpolytrace_overlay.png")
    draw_attribute(values, extent, attribute_png)
    draw_overlay(values, extent, polygons, overlay_png)

    print(f"Attribute image: {attribute_png}")
    print(f"Overlay image: {overlay_png}")
    print(f"Grid shape: {values.shape[1]} x {values.shape[0]}")
    print(f"Polygons rendered: {len(polygons)}")


if __name__ == "__main__":
    main()
