#include "TraceParameterModel.h"

#include <QJsonValue>

#include <cmath>

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
         << "--no-weak-continuity"
         << "--no-weak-continuity-second-pass"
         << "--weak-continuity-final-object-pass"
         << "--weak-tail-completion"
         << "--preserve-observed-skeleton-envelope"
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
             << "--local-polygon-quality-shadow"
             << "--no-local-polygon-quality";
        break;
    case QualityMode::Diagnostics:
        args << "--branch-attribution-shadow"
             << "--no-weak-path-gate"
             << "--local-polygon-quality-shadow"
             << "--no-local-polygon-quality";
        break;
    case QualityMode::Production:
        args << "--no-branch-attribution-shadow"
             << "--no-weak-path-gate"
             << "--no-local-polygon-quality-shadow"
             << "--no-local-polygon-quality";
        break;
    }
    return args;
}

TraceParameterValidation validateTraceParameters(const TraceParameters& p) {
    TraceParameterValidation result;
    const auto finite = [](double value) { return std::isfinite(value); };
    const bool percentileMode = p.thresholdMode == "percentile" ||
        p.thresholdMode == "positive-percentile";

    if (p.traceMode != "ridge-belt" && p.traceMode != "region") {
        result.errors << QString::fromUtf8("追踪模式无效，请重新选择预设或恢复默认参数。");
    }
    if (p.thresholdMode != "percentile" &&
        p.thresholdMode != "positive-percentile" &&
        p.thresholdMode != "fixed") {
        result.errors << QString::fromUtf8("阈值模式无效，请重新选择阈值模式。");
    }
    if (percentileMode && (!finite(p.percentile) || p.percentile <= 0.0 || p.percentile >= 1.0)) {
        result.errors << QString::fromUtf8("分位数模式要求分位数大于 0 且小于 1。分位数 0 不是关闭阈值，而是无效设置。");
    } else if (percentileMode && p.percentile < 0.80) {
        result.errors << QString::fromUtf8("分位数低于 0.80 会让过多背景像元进入骨架追踪。请提高分位数，或改用固定阈值模式并依据属性范围设置阈值。");
    }
    if (!finite(p.threshold)) {
        result.errors << QString::fromUtf8("固定阈值必须是有限数值。");
    }
    if (!finite(p.minArea) || p.minArea <= 0.0) {
        result.errors << QString::fromUtf8("最小面积必须大于 0。设为 0 会保留全部微小碎片，可能造成结果数量和处理负载急剧增加。");
    }
    if (p.beltRadius < 1) {
        result.errors << QString::fromUtf8("带宽半径必须至少为 1 个像元。");
    }
    if (p.minBranchLength < 1) {
        result.errors << QString::fromUtf8("最短分支必须至少为 1 个像元。");
    }
    if (p.residualTrace &&
        (!finite(p.residualPercentile) || p.residualPercentile <= 0.0 || p.residualPercentile >= 1.0)) {
        result.errors << QString::fromUtf8("启用弱响应补追时，补追分位数必须大于 0 且小于 1。");
    }
    if (p.rootAntiMerge && p.localAntiMerge && p.residualTrace) {
        result.errors << QString::fromUtf8("根系防黏连、局部桥分裂和弱响应补追不能同时启用。三者处理目标不同，叠加会显著放大计算量和结果敏感性；请保留推荐的根系防黏连，并只选择一项按需功能进行对比。");
    }

    if (percentileMode && p.percentile > 0.0 && p.percentile < 0.90) {
        result.warnings << QString::fromUtf8("当前分位数低于 0.90，活动像元和骨架分支可能显著增加。建议优先使用推荐预设，仅在明确了解数据分布时继续。");
    }
    if (p.thresholdMode == "fixed" && p.threshold <= 0.0) {
        result.warnings << QString::fromUtf8("固定阈值小于或等于 0，可能把大面积背景纳入追踪。请确认该阈值符合输入属性的数值范围。");
    }
    if (percentileMode && p.weakPercentile > 0.0 && p.weakPercentile >= p.percentile) {
        result.warnings << QString::fromUtf8("弱响应分位不低于主分位数，补充层不会提供更弱的连续性信息，结果可能比预期更少。");
    }
    if (p.residualTrace && percentileMode &&
        p.residualPercentile > 0.0 && p.residualPercentile >= p.percentile) {
        result.warnings << QString::fromUtf8("补追分位数不低于主分位数，二次追踪通常无法补充弱响应断层。");
    }
    if (p.residualTrace && p.residualTraceMaxCells == 0) {
        result.warnings << QString::fromUtf8("补追上限为 0 表示不限制连通块大小，复杂网格上可能明显增加处理时间。");
    }
    if (p.localAntiMerge) {
        result.warnings << QString::fromUtf8("局部桥分裂会把低属性短桥两侧重新分组，分裂后的小对象仍受最小面积过滤，因此结果数量可能减少。仅在确认存在局部黏连时使用。");
    }
    if (p.residualTrace) {
        result.warnings << QString::fromUtf8("弱响应补追会额外检查主结果之外的细长弱响应，可能增加候选数量和处理时间。仅在主结果确有弱响应漏失时使用。");
    }
    if (p.localAntiMerge && p.residualTrace) {
        result.warnings << QString::fromUtf8("局部桥分裂与弱响应补追同时启用时，会先增加弱响应候选再执行局部分裂，处理时间和结果敏感性都会提高。");
    }
    if (p.qualityMode == QualityMode::Experimental) {
        result.warnings << QString::fromUtf8("当前使用受控实验模式：仅将受限弱路径应用于候选结果，分支归属和局部修形只做影子诊断。运行时间和输出对象可能与正式处理模式不同。");
    }
    return result;
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

QJsonObject traceParametersToJson(const TraceParameters& p, const QString& presetKey) {
    QJsonObject object;
    object["format"] = "FaultPolyTrace.TraceParameters";
    object["schemaVersion"] = 1;
    if (!presetKey.trimmed().isEmpty()) {
        object["preset"] = presetKey.trimmed();
    }
    object["qualityMode"] = qualityModeKey(p.qualityMode);
    object["traceMode"] = p.traceMode;
    object["thresholdMode"] = p.thresholdMode;
    object["threshold"] = p.threshold;
    object["percentile"] = p.percentile;
    object["minArea"] = p.minArea;
    object["closeRadius"] = p.closeRadius;
    object["weakPercentile"] = p.weakPercentile;
    object["weakConnectRadius"] = p.weakConnectRadius;
    object["beltRadius"] = p.beltRadius;
    object["postBeltCloseRadius"] = p.postBeltCloseRadius;
    object["attributeConstrainedBelt"] = p.attributeConstrainedBelt;
    object["attributeEnvelopeRadius"] = p.attributeEnvelopeRadius;
    object["minBranchLength"] = p.minBranchLength;
    object["minJunctionBranchLength"] = p.minJunctionBranchLength;
    object["minJunctionBranchMean"] = p.minJunctionBranchMean;
    object["maxGapDistance"] = p.maxGapDistance;
    object["maxGapAngle"] = p.maxGapAngle;
    object["minGapSupportRatio"] = p.minGapSupportRatio;
    object["separateBranchBelts"] = p.separateBranchBelts;
    object["rootAntiMerge"] = p.rootAntiMerge;
    object["rootAntiMergeRadius"] = p.rootAntiMergeRadius;
    object["rootAntiMergeSupportRatio"] = p.rootAntiMergeSupportRatio;
    object["rootAntiMergeMaxBridgeValue"] = p.rootAntiMergeMaxBridgeValue;
    object["rootAntiMergeMinPartCells"] = p.rootAntiMergeMinPartCells;
    object["rootAntiMergeMinPartRatio"] = p.rootAntiMergeMinPartRatio;
    object["rootAntiMergeWeakGuardThreshold"] = p.rootAntiMergeWeakGuardThreshold;
    object["rootAntiMergeWeakGuardRadius"] = p.rootAntiMergeWeakGuardRadius;
    object["localAntiMerge"] = p.localAntiMerge;
    object["localAntiMergeLowThreshold"] = p.localAntiMergeLowThreshold;
    object["localAntiMergeBridgeRadius"] = p.localAntiMergeBridgeRadius;
    object["localAntiMergeMinPartCells"] = p.localAntiMergeMinPartCells;
    object["localAntiMergeMinPartRatio"] = p.localAntiMergeMinPartRatio;
    object["localAntiMergeMinBridgeCells"] = p.localAntiMergeMinBridgeCells;
    object["localAntiMergeMinLowRatio"] = p.localAntiMergeMinLowRatio;
    object["residualTrace"] = p.residualTrace;
    object["residualPercentile"] = p.residualPercentile;
    object["residualFillRadius"] = p.residualFillRadius;
    object["residualEdgeFillRadius"] = p.residualEdgeFillRadius;
    object["residualEdgeFillMinValueRatio"] = p.residualEdgeFillMinValueRatio;
    object["residualEdgeFillMaxPatchSpan"] = p.residualEdgeFillMaxPatchSpan;
    object["residualMinCells"] = p.residualMinCells;
    object["residualTraceMinLength"] = p.residualTraceMinLength;
    object["residualTraceMaxCells"] = p.residualTraceMaxCells;
    object["residualCoveredGuardRadius"] = p.residualCoveredGuardRadius;
    object["simplifyTolerance"] = p.simplifyTolerance;
    object["smoothIterations"] = p.smoothIterations;
    return object;
}

bool traceParametersFromJson(const QJsonObject& object,
                             TraceParameters* parameters,
                             QString* presetKey,
                             QString* errorMessage) {
    auto fail = [&](const QString& message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    };
    if (!parameters || object.isEmpty()) {
        return fail(QString::fromUtf8("参数模板为空或根节点不是 JSON 对象。"));
    }
    if (object.contains("format") && !object.value("format").isString()) {
        return fail(QString::fromUtf8("参数模板格式标识必须是字符串。"));
    }
    const QString format = object.value("format").toString();
    if (!format.isEmpty() && format != "FaultPolyTrace.TraceParameters") {
        return fail(QString::fromUtf8("参数模板格式标识无效。"));
    }
    if (object.contains("schemaVersion") && !object.value("schemaVersion").isDouble()) {
        return fail(QString::fromUtf8("参数模板版本必须是整数。"));
    }
    const double schemaRaw = object.value("schemaVersion").toDouble(1.0);
    if (!std::isfinite(schemaRaw) || std::floor(schemaRaw) != schemaRaw) {
        return fail(QString::fromUtf8("参数模板版本必须是整数。"));
    }
    const int schemaVersion = static_cast<int>(schemaRaw);
    if (schemaVersion < 1 || schemaVersion > 1) {
        return fail(QString::fromUtf8("参数模板版本不受支持：%1。").arg(schemaVersion));
    }

    TraceParameters parsed = *parameters;
    auto readString = [&](const char* key, QString& target, const QStringList& allowed) {
        const QString name = QString::fromLatin1(key);
        if (!object.contains(name)) return true;
        if (!object.value(name).isString()) return false;
        const QString value = object.value(name).toString().trimmed();
        if (!allowed.isEmpty() && !allowed.contains(value)) return false;
        target = value;
        return true;
    };
    auto readDouble = [&](const char* key, double& target, double minimum, double maximum) {
        const QString name = QString::fromLatin1(key);
        if (!object.contains(name)) return true;
        if (!object.value(name).isDouble()) return false;
        const double value = object.value(name).toDouble();
        if (!std::isfinite(value) || value < minimum || value > maximum) return false;
        target = value;
        return true;
    };
    auto readInt = [&](const char* key, int& target, int minimum, int maximum) {
        const QString name = QString::fromLatin1(key);
        if (!object.contains(name) || !object.value(name).isDouble()) return !object.contains(name);
        const double raw = object.value(name).toDouble();
        if (!std::isfinite(raw) || std::floor(raw) != raw || raw < minimum || raw > maximum) return false;
        const int value = static_cast<int>(raw);
        target = value;
        return true;
    };
    auto readBool = [&](const char* key, bool& target) {
        const QString name = QString::fromLatin1(key);
        if (!object.contains(name)) return true;
        if (!object.value(name).isBool()) return false;
        target = object.value(name).toBool();
        return true;
    };
    auto invalid = [&](const char* key) {
        return fail(QString::fromUtf8("参数“%1”的类型或取值范围无效。").arg(QString::fromLatin1(key)));
    };

#define FPT_READ(call, key) do { if (!(call)) return invalid(key); } while (false)
    FPT_READ(readString("traceMode", parsed.traceMode, {"ridge-belt", "region"}), "traceMode");
    FPT_READ(readString("thresholdMode", parsed.thresholdMode, {"percentile", "positive-percentile", "fixed"}), "thresholdMode");
    FPT_READ(readDouble("threshold", parsed.threshold, 0.0, 1.0), "threshold");
    FPT_READ(readDouble("percentile", parsed.percentile, 0.0, 1.0), "percentile");
    FPT_READ(readDouble("minArea", parsed.minArea, 0.0, 1.0e9), "minArea");
    FPT_READ(readInt("closeRadius", parsed.closeRadius, 0, 20), "closeRadius");
    FPT_READ(readDouble("weakPercentile", parsed.weakPercentile, 0.0, 0.9999), "weakPercentile");
    FPT_READ(readInt("weakConnectRadius", parsed.weakConnectRadius, 0, 20), "weakConnectRadius");
    FPT_READ(readInt("beltRadius", parsed.beltRadius, 0, 20), "beltRadius");
    FPT_READ(readInt("postBeltCloseRadius", parsed.postBeltCloseRadius, 0, 20), "postBeltCloseRadius");
    FPT_READ(readBool("attributeConstrainedBelt", parsed.attributeConstrainedBelt), "attributeConstrainedBelt");
    FPT_READ(readInt("attributeEnvelopeRadius", parsed.attributeEnvelopeRadius, 0, 20), "attributeEnvelopeRadius");
    FPT_READ(readInt("minBranchLength", parsed.minBranchLength, 0, 1000), "minBranchLength");
    FPT_READ(readInt("minJunctionBranchLength", parsed.minJunctionBranchLength, 1, 1000), "minJunctionBranchLength");
    FPT_READ(readDouble("minJunctionBranchMean", parsed.minJunctionBranchMean, 0.0, 1.0), "minJunctionBranchMean");
    FPT_READ(readInt("maxGapDistance", parsed.maxGapDistance, 0, 1000), "maxGapDistance");
    FPT_READ(readDouble("maxGapAngle", parsed.maxGapAngle, 0.0, 180.0), "maxGapAngle");
    FPT_READ(readDouble("minGapSupportRatio", parsed.minGapSupportRatio, 0.0, 1.0), "minGapSupportRatio");
    FPT_READ(readBool("separateBranchBelts", parsed.separateBranchBelts), "separateBranchBelts");
    FPT_READ(readBool("rootAntiMerge", parsed.rootAntiMerge), "rootAntiMerge");
    FPT_READ(readInt("rootAntiMergeRadius", parsed.rootAntiMergeRadius, 0, 10), "rootAntiMergeRadius");
    FPT_READ(readDouble("rootAntiMergeSupportRatio", parsed.rootAntiMergeSupportRatio, 0.0, 1.0), "rootAntiMergeSupportRatio");
    FPT_READ(readDouble("rootAntiMergeMaxBridgeValue", parsed.rootAntiMergeMaxBridgeValue, 0.0, 1.0), "rootAntiMergeMaxBridgeValue");
    FPT_READ(readInt("rootAntiMergeMinPartCells", parsed.rootAntiMergeMinPartCells, 0, 1000000), "rootAntiMergeMinPartCells");
    FPT_READ(readDouble("rootAntiMergeMinPartRatio", parsed.rootAntiMergeMinPartRatio, 0.0, 0.5), "rootAntiMergeMinPartRatio");
    FPT_READ(readDouble("rootAntiMergeWeakGuardThreshold", parsed.rootAntiMergeWeakGuardThreshold, 0.0, 1.0), "rootAntiMergeWeakGuardThreshold");
    FPT_READ(readInt("rootAntiMergeWeakGuardRadius", parsed.rootAntiMergeWeakGuardRadius, 0, 20), "rootAntiMergeWeakGuardRadius");
    FPT_READ(readBool("localAntiMerge", parsed.localAntiMerge), "localAntiMerge");
    FPT_READ(readDouble("localAntiMergeLowThreshold", parsed.localAntiMergeLowThreshold, 0.0, 1.0), "localAntiMergeLowThreshold");
    FPT_READ(readInt("localAntiMergeBridgeRadius", parsed.localAntiMergeBridgeRadius, 0, 20), "localAntiMergeBridgeRadius");
    FPT_READ(readInt("localAntiMergeMinPartCells", parsed.localAntiMergeMinPartCells, 1, 1000000), "localAntiMergeMinPartCells");
    FPT_READ(readDouble("localAntiMergeMinPartRatio", parsed.localAntiMergeMinPartRatio, 0.0, 0.5), "localAntiMergeMinPartRatio");
    FPT_READ(readInt("localAntiMergeMinBridgeCells", parsed.localAntiMergeMinBridgeCells, 1, 1000000), "localAntiMergeMinBridgeCells");
    FPT_READ(readDouble("localAntiMergeMinLowRatio", parsed.localAntiMergeMinLowRatio, 0.0, 1.0), "localAntiMergeMinLowRatio");
    FPT_READ(readBool("residualTrace", parsed.residualTrace), "residualTrace");
    FPT_READ(readDouble("residualPercentile", parsed.residualPercentile, 0.0, 1.0), "residualPercentile");
    FPT_READ(readInt("residualFillRadius", parsed.residualFillRadius, 0, 1024), "residualFillRadius");
    FPT_READ(readInt("residualEdgeFillRadius", parsed.residualEdgeFillRadius, 0, 1024), "residualEdgeFillRadius");
    FPT_READ(readDouble("residualEdgeFillMinValueRatio", parsed.residualEdgeFillMinValueRatio, 0.0, 1.0), "residualEdgeFillMinValueRatio");
    FPT_READ(readInt("residualEdgeFillMaxPatchSpan", parsed.residualEdgeFillMaxPatchSpan, 0, 100000000), "residualEdgeFillMaxPatchSpan");
    FPT_READ(readInt("residualMinCells", parsed.residualMinCells, 0, 100000000), "residualMinCells");
    FPT_READ(readInt("residualTraceMinLength", parsed.residualTraceMinLength, 1, 1000), "residualTraceMinLength");
    FPT_READ(readInt("residualTraceMaxCells", parsed.residualTraceMaxCells, 0, 1000000), "residualTraceMaxCells");
    FPT_READ(readInt("residualCoveredGuardRadius", parsed.residualCoveredGuardRadius, 0, 1024), "residualCoveredGuardRadius");
    FPT_READ(readDouble("simplifyTolerance", parsed.simplifyTolerance, 0.0, 1000.0), "simplifyTolerance");
    FPT_READ(readInt("smoothIterations", parsed.smoothIterations, 0, 5), "smoothIterations");
#undef FPT_READ

    if (object.contains("qualityMode")) {
        if (!object.value("qualityMode").isString()) return invalid("qualityMode");
        const QString mode = object.value("qualityMode").toString();
        if (mode != "production" && mode != "diagnostics" && mode != "experimental") return invalid("qualityMode");
        parsed.qualityMode = qualityModeFromKey(mode);
    }
    if (object.contains("preset") && !object.value("preset").isString()) {
        return invalid("preset");
    }
    if (presetKey) {
        *presetKey = object.value("preset").toString().trimmed();
    }
    const TraceParameterValidation validation = validateTraceParameters(parsed);
    if (!validation.isValid()) {
        return fail(validation.errors.join(QString::fromUtf8("\n")));
    }
    *parameters = parsed;
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

} // namespace fpt_ui
