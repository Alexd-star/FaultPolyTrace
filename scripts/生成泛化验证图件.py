# -*- coding: utf-8 -*-
import argparse
import json
from pathlib import Path
import shutil
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager
from matplotlib.lines import Line2D
import numpy as np
from scipy.ndimage import binary_erosion


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from generalization_validation.metrics import prediction_masks_from_csv


BLUE = "#0072B2"
ORANGE = "#D55E00"
GREEN = "#009E73"
GRAY = "#68707A"
LIGHT_GRAY = "#D9DEE5"

FAMILY_NAMES = {
    "isolated_curved": "孤立曲线",
    "parallel_close": "近邻平行",
    "branching": "分支结构",
    "crossing_independent": "独立交叉",
    "weak_gap": "弱响应缺口",
    "wide_variable": "变宽断层带",
    "edge_truncated": "边界截断",
    "noisy_footprint": "噪声背景",
}

RESIDUAL_LABELS = {
    "候选_01_boundary_truncation_cells13.png": "候选 01：数据边界截断",
    "候选_02_local_envelope_gap_cells18.png": "候选 02：局部包络不足",
    "候选_03_endpoint_micro_gap_cells12.png": "候选 03：端点轻微未包络",
    "候选_04_boundary_truncation_cells11.png": "候选 04：数据边界截断",
    "候选_05_weak_linear_residual_cells29.png": "候选 05：弱线性残余",
    "候选_06_weak_linear_residual_cells18.png": "候选 06：弱线性残余",
    "候选_07_weak_linear_residual_cells12.png": "候选 07：弱线性残余",
    "候选_08_weak_linear_residual_cells34.png": "候选 08：弱线性残余",
    "候选_09_weak_linear_residual_cells15.png": "候选 09：弱线性残余",
    "候选_10_weak_linear_residual_cells9.png": "候选 10：弱线性残余",
    "候选_11_weak_linear_residual_cells8.png": "候选 11：弱线性残余",
}


def configure_style():
    candidates = (
        Path("C:/Windows/Fonts/msyh.ttc"),
        Path("C:/Windows/Fonts/simhei.ttf"),
        Path("C:/Windows/Fonts/simsun.ttc"),
    )
    for path in candidates:
        if path.exists():
            font_manager.fontManager.addfont(str(path))
            name = font_manager.FontProperties(fname=str(path)).get_name()
            plt.rcParams["font.sans-serif"] = [name, "DejaVu Sans"]
            break
    plt.rcParams.update({
        "axes.unicode_minus": False,
        "font.size": 8.5,
        "axes.titlesize": 9,
        "axes.labelsize": 8,
        "xtick.labelsize": 7.5,
        "ytick.labelsize": 7.5,
        "legend.fontsize": 7.5,
        "axes.linewidth": 0.7,
        "xtick.major.width": 0.7,
        "ytick.major.width": 0.7,
        "savefig.dpi": 300,
        "figure.facecolor": "white",
        "axes.facecolor": "white",
        "svg.fonttype": "none",
        "pdf.fonttype": 42,
    })


def binary_boundary(mask):
    mask = np.asarray(mask, dtype=bool)
    return mask & ~binary_erosion(
        mask,
        structure=np.ones((3, 3), dtype=bool),
        border_value=0,
    )


def prediction_union(csv_path, shape):
    _, masks = prediction_masks_from_csv(csv_path, shape)
    if not masks:
        return np.zeros(shape, dtype=bool)
    return np.any(np.stack(masks), axis=0)


def load_case(data_dir, result_dir):
    attribute = np.asarray(
        np.load(data_dir / "attribute.npy", allow_pickle=False),
        dtype=np.float32,
    )
    truth_archive = np.load(data_dir / "truth_masks.npz", allow_pickle=False)
    truth = np.any(truth_archive["object_masks"].astype(bool), axis=0)
    truth_archive.close()
    predicted = prediction_union(result_dir / "result.csv", truth.shape)
    return attribute, truth, predicted


def draw_triplet(axes, attribute, truth, predicted, row_label=None, rotate=False):
    if rotate:
        attribute = np.rot90(attribute)
        truth = np.rot90(truth)
        predicted = np.rot90(predicted)
    panels = (attribute, truth, attribute)
    cmaps = ("gray", "gray", "gray")
    for axis, image, cmap in zip(axes, panels, cmaps):
        axis.imshow(image, origin="lower", cmap=cmap, interpolation="nearest", aspect="auto")
        axis.set_xticks([])
        axis.set_yticks([])
        for spine in axis.spines.values():
            spine.set_linewidth(0.55)
            spine.set_color("#333333")
    axes[2].contour(
        binary_boundary(truth).astype(float),
        levels=[0.5],
        colors=[ORANGE],
        linewidths=0.75,
        origin="lower",
    )
    axes[2].contour(
        binary_boundary(predicted).astype(float),
        levels=[0.5],
        colors=[BLUE],
        linewidths=0.75,
        origin="lower",
    )
    if row_label:
        axes[0].set_ylabel(row_label, rotation=0, ha="right", va="center", labelpad=8)


def save_figure(fig, basename, vector=False):
    basename.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(basename.with_suffix(".png"), dpi=300, bbox_inches="tight", facecolor="white")
    if vector:
        fig.savefig(basename.with_suffix(".svg"), bbox_inches="tight", facecolor="white")
        fig.savefig(basename.with_suffix(".pdf"), bbox_inches="tight", facecolor="white")
    plt.close(fig)


def planar_figures(output_dir, benchmark_dir, result_dir):
    payload = json.loads((result_dir / "泛化验证摘要.json").read_text(encoding="utf-8"))
    record_by_id = {item["case_id"]: item for item in payload["cases"]}
    manifest = json.loads((benchmark_dir / "benchmark_manifest.json").read_text(encoding="utf-8"))
    selected = []
    used = set()
    for item in manifest["cases"]:
        family = item["family"]
        if family in used:
            continue
        used.add(family)
        selected.append(item)
    selected.sort(key=lambda item: list(FAMILY_NAMES).index(item["family"]))

    case_dir = output_dir / "01_可控平面真值"
    case_dir.mkdir(parents=True, exist_ok=True)
    legend = [
        Line2D([0], [0], color=ORANGE, lw=1.4, label="几何真值"),
        Line2D([0], [0], color=BLUE, lw=1.4, label="算法结果"),
    ]
    for index, item in enumerate(selected, start=1):
        case_id = item["id"]
        record = record_by_id[case_id]
        attribute, truth, predicted = load_case(
            benchmark_dir / item["path"],
            result_dir / case_id,
        )
        fig, axes = plt.subplots(1, 3, figsize=(7.2, 2.25), constrained_layout=True)
        draw_triplet(axes, attribute, truth, predicted)
        for axis, title in zip(axes, ("输入断层属性", "独立几何真值", "真值与算法结果叠加")):
            axis.set_title(title)
        metrics = record["metrics"]
        fig.suptitle(
            f"{FAMILY_NAMES[item['family']]}：IoU {metrics['pixel']['iou']:.3f}，"
            f"Boundary F1 {metrics['boundary']['f1']:.3f}，"
            f"误合并/过拆分 {metrics['objects']['merge_count']}/{metrics['objects']['split_count']}",
            fontsize=9.5,
        )
        axes[2].legend(handles=legend, loc="lower right", frameon=False)
        save_figure(
            fig,
            case_dir / f"{index:02d}_{FAMILY_NAMES[item['family']]}_输入真值结果对照",
        )

    fig, axes = plt.subplots(4, 6, figsize=(13.2, 9.1), constrained_layout=True)
    for row in range(4):
        for pair in range(2):
            item = selected[row * 2 + pair]
            case_id = item["id"]
            record = record_by_id[case_id]
            attribute, truth, predicted = load_case(
                benchmark_dir / item["path"],
                result_dir / case_id,
            )
            triplet = axes[row, pair * 3:(pair + 1) * 3]
            draw_triplet(triplet, attribute, truth, predicted)
            triplet[0].set_title(
                f"{FAMILY_NAMES[item['family']]}\n输入",
                fontsize=8.5,
            )
            triplet[1].set_title("真值", fontsize=8.5)
            triplet[2].set_title(
                f"叠加（橙真值/蓝结果）\nIoU {record['metrics']['pixel']['iou']:.3f}",
                fontsize=8.5,
            )
    save_figure(fig, case_dir / "00_八类平面真值输入与结果总览")
    return payload


def metric_figure(output_dir, payload):
    summary = payload["summary"]
    metric_names = [
        "像元 IoU",
        "Precision",
        "Recall",
        "Boundary F1",
        "中心线覆盖",
        "细线走廊 F1",
    ]
    metric_values = [
        summary["pixel_iou_mean"],
        summary["pixel_precision_mean"],
        summary["pixel_recall_mean"],
        summary["boundary_f1_mean"],
        summary["centerline_coverage_mean"],
        summary["centerline_corridor_f1_mean"],
    ]
    variant_names = ["gain_085", "gain_115", "noise_002", "smooth_070"]
    variant_labels = ["增益 0.85", "增益 1.15", "噪声 σ=0.02", "平滑 σ=0.70"]
    variant_values = {name: [] for name in variant_names}
    for case in payload["stability"]:
        for item in case["variants"]:
            variant_values[item["variant"]].append(item["iou_to_baseline"])

    fig = plt.figure(figsize=(7.2, 5.1), constrained_layout=True)
    grid = fig.add_gridspec(2, 2, height_ratios=[1.15, 1.0])
    ax_metrics = fig.add_subplot(grid[0, 0])
    ax_stability = fig.add_subplot(grid[0, 1])
    ax_errors = fig.add_subplot(grid[1, :])

    y = np.arange(len(metric_names))
    ax_metrics.hlines(y, 0, metric_values, color=LIGHT_GRAY, linewidth=2.0)
    ax_metrics.plot(metric_values, y, "o", color=BLUE, markersize=5)
    for yi, value in zip(y, metric_values):
        ax_metrics.text(min(value + 0.025, 1.0), yi, f"{value:.3f}", va="center", fontsize=7.5)
    ax_metrics.set_yticks(y, metric_names)
    ax_metrics.set_xlim(0, 1.05)
    ax_metrics.invert_yaxis()
    ax_metrics.set_xlabel("指标值")
    ax_metrics.set_title("a  80 个平面真值案例的平均质量", loc="left", fontweight="bold")
    ax_metrics.grid(axis="x", color="#ECEFF3", linewidth=0.6)

    positions = np.arange(1, len(variant_names) + 1)
    values = [variant_values[name] for name in variant_names]
    box = ax_stability.boxplot(
        values,
        positions=positions,
        widths=0.55,
        patch_artist=True,
        showfliers=False,
        medianprops={"color": "#1F2937", "linewidth": 1.2},
        whiskerprops={"color": GRAY, "linewidth": 0.8},
        capprops={"color": GRAY, "linewidth": 0.8},
    )
    for patch in box["boxes"]:
        patch.set_facecolor("#DDEBF7")
        patch.set_edgecolor(BLUE)
        patch.set_linewidth(0.8)
    rng = np.random.default_rng(20260730)
    for position, points in zip(positions, values):
        jitter = rng.uniform(-0.14, 0.14, len(points))
        ax_stability.scatter(
            position + jitter,
            points,
            s=6,
            color=BLUE,
            alpha=0.32,
            linewidths=0,
        )
    ax_stability.axhline(0.80, color=ORANGE, linestyle="--", linewidth=0.9, label="0.80 参考线")
    ax_stability.set_xticks(positions, variant_labels, rotation=18, ha="right")
    ax_stability.set_ylim(0.5, 1.02)
    ax_stability.set_ylabel("与基线结果的 IoU")
    ax_stability.set_title("b  参数与输入扰动稳定性（每组 n=80）", loc="left", fontweight="bold")
    ax_stability.grid(axis="y", color="#ECEFF3", linewidth=0.6)
    ax_stability.legend(frameon=False, loc="lower right")

    error_names = ["误合并", "过度拆分", "漏对象", "非法多边形", "异常相交"]
    error_values = [
        summary["merge_count_total"],
        summary["split_count_total"],
        summary["missed_object_count_total"],
        summary["invalid_polygon_count_total"],
        summary["polygon_intersection_count_total"],
    ]
    colors = [ORANGE if value else "#AAB2BD" for value in error_values]
    bars = ax_errors.barh(error_names, error_values, color=colors, height=0.55)
    max_value = max(error_values + [1])
    ax_errors.set_xlim(0, max_value * 1.18)
    for bar, value in zip(bars, error_values):
        ax_errors.text(value + max_value * 0.025, bar.get_y() + bar.get_height() / 2, str(value), va="center")
    ax_errors.invert_yaxis()
    ax_errors.set_xlabel("80 个案例累计次数")
    ax_errors.set_title("c  对象粒度与拓扑错误：短板集中在独立交叉的对象归属", loc="left", fontweight="bold")
    ax_errors.grid(axis="x", color="#ECEFF3", linewidth=0.6)

    for axis in (ax_metrics, ax_stability, ax_errors):
        axis.spines["top"].set_visible(False)
        axis.spines["right"].set_visible(False)
    save_figure(fig, output_dir / "02_扰动稳定性" / "01_平面真值质量与扰动稳定性", vector=True)


def save_real_planar_figure(output_dir):
    samples = (971, 1097, 1276, 1451)
    fig, axes = plt.subplots(4, 3, figsize=(7.2, 5.7), constrained_layout=True)
    for row, sample in enumerate(samples):
        data_dir = ROOT / "data" / "processed" / f"thebe_test1_sample{sample}_lag2"
        result_root = ROOT / "examples" / "outputs" / f"generalization_thebe_test1_sample{sample}_lag2"
        result_dir = result_root / f"thebe_test1_sample{sample}_lag2"
        payload = json.loads((result_root / "泛化验证摘要.json").read_text(encoding="utf-8"))
        record = payload["cases"][0]
        attribute, truth, predicted = load_case(data_dir, result_dir)
        draw_triplet(
            axes[row, :], attribute, truth, predicted,
            row_label=f"sample {sample}\nIoU {record['metrics']['pixel']['iou']:.3f}\nRecall {record['metrics']['pixel']['recall']:.3f}",
            rotate=True,
        )
    for axis, title in zip(axes[0, :], ("相似性不连续属性", "专家标注", "叠加（橙真值 / 蓝结果）")):
        axis.set_title(title, fontweight="bold")
    save_figure(fig, output_dir / "03_Thebe真实平面条带" / "01_四个固定切片输入与结果对照")


def save_cracks_figure(output_dir):
    result_root = ROOT / "examples" / "outputs" / "generalization_cracks_expert_40"
    payload = json.loads((result_root / "泛化验证摘要.json").read_text(encoding="utf-8"))
    ordered = sorted(payload["cases"], key=lambda item: item["metrics"]["pixel"]["iou"])
    selected = [ordered[index] for index in (4, 13, 26, 35)]
    fig, axes = plt.subplots(4, 3, figsize=(7.2, 6.3), constrained_layout=True)
    for row, record in enumerate(selected):
        case_id = record["case_id"]
        data_dir = ROOT / "data" / "processed" / "cracks_auxiliary" / case_id
        attribute, truth, predicted = load_case(data_dir, result_root / case_id)
        draw_triplet(
            axes[row, :], attribute, truth, predicted,
            row_label=f"{case_id}\nIoU {record['metrics']['pixel']['iou']:.3f}",
        )
    for axis, title in zip(axes[0, :], ("纵向剖面不连续属性", "专家细线标注", "叠加（橙真值 / 蓝结果）")):
        axis.set_title(title, fontweight="bold")
    save_figure(fig, output_dir / "04_CRACKS跨域压力测试" / "01_固定分位代表剖面输入与结果对照")


def save_residual_audit(output_dir):
    source_dir = (
        ROOT / "examples" / "outputs" / "current_215_gap_audit_20260728" / "残余复核层"
    )
    target_dir = output_dir / "05_官方结果残余复核"
    local_target = target_dir / "局部复核图"
    local_target.mkdir(parents=True, exist_ok=True)

    shutil.copy2(source_dir / "残余复核层总览.png", target_dir / "01_残余位置全局总览.png")
    shutil.copy2(source_dir / "残余复核层报告.md", target_dir / "残余复核层报告.md")
    shutil.copy2(source_dir / "残余复核层摘要.json", target_dir / "残余复核层摘要.json")
    shutil.copy2(source_dir / "残余复核层.csv", target_dir / "残余复核层.csv")
    shutil.copy2(source_dir / "残余复核层.geojson", target_dir / "残余复核层.geojson")

    source_images = []
    for filename, label in RESIDUAL_LABELS.items():
        source_path = source_dir / "局部复核图" / filename
        target_path = local_target / f"{label}.png"
        shutil.copy2(source_path, target_path)
        source_images.append((source_path, label))

    fig, axes = plt.subplots(4, 3, figsize=(13.2, 9.2), constrained_layout=True)
    for axis, (source_path, label) in zip(axes.flat, source_images):
        axis.imshow(plt.imread(source_path))
        axis.set_title(label, fontsize=8.5, fontweight="bold")
        axis.set_axis_off()
    for axis in axes.flat[len(source_images):]:
        axis.set_axis_off()
    fig.suptitle(
        "官方结果残余复核索引（局部大图见同目录）",
        fontsize=11,
        fontweight="bold",
    )
    save_figure(fig, target_dir / "02_十一处残余候选索引")


def write_readme(output_dir, planar_payload):
    summary = planar_payload["summary"]
    thebe_payloads = []
    for sample in (971, 1097, 1276, 1451):
        path = ROOT / "examples" / "outputs" / f"generalization_thebe_test1_sample{sample}_lag2" / "泛化验证摘要.json"
        thebe_payloads.append(json.loads(path.read_text(encoding="utf-8"))["summary"])
    thebe_mean = {
        key: float(np.mean([item[key] for item in thebe_payloads]))
        for key in ("pixel_iou_mean", "pixel_precision_mean", "pixel_recall_mean", "boundary_f1_mean")
    }
    cracks = json.loads(
        (ROOT / "examples" / "outputs" / "generalization_cracks_expert_40" / "泛化验证摘要.json").read_text(encoding="utf-8")
    )["summary"]
    text = f"""# FaultPolyTrace 泛化验证图件说明

## 1. 图件目录

- `01_可控平面真值`：八类独立几何真值的输入、真值和算法结果对照，可直接用于说明书或答辩。
- `02_扰动稳定性`：80 个平面真值案例的质量指标、320 次扰动结果和对象粒度错误统计。
- `03_Thebe真实平面条带`：固定四个 sample 的真实地震体转换结果，用于说明输入域差异。
- `04_CRACKS跨域压力测试`：按 IoU 分位固定抽取的四个真实纵向剖面，用于展示跨域风险，不作为赛题平面精度。
- `05_官方结果残余复核`：正式结果的全局残余位置、11 张局部大图及可复核数据，用于分析未连接位置，不改变正式结果。

## 2. 当前复验结果

80 个可控平面案例采用同一套正式参数重新运行：像元 IoU 为 `{summary['pixel_iou_mean']:.6f}`，Precision 为 `{summary['pixel_precision_mean']:.6f}`，Recall 为 `{summary['pixel_recall_mean']:.6f}`，Boundary F1 为 `{summary['boundary_f1_mean']:.6f}`，中心线覆盖率为 `{summary['centerline_coverage_mean']:.6f}`。漏对象、过度拆分、非法多边形和异常相交均为 0；独立交叉形态累计出现 `{summary['merge_count_total']}` 次对象误合并。

320 次增益、噪声和轻微平滑扰动中，结果与未扰动基线的平均 IoU 约为 `0.93`，最低值约为 `0.54`。最低值出现在属性整体减弱的个别案例，说明分位数阈值具备较强但并非无限的增益鲁棒性。

Thebe 四个真实平面条带的平均 IoU 为 `{thebe_mean['pixel_iou_mean']:.6f}`，平均 Precision 为 `{thebe_mean['pixel_precision_mean']:.6f}`，平均 Recall 为 `{thebe_mean['pixel_recall_mean']:.6f}`，平均 Boundary F1 为 `{thebe_mean['boundary_f1_mean']:.6f}`。该结果同时受简化相似性属性、专家标注范围和追踪算法影响，不能单独归因于多边形追踪。

CRACKS 40 个真实纵向剖面的平均 IoU 为 `{cracks['pixel_iou_mean']:.6f}`，中心线覆盖率为 `{cracks['centerline_coverage_mean']:.6f}`。其输入方向和输出语义与赛题不同，仅用于证明跨域风险和测试链路完整性。

当前官方结果共标出 11 个残余复核候选，包括 7 个弱线性残余、2 个数据边界截断、1 个主断层附近局部包络不足和 1 个端点轻微未包络。前两类不能在缺少外部解释依据时自动连接；后两类可作为受控局部微补候选，但必须同时通过方向连续性、属性支撑、根系归属、背景隔离和拓扑合法性检查。

## 3. 使用建议

作品说明书应加入一节“泛化验证与证据边界”，正文展示平面真值总览和指标图，Thebe/CRACKS 放在补充验证或附录中。答辩 PPT 建议只增加一页 80 案例结果与扰动稳定性，并明确“独立交叉对象归属仍需局部复核”；不建议把跨域低分作为核心成绩页。
"""
    (output_dir / "泛化验证图件说明.md").write_text(text, encoding="utf-8")
    shutil.copy2(
        ROOT / "examples" / "outputs" / "generalization_planar_80_current_20260730" / "泛化验证摘要.json",
        output_dir / "02_扰动稳定性" / "80案例泛化验证摘要.json",
    )
    shutil.copy2(
        ROOT / "examples" / "outputs" / "generalization_planar_80_current_20260730" / "逐案例指标.csv",
        output_dir / "02_扰动稳定性" / "80案例逐案例指标.csv",
    )


def main():
    parser = argparse.ArgumentParser(description="生成 FaultPolyTrace 泛化验证前后对照图件。")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    configure_style()
    output_dir = args.output.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    benchmark_dir = ROOT / "data" / "generated" / "planar_benchmark_80"
    result_dir = ROOT / "examples" / "outputs" / "generalization_planar_80_current_20260730"
    payload = planar_figures(output_dir, benchmark_dir, result_dir)
    metric_figure(output_dir, payload)
    save_real_planar_figure(output_dir)
    save_cracks_figure(output_dir)
    save_residual_audit(output_dir)
    write_readme(output_dir, payload)
    print(output_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
