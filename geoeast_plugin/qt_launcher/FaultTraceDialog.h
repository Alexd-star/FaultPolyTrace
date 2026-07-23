#pragma once

#include <QDialog>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace fpt_ui { struct TraceParameters; }

class QCheckBox;
class QColor;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QSplitter;
class QTextEdit;
class QWidget;
class QToolButton;
class QAction;
class QCloseEvent;

enum class UiLanguage {
    SimplifiedChinese,
    English
};

class FaultTraceDialog : public QDialog {
    Q_OBJECT

public:
    explicit FaultTraceDialog(QWidget* parent = nullptr);

private slots:
    void browseRunner();
    void browseDatFile();
    void browseGeoEastRoot();
    void browseGeoEastDataRoot();
    void choosePolygonColor();
    void showDatabaseConfigDialog();
    void saveCurrentPreset();
    void restoreDefaults();
    void showHelpHint();
    void refreshGrids();
    void refreshOutputs();
    void loadSelectedGridView();
    void loadSelectedOutput();
    void deleteOutput();
    void loadResultView();
    void exportResultViewPng();
    void openGeoEastBasemap();
    void openGeoEastMapping();
    void runTrace();
    void stopProcess();
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void updateLayerVisibility();
    void setLanguage(UiLanguage language);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void retranslateUi();
    QString uiText(const char* zh) const;
    QString uiText(const QString& zh) const;
    QString languageCode() const;
    void setComboByKey(QComboBox* combo, const QString& key);
    QString comboKey(const QComboBox* combo) const;
    void prepareLanguageMetadata();
    void setRunning(bool running);
    void appendLog(const QString& text);
    void appendStatus(const QString& text);
    void loadSettings();
    void saveSettings() const;
    void configureGeoEastEnvironment(QProcessEnvironment& environment) const;
    QStringList commonConnectionArgs() const;
    QStringList traceArgs() const;
    fpt_ui::TraceParameters currentTraceParameters() const;
    QString previewOutputDir() const;
    QString previewGridPath() const;
    QString previewPolygonsPath() const;
    bool launchGeoEastApp(const QString& exeName, const QString& displayName);
    void applyParameterPreset(const QString& presetName);
    void setPolygonColor(const QColor& color);
    void updateStatusContext();
    void updateOptionControls();
    void loadSelectedOutputInternal(bool userInitiated);
    QString defaultRunnerPath() const;
    QString defaultOfficialDatPath() const;
    QStringList missingGeoEastConfig(bool requireGrid, bool requireOutput) const;
    bool ensureGeoEastConfigReady(const QString& operation, bool requireGrid, bool requireOutput);

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
    QComboBox* outputCatalogCombo_ = nullptr;
    QLineEdit* outputEdit_ = nullptr;
    QPushButton* polygonColorButton_ = nullptr;
    QLineEdit* polygonColorEdit_ = nullptr;
    QDoubleSpinBox* polygonLineWidthSpin_ = nullptr;
    QCheckBox* importDatCheck_ = nullptr;
    QLineEdit* datFileEdit_ = nullptr;
    QCheckBox* overwriteGridCheck_ = nullptr;
    QComboBox* presetCombo_ = nullptr;
    QComboBox* qualityModeCombo_ = nullptr;
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
    QCheckBox* showInputLayerCheck_ = nullptr;
    QCheckBox* showResultLayerCheck_ = nullptr;
    QPushButton* showOverlayLayerButton_ = nullptr;
    QPushButton* loadResultViewButton_ = nullptr;
    QPushButton* exportViewButton_ = nullptr;
    QPushButton* openBasemapButton_ = nullptr;
    QPushButton* openMappingButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QPushButton* refreshOutputsButton_ = nullptr;
    QPushButton* loadSelectedOutputButton_ = nullptr;
    QPushButton* deleteOutputButton_ = nullptr;
    QAction* loadResultViewAction_ = nullptr;
    QAction* exportViewAction_ = nullptr;
    QAction* openBasemapAction_ = nullptr;
    QAction* openMappingAction_ = nullptr;
    QPushButton* runButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QAction* showInputLayerAction_ = nullptr;
    QAction* showResultLayerAction_ = nullptr;
    QAction* refreshGridAction_ = nullptr;
    QAction* deleteOutputAction_ = nullptr;
    QAction* restoreDefaultsAction_ = nullptr;
    QAction* runTraceAction_ = nullptr;
    QAction* stopTraceAction_ = nullptr;
    QAction* languageChineseAction_ = nullptr;
    QAction* languageEnglishAction_ = nullptr;
    QTextEdit* logEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* coordinateLabel_ = nullptr;
    QLabel* connectionLabel_ = nullptr;
    QLabel* projectStatusLabel_ = nullptr;
    QLabel* gridStatusLabel_ = nullptr;
    QLabel* layerTitleLabel_ = nullptr;
    QLabel* layerHintLabel_ = nullptr;
    QWidget* renderViewWidget_ = nullptr;
    QWidget* colorBarWidget_ = nullptr;
    QSplitter* mainSplitter_ = nullptr;

    QProcess* process_ = nullptr;
    bool refreshingGrids_ = false;
    bool refreshingOutputs_ = false;
    bool refreshOutputsAfterGrids_ = false;
    bool exportingOutputView_ = false;
    bool exportingGridView_ = false;
    bool deletingOutput_ = false;
    UiLanguage currentLanguage_ = UiLanguage::SimplifiedChinese;
};
