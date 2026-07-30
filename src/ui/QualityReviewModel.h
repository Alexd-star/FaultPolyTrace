#pragma once

#include <QRect>
#include <QString>
#include <QVector>

namespace fpt_ui {

struct QualityReviewItem {
    QString id;
    int objectIndex = -1;
    int polygonId = 0;
    QString type;
    QString decision;
    QString rejectionReason;
    QRect gridWindow;
    double supportRatio = 0.0;
    double areaLossRatio = 0.0;
    double strongCoverageLossRatio = 0.0;
    double weakCoverageLossRatio = 0.0;
    double backgroundLeakageDelta = 0.0;
    double stabilityJaccard = 0.0;
};

struct QualityReviewSummary {
    int gridNx = 0;
    int gridNy = 0;
    int polygonCount = 0;
    int keptPolygonCount = 0;
    int detectedCount = 0;
    int acceptedCount = 0;
    int reviewCount = 0;
    int rejectedCount = 0;
    int truncatedCount = 0;
    int openRingCount = 0;
    int selfIntersectionCount = 0;
    int polygonIntersectionCount = 0;
    int elapsedMs = 0;
    bool shadowMode = false;
};

class QualityReviewModel {
public:
    bool loadDiagnostics(const QString& path, QString* errorMessage = nullptr);
    void clear();

    const QVector<QualityReviewItem>& items() const { return items_; }
    const QualityReviewSummary& summary() const { return summary_; }
    QString sourcePath() const { return sourcePath_; }
    QVector<int> filteredIndexes(const QString& filterKey) const;

    static QString typeText(const QString& type, const QString& languageCode);
    static QString decisionText(const QualityReviewItem& item,
                                bool shadowMode,
                                const QString& languageCode);
    static QString reasonText(const QString& reason, const QString& languageCode);

private:
    QVector<QualityReviewItem> items_;
    QualityReviewSummary summary_;
    QString sourcePath_;
};

} // namespace fpt_ui
