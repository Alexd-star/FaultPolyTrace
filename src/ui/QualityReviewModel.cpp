#include "QualityReviewModel.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QHash>

#include <cmath>
#include <algorithm>
#include <limits>

namespace fpt_ui {
namespace {

constexpr qint64 kMaxDiagnosticsBytes = 32 * 1024 * 1024;
constexpr int kMaxReviewItems = 10000;

bool finiteNumber(const QJsonValue& value, double* result) {
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number)) {
        return false;
    }
    if (result) {
        *result = number;
    }
    return true;
}

int integerValue(const QJsonObject& object, const char* key, int fallback = 0) {
    const QJsonValue value = object.value(QLatin1String(key));
    double number = 0.0;
    if (!finiteNumber(value, &number) || std::floor(number) != number ||
        number < static_cast<double>(std::numeric_limits<int>::min()) ||
        number > static_cast<double>(std::numeric_limits<int>::max())) {
        return fallback;
    }
    return static_cast<int>(number);
}

double numberValue(const QJsonObject& object, const char* key, double fallback = 0.0) {
    double number = 0.0;
    return finiteNumber(object.value(QLatin1String(key)), &number) ? number : fallback;
}

QString zhOrEn(const QString& languageCode, const char* zh, const char* en) {
    return languageCode.startsWith("en", Qt::CaseInsensitive)
        ? QString::fromLatin1(en)
        : QString::fromUtf8(zh);
}

} // namespace

bool QualityReviewModel::loadDiagnostics(const QString& path, QString* errorMessage) {
    clear();
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("诊断文件不存在：%1").arg(path);
        }
        return false;
    }
    if (info.size() <= 0 || info.size() > kMaxDiagnosticsBytes) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("诊断文件大小异常，已拒绝载入。");
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("无法读取诊断文件：%1").arg(path);
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("诊断 JSON 无效：%1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    const QString format = root.value("format").toString();
    if (!format.isEmpty() && format != "FaultPolyTrace.RunDiagnostics") {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("不是 FaultPolyTrace 运行诊断文件。");
        }
        return false;
    }
    const int schemaVersion = root.contains("schemaVersion")
        ? integerValue(root, "schemaVersion", -1)
        : 1;
    if (schemaVersion < 1 || schemaVersion > 2) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("不支持的诊断文件版本：%1").arg(schemaVersion);
        }
        return false;
    }

    const QJsonObject parameters = root.value("parameters").toObject();
    const QJsonObject grid = root.value("grid").toObject();
    const QJsonObject results = root.value("results").toObject();
    const QJsonObject local = root.value("localPolygonQuality").toObject();
    const QJsonObject topology = root.value("topology").toObject();
    const QJsonObject timing = root.value("timing").toObject();

    summary_.gridNx = integerValue(grid, "nx");
    summary_.gridNy = integerValue(grid, "ny");
    summary_.polygonCount = integerValue(results, "polygonCount");
    summary_.keptPolygonCount = integerValue(results, "keptPolygonCount");
    summary_.detectedCount = integerValue(local, "detected");
    summary_.acceptedCount = integerValue(local, "accepted");
    summary_.reviewCount = integerValue(local, "review");
    summary_.rejectedCount = integerValue(local, "rejected");
    summary_.truncatedCount = integerValue(local, "truncatedEvidence");
    summary_.openRingCount = integerValue(topology, "openRings");
    summary_.selfIntersectionCount = integerValue(topology, "selfIntersectingPolygons");
    summary_.polygonIntersectionCount = integerValue(topology, "polygonIntersections");
    summary_.elapsedMs = integerValue(timing, "elapsedMs");
    summary_.shadowMode = parameters.value("localPolygonQualityShadow").toBool(false);

    const QJsonArray evidence = local.value("candidateEvidence").toArray();
    if (evidence.size() > kMaxReviewItems) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("复核候选超过安全上限（%1）。").arg(kMaxReviewItems);
        }
        clear();
        return false;
    }
    items_.reserve(evidence.size());
    QHash<int, int> lowSupportBridgeByObject;
    for (int index = 0; index < evidence.size(); ++index) {
        const QJsonObject object = evidence.at(index).toObject();
        const QJsonObject window = object.value("window").toObject();
        const int minX = integerValue(window, "minGridX", -1);
        const int maxX = integerValue(window, "maxGridX", -1);
        const int minY = integerValue(window, "minGridY", -1);
        const int maxY = integerValue(window, "maxGridY", -1);
        if (minX < 0 || minY < 0 || maxX < minX || maxY < minY ||
            (summary_.gridNx > 0 && maxX >= summary_.gridNx) ||
            (summary_.gridNy > 0 && maxY >= summary_.gridNy)) {
            continue;
        }
        QualityReviewItem item;
        item.objectIndex = integerValue(object, "objectIndex", -1);
        item.polygonId = integerValue(object, "polygonId", 0);
        item.type = object.value("type").toString();
        item.decision = object.value("decision").toString();
        item.rejectionReason = object.value("rejectionReason").toString();
        item.gridWindow = QRect(QPoint(minX, minY), QPoint(maxX, maxY));
        item.supportRatio = numberValue(object, "supportRatio");
        item.areaLossRatio = numberValue(object, "areaLossRatio");
        item.strongCoverageLossRatio = numberValue(object, "strongCoverageLossRatio");
        item.weakCoverageLossRatio = numberValue(object, "weakCoverageLossRatio");
        item.backgroundLeakageDelta = numberValue(object, "backgroundLeakageDelta");
        item.stabilityJaccard = numberValue(object, "stabilityJaccard");
        if (item.type == "low_support_bridge" && item.objectIndex >= 0) {
            const auto existing = lowSupportBridgeByObject.constFind(item.objectIndex);
            if (existing != lowSupportBridgeByObject.cend()) {
                QualityReviewItem& aggregate = items_[existing.value()];
                aggregate.gridWindow = aggregate.gridWindow.united(item.gridWindow);
                aggregate.supportRatio = std::max(aggregate.supportRatio, item.supportRatio);
                aggregate.areaLossRatio = std::max(aggregate.areaLossRatio, item.areaLossRatio);
                aggregate.stabilityJaccard = std::min(aggregate.stabilityJaccard, item.stabilityJaccard);
                continue;
            }
            lowSupportBridgeByObject.insert(item.objectIndex, items_.size());
        }
        items_.push_back(item);
    }
    for (int index = 0; index < items_.size(); ++index) {
        items_[index].id = QStringLiteral("LQ-%1").arg(index + 1, 3, 10, QLatin1Char('0'));
    }
    sourcePath_ = info.absoluteFilePath();
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

void QualityReviewModel::clear() {
    items_.clear();
    summary_ = QualityReviewSummary{};
    sourcePath_.clear();
}

QVector<int> QualityReviewModel::filteredIndexes(const QString& filterKey) const {
    QVector<int> result;
    result.reserve(items_.size());
    for (int index = 0; index < items_.size(); ++index) {
        const QString decision = items_.at(index).decision;
        if (filterKey == "review" && decision != "review") continue;
        if (filterKey == "accepted" && decision != "accepted") continue;
        if (filterKey == "rejected" && decision != "rejected") continue;
        result.push_back(index);
    }
    return result;
}

QString QualityReviewModel::typeText(const QString& type, const QString& languageCode) {
    if (type == "local_bulge") return zhOrEn(languageCode, "局部鼓包", "Local bulge");
    if (type == "low_support_bridge") return zhOrEn(languageCode, "低支撑短桥", "Low-support bridge");
    if (type == "legal_wide_belt") return zhOrEn(languageCode, "合法宽断层带", "Valid wide belt");
    if (type == "natural_end_cap") return zhOrEn(languageCode, "自然端帽", "Natural end cap");
    if (type == "object_budget_exceeded") return zhOrEn(languageCode, "对象预算超限", "Object budget exceeded");
    if (type == "invalid_geometry") return zhOrEn(languageCode, "几何异常", "Invalid geometry");
    return type.isEmpty() ? zhOrEn(languageCode, "未分类候选", "Unclassified candidate") : type;
}

QString QualityReviewModel::decisionText(const QualityReviewItem& item,
                                         bool shadowMode,
                                         const QString& languageCode) {
    if (item.decision == "accepted") {
        return shadowMode
            ? zhOrEn(languageCode, "可晋级候选（未应用）", "Eligible candidate (not applied)")
            : zhOrEn(languageCode, "已通过质量门控", "Accepted by quality gates");
    }
    if (item.decision == "review") return zhOrEn(languageCode, "需要人工复核", "Manual review required");
    if (item.decision == "rejected") return zhOrEn(languageCode, "未通过质量门控", "Rejected by quality gates");
    return item.decision;
}

QString QualityReviewModel::reasonText(const QString& reason, const QString& languageCode) {
    if (reason.isEmpty() || reason == "none") return zhOrEn(languageCode, "无", "None");
    if (reason == "insufficient_evidence") return zhOrEn(languageCode, "属性与形态支撑不足", "Insufficient evidence");
    if (reason == "no_meaningful_change") return zhOrEn(languageCode, "修形变化不显著", "No meaningful change");
    if (reason == "excessive_area_loss") return zhOrEn(languageCode, "面积损失超限", "Excessive area loss");
    if (reason == "strong_coverage_loss") return zhOrEn(languageCode, "强响应覆盖下降", "Strong-response coverage loss");
    if (reason == "weak_coverage_loss") return zhOrEn(languageCode, "弱响应覆盖下降", "Weak-response coverage loss");
    if (reason == "background_leakage") return zhOrEn(languageCode, "背景泄漏增加", "Background leakage increase");
    if (reason == "topology_risk") return zhOrEn(languageCode, "拓扑风险", "Topology risk");
    if (reason == "stability_risk") return zhOrEn(languageCode, "参数扰动稳定性不足", "Stability risk");
    if (reason == "invalid_geometry") return zhOrEn(languageCode, "几何无效", "Invalid geometry");
    return reason;
}

} // namespace fpt_ui
