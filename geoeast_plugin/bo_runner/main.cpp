#include "GeoEastBoAdapter.h"
#include "StagedCommit.h"

#include "../../src/core/FaultTracer.h"
#include "../../src/core/ProductDefaults.h"
#include "../../src/core/Topology.h"
#include "../../src/io/AsciiGridIO.h"
#include "../../src/io/AtomicOutputFile.h"
#include "../../src/io/DiagnosticsIO.h"

#include <chrono>
#include <array>
#include <cctype>
#include <cstdint>
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
#include <sstream>
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
    std::string diagnosticsOutputPath;
    std::string transactionReportPath;
    std::string topologyOutputPath;
    std::string weakContinuityOutputPath;
    std::string previewGridOutputPath;
    std::string previewPolygonsOutputPath;
    std::string exportFaultPolygonsOutputPath;
    std::string importGeoEastDatPath;
    std::string importFaultPolygonsCsvPath;
    bool listGrids = false;
    bool listFaultPolygons = false;
    bool inspectFaultPolygon = false;
    bool validateGridOnly = false;
    bool overwriteGrid = false;
    bool deleteGrid = false;
    bool deleteOutput = false;
    int expectedImportedPolygonCount = -1;
    fpt_geoeast::FaultPolygonStyle polygonStyle;
    fpt::TraceOptions trace;
};

bool parseTailContinuationOption(
    const std::string& arg,
    int& index,
    int argc,
    char** argv,
    Options& options) {
    auto requireValue = [&](const std::string& name) -> std::string {
        if (index + 1 >= argc) {
            throw std::runtime_error("Missing value for " + name);
        }
        return argv[++index];
    };
    if (arg == "--weak-tail-completion") {
        options.trace.weakTailCompletion = true;
    } else if (arg == "--no-weak-tail-completion") {
        options.trace.weakTailCompletion = false;
    } else if (arg == "--weak-tail-topology-gate") {
        options.trace.weakTailTopologyGate = true;
    } else if (arg == "--no-weak-tail-topology-gate") {
        options.trace.weakTailTopologyGate = false;
    } else if (arg == "--simple-ring-compatibility") {
        options.trace.simpleRingCompatibility = true;
    } else if (arg == "--no-simple-ring-compatibility") {
        options.trace.simpleRingCompatibility = false;
    } else if (arg == "--simple-ring-max-fill-cells") {
        options.trace.simpleRingMaximumFillCells =
            std::stoi(requireValue(arg));
    } else if (arg == "--simple-ring-max-opening-cells") {
        options.trace.simpleRingMaximumOpeningCells =
            std::stoi(requireValue(arg));
    } else if (arg == "--weak-tail-completion-shadow") {
        options.trace.weakTailCompletionShadow = true;
    } else if (arg == "--no-weak-tail-completion-shadow") {
        options.trace.weakTailCompletionShadow = false;
    } else if (arg == "--weak-tail-endpoint-port-shadow") {
        options.trace.weakTailEndpointPortShadow = true;
    } else if (arg == "--no-weak-tail-endpoint-port-shadow") {
        options.trace.weakTailEndpointPortShadow = false;
    } else if (arg == "--weak-tail-endpoint-port-ownership") {
        options.trace.weakTailEndpointPortOwnership = true;
    } else if (arg == "--no-weak-tail-endpoint-port-ownership") {
        options.trace.weakTailEndpointPortOwnership = false;
    } else if (arg == "--weak-tail-min-cells") {
        options.trace.weakTailMinCells = std::stoi(requireValue(arg));
    } else if (arg == "--weak-tail-max-cells") {
        options.trace.weakTailMaxCells = std::stoi(requireValue(arg));
    } else if (arg == "--weak-tail-min-aspect") {
        options.trace.weakTailMinAspect = std::stod(requireValue(arg));
    } else if (arg == "--weak-tail-min-mean-support-ratio") {
        options.trace.weakTailMinMeanSupportRatio = std::stod(requireValue(arg));
    } else if (arg == "--weak-tail-min-peak-support-ratio") {
        options.trace.weakTailMinPeakSupportRatio = std::stod(requireValue(arg));
    } else if (arg == "--weak-tail-max-alignment-angle") {
        options.trace.weakTailMaxAlignmentAngle = std::stod(requireValue(arg));
    } else if (arg == "--weak-tail-owner-clearance") {
        options.trace.weakTailOwnershipClearanceRadius = std::stoi(requireValue(arg));
    } else if (arg == "--weak-tail-tangent-radius") {
        options.trace.weakTailTangentRadius = std::stoi(requireValue(arg));
    } else if (arg == "--weak-tail-junction-clearance") {
        options.trace.weakTailJunctionClearanceRadius = std::stoi(requireValue(arg));
    } else if (arg == "--preserve-observed-skeleton-envelope") {
        options.trace.preserveObservedSkeletonEnvelope = true;
    } else if (arg == "--no-preserve-observed-skeleton-envelope") {
        options.trace.preserveObservedSkeletonEnvelope = false;
    } else {
        return false;
    }
    return true;
}

bool parseImportOption(
    const std::string& arg,
    int& index,
    int argc,
    char** argv,
    Options& options) {
    auto requireValue = [&](const std::string& name) -> std::string {
        if (index + 1 >= argc) {
            throw std::runtime_error("Missing value for " + name);
        }
        return argv[++index];
    };
    if (arg == "--import-geoeast-dat") {
        options.importGeoEastDatPath = requireValue(arg);
    } else if (arg == "--import-fault-polygons-csv") {
        options.importFaultPolygonsCsvPath = requireValue(arg);
    } else if (arg == "--expected-polygon-count") {
        options.expectedImportedPolygonCount = std::stoi(requireValue(arg));
    } else {
        return false;
    }
    return true;
}

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
        << "  --delete-grid\n"
        << "  --delete-output\n"
        << "  --import-geoeast-dat <test.dat>\n"
        << "  --import-fault-polygons-csv <polygons.csv>\n"
        << "  --expected-polygon-count <count>\n"
        << "  --overwrite-grid\n"
        << "  --threshold-mode fixed|percentile|positive-percentile\n"
        << "  --threshold <value>\n"
        << "  --percentile <0-1>\n"
        << "  --min-area <value>\n"
        << "  --min-cells <count>\n"
        << "  --open-radius <count>\n"
        << "  --close-radius <count>\n"
        << "  --physical-morphology-shadow | --no-physical-morphology-shadow\n"
        << "  --physical-close-radius-world <value>\n"
        << "  --weak-percentile <0-1>\n"
        << "  --weak-connect-radius <count>\n"
        << "  --max-weak-component-cells <count>\n"
        << "  --min-weak-component-aspect <value>\n"
        << "  --weak-path-gate | --no-weak-path-gate\n"
        << "  --weak-extension-layered-shadow | --no-weak-extension-layered-shadow\n"
        << "  --weak-extension-layered-gate | --no-weak-extension-layered-gate\n"
        << "  --branch-attribution-shadow | --no-branch-attribution-shadow\n"
        << "  --skeleton-graph-v2-shadow | --no-skeleton-graph-v2-shadow\n"
        << "  --skeleton-graph-v2-attribution-shadow | --no-skeleton-graph-v2-attribution-shadow\n"
        << "  --multi-scale-evidence-shadow | --no-multi-scale-evidence-shadow\n"
        << "  --adaptive-boundary-shadow | --no-adaptive-boundary-shadow\n"
        << "  --adaptive-boundary-apply | --no-adaptive-boundary-apply\n"
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
        << "  --weak-continuity\n"
        << "  --no-weak-continuity\n"
        << "  --weak-continuity-shadow\n"
        << "  --no-weak-continuity-shadow\n"
        << "  --weak-continuity-second-pass\n"
        << "  --no-weak-continuity-second-pass\n"
        << "  --weak-continuity-final-object-pass\n"
        << "  --no-weak-continuity-final-object-pass\n"
        << "  --weak-continuity-max-distance <count>\n"
        << "  --weak-continuity-tangent-window <count>\n"
        << "  --weak-continuity-corridor-radius <count>\n"
        << "  --weak-continuity-max-candidates <count>\n"
        << "  --weak-continuity-max-angle <degrees>\n"
        << "  --weak-continuity-max-width-ratio <value>\n"
        << "  --weak-continuity-min-attribute-ratio <value>\n"
        << "  --weak-continuity-reference-threshold-ratio <value>\n"
        << "  --weak-continuity-local-contrast-ratio <value>\n"
        << "  --weak-continuity-min-support-ratio <value>\n"
        << "  --weak-continuity-min-mean-support <value>\n"
        << "  --weak-continuity-max-unsupported-run <count>\n"
        << "  --weak-continuity-max-length-ratio <value>\n"
        << "  --weak-continuity-max-turn-angle <degrees>\n"
        << "  --weak-continuity-min-competition-margin <value>\n"
        << "  --weak-continuity-third-party-clearance <count>\n"
        << "  --weak-continuity-physical-metric | --no-weak-continuity-physical-metric\n"
        << "  --weak-continuity-global-matching | --no-weak-continuity-global-matching\n"
        << "  --weak-continuity-no-connect-cost <value>\n"
        << "  --weak-continuity-max-exact-endpoints <count>\n"
        << "  --weak-continuity-output <csv>\n"
        << "  --weak-tail-completion | --no-weak-tail-completion\n"
        << "  --weak-tail-topology-gate | --no-weak-tail-topology-gate\n"
        << "  --simple-ring-compatibility | --no-simple-ring-compatibility\n"
        << "  --simple-ring-max-fill-cells <count>\n"
        << "  --simple-ring-max-opening-cells <count>\n"
        << "  --weak-tail-completion-shadow | --no-weak-tail-completion-shadow\n"
        << "  --weak-tail-endpoint-port-shadow | --no-weak-tail-endpoint-port-shadow\n"
        << "  --weak-tail-endpoint-port-ownership | --no-weak-tail-endpoint-port-ownership\n"
        << "  --weak-tail-min-cells <count>\n"
        << "  --weak-tail-max-cells <count>\n"
        << "  --weak-tail-min-aspect <value>\n"
        << "  --weak-tail-min-mean-support-ratio <value>\n"
        << "  --weak-tail-min-peak-support-ratio <value>\n"
        << "  --weak-tail-max-alignment-angle <degrees>\n"
        << "  --weak-tail-owner-clearance <count>\n"
        << "  --weak-tail-tangent-radius <count>\n"
        << "  --weak-tail-junction-clearance <count>\n"
        << "  --preserve-observed-skeleton-envelope\n"
        << "  --no-preserve-observed-skeleton-envelope\n"
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
        << "  --diagnostics <json>\n"
        << "  --transaction-report <json>\n"
        << "  --inspect-fault-polygon (requires --output)\n"
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

bool parsePostProcessingOption(
    const std::string& arg,
    int& index,
    int argc,
    char** argv,
    Options& options) {
    auto requireValue = [&](const std::string& name) -> std::string {
        if (index + 1 >= argc) {
            throw std::runtime_error("Missing value for " + name);
        }
        return argv[++index];
    };
    if (arg == "--separate-branch-belts") {
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
        options.polygonStyle = parsePolygonColor(
            requireValue(arg), options.polygonStyle);
    } else if (arg == "--polygon-linewidth") {
        options.polygonStyle.lineWidth = std::stod(requireValue(arg));
    } else {
        return false;
    }
    return true;
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
    fpt::product_defaults::applyVisibleProductionDefaults(options.trace);
    options.trace.traceMode = fpt::TraceMode::RidgeBelt;
    options.trace.thresholdMode = fpt::ThresholdMode::Percentile;
    options.trace.weakContinuityFinalObjectPass = true;
    options.trace.weakTailCompletion = true;
    options.trace.preserveObservedSkeletonEnvelope = true;
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
    options.trace.maxWeakComponentCells = 20000;
    options.trace.minWeakComponentAspect = 3.0;
    options.trace.weakContinuity = false;
    options.trace.weakContinuitySecondPass = false;
    options.trace.weakContinuityFinalObjectPass = true;
    options.trace.weakTailCompletion = true;
    options.trace.preserveObservedSkeletonEnvelope = true;
    options.trace.residualAttributeFill = true;

    if (argc == 1) {
        throw std::runtime_error("No command-line arguments supplied. Use --help to view GeoEast runner usage.");
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--delete-grid") {
            options.deleteGrid = true;
            continue;
        }
        if (parseImportOption(arg, i, argc, argv, options)) {
            continue;
        }
        if (parseTailContinuationOption(arg, i, argc, argv, options)) {
            continue;
        }
        if (parsePostProcessingOption(arg, i, argc, argv, options)) {
            continue;
        }
        if (arg == "--inspect-fault-polygon") {
            options.inspectFaultPolygon = true;
            continue;
        }
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
        } else if (arg == "--diagnostics") {
            options.diagnosticsOutputPath = requireValue(arg);
        } else if (arg == "--transaction-report") {
            options.transactionReportPath = requireValue(arg);
        } else if (arg == "--preview-grid-output" || arg == "--view-grid-output") {
            options.previewGridOutputPath = requireValue(arg);
        } else if (arg == "--preview-polygons-output" || arg == "--view-polygons-output") {
            options.previewPolygonsOutputPath = requireValue(arg);
        } else if (arg == "--export-fault-polygons-output") {
            options.exportFaultPolygonsOutputPath = requireValue(arg);
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
        } else if (arg == "--physical-morphology-shadow") {
            options.trace.physicalMorphologyShadow = true;
        } else if (arg == "--no-physical-morphology-shadow") {
            options.trace.physicalMorphologyShadow = false;
        } else if (arg == "--physical-close-radius-world") {
            options.trace.physicalCloseRadiusWorld = std::stod(requireValue(arg));
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
        } else if (arg == "--weak-extension-layered-shadow") {
            options.trace.weakExtensionLayeredShadow = true;
        } else if (arg == "--no-weak-extension-layered-shadow") {
            options.trace.weakExtensionLayeredShadow = false;
        } else if (arg == "--weak-extension-layered-gate") {
            options.trace.weakExtensionLayeredGate = true;
        } else if (arg == "--no-weak-extension-layered-gate") {
            options.trace.weakExtensionLayeredGate = false;
        } else if (arg == "--branch-attribution-shadow") {
            options.trace.branchAttributionShadow = true;
        } else if (arg == "--no-branch-attribution-shadow") {
            options.trace.branchAttributionShadow = false;
        } else if (arg == "--skeleton-graph-v2-shadow") {
            options.trace.skeletonGraphV2Shadow = true;
        } else if (arg == "--no-skeleton-graph-v2-shadow") {
            options.trace.skeletonGraphV2Shadow = false;
        } else if (arg == "--skeleton-graph-v2-attribution-shadow") {
            options.trace.skeletonGraphV2AttributionShadow = true;
        } else if (arg == "--no-skeleton-graph-v2-attribution-shadow") {
            options.trace.skeletonGraphV2AttributionShadow = false;
        } else if (arg == "--multi-scale-evidence-shadow") {
            options.trace.multiScaleEvidenceShadow = true;
        } else if (arg == "--no-multi-scale-evidence-shadow") {
            options.trace.multiScaleEvidenceShadow = false;
        } else if (arg == "--multi-scale-min-confidence") {
            options.trace.multiScaleEvidenceMinimumConfidence =
                std::stod(requireValue(arg));
        } else if (arg == "--multi-scale-min-ridge-response") {
            options.trace.multiScaleEvidenceMinimumRidgeResponse =
                std::stod(requireValue(arg));
        } else if (arg == "--multi-scale-max-samples") {
            options.trace.multiScaleEvidenceMaximumSamples =
                static_cast<std::size_t>(std::stoull(requireValue(arg)));
        } else if (arg == "--adaptive-boundary-shadow") {
            options.trace.adaptiveBoundaryShadow = true;
        } else if (arg == "--no-adaptive-boundary-shadow") {
            options.trace.adaptiveBoundaryShadow = false;
        } else if (arg == "--adaptive-boundary-apply") {
            options.trace.adaptiveBoundaryApply = true;
        } else if (arg == "--no-adaptive-boundary-apply") {
            options.trace.adaptiveBoundaryApply = false;
        } else if (arg == "--adaptive-boundary-max-displacement") {
            options.trace.adaptiveBoundaryMaximumDisplacementWorld =
                std::stod(requireValue(arg));
        } else if (arg == "--adaptive-boundary-min-support-gain") {
            options.trace.adaptiveBoundaryMinimumSupportGain =
                std::stod(requireValue(arg));
        } else if (arg == "--adaptive-boundary-max-support-loss") {
            options.trace.adaptiveBoundaryMaximumSupportLossRatio =
                std::stod(requireValue(arg));
        } else if (arg == "--adaptive-boundary-max-leakage-increase") {
            options.trace.adaptiveBoundaryMaximumLeakageIncreaseRatio =
                std::stod(requireValue(arg));
        } else if (arg == "--adaptive-boundary-max-polygons") {
            options.trace.adaptiveBoundaryMaximumPolygons =
                static_cast<std::size_t>(std::stoull(requireValue(arg)));
        } else if (arg == "--adaptive-boundary-max-vertices") {
            options.trace.adaptiveBoundaryMaximumVerticesPerPolygon =
                static_cast<std::size_t>(std::stoull(requireValue(arg)));
        } else if (arg == "--local-polygon-quality-shadow") {
            options.trace.localPolygonQualityShadow = true;
        } else if (arg == "--no-local-polygon-quality-shadow") {
            options.trace.localPolygonQualityShadow = false;
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
        } else if (arg == "--weak-continuity") {
            options.trace.weakContinuity = true;
        } else if (arg == "--no-weak-continuity") {
            options.trace.weakContinuity = false;
        } else if (arg == "--weak-continuity-second-pass") {
            options.trace.weakContinuitySecondPass = true;
        } else if (arg == "--no-weak-continuity-second-pass") {
            options.trace.weakContinuitySecondPass = false;
        } else if (arg == "--weak-continuity-final-object-pass") {
            options.trace.weakContinuityFinalObjectPass = true;
        } else if (arg == "--no-weak-continuity-final-object-pass") {
            options.trace.weakContinuityFinalObjectPass = false;
        } else if (arg == "--weak-continuity-shadow") {
            options.trace.weakContinuityShadow = true;
        } else if (arg == "--no-weak-continuity-shadow") {
            options.trace.weakContinuityShadow = false;
        } else if (arg == "--weak-continuity-max-distance") {
            options.trace.weakContinuityMaxDistance = std::stoi(requireValue(arg));
        } else if (arg == "--weak-continuity-tangent-window") {
            options.trace.weakContinuityTangentWindow = std::stoi(requireValue(arg));
        } else if (arg == "--weak-continuity-corridor-radius") {
            options.trace.weakContinuityCorridorRadius = std::stoi(requireValue(arg));
        } else if (arg == "--weak-continuity-max-candidates") {
            options.trace.weakContinuityMaxCandidatesPerEndpoint = std::stoi(requireValue(arg));
        } else if (arg == "--weak-continuity-max-angle") {
            options.trace.weakContinuityMaxEndpointAngle = std::stod(requireValue(arg));
        } else if (arg == "--weak-continuity-max-width-ratio") {
            options.trace.weakContinuityMaxWidthRatio = std::stod(requireValue(arg));
        } else if (arg == "--weak-continuity-min-attribute-ratio") {
            options.trace.weakContinuityMinAttributeRatio = std::stod(requireValue(arg));
        } else if (arg == "--weak-continuity-reference-threshold-ratio") {
            options.trace.weakContinuityReferenceThresholdRatio = std::stod(requireValue(arg));
        } else if (arg == "--weak-continuity-local-contrast-ratio") {
            options.trace.weakContinuityLocalContrastRatio = std::stod(requireValue(arg));
        } else if (arg == "--weak-continuity-min-support-ratio") {
            options.trace.weakContinuityMinSupportRatio = std::stod(requireValue(arg));
        } else if (arg == "--weak-continuity-min-mean-support") {
            options.trace.weakContinuityMinMeanSupport = std::stod(requireValue(arg));
        } else if (arg == "--weak-continuity-max-unsupported-run") {
            options.trace.weakContinuityMaxUnsupportedRun = std::stoi(requireValue(arg));
        } else if (arg == "--weak-continuity-max-length-ratio") {
            options.trace.weakContinuityMaxPathLengthRatio = std::stod(requireValue(arg));
        } else if (arg == "--weak-continuity-max-turn-angle") {
            options.trace.weakContinuityMaxTurnAngle = std::stod(requireValue(arg));
        } else if (arg == "--weak-continuity-min-competition-margin") {
            options.trace.weakContinuityMinCompetitionMargin = std::stod(requireValue(arg));
        } else if (arg == "--weak-continuity-physical-metric") {
            options.trace.weakContinuityUsePhysicalMetric = true;
        } else if (arg == "--no-weak-continuity-physical-metric") {
            options.trace.weakContinuityUsePhysicalMetric = false;
        } else if (arg == "--weak-continuity-global-matching") {
            options.trace.weakContinuityUseGlobalMatching = true;
        } else if (arg == "--no-weak-continuity-global-matching") {
            options.trace.weakContinuityUseGlobalMatching = false;
        } else if (arg == "--weak-continuity-no-connect-cost") {
            options.trace.weakContinuityNoConnectCost = std::stod(requireValue(arg));
        } else if (arg == "--weak-continuity-max-exact-endpoints") {
            options.trace.weakContinuityMaxExactMatchingEndpoints =
                std::stoi(requireValue(arg));
        } else if (arg == "--weak-continuity-third-party-clearance") {
            options.trace.weakContinuityThirdPartySkeletonClearanceRadius = std::stoi(requireValue(arg));
        } else if (arg == "--weak-continuity-output") {
            options.weakContinuityOutputPath = requireValue(arg);
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
    if (!options.listGrids && !options.listFaultPolygons &&
        !options.inspectFaultPolygon &&
        options.exportFaultPolygonsOutputPath.empty() &&
        options.importFaultPolygonsCsvPath.empty() &&
        !options.deleteOutput && options.gridName.empty()) {
        throw std::runtime_error("Missing --grid. Use --list-grids to inspect available BOMapGrid objects.");
    }
    if (!options.listGrids && !options.listFaultPolygons &&
        !options.inspectFaultPolygon &&
        options.exportFaultPolygonsOutputPath.empty() &&
        !options.importGeoEastDatPath.empty() && options.outputName.empty()) {
        throw std::runtime_error("Missing --output for imported GeoEast DAT.");
    }
    if (!options.listGrids && !options.listFaultPolygons &&
        !options.inspectFaultPolygon &&
        options.exportFaultPolygonsOutputPath.empty() && !options.deleteGrid &&
        !options.deleteOutput && options.importGeoEastDatPath.empty() &&
        !options.validateGridOnly && options.outputName.empty()) {
        throw std::runtime_error("Missing --output. Use --list-grids to inspect available BOMapGrid objects.");
    }
    if (options.deleteGrid && options.gridName.empty()) {
        throw std::runtime_error("Missing --grid for --delete-grid.");
    }
    if (options.deleteOutput && options.outputName.empty()) {
        throw std::runtime_error("Missing --output for --delete-output.");
    }
    if (!options.exportFaultPolygonsOutputPath.empty() && options.outputName.empty()) {
        throw std::runtime_error("Missing --output for --export-fault-polygons-output.");
    }
    if (options.inspectFaultPolygon && options.outputName.empty()) {
        throw std::runtime_error("Missing --output for --inspect-fault-polygon.");
    }
    if (!options.importFaultPolygonsCsvPath.empty() && options.outputName.empty()) {
        throw std::runtime_error("Missing --output for --import-fault-polygons-csv.");
    }
    if (!options.importFaultPolygonsCsvPath.empty() &&
        !options.importGeoEastDatPath.empty()) {
        throw std::runtime_error(
            "--import-fault-polygons-csv cannot be combined with --import-geoeast-dat.");
    }
    if (options.expectedImportedPolygonCount == 0 ||
        options.expectedImportedPolygonCount < -1) {
        throw std::runtime_error("--expected-polygon-count must be a positive integer.");
    }
    if (options.expectedImportedPolygonCount > 0 &&
        options.importFaultPolygonsCsvPath.empty()) {
        throw std::runtime_error(
            "--expected-polygon-count requires --import-fault-polygons-csv.");
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

using QuantizedPoint = std::array<std::int64_t, 2>;
using CanonicalRing = std::vector<QuantizedPoint>;

struct FaultPolygonRoundTripReport {
    bool completed = false;
    bool geometryVerified = false;
    bool topologyVerified = false;
    bool styleVerified = false;
    bool storageVerified = false;
    std::size_t expectedPolygonCount = 0;
    std::size_t actualPolygonCount = 0;
    std::size_t expectedVertexCount = 0;
    std::size_t actualVertexCount = 0;
    std::size_t sourceInteriorRingCount = 0;
    std::string expectedGeometryHash;
    std::string actualGeometryHash;
    fpt_geoeast::FaultPolygonStorageSummary storage;
};

CanonicalRing rotateRing(const CanonicalRing& ring, std::size_t start) {
    CanonicalRing rotated;
    rotated.reserve(ring.size());
    for (std::size_t offset = 0; offset < ring.size(); ++offset) {
        rotated.push_back(ring[(start + offset) % ring.size()]);
    }
    return rotated;
}

CanonicalRing minimumRingRotation(const CanonicalRing& ring) {
    if (ring.empty()) {
        return {};
    }
    const QuantizedPoint minimumPoint =
        *std::min_element(ring.begin(), ring.end());
    CanonicalRing best;
    bool initialized = false;
    for (std::size_t index = 0; index < ring.size(); ++index) {
        if (ring[index] != minimumPoint) {
            continue;
        }
        CanonicalRing candidate = rotateRing(ring, index);
        if (!initialized || candidate < best) {
            best = std::move(candidate);
            initialized = true;
        }
    }
    return best;
}

CanonicalRing canonicalizeRing(const std::vector<fpt::Point>& points) {
    constexpr long double coordinateScale = 1000000.0L;
    CanonicalRing ring;
    ring.reserve(points.size());
    for (const fpt::Point& point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            throw std::runtime_error(
                "GeoEast round-trip source contains a non-finite coordinate.");
        }
        const long double scaledX = static_cast<long double>(point.x) * coordinateScale;
        const long double scaledY = static_cast<long double>(point.y) * coordinateScale;
        const long double limit = static_cast<long double>(
            std::numeric_limits<std::int64_t>::max());
        if (std::abs(scaledX) > limit || std::abs(scaledY) > limit) {
            throw std::runtime_error(
                "GeoEast round-trip coordinate exceeds the canonicalization range.");
        }
        ring.push_back({
            static_cast<std::int64_t>(std::llround(scaledX)),
            static_cast<std::int64_t>(std::llround(scaledY))});
    }
    if (ring.size() >= 2 && ring.front() == ring.back()) {
        ring.pop_back();
    }
    CanonicalRing forward = minimumRingRotation(ring);
    std::reverse(ring.begin(), ring.end());
    CanonicalRing reverse = minimumRingRotation(ring);
    return reverse < forward ? reverse : forward;
}

std::vector<CanonicalRing> canonicalizeGeometry(
    const std::vector<fpt::Polygon>& polygons,
    bool keptOnly,
    std::size_t& vertexCount) {
    std::vector<CanonicalRing> rings;
    vertexCount = 0;
    for (const fpt::Polygon& polygon : polygons) {
        if (keptOnly && !polygon.kept) {
            continue;
        }
        if (polygon.points.size() < 3) {
            continue;
        }
        CanonicalRing ring = canonicalizeRing(polygon.points);
        vertexCount += ring.size();
        rings.push_back(std::move(ring));
    }
    std::sort(rings.begin(), rings.end());
    return rings;
}

void fnvAdd(std::uint64_t& hash, std::uint64_t value) {
    constexpr std::uint64_t prime = 1099511628211ULL;
    for (int byte = 0; byte < 8; ++byte) {
        hash ^= (value >> (byte * 8)) & 0xffULL;
        hash *= prime;
    }
}

std::string canonicalGeometryHash(const std::vector<CanonicalRing>& rings) {
    std::uint64_t hash = 1469598103934665603ULL;
    fnvAdd(hash, static_cast<std::uint64_t>(rings.size()));
    for (const CanonicalRing& ring : rings) {
        fnvAdd(hash, static_cast<std::uint64_t>(ring.size()));
        for (const QuantizedPoint& point : ring) {
            fnvAdd(hash, static_cast<std::uint64_t>(point[0]));
            fnvAdd(hash, static_cast<std::uint64_t>(point[1]));
        }
    }
    std::ostringstream text;
    text << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << hash;
    return text.str();
}

bool equalStyle(
    const fpt_geoeast::FaultPolygonStyle& left,
    const fpt_geoeast::FaultPolygonStyle& right) {
    return left.red == right.red &&
           left.green == right.green &&
           left.blue == right.blue &&
           std::abs(left.lineWidth - right.lineWidth) <= 1e-12;
}

FaultPolygonRoundTripReport verifyFaultPolygonRoundTrip(
    const fpt_geoeast::GeoEastProjectPtr& project,
    const std::string& outputName,
    const std::vector<fpt::Polygon>& source,
    const fpt_geoeast::FaultPolygonStyle& expectedStyle) {
    FaultPolygonRoundTripReport report;
    for (const fpt::Polygon& polygon : source) {
        if (polygon.kept && polygon.points.size() >= 3) {
            ++report.expectedPolygonCount;
            report.sourceInteriorRingCount += polygon.holes.size();
        }
    }

    const std::vector<fpt::Polygon> actual =
        fpt_geoeast::readFaultPolygons(project, outputName);
    report.actualPolygonCount = actual.size();
    if (actual.size() != report.expectedPolygonCount) {
        throw std::runtime_error(
            "GeoEast round-trip polygon count mismatch: expected " +
            std::to_string(report.expectedPolygonCount) + ", got " +
            std::to_string(actual.size()));
    }

    const std::vector<CanonicalRing> expectedGeometry =
        canonicalizeGeometry(source, true, report.expectedVertexCount);
    const std::vector<CanonicalRing> actualGeometry =
        canonicalizeGeometry(actual, false, report.actualVertexCount);
    report.expectedGeometryHash = canonicalGeometryHash(expectedGeometry);
    report.actualGeometryHash = canonicalGeometryHash(actualGeometry);
    if (expectedGeometry != actualGeometry) {
        throw std::runtime_error(
            "GeoEast round-trip exterior coordinates differ after canonicalization: expected hash " +
            report.expectedGeometryHash + ", got " + report.actualGeometryHash + ".");
    }
    report.geometryVerified = true;

    const fpt::TopologyDiagnostics topology = fpt::diagnoseTopology(actual, false);
    if (!fpt::topologyIsValid(topology)) {
        throw std::runtime_error(
            "GeoEast round-trip geometry failed complete topology validation.");
    }
    report.topologyVerified = true;

    report.storage = fpt_geoeast::inspectFaultPolygonStorage(project, outputName);
    const bool countConsistent =
        report.storage.headerSegmentCount == static_cast<int>(report.storage.segmentCount) &&
        report.storage.segmentCount == report.expectedPolygonCount;
    const bool idsConsistent =
        report.storage.uniqueSegmentIdCount == report.storage.segmentCount &&
        report.storage.uniquePrimaryKeyCount == report.storage.segmentCount &&
        report.storage.duplicateSegmentIdCount == 0 &&
        report.storage.duplicatePrimaryKeyCount == 0;
    if (!countConsistent || !idsConsistent ||
        report.storage.openSegmentCount != 0 ||
        report.storage.invalidSegmentCount != 0) {
        throw std::runtime_error(
            "GeoEast round-trip storage metadata is inconsistent or contains open/invalid segments.");
    }
    report.storageVerified = true;

    if (!equalStyle(expectedStyle, report.storage.style)) {
        throw std::runtime_error("GeoEast round-trip polygon style mismatch.");
    }
    report.styleVerified = true;
    report.completed = true;

    std::cout << "GeoEast round-trip verified: " << report.actualPolygonCount
              << " closed exterior rings, " << report.actualVertexCount
              << " vertices, geometry hash " << report.actualGeometryHash
              << ", topology/style/storage valid.\n";
    if (report.sourceInteriorRingCount > 0) {
        std::cout << "GeoEast interior-ring policy: "
                  << fpt_geoeast::faultPolygonInteriorRingPolicyName(
                         fpt_geoeast::FaultPolygonInteriorRingPolicy::
                             OmitUnsupportedInteriorRings)
                  << "; omitted " << report.sourceInteriorRingCount
                  << " unsupported interior ring(s).\n";
    }
    return report;
}

std::string makeStagingName(
    const fpt_geoeast::GeoEastProjectPtr& project) {
    const auto ticks = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const std::string prefix = "FPT_STAGE_" + std::to_string(ticks);
    for (int suffix = 0; suffix < 100; ++suffix) {
        const std::string candidate = suffix == 0
            ? prefix
            : prefix + "_" + std::to_string(suffix);
        if (!fpt_geoeast::faultPolygonsExist(project, candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("Cannot allocate a unique GeoEast staging object name.");
}

std::string jsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (unsigned char character : value) {
        switch (character) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (character < 0x20) {
                escaped += "?";
            } else {
                escaped.push_back(static_cast<char>(character));
            }
        }
    }
    return escaped;
}

void writeStagedCommitReport(
    const std::string& path,
    const std::string& targetName,
    const std::string& stagingName,
    bool previousObjectExisted,
    const fpt_geoeast::StagedCommitResult& result,
    const FaultPolygonRoundTripReport& stagingVerification,
    const FaultPolygonRoundTripReport& targetVerification) {
    if (path.empty()) {
        return;
    }
    fpt::AtomicOutputFile output(std::filesystem::u8path(path), 256 * 1024);
    std::ostream& out = output.stream();
    out << "{\n"
        << "  \"format\": \"FaultPolyTrace.GeoEastStagedCommit\",\n"
        << "  \"schemaVersion\": 2,\n"
        << "  \"mode\": \"application_staged_commit\",\n"
        << "  \"targetName\": \"" << jsonEscape(targetName) << "\",\n"
        << "  \"stagingName\": \"" << jsonEscape(stagingName) << "\",\n"
        << "  \"previousObjectExisted\": " << (previousObjectExisted ? "true" : "false") << ",\n"
        << "  \"stagingVerified\": " << (result.stagingVerified ? "true" : "false") << ",\n"
        << "  \"targetVerified\": " << (result.targetVerified ? "true" : "false") << ",\n"
        << "  \"rollbackPerformed\": " << (result.rollbackPerformed ? "true" : "false") << ",\n"
        << "  \"rollbackSucceeded\": " << (result.rollbackSucceeded ? "true" : "false") << ",\n"
        << "  \"stagingCleanupSucceeded\": " << (result.stagingCleanupSucceeded ? "true" : "false") << ",\n"
        << "  \"committed\": " << (result.committed ? "true" : "false") << ",\n"
        << "  \"success\": " << (result.success() ? "true" : "false") << ",\n"
        << "  \"interiorRingPolicy\": \""
        << fpt_geoeast::faultPolygonInteriorRingPolicyName(
               fpt_geoeast::FaultPolygonInteriorRingPolicy::
                   OmitUnsupportedInteriorRings)
        << "\",\n"
        << "  \"sourceInteriorRingCount\": "
        << std::max(stagingVerification.sourceInteriorRingCount,
                    targetVerification.sourceInteriorRingCount) << ",\n"
        << "  \"stagingReadback\": {\n"
        << "    \"completed\": " << (stagingVerification.completed ? "true" : "false") << ",\n"
        << "    \"expectedPolygonCount\": " << stagingVerification.expectedPolygonCount << ",\n"
        << "    \"actualPolygonCount\": " << stagingVerification.actualPolygonCount << ",\n"
        << "    \"expectedGeometryHash\": \""
        << jsonEscape(stagingVerification.expectedGeometryHash) << "\",\n"
        << "    \"actualGeometryHash\": \""
        << jsonEscape(stagingVerification.actualGeometryHash) << "\"\n"
        << "  },\n"
        << "  \"targetReadback\": {\n"
        << "    \"completed\": " << (targetVerification.completed ? "true" : "false") << ",\n"
        << "    \"geometryVerified\": " << (targetVerification.geometryVerified ? "true" : "false") << ",\n"
        << "    \"topologyVerified\": " << (targetVerification.topologyVerified ? "true" : "false") << ",\n"
        << "    \"styleVerified\": " << (targetVerification.styleVerified ? "true" : "false") << ",\n"
        << "    \"storageVerified\": " << (targetVerification.storageVerified ? "true" : "false") << ",\n"
        << "    \"expectedPolygonCount\": " << targetVerification.expectedPolygonCount << ",\n"
        << "    \"actualPolygonCount\": " << targetVerification.actualPolygonCount << ",\n"
        << "    \"expectedVertexCount\": " << targetVerification.expectedVertexCount << ",\n"
        << "    \"actualVertexCount\": " << targetVerification.actualVertexCount << ",\n"
        << "    \"expectedGeometryHash\": \""
        << jsonEscape(targetVerification.expectedGeometryHash) << "\",\n"
        << "    \"actualGeometryHash\": \""
        << jsonEscape(targetVerification.actualGeometryHash) << "\",\n"
        << "    \"headerSegmentCount\": " << targetVerification.storage.headerSegmentCount << ",\n"
        << "    \"storedSegmentCount\": " << targetVerification.storage.segmentCount << ",\n"
        << "    \"closedSegmentCount\": " << targetVerification.storage.closedSegmentCount << ",\n"
        << "    \"openSegmentCount\": " << targetVerification.storage.openSegmentCount << ",\n"
        << "    \"invalidSegmentCount\": " << targetVerification.storage.invalidSegmentCount << ",\n"
        << "    \"pointCount\": " << targetVerification.storage.pointCount << ",\n"
        << "    \"uniqueSegmentIdCount\": " << targetVerification.storage.uniqueSegmentIdCount << ",\n"
        << "    \"uniqueCombinedIdCount\": " << targetVerification.storage.uniqueCombinedIdCount << ",\n"
        << "    \"uniquePrimaryKeyCount\": " << targetVerification.storage.uniquePrimaryKeyCount << ",\n"
        << "    \"style\": {\"red\": " << targetVerification.storage.style.red
        << ", \"green\": " << targetVerification.storage.style.green
        << ", \"blue\": " << targetVerification.storage.style.blue
        << ", \"lineWidth\": " << targetVerification.storage.style.lineWidth << "}\n"
        << "  },\n"
        << "  \"failureMessage\": \"" << jsonEscape(result.failureMessage) << "\",\n"
        << "  \"rollbackFailureMessage\": \"" << jsonEscape(result.rollbackFailureMessage) << "\",\n"
        << "  \"cleanupFailureMessage\": \"" << jsonEscape(result.cleanupFailureMessage) << "\"\n"
        << "}\n";
    output.commit();
}

fpt_geoeast::StagedCommitResult commitFaultPolygonsWithReadback(
    const fpt_geoeast::GeoEastProjectPtr& project,
    const std::string& outputName,
    const std::vector<fpt::Polygon>& polygons,
    const fpt_geoeast::FaultPolygonStyle& style,
    const std::string& reportPath) {
    const bool previousExists =
        fpt_geoeast::faultPolygonsExist(project, outputName);
    std::vector<fpt::Polygon> previousPolygons;
    fpt_geoeast::FaultPolygonStyle previousStyle;
    if (previousExists) {
        previousPolygons = fpt_geoeast::readFaultPolygons(project, outputName);
        previousStyle = fpt_geoeast::readFaultPolygonStyle(project, outputName);
    }
    const std::string stagingName = makeStagingName(project);
    FaultPolygonRoundTripReport stagingVerification;
    FaultPolygonRoundTripReport targetVerification;

    fpt_geoeast::StagedCommitOperations operations;
    operations.writeStaging = [&](const std::string& name) {
        fpt_geoeast::writeFaultPolygons(project, name, polygons, true, style);
    };
    operations.verifyStaging = [&](const std::string& name) {
        stagingVerification = verifyFaultPolygonRoundTrip(
            project, name, polygons, style);
    };
    operations.replaceTarget = [&](const std::string& name) {
        fpt_geoeast::writeFaultPolygons(project, name, polygons, true, style);
    };
    operations.verifyTarget = [&](const std::string& name) {
        targetVerification = verifyFaultPolygonRoundTrip(
            project, name, polygons, style);
    };
    operations.rollbackTarget = [&](const std::string& name) {
        if (previousExists) {
            fpt_geoeast::writeFaultPolygons(
                project, name, previousPolygons, true, previousStyle);
            verifyFaultPolygonRoundTrip(
                project, name, previousPolygons, previousStyle);
        } else if (fpt_geoeast::faultPolygonsExist(project, name)) {
            fpt_geoeast::deleteFaultPolygons(project, name);
        }
    };
    operations.deleteStaging = [&](const std::string& name) {
        if (fpt_geoeast::faultPolygonsExist(project, name)) {
            fpt_geoeast::deleteFaultPolygons(project, name);
        }
    };

    const fpt_geoeast::StagedCommitResult result =
        fpt_geoeast::runStagedCommit(outputName, stagingName, operations);
    writeStagedCommitReport(
        reportPath,
        outputName,
        stagingName,
        previousExists,
        result,
        stagingVerification,
        targetVerification);
    if (!result.success()) {
        std::string message = "GeoEast staged commit failed: " + result.failureMessage;
        if (result.rollbackPerformed && !result.rollbackSucceeded) {
            message += "; rollback failed: " + result.rollbackFailureMessage;
        }
        if (!result.stagingCleanupSucceeded) {
            message += "; staging cleanup failed: " + result.cleanupFailureMessage;
        }
        throw std::runtime_error(message);
    }
    return result;
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

        if (options.inspectFaultPolygon) {
            const fpt_geoeast::FaultPolygonStorageSummary summary =
                fpt_geoeast::inspectFaultPolygonStorage(project, options.outputName);
            std::cout << "BOFaultPolygon storage: " << options.outputName << "\n"
                      << "  header segments: " << summary.headerSegmentCount << "\n"
                      << "  stored segments: " << summary.segmentCount << "\n"
                      << "  closed/open/invalid: " << summary.closedSegmentCount
                      << "/" << summary.openSegmentCount
                      << "/" << summary.invalidSegmentCount << "\n"
                      << "  points: " << summary.pointCount << "\n"
                      << "  unique segment/combined/pk IDs: "
                      << summary.uniqueSegmentIdCount << "/"
                      << summary.uniqueCombinedIdCount << "/"
                      << summary.uniquePrimaryKeyCount << "\n"
                      << "  duplicate segment/pk IDs: "
                      << summary.duplicateSegmentIdCount << "/"
                      << summary.duplicatePrimaryKeyCount << "\n"
                      << "  style RGB/width: " << summary.style.red << ","
                      << summary.style.green << "," << summary.style.blue
                      << "/" << summary.style.lineWidth << "\n";
            return 0;
        }

        if (options.deleteGrid) {
            debugLog("Deleting BOMapGrid: " + options.gridName);
            fpt_geoeast::deleteMapGrid(project, options.gridName);
            debugLog("BOMapGrid delete completed.");
            std::cout << "Deleted BOMapGrid: " << options.gridName << "\n";
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

        if (!options.importFaultPolygonsCsvPath.empty()) {
            debugLog(
                "Reading locked FaultPolyTrace polygon CSV: " +
                options.importFaultPolygonsCsvPath);
            const std::vector<fpt::Polygon> polygons =
                fpt::readKeptPolygonCsv(options.importFaultPolygonsCsvPath);
            if (options.expectedImportedPolygonCount > 0 &&
                static_cast<int>(polygons.size()) !=
                    options.expectedImportedPolygonCount) {
                throw std::runtime_error(
                    "Imported polygon count mismatch: expected " +
                    std::to_string(options.expectedImportedPolygonCount) +
                    ", got " + std::to_string(polygons.size()) + ".");
            }
            const fpt::TopologyDiagnostics topology =
                fpt::diagnoseTopology(polygons, true);
            if (!fpt::topologyIsValid(topology)) {
                throw std::runtime_error(
                    "Imported polygon CSV failed topology validation: open=" +
                    std::to_string(topology.openRingCount) +
                    ", self-intersection=" +
                    std::to_string(topology.selfIntersectingPolygonCount) +
                    ", polygon-intersection=" +
                    std::to_string(topology.polygonIntersectionCount) +
                    ", invalid-polygon=" +
                    std::to_string(topology.invalidPolygonCount) +
                    ", containment=" +
                    std::to_string(topology.polygonContainmentCount) + ".");
            }
            if (!options.topologyOutputPath.empty()) {
                fpt::writeTopologyDetailsCsv(options.topologyOutputPath, topology);
            }
            if (!options.previewPolygonsOutputPath.empty()) {
                fpt::writePolygonCsv(options.previewPolygonsOutputPath, polygons);
            }
            const std::string transactionReportPath =
                !options.transactionReportPath.empty()
                    ? options.transactionReportPath
                    : (options.diagnosticsOutputPath.empty()
                        ? std::string()
                        : options.diagnosticsOutputPath +
                            ".geoeast-commit.json");
            commitFaultPolygonsWithReadback(
                project,
                options.outputName,
                polygons,
                options.polygonStyle,
                transactionReportPath);
            std::cout << "Imported locked FaultPolyTrace polygons: "
                      << polygons.size() << "\n";
            std::cout << "BOFaultPolygon: " << options.outputName << "\n";
            std::cout << "Topology: open=0 self-intersection=0 "
                         "polygon-intersection=0\n";
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
        if (!options.weakContinuityOutputPath.empty()) {
            fpt::writeWeakContinuityCandidatesWorldCsv(
                options.weakContinuityOutputPath,
                result.weakContinuityCandidates,
                grid);
        }
        if (!options.topologyOutputPath.empty()) {
            fpt::writeTopologyDetailsCsv(options.topologyOutputPath, topology);
        }
        if (!options.previewPolygonsOutputPath.empty()) {
            fpt::writePolygonCsv(options.previewPolygonsOutputPath, result.polygons);
        }
        debugLog("Writing BOFaultPolygon: " + options.outputName);
        const std::string transactionReportPath =
            !options.transactionReportPath.empty()
                ? options.transactionReportPath
                : (options.diagnosticsOutputPath.empty()
                    ? std::string()
                    : options.diagnosticsOutputPath + ".geoeast-commit.json");
        const fpt_geoeast::StagedCommitResult commitResult =
            commitFaultPolygonsWithReadback(
                project,
                options.outputName,
                result.polygons,
                options.polygonStyle,
                transactionReportPath);
        debugLog("BOFaultPolygon write completed.");

        int kept = 0;
        for (const fpt::Polygon& polygon : result.polygons) {
            if (polygon.kept) {
                ++kept;
            }
        }

        const auto end = std::chrono::steady_clock::now();
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (!options.diagnosticsOutputPath.empty()) {
            fpt::RunDiagnostics diagnostics;
            diagnostics.inputPath = std::string("BOMapGrid/") + options.gridName;
            diagnostics.outputPath = std::string("BOFaultPolygon/") + options.outputName;
            diagnostics.traceMode = options.trace.traceMode == fpt::TraceMode::RidgeBelt
                ? "ridge-belt" : "region";
            diagnostics.thresholdMode = options.trace.thresholdMode == fpt::ThresholdMode::Fixed
                ? "fixed"
                : (options.trace.thresholdMode == fpt::ThresholdMode::PositivePercentile
                    ? "positive-percentile" : "percentile");
            diagnostics.threshold = options.trace.threshold;
            diagnostics.percentile = options.trace.percentile;
            diagnostics.effectiveThreshold = result.effectiveThreshold;
            diagnostics.primaryThresholdInfo = result.primaryThresholdInfo;
            diagnostics.weakThresholdInfo = result.weakThresholdInfo;
            diagnostics.residualThresholdInfo = result.residualThresholdInfo;
            diagnostics.minArea = options.trace.minArea;
            diagnostics.minCells = options.trace.minCells;
            diagnostics.openRadius = options.trace.openRadius;
            diagnostics.closeRadius = options.trace.closeRadius;
            diagnostics.physicalMorphologyShadow =
                options.trace.physicalMorphologyShadow;
            diagnostics.physicalCloseRadiusWorld =
                options.trace.physicalCloseRadiusWorld;
            diagnostics.weakPercentile = options.trace.weakPercentile;
            diagnostics.weakConnectRadius = options.trace.weakConnectRadius;
            diagnostics.maxWeakComponentCells = options.trace.maxWeakComponentCells;
            diagnostics.minWeakComponentAspect = options.trace.minWeakComponentAspect;
            diagnostics.weakPathGate = options.trace.weakPathGate;
            diagnostics.weakExtensionLayeredShadow =
                options.trace.weakExtensionLayeredShadow;
            diagnostics.weakExtensionLayeredGate =
                options.trace.weakExtensionLayeredGate;
            diagnostics.weakPathSupportRatio = options.trace.weakPathSupportRatio;
            diagnostics.weakPathMaxUnsupportedRun = options.trace.weakPathMaxUnsupportedRun;
            diagnostics.branchAttributionShadow = options.trace.branchAttributionShadow;
            diagnostics.branchAttributionCandidateGroups = options.trace.branchAttributionCandidateGroups;
            diagnostics.skeletonGraphV2Shadow = options.trace.skeletonGraphV2Shadow;
            diagnostics.skeletonGraphV2AttributionShadow =
                options.trace.skeletonGraphV2AttributionShadow;
            diagnostics.multiScaleEvidenceShadow =
                options.trace.multiScaleEvidenceShadow;
            diagnostics.multiScaleEvidenceMinimumConfidence =
                options.trace.multiScaleEvidenceMinimumConfidence;
            diagnostics.multiScaleEvidenceMinimumRidgeResponse =
                options.trace.multiScaleEvidenceMinimumRidgeResponse;
            diagnostics.multiScaleEvidenceMaximumSamples =
                options.trace.multiScaleEvidenceMaximumSamples;
            diagnostics.adaptiveBoundaryShadow =
                options.trace.adaptiveBoundaryShadow;
            diagnostics.adaptiveBoundaryApply =
                options.trace.adaptiveBoundaryApply;
            diagnostics.adaptiveBoundaryMaximumDisplacementWorld =
                options.trace.adaptiveBoundaryMaximumDisplacementWorld;
            diagnostics.adaptiveBoundaryMinimumSupportGain =
                options.trace.adaptiveBoundaryMinimumSupportGain;
            diagnostics.adaptiveBoundaryMaximumSupportLossRatio =
                options.trace.adaptiveBoundaryMaximumSupportLossRatio;
            diagnostics.adaptiveBoundaryMaximumLeakageIncreaseRatio =
                options.trace.adaptiveBoundaryMaximumLeakageIncreaseRatio;
            diagnostics.adaptiveBoundaryMaximumPolygons =
                options.trace.adaptiveBoundaryMaximumPolygons;
            diagnostics.adaptiveBoundaryMaximumVerticesPerPolygon =
                options.trace.adaptiveBoundaryMaximumVerticesPerPolygon;
            diagnostics.localPolygonQualityShadow = options.trace.localPolygonQualityShadow;
            diagnostics.localPolygonQuality = options.trace.localPolygonQuality;
            diagnostics.localQualityWidthRatio = options.trace.localQualityWidthRatio;
            diagnostics.localQualityMinExtraCells = options.trace.localQualityMinExtraCells;
            diagnostics.localQualitySupportThresholdRatio =
                options.trace.localQualitySupportThresholdRatio;
            diagnostics.localQualitySupportBufferCells =
                options.trace.localQualitySupportBufferCells;
            diagnostics.localQualityMaxAreaLossRatio =
                options.trace.localQualityMaxAreaLossRatio;
            diagnostics.localQualityMaxStrongCoverageLossRatio =
                options.trace.localQualityMaxStrongCoverageLossRatio;
            diagnostics.localQualityMaxWeakCoverageLossRatio =
                options.trace.localQualityMaxWeakCoverageLossRatio;
            diagnostics.localQualityMaxBackgroundLeakageIncreaseRatio =
                options.trace.localQualityMaxBackgroundLeakageIncreaseRatio;
            diagnostics.localQualityStabilityThresholdDeltaRatio =
                options.trace.localQualityStabilityThresholdDeltaRatio;
            diagnostics.localQualityMinStabilityJaccard =
                options.trace.localQualityMinStabilityJaccard;
            diagnostics.localQualityMaxCandidatesPerObject =
                options.trace.localQualityMaxCandidatesPerObject;
            diagnostics.localQualityMaxTotalCandidates =
                options.trace.localQualityMaxTotalCandidates;
            diagnostics.localQualityMaxWindowCells =
                options.trace.localQualityMaxWindowCells;
            diagnostics.localQualityMaxObjectCells =
                options.trace.localQualityMaxObjectCells;
            diagnostics.localQualityMaxEvidenceRecords =
                options.trace.localQualityMaxEvidenceRecords;
            diagnostics.beltRadius = options.trace.beltRadius;
            diagnostics.postBeltCloseRadius = options.trace.postBeltCloseRadius;
            diagnostics.attributeConstrainedBelt = options.trace.attributeConstrainedBelt;
            diagnostics.attributeEnvelopeRadius = options.trace.attributeEnvelopeRadius;
            diagnostics.minBranchLength = options.trace.minBranchLength;
            diagnostics.minJunctionBranchLength = options.trace.minJunctionBranchLength;
            diagnostics.minJunctionBranchMean = options.trace.minJunctionBranchMean;
            diagnostics.maxGapDistance = options.trace.maxGapDistance;
            diagnostics.maxGapAngle = options.trace.maxGapAngle;
            diagnostics.minGapSupportRatio = options.trace.minGapSupportRatio;
            diagnostics.weakContinuityShadow = options.trace.weakContinuityShadow;
            diagnostics.weakContinuity = options.trace.weakContinuity;
            diagnostics.weakContinuitySecondPass =
                options.trace.weakContinuitySecondPass;
            diagnostics.weakContinuityFinalObjectPass =
                options.trace.weakContinuityFinalObjectPass;
            diagnostics.weakContinuityMaxDistance = options.trace.weakContinuityMaxDistance;
            diagnostics.weakContinuityTangentWindow = options.trace.weakContinuityTangentWindow;
            diagnostics.weakContinuityCorridorRadius = options.trace.weakContinuityCorridorRadius;
            diagnostics.weakContinuityMaxCandidatesPerEndpoint =
                options.trace.weakContinuityMaxCandidatesPerEndpoint;
            diagnostics.weakContinuityMaxEndpointAngle =
                options.trace.weakContinuityMaxEndpointAngle;
            diagnostics.weakContinuityMaxWidthRatio = options.trace.weakContinuityMaxWidthRatio;
            diagnostics.weakContinuityMinAttributeRatio =
                options.trace.weakContinuityMinAttributeRatio;
            diagnostics.weakContinuityReferenceThresholdRatio =
                options.trace.weakContinuityReferenceThresholdRatio;
            diagnostics.weakContinuityLocalContrastRatio =
                options.trace.weakContinuityLocalContrastRatio;
            diagnostics.weakContinuityMinSupportRatio =
                options.trace.weakContinuityMinSupportRatio;
            diagnostics.weakContinuityMinMeanSupport =
                options.trace.weakContinuityMinMeanSupport;
            diagnostics.weakContinuityMaxUnsupportedRun =
                options.trace.weakContinuityMaxUnsupportedRun;
            diagnostics.weakContinuityMaxPathLengthRatio =
                options.trace.weakContinuityMaxPathLengthRatio;
            diagnostics.weakContinuityMaxTurnAngle = options.trace.weakContinuityMaxTurnAngle;
            diagnostics.weakContinuityMinCompetitionMargin =
                options.trace.weakContinuityMinCompetitionMargin;
            diagnostics.weakContinuityUsePhysicalMetric =
                options.trace.weakContinuityUsePhysicalMetric;
            diagnostics.weakContinuityUseGlobalMatching =
                options.trace.weakContinuityUseGlobalMatching;
            diagnostics.weakContinuityNoConnectCost =
                options.trace.weakContinuityNoConnectCost;
            diagnostics.weakContinuityMaxExactMatchingEndpoints =
                options.trace.weakContinuityMaxExactMatchingEndpoints;
            diagnostics.weakContinuityThirdPartySkeletonClearanceRadius =
                options.trace.weakContinuityThirdPartySkeletonClearanceRadius;
            diagnostics.weakTailCompletionShadow = options.trace.weakTailCompletionShadow;
            diagnostics.weakTailCompletion = options.trace.weakTailCompletion;
            diagnostics.weakTailTopologyGate =
                options.trace.weakTailTopologyGate;
            diagnostics.simpleRingCompatibility =
                options.trace.simpleRingCompatibility;
            diagnostics.simpleRingMaximumFillCells =
                options.trace.simpleRingMaximumFillCells;
            diagnostics.simpleRingMaximumOpeningCells =
                options.trace.simpleRingMaximumOpeningCells;
            diagnostics.weakTailEndpointPortShadow =
                options.trace.weakTailEndpointPortShadow;
            diagnostics.weakTailEndpointPortOwnership =
                options.trace.weakTailEndpointPortOwnership;
            diagnostics.weakTailMinCells = options.trace.weakTailMinCells;
            diagnostics.weakTailMaxCells = options.trace.weakTailMaxCells;
            diagnostics.weakTailMinAspect = options.trace.weakTailMinAspect;
            diagnostics.weakTailMinMeanSupportRatio =
                options.trace.weakTailMinMeanSupportRatio;
            diagnostics.weakTailMinPeakSupportRatio =
                options.trace.weakTailMinPeakSupportRatio;
            diagnostics.weakTailMaxAlignmentAngle =
                options.trace.weakTailMaxAlignmentAngle;
            diagnostics.weakTailOwnershipClearanceRadius =
                options.trace.weakTailOwnershipClearanceRadius;
            diagnostics.weakTailTangentRadius = options.trace.weakTailTangentRadius;
            diagnostics.weakTailJunctionClearanceRadius =
                options.trace.weakTailJunctionClearanceRadius;
            diagnostics.preserveObservedSkeletonEnvelope =
                options.trace.preserveObservedSkeletonEnvelope;
            diagnostics.rootAntiMerge = options.trace.rootAntiMerge;
            diagnostics.rootAntiMergeRadius = options.trace.rootAntiMergeRadius;
            diagnostics.rootAntiMergeSupportRatio = options.trace.rootAntiMergeSupportRatio;
            diagnostics.rootAntiMergeMaxBridgeValue = options.trace.rootAntiMergeMaxBridgeValue;
            diagnostics.rootAntiMergeMinPartCells = options.trace.rootAntiMergeMinPartCells;
            diagnostics.rootAntiMergeMinPartRatio = options.trace.rootAntiMergeMinPartRatio;
            diagnostics.rootAntiMergeWeakGuardThreshold =
                options.trace.rootAntiMergeWeakGuardThreshold;
            diagnostics.rootAntiMergeWeakGuardRadius =
                options.trace.rootAntiMergeWeakGuardRadius;
            diagnostics.localAntiMerge = options.trace.localAntiMerge;
            diagnostics.localAntiMergeLowThreshold =
                options.trace.localAntiMergeLowThreshold;
            diagnostics.localAntiMergeBridgeRadius =
                options.trace.localAntiMergeBridgeRadius;
            diagnostics.localAntiMergeMinPartCells =
                options.trace.localAntiMergeMinPartCells;
            diagnostics.localAntiMergeMinPartRatio =
                options.trace.localAntiMergeMinPartRatio;
            diagnostics.localAntiMergeMinBridgeCells =
                options.trace.localAntiMergeMinBridgeCells;
            diagnostics.localAntiMergeMinLowRatio =
                options.trace.localAntiMergeMinLowRatio;
            diagnostics.adaptiveAreaFilter = options.trace.adaptiveAreaFilter;
            diagnostics.adaptiveAreaMinCells = options.trace.adaptiveAreaMinCells;
            diagnostics.adaptiveAreaMinAspect = options.trace.adaptiveAreaMinAspect;
            diagnostics.adaptiveAreaMinMeanRatio =
                options.trace.adaptiveAreaMinMeanRatio;
            diagnostics.adaptiveAreaMinMaxRatio =
                options.trace.adaptiveAreaMinMaxRatio;
            diagnostics.residualAttributeFill = options.trace.residualAttributeFill;
            diagnostics.residualTrace = options.trace.residualTrace;
            diagnostics.residualPercentile = options.trace.residualPercentile;
            diagnostics.residualFillRadius = options.trace.residualFillRadius;
            diagnostics.residualEdgeFillRadius = options.trace.residualEdgeFillRadius;
            diagnostics.residualEdgeFillMinValueRatio =
                options.trace.residualEdgeFillMinValueRatio;
            diagnostics.residualEdgeFillMaxPatchSpan =
                options.trace.residualEdgeFillMaxPatchSpan;
            diagnostics.residualMinCells = options.trace.residualMinCells;
            diagnostics.residualTraceMinLength = options.trace.residualTraceMinLength;
            diagnostics.residualTraceMaxCells = options.trace.residualTraceMaxCells;
            diagnostics.residualCoveredGuardRadius = options.trace.residualCoveredGuardRadius;
            diagnostics.residualTraceDetached = options.trace.residualTraceDetached;
            diagnostics.residualDetachedMinCells = options.trace.residualDetachedMinCells;
            diagnostics.residualDetachedMinLength = options.trace.residualDetachedMinLength;
            diagnostics.residualDetachedMinAspect = options.trace.residualDetachedMinAspect;
            diagnostics.residualDetachedMinMeanRatio =
                options.trace.residualDetachedMinMeanRatio;
            diagnostics.residualDetachedMinMaxRatio =
                options.trace.residualDetachedMinMaxRatio;
            diagnostics.simplifyTolerance = options.trace.simplifyTolerance;
            diagnostics.smoothIterations = options.trace.smoothIterations;
            diagnostics.preserveCoverageAfterSmoothing =
                options.trace.preserveCoverageAfterSmoothing;
            diagnostics.coveragePreserveMargin = options.trace.coveragePreserveMargin;
            diagnostics.gridNx = grid.nx();
            diagnostics.gridNy = grid.ny();
            diagnostics.activeCellCount = result.activeCellCount;
            diagnostics.physicalMorphologyShadowEvaluated =
                result.physicalMorphologyShadowEvaluated;
            diagnostics.physicalMorphologyRadiusWorld =
                result.physicalMorphologyRadiusWorld;
            diagnostics.physicalMorphologyLegacyCells =
                result.physicalMorphologyLegacyCells;
            diagnostics.physicalMorphologyCandidateCells =
                result.physicalMorphologyCandidateCells;
            diagnostics.physicalMorphologyAddedCells =
                result.physicalMorphologyAddedCells;
            diagnostics.physicalMorphologyRemovedCells =
                result.physicalMorphologyRemovedCells;
            diagnostics.physicalMorphologyJaccard =
                result.physicalMorphologyJaccard;
            diagnostics.polygonCount = static_cast<int>(result.polygons.size());
            diagnostics.keptPolygonCount = kept;
            diagnostics.skeletonEndpoints = result.skeletonEndpoints;
            diagnostics.skeletonJunctions = result.skeletonJunctions;
            diagnostics.skeletonBranches = result.skeletonBranches;
            diagnostics.skeletonGraphV2ShadowEvaluated =
                result.skeletonGraphV2ShadowEvaluated;
            diagnostics.skeletonGraphV2Stats = result.skeletonGraphV2Stats;
            diagnostics.skeletonGraphV2AttributionShadowEvaluated =
                result.skeletonGraphV2AttributionShadowEvaluated;
            diagnostics.skeletonGraphV2Attribution =
                result.skeletonGraphV2Attribution;
            diagnostics.multiScaleEvidenceShadowEvaluated =
                result.multiScaleEvidenceShadowEvaluated;
            diagnostics.multiScaleEvidenceSummary =
                result.multiScaleEvidenceSummary;
            diagnostics.adaptiveBoundaryShadowEvaluated =
                result.adaptiveBoundaryShadowEvaluated;
            diagnostics.adaptiveBoundaryApplied =
                result.adaptiveBoundaryApplied;
            diagnostics.adaptiveBoundarySummary =
                result.adaptiveBoundarySummary;
            diagnostics.adaptiveBoundaryCommitSummary =
                result.adaptiveBoundaryCommitSummary;
            diagnostics.adaptiveBoundaryEvidence =
                result.adaptiveBoundaryEvidence;
            diagnostics.keptBranches = result.keptBranches;
            diagnostics.prunedBranches = result.prunedBranches;
            diagnostics.connectedGaps = result.connectedGaps;
            diagnostics.weakExtensionLayeredShadowEvaluated =
                result.weakExtensionLayeredShadowEvaluated;
            diagnostics.weakExtensionComponentCount =
                result.weakExtensionComponentCount;
            diagnostics.weakExtensionLegacyAcceptedComponents =
                result.weakExtensionLegacyAcceptedComponents;
            diagnostics.weakExtensionLayeredAcceptedComponents =
                result.weakExtensionLayeredAcceptedComponents;
            diagnostics.weakExtensionLayeredRejectedComponents =
                result.weakExtensionLayeredRejectedComponents;
            diagnostics.weakExtensionLayeredOwnerRejectedComponents =
                result.weakExtensionLayeredOwnerRejectedComponents;
            diagnostics.weakExtensionLayeredSizeRejectedComponents =
                result.weakExtensionLayeredSizeRejectedComponents;
            diagnostics.weakExtensionLayeredShapeRejectedComponents =
                result.weakExtensionLayeredShapeRejectedComponents;
            diagnostics.weakExtensionLayeredSupportRejectedComponents =
                result.weakExtensionLayeredSupportRejectedComponents;
            diagnostics.weakExtensionLayeredWidthRejectedComponents =
                result.weakExtensionLayeredWidthRejectedComponents;
            diagnostics.weakExtensionLegacyOnlyCells =
                result.weakExtensionLegacyOnlyCells;
            diagnostics.weakExtensionLayeredOnlyCells =
                result.weakExtensionLayeredOnlyCells;
            diagnostics.weakContinuityStats = result.weakContinuityStats;
            diagnostics.weakContinuityProtectedFromAntiMergeCells =
                result.weakContinuityProtectedFromAntiMergeCells;
            diagnostics.weakContinuityFinalObjectAcceptedConnections =
                result.weakContinuityFinalObjectAcceptedConnections;
            diagnostics.weakContinuityFinalObjectRejectedConnections =
                result.weakContinuityFinalObjectRejectedConnections;
            diagnostics.weakContinuityFinalObjectAddedEnvelopeCells =
                result.weakContinuityFinalObjectAddedEnvelopeCells;
            diagnostics.weakTailStats = result.weakTailStats;
            diagnostics.weakTailEndpointPortShadowEvaluated =
                result.weakTailEndpointPortShadowEvaluated;
            diagnostics.weakTailEndpointPortShadowStats =
                result.weakTailEndpointPortShadowStats;
            diagnostics.weakTailProtectedFromAntiMergeCells =
                result.weakTailProtectedFromAntiMergeCells;
            diagnostics.simpleRingCompatibilityApplied =
                result.simpleRingCompatibilityApplied;
            diagnostics.simpleRingCompatibilityStats =
                result.simpleRingCompatibilityStats;
            diagnostics.rootAntiMergeCandidateCutCells =
                result.rootAntiMergeCandidateCutCells;
            diagnostics.rootAntiMergeCutCells = result.rootAntiMergeCutCells;
            diagnostics.rootAntiMergeRejectedCutCells = result.rootAntiMergeRejectedCutCells;
            diagnostics.rootAntiMergeAcceptedComponents =
                result.rootAntiMergeAcceptedComponents;
            diagnostics.rootAntiMergeRejectedComponents =
                result.rootAntiMergeRejectedComponents;
            diagnostics.rootAntiMergeWeakGuardRejectedCutCells =
                result.rootAntiMergeWeakGuardRejectedCutCells;
            diagnostics.rootAntiMergeWeakGuardRejectedComponents =
                result.rootAntiMergeWeakGuardRejectedComponents;
            diagnostics.residualFillCandidateCells = result.residualFillCandidateCells;
            diagnostics.residualFillAddedCells = result.residualFillAddedCells;
            diagnostics.residualFillRejectedComponents =
                result.residualFillRejectedComponents;
            diagnostics.residualTraceEffectiveThreshold =
                result.residualTraceEffectiveThreshold;
            diagnostics.residualTraceCandidateComponents =
                result.residualTraceCandidateComponents;
            diagnostics.residualTraceSizeRejectedComponents =
                result.residualTraceSizeRejectedComponents;
            diagnostics.residualTraceShapeRejectedComponents =
                result.residualTraceShapeRejectedComponents;
            diagnostics.residualTraceAttributeRejectedComponents =
                result.residualTraceAttributeRejectedComponents;
            diagnostics.residualTraceDetachedRejectedComponents =
                result.residualTraceDetachedRejectedComponents;
            diagnostics.residualTraceGeometryRejectedComponents =
                result.residualTraceGeometryRejectedComponents;
            diagnostics.residualTraceTopologyRejectedComponents =
                result.residualTraceTopologyRejectedComponents;
            diagnostics.residualTraceFallbackAcceptedComponents =
                result.residualTraceFallbackAcceptedComponents;
            diagnostics.residualTraceAcceptedComponents =
                result.residualTraceAcceptedComponents;
            diagnostics.residualTraceAcceptedPolygons =
                result.residualTraceAcceptedPolygons;
            diagnostics.residualTraceAcceptedCells =
                result.residualTraceAcceptedCells;
            diagnostics.localQualityDetectedCount = result.localPolygonQualityStats.detectedCount;
            diagnostics.localQualityAcceptedCount = result.localPolygonQualityStats.acceptedCount;
            diagnostics.localQualityReviewCount = result.localPolygonQualityStats.reviewCount;
            diagnostics.localQualityRejectedCount = result.localPolygonQualityStats.rejectedCount;
            diagnostics.localQualityTotalEvidence = result.localPolygonQualityStats.totalEvidence;
            diagnostics.localQualityStoredEvidence = result.localPolygonQualityStats.storedEvidence;
            diagnostics.localQualityTruncatedEvidence = result.localPolygonQualityStats.truncatedEvidence;
            diagnostics.localQualityMaxEvidenceRecords = options.trace.localQualityMaxEvidenceRecords;
            diagnostics.localPolygonQualityEvidence = result.localPolygonQualityEvidence;
            diagnostics.openRingCount = topology.openRingCount;
            diagnostics.selfIntersectingPolygonCount = topology.selfIntersectingPolygonCount;
            diagnostics.polygonIntersectionCount = topology.polygonIntersectionCount;
            diagnostics.invalidPolygonCount = topology.invalidPolygonCount;
            diagnostics.nonFiniteCoordinateCount = topology.nonFiniteCoordinateCount;
            diagnostics.zeroLengthEdgeCount = topology.zeroLengthEdgeCount;
            diagnostics.duplicateVertexRingCount = topology.duplicateVertexRingCount;
            diagnostics.degenerateRingCount = topology.degenerateRingCount;
            diagnostics.holeOutsideShellCount = topology.holeOutsideShellCount;
            diagnostics.nestedHoleCount = topology.nestedHoleCount;
            diagnostics.polygonContainmentCount = topology.polygonContainmentCount;
            diagnostics.polygonInteriorOverlapCount = topology.polygonInteriorOverlapCount;
            diagnostics.topologyRingValidationMs = topology.ringValidationMs;
            diagnostics.topologySelfIntersectionMs = topology.selfIntersectionMs;
            diagnostics.topologyHoleValidationMs = topology.holeValidationMs;
            diagnostics.topologyPairConflictMs = topology.pairConflictMs;
            diagnostics.elapsedMs = static_cast<int>(std::min<long long>(
                elapsedMs, std::numeric_limits<int>::max()));
            diagnostics.traceStages = result.stageTimings;
            fpt::writeDiagnosticsJson(options.diagnosticsOutputPath, diagnostics);
        }

        std::cout << "FaultPolyTrace GeoEast runner completed.\n";
        std::cout << "Grid: " << grid.nx() << " x " << grid.ny() << "\n";
        std::cout << "Effective threshold: " << result.effectiveThreshold << "\n";
        std::cout << "Active cells: " << result.activeCellCount << "\n";
        if (result.physicalMorphologyShadowEvaluated) {
            std::cout << "Physical morphology shadow legacy/candidate/added/removed/Jaccard: "
                      << result.physicalMorphologyLegacyCells << "/"
                      << result.physicalMorphologyCandidateCells << "/"
                      << result.physicalMorphologyAddedCells << "/"
                      << result.physicalMorphologyRemovedCells << "/"
                      << result.physicalMorphologyJaccard << "\n";
        }
        std::cout << "Skeleton endpoints: " << result.skeletonEndpoints << "\n";
        std::cout << "Skeleton junctions: " << result.skeletonJunctions << "\n";
        std::cout << "Skeleton branches: " << result.skeletonBranches << "\n";
        if (result.skeletonGraphV2ShadowEvaluated) {
            std::cout << "SkeletonGraphV2 raw junctions/clusters/branches/ports/half-edges: "
                      << result.skeletonGraphV2Stats.rawJunctionPixels << "/"
                      << result.skeletonGraphV2Stats.junctionClusters << "/"
                      << result.skeletonGraphV2Stats.branches << "/"
                      << result.skeletonGraphV2Stats.ports << "/"
                      << result.skeletonGraphV2Stats.halfEdges << "\n";
        }
        std::cout << "Connected gaps: " << result.connectedGaps << "\n";
        std::cout << "Kept branches: " << result.keptBranches << "\n";
        std::cout << "Pruned branches: " << result.prunedBranches << "\n";
        std::cout << "Residual fill candidate cells: " << result.residualFillCandidateCells << "\n";
        std::cout << "Residual fill added cells: " << result.residualFillAddedCells << "\n";
        std::cout << "Residual fill rejected components: " << result.residualFillRejectedComponents << "\n";
        if (options.trace.residualTrace) {
            std::cout << "Residual trace candidate/accepted components: "
                      << result.residualTraceCandidateComponents << "/"
                      << result.residualTraceAcceptedComponents << "\n";
            std::cout << "Residual trace accepted polygons/cells: "
                      << result.residualTraceAcceptedPolygons << "/"
                      << result.residualTraceAcceptedCells << "\n";
        }
        if (options.trace.weakContinuity ||
            options.trace.weakContinuityFinalObjectPass) {
            std::cout << "Weak continuity protected anti-merge cells: "
                      << result.weakContinuityProtectedFromAntiMergeCells << "\n";
            if (options.trace.weakContinuityFinalObjectPass) {
                std::cout << "Final-object weak continuity accepted/rejected: "
                          << result.weakContinuityFinalObjectAcceptedConnections << "/"
                          << result.weakContinuityFinalObjectRejectedConnections << "\n";
                std::cout << "Final-object weak continuity added envelope cells: "
                          << result.weakContinuityFinalObjectAddedEnvelopeCells << "\n";
            }
        }
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
        std::cout << "Topology invalid polygons: " << topology.invalidPolygonCount << "\n";
        std::cout << "Topology containments/interior overlaps: "
                  << topology.polygonContainmentCount << "/"
                  << topology.polygonInteriorOverlapCount << "\n";
        if (!options.topologyOutputPath.empty()) {
            std::cout << "Topology details: " << options.topologyOutputPath << "\n";
        }
        if (!options.previewGridOutputPath.empty()) {
            std::cout << "View grid: " << options.previewGridOutputPath << "\n";
        }
        if (!options.previewPolygonsOutputPath.empty()) {
            std::cout << "View polygons: " << options.previewPolygonsOutputPath << "\n";
        }
        if (!options.weakContinuityOutputPath.empty()) {
            std::cout << "Weak continuity candidates: "
                      << options.weakContinuityOutputPath << "\n";
        }
        std::cout << "Output BOFaultPolygon: " << options.outputName << "\n";
        std::cout << "GeoEast staged commit: "
                  << (commitResult.success() ? "verified" : "failed") << "\n";
        if (!transactionReportPath.empty()) {
            std::cout << "GeoEast commit report: " << transactionReportPath << "\n";
        }
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
