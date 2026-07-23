#include "TraceParameterModel.h"

namespace fpt_ui {
namespace {

QString number(double value) {
    return QString::number(value, 'g', 12);
}

} // namespace

QStringList presetKeys() {
    return {
        QString::fromUtf8("全覆盖优先"),
        QString::fromUtf8("贴合融合推荐"),
        QString::fromUtf8("防黏连增强"),
        QString::fromUtf8("均衡推荐"),
        QString::fromUtf8("保守高精度"),
        QString::fromUtf8("弱响应高召回"),
        QString::fromUtf8("宽断层带"),
        QString::fromUtf8("细碎断裂")
    };
}

TraceParameters presetParameters(const QString& canonicalPresetName) {
    TraceParameters p;
    const QString key = canonicalPresetName.trimmed();
    if (key == QString::fromUtf8("全覆盖优先")) {
        p.percentile = 0.975;
        p.weakPercentile = 0.955;
        p.minBranchLength = 3;
        p.minJunctionBranchMean = 0.90;
        p.maxGapAngle = 60.0;
        p.residualTrace = true;
        p.residualTraceMinLength = 5;
        p.residualTraceMaxCells = 120;
        p.simplifyTolerance = 1.5;
        p.smoothIterations = 0;
    } else if (key == QString::fromUtf8("弱响应高召回") ||
               key == QString::fromUtf8("高召回")) {
        p.percentile = 0.975;
        p.minArea = 80.0;
        p.maxGapDistance = 14;
        p.residualTrace = true;
        p.residualTraceMinLength = 5;
        p.residualTraceMaxCells = 120;
        p.simplifyTolerance = 1.5;
        p.smoothIterations = 0;
    } else if (key == QString::fromUtf8("保守高精度")) {
        p.percentile = 0.985;
        p.minArea = 250.0;
        p.weakPercentile = 0.0;
        p.weakConnectRadius = 0;
        p.beltRadius = 2;
        p.postBeltCloseRadius = 0;
        p.attributeConstrainedBelt = false;
        p.minBranchLength = 6;
        p.minJunctionBranchLength = 6;
        p.minJunctionBranchMean = 1.0;
        p.maxGapDistance = 8;
        p.maxGapAngle = 45.0;
        p.residualTraceMinLength = 8;
        p.residualTraceMaxCells = 40;
        p.simplifyTolerance = 3.0;
    } else if (key == QString::fromUtf8("宽断层带")) {
        p.percentile = 0.97;
        p.minArea = 300.0;
        p.closeRadius = 2;
        p.weakPercentile = 0.0;
        p.weakConnectRadius = 0;
        p.attributeConstrainedBelt = false;
        p.minBranchLength = 5;
        p.minJunctionBranchLength = 5;
        p.minJunctionBranchMean = 1.0;
        p.maxGapDistance = 14;
        p.maxGapAngle = 60.0;
        p.residualTraceMinLength = 8;
        p.residualTraceMaxCells = 60;
        p.simplifyTolerance = 6.0;
    } else if (key == QString::fromUtf8("细碎断裂")) {
        p.weakPercentile = 0.0;
        p.weakConnectRadius = 0;
        p.beltRadius = 1;
        p.postBeltCloseRadius = 0;
        p.minBranchLength = 3;
        p.minJunctionBranchLength = 3;
        p.minJunctionBranchMean = 1.0;
        p.maxGapDistance = 8;
        p.maxGapAngle = 50.0;
        p.residualTraceMinLength = 4;
        p.simplifyTolerance = 3.0;
    } else if (key == QString::fromUtf8("防黏连增强")) {
        p.rootAntiMerge = true;
    }
    return p;
}

QStringList traceArguments(const TraceParameters& p) {
    QStringList args;
    args << "--trace-mode" << p.traceMode
         << "--threshold-mode" << p.thresholdMode
         << "--threshold" << number(p.threshold)
         << "--percentile" << number(p.percentile)
         << "--min-area" << number(p.minArea)
         << "--close-radius" << QString::number(p.closeRadius)
         << "--weak-percentile" << number(p.weakPercentile)
         << "--weak-connect-radius" << QString::number(p.weakConnectRadius)
         << "--belt-radius" << QString::number(p.beltRadius)
         << "--post-belt-close-radius" << QString::number(p.postBeltCloseRadius)
         << "--attribute-envelope-radius" << QString::number(p.attributeEnvelopeRadius)
         << "--min-branch-length" << QString::number(p.minBranchLength)
         << "--min-junction-branch-length" << QString::number(p.minJunctionBranchLength)
         << "--min-junction-branch-mean" << number(p.minJunctionBranchMean)
         << "--max-gap-distance" << QString::number(p.maxGapDistance)
         << "--max-gap-angle" << number(p.maxGapAngle)
         << "--min-gap-support-ratio" << number(p.minGapSupportRatio)
         << (p.separateBranchBelts ? "--separate-branch-belts" : "--no-separate-branch-belts")
         << (p.attributeConstrainedBelt ? "--attribute-constrained-belt" : "--no-attribute-constrained-belt")
         << (p.rootAntiMerge ? "--root-anti-merge" : "--no-root-anti-merge")
         << "--root-anti-merge-radius" << QString::number(p.rootAntiMergeRadius)
         << "--root-anti-merge-support-ratio" << number(p.rootAntiMergeSupportRatio)
         << "--root-anti-merge-max-bridge-value" << number(p.rootAntiMergeMaxBridgeValue)
         << "--root-anti-merge-min-part-cells" << QString::number(p.rootAntiMergeMinPartCells)
         << "--root-anti-merge-min-part-ratio" << number(p.rootAntiMergeMinPartRatio)
         << "--root-anti-merge-weak-guard-threshold" << number(p.rootAntiMergeWeakGuardThreshold)
         << "--root-anti-merge-weak-guard-radius" << QString::number(p.rootAntiMergeWeakGuardRadius)
         << (p.localAntiMerge ? "--local-anti-merge" : "--no-local-anti-merge")
         << "--local-anti-merge-low-threshold" << number(p.localAntiMergeLowThreshold)
         << "--local-anti-merge-bridge-radius" << QString::number(p.localAntiMergeBridgeRadius)
         << "--local-anti-merge-min-part-cells" << QString::number(p.localAntiMergeMinPartCells)
         << "--local-anti-merge-min-part-ratio" << number(p.localAntiMergeMinPartRatio)
         << "--local-anti-merge-min-bridge-cells" << QString::number(p.localAntiMergeMinBridgeCells)
         << "--local-anti-merge-min-low-ratio" << number(p.localAntiMergeMinLowRatio)
         << (p.residualTrace ? "--residual-trace" : "--no-residual-trace")
         << "--residual-percentile" << number(p.residualPercentile)
         << "--residual-fill-radius" << QString::number(p.residualFillRadius)
         << "--residual-edge-fill-radius" << QString::number(p.residualEdgeFillRadius)
         << "--residual-edge-fill-min-value-ratio" << number(p.residualEdgeFillMinValueRatio)
         << "--residual-edge-fill-max-patch-span" << QString::number(p.residualEdgeFillMaxPatchSpan)
         << "--residual-min-cells" << QString::number(p.residualMinCells)
         << "--residual-trace-min-length" << QString::number(p.residualTraceMinLength)
         << "--residual-trace-max-cells" << QString::number(p.residualTraceMaxCells)
         << "--residual-covered-guard-radius" << QString::number(p.residualCoveredGuardRadius)
         << "--simplify-tolerance" << number(p.simplifyTolerance)
         << "--smooth-iterations" << QString::number(p.smoothIterations);
    switch (p.qualityMode) {
    case QualityMode::Experimental:
        args << "--branch-attribution-shadow"
             << "--weak-path-gate"
             << "--local-polygon-quality";
        break;
    case QualityMode::Diagnostics:
        args << "--branch-attribution-shadow"
             << "--no-weak-path-gate"
             << "--no-local-polygon-quality";
        break;
    case QualityMode::Production:
        args << "--no-branch-attribution-shadow"
             << "--no-weak-path-gate"
             << "--no-local-polygon-quality";
        break;
    }
    return args;
}

QString qualityModeKey(QualityMode mode) {
    switch (mode) {
    case QualityMode::Diagnostics: return "diagnostics";
    case QualityMode::Experimental: return "experimental";
    case QualityMode::Production: return "production";
    }
    return "production";
}

QualityMode qualityModeFromKey(const QString& key) {
    if (key.compare("diagnostics", Qt::CaseInsensitive) == 0) {
        return QualityMode::Diagnostics;
    }
    if (key.compare("experimental", Qt::CaseInsensitive) == 0) {
        return QualityMode::Experimental;
    }
    return QualityMode::Production;
}

} // namespace fpt_ui
