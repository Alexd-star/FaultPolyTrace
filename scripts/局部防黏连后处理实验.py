import argparse
import csv
import re
import sys
from collections import deque
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

csv.field_size_limit(min(sys.maxsize, 2_147_483_647))


def read_geoeast_dat(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            text = line.strip()
            if not text or text.startswith("#"):
                continue
            parts = text.split()
            if len(parts) < 5:
                continue
            rows.append((int(float(parts[0])), int(float(parts[1])), float(parts[2]), float(parts[3]), float(parts[4])))
    arr = np.asarray(rows, dtype=float)
    line_keys = np.unique(arr[:, 0]).astype(int)
    cmp_keys = np.unique(arr[:, 1]).astype(int)
    line_index = {v: i for i, v in enumerate(line_keys)}
    cmp_index = {v: i for i, v in enumerate(cmp_keys)}
    values = np.zeros((len(cmp_keys), len(line_keys)), dtype=float)
    x_by_line = {}
    y_by_cmp = {}
    for line_no, cmp_no, x, y, value in rows:
        values[cmp_index[int(cmp_no)], line_index[int(line_no)]] = value
        x_by_line[int(line_no)] = x
        y_by_cmp[int(cmp_no)] = y
    xs = np.asarray([x_by_line[int(k)] for k in line_keys], dtype=float)
    ys = np.asarray([y_by_cmp[int(k)] for k in cmp_keys], dtype=float)
    dx = abs(xs[1] - xs[0]) if len(xs) > 1 else 1.0
    dy = abs(ys[1] - ys[0]) if len(ys) > 1 else 1.0
    return values, xs, ys, dx, dy


def parse_wkt_polygon(wkt):
    match = re.search(r"POLYGON\s*\(\((.*)\)\)", str(wkt).strip(), re.IGNORECASE)
    if not match:
        return []
    coords = []
    for pair in match.group(1).split(","):
        parts = pair.strip().split()
        if len(parts) >= 2:
            coords.append((float(parts[0]), float(parts[1])))
    if len(coords) > 1 and coords[0] == coords[-1]:
        coords.pop()
    return coords


def read_polygon_rows(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            coords = parse_wkt_polygon(row.get("wkt", ""))
            rows.append(
                {
                    "id": int(float(row.get("id", len(rows) + 1) or 0)),
                    "status": row.get("status", ""),
                    "area": float(row.get("area", 0.0) or 0.0),
                    "perimeter": float(row.get("perimeter", 0.0) or 0.0),
                    "cell_count": int(float(row.get("cell_count", 0) or 0)),
                    "mean_value": float(row.get("mean_value", 0.0) or 0.0),
                    "max_value": float(row.get("max_value", 0.0) or 0.0),
                    "coords": coords,
                }
            )
    return rows


def read_candidate_ids(path):
    ids = []
    with open(path, "r", encoding="utf-8-sig", errors="ignore", newline="") as f:
        for row in csv.DictReader(f):
            if row.get("recommendation") == "建议进入局部拆分候选":
                ids.append(int(float(row["id"])))
    return set(ids)


def rasterize_polygon(coords, width, height, xs, ys, dx, dy):
    xmin_edge = float(xs.min() - 0.5 * dx)
    ymax_edge = float(ys.max() + 0.5 * dy)
    image = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(image)
    pixel_coords = [((x - xmin_edge) / dx, (ymax_edge - y) / dy) for x, y in coords]
    draw.polygon(pixel_coords, fill=1)
    return np.asarray(image, dtype=bool)[::-1, :]


def connected_components(mask, min_cells=1):
    height, width = mask.shape
    visited = np.zeros_like(mask, dtype=bool)
    components = []
    offsets = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]
    for y0 in range(height):
        for x0 in range(width):
            if not mask[y0, x0] or visited[y0, x0]:
                continue
            queue = deque([(x0, y0)])
            visited[y0, x0] = True
            cells = []
            while queue:
                x, y = queue.popleft()
                cells.append((x, y))
                for ox, oy in offsets:
                    nx, ny = x + ox, y + oy
                    if 0 <= nx < width and 0 <= ny < height and mask[ny, nx] and not visited[ny, nx]:
                        visited[ny, nx] = True
                        queue.append((nx, ny))
            if len(cells) >= min_cells:
                components.append(cells)
    components.sort(key=len, reverse=True)
    return components


def trace_boundary_rings(component_mask, xs, ys, dx, dy):
    h, w = component_mask.shape
    edges = []
    yy, xx = np.nonzero(component_mask)
    for x, y in zip(xx, yy):
        if y - 1 < 0 or not component_mask[y - 1, x]:
            edges.append(((x, y), (x + 1, y)))
        if x + 1 >= w or not component_mask[y, x + 1]:
            edges.append(((x + 1, y), (x + 1, y + 1)))
        if y + 1 >= h or not component_mask[y + 1, x]:
            edges.append(((x + 1, y + 1), (x, y + 1)))
        if x - 1 < 0 or not component_mask[y, x - 1]:
            edges.append(((x, y + 1), (x, y)))

    next_by_start = {}
    for a, b in edges:
        next_by_start.setdefault(a, []).append(b)
    for pts in next_by_start.values():
        pts.sort()

    rings = []
    while next_by_start:
        start = next(iter(next_by_start))
        current = start
        vertices = [current]
        guard = 0
        while guard <= len(edges) + 4:
            guard += 1
            choices = next_by_start.get(current)
            if not choices:
                break
            nxt = choices.pop(0)
            if not choices:
                next_by_start.pop(current, None)
            current = nxt
            if current == start:
                break
            vertices.append(current)
        if current == start and len(vertices) >= 3:
            ring = [(float(xs.min() - 0.5 * dx + vx * dx), float(ys.max() + 0.5 * dy - vy * dy)) for vx, vy in vertices]
            if signed_area(ring) > 0:
                rings.append(ring)
            else:
                rings.append(list(reversed(ring)))
    rings.sort(key=lambda r: abs(signed_area(r)), reverse=True)
    return rings


def signed_area(points):
    s = 0.0
    for i, a in enumerate(points):
        b = points[(i + 1) % len(points)]
        s += a[0] * b[1] - b[0] * a[1]
    return 0.5 * s


def perimeter(points):
    total = 0.0
    for i, a in enumerate(points):
        b = points[(i + 1) % len(points)]
        total += ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5
    return total


def wkt(points):
    text = ",".join(f"{x:.6f} {y:.6f}" for x, y in points)
    if points:
        text += f",{points[0][0]:.6f} {points[0][1]:.6f}"
    return f"POLYGON(({text}))"


def make_row(poly_id, status, points, cells, values):
    arr = np.asarray(cells, dtype=int)
    vals = values[arr[:, 1], arr[:, 0]] if len(cells) else np.asarray([], dtype=float)
    return {
        "id": poly_id,
        "status": status,
        "area": abs(signed_area(points)),
        "perimeter": perimeter(points),
        "cell_count": int(len(cells)),
        "mean_value": float(np.mean(vals)) if vals.size else 0.0,
        "max_value": float(np.max(vals)) if vals.size else 0.0,
        "wkt": wkt(points),
    }


def main():
    parser = argparse.ArgumentParser(description="局部防黏连后处理实验：仅对明确候选多边形切掉低属性桥后重新追踪边界")
    parser.add_argument("--dat", required=True)
    parser.add_argument("--csv", required=True)
    parser.add_argument("--candidate-csv", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--low-threshold", type=float, default=0.05)
    parser.add_argument("--min-part-cells", type=int, default=80)
    args = parser.parse_args()

    values, xs, ys, dx, dy = read_geoeast_dat(args.dat)
    rows = read_polygon_rows(args.csv)
    candidate_ids = read_candidate_ids(args.candidate_csv)
    out_rows = []
    next_id = 1
    split_summary = []
    for row in rows:
        if row["status"].lower() != "kept" or row["id"] not in candidate_ids or len(row["coords"]) < 3:
            out = {
                "id": next_id,
                "status": row["status"],
                "area": row["area"],
                "perimeter": row["perimeter"],
                "cell_count": row["cell_count"],
                "mean_value": row["mean_value"],
                "max_value": row["max_value"],
                "wkt": wkt(row["coords"]),
            }
            out_rows.append(out)
            next_id += 1
            continue
        poly_mask = rasterize_polygon(row["coords"], values.shape[1], values.shape[0], xs, ys, dx, dy)
        split_mask = poly_mask & (values > args.low_threshold)
        parts = connected_components(split_mask, min_cells=args.min_part_cells)
        if len(parts) < 2:
            out = {
                "id": next_id,
                "status": row["status"],
                "area": row["area"],
                "perimeter": row["perimeter"],
                "cell_count": row["cell_count"],
                "mean_value": row["mean_value"],
                "max_value": row["max_value"],
                "wkt": wkt(row["coords"]),
            }
            out_rows.append(out)
            next_id += 1
            continue
        split_summary.append((row["id"], len(parts)))
        for cells in parts:
            component_mask = np.zeros_like(poly_mask, dtype=bool)
            for x, y in cells:
                component_mask[y, x] = True
            rings = trace_boundary_rings(component_mask, xs, ys, dx, dy)
            if not rings:
                continue
            out_rows.append(make_row(next_id, "kept", rings[0], cells, values))
            next_id += 1

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, "w", encoding="utf-8", newline="") as f:
        fields = ["id", "status", "area", "perimeter", "cell_count", "mean_value", "max_value", "wkt"]
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in out_rows:
            writer.writerow(row)

    summary_path = Path(args.output).with_suffix(".split_summary.txt")
    with open(summary_path, "w", encoding="utf-8") as f:
        f.write("局部防黏连后处理实验摘要\n")
        f.write(f"输入: {args.csv}\n")
        f.write(f"候选: {args.candidate_csv}\n")
        f.write(f"输出: {args.output}\n")
        f.write(f"拆分对象数: {len(split_summary)}\n")
        for old_id, part_count in split_summary:
            f.write(f"多边形 {old_id} -> {part_count} 个主体\n")
    print(args.output)
    print(summary_path)


if __name__ == "__main__":
    main()
