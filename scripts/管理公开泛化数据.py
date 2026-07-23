# -*- coding: utf-8 -*-
import argparse
import json
from pathlib import Path
import sys

from generalization_validation.cracks import prepare_cracks_auxiliary
from generalization_validation.registry import (
    dataverse_file_inventory,
    download_dataset,
    download_dataverse_files,
    load_registry,
)


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="管理公开泛化验证数据的来源、下载和本地校验。")
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("list", help="列出公开数据来源及用途边界")
    download = subparsers.add_parser("download", help="下载支持自动获取的数据集")
    download.add_argument("--dataset", required=True)
    download.add_argument("--output", type=Path, default=Path("data/external"))
    download.add_argument("--include", action="append", default=[])
    inspect = subparsers.add_parser("inspect-thebe", help="读取 Thebe Dataverse 文件级清单，不下载大文件")
    inspect.add_argument("--output", type=Path, default=Path("data/external/thebe"))
    selective = subparsers.add_parser("download-thebe", help="按文件名选择性下载 Thebe 文件")
    selective.add_argument("--file", action="append", required=True)
    selective.add_argument("--output", type=Path, default=Path("data/external/thebe"))
    prepare = subparsers.add_parser("prepare-cracks", help="生成 CRACKS 垂直剖面辅助基准")
    prepare.add_argument("--source", type=Path, default=Path("data/external/cracks"))
    prepare.add_argument("--output", type=Path, default=Path("data/processed/cracks_auxiliary"))
    prepare.add_argument("--annotator", default="expert")
    prepare.add_argument("--limit", type=int)
    args = parser.parse_args()

    registry = load_registry()
    if args.command == "list":
        for item in registry["datasets"]:
            print(f"{item['id']}: {item['name']}")
            print(f"  真实数据: {'是' if item['is_real'] else '否'}")
            print(f"  标签: {'有' if item['has_fault_labels'] else '无'}")
            print(f"  平面验证角色: {item['planar_validation_role']}")
            print(f"  来源: {item['source_url']}")
            print(f"  边界: {item['usage_boundary']}")
        return 0
    if args.command == "inspect-thebe":
        args.output.mkdir(parents=True, exist_ok=True)
        manifest = dataverse_file_inventory("doi:10.7910/DVN/YBYGBK")
        (args.output / "dataverse_inventory.json").write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8"
        )
    elif args.command == "download-thebe":
        manifest = download_dataverse_files("doi:10.7910/DVN/YBYGBK", args.file, args.output)
    elif args.command == "prepare-cracks":
        manifest = prepare_cracks_auxiliary(
            args.source / "images.zip",
            args.source / "Fault segmentations.zip",
            args.output,
            annotator=args.annotator,
            limit=args.limit,
        )
    else:
        manifest = download_dataset(args.dataset, args.output, include=args.include, registry=registry)
    print(json.dumps(manifest, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
