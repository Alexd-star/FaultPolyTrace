#include "FaultTraceDialog.h"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QString repoRootFromAppDir() {
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 5; ++i) {
        if (dir.exists("geoeast_plugin") && dir.exists("src")) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QDir::currentPath();
}

QString envOrDefault(const char* name, const QString& fallback) {
    const QByteArray value = qgetenv(name);
    return value.isEmpty() ? fallback : QString::fromLocal8Bit(value);
}

QString quoteForLog(const QStringList& args) {
    QStringList quoted;
    for (const QString& arg : args) {
        quoted << (arg.contains(' ') ? QString("\"%1\"").arg(arg) : arg);
    }
    return quoted.join(' ');
}

} // namespace

FaultTraceDialog::FaultTraceDialog(QWidget* parent) : QDialog(parent), process_(new QProcess(this)) {
    buildUi();
    loadSettings();

    connect(process_, &QProcess::readyReadStandardOutput, this, &FaultTraceDialog::onReadyRead);
    connect(process_, &QProcess::readyReadStandardError, this, &FaultTraceDialog::onReadyRead);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FaultTraceDialog::onProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, &FaultTraceDialog::onProcessError);
}

void FaultTraceDialog::buildUi() {
    setWindowTitle(QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪"));
    resize(980, 720);

    runnerEdit_ = new QLineEdit(defaultRunnerPath(), this);
    QPushButton* browseRunnerButton = new QPushButton(QString::fromUtf8("浏览"), this);
    connect(browseRunnerButton, &QPushButton::clicked, this, &FaultTraceDialog::browseRunner);

    geoEastRootEdit_ = new QLineEdit(envOrDefault("FPT_GEOEAST_ROOT", "C:/Users/26312/GeoEast"), this);
    geoEastDataRootEdit_ = new QLineEdit(envOrDefault(
        "FPT_GEOEAST_DATA_ROOT",
        "D:/BaiduNetdiskDownload/BGPCup2025/GeoEast-RC/GeoEastData.DataModel+SQLITE.20230728/GeoEastData"), this);
    pgHostEdit_ = new QLineEdit(envOrDefault("FPT_GEOEAST_PG_HOST", "127.0.0.1"), this);
    pgPortEdit_ = new QLineEdit(envOrDefault("FPT_GEOEAST_PG_PORT", "5433"), this);
    QPushButton* browseGeoEastRootButton = new QPushButton(QString::fromUtf8("浏览"), this);
    QPushButton* browseGeoEastDataRootButton = new QPushButton(QString::fromUtf8("浏览"), this);
    connect(browseGeoEastRootButton, &QPushButton::clicked, this, &FaultTraceDialog::browseGeoEastRoot);
    connect(browseGeoEastDataRootButton, &QPushButton::clicked, this, &FaultTraceDialog::browseGeoEastDataRoot);

    QHBoxLayout* runnerLayout = new QHBoxLayout;
    runnerLayout->addWidget(runnerEdit_);
    runnerLayout->addWidget(browseRunnerButton);

    QHBoxLayout* geoEastRootLayout = new QHBoxLayout;
    geoEastRootLayout->addWidget(geoEastRootEdit_);
    geoEastRootLayout->addWidget(browseGeoEastRootButton);

    QHBoxLayout* geoEastDataRootLayout = new QHBoxLayout;
    geoEastDataRootLayout->addWidget(geoEastDataRootEdit_);
    geoEastDataRootLayout->addWidget(browseGeoEastDataRootButton);

    QHBoxLayout* pgLayout = new QHBoxLayout;
    pgLayout->addWidget(pgHostEdit_);
    pgLayout->addWidget(pgPortEdit_);

    QGroupBox* connectionBox = new QGroupBox(QString::fromUtf8("GeoEast 数据库"), this);
    databaseEdit_ = new QLineEdit(envOrDefault("FPT_GEOEAST_DB", "ndp"), this);
    projectEdit_ = new QLineEdit(envOrDefault("FPT_GEOEAST_PROJECT", "BGPCup2026_Test7"), this);
    userEdit_ = new QLineEdit(envOrDefault("FPT_GEOEAST_USER", "26312"), this);
    passwordEdit_ = new QLineEdit("", this);
    passwordEdit_->setEchoMode(QLineEdit::Password);

    QFormLayout* connectionLayout = new QFormLayout(connectionBox);
    connectionLayout->addRow(QString::fromUtf8("Runner"), runnerLayout);
    connectionLayout->addRow(QString::fromUtf8("GeoEast 根目录"), geoEastRootLayout);
    connectionLayout->addRow(QString::fromUtf8("GeoEast 数据根目录"), geoEastDataRootLayout);
    connectionLayout->addRow(QString::fromUtf8("PG 主机/端口"), pgLayout);
    connectionLayout->addRow(QString::fromUtf8("数据库"), databaseEdit_);
    connectionLayout->addRow(QString::fromUtf8("项目"), projectEdit_);
    connectionLayout->addRow(QString::fromUtf8("用户"), userEdit_);
    connectionLayout->addRow(QString::fromUtf8("密码"), passwordEdit_);

    QGroupBox* dataBox = new QGroupBox(QString::fromUtf8("输入输出"), this);
    gridCombo_ = new QComboBox(this);
    gridCombo_->setEditable(true);
    gridCombo_->addItem("test7_fault_attribute_bo");
    gridCombo_->addItem("test7_fault_attribute");
    gridCombo_->addItem("test");
    gridCombo_->setCurrentText(envOrDefault("FPT_GEOEAST_GRID", "test7_fault_attribute_bo"));
    outputEdit_ = new QLineEdit(envOrDefault("FPT_GEOEAST_OUTPUT", "FaultPolyTrace_Result"), this);
    polygonColorEdit_ = new QLineEdit(envOrDefault("FPT_POLYGON_COLOR", "#00FFFF"), this);
    polygonColorEdit_->setReadOnly(true);
    polygonColorButton_ = new QPushButton(QString::fromUtf8("选择颜色"), this);
    polygonLineWidthSpin_ = new QDoubleSpinBox(this);
    polygonLineWidthSpin_->setRange(0.01, 5.0);
    polygonLineWidthSpin_->setDecimals(2);
    polygonLineWidthSpin_->setSingleStep(0.005);
    polygonLineWidthSpin_->setValue(0.025);
    connect(polygonColorButton_, &QPushButton::clicked, this, &FaultTraceDialog::choosePolygonColor);
    setPolygonColor(QColor(polygonColorEdit_->text()));
    refreshButton_ = new QPushButton(QString::fromUtf8("刷新网格"), this);
    connect(refreshButton_, &QPushButton::clicked, this, &FaultTraceDialog::refreshGrids);

    importDatCheck_ = new QCheckBox(QString::fromUtf8("从官方 DAT 导入/覆盖输入网格"), this);
    importDatCheck_->setChecked(false);
    datFileEdit_ = new QLineEdit(defaultOfficialDatPath(), this);
    QPushButton* browseDatButton = new QPushButton(QString::fromUtf8("浏览"), this);
    overwriteGridCheck_ = new QCheckBox(QString::fromUtf8("覆盖同名 BOMapGrid"), this);
    overwriteGridCheck_->setChecked(true);
    connect(browseDatButton, &QPushButton::clicked, this, &FaultTraceDialog::browseDatFile);

    QHBoxLayout* gridLayout = new QHBoxLayout;
    gridLayout->addWidget(gridCombo_);
    gridLayout->addWidget(refreshButton_);

    QHBoxLayout* datLayout = new QHBoxLayout;
    datLayout->addWidget(datFileEdit_);
    datLayout->addWidget(browseDatButton);

    QFormLayout* dataLayout = new QFormLayout(dataBox);
    dataLayout->addRow(QString::fromUtf8("输入网格"), gridLayout);
    dataLayout->addRow(QString::fromUtf8("输出多边形"), outputEdit_);
    QHBoxLayout* styleLayout = new QHBoxLayout;
    styleLayout->addWidget(polygonColorEdit_);
    styleLayout->addWidget(polygonColorButton_);
    styleLayout->addWidget(new QLabel(QString::fromUtf8("线宽"), this));
    styleLayout->addWidget(polygonLineWidthSpin_);
    dataLayout->addRow(QString::fromUtf8("结果样式"), styleLayout);
    dataLayout->addRow(importDatCheck_);
    dataLayout->addRow(QString::fromUtf8("DAT 文件"), datLayout);
    dataLayout->addRow(overwriteGridCheck_);

    QGroupBox* paramsBox = new QGroupBox(QString::fromUtf8("追踪参数"), this);
    presetCombo_ = new QComboBox(this);
    presetCombo_->addItem(QString::fromUtf8("全覆盖优先"));
    presetCombo_->addItem(QString::fromUtf8("贴合融合推荐"));
    presetCombo_->addItem(QString::fromUtf8("均衡推荐"));
    presetCombo_->addItem(QString::fromUtf8("保守高精度"));
    presetCombo_->addItem(QString::fromUtf8("弱响应高召回"));
    presetCombo_->addItem(QString::fromUtf8("宽断层带"));
    presetCombo_->addItem(QString::fromUtf8("细碎断裂"));

    traceModeCombo_ = new QComboBox(this);
    traceModeCombo_->addItem("ridge-belt");
    traceModeCombo_->addItem("region");
    thresholdModeCombo_ = new QComboBox(this);
    thresholdModeCombo_->addItem("percentile");
    thresholdModeCombo_->addItem("positive-percentile");
    thresholdModeCombo_->addItem("fixed");

    thresholdSpin_ = new QDoubleSpinBox(this);
    thresholdSpin_->setRange(0.0, 1.0);
    thresholdSpin_->setDecimals(4);
    thresholdSpin_->setSingleStep(0.01);
    thresholdSpin_->setValue(0.95);

    percentileSpin_ = new QDoubleSpinBox(this);
    percentileSpin_->setRange(0.0, 1.0);
    percentileSpin_->setDecimals(4);
    percentileSpin_->setSingleStep(0.005);
    percentileSpin_->setValue(0.98);

    minAreaSpin_ = new QDoubleSpinBox(this);
    minAreaSpin_->setRange(0.0, 1000000000.0);
    minAreaSpin_->setDecimals(2);
    minAreaSpin_->setSingleStep(50.0);
    minAreaSpin_->setValue(200.0);

    closeRadiusSpin_ = new QSpinBox(this);
    closeRadiusSpin_->setRange(0, 20);
    closeRadiusSpin_->setValue(1);
    weakPercentileSpin_ = new QDoubleSpinBox(this);
    weakPercentileSpin_->setRange(0.0, 0.9999);
    weakPercentileSpin_->setDecimals(4);
    weakPercentileSpin_->setSingleStep(0.005);
    weakPercentileSpin_->setValue(0.0);
    weakConnectRadiusSpin_ = new QSpinBox(this);
    weakConnectRadiusSpin_->setRange(0, 20);
    weakConnectRadiusSpin_->setValue(0);
    beltRadiusSpin_ = new QSpinBox(this);
    beltRadiusSpin_->setRange(0, 20);
    beltRadiusSpin_->setValue(2);
    postBeltCloseRadiusSpin_ = new QSpinBox(this);
    postBeltCloseRadiusSpin_->setRange(0, 20);
    postBeltCloseRadiusSpin_->setValue(1);
    attributeConstrainedBeltCheck_ = new QCheckBox(QString::fromUtf8("属性约束断层带"), this);
    attributeConstrainedBeltCheck_->setChecked(true);
    attributeEnvelopeRadiusSpin_ = new QSpinBox(this);
    attributeEnvelopeRadiusSpin_->setRange(0, 20);
    attributeEnvelopeRadiusSpin_->setValue(1);
    minBranchLengthSpin_ = new QSpinBox(this);
    minBranchLengthSpin_->setRange(0, 1000);
    minBranchLengthSpin_->setValue(4);
    minJunctionBranchLengthSpin_ = new QSpinBox(this);
    minJunctionBranchLengthSpin_->setRange(1, 1000);
    minJunctionBranchLengthSpin_->setValue(4);
    minJunctionBranchMeanSpin_ = new QDoubleSpinBox(this);
    minJunctionBranchMeanSpin_->setRange(0.0, 1.0);
    minJunctionBranchMeanSpin_->setDecimals(4);
    minJunctionBranchMeanSpin_->setSingleStep(0.01);
    minJunctionBranchMeanSpin_->setValue(1.0);
    maxGapDistanceSpin_ = new QSpinBox(this);
    maxGapDistanceSpin_->setRange(0, 1000);
    maxGapDistanceSpin_->setValue(12);
    maxGapAngleSpin_ = new QDoubleSpinBox(this);
    maxGapAngleSpin_->setRange(0.0, 180.0);
    maxGapAngleSpin_->setDecimals(1);
    maxGapAngleSpin_->setValue(55.0);
    rootAntiMergeCheck_ = new QCheckBox(QString::fromUtf8("根系防黏连"), this);
    rootAntiMergeCheck_->setChecked(false);
    rootAntiMergeRadiusSpin_ = new QSpinBox(this);
    rootAntiMergeRadiusSpin_->setRange(0, 10);
    rootAntiMergeRadiusSpin_->setValue(1);
    rootAntiMergeSupportRatioSpin_ = new QDoubleSpinBox(this);
    rootAntiMergeSupportRatioSpin_->setRange(0.0, 1.0);
    rootAntiMergeSupportRatioSpin_->setDecimals(3);
    rootAntiMergeSupportRatioSpin_->setSingleStep(0.01);
    rootAntiMergeSupportRatioSpin_->setValue(0.98);
    simplifyToleranceSpin_ = new QDoubleSpinBox(this);
    simplifyToleranceSpin_->setRange(0.0, 1000.0);
    simplifyToleranceSpin_->setDecimals(2);
    simplifyToleranceSpin_->setSingleStep(1.0);
    simplifyToleranceSpin_->setValue(5.0);
    smoothIterationsSpin_ = new QSpinBox(this);
    smoothIterationsSpin_->setRange(0, 5);
    smoothIterationsSpin_->setValue(1);
    residualTraceMinLengthSpin_ = new QSpinBox(this);
    residualTraceMinLengthSpin_->setRange(1, 1000);
    residualTraceMinLengthSpin_->setValue(6);
    residualTraceMaxCellsSpin_ = new QSpinBox(this);
    residualTraceMaxCellsSpin_->setRange(0, 1000000);
    residualTraceMaxCellsSpin_->setValue(80);
    connect(presetCombo_, &QComboBox::currentTextChanged, this, &FaultTraceDialog::applyParameterPreset);

    QGridLayout* paramsLayout = new QGridLayout(paramsBox);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("参数预设"), this), 0, 0);
    paramsLayout->addWidget(presetCombo_, 0, 1);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("追踪模式"), this), 0, 2);
    paramsLayout->addWidget(traceModeCombo_, 0, 3);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("阈值模式"), this), 1, 0);
    paramsLayout->addWidget(thresholdModeCombo_, 1, 1);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("固定阈值"), this), 1, 2);
    paramsLayout->addWidget(thresholdSpin_, 1, 3);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("分位数"), this), 2, 0);
    paramsLayout->addWidget(percentileSpin_, 2, 1);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("最小面积"), this), 2, 2);
    paramsLayout->addWidget(minAreaSpin_, 2, 3);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("闭运算半径"), this), 3, 0);
    paramsLayout->addWidget(closeRadiusSpin_, 3, 1);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("弱延拓分位"), this), 3, 2);
    paramsLayout->addWidget(weakPercentileSpin_, 3, 3);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("弱延拓半径"), this), 4, 0);
    paramsLayout->addWidget(weakConnectRadiusSpin_, 4, 1);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("带宽半径"), this), 4, 2);
    paramsLayout->addWidget(beltRadiusSpin_, 4, 3);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("带后闭合"), this), 5, 0);
    paramsLayout->addWidget(postBeltCloseRadiusSpin_, 5, 1);
    paramsLayout->addWidget(attributeConstrainedBeltCheck_, 5, 2);
    paramsLayout->addWidget(attributeEnvelopeRadiusSpin_, 5, 3);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("最短分支"), this), 6, 0);
    paramsLayout->addWidget(minBranchLengthSpin_, 6, 1);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("交切短枝"), this), 6, 2);
    paramsLayout->addWidget(minJunctionBranchLengthSpin_, 6, 3);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("短枝均值"), this), 7, 0);
    paramsLayout->addWidget(minJunctionBranchMeanSpin_, 7, 1);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("最大断距"), this), 7, 2);
    paramsLayout->addWidget(maxGapDistanceSpin_, 7, 3);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("最大夹角"), this), 8, 0);
    paramsLayout->addWidget(maxGapAngleSpin_, 8, 1);
    paramsLayout->addWidget(rootAntiMergeCheck_, 8, 2);
    paramsLayout->addWidget(rootAntiMergeRadiusSpin_, 8, 3);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("防黏连支撑"), this), 9, 0);
    paramsLayout->addWidget(rootAntiMergeSupportRatioSpin_, 9, 1);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("简化容差"), this), 9, 2);
    paramsLayout->addWidget(simplifyToleranceSpin_, 9, 3);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("平滑次数"), this), 10, 0);
    paramsLayout->addWidget(smoothIterationsSpin_, 10, 1);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("补追最短"), this), 10, 2);
    paramsLayout->addWidget(residualTraceMinLengthSpin_, 10, 3);
    paramsLayout->addWidget(new QLabel(QString::fromUtf8("补追上限"), this), 11, 0);
    paramsLayout->addWidget(residualTraceMaxCellsSpin_, 11, 1);

    runButton_ = new QPushButton(QString::fromUtf8("开始追踪"), this);
    stopButton_ = new QPushButton(QString::fromUtf8("停止"), this);
    stopButton_->setEnabled(false);
    connect(runButton_, &QPushButton::clicked, this, &FaultTraceDialog::runTrace);
    connect(stopButton_, &QPushButton::clicked, this, &FaultTraceDialog::stopProcess);

    QHBoxLayout* buttonsLayout = new QHBoxLayout;
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(runButton_);
    buttonsLayout->addWidget(stopButton_);

    logEdit_ = new QTextEdit(this);
    logEdit_->setReadOnly(true);
    logEdit_->setLineWrapMode(QTextEdit::NoWrap);
    statusLabel_ = new QLabel(QString::fromUtf8("就绪"), this);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(connectionBox);
    mainLayout->addWidget(dataBox);
    mainLayout->addWidget(paramsBox);
    mainLayout->addLayout(buttonsLayout);
    mainLayout->addWidget(logEdit_, 1);
    mainLayout->addWidget(statusLabel_);

    presetCombo_->setCurrentText(QString::fromUtf8("贴合融合推荐"));
    applyParameterPreset(QString::fromUtf8("贴合融合推荐"));
}

QString FaultTraceDialog::defaultRunnerPath() const {
    const QString fromEnv = envOrDefault("FPT_GEOEAST_RUNNER", "");
    if (!fromEnv.isEmpty()) {
        return QDir::toNativeSeparators(fromEnv);
    }
    QDir root(repoRootFromAppDir());
    const QString candidate = root.absoluteFilePath("geoeast_plugin/bo_runner/faultpolytrace_geoeast_runner.exe");
    return QFileInfo::exists(candidate) ? QDir::toNativeSeparators(candidate) : candidate;
}

QString FaultTraceDialog::defaultOfficialDatPath() const {
    return envOrDefault(
        "FPT_OFFICIAL_DAT",
        QString::fromUtf8("D:/BaiduNetdiskDownload/BGPCup2025/赛题7-基于平面断层属性的断层多边形自动追踪/test.dat"));
}

void FaultTraceDialog::browseRunner() {
    const QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("选择 GeoEast Runner"), QFileInfo(runnerEdit_->text()).absolutePath(),
        QString::fromUtf8("可执行文件 (*.exe);;所有文件 (*.*)"));
    if (!path.isEmpty()) {
        runnerEdit_->setText(QDir::toNativeSeparators(path));
    }
}

void FaultTraceDialog::browseDatFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("选择官方 DAT 文件"), QFileInfo(datFileEdit_->text()).absolutePath(),
        QString::fromUtf8("DAT 文件 (*.dat);;所有文件 (*.*)"));
    if (!path.isEmpty()) {
        datFileEdit_->setText(QDir::toNativeSeparators(path));
    }
}

void FaultTraceDialog::browseGeoEastRoot() {
    const QString path = QFileDialog::getExistingDirectory(
        this, QString::fromUtf8("选择 GeoEast 安装根目录"), geoEastRootEdit_->text());
    if (!path.isEmpty()) {
        geoEastRootEdit_->setText(QDir::toNativeSeparators(path));
    }
}

void FaultTraceDialog::browseGeoEastDataRoot() {
    const QString path = QFileDialog::getExistingDirectory(
        this, QString::fromUtf8("选择 GeoEastData 根目录"), geoEastDataRootEdit_->text());
    if (!path.isEmpty()) {
        geoEastDataRootEdit_->setText(QDir::toNativeSeparators(path));
    }
}

void FaultTraceDialog::loadSettings() {
    QSettings settings(QCoreApplication::applicationDirPath() + "/faultpolytrace_launcher.ini", QSettings::IniFormat);
    runnerEdit_->setText(settings.value("runner", runnerEdit_->text()).toString());
    geoEastRootEdit_->setText(settings.value("geoEastRoot", geoEastRootEdit_->text()).toString());
    geoEastDataRootEdit_->setText(settings.value("geoEastDataRoot", geoEastDataRootEdit_->text()).toString());
    pgHostEdit_->setText(settings.value("pgHost", pgHostEdit_->text()).toString());
    pgPortEdit_->setText(settings.value("pgPort", pgPortEdit_->text()).toString());
    databaseEdit_->setText(settings.value("database", databaseEdit_->text()).toString());
    projectEdit_->setText(settings.value("project", projectEdit_->text()).toString());
    userEdit_->setText(settings.value("user", userEdit_->text()).toString());
    gridCombo_->setCurrentText(settings.value("grid", gridCombo_->currentText()).toString());
    outputEdit_->setText(settings.value("output", outputEdit_->text()).toString());
    datFileEdit_->setText(settings.value("datFile", datFileEdit_->text()).toString());
    polygonColorEdit_->setText(settings.value("polygonColor", polygonColorEdit_->text()).toString());
    setPolygonColor(QColor(polygonColorEdit_->text()));
    polygonLineWidthSpin_->setValue(settings.value("polygonLineWidth", polygonLineWidthSpin_->value()).toDouble());
    rootAntiMergeCheck_->setChecked(settings.value("rootAntiMerge", rootAntiMergeCheck_->isChecked()).toBool());
    rootAntiMergeRadiusSpin_->setValue(settings.value("rootAntiMergeRadius", rootAntiMergeRadiusSpin_->value()).toInt());
    rootAntiMergeSupportRatioSpin_->setValue(settings.value("rootAntiMergeSupportRatio", rootAntiMergeSupportRatioSpin_->value()).toDouble());
    residualTraceMinLengthSpin_->setValue(settings.value("residualTraceMinLength", residualTraceMinLengthSpin_->value()).toInt());
    residualTraceMaxCellsSpin_->setValue(settings.value("residualTraceMaxCells", residualTraceMaxCellsSpin_->value()).toInt());
}

void FaultTraceDialog::saveSettings() const {
    QSettings settings(QCoreApplication::applicationDirPath() + "/faultpolytrace_launcher.ini", QSettings::IniFormat);
    settings.setValue("runner", runnerEdit_->text());
    settings.setValue("geoEastRoot", geoEastRootEdit_->text());
    settings.setValue("geoEastDataRoot", geoEastDataRootEdit_->text());
    settings.setValue("pgHost", pgHostEdit_->text());
    settings.setValue("pgPort", pgPortEdit_->text());
    settings.setValue("database", databaseEdit_->text());
    settings.setValue("project", projectEdit_->text());
    settings.setValue("user", userEdit_->text());
    settings.setValue("grid", gridCombo_->currentText());
    settings.setValue("output", outputEdit_->text());
    settings.setValue("datFile", datFileEdit_->text());
    settings.setValue("polygonColor", polygonColorEdit_->text());
    settings.setValue("polygonLineWidth", polygonLineWidthSpin_->value());
    settings.setValue("rootAntiMerge", rootAntiMergeCheck_->isChecked());
    settings.setValue("rootAntiMergeRadius", rootAntiMergeRadiusSpin_->value());
    settings.setValue("rootAntiMergeSupportRatio", rootAntiMergeSupportRatioSpin_->value());
    settings.setValue("residualTraceMinLength", residualTraceMinLengthSpin_->value());
    settings.setValue("residualTraceMaxCells", residualTraceMaxCellsSpin_->value());
}

void FaultTraceDialog::choosePolygonColor() {
    const QColor initial(polygonColorEdit_->text());
    const QColor color = QColorDialog::getColor(
        initial.isValid() ? initial : QColor(0, 255, 255),
        this,
        QString::fromUtf8("选择 GeoEast 结果多边形颜色"));
    if (color.isValid()) {
        setPolygonColor(color);
    }
}

void FaultTraceDialog::setPolygonColor(const QColor& color) {
    const QColor effective = color.isValid() ? color : QColor(0, 255, 255);
    polygonColorEdit_->setText(effective.name(QColor::HexRgb).toUpper());
    const QString style = QString("background-color:%1; color:%2; font-weight:bold;")
        .arg(effective.name(QColor::HexRgb))
        .arg(effective.lightness() < 128 ? "#FFFFFF" : "#000000");
    polygonColorEdit_->setStyleSheet(style);
}

void FaultTraceDialog::configureGeoEastEnvironment(QProcessEnvironment& environment) const {
    const QString geoEastRoot = QDir::fromNativeSeparators(geoEastRootEdit_->text().trimmed());
    const QString ngp = geoEastRoot + "/NGP";
    const QString geoeast = geoEastRoot + "/GeoEast-RC";
    const QString pgHome = ngp + "/support/PostgreSQL/product";
    const QString ngpDataRoot = QDir::fromNativeSeparators(geoEastDataRootEdit_->text().trimmed());

    environment.insert("NGP", QDir::toNativeSeparators(ngp));
    environment.insert("IECO", QDir::toNativeSeparators(ngp));
    environment.insert("NDPDIR", QDir::toNativeSeparators(ngp));
    environment.insert("NGPDIR", QDir::toNativeSeparators(ngp));
    environment.insert("GEOEAST", QDir::toNativeSeparators(geoeast));
    environment.insert("PG_HOME", QDir::toNativeSeparators(pgHome));
    environment.insert("NGP_DBDATA_PATH", QDir::toNativeSeparators(ngpDataRoot + "/pgdata/" + databaseEdit_->text().trimmed()));
    environment.insert("NGPDATA", QDir::toNativeSeparators(ngpDataRoot + "/DATA/data"));

    QStringList pathItems;
    pathItems << pgHome + "/bin"
              << pgHome + "/lib"
              << geoeast + "/bin"
              << geoeast + "/bin/dbus"
              << ngp + "/libso/nap"
              << ngp + "/libso/ndp"
              << ngp + "/libso/common"
              << ngp + "/libso/geoscene"
              << ngp + "/Qt/Qt5.15.2/5.15.2/msvc2019_64/bin"
              << ngp + "/licmng/bin";

    QString existingPath = environment.value("PATH");
    QString path = QDir::toNativeSeparators(pathItems.join(';'));
    if (!existingPath.isEmpty()) {
        path += ';' + existingPath;
    }
    environment.insert("PATH", path);

    const QString networkDir = pgHome + "/network";
    QDir().mkpath(networkDir);
    QFile hostFile(networkDir + "/hostname.pg");
    if (hostFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        const QByteArray line = databaseEdit_->text().trimmed().toLocal8Bit() + "=" +
            pgHostEdit_->text().trimmed().toLocal8Bit() + ":" + pgPortEdit_->text().trimmed().toLocal8Bit() + "\n";
        hostFile.write(line);
    }
}

QStringList FaultTraceDialog::commonConnectionArgs() const {
    QStringList args;
    args << "--db" << databaseEdit_->text().trimmed()
         << "--project" << projectEdit_->text().trimmed()
         << "--user" << userEdit_->text().trimmed()
         << "--password" << passwordEdit_->text();
    return args;
}

QStringList FaultTraceDialog::traceArgs() const {
    QStringList args = commonConnectionArgs();
    if (importDatCheck_->isChecked()) {
        args << "--import-geoeast-dat" << datFileEdit_->text().trimmed();
        if (overwriteGridCheck_->isChecked()) {
            args << "--overwrite-grid";
        }
    }
    args << "--grid" << gridCombo_->currentText().trimmed()
         << "--output" << outputEdit_->text().trimmed()
         << "--trace-mode" << traceModeCombo_->currentText()
         << "--threshold-mode" << thresholdModeCombo_->currentText()
         << "--threshold" << QString::number(thresholdSpin_->value(), 'g', 12)
         << "--percentile" << QString::number(percentileSpin_->value(), 'g', 12)
         << "--close-radius" << QString::number(closeRadiusSpin_->value())
         << "--weak-percentile" << QString::number(weakPercentileSpin_->value(), 'g', 12)
         << "--weak-connect-radius" << QString::number(weakConnectRadiusSpin_->value())
         << "--belt-radius" << QString::number(beltRadiusSpin_->value())
         << "--post-belt-close-radius" << QString::number(postBeltCloseRadiusSpin_->value())
         << "--attribute-envelope-radius" << QString::number(attributeEnvelopeRadiusSpin_->value())
         << "--min-branch-length" << QString::number(minBranchLengthSpin_->value())
         << "--min-junction-branch-length" << QString::number(minJunctionBranchLengthSpin_->value())
         << "--min-junction-branch-mean" << QString::number(minJunctionBranchMeanSpin_->value(), 'g', 12)
         << "--max-gap-distance" << QString::number(maxGapDistanceSpin_->value())
         << "--max-gap-angle" << QString::number(maxGapAngleSpin_->value(), 'g', 12)
         << "--min-gap-support-ratio" << "0.25"
         << "--no-separate-branch-belts"
         << "--root-anti-merge-radius" << QString::number(rootAntiMergeRadiusSpin_->value())
         << "--root-anti-merge-support-ratio" << QString::number(rootAntiMergeSupportRatioSpin_->value(), 'g', 12)
         << "--root-anti-merge-max-bridge-value" << "1e-12"
         << "--residual-percentile" << "0.955"
         << "--residual-fill-radius" << "4"
         << "--residual-min-cells" << "2"
         << "--residual-trace-min-length" << QString::number(residualTraceMinLengthSpin_->value())
         << "--residual-trace-max-cells" << QString::number(residualTraceMaxCellsSpin_->value())
         << "--residual-covered-guard-radius" << "0"
         << "--no-residual-trace"
         << "--simplify-tolerance" << QString::number(simplifyToleranceSpin_->value(), 'g', 12)
         << "--smooth-iterations" << QString::number(smoothIterationsSpin_->value())
         << "--polygon-color" << polygonColorEdit_->text()
         << "--polygon-linewidth" << QString::number(polygonLineWidthSpin_->value(), 'g', 12)
         << "--min-area" << QString::number(minAreaSpin_->value(), 'g', 12);
    if (attributeConstrainedBeltCheck_->isChecked()) {
        args << "--attribute-constrained-belt";
    } else {
        args << "--no-attribute-constrained-belt";
    }
    if (rootAntiMergeCheck_->isChecked()) {
        args << "--root-anti-merge";
    } else {
        args << "--no-root-anti-merge";
    }
    return args;
}

void FaultTraceDialog::applyParameterPreset(const QString& presetName) {
    traceModeCombo_->setCurrentText("ridge-belt");
    thresholdModeCombo_->setCurrentText("percentile");
    thresholdSpin_->setValue(0.95);

    if (presetName == QString::fromUtf8("全覆盖优先")) {
        traceModeCombo_->setCurrentText("ridge-belt");
        thresholdModeCombo_->setCurrentText("percentile");
        percentileSpin_->setValue(0.975);
        minAreaSpin_->setValue(120.0);
        closeRadiusSpin_->setValue(1);
        weakPercentileSpin_->setValue(0.955);
        weakConnectRadiusSpin_->setValue(1);
        beltRadiusSpin_->setValue(3);
        postBeltCloseRadiusSpin_->setValue(1);
        attributeConstrainedBeltCheck_->setChecked(true);
        attributeEnvelopeRadiusSpin_->setValue(1);
        minBranchLengthSpin_->setValue(3);
        minJunctionBranchLengthSpin_->setValue(2);
        minJunctionBranchMeanSpin_->setValue(0.90);
        maxGapDistanceSpin_->setValue(12);
        maxGapAngleSpin_->setValue(60.0);
        rootAntiMergeCheck_->setChecked(false);
        rootAntiMergeRadiusSpin_->setValue(1);
        rootAntiMergeSupportRatioSpin_->setValue(0.98);
        simplifyToleranceSpin_->setValue(1.5);
        smoothIterationsSpin_->setValue(0);
        residualTraceMinLengthSpin_->setValue(5);
        residualTraceMaxCellsSpin_->setValue(120);
    } else if (presetName == QString::fromUtf8("保守高精度")) {
        percentileSpin_->setValue(0.985);
        minAreaSpin_->setValue(250.0);
        closeRadiusSpin_->setValue(1);
        weakPercentileSpin_->setValue(0.0);
        weakConnectRadiusSpin_->setValue(0);
        beltRadiusSpin_->setValue(2);
        postBeltCloseRadiusSpin_->setValue(0);
        attributeConstrainedBeltCheck_->setChecked(false);
        attributeEnvelopeRadiusSpin_->setValue(1);
        minBranchLengthSpin_->setValue(6);
        minJunctionBranchLengthSpin_->setValue(6);
        minJunctionBranchMeanSpin_->setValue(1.0);
        maxGapDistanceSpin_->setValue(8);
        maxGapAngleSpin_->setValue(45.0);
        rootAntiMergeCheck_->setChecked(false);
        rootAntiMergeRadiusSpin_->setValue(1);
        rootAntiMergeSupportRatioSpin_->setValue(0.98);
        simplifyToleranceSpin_->setValue(5.0);
        smoothIterationsSpin_->setValue(1);
        residualTraceMinLengthSpin_->setValue(8);
        residualTraceMaxCellsSpin_->setValue(40);
    } else if (presetName == QString::fromUtf8("弱响应高召回")) {
        percentileSpin_->setValue(0.98);
        minAreaSpin_->setValue(120.0);
        closeRadiusSpin_->setValue(1);
        weakPercentileSpin_->setValue(0.965);
        weakConnectRadiusSpin_->setValue(1);
        beltRadiusSpin_->setValue(3);
        postBeltCloseRadiusSpin_->setValue(1);
        attributeConstrainedBeltCheck_->setChecked(true);
        attributeEnvelopeRadiusSpin_->setValue(1);
        minBranchLengthSpin_->setValue(4);
        minJunctionBranchLengthSpin_->setValue(2);
        minJunctionBranchMeanSpin_->setValue(0.94);
        maxGapDistanceSpin_->setValue(12);
        maxGapAngleSpin_->setValue(55.0);
        rootAntiMergeCheck_->setChecked(false);
        rootAntiMergeRadiusSpin_->setValue(1);
        rootAntiMergeSupportRatioSpin_->setValue(0.98);
        simplifyToleranceSpin_->setValue(2.0);
        smoothIterationsSpin_->setValue(1);
        residualTraceMinLengthSpin_->setValue(5);
        residualTraceMaxCellsSpin_->setValue(120);
    } else if (presetName == QString::fromUtf8("宽断层带")) {
        percentileSpin_->setValue(0.97);
        minAreaSpin_->setValue(300.0);
        closeRadiusSpin_->setValue(2);
        weakPercentileSpin_->setValue(0.0);
        weakConnectRadiusSpin_->setValue(0);
        beltRadiusSpin_->setValue(3);
        postBeltCloseRadiusSpin_->setValue(1);
        attributeConstrainedBeltCheck_->setChecked(false);
        attributeEnvelopeRadiusSpin_->setValue(1);
        minBranchLengthSpin_->setValue(5);
        minJunctionBranchLengthSpin_->setValue(5);
        minJunctionBranchMeanSpin_->setValue(1.0);
        maxGapDistanceSpin_->setValue(14);
        maxGapAngleSpin_->setValue(60.0);
        rootAntiMergeCheck_->setChecked(false);
        rootAntiMergeRadiusSpin_->setValue(1);
        rootAntiMergeSupportRatioSpin_->setValue(0.98);
        simplifyToleranceSpin_->setValue(6.0);
        smoothIterationsSpin_->setValue(1);
        residualTraceMinLengthSpin_->setValue(8);
        residualTraceMaxCellsSpin_->setValue(60);
    } else if (presetName == QString::fromUtf8("细碎断裂")) {
        percentileSpin_->setValue(0.98);
        minAreaSpin_->setValue(120.0);
        closeRadiusSpin_->setValue(1);
        weakPercentileSpin_->setValue(0.0);
        weakConnectRadiusSpin_->setValue(0);
        beltRadiusSpin_->setValue(1);
        postBeltCloseRadiusSpin_->setValue(0);
        attributeConstrainedBeltCheck_->setChecked(true);
        attributeEnvelopeRadiusSpin_->setValue(1);
        minBranchLengthSpin_->setValue(3);
        minJunctionBranchLengthSpin_->setValue(3);
        minJunctionBranchMeanSpin_->setValue(1.0);
        maxGapDistanceSpin_->setValue(8);
        maxGapAngleSpin_->setValue(50.0);
        rootAntiMergeCheck_->setChecked(false);
        rootAntiMergeRadiusSpin_->setValue(1);
        rootAntiMergeSupportRatioSpin_->setValue(0.98);
        simplifyToleranceSpin_->setValue(3.0);
        smoothIterationsSpin_->setValue(1);
        residualTraceMinLengthSpin_->setValue(4);
        residualTraceMaxCellsSpin_->setValue(80);
    } else if (presetName == QString::fromUtf8("贴合融合推荐")) {
        percentileSpin_->setValue(0.98);
        minAreaSpin_->setValue(120.0);
        closeRadiusSpin_->setValue(1);
        weakPercentileSpin_->setValue(0.965);
        weakConnectRadiusSpin_->setValue(1);
        beltRadiusSpin_->setValue(3);
        postBeltCloseRadiusSpin_->setValue(1);
        attributeConstrainedBeltCheck_->setChecked(true);
        attributeEnvelopeRadiusSpin_->setValue(1);
        minBranchLengthSpin_->setValue(4);
        minJunctionBranchLengthSpin_->setValue(2);
        minJunctionBranchMeanSpin_->setValue(0.94);
        maxGapDistanceSpin_->setValue(12);
        maxGapAngleSpin_->setValue(55.0);
        rootAntiMergeCheck_->setChecked(false);
        rootAntiMergeRadiusSpin_->setValue(1);
        rootAntiMergeSupportRatioSpin_->setValue(0.98);
        simplifyToleranceSpin_->setValue(2.0);
        smoothIterationsSpin_->setValue(1);
        residualTraceMinLengthSpin_->setValue(6);
        residualTraceMaxCellsSpin_->setValue(80);
    } else {
        percentileSpin_->setValue(0.98);
        minAreaSpin_->setValue(120.0);
        closeRadiusSpin_->setValue(1);
        weakPercentileSpin_->setValue(0.965);
        weakConnectRadiusSpin_->setValue(1);
        beltRadiusSpin_->setValue(3);
        postBeltCloseRadiusSpin_->setValue(1);
        attributeConstrainedBeltCheck_->setChecked(true);
        attributeEnvelopeRadiusSpin_->setValue(1);
        minBranchLengthSpin_->setValue(4);
        minJunctionBranchLengthSpin_->setValue(2);
        minJunctionBranchMeanSpin_->setValue(0.94);
        maxGapDistanceSpin_->setValue(20);
        maxGapAngleSpin_->setValue(55.0);
        rootAntiMergeCheck_->setChecked(false);
        rootAntiMergeRadiusSpin_->setValue(1);
        rootAntiMergeSupportRatioSpin_->setValue(0.98);
        simplifyToleranceSpin_->setValue(2.0);
        smoothIterationsSpin_->setValue(1);
        residualTraceMinLengthSpin_->setValue(6);
        residualTraceMaxCellsSpin_->setValue(80);
    }
}

void FaultTraceDialog::refreshGrids() {
    if (process_->state() != QProcess::NotRunning) {
        return;
    }
    saveSettings();
    const QString runner = runnerEdit_->text().trimmed();
    if (!QFileInfo::exists(runner)) {
        QMessageBox::warning(this, QString::fromUtf8("Runner 不存在"), runner);
        return;
    }

    refreshingGrids_ = true;
    logEdit_->clear();
    QStringList args = commonConnectionArgs();
    args << "--list-grids";
    appendStatus(QString::fromUtf8("正在刷新 GeoEast 网格对象..."));
    appendLog("> " + runner + " " + quoteForLog(args));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    configureGeoEastEnvironment(environment);
    process_->setProcessEnvironment(environment);
    setRunning(true);
    process_->start(runner, args);
}

void FaultTraceDialog::runTrace() {
    if (process_->state() != QProcess::NotRunning) {
        return;
    }
    saveSettings();
    const QString runner = runnerEdit_->text().trimmed();
    if (!QFileInfo::exists(runner)) {
        QMessageBox::warning(this, QString::fromUtf8("Runner 不存在"), runner);
        return;
    }
    if (gridCombo_->currentText().trimmed().isEmpty() || outputEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("参数不完整"), QString::fromUtf8("输入网格和输出多边形名称不能为空。"));
        return;
    }

    refreshingGrids_ = false;
    logEdit_->clear();
    QStringList args = traceArgs();
    appendStatus(QString::fromUtf8("正在追踪断层多边形..."));
    appendLog("> " + runner + " " + quoteForLog(args));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    configureGeoEastEnvironment(environment);
    process_->setProcessEnvironment(environment);
    setRunning(true);
    process_->start(runner, args);
}

void FaultTraceDialog::stopProcess() {
    if (process_->state() != QProcess::NotRunning) {
        process_->terminate();
    }
}

void FaultTraceDialog::onReadyRead() {
    const QString output = QString::fromLocal8Bit(process_->readAllStandardOutput());
    const QString error = QString::fromLocal8Bit(process_->readAllStandardError());
    if (!output.isEmpty()) {
        appendLog(output);
    }
    if (!error.isEmpty()) {
        appendLog(error);
    }
}

void FaultTraceDialog::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    onReadyRead();
    setRunning(false);

    if (refreshingGrids_ && exitCode == 0 && status == QProcess::NormalExit) {
        const QString text = logEdit_->toPlainText();
        QStringList names;
        for (const QString& rawLine : text.split('\n')) {
            const QString line = rawLine.trimmed();
            if (line.isEmpty() || line.startsWith("BOMapGrid") || line.startsWith("Total:") ||
                line.startsWith("load ") || line.startsWith("Env ") || line.startsWith("GeoEast ") ||
                line.startsWith("Connecting ") || line.startsWith(">")) {
                continue;
            }
            if (!line.contains(' ') && !line.contains(':')) {
                names << line;
            }
        }
        if (!names.isEmpty()) {
            const QString current = gridCombo_->currentText();
            gridCombo_->clear();
            gridCombo_->addItems(names);
            int index = gridCombo_->findText(current);
            if (index >= 0) {
                gridCombo_->setCurrentIndex(index);
            }
        }
        appendStatus(QString::fromUtf8("网格对象刷新完成"));
        refreshingGrids_ = false;
        return;
    }

    if (exitCode == 0 && status == QProcess::NormalExit) {
        appendStatus(QString::fromUtf8("追踪完成，结果已写入 GeoEast 数据库"));
    } else {
        appendStatus(QString::fromUtf8("运行失败，请查看日志"));
    }
    refreshingGrids_ = false;
}

void FaultTraceDialog::onProcessError(QProcess::ProcessError error) {
    appendLog(QString::fromUtf8("进程错误：%1").arg(static_cast<int>(error)));
    appendStatus(QString::fromUtf8("进程启动或运行异常"));
    setRunning(false);
}

void FaultTraceDialog::setRunning(bool running) {
    refreshButton_->setEnabled(!running);
    runButton_->setEnabled(!running);
    stopButton_->setEnabled(running);
}

void FaultTraceDialog::appendLog(const QString& text) {
    logEdit_->moveCursor(QTextCursor::End);
    logEdit_->insertPlainText(text);
    if (!text.endsWith('\n')) {
        logEdit_->insertPlainText("\n");
    }
    logEdit_->moveCursor(QTextCursor::End);
}

void FaultTraceDialog::appendStatus(const QString& text) {
    statusLabel_->setText(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + text);
}
