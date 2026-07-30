import csv
import json
from pathlib import Path
import subprocess
import time

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib import font_manager
import numpy as np
from scipy.ndimage import binary_erosion, gaussian_filter

from .attributes import robust_normalize
from .io import read_ascii_grid, write_ascii_grid
from .metrics import evaluate_prediction_csv, prediction_masks_from_csv
from .synthetic import FAMILIES, generate_case, save_case


def _configure_chinese_font():
    candidates = (
        Path("C:/Windows/Fonts/msyh.ttc"),
        Path("C:/Windows/Fonts/msyhbd.ttc"),
        Path("C:/Windows/Fonts/simhei.ttf"),
    )
    for font_path in candidates:
        if font_path.exists():
            font_manager.fontManager.addfont(str(font_path))
            name = font_manager.FontProperties(fname=str(font_path)).get_name()
            plt.rcParams["font.sans-serif"] = [name, "DejaVu Sans"]
            plt.rcParams["axes.unicode_minus"] = False
            return name
    return None


_configure_chinese_font()


FORMAL_TRACE_ARGUMENTS = (
    "--input-format", "auto",
    "--trace-mode", "ridge-belt",
    "--threshold-mode", "percentile",
    "--threshold", "0.95",
    "--percentile", "0.98",
    "--min-area", "120",
    "--close-radius", "1",
    "--belt-radius", "3",
    "--post-belt-close-radius", "1",
    "--weak-percentile", "0.965",
    "--weak-connect-radius", "1",
    "--attribute-envelope-radius", "1",
    "--min-branch-length", "4",
    "--min-junction-branch-length", "2",
    "--min-junction-branch-mean", "0.94",
    "--max-gap-distance", "12",
    "--max-gap-angle", "55",
    "--min-gap-support-ratio", "0.25",
    "--no-separate-branch-belts",
    "--attribute-constrained-belt",
    "--root-anti-merge",
    "--root-anti-merge-radius", "1",
    "--root-anti-merge-support-ratio", "0.25",
    "--root-anti-merge-max-bridge-value", "1e-12",
    "--root-anti-merge-min-part-cells", "0",
    "--root-anti-merge-min-part-ratio", "0.12",
    "--root-anti-merge-weak-guard-threshold", "0.66",
    "--root-anti-merge-weak-guard-radius", "0",
    "--no-local-anti-merge",
    "--no-residual-trace",
    "--simplify-tolerance", "2",
    "--smooth-iterations", "1",
)


def _load_background(path):
    path = Path(path)
    if path.suffix.lower() == ".npy":
        return np.asarray(np.load(path, mmap_mode="r", allow_pickle=False), dtype=np.float32)
    return read_ascii_grid(path).values


def generate_benchmark(output_dir, count=40, width=256, height=192, seed=20260723, backgrounds=None):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    background_paths = [Path(item) for item in (backgrounds or [])]
    cases = []
    for index in range(int(count)):
        family = FAMILIES[index % len(FAMILIES)]
        case_seed = int(seed) + index * 7919
        background_path = background_paths[index % len(background_paths)] if background_paths else None
        background = _load_background(background_path) if background_path else None
        case = generate_case(width, height, case_seed, family, background=background)
        if background_path:
            case.metadata["background_source"] = str(background_path)
        case_name = f"case_{index + 1:04d}_{family}"
        save_case(case, output_dir / case_name)
        cases.append({"id": case_name, "path": case_name, **case.metadata})
    manifest = {
        "schema_version": 1,
        "benchmark_type": "controlled_planar_fault_attribute",
        "truth_independence": "geometry_is_generated_before_attribute_rendering",
        "real_data_claim": False,
        "count": len(cases),
        "families": list(FAMILIES),
        "base_seed": int(seed),
        "cases": cases,
    }
    (output_dir / "benchmark_manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return manifest


def _trace_command(executable, input_grid, output_csv, trace_arguments=FORMAL_TRACE_ARGUMENTS):
    output_csv = Path(output_csv)
    return [
        str(executable),
        "--input", str(input_grid),
        "--output", str(output_csv),
        "--diagnostics", str(output_csv.with_suffix(".diagnostics.json")),
        "--branches-output", str(output_csv.with_suffix(".branches.csv")),
        "--topology-output", str(output_csv.with_suffix(".topology.csv")),
        *trace_arguments,
    ]


def run_trace(executable, input_grid, output_csv, trace_arguments=FORMAL_TRACE_ARGUMENTS):
    command = _trace_command(Path(executable), Path(input_grid), Path(output_csv), trace_arguments)
    started = time.perf_counter()
    process = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
    elapsed_ms = int(round((time.perf_counter() - started) * 1000.0))
    if process.returncode != 0:
        raise RuntimeError(
            f"FaultPolyTrace failed with exit code {process.returncode}:\n{process.stdout}\n{process.stderr}"
        )
    return {"elapsed_ms": elapsed_ms, "command": command, "stdout": process.stdout, "stderr": process.stderr}


def _binary_boundary(mask):
    mask = np.asarray(mask, dtype=bool)
    return mask & ~binary_erosion(mask, structure=np.ones((3, 3), dtype=bool), border_value=0)


def _save_review_figure(attribute, truth, predicted, output_path, title):
    fig, axes = plt.subplots(1, 3, figsize=(9.0, 3.1), dpi=160)
    panels = ((attribute, "断层属性"), (truth, "几何真值"), (attribute, "结果叠加"))
    for axis, (image, label) in zip(axes, panels):
        axis.imshow(image, origin="lower", cmap="gray", interpolation="nearest")
        axis.set_title(label, fontsize=9)
        axis.set_xticks([])
        axis.set_yticks([])
    truth_boundary = _binary_boundary(truth)
    predicted_boundary = _binary_boundary(predicted)
    axes[2].contour(truth_boundary.astype(float), levels=[0.5], colors=["#198754"], linewidths=0.8, origin="lower")
    axes[2].contour(predicted_boundary.astype(float), levels=[0.5], colors=["#C62828"], linewidths=0.8, origin="lower")
    axes[2].legend(
        handles=[
            Line2D([0], [0], color="#198754", lw=1.4, label="几何真值边界"),
            Line2D([0], [0], color="#C62828", lw=1.4, label="算法结果边界"),
        ],
        loc="lower right",
        frameon=True,
        framealpha=0.9,
        fontsize=6.5,
    )
    fig.suptitle(title, fontsize=10)
    fig.tight_layout()
    fig.savefig(output_path, bbox_inches="tight")
    plt.close(fig)


def _prediction_union(csv_path, shape):
    _, masks = prediction_masks_from_csv(csv_path, shape)
    return np.any(np.stack(masks), axis=0) if masks else np.zeros(shape, dtype=bool)


def _mask_iou(first, second):
    union = np.count_nonzero(first | second)
    return float(np.count_nonzero(first & second) / union) if union else 1.0


def _perturbations(attribute, seed):
    rng = np.random.default_rng(seed)
    return {
        "gain_085": np.clip(attribute * 0.85, 0.0, 1.0),
        "gain_115": np.clip(attribute * 1.15, 0.0, 1.0),
        "noise_002": np.clip(attribute + rng.normal(0.0, 0.02, attribute.shape), 0.0, 1.0),
        "smooth_070": robust_normalize(gaussian_filter(attribute, sigma=0.70)),
    }


def _case_stability(
    executable,
    case_id,
    case_dir,
    result_dir,
    baseline_mask,
    seed,
    trace_arguments=FORMAL_TRACE_ARGUMENTS,
):
    attribute = np.asarray(np.load(case_dir / "attribute.npy", allow_pickle=False), dtype=np.float32)
    records = []
    variant_dir = result_dir / "perturbations"
    variant_dir.mkdir(parents=True, exist_ok=True)
    for name, values in _perturbations(attribute, seed).items():
        grid_path = variant_dir / f"{name}.txt"
        csv_path = variant_dir / f"{name}.csv"
        write_ascii_grid(grid_path, values)
        run = run_trace(executable, grid_path, csv_path, trace_arguments)
        mask = _prediction_union(csv_path, attribute.shape)
        records.append({"variant": name, "iou_to_baseline": _mask_iou(mask, baseline_mask), "elapsed_ms": run["elapsed_ms"]})
    return {
        "case_id": case_id,
        "minimum_iou_to_baseline": min(item["iou_to_baseline"] for item in records),
        "mean_iou_to_baseline": float(np.mean([item["iou_to_baseline"] for item in records])),
        "variants": records,
    }


def _aggregate(records):
    if not records:
        raise ValueError("No benchmark records were evaluated.")
    return {
        "case_count": len(records),
        "pixel_iou_mean": float(np.mean([item["metrics"]["pixel"]["iou"] for item in records])),
        "pixel_precision_mean": float(np.mean([item["metrics"]["pixel"]["precision"] for item in records])),
        "pixel_recall_mean": float(np.mean([item["metrics"]["pixel"]["recall"] for item in records])),
        "boundary_f1_mean": float(np.mean([item["metrics"]["boundary"]["f1"] for item in records])),
        "centerline_coverage_mean": float(np.mean([item["metrics"]["centerline"]["coverage"] for item in records])),
        "centerline_corridor_precision_mean": float(np.mean([item["metrics"]["centerline"]["corridor_precision"] for item in records])),
        "centerline_corridor_f1_mean": float(np.mean([item["metrics"]["centerline"]["corridor_f1"] for item in records])),
        "merge_count_total": int(sum(item["metrics"]["objects"]["merge_count"] for item in records)),
        "merge_excess_total": int(sum(item["metrics"]["objects"].get("merge_excess", 0) for item in records)),
        "max_references_per_prediction": int(max(item["metrics"]["objects"].get("max_references_per_prediction", 0) for item in records)),
        "split_count_total": int(sum(item["metrics"]["objects"]["split_count"] for item in records)),
        "split_excess_total": int(sum(item["metrics"]["objects"].get("split_excess", 0) for item in records)),
        "max_predictions_per_reference": int(max(item["metrics"]["objects"].get("max_predictions_per_reference", 0) for item in records)),
        "missed_object_count_total": int(sum(item["metrics"]["objects"]["missed_reference_count"] for item in records)),
        "invalid_polygon_count_total": int(sum(item["metrics"]["topology"]["invalid_polygon_count"] for item in records)),
        "polygon_intersection_count_total": int(sum(item["metrics"]["topology"]["polygon_intersection_count"] for item in records)),
        "elapsed_ms_mean": float(np.mean([item["run"]["elapsed_ms"] for item in records])),
        "elapsed_ms_max": int(max(item["run"]["elapsed_ms"] for item in records)),
    }


def _evidence_opening(payload):
    benchmark_type = payload.get("benchmark_type")
    if benchmark_type == "controlled_planar_fault_attribute":
        return "本报告使用几何先生成、属性后渲染的可控平面断层属性真值，评价多形态、对象粒度和扰动稳定性。它不能替代独立真实工区及专家解释证据。"
    if benchmark_type == "real_planar_attribute_from_seismic_volume":
        return "本报告使用真实地震体计算得到的平面不连续性属性，并以同一 sample 的专家断层体素作为评价参考。标签未参与属性计算；结果同时受属性方法、专家标注范围和多边形追踪影响，不能只归因于追踪算法。"
    return "本报告属于公开真实数据辅助压力测试。数据是纵向地震剖面，专家真值是细线标注，并非赛题要求的平面断层属性和带状多边形真值；指标只能揭示跨输入域适配风险，不得作为赛题平面精度结论。"


def _write_reports(output_dir, payload):
    (output_dir / "泛化验证摘要.json").write_text(
        json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    with (output_dir / "逐案例指标.csv").open("w", encoding="utf-8-sig", newline="") as stream:
        fields = ["case_id", "family", "iou", "precision", "recall", "boundary_f1", "centerline_coverage", "centerline_corridor_precision", "centerline_corridor_f1", "merge_count", "merge_excess", "split_count", "split_excess", "missed_objects", "elapsed_ms"]
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for item in payload["cases"]:
            metrics = item["metrics"]
            writer.writerow({
                "case_id": item["case_id"], "family": item["family"],
                "iou": metrics["pixel"]["iou"], "precision": metrics["pixel"]["precision"],
                "recall": metrics["pixel"]["recall"], "boundary_f1": metrics["boundary"]["f1"],
                "centerline_coverage": metrics["centerline"]["coverage"],
                "centerline_corridor_precision": metrics["centerline"]["corridor_precision"],
                "centerline_corridor_f1": metrics["centerline"]["corridor_f1"],
                "merge_count": metrics["objects"]["merge_count"],
                "merge_excess": metrics["objects"].get("merge_excess", 0),
                "split_count": metrics["objects"]["split_count"],
                "split_excess": metrics["objects"].get("split_excess", 0),
                "missed_objects": metrics["objects"]["missed_reference_count"], "elapsed_ms": item["run"]["elapsed_ms"],
            })
    summary = payload["summary"]
    opening = _evidence_opening(payload)
    lines = [
        "# FaultPolyTrace 泛化验证报告", "",
        opening, "",
        "## 汇总", "", "| 指标 | 数值 |", "|---|---:|",
        f"| 案例数 | {summary['case_count']} |",
        f"| 像元 IoU 均值 | {summary['pixel_iou_mean']:.6f} |",
        f"| Precision 均值 | {summary['pixel_precision_mean']:.6f} |",
        f"| Recall 均值 | {summary['pixel_recall_mean']:.6f} |",
        f"| Boundary F1 均值 | {summary['boundary_f1_mean']:.6f} |",
        f"| 中心线覆盖均值 | {summary['centerline_coverage_mean']:.6f} |",
        f"| 细线走廊 Precision 均值 | {summary['centerline_corridor_precision_mean']:.6f} |",
        f"| 细线走廊 F1 均值 | {summary['centerline_corridor_f1_mean']:.6f} |",
        f"| 误合并总数 | {summary['merge_count_total']} |",
        f"| 误合并额外吞并对象数 | {summary['merge_excess_total']} |",
        f"| 过度拆分总数 | {summary['split_count_total']} |",
        f"| 过度拆分额外结果数 | {summary['split_excess_total']} |",
        f"| 漏对象总数 | {summary['missed_object_count_total']} |",
        f"| 非法多边形总数 | {summary['invalid_polygon_count_total']} |",
        f"| 多边形面积相交总数 | {summary['polygon_intersection_count_total']} |",
        f"| 平均/最大耗时 | {summary['elapsed_ms_mean']:.1f} / {summary['elapsed_ms_max']} ms |",
    ]
    if payload.get("stability"):
        minimum = min(item["minimum_iou_to_baseline"] for item in payload["stability"])
        lines.extend(["", "## 扰动稳定性", "", f"全部增益、噪声和轻微平滑扰动中的最小结果 IoU 为 `{minimum:.6f}`。"])
    lines.extend(["", "## 证据边界", "", "真实工区泛化必须另外记录数据来源、属性计算方法、固定参数和独立解释。没有标签的真实数据只能用于运行稳定性和人工质控，不得报告为真值 Precision/Recall。"])
    (output_dir / "泛化验证报告.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def run_benchmark(
    dataset_dir,
    executable,
    output_dir,
    boundary_tolerance=2.0,
    with_perturbations=False,
    centerline_tolerance=3.0,
    trace_arguments=FORMAL_TRACE_ARGUMENTS,
    reuse_existing=False,
):
    dataset_dir = Path(dataset_dir)
    executable = Path(executable)
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((dataset_dir / "benchmark_manifest.json").read_text(encoding="utf-8"))
    records = []
    stability = []
    for index, case_info in enumerate(manifest["cases"]):
        case_id = case_info["id"]
        case_dir = dataset_dir / case_info["path"]
        result_dir = output_dir / case_id
        result_dir.mkdir(parents=True, exist_ok=True)
        output_csv = result_dir / "result.csv"
        if reuse_existing and output_csv.exists():
            diagnostics_path = output_csv.with_suffix(".diagnostics.json")
            elapsed_ms = 0
            if diagnostics_path.exists():
                diagnostics = json.loads(diagnostics_path.read_text(encoding="utf-8"))
                elapsed_ms = int(diagnostics.get("results", {}).get("elapsedMs", 0))
            run = {
                "elapsed_ms": elapsed_ms,
                "command": _trace_command(executable, case_dir / "attribute.txt", output_csv, trace_arguments),
                "stdout": "",
                "stderr": "",
                "reused_existing_output": True,
            }
        else:
            run = run_trace(executable, case_dir / "attribute.txt", output_csv, trace_arguments)
        truth_archive = np.load(case_dir / "truth_masks.npz", allow_pickle=False)
        object_masks = truth_archive["object_masks"].astype(bool)
        centerline = truth_archive["centerline_mask"].astype(bool)
        truth_archive.close()
        metrics = evaluate_prediction_csv(
            output_csv,
            object_masks,
            boundary_tolerance,
            centerline,
            centerline_tolerance,
        )
        predicted = _prediction_union(output_csv, object_masks.shape[1:])
        attribute = np.asarray(np.load(case_dir / "attribute.npy", allow_pickle=False), dtype=np.float32)
        _save_review_figure(attribute, np.any(object_masks, axis=0), predicted, result_dir / "复核图.png", case_id)
        record = {"case_id": case_id, "family": case_info["family"], "metrics": metrics, "run": run}
        records.append(record)
        if with_perturbations:
            stability.append(
                _case_stability(
                    executable,
                    case_id,
                    case_dir,
                    result_dir,
                    predicted,
                    int(case_info.get("seed", 0)),
                    trace_arguments,
                )
            )
    payload = {
        "schema_version": 1,
        "evidence_level": manifest.get("evidence_level", "controlled_synthetic_or_semisynthetic_truth"),
        "benchmark_type": manifest.get("benchmark_type"),
        "direct_planar_evidence": bool(
            manifest.get("direct_planar_evidence", manifest.get("benchmark_type") == "controlled_planar_fault_attribute")
        ),
        "real_cross_survey_generalization_proven": False,
        "formal_trace_arguments": list(trace_arguments),
        "summary": _aggregate(records),
        "cases": records,
        "stability": stability,
    }
    _write_reports(output_dir, payload)
    return payload
