# -*- coding: utf-8 -*-
import argparse
import json
from pathlib import Path
import sys

from generalization_validation.runner import generate_benchmark, run_benchmark


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="生成并运行 FaultPolyTrace 平面断层泛化基准。")
    subparsers = parser.add_subparsers(dest="command", required=True)

    generate = subparsers.add_parser("generate", help="生成带独立几何真值的可控基准")
    generate.add_argument("--output", type=Path, required=True)
    generate.add_argument("--count", type=int, default=40)
    generate.add_argument("--width", type=int, default=256)
    generate.add_argument("--height", type=int, default=192)
    generate.add_argument("--seed", type=int, default=20260723)
    generate.add_argument("--background", action="append", default=[], help="可选真实背景 .npy 或 FaultPolyTrace ASCII 网格")

    run = subparsers.add_parser("run", help="用固定正式参数运行并生成统一指标报告")
    run.add_argument("--dataset", type=Path, required=True)
    run.add_argument("--exe", type=Path, default=Path("build/faultpolytrace.exe"))
    run.add_argument("--output", type=Path, required=True)
    run.add_argument("--boundary-tolerance", type=float, default=2.0)
    run.add_argument("--centerline-tolerance", type=float, default=3.0)
    run.add_argument("--with-perturbations", action="store_true")

    args = parser.parse_args()
    if args.command == "generate":
        result = generate_benchmark(args.output, args.count, args.width, args.height, args.seed, args.background)
        printable = {
            "output": str(args.output),
            "case_count": result["count"],
            "families": result["families"],
            "truth_independence": result["truth_independence"],
        }
    else:
        result = run_benchmark(
            args.dataset,
            args.exe,
            args.output,
            args.boundary_tolerance,
            args.with_perturbations,
            args.centerline_tolerance,
        )
        printable = result["summary"]
    print(json.dumps(printable, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
