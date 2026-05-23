import argparse
import json
import os
import subprocess
import time
from datetime import datetime


def run_once(command):
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    peak_memory = 0
    samples = []
    try:
        import psutil

        ps = psutil.Process(process.pid)
        while process.poll() is None:
            try:
                rss = ps.memory_info().rss
                peak_memory = max(peak_memory, rss)
                samples.append(rss)
            except psutil.Error:
                pass
            time.sleep(0.05)
    except Exception:
        process.wait()

    output = process.stdout.read() if process.stdout else ""
    return process.returncode, output, peak_memory, samples


def load_json(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        return json.load(f)


def fmt_mb(bytes_value):
    if not bytes_value:
        return "未采集"
    return f"{bytes_value / 1024 / 1024:.2f} MB"


def main():
    parser = argparse.ArgumentParser(description="生成官方赛题7性能与内存报告。")
    parser.add_argument("--exe", default="build/faultpolytrace.exe")
    parser.add_argument("--dat", required=True)
    parser.add_argument("--output-prefix", default="examples/outputs/official/perf_test7")
    parser.add_argument("--report", default="docs/赛题7性能与内存报告.md")
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--smooth", action="store_true")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output_prefix), exist_ok=True)
    os.makedirs(os.path.dirname(args.report), exist_ok=True)

    rows = []
    for i in range(1, args.runs + 1):
        output_csv = f"{args.output_prefix}_run{i}.csv"
        command = [
            args.exe,
            "--input",
            args.dat,
            "--input-format",
            "geoeast-dat",
            "--output",
            output_csv,
            "--trace-mode",
            "ridge-belt",
            "--threshold-mode",
            "percentile",
            "--percentile",
            "0.98",
            "--min-area",
            "120",
            "--close-radius",
            "1",
            "--weak-percentile",
            "0.965",
            "--weak-connect-radius",
            "1",
            "--max-weak-component-cells",
            "20000",
            "--min-weak-component-aspect",
            "3",
            "--belt-radius",
            "3",
            "--attribute-constrained-belt",
            "--attribute-envelope-radius",
            "1",
            "--post-belt-close-radius",
            "1",
            "--min-branch-length",
            "4",
            "--min-junction-branch-length",
            "2",
            "--min-junction-branch-mean",
            "0.94",
            "--max-gap-distance",
            "12",
            "--max-gap-angle",
            "55",
            "--min-gap-support-ratio",
            "0.25",
            "--no-separate-branch-belts",
            "--root-anti-merge",
            "--root-anti-merge-support-ratio",
            "0.25",
            "--root-anti-merge-min-part-cells",
            "0",
            "--root-anti-merge-min-part-ratio",
            "0.12",
            "--root-anti-merge-weak-guard-threshold",
            "0.66",
            "--root-anti-merge-weak-guard-radius",
            "0",
            "--residual-percentile",
            "0.955",
            "--residual-fill-radius",
            "4",
            "--residual-edge-fill-radius",
            "2",
            "--residual-edge-fill-min-value-ratio",
            "0.97",
            "--residual-edge-fill-max-patch-span",
            "5",
            "--residual-min-cells",
            "2",
            "--residual-trace-min-length",
            "6",
            "--residual-trace-max-cells",
            "80",
            "--residual-covered-guard-radius",
            "0",
            "--no-residual-trace",
            "--simplify-tolerance",
            "2",
            "--smooth-iterations",
            "1",
        ]
        if args.smooth:
            command += ["--simplify-tolerance", "2", "--smooth-iterations", "1"]

        start = time.perf_counter()
        code, output, peak_memory, _ = run_once(command)
        wall_ms = int((time.perf_counter() - start) * 1000)
        diag_path = output_csv + ".diagnostics.json"
        diag = load_json(diag_path) if os.path.exists(diag_path) else {}
        rows.append(
            {
                "run": i,
                "code": code,
                "wall_ms": wall_ms,
                "core_ms": diag.get("results", {}).get("elapsedMs", 0),
                "read_ms": diag.get("timing", {}).get("readMs", 0),
                "trace_ms": diag.get("timing", {}).get("traceMs", 0),
                "topology_ms": diag.get("timing", {}).get("topologyMs", 0),
                "write_ms": diag.get("timing", {}).get("writeMs", 0),
                "peak_memory": peak_memory,
                "polygons": diag.get("results", {}).get("polygonCount", 0),
                "kept": diag.get("results", {}).get("keptPolygonCount", 0),
                "open": diag.get("topology", {}).get("openRings", 0),
                "self": diag.get("topology", {}).get("selfIntersectingPolygons", 0),
                "intersections": diag.get("topology", {}).get("polygonIntersections", 0),
                "output": output_csv,
                "log": output.strip(),
            }
        )
        if code != 0:
            break

    ok_rows = [row for row in rows if row["code"] == 0]
    avg_core = sum(row["core_ms"] for row in ok_rows) / len(ok_rows) if ok_rows else 0
    max_core = max((row["core_ms"] for row in ok_rows), default=0)
    max_peak = max((row["peak_memory"] for row in ok_rows), default=0)

    lines = []
    lines.append("# 赛题7性能与内存报告")
    lines.append("")
    lines.append(f"生成时间：{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    lines.append("")
    lines.append("## 1. 测试对象")
    lines.append("")
    lines.append(f"- 可执行程序：`{args.exe}`")
    lines.append(f"- 官方数据：`{args.dat}`")
    lines.append(f"- 平滑模式：{'开启' if args.smooth else '关闭'}")
    lines.append(f"- 运行次数：{args.runs}")
    lines.append("")
    lines.append("## 2. 汇总结论")
    lines.append("")
    lines.append(f"- 平均核心耗时：{avg_core:.0f} ms")
    lines.append(f"- 最大核心耗时：{max_core} ms")
    lines.append(f"- 采集到的峰值内存：{fmt_mb(max_peak)}")
    lines.append("- 1 分钟性能要求：满足" if max_core and max_core < 60000 else "- 1 分钟性能要求：需要复核")
    lines.append("- 拓扑稳定性：开放环、自相交、多边形相交均应为 0，详见下表。")
    lines.append("- 已启用性能优化：百分位阈值选择算法、拓扑包围盒剪枝、GeoEast DAT 快速解析。")
    lines.append("")
    lines.append("## 3. 单次运行结果")
    lines.append("")
    lines.append("| 次数 | 返回码 | 核心耗时(ms) | 墙钟耗时(ms) | 峰值内存 | 多边形 | 保留 | 开放环 | 自相交 | 多边形相交 |")
    lines.append("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    for row in rows:
        lines.append(
            f"| {row['run']} | {row['code']} | {row['core_ms']} | {row['wall_ms']} | "
            f"{fmt_mb(row['peak_memory'])} | {row['polygons']} | {row['kept']} | "
            f"{row['open']} | {row['self']} | {row['intersections']} |"
        )
    lines.append("")
    lines.append("## 4. 分段耗时")
    lines.append("")
    lines.append("| 次数 | 读入(ms) | 追踪(ms) | 拓扑(ms) | 写出(ms) | 总耗时(ms) |")
    lines.append("|---:|---:|---:|---:|---:|---:|")
    for row in rows:
        lines.append(
            f"| {row['run']} | {row['read_ms']} | {row['trace_ms']} | "
            f"{row['topology_ms']} | {row['write_ms']} | {row['core_ms']} |"
        )
    lines.append("")
    lines.append("## 5. 说明")
    lines.append("")
    lines.append("- 核心耗时来自程序诊断 JSON 中的 `elapsedMs`。")
    lines.append("- 分段耗时来自程序诊断 JSON 中的 `timing` 字段，用于定位读入、追踪、拓扑诊断和写出瓶颈。")
    lines.append("- 峰值内存通过 `psutil` 采样进程 RSS；如果环境没有 `psutil`，报告会显示未采集。")
    lines.append("- 当前算法核心使用 C++ RAII 容器管理内存，未使用手工裸指针数组作为主要数据结构。")

    with open(args.report, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
        f.write("\n")
    print(f"Report written: {args.report}")


if __name__ == "__main__":
    main()
