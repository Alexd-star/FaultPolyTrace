#include "../core/FaultTracer.h"
#include "../core/Topology.h"
#include "../io/AsciiGridIO.h"
#include "../io/DiagnosticsIO.h"

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

struct CliOptions {
    std::string input;
    std::string output;
    std::string diagnosticsOutput;
    std::string branchesOutput;
    std::string topologyOutput;
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
    double rootAntiMergeSupportRatio = 0.98;
    double rootAntiMergeMaxBridgeValue = 1e-12;
    bool adaptiveAreaFilter = true;
    int adaptiveAreaMinCells = 4;
    double adaptiveAreaMinAspect = 2.0;
    double adaptiveAreaMinMeanRatio = 0.55;
    double adaptiveAreaMinMaxRatio = 0.90;
    bool residualAttributeFill = true;
    bool residualTrace = false;
    double residualPercentile = 0.955;
    int residualFillRadius = 4;
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
        << "  --no-root-anti-merge       Disable root-trace anti-merge guard. Default: on\n"
        << "  --root-anti-merge-radius <n> Neighborhood radius for bridge detection. Default: 1\n"
        << "  --root-anti-merge-support-ratio <v> Attribute support ratio needed to keep bridge cells. Default: 0.98\n"
        << "  --root-anti-merge-max-bridge-value <v> Max raw attribute value treated as background bridge. Default: 1e-12\n"
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

std::string traceModeName(fpt::TraceMode mode) {
    return mode == fpt::TraceMode::RidgeBelt ? "ridge-belt" : "region";
}

std::string thresholdModeName(fpt::ThresholdMode mode) {
    if (mode == fpt::ThresholdMode::PositivePercentile) {
        return "positive-percentile";
    }
    return mode == fpt::ThresholdMode::Percentile ? "percentile" : "fixed";
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
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const CliOptions cli = parseArgs(argc, argv);
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
        traceOptions.adaptiveAreaFilter = cli.adaptiveAreaFilter;
        traceOptions.adaptiveAreaMinCells = cli.adaptiveAreaMinCells;
        traceOptions.adaptiveAreaMinAspect = cli.adaptiveAreaMinAspect;
        traceOptions.adaptiveAreaMinMeanRatio = cli.adaptiveAreaMinMeanRatio;
        traceOptions.adaptiveAreaMinMaxRatio = cli.adaptiveAreaMinMaxRatio;
        traceOptions.residualAttributeFill = cli.residualAttributeFill;
        traceOptions.residualTrace = cli.residualTrace;
        traceOptions.residualPercentile = cli.residualPercentile;
        traceOptions.residualFillRadius = cli.residualFillRadius;
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
        diagnostics.adaptiveAreaFilter = cli.adaptiveAreaFilter;
        diagnostics.adaptiveAreaMinCells = cli.adaptiveAreaMinCells;
        diagnostics.adaptiveAreaMinAspect = cli.adaptiveAreaMinAspect;
        diagnostics.adaptiveAreaMinMeanRatio = cli.adaptiveAreaMinMeanRatio;
        diagnostics.adaptiveAreaMinMaxRatio = cli.adaptiveAreaMinMaxRatio;
        diagnostics.residualAttributeFill = cli.residualAttributeFill;
        diagnostics.residualTrace = cli.residualTrace;
        diagnostics.residualPercentile = cli.residualPercentile;
        diagnostics.residualFillRadius = cli.residualFillRadius;
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
        diagnostics.rootAntiMergeCutCells = result.rootAntiMergeCutCells;
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
        if (cli.traceMode == fpt::TraceMode::RidgeBelt) {
            std::cout << "Skeleton endpoints: " << result.skeletonEndpoints << "\n";
            std::cout << "Skeleton junctions: " << result.skeletonJunctions << "\n";
            std::cout << "Skeleton branches: " << result.skeletonBranches << "\n";
            std::cout << "Connected gaps: " << result.connectedGaps << "\n";
            std::cout << "Kept branches: " << result.keptBranches << "\n";
            std::cout << "Pruned branches: " << result.prunedBranches << "\n";
        }
        std::cout << "Polygons: " << result.polygons.size() << " total, " << kept << " kept\n";
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
