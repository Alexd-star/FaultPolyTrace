#pragma once

#include <QMainWindow>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVector>

#include "QualityReviewModel.h"

namespace fpt_ui { class LayerToggle; struct TraceParameters; }

class AttributeView;
class QAction;
class QColor;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QProcessEnvironment;
class QPushButton;
class QSpinBox;
class QSplitter;
class QStackedWidget;
class QTextEdit;
class QToolButton;
class QFrame;
class QProgressBar;

enum class UiLanguage {
    SimplifiedChinese,
    English
};

class FaultPolyTraceWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit FaultPolyTraceWindow(QWidget* parent = nullptr);

private slots:
    void browseInput();
    void browseOutputDir();
    void chooseColor();
    void runTrace();
    void stopTrace();
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void refreshInputPreview();
    void refreshResultPreview();
    void clearResultStateForNewInput();
    void exportPng();
    void showOverlayResult();
    void openResultCsv();
    void openResultGeoJson();
    void openDiagnosticsJson();
    void showGeoEastConfigDialog();
    void refreshGeoEastGrids();
    void refreshAvailableResults();
    void loadSelectedResult();
    void deleteGeoEastResult();
    void saveTemplate();
    void loadTemplate();
    void saveRunLog();
    void copyResultSummary();
    void restoreDefaults();
    void loadHistory();
    void openOutputDir();
    void openHelp();
    void openUserManual();
    void openProductManual();
    void openProjectStructureDoc();
    void updateOptionControls();
    void updateDataSourceMode();
    void updateLayerVisibility();
    void loadQualityReview();
    void updateQualityReviewFilter();
    void showPreviousQualityReview();
    void showNextQualityReview();
    void locateCurrentQualityReview();
    void exportQualityReviewSummary();
    void setLanguage(UiLanguage language);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    enum class ProcessTask {
        None,
        ExternalTrace,
        GeoEastTrace,
        RefreshGeoEastGrids,
        RefreshGeoEastResults,
        ExportGeoEastGrid,
        ExportGeoEastResult,
        DeleteGeoEastResult
    };

    void buildUi();
    void retranslateUi();
    QString uiText(const char* zh) const;
    QString uiText(const QString& zh) const;
    QString languageCode() const;
    void setComboByKey(QComboBox* combo, const QString& key);
    QString comboKey(const QComboBox* combo) const;
    void prepareLanguageMetadata();
    void loadSettings();
    void saveSettings() const;
    void setRunning(bool running);
    void appendLog(const QString& text);
    void updateStatus(const QString& text);
    void setPolygonColor(const QColor& color);
    void syncPresetIndicator();
    void applyPreset(const QString& preset);
    void applyTraceParameters(const fpt_ui::TraceParameters& parameters);
    bool confirmTraceParametersForRun();
    void refreshHistory();
    void appendHistory(qint64 elapsedMs);
    void updateResultPanel();
    void clearQualityReview();
    void refreshQualityReviewSelection(bool locate);
    void updateTaskPreview();
    void loadSelectedResultInternal(bool userInitiated);
    QStringList traceArgs() const;
    fpt_ui::TraceParameters currentTraceParameters() const;
    QStringList geoEastConnectionArgs() const;
    QStringList geoEastTraceArgs() const;
    QString outputCsvPath() const;
    QString outputBasePath() const;
    QString defaultInputPath() const;
    QString defaultOfficialDatPath() const;
    QString defaultOutputDir() const;
    QString coreRunnerPath() const;
    QString geoEastRunnerPath() const;
    QString defaultGeoEastRoot() const;
    QString defaultGeoEastDataRoot() const;
    QString settingsPath() const;
    QString historyPath() const;
    QString locateBundledFile(const QString& relativePath) const;
    bool openBundledDocument(const QString& relativePath, const QString& displayName);
    bool writeGeoJson(const QString& csvPath, QString* geoJsonPath, QString* errorMessage) const;
    bool isGeoEastMode() const;
    void configureGeoEastEnvironment(QProcessEnvironment& environment) const;
    void startProcess(ProcessTask task, const QString& runner, const QStringList& args, const QString& statusText);
    QString geoEastOutputName() const;
    QStringList missingGeoEastConfig(bool requireGrid, bool requireOutput) const;
    bool ensureGeoEastConfigReady(const QString& operation, bool requireGrid, bool requireOutput);
    QStringList localResultCandidates() const;

    QProcess* process_ = nullptr;
    qint64 processStartMs_ = 0;
    ProcessTask currentTask_ = ProcessTask::None;
    QString processOutput_;
    QString lastOutputCsv_;
    QString lastGeoEastOutput_;
    QString loadedGeoEastGrid_;
    QString loadedGeoEastOutput_;
    QString pendingGeoEastGridRestore_;
    QString pendingGeoEastResultRestore_;
    bool refreshResultsAfterGrids_ = false;
    bool cancelRequested_ = false;
    bool userInitiatedLayerLoad_ = false;
    QStringList startupNotices_;

    QComboBox* sourceModeCombo_ = nullptr;
    QStackedWidget* sourceStack_ = nullptr;
    QLineEdit* inputEdit_ = nullptr;
    QLineEdit* outputDirEdit_ = nullptr;
    QLineEdit* outputNameEdit_ = nullptr;
    QComboBox* inputFormatCombo_ = nullptr;
    QComboBox* geoEastGridCombo_ = nullptr;
    QComboBox* resultCatalogCombo_ = nullptr;
    QLineEdit* geoEastRunnerEdit_ = nullptr;
    QLineEdit* geoEastRootEdit_ = nullptr;
    QLineEdit* geoEastDataRootEdit_ = nullptr;
    QLineEdit* pgHostEdit_ = nullptr;
    QLineEdit* pgPortEdit_ = nullptr;
    QLineEdit* databaseEdit_ = nullptr;
    QLineEdit* projectEdit_ = nullptr;
    QLineEdit* userEdit_ = nullptr;
    QLineEdit* passwordEdit_ = nullptr;
    QPushButton* geoEastRefreshButton_ = nullptr;
    QPushButton* geoEastConfigButton_ = nullptr;
    QPushButton* geoEastDeleteButton_ = nullptr;
    QPushButton* refreshResultsButton_ = nullptr;
    QPushButton* loadSelectedResultButton_ = nullptr;
    QComboBox* viewModeCombo_ = nullptr;
    QComboBox* historyCombo_ = nullptr;
    QComboBox* presetCombo_ = nullptr;
    QComboBox* qualityModeCombo_ = nullptr;
    QComboBox* traceModeCombo_ = nullptr;
    QComboBox* thresholdModeCombo_ = nullptr;
    QDoubleSpinBox* percentileSpin_ = nullptr;
    QDoubleSpinBox* thresholdSpin_ = nullptr;
    QDoubleSpinBox* minAreaSpin_ = nullptr;
    QSpinBox* closeRadiusSpin_ = nullptr;
    QDoubleSpinBox* weakPercentileSpin_ = nullptr;
    QSpinBox* weakConnectRadiusSpin_ = nullptr;
    QSpinBox* beltRadiusSpin_ = nullptr;
    QSpinBox* postBeltCloseRadiusSpin_ = nullptr;
    QCheckBox* attributeConstrainedBeltCheck_ = nullptr;
    QSpinBox* attributeEnvelopeRadiusSpin_ = nullptr;
    QSpinBox* minBranchLengthSpin_ = nullptr;
    QSpinBox* minJunctionBranchLengthSpin_ = nullptr;
    QDoubleSpinBox* minJunctionBranchMeanSpin_ = nullptr;
    QSpinBox* maxGapDistanceSpin_ = nullptr;
    QDoubleSpinBox* maxGapAngleSpin_ = nullptr;
    QCheckBox* rootAntiMergeCheck_ = nullptr;
    QSpinBox* rootAntiMergeRadiusSpin_ = nullptr;
    QDoubleSpinBox* rootAntiMergeSupportRatioSpin_ = nullptr;
    QSpinBox* rootAntiMergeMinPartCellsSpin_ = nullptr;
    QDoubleSpinBox* rootAntiMergeMinPartRatioSpin_ = nullptr;
    QDoubleSpinBox* rootAntiMergeWeakGuardThresholdSpin_ = nullptr;
    QSpinBox* rootAntiMergeWeakGuardRadiusSpin_ = nullptr;
    QCheckBox* localAntiMergeCheck_ = nullptr;
    QDoubleSpinBox* localAntiMergeLowThresholdSpin_ = nullptr;
    QSpinBox* localAntiMergeBridgeRadiusSpin_ = nullptr;
    QSpinBox* localAntiMergeMinPartCellsSpin_ = nullptr;
    QDoubleSpinBox* localAntiMergeMinPartRatioSpin_ = nullptr;
    QSpinBox* localAntiMergeMinBridgeCellsSpin_ = nullptr;
    QDoubleSpinBox* localAntiMergeMinLowRatioSpin_ = nullptr;
    QSpinBox* smoothIterationsSpin_ = nullptr;
    QDoubleSpinBox* simplifyToleranceSpin_ = nullptr;
    QCheckBox* residualTraceCheck_ = nullptr;
    QSpinBox* residualTraceMinLengthSpin_ = nullptr;
    QSpinBox* residualTraceMaxCellsSpin_ = nullptr;
    QLineEdit* colorEdit_ = nullptr;
    QProgressBar* busyBar_ = nullptr;
    QDoubleSpinBox* lineWidthSpin_ = nullptr;
    QDoubleSpinBox* alphaSpin_ = nullptr;
    fpt_ui::LayerToggle* showGridLayerCheck_ = nullptr;
    fpt_ui::LayerToggle* showPolygonLayerCheck_ = nullptr;
    fpt_ui::LayerToggle* showQualityReviewLayerCheck_ = nullptr;
    QPushButton* runButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QToolButton* openFilesButton_ = nullptr;
    QAction* openCsvMenuAction_ = nullptr;
    QAction* openGeoJsonMenuAction_ = nullptr;
    QAction* openDiagnosticsMenuAction_ = nullptr;
    QAction* openOutputDirMenuAction_ = nullptr;
    QFrame* doneBanner_ = nullptr;
    QLabel* doneBannerLabel_ = nullptr;
    bool presetCustomized_ = false;
    bool presetSyncGuard_ = false;
    bool presetSyncPending_ = false;
    QCheckBox* verboseLogCheck_ = nullptr;
    QStringList pendingDebugLines_;
    QPushButton* exportPngButton_ = nullptr;
    QAction* runAction_ = nullptr;
    QAction* stopAction_ = nullptr;
    QAction* refreshResultsAction_ = nullptr;
    QAction* languageChineseAction_ = nullptr;
    QAction* languageEnglishAction_ = nullptr;
    QTextEdit* logEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* coordinateLabel_ = nullptr;
    QLabel* elapsedLabel_ = nullptr;
    QLabel* resultSummaryLabel_ = nullptr;
    QLabel* resultPathLabel_ = nullptr;
    QLabel* qualityReviewSummaryLabel_ = nullptr;
    QLabel* qualityReviewDetailLabel_ = nullptr;
    QComboBox* qualityReviewFilterCombo_ = nullptr;
    QComboBox* qualityReviewCombo_ = nullptr;
    QPushButton* qualityReviewPreviousButton_ = nullptr;
    QPushButton* qualityReviewNextButton_ = nullptr;
    QPushButton* qualityReviewLocateButton_ = nullptr;
    QPushButton* qualityReviewExportButton_ = nullptr;
    QLabel* layerTitleLabel_ = nullptr;
    QLabel* layerHintLabel_ = nullptr;
    AttributeView* view_ = nullptr;
    QWidget* colorBarWidget_ = nullptr;
    QSplitter* mainSplitter_ = nullptr;
    fpt_ui::QualityReviewModel qualityReviewModel_;
    QVector<int> filteredQualityReviewIndexes_;
    UiLanguage currentLanguage_ = UiLanguage::SimplifiedChinese;
};
