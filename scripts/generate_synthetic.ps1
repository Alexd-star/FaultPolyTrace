$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $repoRoot "examples\synthetic"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$python = @'
from pathlib import Path
import math
import random

root = Path(r"__OUT_DIR__")
random.seed(42)

def write_grid(path, nx, ny, data):
    with path.open("w", encoding="utf-8") as f:
        f.write(f"{nx} {ny}\n")
        f.write("0 0\n")
        f.write("1 1\n")
        for y in range(ny):
            f.write(" ".join(f"{data[y][x]:.6f}" for x in range(nx)) + "\n")

def distance_to_segment(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    wx, wy = px - ax, py - ay
    denom = vx * vx + vy * vy
    t = 0.0 if denom == 0 else max(0.0, min(1.0, (wx * vx + wy * vy) / denom))
    cx, cy = ax + t * vx, ay + t * vy
    return math.hypot(px - cx, py - cy)

def cross_fault_grid():
    nx, ny = 160, 120
    segments = [
        (15, 25, 145, 96, 1.0),
        (40, 105, 128, 18, 0.85),
        (85, 5, 100, 110, 0.7),
    ]
    data = []
    for y in range(ny):
        row = []
        for x in range(nx):
            value = 0.04 * random.random()
            for ax, ay, bx, by, amp in segments:
                d = distance_to_segment(x, y, ax, ay, bx, by)
                value = max(value, amp * math.exp(-(d * d) / (2 * 2.4 * 2.4)))
            row.append(value)
        data.append(row)
    write_grid(root / "cross_fault_grid.txt", nx, ny, data)

def noisy_blobs_grid():
    nx, ny = 120, 100
    centers = [(30, 30, 18, 10, 0.9), (80, 70, 25, 13, 0.75), (92, 28, 10, 8, 0.65)]
    data = []
    for y in range(ny):
        row = []
        for x in range(nx):
            value = 0.08 * random.random()
            for cx, cy, rx, ry, amp in centers:
                d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
                value = max(value, amp * math.exp(-d * 2.0))
            if random.random() < 0.002:
                value = max(value, 0.9)
            row.append(value)
        data.append(row)
    write_grid(root / "noisy_blobs_grid.txt", nx, ny, data)

def l_shape_grid():
    nx, ny = 12, 12
    data = [[0.0 for _ in range(nx)] for _ in range(ny)]
    for y in range(2, 9):
        for x in range(2, 5):
            data[y][x] = 1.0
    for y in range(6, 9):
        for x in range(2, 10):
            data[y][x] = 1.0
    write_grid(root / "l_shape_grid.txt", nx, ny, data)

def ridge_fault_grid():
    nx, ny = 180, 120
    polylines = [
        [(12, 82), (45, 68), (78, 66), (126, 42), (165, 35)],
        [(35, 25), (55, 42), (82, 55), (111, 70), (145, 92)],
        [(92, 15), (91, 45), (98, 78), (112, 108)],
    ]
    data = []
    for y in range(ny):
        row = []
        for x in range(nx):
            value = 0.025 * random.random()
            for line in polylines:
                for (ax, ay), (bx, by) in zip(line, line[1:]):
                    d = distance_to_segment(x, y, ax, ay, bx, by)
                    value = max(value, math.exp(-(d * d) / (2 * 1.45 * 1.45)))
            row.append(value)
        data.append(row)
    write_grid(root / "ridge_fault_grid.txt", nx, ny, data)

def broken_ridge_fault_grid():
    nx, ny = 140, 90
    segments = [
        (12, 70, 42, 58),
        (50, 55, 78, 44),
        (86, 41, 122, 27),
    ]
    data = []
    for y in range(ny):
        row = []
        for x in range(nx):
            value = 0.02 * random.random()
            for ax, ay, bx, by in segments:
                d = distance_to_segment(x, y, ax, ay, bx, by)
                value = max(value, math.exp(-(d * d) / (2 * 1.35 * 1.35)))
            row.append(value)
        data.append(row)
    write_grid(root / "broken_ridge_fault_grid.txt", nx, ny, data)

cross_fault_grid()
noisy_blobs_grid()
l_shape_grid()
ridge_fault_grid()
broken_ridge_fault_grid()
print(f"Generated synthetic grids in {root}")
'@

$python = $python.Replace("__OUT_DIR__", ($outDir -replace "\\", "\\"))
$python | python -
