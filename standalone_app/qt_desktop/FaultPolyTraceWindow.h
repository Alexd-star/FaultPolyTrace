#pragma once

#include <QMainWindow>
#include <QProcess>
#include <QString>

class AttributeView;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
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
    void saveTemplate();
    void loadTemplate();
    void restoreDefaults();
    void loadHistory();
    void openOutputDir();
    void openHelp();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void loadSettings();
    void saveSettings() const;
    void setRunning(bool running);
    void appendLog(const QString& text);
    void updateStatus(const QString& text);
    void applyPreset(const QString& preset);
    void refreshHistory();
    void appendHistory(qint64 elapsedMs);
    void updateTaskPreview();
    QStringList traceArgs() const;
    QString outputCsvPath() const;
    QString outputBasePath() const;
    QString defaultInputPath() const;
    QString defaultOutputDir() const;
    QString coreRunnerPath() const;
    QString settingsPath() const;
    QString historyPath() const;
    bool writeGeoJson(const QString& csvPath, QString* geoJsonPath, QString* errorMessage) const;

    QProcess* process_ = nullptr;
    qint64 processStartMs_ = 0;
    QString lastOutputCsv_;

    QLineEdit* inputEdit_ = nullptr;
    QLineEdit* outputDirEdit_ = nullptr;
    QLineEdit* outputNameEdit_ = nullptr;
    QComboBox* inputFormatCombo_ = nullptr;
    QComboBox* viewModeCombo_ = nullptr;
    QComboBox* historyCombo_ = nullptr;
    QComboBox* presetCombo_ = nullptr;
    QComboBox* thresholdModeCombo_ = nullptr;
    QDoubleSpinBox* percentileSpin_ = nullptr;
    QDoubleSpinBox* thresholdSpin_ = nullptr;
    QDoubleSpinBox* minAreaSpin_ = nullptr;
    QSpinBox* closeRadiusSpin_ = nullptr;
    QSpinBox* beltRadiusSpin_ = nullptr;
    QSpinBox* maxGapDistanceSpin_ = nullptr;
    QDoubleSpinBox* maxGapAngleSpin_ = nullptr;
    QSpinBox* smoothIterationsSpin_ = nullptr;
    QDoubleSpinBox* simplifyToleranceSpin_ = nullptr;
    QLineEdit* colorEdit_ = nullptr;
    QDoubleSpinBox* lineWidthSpin_ = nullptr;
    QDoubleSpinBox* alphaSpin_ = nullptr;
    QPushButton* runButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QTextEdit* logEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* elapsedLabel_ = nullptr;
    AttributeView* view_ = nullptr;
};
