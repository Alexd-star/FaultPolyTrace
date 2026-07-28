#include "GeoEastBoAdapter.h"

#include "../../src/core/FaultTracer.h"
#include "../../src/core/Topology.h"
#include "../../src/io/AsciiGridIO.h"
#include "../../src/io/AtomicOutputFile.h"
#include "../../src/io/DiagnosticsIO.h"

#include <chrono>
#include <array>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <limits>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "boproject.h"
#include "bosystemroot.h"

namespace geobo = ngp::bo;

#ifdef _WIN32
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char* outputString);
#endif

namespace {

struct Options {
    std::string databaseName;
    std::string projectName;
    std::string userName;
    std::string password;
    std::string gridName;
    std::string outputName;
    std::string topologyOutputPath;
    std::string previewGridOutputPath;
    std::string previewPolygonsOutputPath;
    std::string exportFaultPolygonsOutputPath;
    std::string importGeoEastDatPath;
    bool listGrids = false;
    bool listFaultPolygons = false;
    bool validateGridOnly = false;
    bool overwriteGrid = false;
    bool deleteOutput = false;
    fpt_geoeast::FaultPolygonStyle polygonStyle;
    fpt::TraceOptions trace;
};

void printUsage() {
    std::cout
        << "Usage:\n"
        << "  faultpolytrace_geoeast_runner --db <database> --project <project> "
        << "--user <user> [--password <password>] --grid <BOMapGrid> --output <BOFaultPolygon> [options]\n\n"
        << "Options:\n"
        << "  --trace-mode region|ridge-belt\n"
        << "  --list-grids\n"
        << "  --list-fault-polygons\n"
        << "  --validate-grid\n"
        << "  --delete-output\n"
        << "  --import-geoeast-dat <test.dat>\n"
        << "  --overwrite-grid\n"
        << "  --threshold-mode fixed|percentile|positive-percentile\n"
        << "  --threshold <value>\n"
        << "  --percentile <0-1>\n"
        << "  --min-area <value>\n"
        << "  --min-cells <count>\n"
        << "  --open-radius <count>\n"
        << "  --close-radius <count>\n"
        << "  --weak-percentile <0-1>\n"
        << "  --weak-connect-radius <count>\n"
        << "  --max-weak-component-cells <count>\n"
        << "  --min-weak-component-aspect <value>\n"
        << "  --weak-path-gate | --no-weak-path-gate\n"
        << "  --branch-attribution-shadow | --no-branch-attribution-shadow\n"
        << "  --local-polygon-quality | --no-local-polygon-quality\n"
        << "  --belt-radius <count>\n"
        << "  --post-belt-close-radius <count>\n"
        << "  --attribute-constrained-belt\n"
        << "  --no-attribute-constrained-belt\n"
        << "  --attribute-envelope-radius <count>\n"
        << "  --min-branch-length <count>\n"
        << "  --min-junction-branch-length <count>\n"
        << "  --min-junction-branch-mean <value>\n"
        << "  --max-gap-distance <count>\n"
        << "  --max-gap-angle <degrees>\n"
        << "  --min-gap-support-ratio <0-1>\n"
        << "  --separate-branch-belts\n"
        << "  --no-separate-branch-belts\n"
        << "  --root-anti-merge\n"
        << "  --no-root-anti-merge\n"
        << "  --root-anti-merge-radius <count>\n"
        << "  --root-anti-merge-support-ratio <0-1>\n"
        << "  --root-anti-merge-max-bridge-value <value>\n"
        << "  --root-anti-merge-min-part-cells <count>\n"
        << "  --root-anti-merge-min-part-ratio <0-0.5>\n"
        << "  --root-anti-merge-weak-guard-threshold <value>\n"
        << "  --root-anti-merge-weak-guard-radius <count>\n"
        << "  --local-anti-merge\n"
        << "  --no-local-anti-merge\n"
        << "  --local-anti-merge-low-threshold <value>\n"
        << "  --local-anti-merge-bridge-radius <count>\n"
        << "  --local-anti-merge-min-part-cells <count>\n"
        << "  --local-anti-merge-min-part-ratio <0-0.5>\n"
        << "  --local-anti-merge-min-bridge-cells <count>\n"
        << "  --local-anti-merge-min-low-ratio <0-1>\n"
        << "  --no-adaptive-area-filter\n"
        << "  --adaptive-area-min-cells <count>\n"
        << "  --adaptive-area-min-aspect <value>\n"
        << "  --adaptive-area-min-mean-ratio <value>\n"
        << "  --adaptive-area-min-max-ratio <value>\n"
        << "  --no-residual-fill\n"
        << "  --residual-trace\n"
        << "  --no-residual-trace\n"
        << "  --residual-percentile <0-1>\n"
        << "  --residual-fill-radius <count>\n"
        << "  --residual-edge-fill-radius <count>\n"
        << "  --residual-edge-fill-min-value-ratio <value>\n"
        << "  --residual-edge-fill-max-patch-span <count>\n"
        << "  --residual-min-cells <count>\n"
        << "  --residual-trace-min-length <count>\n"
        << "  --residual-trace-max-cells <count>\n"
        << "  --residual-covered-guard-radius <count>\n"
        << "  --no-residual-detached-trace\n"
        << "  --residual-detached-min-cells <count>\n"
        << "  --residual-detached-min-length <count>\n"
        << "  --residual-detached-min-aspect <value>\n"
        << "  --residual-detached-min-mean-ratio <value>\n"
        << "  --residual-detached-min-max-ratio <value>\n"
        << "  --simplify-tolerance <dist>\n"
        << "  --smooth-iterations <count>\n"
        << "  --polygon-color <cyan|yellow|magenta|white|green|blue|red|R,G,B|#RRGGBB>\n"
        << "  --polygon-linewidth <value>\n"
        << "  --topology-output <csv>\n"
        << "  --view-grid-output <txt>\n"
        << "  --view-polygons-output <csv>\n"
        << "  --export-fault-polygons-output <csv>\n"
        << "  --preview-grid-output <txt> (compatibility alias)\n"
        << "  --preview-polygons-output <csv> (compatibility alias)\n";
}

int clampColor(int value) {
    return std::max(0, std::min(255, value));
}

fpt_geoeast::FaultPolygonStyle parsePolygonColor(
    const std::string& text,
    const fpt_geoeast::FaultPolygonStyle& current) {
    fpt_geoeast::FaultPolygonStyle style = current;
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (lower == "cyan") {
        style.red = 0; style.green = 255; style.blue = 255;
    } else if (lower == "yellow") {
        style.red = 255; style.green = 255; style.blue = 0;
    } else if (lower == "magenta") {
        style.red = 255; style.green = 0; style.blue = 255;
    } else if (lower == "white") {
        style.red = 255; style.green = 255; style.blue = 255;
    } else if (lower == "green") {
        style.red = 0; style.green = 255; style.blue = 0;
    } else if (lower == "blue") {
        style.red = 0; style.green = 0; style.blue = 255;
    } else if (lower == "red") {
        style.red = 255; style.green = 0; style.blue = 0;
    } else if (lower.size() == 7 && lower[0] == '#') {
        style.red = clampColor(std::stoi(lower.substr(1, 2), nullptr, 16));
        style.green = clampColor(std::stoi(lower.substr(3, 2), nullptr, 16));
        style.blue = clampColor(std::stoi(lower.substr(5, 2), nullptr, 16));
    } else {
        const std::size_t first = lower.find(',');
        const std::size_t second = first == std::string::npos ? std::string::npos : lower.find(',', first + 1);
        if (first == std::string::npos || second == std::string::npos) {
            throw std::runtime_error("Invalid --polygon-color: " + text);
        }
        style.red = clampColor(std::stoi(lower.substr(0, first)));
        style.green = clampColor(std::stoi(lower.substr(first + 1, second - first - 1)));
        style.blue = clampColor(std::stoi(lower.substr(second + 1)));
    }
    return style;
}

void debugLog(const std::string& message) {
#ifdef _WIN32
    OutputDebugStringA((message + "\n").c_str());
#endif
    std::cerr << message << "\n";
}

void validateGeoEastEnvironment() {
    const char* required[] = {"NGP", "IECO", "NDPDIR", "NGPDIR", "GEOEAST", "PG_HOME"};
    for (const char* name : required) {
        const char* value = std::getenv(name);
        if (!value || !*value) {
            throw std::runtime_error(std::string("Missing GeoEast environment variable: ") + name);
        }
        debugLog(std::string("Env ") + name + "=" + value);
    }

    const std::string hostFile = std::string(std::getenv("PG_HOME")) + "/network/hostname.pg";
    std::ifstream input(hostFile.c_str());
    if (!input.good()) {
        throw std::runtime_error("Missing GeoEast PostgreSQL host file: " + hostFile);
    }
    debugLog("GeoEast PostgreSQL host file found: " + hostFile);
}

Options parseArgs(int argc, char** argv) {
    Options options;
    auto envOrEmpty = [](const char* name) -> std::string {
        const char* value = std::getenv(name);
        return value && *value ? value : "";
    };
    auto envOrDefault = [&](const char* name, const std::string& fallback) -> std::string {
        const std::string value = envOrEmpty(name);
        return value.empty() ? fallback : value;
    };

    options.databaseName = envOrDefault("FPT_GEOEAST_DB", "ndp");
    options.projectName = envOrEmpty("FPT_GEOEAST_PROJECT");
    options.userName = envOrDefault("FPT_GEOEAST_USER", envOrEmpty("USERNAME"));
    options.password = "";
    options.gridName = envOrEmpty("FPT_GEOEAST_GRID");
    options.outputName = "FaultPolyTrace_Result";
    options.trace.traceMode = fpt::TraceMode::RidgeBelt;
    options.trace.thresholdMode = fpt::ThresholdMode::Percentile;
    options.trace.percentile = 0.98;
    options.trace.closeRadius = 1;
    options.trace.weakPercentile = 0.965;
    options.trace.weakConnectRadius = 1;
    options.trace.maxWeakComponentCells = 20000;
    options.trace.minWeakComponentAspect = 3.0;
    options.trace.beltRadius = 3;
    options.trace.postBeltCloseRadius = 1;
    options.trace.attributeConstrainedBelt = true;
    options.trace.attributeEnvelopeRadius = 1;
    options.trace.minBranchLength = 4;
    options.trace.minJunctionBranchLength = 2;
    options.trace.minJunctionBranchMean = 0.94;
    options.trace.maxGapDistance = 12;
    options.trace.maxGapAngle = 55.0;
    options.trace.minGapSupportRatio = 0.25;
    options.trace.residualAttributeFill = true;
    options.trace.residualTrace = false;
    options.trace.residualPercentile = 0.955;
    options.trace.residualFillRadius = 4;
    options.trace.residualEdgeFillRadius = 2;
    options.trace.residualMinCells = 2;
    options.trace.residualTraceMinLength = 6;
    options.trace.residualTraceMaxCells = 80;
    options.trace.residualCoveredGuardRadius = 0;
    options.trace.minArea = 120.0;
    options.trace.simplifyTolerance = 2.0;
    options.trace.smoothIterations = 1;

    if (argc == 1) {
        throw std::runtime_error("No command-line arguments supplied. Use --help to view GeoEast runner usage.");
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto requireValue = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for " + name);
            }
            return argv[++i];
        };

        if (arg == "--db") {
            options.databaseName = requireValue(arg);
        } else if (arg == "--project") {
            options.projectName = requireValue(arg);
        } else if (arg == "--user") {
            options.userName = requireValue(arg);
        } else if (arg == "--password") {
            options.password = requireValue(arg);
        } else if (arg == "--grid") {
            options.gridName = requireValue(arg);
        } else if (arg == "--output") {
            options.outputName = requireValue(arg);
        } else if (arg == "--topology-output") {
            options.topologyOutputPath = requireValue(arg);
        } else if (arg == "--preview-grid-output" || arg == "--view-grid-output") {
            options.previewGridOutputPath = requireValue(arg);
        } else if (arg == "--preview-polygons-output" || arg == "--view-polygons-output") {
            options.previewPolygonsOutputPath = requireValue(arg);
        } else if (arg == "--export-fault-polygons-output") {
            options.exportFaultPolygonsOutputPath = requireValue(arg);
        } else if (arg == "--import-geoeast-dat") {
            options.importGeoEastDatPath = requireValue(arg);
        } else if (arg == "--list-grids") {
            options.listGrids = true;
        } else if (arg == "--list-fault-polygons") {
            options.listFaultPolygons = true;
        } else if (arg == "--validate-grid") {
            options.validateGridOnly = true;
        } else if (arg == "--delete-output") {
            options.deleteOutput = true;
        } else if (arg == "--overwrite-grid") {
            options.overwriteGrid = true;
        } else if (arg == "--trace-mode") {
            const std::string mode = requireValue(arg);
            if (mode == "region") {
                options.trace.traceMode = fpt::TraceMode::Region;
            } else if (mode == "ridge-belt") {
                options.trace.traceMode = fpt::TraceMode::RidgeBelt;
            } else {
                throw std::runtime_error("Unknown trace mode: " + mode);
            }
        } else if (arg == "--threshold-mode") {
            const std::string mode = requireValue(arg);
            if (mode == "fixed") {
                options.trace.thresholdMode = fpt::ThresholdMode::Fixed;
            } else if (mode == "percentile") {
                options.trace.thresholdMode = fpt::ThresholdMode::Percentile;
            } else if (mode == "positive-percentile") {
                options.trace.thresholdMode = fpt::ThresholdMode::PositivePercentile;
            } else {
                throw std::runtime_error("Unknown threshold mode: " + mode);
            }
        } else if (arg == "--threshold") {
            options.trace.threshold = std::stod(requireValue(arg));
        } else if (arg == "--percentile") {
            options.trace.percentile = std::stod(requireValue(arg));
        } else if (arg == "--min-area") {
            options.trace.minArea = std::stod(requireValue(arg));
        } else if (arg == "--min-cells") {
            options.trace.minCells = std::stoi(requireValue(arg));
        } else if (arg == "--open-radius") {
            options.trace.openRadius = std::stoi(requireValue(arg));
        } else if (arg == "--close-radius") {
            options.trace.closeRadius = std::stoi(requireValue(arg));
        } else if (arg == "--weak-percentile") {
            options.trace.weakPercentile = std::stod(requireValue(arg));
        } else if (arg == "--weak-connect-radius") {
            options.trace.weakConnectRadius = std::stoi(requireValue(arg));
        } else if (arg == "--max-weak-component-cells") {
            options.trace.maxWeakComponentCells = std::stoi(requireValue(arg));
        } else if (arg == "--min-weak-component-aspect") {
            options.trace.minWeakComponentAspect = std::stod(requireValue(arg));
        } else if (arg == "--weak-path-gate") {
            options.trace.weakPathGate = true;
        } else if (arg == "--no-weak-path-gate") {
            options.trace.weakPathGate = false;
        } else if (arg == "--branch-attribution-shadow") {
            options.trace.branchAttributionShadow = true;
        } else if (arg == "--no-branch-attribution-shadow") {
            options.trace.branchAttributionShadow = false;
        } else if (arg == "--local-polygon-quality") {
            options.trace.localPolygonQuality = true;
        } else if (arg == "--no-local-polygon-quality") {
            options.trace.localPolygonQuality = false;
        } else if (arg == "--belt-radius") {
            options.trace.beltRadius = std::stoi(requireValue(arg));
        } else if (arg == "--post-belt-close-radius") {
            options.trace.postBeltCloseRadius = std::stoi(requireValue(arg));
        } else if (arg == "--attribute-constrained-belt") {
            options.trace.attributeConstrainedBelt = true;
        } else if (arg == "--no-attribute-constrained-belt") {
            options.trace.attributeConstrainedBelt = false;
        } else if (arg == "--attribute-envelope-radius") {
            options.trace.attributeEnvelopeRadius = std::stoi(requireValue(arg));
        } else if (arg == "--min-branch-length") {
            options.trace.minBranchLength = std::stoi(requireValue(arg));
        } else if (arg == "--min-junction-branch-length") {
            options.trace.minJunctionBranchLength = std::stoi(requireValue(arg));
        } else if (arg == "--min-junction-branch-mean") {
            options.trace.minJunctionBranchMean = std::stod(requireValue(arg));
        } else if (arg == "--max-gap-distance") {
            options.trace.maxGapDistance = std::stoi(requireValue(arg));
        } else if (arg == "--max-gap-angle") {
            options.trace.maxGapAngle = std::stod(requireValue(arg));
        } else if (arg == "--min-gap-support-ratio") {
            options.trace.minGapSupportRatio = std::stod(requireValue(arg));
        } else if (arg == "--separate-branch-belts") {
            options.trace.separateBranchBelts = true;
        } else if (arg == "--no-separate-branch-belts") {
            options.trace.separateBranchBelts = false;
        } else if (arg == "--root-anti-merge") {
            options.trace.rootAntiMerge = true;
        } else if (arg == "--no-root-anti-merge") {
            options.trace.rootAntiMerge = false;
        } else if (arg == "--root-anti-merge-radius") {
            options.trace.rootAntiMergeRadius = std::stoi(requireValue(arg));
        } else if (arg == "--root-anti-merge-support-ratio") {
            options.trace.rootAntiMergeSupportRatio = std::stod(requireValue(arg));
        } else if (arg == "--root-anti-merge-max-bridge-value") {
            options.trace.rootAntiMergeMaxBridgeValue = std::stod(requireValue(arg));
        } else if (arg == "--root-anti-merge-min-part-cells") {
            options.trace.rootAntiMergeMinPartCells = std::stoi(requireValue(arg));
        } else if (arg == "--root-anti-merge-min-part-ratio") {
            options.trace.rootAntiMergeMinPartRatio = std::stod(requireValue(arg));
        } else if (arg == "--root-anti-merge-weak-guard-threshold") {
            options.trace.rootAntiMergeWeakGuardThreshold = std::stod(requireValue(arg));
        } else if (arg == "--root-anti-merge-weak-guard-radius") {
            options.trace.rootAntiMergeWeakGuardRadius = std::stoi(requireValue(arg));
        } else if (arg == "--local-anti-merge") {
            options.trace.localAntiMerge = true;
        } else if (arg == "--no-local-anti-merge") {
            options.trace.localAntiMerge = false;
        } else if (arg == "--local-anti-merge-low-threshold") {
            options.trace.localAntiMergeLowThreshold = std::stod(requireValue(arg));
        } else if (arg == "--local-anti-merge-bridge-radius") {
            options.trace.localAntiMergeBridgeRadius = std::stoi(requireValue(arg));
        } else if (arg == "--local-anti-merge-min-part-cells") {
            options.trace.localAntiMergeMinPartCells = std::stoi(requireValue(arg));
        } else if (arg == "--local-anti-merge-min-part-ratio") {
            options.trace.localAntiMergeMinPartRatio = std::stod(requireValue(arg));
        } else if (arg == "--local-anti-merge-min-bridge-cells") {
            options.trace.localAntiMergeMinBridgeCells = std::stoi(requireValue(arg));
        } else if (arg == "--local-anti-merge-min-low-ratio") {
            options.trace.localAntiMergeMinLowRatio = std::stod(requireValue(arg));
        } else if (arg == "--no-adaptive-area-filter") {
            options.trace.adaptiveAreaFilter = false;
        } else if (arg == "--adaptive-area-min-cells") {
            options.trace.adaptiveAreaMinCells = std::stoi(requireValue(arg));
        } else if (arg == "--adaptive-area-min-aspect") {
            options.trace.adaptiveAreaMinAspect = std::stod(requireValue(arg));
        } else if (arg == "--adaptive-area-min-mean-ratio") {
            options.trace.adaptiveAreaMinMeanRatio = std::stod(requireValue(arg));
        } else if (arg == "--adaptive-area-min-max-ratio") {
            options.trace.adaptiveAreaMinMaxRatio = std::stod(requireValue(arg));
        } else if (arg == "--no-residual-fill") {
            options.trace.residualAttributeFill = false;
        } else if (arg == "--residual-trace") {
            options.trace.residualTrace = true;
        } else if (arg == "--no-residual-trace") {
            options.trace.residualTrace = false;
        } else if (arg == "--residual-percentile") {
            options.trace.residualPercentile = std::stod(requireValue(arg));
        } else if (arg == "--residual-fill-radius") {
            options.trace.residualFillRadius = std::stoi(requireValue(arg));
        } else if (arg == "--residual-edge-fill-radius") {
            options.trace.residualEdgeFillRadius = std::stoi(requireValue(arg));
        } else if (arg == "--residual-edge-fill-min-value-ratio") {
            options.trace.residualEdgeFillMinValueRatio = std::stod(requireValue(arg));
        } else if (arg == "--residual-edge-fill-max-patch-span") {
            options.trace.residualEdgeFillMaxPatchSpan = std::stoi(requireValue(arg));
        } else if (arg == "--residual-min-cells") {
            options.trace.residualMinCells = std::stoi(requireValue(arg));
        } else if (arg == "--residual-trace-min-length") {
            options.trace.residualTraceMinLength = std::stoi(requireValue(arg));
        } else if (arg == "--residual-trace-max-cells") {
            options.trace.residualTraceMaxCells = std::stoi(requireValue(arg));
        } else if (arg == "--residual-covered-guard-radius") {
            options.trace.residualCoveredGuardRadius = std::stoi(requireValue(arg));
        } else if (arg == "--no-residual-detached-trace") {
            options.trace.residualTraceDetached = false;
        } else if (arg == "--residual-detached-min-cells") {
            options.trace.residualDetachedMinCells = std::stoi(requireValue(arg));
        } else if (arg == "--residual-detached-min-length") {
            options.trace.residualDetachedMinLength = std::stoi(requireValue(arg));
        } else if (arg == "--residual-detached-min-aspect") {
            options.trace.residualDetachedMinAspect = std::stod(requireValue(arg));
        } else if (arg == "--residual-detached-min-mean-ratio") {
            options.trace.residualDetachedMinMeanRatio = std::stod(requireValue(arg));
        } else if (arg == "--residual-detached-min-max-ratio") {
            options.trace.residualDetachedMinMaxRatio = std::stod(requireValue(arg));
        } else if (arg == "--simplify-tolerance") {
            options.trace.simplifyTolerance = std::stod(requireValue(arg));
        } else if (arg == "--smooth-iterations") {
            options.trace.smoothIterations = std::stoi(requireValue(arg));
        } else if (arg == "--polygon-color") {
            options.polygonStyle = parsePolygonColor(requireValue(arg), options.polygonStyle);
        } else if (arg == "--polygon-linewidth") {
            options.polygonStyle.lineWidth = std::stod(requireValue(arg));
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    if (options.databaseName.empty() || options.projectName.empty() || options.userName.empty()) {
        throw std::runtime_error("Missing required GeoEast connection or data arguments.");
    }
    if (!options.listGrids && !options.listFaultPolygons && options.exportFaultPolygonsOutputPath.empty() && !options.deleteOutput && options.gridName.empty()) {
        throw std::runtime_error("Missing --grid. Use --list-grids to inspect available BOMapGrid objects.");
    }
    if (!options.listGrids && !options.listFaultPolygons && options.exportFaultPolygonsOutputPath.empty() && !options.importGeoEastDatPath.empty() && options.outputName.empty()) {
        throw std::runtime_error("Missing --output for imported GeoEast DAT.");
    }
    if (!options.listGrids && !options.listFaultPolygons && options.exportFaultPolygonsOutputPath.empty() && !options.deleteOutput && options.importGeoEastDatPath.empty() && !options.validateGridOnly && options.outputName.empty()) {
        throw std::runtime_error("Missing --output. Use --list-grids to inspect available BOMapGrid objects.");
    }
    if (options.deleteOutput && options.outputName.empty()) {
        throw std::runtime_error("Missing --output for --delete-output.");
    }
    if (!options.exportFaultPolygonsOutputPath.empty() && options.outputName.empty()) {
        throw std::runtime_error("Missing --output for --export-fault-polygons-output.");
    }
    return options;
}

void printGridStats(const fpt::Grid2D& grid) {
    double minValue = std::numeric_limits<double>::max();
    double maxValue = -std::numeric_limits<double>::max();
    double sum = 0.0;
    std::size_t finiteCount = 0;
    std::size_t nonZeroCount = 0;

    for (double value : grid.values()) {
        if (!std::isfinite(value)) {
            continue;
        }
        minValue = std::min(minValue, value);
        maxValue = std::max(maxValue, value);
        sum += value;
        ++finiteCount;
        if (value != 0.0) {
            ++nonZeroCount;
        }
    }

    std::cout << "Grid stats:\n";
    std::cout << "  size: " << grid.nx() << " x " << grid.ny() << "\n";
    std::cout << "  origin: sx=" << grid.sx() << " sy=" << grid.sy() << "\n";
    std::cout << "  spacing: dx=" << grid.dx() << " dy=" << grid.dy() << "\n";
    std::cout << "  finite cells: " << finiteCount << "\n";
    std::cout << "  nonzero cells: " << nonZeroCount << "\n";
    if (finiteCount > 0) {
        std::cout << "  min: " << minValue << "\n";
        std::cout << "  max: " << maxValue << "\n";
        std::cout << "  mean: " << (sum / static_cast<double>(finiteCount)) << "\n";
    }
}

void writePreviewGrid(const std::string& path, const fpt::Grid2D& grid) {
    if (path.empty()) {
        return;
    }

    fpt::AtomicOutputFile output(std::filesystem::u8path(path), 1024 * 1024);
    std::ostream& out = output.stream();

    out << grid.nx() << " " << grid.ny() << "\n";
    out << std::fixed << std::setprecision(6);
    out << grid.sx() << " " << grid.sy() << "\n";
    out << grid.dx() << " " << grid.dy() << "\n";
    for (int y = 0; y < grid.ny(); ++y) {
        for (int x = 0; x < grid.nx(); ++x) {
            if (x > 0) {
                out << " ";
            }
            out << grid.value(x, y);
        }
        out << "\n";
    }
    output.commit();
}

void verifyFaultPolygonRoundTrip(
    const fpt_geoeast::GeoEastProjectPtr& project,
    const std::string& outputName,
    const std::vector<fpt::Polygon>& source) {
    std::vector<const fpt::Polygon*> expected;
    for (const fpt::Polygon& polygon : source) {
        if (polygon.kept && polygon.points.size() >= 3) {
            expected.push_back(&polygon);
        }
    }
    const std::vector<fpt::Polygon> actual =
        fpt_geoeast::readFaultPolygons(project, outputName);
    if (actual.size() != expected.size()) {
        throw std::runtime_error(
            "GeoEast round-trip polygon count mismatch: expected " +
            std::to_string(expected.size()) + ", got " +
            std::to_string(actual.size()));
    }

    auto bounds = [](const std::vector<fpt::Point>& points) {
        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = -std::numeric_limits<double>::max();
        double maxY = -std::numeric_limits<double>::max();
        for (const fpt::Point& point : points) {
            minX = std::min(minX, point.x);
            minY = std::min(minY, point.y);
            maxX = std::max(maxX, point.x);
            maxY = std::max(maxY, point.y);
        }
        return std::array<double, 4>{minX, minY, maxX, maxY};
    };
    std::vector<std::array<double, 4>> expectedBounds;
    std::vector<std::array<double, 4>> actualBounds;
    expectedBounds.reserve(expected.size());
    actualBounds.reserve(actual.size());
    for (const fpt::Polygon* polygon : expected) {
        expectedBounds.push_back(bounds(polygon->points));
    }
    for (const fpt::Polygon& polygon : actual) {
        actualBounds.push_back(bounds(polygon.points));
    }
    std::sort(expectedBounds.begin(), expectedBounds.end());
    std::sort(actualBounds.begin(), actualBounds.end());

    constexpr double tolerance = 1e-7;
    for (std::size_t index = 0; index < expectedBounds.size(); ++index) {
        for (std::size_t axis = 0; axis < expectedBounds[index].size(); ++axis) {
            const double scale = std::max(1.0, std::abs(expectedBounds[index][axis]));
            if (std::abs(expectedBounds[index][axis] - actualBounds[index][axis]) > tolerance * scale) {
                throw std::runtime_error(
                    "GeoEast round-trip bounds mismatch at polygon " +
                    std::to_string(index + 1));
            }
        }
    }
    std::cout << "GeoEast round-trip verified: " << actual.size()
              << " polygons, bounds preserved.\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseArgs(argc, argv);
        const auto start = std::chrono::steady_clock::now();

        validateGeoEastEnvironment();

        debugLog("Connecting GeoEast database: " + options.databaseName);
        std::shared_ptr<geobo::BOSystemRoot> root = geobo::BOSystemRoot::instance(options.databaseName);
        debugLog("BOSystemRoot::instance returned.");
        if (root->Fail()) {
            throw std::runtime_error("Failed to connect database: " + root->Message());
        }
        debugLog("Setting GeoEast auth user: " + options.userName);
        root->setAuth(options.userName, options.password);

        debugLog("Getting GeoEast project: " + options.projectName);
        fpt_geoeast::GeoEastProjectPtr project = root->getChild<geobo::BOProject>(options.projectName);
        debugLog("BOProject getChild returned.");
        if (!project) {
            throw std::runtime_error("Failed to get project: " + options.projectName + " " + root->Message());
        }

        if (options.listGrids) {
            debugLog("Listing BOMapGrid objects...");
            const std::vector<std::string> gridNames = fpt_geoeast::listMapGrids(project);
            debugLog("BOMapGrid list completed.");
            std::cout << "BOMapGrid objects in project " << options.projectName << ":\n";
            for (const std::string& gridName : gridNames) {
                std::cout << "  " << gridName << "\n";
            }
            std::cout << "Total: " << gridNames.size() << "\n";
            return 0;
        }

        if (options.listFaultPolygons) {
            debugLog("Listing BOFaultPolygon objects...");
            const std::vector<std::string> polygonNames = fpt_geoeast::listFaultPolygons(project);
            debugLog("BOFaultPolygon list completed.");
            std::cout << "BOFaultPolygon objects in project " << options.projectName << ":\n";
            for (const std::string& polygonName : polygonNames) {
                std::cout << "  " << polygonName << "\n";
            }
            std::cout << "Total: " << polygonNames.size() << "\n";
            return 0;
        }

        if (options.deleteOutput) {
            debugLog("Deleting BOFaultPolygon: " + options.outputName);
            fpt_geoeast::deleteFaultPolygons(project, options.outputName);
            debugLog("BOFaultPolygon delete completed.");
            std::cout << "Deleted BOFaultPolygon: " << options.outputName << "\n";
            return 0;
        }

        if (!options.exportFaultPolygonsOutputPath.empty()) {
            debugLog("Exporting BOFaultPolygon: " + options.outputName);
            if (!options.previewGridOutputPath.empty() && !options.gridName.empty()) {
                debugLog("Exporting BOMapGrid view for result review: " + options.gridName);
                const fpt::Grid2D grid = fpt_geoeast::readMapGrid(project, options.gridName);
                writePreviewGrid(options.previewGridOutputPath, grid);
                std::cout << "Exported BOMapGrid view: " << options.gridName << "\n";
                std::cout << "View grid: " << options.previewGridOutputPath << "\n";
            }
            const std::vector<fpt::Polygon> polygons = fpt_geoeast::readFaultPolygons(project, options.outputName);
            fpt::writePolygonCsv(options.exportFaultPolygonsOutputPath, polygons);
            debugLog("BOFaultPolygon export completed.");
            std::cout << "Exported BOFaultPolygon: " << options.outputName << "\n";
            std::cout << "Output CSV: " << options.exportFaultPolygonsOutputPath << "\n";
            std::cout << "Polygons: " << polygons.size() << "\n";
            return 0;
        }

        fpt::Grid2D grid;
        if (!options.importGeoEastDatPath.empty()) {
            debugLog("Reading official GeoEast DAT grid: " + options.importGeoEastDatPath);
            grid = fpt::readGeoEastDatGrid(options.importGeoEastDatPath);
            debugLog("Official GeoEast DAT grid parsed.");
            printGridStats(grid);
            debugLog("Writing parsed grid to GeoEast BOMapGrid: " + options.gridName);
            fpt_geoeast::writeMapGrid(project, options.projectName, options.gridName, grid, options.overwriteGrid);
            debugLog("Imported BOMapGrid write completed.");
        } else {
            debugLog("Reading BOMapGrid: " + options.gridName);
            grid = fpt_geoeast::readMapGrid(project, options.gridName);
            debugLog("BOMapGrid read completed.");
        }
        if (options.validateGridOnly) {
            printGridStats(grid);
            writePreviewGrid(options.previewGridOutputPath, grid);
            if (!options.previewGridOutputPath.empty()) {
                std::cout << "View grid: " << options.previewGridOutputPath << "\n";
            }
            return 0;
        }
        writePreviewGrid(options.previewGridOutputPath, grid);
        debugLog("Running FaultPolyTrace core.");
        fpt::TraceResult result = fpt::traceFaultPolygons(grid, options.trace);
        fpt::TopologyDiagnostics topology = fpt::diagnoseTopology(result.polygons, true);
        if (!options.topologyOutputPath.empty()) {
            fpt::writeTopologyDetailsCsv(options.topologyOutputPath, topology);
        }
        if (!options.previewPolygonsOutputPath.empty()) {
            fpt::writePolygonCsv(options.previewPolygonsOutputPath, result.polygons);
        }
        debugLog("Writing BOFaultPolygon: " + options.outputName);
        fpt_geoeast::writeFaultPolygons(project, options.outputName, result.polygons, true, options.polygonStyle);
        verifyFaultPolygonRoundTrip(project, options.outputName, result.polygons);
        debugLog("BOFaultPolygon write completed.");

        int kept = 0;
        for (const fpt::Polygon& polygon : result.polygons) {
            if (polygon.kept) {
                ++kept;
            }
        }

        const auto end = std::chrono::steady_clock::now();
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::cout << "FaultPolyTrace GeoEast runner completed.\n";
        std::cout << "Grid: " << grid.nx() << " x " << grid.ny() << "\n";
        std::cout << "Effective threshold: " << result.effectiveThreshold << "\n";
        std::cout << "Polygons: " << result.polygons.size() << " total, " << kept << " kept\n";
        if (options.trace.rootAntiMerge) {
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
        if (options.trace.localAntiMerge) {
            std::cout << "Local anti-merge candidate cut cells: " << result.localAntiMergeCandidateCutCells << "\n";
            std::cout << "Local anti-merge accepted cut cells: " << result.localAntiMergeCutCells << "\n";
            std::cout << "Local anti-merge rejected cut cells: " << result.localAntiMergeRejectedCutCells << "\n";
            std::cout << "Local anti-merge accepted/rejected components: "
                      << result.localAntiMergeAcceptedComponents << "/"
                      << result.localAntiMergeRejectedComponents << "\n";
        }
        std::cout << "Suppressed internal rings: " << result.suppressedInternalRings << "\n";
        std::cout << "Coverage preserve simplify fallback polygons: "
                  << result.coveragePreserveSimplifyFallbackPolygons << "\n";
        std::cout << "Coverage preserve smooth fallback polygons: "
                  << result.coveragePreserveSmoothFallbackPolygons << "\n";
        std::cout << "Coverage preserve topology override polygons: "
                  << result.coveragePreserveTopologyOverridePolygons << "\n";
        std::cout << "Topology open rings: " << topology.openRingCount << "\n";
        std::cout << "Topology self-intersections: " << topology.selfIntersectingPolygonCount << "\n";
        std::cout << "Topology polygon intersections: " << topology.polygonIntersectionCount << "\n";
        if (!options.topologyOutputPath.empty()) {
            std::cout << "Topology details: " << options.topologyOutputPath << "\n";
        }
        if (!options.previewGridOutputPath.empty()) {
            std::cout << "View grid: " << options.previewGridOutputPath << "\n";
        }
        if (!options.previewPolygonsOutputPath.empty()) {
            std::cout << "View polygons: " << options.previewPolygonsOutputPath << "\n";
        }
        std::cout << "Output BOFaultPolygon: " << options.outputName << "\n";
        std::cout << "Elapsed: " << elapsedMs << " ms\n";
        return 0;
    } catch (const std::bad_alloc& ex) {
        debugLog(std::string("Error: bad allocation. Last GeoEast step above is the failing area. Detail: ") + ex.what());
        std::cerr << "\n";
        printUsage();
        return 1;
    } catch (const std::exception& ex) {
        debugLog(std::string("Error: ") + ex.what());
        std::cerr << "\n";
        printUsage();
        return 1;
    } catch (...) {
        debugLog("Error: unexpected internal failure was contained; no GeoEast success result was reported.");
        return 2;
    }
}
