#pragma once

#include "../core/ProductDefaults.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace fpt_ui {

enum class QualityMode {
    Production,
    Diagnostics,
    Experimental
};

struct TraceParameters {
    TraceParameters() {
        fpt::product_defaults::applyVisibleProductionDefaults(*this);
    }

    QString traceMode = "ridge-belt";
    QString thresholdMode = "percentile";
    double threshold = 0.95;
    double percentile = 0.98;
    double minArea = 120.0;
    int closeRadius = 1;
    double weakPercentile = 0.965;
    int weakConnectRadius = 1;
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
    bool weakTailTopologyGate = false;
    bool simpleRingCompatibility = true;
    int simpleRingMaximumFillCells = 25;
    int simpleRingMaximumOpeningCells = 128;
    double simplifyTolerance = 2.0;
    int smoothIterations = 1;
    QualityMode qualityMode = QualityMode::Production;
};

struct TraceParameterValidation {
    QStringList errors;
    QStringList warnings;

    bool isValid() const { return errors.isEmpty(); }
    bool requiresConfirmation() const { return !warnings.isEmpty(); }
};

QStringList presetKeys();
TraceParameters presetParameters(const QString& canonicalPresetName);
QStringList traceArguments(const TraceParameters& parameters);
TraceParameterValidation validateTraceParameters(const TraceParameters& parameters);
QString qualityModeKey(QualityMode mode);
QualityMode qualityModeFromKey(const QString& key);
QJsonObject traceParametersToJson(const TraceParameters& parameters,
                                  const QString& presetKey = QString());
bool traceParametersFromJson(const QJsonObject& object,
                             TraceParameters* parameters,
                             QString* presetKey = nullptr,
                             QString* errorMessage = nullptr);

} // namespace fpt_ui
