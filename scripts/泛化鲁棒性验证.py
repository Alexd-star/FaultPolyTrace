import csv
import json
import math
import random
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "faultpolytrace.exe"
OUT_ROOT = ROOT / "examples" / "outputs" / "generalization_validation"
DATA_ROOT = ROOT / "examples" / "synthetic_generalization"


def distance_to_segment(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    wx, wy = px - ax, py - ay
    denom = vx * vx + vy * vy
    t = 0.0 if denom == 0 else max(0.0, min(1.0, (wx * vx + wy * vy) / denom))
    cx, cy = ax + t * vx, ay + t * vy
    return math.hypot(px - cx, py - cy)


def add_segment(data, ax, ay, bx, by, amp=1.0, sigma=1.5, gap=None):
    ny = len(data)
    nx = len(data[0])
    for y in range(ny):
        for x in range(nx):
            if gap is not None:
                gx0, gy0, gx1, gy1 = gap
                if min(gx0, gx1) <= x <= max(gx0, gx1) and min(gy0, gy1) <= y <= max(gy0, gy1):
                    continue
            d = distance_to_segment(x, y, ax, ay, bx, by)
            data[y][x] = max(data[y][x], amp * math.exp(-(d * d) / (2.0 * sigma * sigma)))


def add_polyline(data, points, amp=1.0, sigma=1.5):
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        add_segment(data, ax, ay, bx, by, amp=amp, sigma=sigma)


def line_cells(points, width, height, step=0.5):
    cells = set()
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        length = max(1.0, math.hypot(bx - ax, by - ay))
        samples = max(2, int(length / step) + 1)
        for i in range(samples + 1):
            t = i / samples
            x = int(round(ax + (bx - ax) * t))
            y = int(round(ay + (by - ay) * t))
            if 0 <= x < width and 0 <= y < height:
                cells.add((x, y))
    return cells


def band_cells(lines, width, height, radius):
    cells = set()
    r = max(0.0, float(radius))
    margin = int(math.ceil(r)) + 1
    for points in lines:
        for (ax, ay), (bx, by) in zip(points, points[1:]):
            min_x = max(0, int(math.floor(min(ax, bx))) - margin)
            max_x = min(width - 1, int(math.ceil(max(ax, bx))) + margin)
            min_y = max(0, int(math.floor(min(ay, by))) - margin)
            max_y = min(height - 1, int(math.ceil(max(ay, by))) + margin)
            for y in range(min_y, max_y + 1):
                for x in range(min_x, max_x + 1):
                    if distance_to_segment(x + 0.5, y + 0.5, ax, ay, bx, by) <= r:
                        cells.add((x, y))
    return cells


def add_truth(
    cases,
    case,
    width,
    height,
    lines,
    truth_radius=4.0,
    min_centerline_coverage=1.0,
    min_band_coverage=0.72,
    max_outside_band_ratio=0.45,
):
    centerline = set()
    for points in lines:
        centerline.update(line_cells(points, width, height))
    case["truth_centerline_cells"] = centerline
    case["truth_band_cells"] = band_cells(lines, width, height, truth_radius)
    case["min_centerline_coverage"] = min_centerline_coverage
    case["min_band_coverage"] = min_band_coverage
    case["max_outside_band_ratio"] = max_outside_band_ratio
    cases.append(case)


def add_noise(data, scale=0.03, spikes=0):
    ny = len(data)
    nx = len(data[0])
    for y in range(ny):
        for x in range(nx):
            data[y][x] = max(0.0, min(1.0, data[y][x] + scale * random.random()))
    for _ in range(spikes):
        x = random.randrange(nx)
        y = random.randrange(ny)
        data[y][x] = max(data[y][x], random.uniform(0.75, 1.0))


def empty_grid(nx, ny):
    return [[0.0 for _ in range(nx)] for _ in range(ny)]


def write_grid(path, data):
    ny = len(data)
    nx = len(data[0])
    with path.open("w", encoding="utf-8", newline="\n") as f:
        f.write(f"{nx} {ny}\n")
        f.write("0 0\n")
        f.write("1 1\n")
        for row in data:
            f.write(" ".join(f"{v:.6f}" for v in row) + "\n")


def polygon_cells_from_csv(path, width, height):
    covered = set()
    with path.open("r", encoding="utf-8", errors="ignore", newline="") as f:
        for row in csv.DictReader(f):
            if row.get("status") != "kept":
                continue
            wkt = row.get("wkt", "")
            if not wkt.upper().startswith("POLYGON((") or not wkt.endswith("))"):
                continue
            body = wkt[wkt.find("((") + 2:-2]
            coords = []
            for pair in body.split(","):
                parts = pair.strip().split()
                if len(parts) >= 2:
                    coords.append((float(parts[0]), float(parts[1])))
            if len(coords) < 3:
                continue
            min_x = max(0, int(math.floor(min(x for x, _ in coords))))
            max_x = min(width - 1, int(math.ceil(max(x for x, _ in coords))))
            min_y = max(0, int(math.floor(min(y for _, y in coords))))
            max_y = min(height - 1, int(math.ceil(max(y for _, y in coords))))
            for y in range(min_y, max_y + 1):
                cy = y + 0.5
                for x in range(min_x, max_x + 1):
                    if point_in_polygon(x + 0.5, cy, coords):
                        covered.add((x, y))
    return covered


def point_in_polygon(px, py, coords):
    for (ax, ay), (bx, by) in zip(coords, coords[1:] + coords[:1]):
        if point_on_segment(px, py, ax, ay, bx, by):
            return True
    inside = False
    j = len(coords) - 1
    for i, (xi, yi) in enumerate(coords):
        xj, yj = coords[j]
        intersects = ((yi > py) != (yj > py)) and (
            px < (xj - xi) * (py - yi) / ((yj - yi) if abs(yj - yi) > 1e-12 else 1e-12) + xi
        )
        if intersects:
            inside = not inside
        j = i
    return inside


def point_on_segment(px, py, ax, ay, bx, by, eps=1e-7):
    vx = bx - ax
    vy = by - ay
    wx = px - ax
    wy = py - ay
    cross = abs(vx * wy - vy * wx)
    if cross > eps:
        return False
    dot = wx * vx + wy * vy
    if dot < -eps:
        return False
    length2 = vx * vx + vy * vy
    return dot <= length2 + eps


def make_cases():
    random.seed(20260505)
    DATA_ROOT.mkdir(parents=True, exist_ok=True)
    cases = []

    width, height = 180, 120
    lines = [[(10, 86), (55, 70), (100, 62), (166, 35)], [(35, 20), (68, 42), (108, 65), (150, 98)], [(95, 8), (95, 45), (104, 83), (118, 112)]]
    data = empty_grid(width, height)
    for points, amp, sigma in [(lines[0], 1.0, 1.4), (lines[1], 0.90, 1.4), (lines[2], 0.78, 1.25)]:
        add_polyline(data, points, amp=amp, sigma=sigma)
    add_noise(data, 0.02, 10)
    path = DATA_ROOT / "case_01_crossing_ridges.txt"
    write_grid(path, data)
    add_truth(cases, {
        "name": "case_01_crossing_ridges",
        "title": "多组交切细长断层",
        "path": path,
        "args": ["--percentile", "0.98", "--min-area", "25"],
        "min_polygons": 1,
        "note": "交切处允许形成连通断层网络，但必须拓扑干净、闭合稳定。",
        "max_elapsed_ms": 3000,
    }, width, height, lines, truth_radius=4.0, min_band_coverage=0.78, max_outside_band_ratio=0.55)

    width, height = 160, 100
    lines = [[(12, 75), (45, 63), (55, 60), (88, 47), (99, 44), (146, 26)]]
    data = empty_grid(width, height)
    add_segment(data, 12, 75, 45, 63, amp=1.0, sigma=1.3)
    add_segment(data, 55, 60, 88, 47, amp=0.96, sigma=1.3)
    add_segment(data, 99, 44, 146, 26, amp=0.92, sigma=1.3)
    add_noise(data, 0.025, 8)
    path = DATA_ROOT / "case_02_broken_fault.txt"
    write_grid(path, data)
    add_truth(cases, {
        "name": "case_02_broken_fault",
        "title": "断续断层自动补连",
        "path": path,
        "args": ["--percentile", "0.90", "--min-area", "10"],
        "min_polygons": 1,
        "max_elapsed_ms": 3000,
    }, width, height, lines, truth_radius=4.0, min_band_coverage=0.70, max_outside_band_ratio=0.60)

    width, height = 180, 110
    lines = [[(14, 78), (50, 70), (86, 63), (128, 52), (166, 44)], [(32, 28), (75, 42), (122, 58), (160, 80)]]
    data = empty_grid(width, height)
    add_polyline(data, [(14, 78), (50, 70), (86, 63)], amp=1.0, sigma=1.3)
    add_polyline(data, [(86, 63), (128, 52), (166, 44)], amp=0.72, sigma=1.3)
    add_polyline(data, lines[1], amp=0.82, sigma=1.2)
    add_noise(data, 0.03, 12)
    path = DATA_ROOT / "case_03_weak_continuation.txt"
    write_grid(path, data)
    add_truth(cases, {
        "name": "case_03_weak_continuation",
        "title": "强弱变化断层响应",
        "path": path,
        "args": ["--percentile", "0.98", "--weak-percentile", "0.965", "--weak-connect-radius", "1", "--min-area", "20"],
        "min_polygons": 1,
        "max_elapsed_ms": 3000,
    }, width, height, lines, truth_radius=4.0, min_band_coverage=0.76, max_outside_band_ratio=0.60)

    width, height = 150, 110
    lines = [[(18, 82), (58, 68), (98, 55), (135, 36)], [(35, 24), (70, 42), (114, 70)]]
    data = empty_grid(width, height)
    add_polyline(data, lines[0], amp=1.0, sigma=3.0)
    add_polyline(data, lines[1], amp=0.82, sigma=2.8)
    add_noise(data, 0.035, 18)
    path = DATA_ROOT / "case_04_wide_belts.txt"
    write_grid(path, data)
    add_truth(cases, {
        "name": "case_04_wide_belts",
        "title": "宽断层带响应",
        "path": path,
        "args": ["--percentile", "0.95", "--min-area", "20"],
        "min_polygons": 1,
        "max_elapsed_ms": 3000,
    }, width, height, lines, truth_radius=6.0, min_band_coverage=0.78, max_outside_band_ratio=0.55)

    width, height = 170, 120
    lines = [[(15, 20), (55, 34), (95, 45), (150, 60)], [(35, 96), (70, 83), (112, 75), (155, 88)]]
    data = empty_grid(width, height)
    add_polyline(data, lines[0], amp=0.92, sigma=1.35)
    add_polyline(data, lines[1], amp=0.86, sigma=1.25)
    add_noise(data, 0.08, 120)
    path = DATA_ROOT / "case_05_noisy_background.txt"
    write_grid(path, data)
    add_truth(cases, {
        "name": "case_05_noisy_background",
        "title": "噪声背景中的断层",
        "path": path,
        "args": ["--percentile", "0.985", "--min-area", "35", "--min-branch-length", "5"],
        "min_polygons": 1,
        "max_polygons": 80,
        "max_elapsed_ms": 3000,
    }, width, height, lines, truth_radius=4.0, min_band_coverage=0.74, max_outside_band_ratio=0.70)

    width, height = 180, 120
    lines = [[(18, 35), (62, 43), (112, 51), (160, 60)], [(20, 75), (65, 82), (112, 88), (158, 98)]]
    data = empty_grid(width, height)
    add_polyline(data, lines[0], amp=1.0, sigma=1.25)
    add_polyline(data, lines[1], amp=0.96, sigma=1.25)
    add_noise(data, 0.025, 8)
    path = DATA_ROOT / "case_06_parallel_not_merge.txt"
    write_grid(path, data)
    add_truth(cases, {
        "name": "case_06_parallel_not_merge",
        "title": "近邻平行断层不误合并",
        "path": path,
        "args": ["--percentile", "0.98", "--min-area", "20", "--max-gap-distance", "8"],
        "min_polygons": 2,
        "max_polygons": 12,
        "max_elapsed_ms": 3000,
    }, width, height, lines, truth_radius=4.0, min_band_coverage=0.78, max_outside_band_ratio=0.55)

    return cases


def run_case(case):
    out_csv = OUT_ROOT / f"{case['name']}.csv"
    args = [
        str(EXE),
        "--input", str(case["path"]),
        "--output", str(out_csv),
        "--trace-mode", "region",
        "--threshold-mode", "percentile",
        "--percentile", "0.95",
        "--min-area", "10",
        "--close-radius", "1",
        "--simplify-tolerance", "1",
        "--smooth-iterations", "0",
    ] + case.get("args", [])
    completed = subprocess.run(args, cwd=str(ROOT), text=True, capture_output=True, encoding="utf-8", errors="ignore")
    diag_path = Path(str(out_csv) + ".diagnostics.json")
    if completed.returncode != 0:
        return {
            "name": case["name"],
            "title": case["title"],
            "passed": False,
            "reason": "程序运行失败",
            "stdout": completed.stdout,
            "stderr": completed.stderr,
        }
    with diag_path.open("r", encoding="utf-8", errors="ignore") as f:
        diag = json.load(f)
    with out_csv.open("r", encoding="utf-8", errors="ignore", newline="") as f:
        rows = list(csv.DictReader(f))
    kept = sum(1 for row in rows if row.get("status") == "kept")
    truth_centerline_cells = case.get("truth_centerline_cells", set())
    truth_band_cells = case.get("truth_band_cells", set())
    covered_cells = polygon_cells_from_csv(out_csv, diag["grid"]["nx"], diag["grid"]["ny"]) if truth_centerline_cells else set()
    centerline_covered = len(truth_centerline_cells & covered_cells)
    centerline_coverage = centerline_covered / len(truth_centerline_cells) if truth_centerline_cells else 1.0
    band_covered = len(truth_band_cells & covered_cells)
    band_coverage = band_covered / len(truth_band_cells) if truth_band_cells else 1.0
    outside_band_ratio = (
        max(0, len(covered_cells) - len(covered_cells & truth_band_cells)) / len(covered_cells)
        if covered_cells else 0.0
    )
    checks = []
    checks.append(("开放环为 0", diag["topology"]["openRings"] == 0))
    checks.append(("自相交为 0", diag["topology"]["selfIntersectingPolygons"] == 0))
    checks.append(("多边形异常相交为 0", diag["topology"]["polygonIntersections"] == 0))
    checks.append(("保留多边形数量达标", kept >= case.get("min_polygons", 0)))
    checks.append(("应识别断层中心线 100% 覆盖", centerline_coverage >= case.get("min_centerline_coverage", 1.0)))
    checks.append(("断层影响带覆盖率达标", band_coverage >= case.get("min_band_coverage", 0.0)))
    checks.append(("断层带外覆盖比例不过高", outside_band_ratio <= case.get("max_outside_band_ratio", 1.0)))
    if "max_polygons" in case:
        checks.append(("保留多边形数量不过多", kept <= case["max_polygons"]))
    if "min_connected_gaps" in case:
        checks.append(("断点补连数量达标", diag["skeleton"]["connectedGaps"] >= case["min_connected_gaps"]))
    checks.append(("耗时达标", diag["timing"]["elapsedMs"] <= case.get("max_elapsed_ms", 3000)))
    passed = all(ok for _, ok in checks)
    failed = [name for name, ok in checks if not ok]
    return {
        "name": case["name"],
        "title": case["title"],
        "passed": passed,
        "reason": "；".join(failed) if failed else "通过",
        "kept": kept,
        "polygon_count": diag["results"]["polygonCount"],
        "active_cells": diag["results"]["activeCellCount"],
        "truth_centerline_cells": len(truth_centerline_cells),
        "centerline_covered": centerline_covered,
        "centerline_coverage": centerline_coverage,
        "truth_band_cells": len(truth_band_cells),
        "band_covered": band_covered,
        "band_coverage": band_coverage,
        "outside_band_ratio": outside_band_ratio,
        "connected_gaps": diag["skeleton"]["connectedGaps"],
        "open_rings": diag["topology"]["openRings"],
        "self_intersections": diag["topology"]["selfIntersectingPolygons"],
        "polygon_intersections": diag["topology"]["polygonIntersections"],
        "elapsed_ms": diag["timing"]["elapsedMs"],
        "trace_ms": diag["timing"]["traceMs"],
        "output": str(out_csv),
        "note": case.get("note", ""),
    }


def write_report(results):
    report_path = OUT_ROOT / "泛化鲁棒性验证报告.md"
    lines = [
        "# 泛化鲁棒性验证报告",
        "",
        "本报告用于检查 FaultPolyTrace 是否只对官方 test.dat 有效。验证数据由脚本自动生成，覆盖交切、断续、弱响应、宽断层带、噪声背景和近邻平行断层等常见断层属性形态。",
        "",
        "## 1. 验证结论",
        "",
    ]
    failed = [r for r in results if not r["passed"]]
    lines.append(f"- 场景总数：{len(results)}")
    lines.append(f"- 通过场景：{len(results) - len(failed)}")
    lines.append(f"- 未通过场景：{len(failed)}")
    lines.append(f"- 总体结论：{'通过' if not failed else '存在需修正场景'}")
    lines.extend(["", "## 2. 场景结果", "", "| 场景 | 结论 | 多边形 | 中心线覆盖率 | 断层带覆盖率 | 带外覆盖比例 | 补连 | 开环 | 自交 | 多边形相交 | 总耗时 ms | 说明 |", "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|"])
    for r in results:
        lines.append(
            f"| {r['title']} | {'通过' if r['passed'] else '未通过'} | "
            f"{r.get('kept', '')} | {r.get('centerline_coverage', 0):.3f} | {r.get('band_coverage', 0):.3f} | {r.get('outside_band_ratio', 0):.3f} | "
            f"{r.get('connected_gaps', '')} | {r.get('open_rings', '')} | "
            f"{r.get('self_intersections', '')} | {r.get('polygon_intersections', '')} | "
            f"{r.get('elapsed_ms', '')} | {r['reason']} |"
        )
    lines.extend([
        "",
        "## 3. 解释",
        "",
        "本验证不使用官方 test.dat 的坐标、图像或对象名，输入为独立生成的规则网格。所有场景使用同一套 Ridge-Belt 主流程，只允许少量与场景类型相匹配的通用参数变化，例如宽断层带使用更大的带宽半径，噪声背景使用更高分位数阈值。",
        "",
        "交切断层在属性图上可能天然形成一个连通网络，因此验证重点不是强制拆成多个多边形，而是检查闭合、拓扑和稳定性。为防止过度合并，另设近邻平行断层场景，要求相互分离的断层不被错误连成一片。",
        "",
        "通过本验证只能说明当前算法具备基础泛化能力，不能等价于商业产品已在所有地质区块中完全成熟。正式企业级产品还应增加真实多区块数据回归集、人工解释对照、参数自动推荐和交互式质控闭环。",
        "",
        "中心线覆盖率是第一硬门槛，要求合成数据中所有应识别断层中心线采样点都被闭合多边形覆盖。断层带覆盖率和带外覆盖比例是第二层产品质量指标，用于检查边界是否贴合、是否过度解释。该指标比单纯统计多边形数量更接近产品级验收逻辑。",
    ])
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return report_path


def main():
    if not EXE.exists():
        raise SystemExit(f"未找到可执行文件，请先运行 scripts/build.ps1: {EXE}")
    OUT_ROOT.mkdir(parents=True, exist_ok=True)
    cases = make_cases()
    results = [run_case(case) for case in cases]
    summary_csv = OUT_ROOT / "泛化鲁棒性验证汇总.csv"
    fields = [
        "name", "title", "passed", "reason", "note", "kept", "polygon_count", "active_cells",
        "truth_centerline_cells", "centerline_covered", "centerline_coverage",
        "truth_band_cells", "band_covered", "band_coverage", "outside_band_ratio",
        "connected_gaps", "open_rings", "self_intersections", "polygon_intersections",
        "trace_ms", "elapsed_ms", "output",
    ]
    with summary_csv.open("w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(results)
    report = write_report(results)
    print(report)
    if any(not r["passed"] for r in results):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
