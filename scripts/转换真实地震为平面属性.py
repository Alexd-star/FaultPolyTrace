# -*- coding: utf-8 -*-
import argparse
import hashlib
import json
from pathlib import Path
import sys

import numpy as np
from PIL import Image

from generalization_validation.attributes import extract_label_slice, planar_similarity_attribute
from generalization_validation.io import load_numpy_array, write_ascii_grid
from generalization_validation.metrics import connected_component_masks
from generalization_validation.synthetic import object_masks_geojson


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def _sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def convert(args):
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    seismic = load_numpy_array(args.seismic, args.seismic_key, staging_dir=args.staging_dir)
    attribute = planar_similarity_attribute(
        seismic,
        sample_index=args.sample_index,
        half_window=args.half_window,
        stepout=args.stepout,
        axis_order=args.axis_order,
        smoothing_sigma=args.smoothing_sigma,
        max_vertical_lag=args.max_vertical_lag,
    )
    write_ascii_grid(output / "attribute.txt", attribute, args.sx, args.sy, args.dx, args.dy)
    np.save(output / "attribute.npy", attribute, allow_pickle=False)
    Image.fromarray((attribute * 255.0).astype(np.uint8)).save(output / "attribute.png")
    metadata = {
        "schema_version": 1,
        "source_type": "real_field_seismic_volume",
        "seismic_source_name": Path(args.seismic).name,
        "seismic_sha256": _sha256(args.seismic),
        "seismic_key": args.seismic_key,
        "axis_order": args.axis_order,
        "sample_index": args.sample_index,
        "half_window": args.half_window,
        "stepout": args.stepout,
        "smoothing_sigma": args.smoothing_sigma,
        "max_vertical_lag": args.max_vertical_lag,
        "attribute_method": "lag_compensated_minimum_adjacent_trace_similarity_discontinuity",
        "label_used_as_input": False,
        "disk_staging_enabled": args.staging_dir is not None,
        "grid": {"sx": args.sx, "sy": args.sy, "dx": args.dx, "dy": args.dy},
    }
    if args.label:
        labels = load_numpy_array(args.label, args.label_key, staging_dir=args.staging_dir)
        truth = extract_label_slice(labels, args.sample_index, args.label_axis_order or args.axis_order, args.label_threshold)
        masks = connected_component_masks(truth, args.minimum_label_component_cells)
        if masks.shape[0] == 0:
            raise ValueError("The selected label slice has no reference object after component filtering.")
        if masks.shape[1:] != attribute.shape:
            raise ValueError(f"Label slice shape {masks.shape[1:]} does not match attribute {attribute.shape}.")
        np.savez_compressed(output / "truth_masks.npz", object_masks=masks.astype(np.uint8), centerline_mask=truth.astype(np.uint8))
        Image.fromarray((truth * 255).astype(np.uint8)).save(output / "truth_mask.png")
        (output / "truth.geojson").write_text(
            json.dumps(object_masks_geojson(masks), ensure_ascii=False, indent=2), encoding="utf-8"
        )
        metadata.update({
            "label_source_name": Path(args.label).name,
            "label_sha256": _sha256(args.label),
            "label_key": args.label_key,
            "label_axis_order": args.label_axis_order or args.axis_order,
            "label_threshold": args.label_threshold,
            "label_role": "evaluation_only",
            "reference_object_count": int(masks.shape[0]),
            "minimum_label_component_cells": args.minimum_label_component_cells,
        })
        benchmark = {
            "schema_version": 1,
            "benchmark_type": "real_planar_attribute_from_seismic_volume",
            "evidence_level": "real_field_planar_with_expert_slice",
            "real_data_claim": True,
            "direct_planar_evidence": True,
            "truth_semantics": "expert_fault_voxels_on_the_same_sample_slice",
            "count": 1,
            "cases": [
                {
                    "id": output.name,
                    "path": ".",
                    "family": "real_planar_attribute_slice",
                    "sample_index": args.sample_index,
                    "attribute_method": metadata["attribute_method"],
                    "label_role": "evaluation_only",
                    "object_count": int(masks.shape[0]),
                }
            ],
        }
        (output / "benchmark_manifest.json").write_text(
            json.dumps(benchmark, ensure_ascii=False, indent=2), encoding="utf-8"
        )
    (output / "conversion_manifest.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return metadata


def build_parser():
    parser = argparse.ArgumentParser(description="从真实三维地震体计算二维平面断层不连续属性。")
    parser.add_argument("--seismic", type=Path, required=True, help=".npy 或 .npz 三维地震体")
    parser.add_argument("--seismic-key")
    parser.add_argument("--staging-dir", type=Path, help="可选 NPZ 流式解压缓存目录；大体积数据建议设置")
    parser.add_argument("--axis-order", default="xys", help="输入轴顺序，例如 xys、sxy")
    parser.add_argument("--sample-index", type=int, required=True)
    parser.add_argument("--half-window", type=int, default=5)
    parser.add_argument("--stepout", type=int, default=1)
    parser.add_argument("--smoothing-sigma", type=float, default=0.65)
    parser.add_argument("--max-vertical-lag", type=int, default=2, help="邻道最佳匹配的最大垂向样点偏移")
    parser.add_argument("--label", type=Path, help="可选专家标签体，仅用于评价")
    parser.add_argument("--label-key")
    parser.add_argument("--label-axis-order")
    parser.add_argument("--label-threshold", type=float, default=0.5)
    parser.add_argument("--minimum-label-component-cells", type=int, default=3)
    parser.add_argument("--sx", type=float, default=0.0)
    parser.add_argument("--sy", type=float, default=0.0)
    parser.add_argument("--dx", type=float, default=1.0)
    parser.add_argument("--dy", type=float, default=1.0)
    parser.add_argument("--output", type=Path, required=True)
    return parser


if __name__ == "__main__":
    options = build_parser().parse_args()
    result = convert(options)
    print(json.dumps(result, ensure_ascii=False, indent=2))
