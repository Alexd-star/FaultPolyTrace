# -*- coding: utf-8 -*-
"""Topology validation for local morphology repair experiments.

This helper is evidence-only. It reads a result CSV, validates kept polygon
WKT, checks geometry validity, and reports polygon area intersections. It does
not repair or rewrite algorithm output.
"""

import argparse
import csv
import json
import sys
from pathlib import Path

from shapely import wkt
from shapely.geometry import GeometryCollection, MultiPolygon, Polygon

csv.field_size_limit(min(sys.maxsize, 2_147_483_647))


def _safe_float(value, default=0.0):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def _iter_polygon_parts(geometry):
    if geometry.is_empty:
        return []
    if isinstance(geometry, Polygon):
        return [geometry]
    if isinstance(geometry, MultiPolygon):
        return [part for part in geometry.geoms if not part.is_empty]
    if isinstance(geometry, GeometryCollection):
        return [part for part in geometry.geoms if isinstance(part, Polygon) and not part.is_empty]
    return []


def _bounds_overlap(a, b):
    return not (a[2] < b[0] or a[0] > b[2] or a[3] < b[1] or a[1] > b[3])


def _load_kept_geometries(csv_path):
    geometries = []
    details = []
    kept_count = 0
    with open(csv_path, "r", encoding="utf-8-sig", errors="ignore", newline="") as f:
        reader = csv.DictReader(f)
        for row_number, row in enumerate(reader, start=2):
            if str(row.get("status", "")).lower() != "kept":
                continue
            kept_count += 1
            polygon_id = str(row.get("id", kept_count) or kept_count)
            wkt_text = str(row.get("wkt", "") or "").strip()
            try:
                geometry = wkt.loads(wkt_text)
            except Exception as exc:  # GEOSException class differs by Shapely version.
                details.append(
                    {
                        "issue": "parse_error",
                        "polygon_id": polygon_id,
                        "other_polygon_id": "",
                        "row_number": row_number,
                        "area": 0.0,
                        "message": str(exc),
                    }
                )
                continue

            polygon_parts = _iter_polygon_parts(geometry)
            if not polygon_parts:
                details.append(
                    {
                        "issue": "empty_or_non_polygon",
                        "polygon_id": polygon_id,
                        "other_polygon_id": "",
                        "row_number": row_number,
                        "area": 0.0,
                        "message": f"geometry type is {geometry.geom_type}",
                    }
                )
                continue

            for part_index, part in enumerate(polygon_parts, start=1):
                part_id = polygon_id if len(polygon_parts) == 1 else f"{polygon_id}.{part_index}"
                if not part.is_valid:
                    details.append(
                        {
                            "issue": "invalid_geometry",
                            "polygon_id": part_id,
                            "other_polygon_id": "",
                            "row_number": row_number,
                            "area": _safe_float(part.area),
                            "message": "Shapely reports invalid polygon geometry",
                        }
                    )
                geometries.append({"id": part_id, "row_number": row_number, "geometry": part, "bounds": part.bounds})
    return kept_count, geometries, details


def check_csv_topology(csv_path, area_tolerance=1e-6):
    """Return ``(summary, details)`` for a result CSV topology check."""
    csv_path = Path(csv_path)
    kept_count, geometries, details = _load_kept_geometries(csv_path)

    invalid_count = sum(1 for detail in details if detail["issue"] == "invalid_geometry")
    parse_error_count = sum(1 for detail in details if detail["issue"] == "parse_error")
    non_polygon_count = sum(1 for detail in details if detail["issue"] == "empty_or_non_polygon")

    intersection_count = 0
    valid_geometries = [item for item in geometries if item["geometry"].is_valid and not item["geometry"].is_empty]
    for left_index, left in enumerate(valid_geometries):
        left_geometry = left["geometry"]
        for right in valid_geometries[left_index + 1 :]:
            if not _bounds_overlap(left["bounds"], right["bounds"]):
                continue
            right_geometry = right["geometry"]
            intersection = left_geometry.intersection(right_geometry)
            area = _safe_float(intersection.area)
            if area > float(area_tolerance):
                intersection_count += 1
                details.append(
                    {
                        "issue": "polygon_area_intersection",
                        "polygon_id": left["id"],
                        "other_polygon_id": right["id"],
                        "row_number": left["row_number"],
                        "area": area,
                        "message": "Two kept polygons overlap by positive area",
                    }
                )

    summary = {
        "csv": str(csv_path),
        "kept_polygon_count": kept_count,
        "geometry_part_count": len(geometries),
        "parse_error_count": parse_error_count,
        "invalid_geometry_count": invalid_count,
        "empty_or_non_polygon_count": non_polygon_count,
        "polygon_intersection_count": intersection_count,
        "area_tolerance": float(area_tolerance),
        "pass": parse_error_count == 0 and invalid_count == 0 and non_polygon_count == 0 and intersection_count == 0,
    }
    return summary, details


def write_outputs(out_dir, summary, details):
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    summary_path = out_dir / "局部修形拓扑核验摘要.json"
    detail_csv = out_dir / "局部修形拓扑核验明细.csv"
    report_path = out_dir / "局部修形拓扑核验报告.md"

    summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    fields = ["issue", "polygon_id", "other_polygon_id", "row_number", "area", "message"]
    with open(detail_csv, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        for detail in details:
            writer.writerow(detail)

    status = "通过" if summary["pass"] else "未通过"
    lines = [
        "# 局部修形拓扑核验报告",
        "",
        "本报告只验证局部修形实验 CSV 的几何合法性，不修改任何结果。它用于判断实验结果是否具备进入下一轮视觉复核和正式后处理设计的资格。",
        "",
        "## 核验结论",
        "",
        f"- 结论：{status}",
        f"- kept 多边形数量：{summary['kept_polygon_count']}",
        f"- 几何部件数量：{summary['geometry_part_count']}",
        f"- WKT 解析错误：{summary['parse_error_count']}",
        f"- 无效几何：{summary['invalid_geometry_count']}",
        f"- 空几何或非多边形：{summary['empty_or_non_polygon_count']}",
        f"- 多边形面积相交：{summary['polygon_intersection_count']}",
        f"- 面积相交容差：{summary['area_tolerance']}",
        "",
        "## 输出文件",
        "",
        f"- 摘要 JSON：`{summary_path}`",
        f"- 明细 CSV：`{detail_csv}`",
    ]
    if details:
        lines.extend(["", "## 问题明细预览", "", "| 类型 | Polygon | 关联 Polygon | 面积 | 说明 |", "|---|---:|---:|---:|---|"])
        for detail in details[:30]:
            lines.append(
                f"| {detail.get('issue', '')} | {detail.get('polygon_id', '')} | "
                f"{detail.get('other_polygon_id', '')} | {_safe_float(detail.get('area')):.6f} | {detail.get('message', '')} |"
            )
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return summary_path, detail_csv, report_path


def main():
    parser = argparse.ArgumentParser(description="核验局部修形实验 CSV 的拓扑合法性")
    parser.add_argument("--csv", required=True, help="待核验的结果 CSV")
    parser.add_argument("--out-dir", required=True, help="输出目录")
    parser.add_argument("--area-tolerance", type=float, default=1e-6, help="面积相交容差")
    args = parser.parse_args()

    summary, details = check_csv_topology(args.csv, area_tolerance=args.area_tolerance)
    summary_path, detail_csv, report_path = write_outputs(args.out_dir, summary, details)
    print(summary_path)
    print(detail_csv)
    print(report_path)
    if not summary["pass"]:
        sys.exit(2)


if __name__ == "__main__":
    main()
