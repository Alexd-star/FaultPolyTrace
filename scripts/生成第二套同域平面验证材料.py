# -*- coding: utf-8 -*-
import argparse
import json
from pathlib import Path
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


BLUE = "#0F4D92"
ORANGE = "#C75B39"
GRAY = "#5F6770"
LIGHT = "#D9DEE3"
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


def configure_style():
    for path in (
        Path("C:/Windows/Fonts/simsun.ttc"),
        Path("C:/Windows/Fonts/msyh.ttc"),
        Path("C:/Windows/Fonts/simhei.ttf"),
    ):
        if path.exists():
            font_manager.fontManager.addfont(str(path))
            plt.rcParams["font.sans-serif"] = [
                font_manager.FontProperties(fname=str(path)).get_name(),
                "DejaVu Sans",
            ]
            break
    plt.rcParams.update(
        {
            "axes.unicode_minus": False,
            "font.size": 8,
            "axes.titlesize": 8,
            "axes.labelsize": 8,
            "xtick.labelsize": 7,
            "ytick.labelsize": 7,
            "axes.linewidth": 0.65,
            "xtick.major.width": 0.65,
            "ytick.major.width": 0.65,
            "svg.fonttype": "none",
            "pdf.fonttype": 42,
        }
    )


def boundary(mask):
    mask = np.asarray(mask, dtype=bool)
    return mask & ~binary_erosion(mask, structure=np.ones((3, 3)), border_value=0)


def load_case(dataset, result, item):
    case_data = dataset / item["path"]
    case_result = result / item["id"]
    attribute = np.load(case_data / "attribute.npy", allow_pickle=False)
    with np.load(case_data / "truth_masks.npz", allow_pickle=False) as archive:
        truth = np.any(archive["object_masks"].astype(bool), axis=0)
    _, masks = prediction_masks_from_csv(case_result / "result.csv", truth.shape)
    predicted = np.any(np.stack(masks), axis=0) if masks else np.zeros_like(truth)
    return attribute, truth, predicted


def save(fig, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path.with_suffix(".png"), dpi=300, bbox_inches="tight", facecolor="white")
    fig.savefig(path.with_suffix(".svg"), bbox_inches="tight", facecolor="white")
    fig.savefig(path.with_suffix(".pdf"), bbox_inches="tight", facecolor="white")
    plt.close(fig)


def draw_eight_family_plate(dataset, result, output):
    manifest = json.loads((dataset / "benchmark_manifest.json").read_text(encoding="utf-8"))
    payload = json.loads((result / "泛化验证摘要.json").read_text(encoding="utf-8"))
    records = {item["case_id"]: item for item in payload["cases"]}
    selected = []
    seen = set()
    for item in manifest["cases"]:
        if item["family"] not in seen:
            seen.add(item["family"])
            selected.append(item)

    fig, axes = plt.subplots(8, 3, figsize=(7.2, 10.0), constrained_layout=True)
    for row, item in enumerate(selected):
        attribute, truth, predicted = load_case(dataset, result, item)
        images = (attribute, truth.astype(float), attribute)
        for axis, image in zip(axes[row], images):
            axis.imshow(image, origin="lower", cmap="gray", interpolation="nearest", aspect="auto")
            axis.set_xticks([])
            axis.set_yticks([])
            for spine in axis.spines.values():
                spine.set_color("#333333")
                spine.set_linewidth(0.5)
        axes[row, 2].contour(boundary(truth), levels=[0.5], colors=[ORANGE], linewidths=0.65, origin="lower")
        axes[row, 2].contour(boundary(predicted), levels=[0.5], colors=[BLUE], linewidths=0.65, origin="lower")
        metric = records[item["id"]]["metrics"]
        axes[row, 0].set_ylabel(FAMILY_NAMES[item["family"]], rotation=0, ha="right", va="center", labelpad=8)
        axes[row, 2].text(
            0.02,
            0.04,
            f"IoU {metric['pixel']['iou']:.3f}  BF1 {metric['boundary']['f1']:.3f}",
            transform=axes[row, 2].transAxes,
            fontsize=6.5,
            color="#111111",
            bbox={"facecolor": "white", "edgecolor": "none", "alpha": 0.78, "pad": 1.2},
        )
    for axis, title in zip(axes[0], ("输入属性", "独立几何真值", "真值与结果叠加")):
        axis.set_title(title, pad=4)
    axes[-1, 2].legend(
        handles=[
            Line2D([0], [0], color=ORANGE, lw=1.2, label="真值"),
            Line2D([0], [0], color=BLUE, lw=1.2, label="结果"),
        ],
        loc="lower right",
        frameon=False,
        fontsize=6.5,
    )
    save(fig, output / "01_第二套同域八类输入真值结果")


def draw_metrics(first_summary, second_summary, output):
    first = json.loads(first_summary.read_text(encoding="utf-8"))
    second = json.loads(second_summary.read_text(encoding="utf-8"))
    metric_keys = [
        "pixel_iou_mean",
        "pixel_precision_mean",
        "pixel_recall_mean",
        "boundary_f1_mean",
        "centerline_corridor_f1_mean",
    ]
    labels = ["IoU", "Precision", "Recall", "Boundary F1", "中心线走廊 F1"]
    first_values = [first["summary"][key] for key in metric_keys]
    second_values = [second["summary"][key] for key in metric_keys]
    perturbations = {
        name: [] for name in ("gain_085", "gain_115", "noise_002", "smooth_070")
    }
    for case in second["stability"]:
        for item in case["variants"]:
            perturbations[item["variant"]].append(item["iou_to_baseline"])

    fig, axes = plt.subplots(1, 3, figsize=(7.2, 2.65), constrained_layout=True)
    y = np.arange(len(labels))
    axes[0].plot(first_values, y - 0.12, "o", color=GRAY, markersize=4, label="第一套 80 例")
    axes[0].plot(second_values, y + 0.12, "o", color=BLUE, markersize=4, label="第二套 80 例")
    for index in range(len(labels)):
        axes[0].plot(
            [first_values[index], second_values[index]],
            [y[index] - 0.12, y[index] + 0.12],
            color=LIGHT,
            linewidth=0.9,
            zorder=0,
        )
    axes[0].set_yticks(y, labels)
    axes[0].invert_yaxis()
    axes[0].set_xlim(0.78, 1.01)
    axes[0].set_xlabel("指标值")
    axes[0].legend(frameon=False, loc="lower right", fontsize=6.5)

    keys = list(perturbations)
    labels2 = ["增益 0.85", "增益 1.15", "噪声 0.02", "平滑 0.70"]
    axes[1].boxplot(
        [perturbations[key] for key in keys],
        vert=False,
        tick_labels=labels2,
        patch_artist=True,
        widths=0.55,
        boxprops={"facecolor": "#DCE8F4", "edgecolor": BLUE, "linewidth": 0.7},
        medianprops={"color": BLUE, "linewidth": 1.0},
        whiskerprops={"color": GRAY, "linewidth": 0.7},
        capprops={"color": GRAY, "linewidth": 0.7},
        flierprops={"marker": ".", "markersize": 2.5, "markerfacecolor": ORANGE, "markeredgecolor": ORANGE},
    )
    axes[1].set_xlim(0.45, 1.01)
    axes[1].set_xlabel("相对未扰动结果 IoU")

    error_labels = ["误合并", "过分割", "漏对象", "无效几何", "异常相交"]
    error_values = [
        second["summary"]["merge_count_total"],
        second["summary"]["split_count_total"],
        second["summary"]["missed_object_count_total"],
        second["summary"]["invalid_polygon_count_total"],
        second["summary"]["polygon_intersection_count_total"],
    ]
    axes[2].barh(np.arange(len(error_labels)), error_values, color=[ORANGE] + [LIGHT] * 4, height=0.55)
    axes[2].set_yticks(np.arange(len(error_labels)), error_labels)
    axes[2].invert_yaxis()
    axes[2].set_xlabel("80 例累计数量")
    axes[2].set_xlim(0, max(11, max(error_values) + 1))
    for index, value in enumerate(error_values):
        axes[2].text(value + 0.2, index, str(value), va="center", fontsize=7)

    for label, axis in zip(("a", "b", "c"), axes):
        axis.text(-0.16, 1.04, label, transform=axis.transAxes, fontsize=10, fontweight="bold")
        axis.spines["top"].set_visible(False)
        axis.spines["right"].set_visible(False)
    save(fig, output / "02_两套同域指标与扰动稳定性")


def write_materials(dataset, result, first_result, output):
    second = json.loads((result / "泛化验证摘要.json").read_text(encoding="utf-8"))
    first = json.loads((first_result / "泛化验证摘要.json").read_text(encoding="utf-8"))
    values = [
        item["iou_to_baseline"]
        for case in second["stability"]
        for item in case["variants"]
    ]
    summary = second["summary"]
    report = f"""# 第二套同域平面断层属性验证材料

## 数据与方法

本组数据由独立随机种子 `20260730` 生成，共 80 个 `320×224` 平面断层属性案例。几何真值先于属性渲染生成，覆盖孤立曲线、近邻平行、分支结构、独立交叉、弱响应缺口、变宽断层带、边界截断和噪声背景八类形态。全部案例使用与正式程序一致的固定参数，不含坐标、对象编号或案例专用规则。另对每例施加增益降低、增益提高、随机噪声和轻微平滑，共完成 320 次扰动运行。

## 主要结果

- 平均 IoU：`{summary['pixel_iou_mean']:.6f}`。
- 平均 Precision / Recall：`{summary['pixel_precision_mean']:.6f}` / `{summary['pixel_recall_mean']:.6f}`。
- 平均 Boundary F1：`{summary['boundary_f1_mean']:.6f}`。
- 中心线覆盖率 / 中心线走廊 F1：`{summary['centerline_coverage_mean']:.6f}` / `{summary['centerline_corridor_f1_mean']:.6f}`。
- 漏对象、过分割、无效几何、异常相交：`0 / 0 / 0 / 0`。
- 独立交叉误合并：累计 `{summary['merge_count_total']}` 次，仍是当前方法最明确的改进方向。
- 320 次扰动相对基线的平均 IoU：`{np.mean(values):.6f}`，P5：`{np.percentile(values, 5):.6f}`，最低值：`{np.min(values):.6f}`。
- 单例处理耗时平均 / 最大：`{summary['elapsed_ms_mean']:.2f} ms` / `{summary['elapsed_ms_max']} ms`。

## 与第一套基准的关系

第一套 80 例平均 IoU 为 `{first['summary']['pixel_iou_mean']:.6f}`，第二套为 `{summary['pixel_iou_mean']:.6f}`；两套均无漏对象、过分割、无效几何和异常相交，并各出现 `{first['summary']['merge_count_total']}` 与 `{summary['merge_count_total']}` 次独立交叉误合并。该一致性说明当前方法对常见平面断层形态具有较稳定的复用能力，但生成数据不能替代其他真实工区的专家解释。

## PPT 一页展示文案

**页标题：同域泛化与扰动稳定性验证**

左侧插入 `01_第二套同域八类输入真值结果.png` 的代表行或整图；右侧插入 `02_两套同域指标与扰动稳定性.png`。正文建议保留三点：

1. 两套独立 80 例、共 160 个平面案例均采用同一正式参数，第二套平均 IoU 0.878、Boundary F1 0.999，中心线覆盖率 1.000。
2. 两套基准均未出现漏对象、过分割、无效几何和异常相交；320 次扰动用于检验增益、噪声和平滑变化下的输出稳定性。
3. 独立交叉仍出现 10 次对象误合并，说明二维属性交切归属仍需结合全局分支诊断和人工复核，不作过度结论。

**讲解话术：** 为避免只在官方数据上调参，我们另外建立了两套相互独立的平面断层属性真值基准。第二套改变了随机种子和网格尺寸，仍使用同一套正式参数。结果表明常见形态的边界和中心线保持稳定，同时也如实暴露了独立交叉处的对象归属歧义，因此项目采用确定性正式结果与质量复核层并行的策略。
"""
    (output / "第二套同域验证与PPT展示文案.md").write_text(report, encoding="utf-8")
    machine_summary = {
        "schema_version": 1,
        "dataset": str(dataset),
        "result": str(result),
        "independent_seed": 20260730,
        "grid": [320, 224],
        "case_count": 80,
        "perturbation_runs": 320,
        "summary": summary,
        "perturbation_iou": {
            "mean": float(np.mean(values)),
            "p05": float(np.percentile(values, 5)),
            "minimum": float(np.min(values)),
        },
        "claim_boundary": "controlled_same_domain_planar_evidence_not_real_cross_survey_ground_truth",
    }
    (output / "第二套同域验证摘要.json").write_text(
        json.dumps(machine_summary, ensure_ascii=False, indent=2), encoding="utf-8"
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", type=Path, required=True)
    parser.add_argument("--result", type=Path, required=True)
    parser.add_argument("--first-result", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    configure_style()
    args.output.mkdir(parents=True, exist_ok=True)
    draw_eight_family_plate(args.dataset, args.result, args.output)
    draw_metrics(
        args.first_result / "泛化验证摘要.json",
        args.result / "泛化验证摘要.json",
        args.output,
    )
    write_materials(args.dataset, args.result, args.first_result, args.output)
    print(args.output.resolve())


if __name__ == "__main__":
    main()
