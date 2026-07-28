#include "../core/FaultTracer.h"
#include "../core/Topology.h"
#include "../io/AsciiGridIO.h"
#include "../io/DiagnosticsIO.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

std::vector<std::string> commandLineArgumentsUtf8(int argc, char** argv) {
#ifdef _WIN32
    int wideCount = 0;
    LPWSTR* wideArguments = CommandLineToArgvW(GetCommandLineW(), &wideCount);
    if (wideArguments && wideCount > 0) {
        std::vector<std::string> converted;
        converted.reserve(static_cast<std::size_t>(wideCount));
        for (int index = 0; index < wideCount; ++index) {
            const wchar_t* value = wideArguments[index];
            const int byteCount = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
            if (byteCount <= 0) {
                converted.clear();
                break;
            }
            std::string utf8(static_cast<std::size_t>(byteCount), '\0');
            WideCharToMultiByte(CP_UTF8, 0, value, -1, utf8.data(), byteCount, nullptr, nullptr);
            utf8.resize(static_cast<std::size_t>(byteCount - 1));
            converted.push_back(std::move(utf8));
        }
        LocalFree(wideArguments);
        if (!converted.empty()) {
            return converted;
        }
    } else if (wideArguments) {
        LocalFree(wideArguments);
    }
#endif
    std::vector<std::string> copied;
    copied.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        copied.emplace_back(argv[index] ? argv[index] : "");
    }
    return copied;
}

struct CliOptions {
    std::string input;
    std::string output;
    std::string diagnosticsOutput;
    std::string branchesOutput;
    std::string topologyOutput;
    std::string branchAttributionDifferencesOutput;
    std::string inputFormat = "auto";
    fpt::TraceMode traceMode = fpt::TraceMode::RidgeBelt;
    fpt::ThresholdMode thresholdMode = fpt::ThresholdMode::Percentile;
    double threshold = 0.5;
    double percentile = 0.98;
    double minArea = 120.0;
    int minCells = 1;
    int openRadius = 0;
    int closeRadius = 1;
    double weakPercentile = 0.965;
    int weakConnectRadius = 1;
    int maxWeakComponentCells = 20000;
    double minWeakComponentAspect = 3.0;
    bool weakPathGate = false;
    double weakPathSupportRatio = 0.50;
    int weakPathMaxUnsupportedRun = 3;
    bool branchAttributionShadow = false;
    bool branchAttributionCandidateGroups = false;
    bool localPolygonQuality = false;
    double localQualityWidthRatio = 2.4;
    int localQualityMinExtraCells = 8;
    double localQualitySupportThresholdRatio = 0.70;
    int localQualitySupportBufferCells = 1;
    double localQualityMaxAreaLossRatio = 0.18;
    double localQualityMaxStrongCoverageLossRatio = 0.0;
    double localQualityMaxWeakCoverageLossRatio = 0.0;
    double localQualityMaxBackgroundLeakageIncreaseRatio = 0.0;
    double localQualityStabilityThresholdDeltaRatio = 0.02;
    double localQualityMinStabilityJaccard = 0.90;
    std::size_t localQualityMaxCandidatesPerObject = 64;
    std::size_t localQualityMaxTotalCandidates = 4096;
    std::size_t localQualityMaxWindowCells = 65536;
    std::size_t localQualityMaxObjectCells = 2000000;
    std::size_t localQualityMaxEvidenceRecords = 4096;
    int beltRadius = 3;
    int postBeltCloseRadius = 1;
    bool attributeConstrainedBelt = true;
    int attributeEnvelopeRadius = 1;
    int minBranchLength = 4;
    int minJunctionBranchLength = 2;
    double minJunctionBranchMean = 0.94;
    int maxGapDistance = 12;
    double maxGapAngle = 55.0;
    double minGapSupportRatio = 0.25;
    bool separateBranchBelts = false;
    bool rootAntiMerge = false;
    int rootAntiMergeRadius = 1;
    double rootAntiMergeSupportRatio = 0.25;
    double rootAntiMergeMaxBridgeValue = 1e-12;
    int rootAntiMergeMinPartCells = 0;
    double rootAntiMergeMinPartRatio = 0.12;
    double rootAntiMergeWeakGuardThreshold = 0.66;
    int rootAntiMergeWeakGuardRadius = 0;
    bool localAntiMerge = false;
    double localAntiMergeLowThreshold = 0.05;
    int localAntiMergeBridgeRadius = 2;
    int localAntiMergeMinPartCells = 80;
    double localAntiMergeMinPartRatio = 0.08;
    int localAntiMergeMinBridgeCells = 6;
    double localAntiMergeMinLowRatio = 0.55;
    bool adaptiveAreaFilter = true;
    int adaptiveAreaMinCells = 4;
    double adaptiveAreaMinAspect = 2.0;
    double adaptiveAreaMinMeanRatio = 0.55;
    double adaptiveAreaMinMaxRatio = 0.90;
    bool residualAttributeFill = true;
    bool residualTrace = false;
    double residualPercentile = 0.955;
    int residualFillRadius = 4;
    int residualEdgeFillRadius = 2;
    double residualEdgeFillMinValueRatio = 0.97;
    int residualEdgeFillMaxPatchSpan = 5;
    int residualMinCells = 2;
    int residualTraceMinLength = 6;
    int residualTraceMaxCells = 80;
    int residualCoveredGuardRadius = 0;
    bool residualTraceDetached = true;
    int residualDetachedMinCells = 8;
    int residualDetachedMinLength = 10;
    double residualDetachedMinAspect = 3.0;
    double residualDetachedMinMeanRatio = 0.70;
    double residualDetachedMinMaxRatio = 0.92;
    double simplifyTolerance = 2.0;
    int smoothIterations = 1;
};

void printUsage() {
    std::cout
        << "Usage:\n"
        << "  faultpolytrace --input <grid.txt> --output <polygons.csv> [options]\n\n"
        << "Options:\n"
        << "  --input-format <format>    auto, matrix, or geoeast-dat. Default: auto\n"
        << "  --trace-mode <mode>         region or ridge-belt. Default: ridge-belt\n"
        << "  --threshold <value>         Fixed threshold value. Default: 0.5\n"
        << "  --threshold-mode <mode>     fixed, percentile, or positive-percentile. Default: percentile\n"
        << "  --percentile <value>        Percentile in (0,1), e.g. 0.98 keeps top 2%. Default: 0.98\n"
        << "  --min-area <value>          Minimum polygon area. Default: 120\n"
        << "  --min-cells <count>         Minimum connected cells. Default: 1\n"
        << "  --open-radius <count>       Morphological opening radius. Default: 0\n"
        << "  --close-radius <count>      Morphological closing radius. Default: 1\n"
        << "  --weak-percentile <value>   Optional weak continuation percentile in (0,1). Default: 0.965\n"
        << "  --weak-connect-radius <n>   Weak continuation growth radius in cells. Default: 1\n"
        << "  --max-weak-component-cells <count> Max accepted weak component cells. Default: 20000\n"
        << "  --min-weak-component-aspect <value> Min accepted weak component aspect ratio. Default: 3\n"
        << "  --weak-path-gate          Enable experimental path-level weak continuation gate. Default: off\n"
        << "  --no-weak-path-gate       Disable experimental path-level weak continuation gate\n"
        << "  --weak-path-support-ratio <v> Support threshold ratio between weak and strong thresholds. Default: 0.50\n"
        << "  --weak-path-max-unsupported-run <n> Maximum consecutive low-support cells. Default: 3\n"
        << "  --branch-attribution-shadow Enable experimental multi-scale junction diagnostics without changing output. Default: off\n"
        << "  --no-branch-attribution-shadow Disable multi-scale junction shadow diagnostics\n"
        << "  --branch-attribution-candidate-groups Use non-ambiguous multi-scale root groups only for root anti-merge ownership. Default: off\n"
        << "  --no-branch-attribution-candidate-groups Disable experimental candidate root groups\n"
        << "  --branch-attribution-differences <path> Write per-junction legacy/new pairing evidence CSV. Requires shadow or candidate mode\n"
        << "  --local-polygon-quality   Enable experimental local polygon quality repair. Default: off\n"
        << "  --no-local-polygon-quality Disable experimental local polygon quality repair\n"
        << "  --local-quality-width-ratio <v> Local width anomaly ratio. Default: 2.4\n"
        << "  --local-quality-min-extra-cells <n> Minimum excess cells for a candidate. Default: 8\n"
        << "  --local-quality-support-threshold-ratio <v> Support threshold relative to effective threshold. Default: 0.70\n"
        << "  --local-quality-support-buffer-cells <n> Support envelope buffer in cells. Default: 1\n"
        << "  --local-quality-max-area-loss-ratio <v> Maximum accepted polygon area loss. Default: 0.18\n"
        << "  --local-quality-max-strong-coverage-loss-ratio <v> Maximum strong-response coverage loss. Default: 0\n"
        << "  --local-quality-max-weak-coverage-loss-ratio <v> Maximum weak-response coverage loss. Default: 0\n"
        << "  --local-quality-max-background-leakage-increase-ratio <v> Maximum background leakage increase. Default: 0\n"
        << "  --local-quality-stability-threshold-delta-ratio <v> Threshold perturbation used by the stability gate. Default: 0.02\n"
        << "  --local-quality-min-stability-jaccard <v> Minimum perturbed-mask Jaccard. Default: 0.90\n"
        << "  --local-quality-max-candidates-per-object <n> Maximum processed candidates per polygon. Default: 64\n"
        << "  --local-quality-max-total-candidates <n> Maximum processed candidates per run. Default: 4096\n"
        << "  --local-quality-max-window-cells <n> Maximum cells in one candidate window. Default: 65536\n"
        << "  --local-quality-max-object-cells <n> Maximum raster cells evaluated per polygon. Default: 2000000\n"
        << "  --local-quality-max-evidence-records <n> Maximum candidate evidence records stored. Default: 4096\n"
        << "  --belt-radius <count>       Ridge-belt buffer radius in cells. Default: 3\n"
        << "  --post-belt-close-radius <count> Post-buffer closing radius. Default: 1\n"
        << "  --attribute-constrained-belt Constrain ridge belt by original attribute envelope. Default: on\n"
        << "  --no-attribute-constrained-belt Disable attribute envelope constraint. Default: off\n"
        << "  --attribute-envelope-radius <count> Attribute envelope radius in cells. Default: 1\n"
        << "  --min-branch-length <count> Minimum skeleton branch pixels kept in ridge-belt mode. Default: 4\n"
        << "  --min-junction-branch-length <count> Minimum branch pixels kept near junctions. Default: 2\n"
        << "  --min-junction-branch-mean <value> Minimum mean attribute for short junction branches. Default: 0.94\n"
        << "  --max-gap-distance <count>  Max endpoint gap connected in ridge-belt mode. Default: 12\n"
        << "  --max-gap-angle <degrees>   Max endpoint direction mismatch. Default: 55\n"
        << "  --min-gap-support-ratio <v> Minimum weak support ratio for endpoint continuation. Default: 0.25\n"
        << "  --separate-branch-belts    Output root-trace-separated local belts. Default: off\n"
        << "  --no-separate-branch-belts Use one merged ridge belt before polygon tracing. Default: on\n"
        << "  --root-anti-merge          Cut unsupported bridge cells between different root traces. Default: off\n"
        << "  --no-root-anti-merge       Disable root-trace anti-merge guard. Root anti-merge is off by default\n"
        << "  --root-anti-merge-radius <n> Neighborhood radius for bridge detection. Default: 1\n"
        << "  --root-anti-merge-support-ratio <v> Attribute support ratio needed to keep bridge cells. Default: 0.25\n"
        << "  --root-anti-merge-max-bridge-value <v> Max raw attribute value treated as background bridge. Default: 1e-12\n"
        << "  --root-anti-merge-min-part-cells <n> Minimum major split part cells after bridge cutting. Default: auto\n"
        << "  --root-anti-merge-min-part-ratio <v> Minimum major split part ratio after bridge cutting. Default: 0.12\n"
        << "  --root-anti-merge-weak-guard-threshold <v> Protect bridge cuts near weak response cells. Default: 0.66\n"
        << "  --root-anti-merge-weak-guard-radius <n> Experimental weak response protection radius in cells. Default: 0\n"
        << "  --local-anti-merge         Experimental local low-value bridge cutting. Default: off\n"
        << "  --no-local-anti-merge      Disable local low-value bridge cutting. Local anti-merge is off by default\n"
        << "  --local-anti-merge-low-threshold <v> Raw value treated as local background bridge. Default: 0.05\n"
        << "  --local-anti-merge-bridge-radius <n> Neighborhood radius for local bridge support. Default: 2\n"
        << "  --local-anti-merge-min-part-cells <n> Minimum major split part cells. Default: 80\n"
        << "  --local-anti-merge-min-part-ratio <v> Minimum major split part ratio. Default: 0.08\n"
        << "  --local-anti-merge-min-bridge-cells <n> Minimum low bridge cells before cutting. Default: 6\n"
        << "  --local-anti-merge-min-low-ratio <v> Minimum low-value ratio for a component to be tested. Default: 0.55\n"
        << "  --no-adaptive-area-filter   Disable line-like small polygon retention. Default: on\n"
        << "  --adaptive-area-min-cells <n> Minimum cells for adaptive retention. Default: 4\n"
        << "  --adaptive-area-min-aspect <v> Minimum line-like aspect ratio. Default: 2\n"
        << "  --adaptive-area-min-mean-ratio <v> Mean-value ratio to strong threshold. Default: 0.55\n"
        << "  --adaptive-area-min-max-ratio <v> Max-value ratio to strong threshold. Default: 0.90\n"
        << "  --no-residual-fill          Disable residual in-trace attribute fill. Default: on\n"
        << "  --residual-trace            Enable secondary residual trace for high-recall review. Default: off\n"
        << "  --no-residual-trace         Disable secondary residual trace. Default: off\n"
        << "  --residual-percentile <v>   Residual weak-response percentile. Default: 0.955\n"
        << "  --residual-fill-radius <n>  Residual fill reach from root trace in cells. Default: 4\n"
        << "  --residual-edge-fill-radius <n> Controlled edge micro-fill radius in cells. Default: 2\n"
        << "  --residual-edge-fill-min-value-ratio <v> Min value ratio for edge micro-fill. Default: 0.97\n"
        << "  --residual-edge-fill-max-patch-span <n> Max patch width/height for edge micro-fill. Default: 5\n"
        << "  --residual-min-cells <n>    Minimum residual component cells. Default: 2\n"
        << "  --residual-trace-min-length <n> Minimum longest axis of secondary residual trace. Default: 6\n"
        << "  --residual-trace-max-cells <n> Maximum cells of one secondary residual trace, 0 unlimited. Default: 80\n"
        << "  --residual-covered-guard-radius <n> Guard radius around kept polygons before residual trace. Default: 0\n"
        << "  --no-residual-detached-trace Disable strict detached residual trace. Default: on\n"
        << "  --residual-detached-min-cells <n> Minimum cells for detached residual trace. Default: 8\n"
        << "  --residual-detached-min-length <n> Minimum longest axis for detached residual trace. Default: 10\n"
        << "  --residual-detached-min-aspect <v> Minimum aspect for detached residual trace. Default: 3\n"
        << "  --residual-detached-min-mean-ratio <v> Mean ratio for detached residual trace. Default: 0.70\n"
        << "  --residual-detached-min-max-ratio <v> Max ratio for detached residual trace. Default: 0.92\n"
        << "  --simplify-tolerance <dist> Douglas-Peucker ring simplification tolerance. Default: 2\n"
        << "  --smooth-iterations <count> Chaikin ring smoothing iterations, 0-5. Default: 1\n"
        << "  --diagnostics <path>        Diagnostics JSON path. Default: <output>.diagnostics.json\n"
        << "  --branches-output <path>    Ridge-belt branch CSV path. Default: <output>.branches.csv\n"
        << "  --topology-output <path>    Topology detail CSV path. Default: <output>.topology.csv\n";
}

std::string normalizedPathKey(const std::string& path) {
#ifdef _WIN32
    const int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                               path.data(), static_cast<int>(path.size()),
                                               nullptr, 0);
    if (wideLength <= 0) {
        throw std::runtime_error("Invalid UTF-8 output path: " + path);
    }
    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                        path.data(), static_cast<int>(path.size()), wide.data(), wideLength);
    const DWORD fullLength = GetFullPathNameW(wide.c_str(), 0, nullptr, nullptr);
    if (fullLength == 0) {
        throw std::runtime_error("Cannot normalize output path: " + path);
    }
    std::wstring full(static_cast<std::size_t>(fullLength), L'\0');
    GetFullPathNameW(wide.c_str(), fullLength, full.data(), nullptr);
    if (!full.empty() && full.back() == L'\0') {
        full.pop_back();
    }
    std::transform(full.begin(), full.end(), full.begin(), [](wchar_t value) {
        return value == L'\\' ? L'/' : static_cast<wchar_t>(std::towlower(value));
    });
    const int utf8Length = WideCharToMultiByte(CP_UTF8, 0, full.data(), static_cast<int>(full.size()),
                                               nullptr, 0, nullptr, nullptr);
    std::string key(static_cast<std::size_t>(utf8Length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, full.data(), static_cast<int>(full.size()),
                        key.data(), utf8Length, nullptr, nullptr);
    return key;
#else
    std::string key = std::filesystem::absolute(
        std::filesystem::path(path)).lexically_normal().generic_string();
    return key;
#endif
}

std::filesystem::path filesystemPathFromUtf8(const std::string& path) {
#ifdef _WIN32
    const int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                               path.data(), static_cast<int>(path.size()),
                                               nullptr, 0);
    if (wideLength <= 0) {
        throw std::runtime_error("Invalid UTF-8 file path: " + path);
    }
    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                        path.data(), static_cast<int>(path.size()), wide.data(), wideLength);
    return std::filesystem::path(wide);
#else
    return std::filesystem::path(path);
#endif
}

void prepareOutputDirectories(const CliOptions& options) {
    const std::vector<std::string> paths = {
        options.output,
        options.diagnosticsOutput,
        options.branchesOutput,
        options.topologyOutput,
        options.branchAttributionDifferencesOutput
    };
    for (const std::string& path : paths) {
        if (path.empty()) {
            continue;
        }
        const std::filesystem::path parent = filesystemPathFromUtf8(path).parent_path();
        if (!parent.empty()) {
            std::error_code error;
            std::filesystem::create_directories(parent, error);
            if (error) {
                throw std::runtime_error("Cannot create output directory for: " + path +
                                         " (" + error.message() + ")");
            }
        }
    }
}

void validateDistinctOutputPaths(const CliOptions& options) {
    const std::vector<std::pair<std::string, std::string>> outputs = {
        {"--output", options.output},
        {"--diagnostics", options.diagnosticsOutput},
        {"--branches-output", options.branchesOutput},
        {"--topology-output", options.topologyOutput},
        {
            "--branch-attribution-differences",
            options.branchAttributionDifferencesOutput
        }
    };
    const std::string inputKey = normalizedPathKey(options.input);
    std::unordered_map<std::string, std::string> optionByPath;
    for (const auto& output : outputs) {
        if (output.second.empty()) {
            continue;
        }
        const std::string key = normalizedPathKey(output.second);
        if (key == inputKey) {
            throw std::runtime_error(
                "Output path " + output.first +
                " must not overwrite --input.");
        }
        const auto inserted = optionByPath.emplace(key, output.first);
        if (!inserted.second) {
            throw std::runtime_error(
                "Output paths must be distinct: " +
                inserted.first->second + " and " + output.first +
                " resolve to the same file.");
        }
    }
}

bool likelyStartedByDoubleClick() {
#ifdef _WIN32
    DWORD inputConsoleMode = 0;
    DWORD outputConsoleMode = 0;
    if (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &inputConsoleMode) == 0 ||
        GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &outputConsoleMode) == 0) {
        return false;
    }
    DWORD processes[4] = {};
    const DWORD count = GetConsoleProcessList(processes, 4);
    return count <= 1;
#else
    return false;
#endif
}

void printNoArgumentGuide() {
    std::cout
        << "FaultPolyTrace 断层多边形自动追踪 - 独立算法程序\n"
        << "================================================\n\n"
        << "这个程序是命令行算法核心，用于外部网格文件批处理和回归验证。\n"
        << "如果直接双击但没有提供输入参数，程序不会开始计算。\n\n"
        << "推荐入口：\n"
        << "  1. 图形界面：双击发布包根目录的 01_打开图形界面.cmd，或运行 qt_launcher/faultpolytrace_launcher.exe。\n"
        << "  2. 离线演示：双击发布包根目录的 02_运行离线演示.cmd。\n"
        << "  3. 命令行：在 PowerShell 中运行 bin/faultpolytrace.exe --input <网格文件> --output <结果CSV>。\n\n"
        << "命令行示例：\n"
        << "  bin\\faultpolytrace.exe --input examples\\synthetic\\broken_ridge_fault_grid.txt --input-format matrix --output outputs\\demo\\fault_polygons.csv\n\n";
    printUsage();
}

std::string traceModeName(fpt::TraceMode mode) {
    return mode == fpt::TraceMode::RidgeBelt ? "ridge-belt" : "region";
}

std::string thresholdModeName(fpt::ThresholdMode mode) {
    if (mode == fpt::ThresholdMode::PositivePercentile) {
        return "positive-percentile";
    }
    return mode == fpt::ThresholdMode::Percentile ? "percentile" : "fixed";
}

std::size_t parseSizeValue(
    const std::string& value,
    const std::string& optionName) {
    if (value.empty() || value.front() == '-') {
        throw std::runtime_error(
            optionName + " must be a non-negative integer.");
    }
    std::size_t consumed = 0;
    const unsigned long long parsed = std::stoull(value, &consumed);
    if (consumed != value.size() ||
        parsed > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error(
            optionName + " is outside the supported integer range.");
    }
    return static_cast<std::size_t>(parsed);
}

CliOptions parseArgs(int argc, char** argv) {
    CliOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto requireValue = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for " + name);
            }
            return argv[++i];
        };

        if (arg == "--input") {
            options.input = requireValue(arg);
        } else if (arg == "--output") {
            options.output = requireValue(arg);
        } else if (arg == "--input-format") {
            options.inputFormat = requireValue(arg);
            if (options.inputFormat != "auto" &&
                options.inputFormat != "matrix" &&
                options.inputFormat != "geoeast-dat") {
                throw std::runtime_error("Unknown input format: " + options.inputFormat);
            }
        } else if (arg == "--diagnostics") {
            options.diagnosticsOutput = requireValue(arg);
        } else if (arg == "--branches-output") {
            options.branchesOutput = requireValue(arg);
        } else if (arg == "--topology-output") {
            options.topologyOutput = requireValue(arg);
        } else if (arg == "--branch-attribution-differences") {
            options.branchAttributionDifferencesOutput =
                requireValue(arg);
        } else if (arg == "--trace-mode") {
            const std::string mode = requireValue(arg);
            if (mode == "region") {
                options.traceMode = fpt::TraceMode::Region;
            } else if (mode == "ridge-belt") {
                options.traceMode = fpt::TraceMode::RidgeBelt;
            } else {
                throw std::runtime_error("Unknown trace mode: " + mode);
            }
        } else if (arg == "--threshold-mode") {
            const std::string mode = requireValue(arg);
            if (mode == "fixed") {
                options.thresholdMode = fpt::ThresholdMode::Fixed;
            } else if (mode == "percentile") {
                options.thresholdMode = fpt::ThresholdMode::Percentile;
            } else if (mode == "positive-percentile") {
                options.thresholdMode = fpt::ThresholdMode::PositivePercentile;
            } else {
                throw std::runtime_error("Unknown threshold mode: " + mode);
            }
        } else if (arg == "--threshold") {
            options.threshold = std::stod(requireValue(arg));
        } else if (arg == "--percentile") {
            options.percentile = std::stod(requireValue(arg));
        } else if (arg == "--min-area") {
            options.minArea = std::stod(requireValue(arg));
        } else if (arg == "--min-cells") {
            options.minCells = std::stoi(requireValue(arg));
        } else if (arg == "--open-radius") {
            options.openRadius = std::stoi(requireValue(arg));
        } else if (arg == "--close-radius") {
            options.closeRadius = std::stoi(requireValue(arg));
        } else if (arg == "--weak-percentile") {
            options.weakPercentile = std::stod(requireValue(arg));
        } else if (arg == "--weak-connect-radius") {
            options.weakConnectRadius = std::stoi(requireValue(arg));
        } else if (arg == "--max-weak-component-cells") {
            options.maxWeakComponentCells = std::stoi(requireValue(arg));
        } else if (arg == "--min-weak-component-aspect") {
            options.minWeakComponentAspect = std::stod(requireValue(arg));
        } else if (arg == "--weak-path-gate") {
            options.weakPathGate = true;
        } else if (arg == "--no-weak-path-gate") {
            options.weakPathGate = false;
        } else if (arg == "--weak-path-support-ratio") {
            options.weakPathSupportRatio = std::stod(requireValue(arg));
        } else if (arg == "--weak-path-max-unsupported-run") {
            options.weakPathMaxUnsupportedRun = std::stoi(requireValue(arg));
        } else if (arg == "--branch-attribution-shadow") {
            options.branchAttributionShadow = true;
        } else if (arg == "--no-branch-attribution-shadow") {
            options.branchAttributionShadow = false;
        } else if (arg == "--branch-attribution-candidate-groups") {
            options.branchAttributionCandidateGroups = true;
        } else if (arg == "--no-branch-attribution-candidate-groups") {
            options.branchAttributionCandidateGroups = false;
        } else if (arg == "--local-polygon-quality") {
            options.localPolygonQuality = true;
        } else if (arg == "--no-local-polygon-quality") {
            options.localPolygonQuality = false;
        } else if (arg == "--local-quality-width-ratio") {
            options.localQualityWidthRatio = std::stod(requireValue(arg));
        } else if (arg == "--local-quality-min-extra-cells") {
            options.localQualityMinExtraCells = std::stoi(requireValue(arg));
        } else if (arg == "--local-quality-support-threshold-ratio") {
            options.localQualitySupportThresholdRatio = std::stod(requireValue(arg));
        } else if (arg == "--local-quality-support-buffer-cells") {
            options.localQualitySupportBufferCells = std::stoi(requireValue(arg));
        } else if (arg == "--local-quality-max-area-loss-ratio") {
            options.localQualityMaxAreaLossRatio = std::stod(requireValue(arg));
        } else if (arg == "--local-quality-max-strong-coverage-loss-ratio") {
            options.localQualityMaxStrongCoverageLossRatio = std::stod(requireValue(arg));
        } else if (arg == "--local-quality-max-weak-coverage-loss-ratio") {
            options.localQualityMaxWeakCoverageLossRatio = std::stod(requireValue(arg));
        } else if (arg == "--local-quality-max-background-leakage-increase-ratio") {
            options.localQualityMaxBackgroundLeakageIncreaseRatio = std::stod(requireValue(arg));
        } else if (arg == "--local-quality-stability-threshold-delta-ratio") {
            options.localQualityStabilityThresholdDeltaRatio = std::stod(requireValue(arg));
        } else if (arg == "--local-quality-min-stability-jaccard") {
            options.localQualityMinStabilityJaccard = std::stod(requireValue(arg));
        } else if (arg == "--local-quality-max-candidates-per-object") {
            options.localQualityMaxCandidatesPerObject =
                parseSizeValue(requireValue(arg), arg);
        } else if (arg == "--local-quality-max-total-candidates") {
            options.localQualityMaxTotalCandidates =
                parseSizeValue(requireValue(arg), arg);
        } else if (arg == "--local-quality-max-window-cells") {
            options.localQualityMaxWindowCells =
                parseSizeValue(requireValue(arg), arg);
        } else if (arg == "--local-quality-max-object-cells") {
            options.localQualityMaxObjectCells =
                parseSizeValue(requireValue(arg), arg);
        } else if (arg == "--local-quality-max-evidence-records") {
            options.localQualityMaxEvidenceRecords =
                parseSizeValue(requireValue(arg), arg);
        } else if (arg == "--belt-radius") {
            options.beltRadius = std::stoi(requireValue(arg));
        } else if (arg == "--post-belt-close-radius") {
            options.postBeltCloseRadius = std::stoi(requireValue(arg));
        } else if (arg == "--attribute-constrained-belt") {
            options.attributeConstrainedBelt = true;
        } else if (arg == "--no-attribute-constrained-belt") {
            options.attributeConstrainedBelt = false;
        } else if (arg == "--attribute-envelope-radius") {
            options.attributeEnvelopeRadius = std::stoi(requireValue(arg));
        } else if (arg == "--min-branch-length") {
            options.minBranchLength = std::stoi(requireValue(arg));
        } else if (arg == "--min-junction-branch-length") {
            options.minJunctionBranchLength = std::stoi(requireValue(arg));
        } else if (arg == "--min-junction-branch-mean") {
            options.minJunctionBranchMean = std::stod(requireValue(arg));
        } else if (arg == "--max-gap-distance") {
            options.maxGapDistance = std::stoi(requireValue(arg));
        } else if (arg == "--max-gap-angle") {
            options.maxGapAngle = std::stod(requireValue(arg));
        } else if (arg == "--min-gap-support-ratio") {
            options.minGapSupportRatio = std::stod(requireValue(arg));
        } else if (arg == "--separate-branch-belts") {
            options.separateBranchBelts = true;
        } else if (arg == "--no-separate-branch-belts") {
            options.separateBranchBelts = false;
        } else if (arg == "--root-anti-merge") {
            options.rootAntiMerge = true;
        } else if (arg == "--no-root-anti-merge") {
            options.rootAntiMerge = false;
        } else if (arg == "--root-anti-merge-radius") {
            options.rootAntiMergeRadius = std::stoi(requireValue(arg));
        } else if (arg == "--root-anti-merge-support-ratio") {
            options.rootAntiMergeSupportRatio = std::stod(requireValue(arg));
        } else if (arg == "--root-anti-merge-max-bridge-value") {
            options.rootAntiMergeMaxBridgeValue = std::stod(requireValue(arg));
        } else if (arg == "--root-anti-merge-min-part-cells") {
            options.rootAntiMergeMinPartCells = std::stoi(requireValue(arg));
        } else if (arg == "--root-anti-merge-min-part-ratio") {
            options.rootAntiMergeMinPartRatio = std::stod(requireValue(arg));
        } else if (arg == "--root-anti-merge-weak-guard-threshold") {
            options.rootAntiMergeWeakGuardThreshold = std::stod(requireValue(arg));
        } else if (arg == "--root-anti-merge-weak-guard-radius") {
            options.rootAntiMergeWeakGuardRadius = std::stoi(requireValue(arg));
        } else if (arg == "--local-anti-merge") {
            options.localAntiMerge = true;
        } else if (arg == "--no-local-anti-merge") {
            options.localAntiMerge = false;
        } else if (arg == "--local-anti-merge-low-threshold") {
            options.localAntiMergeLowThreshold = std::stod(requireValue(arg));
        } else if (arg == "--local-anti-merge-bridge-radius") {
            options.localAntiMergeBridgeRadius = std::stoi(requireValue(arg));
        } else if (arg == "--local-anti-merge-min-part-cells") {
            options.localAntiMergeMinPartCells = std::stoi(requireValue(arg));
        } else if (arg == "--local-anti-merge-min-part-ratio") {
            options.localAntiMergeMinPartRatio = std::stod(requireValue(arg));
        } else if (arg == "--local-anti-merge-min-bridge-cells") {
            options.localAntiMergeMinBridgeCells = std::stoi(requireValue(arg));
        } else if (arg == "--local-anti-merge-min-low-ratio") {
            options.localAntiMergeMinLowRatio = std::stod(requireValue(arg));
        } else if (arg == "--no-adaptive-area-filter") {
            options.adaptiveAreaFilter = false;
        } else if (arg == "--adaptive-area-min-cells") {
            options.adaptiveAreaMinCells = std::stoi(requireValue(arg));
        } else if (arg == "--adaptive-area-min-aspect") {
            options.adaptiveAreaMinAspect = std::stod(requireValue(arg));
        } else if (arg == "--adaptive-area-min-mean-ratio") {
            options.adaptiveAreaMinMeanRatio = std::stod(requireValue(arg));
        } else if (arg == "--adaptive-area-min-max-ratio") {
            options.adaptiveAreaMinMaxRatio = std::stod(requireValue(arg));
        } else if (arg == "--no-residual-fill") {
            options.residualAttributeFill = false;
        } else if (arg == "--residual-trace") {
            options.residualTrace = true;
        } else if (arg == "--no-residual-trace") {
            options.residualTrace = false;
        } else if (arg == "--residual-percentile") {
            options.residualPercentile = std::stod(requireValue(arg));
        } else if (arg == "--residual-fill-radius") {
            options.residualFillRadius = std::stoi(requireValue(arg));
        } else if (arg == "--residual-edge-fill-radius") {
            options.residualEdgeFillRadius = std::stoi(requireValue(arg));
        } else if (arg == "--residual-edge-fill-min-value-ratio") {
            options.residualEdgeFillMinValueRatio = std::stod(requireValue(arg));
        } else if (arg == "--residual-edge-fill-max-patch-span") {
            options.residualEdgeFillMaxPatchSpan = std::stoi(requireValue(arg));
        } else if (arg == "--residual-min-cells") {
            options.residualMinCells = std::stoi(requireValue(arg));
        } else if (arg == "--residual-trace-min-length") {
            options.residualTraceMinLength = std::stoi(requireValue(arg));
        } else if (arg == "--residual-trace-max-cells") {
            options.residualTraceMaxCells = std::stoi(requireValue(arg));
        } else if (arg == "--residual-covered-guard-radius") {
            options.residualCoveredGuardRadius = std::stoi(requireValue(arg));
        } else if (arg == "--no-residual-detached-trace") {
            options.residualTraceDetached = false;
        } else if (arg == "--residual-detached-min-cells") {
            options.residualDetachedMinCells = std::stoi(requireValue(arg));
        } else if (arg == "--residual-detached-min-length") {
            options.residualDetachedMinLength = std::stoi(requireValue(arg));
        } else if (arg == "--residual-detached-min-aspect") {
            options.residualDetachedMinAspect = std::stod(requireValue(arg));
        } else if (arg == "--residual-detached-min-mean-ratio") {
            options.residualDetachedMinMeanRatio = std::stod(requireValue(arg));
        } else if (arg == "--residual-detached-min-max-ratio") {
            options.residualDetachedMinMaxRatio = std::stod(requireValue(arg));
        } else if (arg == "--simplify-tolerance") {
            options.simplifyTolerance = std::stod(requireValue(arg));
        } else if (arg == "--smooth-iterations") {
            options.smoothIterations = std::stoi(requireValue(arg));
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    if (options.input.empty() || options.output.empty()) {
        throw std::runtime_error("Both --input and --output are required.");
    }
    if (options.diagnosticsOutput.empty()) {
        options.diagnosticsOutput = options.output + ".diagnostics.json";
    }
    if (options.branchesOutput.empty()) {
        options.branchesOutput = options.output + ".branches.csv";
    }
    if (options.topologyOutput.empty()) {
        options.topologyOutput = options.output + ".topology.csv";
    }
    if (!options.branchAttributionDifferencesOutput.empty() &&
        !options.branchAttributionShadow &&
        !options.branchAttributionCandidateGroups) {
        throw std::runtime_error(
            "--branch-attribution-differences requires "
            "--branch-attribution-shadow or "
            "--branch-attribution-candidate-groups.");
    }
    if (options.branchAttributionCandidateGroups &&
        !options.rootAntiMerge) {
        throw std::runtime_error(
            "--branch-attribution-candidate-groups requires "
            "--root-anti-merge.");
    }
    if (options.branchAttributionCandidateGroups &&
        options.separateBranchBelts) {
        throw std::runtime_error(
            "--branch-attribution-candidate-groups requires "
            "--no-separate-branch-belts.");
    }
    if (options.branchAttributionCandidateGroups &&
        options.traceMode != fpt::TraceMode::RidgeBelt) {
        throw std::runtime_error(
            "--branch-attribution-candidate-groups requires "
            "--trace-mode ridge-belt.");
    }
    fpt::LocalPolygonQualityOptions localQualityOptions;
    localQualityOptions.widthRatio = options.localQualityWidthRatio;
    localQualityOptions.minExtraCells = options.localQualityMinExtraCells;
    localQualityOptions.supportThresholdRatio =
        options.localQualitySupportThresholdRatio;
    localQualityOptions.supportBufferCells =
        options.localQualitySupportBufferCells;
    localQualityOptions.maxAreaLossRatio =
        options.localQualityMaxAreaLossRatio;
    localQualityOptions.maxStrongCoverageLossRatio =
        options.localQualityMaxStrongCoverageLossRatio;
    localQualityOptions.maxWeakCoverageLossRatio =
        options.localQualityMaxWeakCoverageLossRatio;
    localQualityOptions.maxBackgroundLeakageIncreaseRatio =
        options.localQualityMaxBackgroundLeakageIncreaseRatio;
    localQualityOptions.stabilityThresholdDeltaRatio =
        options.localQualityStabilityThresholdDeltaRatio;
    localQualityOptions.minStabilityJaccard =
        options.localQualityMinStabilityJaccard;
    localQualityOptions.maxCandidatesPerObject =
        options.localQualityMaxCandidatesPerObject;
    localQualityOptions.maxTotalCandidates =
        options.localQualityMaxTotalCandidates;
    localQualityOptions.maxWindowCells = options.localQualityMaxWindowCells;
    localQualityOptions.maxObjectCells = options.localQualityMaxObjectCells;
    localQualityOptions.maxEvidenceRecords =
        options.localQualityMaxEvidenceRecords;
    if (options.localPolygonQuality) {
        fpt::validateLocalPolygonQualityOptions(localQualityOptions);
    }
    validateDistinctOutputPaths(options);
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
        std::vector<std::string> utf8Arguments = commandLineArgumentsUtf8(argc, argv);
        std::vector<char*> utf8Argv;
        utf8Argv.reserve(utf8Arguments.size());
        for (std::string& argument : utf8Arguments) {
            utf8Argv.push_back(argument.data());
        }
        argc = static_cast<int>(utf8Argv.size());
        argv = utf8Argv.data();

        if (argc == 1) {
            printNoArgumentGuide();
            if (likelyStartedByDoubleClick()) {
                std::cout << "\n按 Enter 键退出...";
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
            return 0;
        }

        const CliOptions cli = parseArgs(argc, argv);
        prepareOutputDirectories(cli);
        const auto start = std::chrono::steady_clock::now();

        fpt::Grid2D grid;
        if (cli.inputFormat == "matrix") {
            grid = fpt::readAsciiGrid(cli.input);
        } else if (cli.inputFormat == "geoeast-dat") {
            grid = fpt::readGeoEastDatGrid(cli.input);
        } else {
            grid = fpt::readAutoGrid(cli.input);
        }
        const auto afterRead = std::chrono::steady_clock::now();

        fpt::TraceOptions traceOptions;
        traceOptions.traceMode = cli.traceMode;
        traceOptions.thresholdMode = cli.thresholdMode;
        traceOptions.threshold = cli.threshold;
        traceOptions.percentile = cli.percentile;
        traceOptions.minArea = cli.minArea;
        traceOptions.minCells = cli.minCells;
        traceOptions.openRadius = cli.openRadius;
        traceOptions.closeRadius = cli.closeRadius;
        traceOptions.weakPercentile = cli.weakPercentile;
        traceOptions.weakConnectRadius = cli.weakConnectRadius;
        traceOptions.maxWeakComponentCells = cli.maxWeakComponentCells;
        traceOptions.minWeakComponentAspect = cli.minWeakComponentAspect;
        traceOptions.weakPathGate = cli.weakPathGate;
        traceOptions.weakPathSupportRatio = cli.weakPathSupportRatio;
        traceOptions.weakPathMaxUnsupportedRun = cli.weakPathMaxUnsupportedRun;
        traceOptions.branchAttributionShadow =
            cli.branchAttributionShadow;
        traceOptions.branchAttributionCandidateGroups =
            cli.branchAttributionCandidateGroups;
        traceOptions.localPolygonQuality = cli.localPolygonQuality;
        traceOptions.localQualityWidthRatio = cli.localQualityWidthRatio;
        traceOptions.localQualityMinExtraCells = cli.localQualityMinExtraCells;
        traceOptions.localQualitySupportThresholdRatio =
            cli.localQualitySupportThresholdRatio;
        traceOptions.localQualitySupportBufferCells =
            cli.localQualitySupportBufferCells;
        traceOptions.localQualityMaxAreaLossRatio =
            cli.localQualityMaxAreaLossRatio;
        traceOptions.localQualityMaxStrongCoverageLossRatio =
            cli.localQualityMaxStrongCoverageLossRatio;
        traceOptions.localQualityMaxWeakCoverageLossRatio =
            cli.localQualityMaxWeakCoverageLossRatio;
        traceOptions.localQualityMaxBackgroundLeakageIncreaseRatio =
            cli.localQualityMaxBackgroundLeakageIncreaseRatio;
        traceOptions.localQualityStabilityThresholdDeltaRatio =
            cli.localQualityStabilityThresholdDeltaRatio;
        traceOptions.localQualityMinStabilityJaccard =
            cli.localQualityMinStabilityJaccard;
        traceOptions.localQualityMaxCandidatesPerObject =
            cli.localQualityMaxCandidatesPerObject;
        traceOptions.localQualityMaxTotalCandidates =
            cli.localQualityMaxTotalCandidates;
        traceOptions.localQualityMaxWindowCells =
            cli.localQualityMaxWindowCells;
        traceOptions.localQualityMaxObjectCells =
            cli.localQualityMaxObjectCells;
        traceOptions.localQualityMaxEvidenceRecords =
            cli.localQualityMaxEvidenceRecords;
        traceOptions.beltRadius = cli.beltRadius;
        traceOptions.postBeltCloseRadius = cli.postBeltCloseRadius;
        traceOptions.attributeConstrainedBelt = cli.attributeConstrainedBelt;
        traceOptions.attributeEnvelopeRadius = cli.attributeEnvelopeRadius;
        traceOptions.minBranchLength = cli.minBranchLength;
        traceOptions.minJunctionBranchLength = cli.minJunctionBranchLength;
        traceOptions.minJunctionBranchMean = cli.minJunctionBranchMean;
        traceOptions.maxGapDistance = cli.maxGapDistance;
        traceOptions.maxGapAngle = cli.maxGapAngle;
        traceOptions.minGapSupportRatio = cli.minGapSupportRatio;
        traceOptions.separateBranchBelts = cli.separateBranchBelts;
        traceOptions.rootAntiMerge = cli.rootAntiMerge;
        traceOptions.rootAntiMergeRadius = cli.rootAntiMergeRadius;
        traceOptions.rootAntiMergeSupportRatio = cli.rootAntiMergeSupportRatio;
        traceOptions.rootAntiMergeMaxBridgeValue = cli.rootAntiMergeMaxBridgeValue;
        traceOptions.rootAntiMergeMinPartCells = cli.rootAntiMergeMinPartCells;
        traceOptions.rootAntiMergeMinPartRatio = cli.rootAntiMergeMinPartRatio;
        traceOptions.rootAntiMergeWeakGuardThreshold = cli.rootAntiMergeWeakGuardThreshold;
        traceOptions.rootAntiMergeWeakGuardRadius = cli.rootAntiMergeWeakGuardRadius;
        traceOptions.localAntiMerge = cli.localAntiMerge;
        traceOptions.localAntiMergeLowThreshold = cli.localAntiMergeLowThreshold;
        traceOptions.localAntiMergeBridgeRadius = cli.localAntiMergeBridgeRadius;
        traceOptions.localAntiMergeMinPartCells = cli.localAntiMergeMinPartCells;
        traceOptions.localAntiMergeMinPartRatio = cli.localAntiMergeMinPartRatio;
        traceOptions.localAntiMergeMinBridgeCells = cli.localAntiMergeMinBridgeCells;
        traceOptions.localAntiMergeMinLowRatio = cli.localAntiMergeMinLowRatio;
        traceOptions.adaptiveAreaFilter = cli.adaptiveAreaFilter;
        traceOptions.adaptiveAreaMinCells = cli.adaptiveAreaMinCells;
        traceOptions.adaptiveAreaMinAspect = cli.adaptiveAreaMinAspect;
        traceOptions.adaptiveAreaMinMeanRatio = cli.adaptiveAreaMinMeanRatio;
        traceOptions.adaptiveAreaMinMaxRatio = cli.adaptiveAreaMinMaxRatio;
        traceOptions.residualAttributeFill = cli.residualAttributeFill;
        traceOptions.residualTrace = cli.residualTrace;
        traceOptions.residualPercentile = cli.residualPercentile;
        traceOptions.residualFillRadius = cli.residualFillRadius;
        traceOptions.residualEdgeFillRadius = cli.residualEdgeFillRadius;
        traceOptions.residualEdgeFillMinValueRatio = cli.residualEdgeFillMinValueRatio;
        traceOptions.residualEdgeFillMaxPatchSpan = cli.residualEdgeFillMaxPatchSpan;
        traceOptions.residualMinCells = cli.residualMinCells;
        traceOptions.residualTraceMinLength = cli.residualTraceMinLength;
        traceOptions.residualTraceMaxCells = cli.residualTraceMaxCells;
        traceOptions.residualCoveredGuardRadius = cli.residualCoveredGuardRadius;
        traceOptions.residualTraceDetached = cli.residualTraceDetached;
        traceOptions.residualDetachedMinCells = cli.residualDetachedMinCells;
        traceOptions.residualDetachedMinLength = cli.residualDetachedMinLength;
        traceOptions.residualDetachedMinAspect = cli.residualDetachedMinAspect;
        traceOptions.residualDetachedMinMeanRatio = cli.residualDetachedMinMeanRatio;
        traceOptions.residualDetachedMinMaxRatio = cli.residualDetachedMinMaxRatio;
        traceOptions.simplifyTolerance = cli.simplifyTolerance;
        traceOptions.smoothIterations = cli.smoothIterations;

        fpt::TraceResult result = fpt::traceFaultPolygons(grid, traceOptions);
        const auto afterTrace = std::chrono::steady_clock::now();
        fpt::TopologyDiagnostics topology = fpt::diagnoseTopology(result.polygons, true);
        const auto afterTopology = std::chrono::steady_clock::now();
        fpt::writePolygonCsv(cli.output, result.polygons);
        fpt::writeTopologyDetailsCsv(cli.topologyOutput, topology);
        if (cli.traceMode == fpt::TraceMode::RidgeBelt) {
            fpt::writeBranchCsv(cli.branchesOutput, result.branches);
        }
        if (!cli.branchAttributionDifferencesOutput.empty()) {
            fpt::writeBranchAttributionDifferencesCsv(
                cli.branchAttributionDifferencesOutput,
                result.branchAttributionJunctions);
        }
        const auto afterWrites = std::chrono::steady_clock::now();

        int kept = 0;
        for (const fpt::Polygon& polygon : result.polygons) {
            if (polygon.kept) {
                ++kept;
            }
        }

        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(afterWrites - start).count();

        fpt::RunDiagnostics diagnostics;
        diagnostics.inputPath = cli.input;
        diagnostics.outputPath = cli.output;
        diagnostics.traceMode = traceModeName(cli.traceMode);
        diagnostics.thresholdMode = thresholdModeName(cli.thresholdMode);
        diagnostics.threshold = cli.threshold;
        diagnostics.percentile = cli.percentile;
        diagnostics.effectiveThreshold = result.effectiveThreshold;
        diagnostics.minArea = cli.minArea;
        diagnostics.minCells = cli.minCells;
        diagnostics.openRadius = cli.openRadius;
        diagnostics.closeRadius = cli.closeRadius;
        diagnostics.weakPercentile = cli.weakPercentile;
        diagnostics.weakConnectRadius = cli.weakConnectRadius;
        diagnostics.maxWeakComponentCells = cli.maxWeakComponentCells;
        diagnostics.minWeakComponentAspect = cli.minWeakComponentAspect;
        diagnostics.weakPathGate = cli.weakPathGate;
        diagnostics.weakPathSupportRatio = cli.weakPathSupportRatio;
        diagnostics.weakPathMaxUnsupportedRun = cli.weakPathMaxUnsupportedRun;
        diagnostics.branchAttributionShadow =
            cli.branchAttributionShadow;
        diagnostics.branchAttributionCandidateGroups =
            cli.branchAttributionCandidateGroups;
        diagnostics.localPolygonQuality = cli.localPolygonQuality;
        diagnostics.localQualityWidthRatio = cli.localQualityWidthRatio;
        diagnostics.localQualityMinExtraCells = cli.localQualityMinExtraCells;
        diagnostics.localQualitySupportThresholdRatio =
            cli.localQualitySupportThresholdRatio;
        diagnostics.localQualitySupportBufferCells =
            cli.localQualitySupportBufferCells;
        diagnostics.localQualityMaxAreaLossRatio =
            cli.localQualityMaxAreaLossRatio;
        diagnostics.localQualityMaxStrongCoverageLossRatio =
            cli.localQualityMaxStrongCoverageLossRatio;
        diagnostics.localQualityMaxWeakCoverageLossRatio =
            cli.localQualityMaxWeakCoverageLossRatio;
        diagnostics.localQualityMaxBackgroundLeakageIncreaseRatio =
            cli.localQualityMaxBackgroundLeakageIncreaseRatio;
        diagnostics.localQualityStabilityThresholdDeltaRatio =
            cli.localQualityStabilityThresholdDeltaRatio;
        diagnostics.localQualityMinStabilityJaccard =
            cli.localQualityMinStabilityJaccard;
        diagnostics.localQualityMaxCandidatesPerObject =
            cli.localQualityMaxCandidatesPerObject;
        diagnostics.localQualityMaxTotalCandidates =
            cli.localQualityMaxTotalCandidates;
        diagnostics.localQualityMaxWindowCells =
            cli.localQualityMaxWindowCells;
        diagnostics.localQualityMaxObjectCells =
            cli.localQualityMaxObjectCells;
        diagnostics.localQualityMaxEvidenceRecords =
            cli.localQualityMaxEvidenceRecords;
        diagnostics.beltRadius = cli.beltRadius;
        diagnostics.postBeltCloseRadius = cli.postBeltCloseRadius;
        diagnostics.attributeConstrainedBelt = cli.attributeConstrainedBelt;
        diagnostics.attributeEnvelopeRadius = cli.attributeEnvelopeRadius;
        diagnostics.minBranchLength = cli.minBranchLength;
        diagnostics.minJunctionBranchLength = cli.minJunctionBranchLength;
        diagnostics.minJunctionBranchMean = cli.minJunctionBranchMean;
        diagnostics.maxGapDistance = cli.maxGapDistance;
        diagnostics.maxGapAngle = cli.maxGapAngle;
        diagnostics.minGapSupportRatio = cli.minGapSupportRatio;
        diagnostics.rootAntiMerge = cli.rootAntiMerge;
        diagnostics.rootAntiMergeRadius = cli.rootAntiMergeRadius;
        diagnostics.rootAntiMergeSupportRatio = cli.rootAntiMergeSupportRatio;
        diagnostics.rootAntiMergeMaxBridgeValue = cli.rootAntiMergeMaxBridgeValue;
        diagnostics.rootAntiMergeMinPartCells = cli.rootAntiMergeMinPartCells;
        diagnostics.rootAntiMergeMinPartRatio = cli.rootAntiMergeMinPartRatio;
        diagnostics.rootAntiMergeWeakGuardThreshold = cli.rootAntiMergeWeakGuardThreshold;
        diagnostics.rootAntiMergeWeakGuardRadius = cli.rootAntiMergeWeakGuardRadius;
        diagnostics.localAntiMerge = cli.localAntiMerge;
        diagnostics.localAntiMergeLowThreshold = cli.localAntiMergeLowThreshold;
        diagnostics.localAntiMergeBridgeRadius = cli.localAntiMergeBridgeRadius;
        diagnostics.localAntiMergeMinPartCells = cli.localAntiMergeMinPartCells;
        diagnostics.localAntiMergeMinPartRatio = cli.localAntiMergeMinPartRatio;
        diagnostics.localAntiMergeMinBridgeCells = cli.localAntiMergeMinBridgeCells;
        diagnostics.localAntiMergeMinLowRatio = cli.localAntiMergeMinLowRatio;
        diagnostics.adaptiveAreaFilter = cli.adaptiveAreaFilter;
        diagnostics.adaptiveAreaMinCells = cli.adaptiveAreaMinCells;
        diagnostics.adaptiveAreaMinAspect = cli.adaptiveAreaMinAspect;
        diagnostics.adaptiveAreaMinMeanRatio = cli.adaptiveAreaMinMeanRatio;
        diagnostics.adaptiveAreaMinMaxRatio = cli.adaptiveAreaMinMaxRatio;
        diagnostics.residualAttributeFill = cli.residualAttributeFill;
        diagnostics.residualTrace = cli.residualTrace;
        diagnostics.residualPercentile = cli.residualPercentile;
        diagnostics.residualFillRadius = cli.residualFillRadius;
        diagnostics.residualEdgeFillRadius = cli.residualEdgeFillRadius;
        diagnostics.residualEdgeFillMinValueRatio = cli.residualEdgeFillMinValueRatio;
        diagnostics.residualEdgeFillMaxPatchSpan = cli.residualEdgeFillMaxPatchSpan;
        diagnostics.residualMinCells = cli.residualMinCells;
        diagnostics.residualTraceMinLength = cli.residualTraceMinLength;
        diagnostics.residualTraceMaxCells = cli.residualTraceMaxCells;
        diagnostics.residualCoveredGuardRadius = cli.residualCoveredGuardRadius;
        diagnostics.residualTraceDetached = cli.residualTraceDetached;
        diagnostics.residualDetachedMinCells = cli.residualDetachedMinCells;
        diagnostics.residualDetachedMinLength = cli.residualDetachedMinLength;
        diagnostics.residualDetachedMinAspect = cli.residualDetachedMinAspect;
        diagnostics.residualDetachedMinMeanRatio = cli.residualDetachedMinMeanRatio;
        diagnostics.residualDetachedMinMaxRatio = cli.residualDetachedMinMaxRatio;
        diagnostics.simplifyTolerance = cli.simplifyTolerance;
        diagnostics.smoothIterations = cli.smoothIterations;
        diagnostics.preserveCoverageAfterSmoothing = traceOptions.preserveCoverageAfterSmoothing;
        diagnostics.coveragePreserveMargin = traceOptions.coveragePreserveMargin;
        diagnostics.gridNx = grid.nx();
        diagnostics.gridNy = grid.ny();
        diagnostics.activeCellCount = result.activeCellCount;
        diagnostics.polygonCount = static_cast<int>(result.polygons.size());
        diagnostics.keptPolygonCount = kept;
        diagnostics.skeletonEndpoints = result.skeletonEndpoints;
        diagnostics.skeletonJunctions = result.skeletonJunctions;
        diagnostics.skeletonBranches = result.skeletonBranches;
        diagnostics.keptBranches = result.keptBranches;
        diagnostics.prunedBranches = result.prunedBranches;
        diagnostics.connectedGaps = result.connectedGaps;
        diagnostics.rootAntiMergeCandidateCutCells = result.rootAntiMergeCandidateCutCells;
        diagnostics.rootAntiMergeCutCells = result.rootAntiMergeCutCells;
        diagnostics.rootAntiMergeRejectedCutCells = result.rootAntiMergeRejectedCutCells;
        diagnostics.rootAntiMergeAcceptedComponents = result.rootAntiMergeAcceptedComponents;
        diagnostics.rootAntiMergeRejectedComponents = result.rootAntiMergeRejectedComponents;
        diagnostics.rootAntiMergeWeakGuardRejectedCutCells = result.rootAntiMergeWeakGuardRejectedCutCells;
        diagnostics.rootAntiMergeWeakGuardRejectedComponents = result.rootAntiMergeWeakGuardRejectedComponents;
        diagnostics.localAntiMergeCandidateCutCells = result.localAntiMergeCandidateCutCells;
        diagnostics.localAntiMergeCutCells = result.localAntiMergeCutCells;
        diagnostics.localAntiMergeRejectedCutCells = result.localAntiMergeRejectedCutCells;
        diagnostics.localAntiMergeAcceptedComponents = result.localAntiMergeAcceptedComponents;
        diagnostics.localAntiMergeRejectedComponents = result.localAntiMergeRejectedComponents;
        diagnostics.residualFillCandidateCells = result.residualFillCandidateCells;
        diagnostics.residualFillAddedCells = result.residualFillAddedCells;
        diagnostics.residualFillRejectedComponents = result.residualFillRejectedComponents;
        diagnostics.weakPathCandidateCells = result.weakPathCandidateCells;
        diagnostics.weakPathAcceptedCells = result.weakPathAcceptedCells;
        diagnostics.weakPathRejectedCells = result.weakPathRejectedCells;
        diagnostics.weakPathSeedCells = result.weakPathSeedCells;
        diagnostics.weakPathSeedContactCells =
            result.weakPathSeedContactCells;
        diagnostics.weakPathRootConflictRejectedCells =
            result.weakPathRootConflictRejectedCells;
        diagnostics.weakPathCandidateComponents =
            result.weakPathCandidateComponents;
        diagnostics.weakPathAcceptedComponents =
            result.weakPathAcceptedComponents;
        diagnostics.weakPathRejectedComponents =
            result.weakPathRejectedComponents;
        diagnostics.weakPathPartiallyRejectedComponents =
            result.weakPathPartiallyRejectedComponents;
        diagnostics.weakPathEvidenceEvaluatedFragments =
            result.weakPathEvidenceEvaluatedFragments;
        diagnostics.weakPathAttributeRejectedFragments =
            result.weakPathAttributeRejectedFragments;
        diagnostics.weakPathShapeRejectedFragments =
            result.weakPathShapeRejectedFragments;
        diagnostics.weakPathDirectionRejectedFragments =
            result.weakPathDirectionRejectedFragments;
        diagnostics.branchAttributionJunctionClusters =
            result.branchAttributionJunctionClusters;
        diagnostics.branchAttributionAcceptedPairs =
            result.branchAttributionAcceptedPairs;
        diagnostics.branchAttributionAmbiguousClusters =
            result.branchAttributionAmbiguousClusters;
        diagnostics.branchAttributionUnmatchedEnds =
            result.branchAttributionUnmatchedEnds;
        diagnostics.branchAttributionGreedyFallbackClusters =
            result.branchAttributionGreedyFallbackClusters;
        diagnostics.branchAttributionCandidatePairingDifferingClusters =
            result.branchAttributionCandidatePairingDifferingClusters;
        diagnostics.branchAttributionCandidateDifferingClusters =
            result.branchAttributionCandidateDifferingClusters;
        diagnostics.branchAttributionAcceptedPairingDifferingClusters =
            result.branchAttributionAcceptedPairingDifferingClusters;
        diagnostics.branchAttributionAcceptedDifferingClusters =
            result.branchAttributionAcceptedDifferingClusters;
        diagnostics.branchAttributionCandidateGroupingAppliedClusters =
            result.branchAttributionCandidateGroupingAppliedClusters;
        diagnostics.branchAttributionCandidateGroupingAmbiguousFallbackClusters =
            result.branchAttributionCandidateGroupingAmbiguousFallbackClusters;
        diagnostics.branchAttributionCandidateGroupingLegacyFallbackClusters =
            result.branchAttributionCandidateGroupingLegacyFallbackClusters;
        diagnostics.branchAttributionCandidateGroupingComplexityFallbackClusters =
            result.branchAttributionCandidateGroupingComplexityFallbackClusters;
        diagnostics.branchAttributionCandidateGroupingSpanFallbackClusters =
            result.branchAttributionCandidateGroupingSpanFallbackClusters;
        diagnostics.branchAttributionCandidateGroupingExcludedInternalBranches =
            result.branchAttributionCandidateGroupingExcludedInternalBranches;
        diagnostics.branchAttributionCandidateGroupingRootGroups =
            result.branchAttributionCandidateGroupingRootGroups;
        diagnostics.branchAttributionPairingDifferingClusters =
            result.branchAttributionPairingDifferingClusters;
        diagnostics.branchAttributionDifferingClusters =
            result.branchAttributionDifferingClusters;
        diagnostics.localQualityDetectedCount =
            result.localPolygonQualityStats.detectedCount;
        diagnostics.localQualityAcceptedCount =
            result.localPolygonQualityStats.acceptedCount;
        diagnostics.localQualityReviewCount =
            result.localPolygonQualityStats.reviewCount;
        diagnostics.localQualityRejectedCount =
            result.localPolygonQualityStats.rejectedCount;
        diagnostics.localQualityInsufficientEvidenceRejectedCount =
            result.localPolygonQualityStats.insufficientEvidenceRejectedCount;
        diagnostics.localQualityNoMeaningfulChangeRejectedCount =
            result.localPolygonQualityStats.noMeaningfulChangeRejectedCount;
        diagnostics.localQualityAreaLossRejectedCount =
            result.localPolygonQualityStats.areaLossRejectedCount;
        diagnostics.localQualityStrongCoverageRejectedCount =
            result.localPolygonQualityStats.strongCoverageRejectedCount;
        diagnostics.localQualityWeakCoverageRejectedCount =
            result.localPolygonQualityStats.weakCoverageRejectedCount;
        diagnostics.localQualityBackgroundLeakageRejectedCount =
            result.localPolygonQualityStats.backgroundLeakageRejectedCount;
        diagnostics.localQualityTopologyRejectedCount =
            result.localPolygonQualityStats.topologyRejectedCount;
        diagnostics.localQualityStabilityRejectedCount =
            result.localPolygonQualityStats.stabilityRejectedCount;
        diagnostics.localQualityInvalidGeometryRejectedCount =
            result.localPolygonQualityStats.invalidGeometryRejectedCount;
        diagnostics.localQualityObjectBudgetReviewCount =
            result.localPolygonQualityStats.objectBudgetReviewCount;
        diagnostics.localQualityPostRepairFilteredCount =
            result.localPolygonQualityStats.postRepairFilteredCount;
        diagnostics.localQualityTotalEvidence =
            result.localPolygonQualityStats.totalEvidence;
        diagnostics.localQualityStoredEvidence =
            result.localPolygonQualityStats.storedEvidence;
        diagnostics.localQualityTruncatedEvidence =
            result.localPolygonQualityStats.truncatedEvidence;
        diagnostics.localPolygonQualityEvidence =
            result.localPolygonQualityEvidence;
        diagnostics.suppressedInternalRings = result.suppressedInternalRings;
        diagnostics.coveragePreserveSimplifyFallbackPolygons = result.coveragePreserveSimplifyFallbackPolygons;
        diagnostics.coveragePreserveSmoothFallbackPolygons = result.coveragePreserveSmoothFallbackPolygons;
        diagnostics.coveragePreserveTopologyOverridePolygons = result.coveragePreserveTopologyOverridePolygons;
        diagnostics.openRingCount = topology.openRingCount;
        diagnostics.selfIntersectingPolygonCount = topology.selfIntersectingPolygonCount;
        diagnostics.polygonIntersectionCount = topology.polygonIntersectionCount;
        diagnostics.readMs = std::chrono::duration_cast<std::chrono::milliseconds>(afterRead - start).count();
        diagnostics.traceMs = std::chrono::duration_cast<std::chrono::milliseconds>(afterTrace - afterRead).count();
        diagnostics.topologyMs = std::chrono::duration_cast<std::chrono::milliseconds>(afterTopology - afterTrace).count();
        diagnostics.writeMs = std::chrono::duration_cast<std::chrono::milliseconds>(afterWrites - afterTopology).count();
        diagnostics.elapsedMs = elapsedMs;
        fpt::writeDiagnosticsJson(cli.diagnosticsOutput, diagnostics);

        std::cout << "FaultPolyTrace completed.\n";
        std::cout << "Grid: " << grid.nx() << " x " << grid.ny() << "\n";
        std::cout << "Effective threshold: " << result.effectiveThreshold << "\n";
        std::cout << "Active cells: " << result.activeCellCount << "\n";
        if (cli.weakPathGate) {
            std::cout << "Weak path candidate/accepted/rejected cells: "
                      << result.weakPathCandidateCells << "/"
                      << result.weakPathAcceptedCells << "/"
                      << result.weakPathRejectedCells << "\n";
            std::cout << "Weak path candidate/accepted/rejected components: "
                      << result.weakPathCandidateComponents << "/"
                      << result.weakPathAcceptedComponents << "/"
                      << result.weakPathRejectedComponents << "\n";
            std::cout << "Weak path partial-components/root-conflict-cells/evidence-fragments: "
                      << result.weakPathPartiallyRejectedComponents << "/"
                      << result.weakPathRootConflictRejectedCells << "/"
                      << result.weakPathEvidenceEvaluatedFragments << "\n";
        }
        if (cli.branchAttributionShadow) {
            std::cout << "Branch attribution shadow clusters/pairs/ambiguous/unmatched/fallback: "
                      << result.branchAttributionJunctionClusters << "/"
                      << result.branchAttributionAcceptedPairs << "/"
                      << result.branchAttributionAmbiguousClusters << "/"
                      << result.branchAttributionUnmatchedEnds << "/"
                      << result.branchAttributionGreedyFallbackClusters << "\n";
            std::cout << "Branch attribution candidate root/direct differences: "
                      << result.branchAttributionCandidateDifferingClusters
                      << "/"
                      << result.branchAttributionCandidatePairingDifferingClusters
                      << "\n";
            std::cout << "Branch attribution accepted root/direct differences: "
                      << result.branchAttributionAcceptedDifferingClusters
                      << "/"
                      << result.branchAttributionAcceptedPairingDifferingClusters
                      << "\n";
        }
        if (cli.branchAttributionCandidateGroups) {
            std::cout << "Branch attribution candidate grouping applied/ambiguous-fallback/legacy-fallback/complexity-fallback/span-fallback: "
                      << result.branchAttributionCandidateGroupingAppliedClusters
                      << "/"
                      << result.branchAttributionCandidateGroupingAmbiguousFallbackClusters
                      << "/"
                      << result.branchAttributionCandidateGroupingLegacyFallbackClusters
                      << "/"
                      << result.branchAttributionCandidateGroupingComplexityFallbackClusters
                      << "/"
                      << result.branchAttributionCandidateGroupingSpanFallbackClusters
                      << "\n";
            std::cout << "Branch attribution candidate grouping excluded-internal/root-groups: "
                      << result.branchAttributionCandidateGroupingExcludedInternalBranches
                      << "/"
                      << result.branchAttributionCandidateGroupingRootGroups
                      << "\n";
        }
        if (cli.localPolygonQuality) {
            std::cout << "Local polygon quality detected/accepted/review/rejected: "
                      << result.localPolygonQualityStats.detectedCount << "/"
                      << result.localPolygonQualityStats.acceptedCount << "/"
                      << result.localPolygonQualityStats.reviewCount << "/"
                      << result.localPolygonQualityStats.rejectedCount << "\n";
            std::cout << "Local polygon quality rejected evidence/no-change/area/strong/weak/background/topology/stability/invalid-geometry: "
                      << result.localPolygonQualityStats.insufficientEvidenceRejectedCount << "/"
                      << result.localPolygonQualityStats.noMeaningfulChangeRejectedCount << "/"
                      << result.localPolygonQualityStats.areaLossRejectedCount << "/"
                      << result.localPolygonQualityStats.strongCoverageRejectedCount << "/"
                      << result.localPolygonQualityStats.weakCoverageRejectedCount << "/"
                      << result.localPolygonQualityStats.backgroundLeakageRejectedCount << "/"
                      << result.localPolygonQualityStats.topologyRejectedCount << "/"
                      << result.localPolygonQualityStats.stabilityRejectedCount << "/"
                      << result.localPolygonQualityStats.invalidGeometryRejectedCount << "\n";
            std::cout << "Local polygon quality object-budget reviews: "
                      << result.localPolygonQualityStats.objectBudgetReviewCount << "\n";
        }
        if (cli.traceMode == fpt::TraceMode::RidgeBelt) {
            std::cout << "Skeleton endpoints: " << result.skeletonEndpoints << "\n";
            std::cout << "Skeleton junctions: " << result.skeletonJunctions << "\n";
            std::cout << "Skeleton branches: " << result.skeletonBranches << "\n";
            std::cout << "Connected gaps: " << result.connectedGaps << "\n";
            std::cout << "Kept branches: " << result.keptBranches << "\n";
            std::cout << "Pruned branches: " << result.prunedBranches << "\n";
            if (cli.rootAntiMerge) {
                std::cout << "Root anti-merge candidate cut cells: " << result.rootAntiMergeCandidateCutCells << "\n";
                std::cout << "Root anti-merge accepted cut cells: " << result.rootAntiMergeCutCells << "\n";
                std::cout << "Root anti-merge rejected cut cells: " << result.rootAntiMergeRejectedCutCells << "\n";
                std::cout << "Root anti-merge accepted/rejected components: "
                          << result.rootAntiMergeAcceptedComponents << "/"
                          << result.rootAntiMergeRejectedComponents << "\n";
                std::cout << "Root anti-merge weak-guard rejected cut cells: "
                          << result.rootAntiMergeWeakGuardRejectedCutCells << "\n";
                std::cout << "Root anti-merge weak-guard rejected components: "
                          << result.rootAntiMergeWeakGuardRejectedComponents << "\n";
            }
            if (cli.localAntiMerge) {
                std::cout << "Local anti-merge candidate cut cells: " << result.localAntiMergeCandidateCutCells << "\n";
                std::cout << "Local anti-merge accepted cut cells: " << result.localAntiMergeCutCells << "\n";
                std::cout << "Local anti-merge rejected cut cells: " << result.localAntiMergeRejectedCutCells << "\n";
                std::cout << "Local anti-merge accepted/rejected components: "
                          << result.localAntiMergeAcceptedComponents << "/"
                          << result.localAntiMergeRejectedComponents << "\n";
            }
        }
        std::cout << "Polygons: " << result.polygons.size() << " total, " << kept << " kept\n";
        std::cout << "Suppressed internal rings: " << result.suppressedInternalRings << "\n";
        std::cout << "Topology open rings: " << topology.openRingCount << "\n";
        std::cout << "Topology self-intersections: " << topology.selfIntersectingPolygonCount << "\n";
        std::cout << "Topology polygon intersections: " << topology.polygonIntersectionCount << "\n";
        std::cout << "Elapsed: " << elapsedMs << " ms\n";
        std::cout << "Output: " << cli.output << "\n";
        std::cout << "Diagnostics: " << cli.diagnosticsOutput << "\n";
        std::cout << "Topology details: " << cli.topologyOutput << "\n";
        if (cli.traceMode == fpt::TraceMode::RidgeBelt) {
            std::cout << "Branches: " << cli.branchesOutput << "\n";
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n\n";
        printUsage();
        return 1;
    }
}
