from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from PIL import Image

from figure_style import PNG_DIR, QA_DIR, VECTOR_DIR


EXPECTED = [f"F{i:02d}" for i in range(1, 24)]


def main() -> None:
    rows = []
    failures = []
    for prefix in EXPECTED:
        matches = sorted(PNG_DIR.glob(f"{prefix}_*.png"))
        if len(matches) != 1:
            failures.append(f"{prefix}: PNG 数量为 {len(matches)}")
            continue
        png = matches[0]
        stem = png.stem
        svg = VECTOR_DIR / f"{stem}.svg"
        pdf = VECTOR_DIR / f"{stem}.pdf"
        gray = QA_DIR / f"{stem}_gray.png"
        with Image.open(png) as image:
            rgb = np.asarray(image.convert("RGB"))
            width, height = image.size
        contrast = float(rgb.std())
        white_ratio = float(np.mean(np.all(rgb > 248, axis=2)))
        checks = {
            "png_nonempty": png.stat().st_size > 10_000,
            "svg_nonempty": svg.exists() and svg.stat().st_size > 1_000,
            "pdf_nonempty": pdf.exists() and pdf.stat().st_size > 1_000,
            "gray_exists": gray.exists() and gray.stat().st_size > 1_000,
            "resolution": width >= 1200 and height >= 500,
            "contrast": contrast >= 10.0,
            "not_blank": white_ratio < 0.97,
            "aspect": 0.8 <= width / height <= 4.5,
        }
        failed = [name for name, passed in checks.items() if not passed]
        if failed:
            failures.append(f"{stem}: {', '.join(failed)}")
        rows.append({
            "figure": stem,
            "width": width,
            "height": height,
            "contrast_std": round(contrast, 2),
            "white_ratio": round(white_ratio, 4),
            "status": "PASS" if not failed else "FAIL",
            "failed_checks": failed,
        })

    QA_DIR.mkdir(parents=True, exist_ok=True)
    (QA_DIR / "图件自动审计.json").write_text(json.dumps({"figures": rows, "failures": failures}, ensure_ascii=False, indent=2), encoding="utf-8")
    lines = ["# 图件自动审计报告", "", f"- 预期图件：{len(EXPECTED)}", f"- 已检查：{len(rows)}", f"- 失败：{len(failures)}", "", "| 图件 | 像素尺寸 | 对比度标准差 | 白色比例 | 状态 |", "|---|---:|---:|---:|---|"]
    for row in rows:
        lines.append(f"| {row['figure']} | {row['width']} × {row['height']} | {row['contrast_std']:.2f} | {row['white_ratio']:.4f} | {row['status']} |")
    if failures:
        lines.extend(["", "## 失败明细", ""] + [f"- {failure}" for failure in failures])
    (QA_DIR / "图件自动审计报告.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    if failures:
        raise SystemExit("\n".join(failures))


if __name__ == "__main__":
    main()

