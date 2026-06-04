#pragma once

#include <QMainWindow>
#include <QProcess>
#include <QString>
#include <QStringList>

class AttributeView;
class QAction;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QProcessEnvironment;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTextEdit;

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
    void exportPng();
    void showOverlayResult();
    void openResultCsv();
    void openResultGeoJson();
    void openDiagnosticsJson();
    void showGeoEastConfigDialog();
    void refreshGeoEastGrids();
    void deleteGeoEastResult();
    void saveTemplate();
    void loadTemplate();
    void restoreDefaults();
    void loadHistory();
    void openOutputDir();
    void openHelp();
    void updateOptionControls();
    void updateDataSourceMode();
    void updateLayerVisibility();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    enum class ProcessTask {
        None,
        ExternalTrace,
        GeoEastTrace,
        RefreshGeoEastGrids,
        DeleteGeoEastResult
    };

    void buildUi();
    void loadSettings();
    void saveSettings() const;
    void setRunning(bool running);
    void appendLog(const QString& text);
    void updateStatus(const QString& text);
    void applyPreset(const QString& preset);
    void refreshHistory();
    void appendHistory(qint64 elapsedMs);
    void updateResultPanel();
    void updateTaskPreview();
    QStringList traceArgs() const;
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
    bool writeGeoJson(const QString& csvPath, QString* geoJsonPath, QString* errorMessage) const;
    bool isGeoEastMode() const;
    void configureGeoEastEnvironment(QProcessEnvironment& environment) const;
    void startProcess(ProcessTask task, const QString& runner, const QStringList& args, const QString& statusText);
    QString geoEastOutputName() const;
    QStringList missingGeoEastConfig(bool requireGrid, bool requireOutput) const;
    bool ensureGeoEastConfigReady(const QString& operation, bool requireGrid, bool requireOutput);

    QProcess* process_ = nullptr;
    qint64 processStartMs_ = 0;
    ProcessTask currentTask_ = ProcessTask::None;
    QString processOutput_;
    QString lastOutputCsv_;
    QString lastGeoEastOutput_;

    QComboBox* sourceModeCombo_ = nullptr;
    QStackedWidget* sourceStack_ = nullptr;
    QLineEdit* inputEdit_ = nullptr;
    QLineEdit* outputDirEdit_ = nullptr;
    QLineEdit* outputNameEdit_ = nullptr;
    QComboBox* inputFormatCombo_ = nullptr;
    QComboBox* geoEastGridCombo_ = nullptr;
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
    QComboBox* viewModeCombo_ = nullptr;
    QComboBox* historyCombo_ = nullptr;
    QComboBox* presetCombo_ = nullptr;
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
    QDoubleSpinBox* lineWidthSpin_ = nullptr;
    QDoubleSpinBox* alphaSpin_ = nullptr;
    QCheckBox* showGridLayerCheck_ = nullptr;
    QCheckBox* showPolygonLayerCheck_ = nullptr;
    QPushButton* runButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QPushButton* showOverlayButton_ = nullptr;
    QPushButton* openCsvButton_ = nullptr;
    QPushButton* openGeoJsonButton_ = nullptr;
    QPushButton* openDiagnosticsButton_ = nullptr;
    QPushButton* openOutputDirButton_ = nullptr;
    QPushButton* exportPngButton_ = nullptr;
    QAction* runAction_ = nullptr;
    QAction* stopAction_ = nullptr;
    QTextEdit* logEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* elapsedLabel_ = nullptr;
    QLabel* resultSummaryLabel_ = nullptr;
    QLabel* resultPathLabel_ = nullptr;
    AttributeView* view_ = nullptr;
};
