#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QProcess>
#include <QString>

class QCheckBox;
class QColor;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTextEdit;
class QWidget;
class QAction;

class FaultTraceDialog : public QDialog {
    Q_OBJECT

public:
    explicit FaultTraceDialog(QWidget* parent = nullptr);

private slots:
    void browseRunner();
    void browseCoreRunner();
    void browseInputData();
    void browseDatFile();
    void browseStandaloneInput();
    void browseStandaloneOutputDir();
    void browseGeoEastRoot();
    void browseGeoEastDataRoot();
    void choosePolygonColor();
    void showDatabaseConfigDialog();
    void saveCurrentPreset();
    void saveParameterTemplate();
    void loadParameterTemplate();
    void restoreDefaultConfiguration();
    void showHelpHint();
    void updateRunMode();
    void updatePreviewMode();
    void refreshInputPreview();
    void refreshGrids();
    void runTrace();
    void stopProcess();
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    void buildUi();
    void setRunning(bool running);
    void appendLog(const QString& text);
    void appendStatus(const QString& text);
    void loadSettings();
    void saveSettings() const;
    void configureGeoEastEnvironment(QProcessEnvironment& environment) const;
    QStringList commonConnectionArgs() const;
    QStringList traceArgs() const;
    QStringList standaloneTraceArgs() const;
    QString currentStandaloneOutputCsv() const;
    QString defaultCoreRunnerPath() const;
    void applyParameterPreset(const QString& presetName);
    QJsonObject parameterTemplateJson() const;
    void applyParameterTemplateJson(const QJsonObject& object);
    void setPolygonColor(const QColor& color);
    void updateStatusContext();
    void updateOptionControls();
    void loadLatestStandaloneResult();
    void ensureProductDefaults();
    QString settingsFilePath() const;
    QString defaultRunnerPath() const;
    QString defaultOfficialDatPath() const;
    QString defaultStandaloneInputPath() const;
    QString defaultStandaloneOutputDir() const;

    QComboBox* runModeCombo_ = nullptr;
    QLineEdit* coreRunnerEdit_ = nullptr;
    QLineEdit* runnerEdit_ = nullptr;
    QLineEdit* geoEastRootEdit_ = nullptr;
    QLineEdit* geoEastDataRootEdit_ = nullptr;
    QLineEdit* pgHostEdit_ = nullptr;
    QLineEdit* pgPortEdit_ = nullptr;
    QLineEdit* databaseEdit_ = nullptr;
    QLineEdit* projectEdit_ = nullptr;
    QLineEdit* userEdit_ = nullptr;
    QLineEdit* passwordEdit_ = nullptr;
    QComboBox* gridCombo_ = nullptr;
    QLineEdit* outputEdit_ = nullptr;
    QPushButton* polygonColorButton_ = nullptr;
    QLineEdit* polygonColorEdit_ = nullptr;
    QDoubleSpinBox* polygonLineWidthSpin_ = nullptr;
    QCheckBox* importDatCheck_ = nullptr;
    QLineEdit* datFileEdit_ = nullptr;
    QLineEdit* standaloneInputEdit_ = nullptr;
    QLineEdit* standaloneOutputDirEdit_ = nullptr;
    QLineEdit* standaloneOutputNameEdit_ = nullptr;
    QComboBox* inputFormatCombo_ = nullptr;
    QComboBox* previewModeCombo_ = nullptr;
    QCheckBox* overwriteGridCheck_ = nullptr;
    QComboBox* presetCombo_ = nullptr;
    QComboBox* traceModeCombo_ = nullptr;
    QComboBox* thresholdModeCombo_ = nullptr;
    QDoubleSpinBox* thresholdSpin_ = nullptr;
    QDoubleSpinBox* percentileSpin_ = nullptr;
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
    QDoubleSpinBox* simplifyToleranceSpin_ = nullptr;
    QSpinBox* smoothIterationsSpin_ = nullptr;
    QCheckBox* residualTraceCheck_ = nullptr;
    QSpinBox* residualTraceMinLengthSpin_ = nullptr;
    QSpinBox* residualTraceMaxCellsSpin_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QPushButton* runButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QAction* refreshGridAction_ = nullptr;
    QAction* runTraceAction_ = nullptr;
    QAction* stopTraceAction_ = nullptr;
    QTextEdit* logEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* coordinateLabel_ = nullptr;
    QLabel* connectionLabel_ = nullptr;
    QLabel* projectStatusLabel_ = nullptr;
    QLabel* gridStatusLabel_ = nullptr;
    QLabel* elapsedLabel_ = nullptr;
    QWidget* renderViewWidget_ = nullptr;

    QProcess* process_ = nullptr;
    bool refreshingGrids_ = false;
    bool standaloneRun_ = false;
    qint64 processStartMs_ = 0;
    QString lastStandaloneOutputCsv_;
};
