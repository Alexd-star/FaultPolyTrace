#include "FaultPolyTraceWindow.h"
#include "FptIcons.h"
#include "FptControls.h"
#include "FptLog.h"
#include "FptTheme.h"
#include "UiLocalization.h"
#include "TraceParameterModel.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QPixmap>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTextEdit>
#include <QTextCursor>
#include <QTextStream>
#include <QToolBar>
#include <QToolButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>

namespace {

class GuidedCatalogComboBox : public QComboBox {
public:
    using BeforePopupHandler = std::function<bool()>;

    explicit GuidedCatalogComboBox(QWidget* parent = nullptr)
        : QComboBox(parent) {
    }

    void setBeforePopupHandler(BeforePopupHandler handler) {
        beforePopupHandler_ = std::move(handler);
    }

protected:
    void showPopup() override {
        if (beforePopupHandler_ && beforePopupHandler_()) {
            return;
        }
        QComboBox::showPopup();
    }

private:
    BeforePopupHandler beforePopupHandler_;
};

class NoWheelValueFilter : public QObject {
public:
    explicit NoWheelValueFilter(QObject* parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject* object, QEvent* event) override {
        if (event->type() == QEvent::Wheel &&
            (qobject_cast<QAbstractSpinBox*>(object) || qobject_cast<QComboBox*>(object))) {
            event->ignore();
            return true;
        }
        return QObject::eventFilter(object, event);
    }
};

QString repoRootFromAppDir() {
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        if (dir.exists("src") && dir.exists("examples")) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QDir::currentPath();
}

QString appConfigDir() {
    QString dir = QString::fromLocal8Bit(qgetenv("FPT_CONFIG_ROOT")).trimmed();
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    }
    if (dir.isEmpty()) {
        dir = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("config");
    }
    QDir().mkpath(dir);
    return dir;
}

QString envValue(const char* name) {
    return QString::fromLocal8Bit(qgetenv(name)).trimmed();
}

QString firstExistingPath(const QStringList& candidates) {
    for (const QString& candidate : candidates) {
        const QString path = QDir::fromNativeSeparators(candidate.trimmed());
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            return QDir::toNativeSeparators(path);
        }
    }
    return QString();
}

QString geoEastRootFromPath(const QString& value) {
    const QString path = QDir::fromNativeSeparators(value.trimmed());
    if (path.isEmpty()) {
        return QString();
    }

    QFileInfo info(path);
    QDir dir(info.isDir() ? path : info.absolutePath());
    for (int i = 0; i < 5; ++i) {
        if (dir.exists("GeoEast-RC") || dir.exists("NGP")) {
            return QDir::toNativeSeparators(dir.absolutePath());
        }
        const QString name = dir.dirName();
        if (name.compare("GeoEast-RC", Qt::CaseInsensitive) == 0 ||
            name.compare("NGP", Qt::CaseInsensitive) == 0 ||
            name.compare("iECO", Qt::CaseInsensitive) == 0) {
            QDir parent(dir);
            if (parent.cdUp()) {
                return QDir::toNativeSeparators(parent.absolutePath());
            }
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QString();
}

QString detectGeoEastRoot() {
    for (const char* name : {"FPT_GEOEAST_ROOT", "GEOEAST_ROOT"}) {
        const QString value = envValue(name);
        if (!value.isEmpty()) {
            return QDir::toNativeSeparators(value);
        }
    }

    for (const char* name : {"GEOEAST", "NGP", "IECO", "IECO_ROOT", "NGP_ROOT"}) {
        const QString root = geoEastRootFromPath(envValue(name));
        if (!root.isEmpty()) {
            return root;
        }
    }

    const QString appRoot = geoEastRootFromPath(QCoreApplication::applicationDirPath());
    if (!appRoot.isEmpty()) {
        return appRoot;
    }

    const QString userProfile = envValue("USERPROFILE");
    return firstExistingPath({
        userProfile.isEmpty() ? QString() : QDir(userProfile).absoluteFilePath("GeoEast"),
        "C:/GeoEast",
        "D:/GeoEast",
        "C:/Program Files/GeoEast"
    });
}

QString detectGeoEastDataRoot(const QString& geoEastRoot) {
    for (const char* name : {"FPT_GEOEAST_DATA_ROOT", "GEOEAST_DATA_ROOT"}) {
        const QString value = envValue(name);
        if (!value.isEmpty()) {
            return QDir::toNativeSeparators(value);
        }
    }

    const QString dbData = envValue("NGP_DBDATA_PATH");
    if (!dbData.isEmpty()) {
        QDir dir(QDir::fromNativeSeparators(dbData));
        if (dir.cdUp() && dir.cdUp()) {
            return QDir::toNativeSeparators(dir.absolutePath());
        }
    }

    const QString root = QDir::fromNativeSeparators(geoEastRoot);
    if (root.isEmpty()) {
        return QString();
    }

    const QString direct = firstExistingPath({
        QDir(root).absoluteFilePath("GeoEast-RC/GeoEastData"),
        QDir(root).absoluteFilePath("GeoEastData")
    });
    if (!direct.isEmpty()) {
        return direct;
    }

    QDir rc(QDir(root).absoluteFilePath("GeoEast-RC"));
    const QFileInfoList matches = rc.entryInfoList(QStringList() << "GeoEastData*", QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : matches) {
        const QString nested = QDir(info.absoluteFilePath()).absoluteFilePath("GeoEastData");
        return QFileInfo::exists(nested)
            ? QDir::toNativeSeparators(nested)
            : QDir::toNativeSeparators(info.absoluteFilePath());
    }
    return QString();
}

QString detectGeoEastDatabaseName(const QString& dataRoot) {
    const QString fromEnv = envValue("FPT_GEOEAST_DB");
    if (!fromEnv.isEmpty()) {
        return fromEnv;
    }

    const QString dbData = envValue("NGP_DBDATA_PATH");
    if (!dbData.isEmpty()) {
        const QString name = QFileInfo(QDir::fromNativeSeparators(dbData)).fileName();
        if (!name.isEmpty()) {
            return name;
        }
    }

    QDir pgData(QDir(QDir::fromNativeSeparators(dataRoot)).absoluteFilePath("pgdata"));
    if (pgData.exists()) {
        const QFileInfoList databases = pgData.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& info : databases) {
            if (info.fileName().compare("ndp", Qt::CaseInsensitive) == 0) {
                return info.fileName();
            }
        }
        if (!databases.isEmpty()) {
            return databases.first().fileName();
        }
    }
    return "ndp";
}

QString geoEastConfigHelpText() {
    return QString::fromUtf8(
        "GeoEast 数据库配置用于让 FaultPolyTrace 读取 BOMapGrid，并把追踪结果写入 BOFaultPolygon。\n\n"
        "字段来源：\n"
        "1. 执行程序文件：通常位于 GeoEast-RC\\bin\\faultpolytrace_geoeast_runner.exe，发布包或 GeoEast 插件部署后会自动检测。\n"
        "2. GeoEast 根目录：包含 NGP 和 GeoEast-RC 两个子目录的安装根目录。\n"
        "3. GeoEast 数据根目录：GeoEastData 根目录，通常包含 pgdata 和 DATA 等数据目录。\n"
        "4. PG 主机和端口：本机 GeoEast 通常为 127.0.0.1 和 5433；如果连接远端数据库，请填写管理员提供的地址。\n"
        "5. 数据库：通常为 ndp，也可从 GeoEast 项目管理或数据源配置中确认。\n"
        "6. 项目：GeoEast 当前打开或需要处理的项目名，可在 GeoEast 主控标题、打开项目窗口或项目管理工具中查看。\n"
        "7. 用户：GeoEast 登录用户，通常与 Windows 用户名一致。\n\n"
        "建议流程：先点击【自动检测】，再点击【检查配置】。检查通过后点击【应用并保存配置】，然后返回主界面点击【刷新网格列表】。");
}

QStringList validateGeoEastConfigValues(const QString& runner,
                                        const QString& geoEastRoot,
                                        const QString& dataRoot,
                                        const QString& host,
                                        const QString& port,
                                        const QString& database,
                                        const QString& project,
                                        const QString& user) {
    QStringList issues;
    const QString runnerPath = runner.trimmed();
    const QString rootPath = QDir::fromNativeSeparators(geoEastRoot.trimmed());
    const QString dataRootPath = QDir::fromNativeSeparators(dataRoot.trimmed());
    if (runnerPath.isEmpty()) {
        issues << QString::fromUtf8("执行程序文件未填写");
    } else if (!QFileInfo::exists(runnerPath)) {
        issues << QString::fromUtf8("执行程序文件不存在：%1").arg(QDir::toNativeSeparators(runnerPath));
    }
    if (rootPath.isEmpty()) {
        issues << QString::fromUtf8("GeoEast 根目录未填写");
    } else if (!QDir(rootPath).exists()) {
        issues << QString::fromUtf8("GeoEast 根目录不存在：%1").arg(QDir::toNativeSeparators(rootPath));
    } else if (!QDir(rootPath).exists("NGP") || !QDir(rootPath).exists("GeoEast-RC")) {
        issues << QString::fromUtf8("GeoEast 根目录应包含 NGP 和 GeoEast-RC 子目录");
    }
    if (dataRootPath.isEmpty()) {
        issues << QString::fromUtf8("GeoEast 数据根目录未填写");
    } else if (!QDir(dataRootPath).exists()) {
        issues << QString::fromUtf8("GeoEast 数据根目录不存在：%1").arg(QDir::toNativeSeparators(dataRootPath));
    }
    if (host.trimmed().isEmpty()) {
        issues << QString::fromUtf8("PG 主机未填写");
    }
    bool portOk = false;
    const int portValue = port.trimmed().toInt(&portOk);
    if (!portOk || portValue <= 0 || portValue > 65535) {
        issues << QString::fromUtf8("PG 端口应为 1 到 65535 之间的数字");
    }
    if (database.trimmed().isEmpty()) {
        issues << QString::fromUtf8("数据库名未填写");
    }
    if (project.trimmed().isEmpty()) {
        issues << QString::fromUtf8("项目名未填写。请在 GeoEast 主控标题、打开项目窗口或项目管理工具中确认");
    }
    if (user.trimmed().isEmpty()) {
        issues << QString::fromUtf8("用户名未填写");
    }
    return issues;
}

QString defaultUserName() {
    const QString fromEnv = envValue("FPT_GEOEAST_USER");
    if (!fromEnv.isEmpty()) {
        return fromEnv;
    }
    const QString username = envValue("USERNAME");
    return username.isEmpty() ? envValue("USER") : username;
}

QString settingOrCurrent(const QSettings& settings, const QString& key, const QString& current, int schemaVersion) {
    return schemaVersion < 2 ? current : settings.value(key, current).toString();
}

QString elapsedText(qint64 ms) {
    return QString::fromUtf8("%1 ms (%2 s)").arg(ms).arg(ms / 1000.0, 0, 'f', ms < 1000 ? 3 : 2);
}

QString quoteForLog(const QStringList& args) {
    QStringList quoted;
    bool redactNext = false;
    for (const QString& arg : args) {
        const QString value = redactNext ? QString::fromUtf8("<已隐藏>") : arg;
        quoted << (value.contains(' ') ? QString("\"%1\"").arg(value) : value);
        redactNext = arg == "--password";
    }
    return quoted.join(' ');
}

QVector<double> splitNumbers(const QString& line) {
    QVector<double> numbers;
    for (const QString& token : line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)) {
        bool ok = false;
        const double value = token.toDouble(&ok);
        if (ok) {
            numbers << value;
        }
    }
    return numbers;
}

QVector<double> splitNumbers(const QByteArray& line) {
    QVector<double> numbers;
    const char* cursor = line.constData();
    char* end = nullptr;
    while (*cursor != '\0') {
        const double value = std::strtod(cursor, &end);
        if (end == cursor) {
            ++cursor;
            continue;
        }
        numbers << value;
        cursor = end;
    }
    return numbers;
}

QVector<QPolygonF> parsePolygonRings(const QString& wkt, double* pxMin, double* pyMin, double* pxMax, double* pyMax) {
    QVector<QPolygonF> rings;
    static const QRegularExpression ringRe("\\(([^()]+)\\)");
    static const QRegularExpression numberRe("[-+]?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?");

    QRegularExpressionMatchIterator ringIterator = ringRe.globalMatch(wkt);
    while (ringIterator.hasNext()) {
        const QString ringText = ringIterator.next().captured(1);
        QRegularExpressionMatchIterator numberIterator = numberRe.globalMatch(ringText);
        QVector<double> numbers;
        while (numberIterator.hasNext()) {
            numbers << numberIterator.next().captured(0).toDouble();
        }

        QPolygonF ring;
        for (int i = 0; i + 1 < numbers.size(); i += 2) {
            const QPointF point(numbers[i], numbers[i + 1]);
            ring << point;
            if (pxMin && pyMin && pxMax && pyMax) {
                *pxMin = qMin(*pxMin, point.x());
                *pyMin = qMin(*pyMin, point.y());
                *pxMax = qMax(*pxMax, point.x());
                *pyMax = qMax(*pyMax, point.y());
            }
        }
        if (ring.size() >= 3) {
            rings << ring;
        }
    }
    return rings;
}

QLabel* formLabel(const QString& text, QWidget* parent) {
    QLabel* label = new QLabel(text, parent);
    label->setMinimumWidth(90);
    label->setMaximumWidth(126);
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return label;
}
void addRow(QGridLayout* layout, int row, const QString& label, QWidget* widget, QWidget* parent) {
    widget->setMinimumHeight(24);
    layout->addWidget(formLabel(label, parent), row, 0);
    layout->addWidget(widget, row, 1);
}

void configureFormGrid(QGridLayout* layout) {
    layout->setContentsMargins(10, 14, 10, 10);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(6);
    layout->setColumnMinimumWidth(0, 90);
    layout->setColumnStretch(1, 1);
}

void configureSmallButton(QPushButton* button) {
    button->setMinimumWidth(64);
    button->setMinimumHeight(24);
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}
void configureElasticButton(QPushButton* button) {
    button->setMinimumWidth(0);
    button->setMinimumHeight(24);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void configurePathLineEdit(QLineEdit* edit) {
    edit->setMinimumWidth(0);
    edit->setMaximumWidth(QWIDGETSIZE_MAX);
    edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    edit->setCursorPosition(0);
}



} // namespace

class AttributeView : public QWidget {
public:
    enum class Mode {
        Task,
        Raw,
        Result,
        Overlay
    };

    explicit AttributeView(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(640, 460);
        setMouseTracking(true);
        setCursor(Qt::OpenHandCursor);
    }

    void setMode(Mode mode) {
        mode_ = mode;
        update();
    }

    int polygonCount() const {
        return polygons_.size();
    }

    void setLayerVisible(bool showGrid, bool showPolygons) {
        showGridLayer_ = showGrid;
        showPolygonLayer_ = showPolygons;
        update();
    }

    void setStyle(const QColor& color, double lineWidth, double alpha) {
        color_ = color.isValid() ? color : QColor("#00FFFF");
        lineWidth_ = lineWidth;
        alpha_ = qBound(0.05, alpha, 1.0);
        update();
    }

    void setLanguage(UiLanguage language) {
        languageCode_ = language == UiLanguage::English ? "en" : "zh-CN";
        update();
    }

    void setTaskSummary(const QStringList& lines) {
        taskLines_ = lines;
        update();
    }

    void setPanMode(bool enabled) {
        panMode_ = enabled;
        setCursor(enabled ? Qt::OpenHandCursor : Qt::ArrowCursor);
        update();
    }

    void zoomIn() {
        zoom_ = qMin(4.0, zoom_ * 1.25);
        update();
    }

    void zoomOut() {
        zoom_ = qMax(0.35, zoom_ / 1.25);
        update();
    }

    void resetView() {
        zoom_ = 1.0;
        panOffset_ = QPointF(0, 0);
        panMode_ = true;
        dragging_ = false;
        setCursor(Qt::OpenHandCursor);
        update();
    }

    bool loadGrid(const QString& path, const QString& format, QString* error) {
        const QFileInfo sourceInfo(path);
        const QString canonicalPath = sourceInfo.absoluteFilePath();
        const qint64 sourceMtime = sourceInfo.exists() ? sourceInfo.lastModified().toMSecsSinceEpoch() : -1;
        if (sourceInfo.exists() && canonicalPath == gridCachePath_ && format == gridCacheFormat_ &&
            sourceInfo.size() == gridCacheSize_ && sourceMtime == gridCacheMtime_ &&
            nx_ > 0 && ny_ > 0 && !grid_.isEmpty()) {
            return true;
        }
        grid_.clear();
        gridImage_ = QImage();
        gridImageDirty_ = true;
        nx_ = 0;
        ny_ = 0;
        xMin_ = 0.0;
        yMin_ = 0.0;
        xMax_ = 1.0;
        yMax_ = 1.0;
        min_ = 0.0;
        max_ = 1.0;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (error) {
                *error = QString::fromUtf8("无法打开输入文件：%1").arg(path);
            }
            update();
            return false;
        }

        QVector<QVector<double>> headerNumbers;
        while (!file.atEnd() && headerNumbers.size() < 3) {
            const QByteArray rawLine = file.readLine();
            const QVector<double> numbers = splitNumbers(rawLine);
            if (!numbers.isEmpty()) {
                headerNumbers << numbers;
            }
        }
        if (headerNumbers.isEmpty()) {
            if (error) {
                *error = QString::fromUtf8("输入文件为空。");
            }
            update();
            return false;
        }

        const QString fmt = format.toLower();
        const bool matrixLike = fmt == "matrix" ||
            (fmt == "auto" && headerNumbers.size() >= 3 &&
             headerNumbers[0].size() == 2 && headerNumbers[1].size() == 2 && headerNumbers[2].size() == 2);
        file.seek(0);

        if (matrixLike) {
            QVector<double> first;
            QVector<double> second;
            QVector<double> third;
            int headerIndex = 0;
            while (!file.atEnd()) {
                const QVector<double> numbers = splitNumbers(file.readLine());
                if (numbers.isEmpty()) {
                    continue;
                }
                if (headerIndex == 0) {
                    first = numbers;
                } else if (headerIndex == 1) {
                    second = numbers;
                } else if (headerIndex == 2) {
                    third = numbers;
                } else {
                    for (double value : numbers) {
                        grid_ << value;
                    }
                }
                ++headerIndex;
            }
            if (first.size() < 2 || second.size() < 2 || third.size() < 2) {
                if (error) {
                    *error = QString::fromUtf8("矩阵网格头不足。");
                }
                return false;
            }
            nx_ = qRound(first[0]);
            ny_ = qRound(first[1]);
            if (nx_ <= 0 || ny_ <= 0) {
                if (error) {
                    *error = QString::fromUtf8("矩阵网格头无效。");
                }
                return false;
            }
            if (grid_.size() < nx_ * ny_) {
                if (error) {
                    *error = QString::fromUtf8("矩阵网格数值数量不足。");
                }
                return false;
            }
            grid_.resize(nx_ * ny_);
            xMin_ = second[0];
            yMin_ = second[1];
            const double dx = third[0] == 0.0 ? 1.0 : third[0];
            const double dy = third[1] == 0.0 ? 1.0 : third[1];
            xMax_ = xMin_ + dx * qMax(1, nx_);
            yMax_ = yMin_ + dy * qMax(1, ny_);
        } else {
            QVector<int> lineKeys;
            QVector<int> cmpKeys;
            QMap<int, double> lineToX;
            QMap<int, double> cmpToY;

            while (!file.atEnd()) {
                const QVector<double> values = splitNumbers(file.readLine());
                if (values.size() < 5) {
                    continue;
                }
                const int line = qRound(values[0]);
                const int cmp = qRound(values[1]);
                if (!lineToX.contains(line)) {
                    lineToX.insert(line, values[2]);
                    lineKeys << line;
                }
                if (!cmpToY.contains(cmp)) {
                    cmpToY.insert(cmp, values[3]);
                    cmpKeys << cmp;
                }
            }
            if (lineKeys.isEmpty() || cmpKeys.isEmpty()) {
                if (error) {
                    *error = QString::fromUtf8("未识别到 GeoEast DAT 五列采样数据。");
                }
                return false;
            }
            std::sort(lineKeys.begin(), lineKeys.end());
            std::sort(cmpKeys.begin(), cmpKeys.end());
            nx_ = lineKeys.size();
            ny_ = cmpKeys.size();
            grid_.fill(0.0, nx_ * ny_);

            QMap<int, int> lineIndex;
            QMap<int, int> cmpIndex;
            for (int i = 0; i < lineKeys.size(); ++i) {
                lineIndex.insert(lineKeys[i], i);
            }
            for (int i = 0; i < cmpKeys.size(); ++i) {
                cmpIndex.insert(cmpKeys[i], i);
            }

            const double dx = lineKeys.size() > 1
                ? qAbs(lineToX.value(lineKeys[1]) - lineToX.value(lineKeys[0]))
                : 1.0;
            const double dy = cmpKeys.size() > 1
                ? qAbs(cmpToY.value(cmpKeys[1]) - cmpToY.value(cmpKeys[0]))
                : 1.0;
            xMin_ = lineToX.value(lineKeys.first()) - 0.5 * dx;
            yMin_ = cmpToY.value(cmpKeys.first()) - 0.5 * dy;
            xMax_ = xMin_ + dx * qMax(1, nx_);
            yMax_ = yMin_ + dy * qMax(1, ny_);

            file.seek(0);
            while (!file.atEnd()) {
                const QVector<double> values = splitNumbers(file.readLine());
                if (values.size() < 5) {
                    continue;
                }
                const int line = qRound(values[0]);
                const int cmp = qRound(values[1]);
                const int col = lineIndex.value(line, -1);
                const int row = cmpIndex.value(cmp, -1);
                if (row >= 0 && row < ny_ && col >= 0 && col < nx_) {
                    grid_[row * nx_ + col] = values[4];
                }
            }
        }

        min_ = std::numeric_limits<double>::max();
        max_ = -std::numeric_limits<double>::max();
        for (double value : grid_) {
            min_ = qMin(min_, value);
            max_ = qMax(max_, value);
        }
        if (qFuzzyCompare(min_, max_)) {
            max_ = min_ + 1.0;
        }
        gridImageDirty_ = true;
        status_ = QString::fromUtf8("原始网格：%1 x %2").arg(nx_).arg(ny_);
        gridCachePath_ = canonicalPath;
        gridCacheFormat_ = format;
        gridCacheSize_ = sourceInfo.size();
        gridCacheMtime_ = sourceMtime;
        update();
        return true;
    }

    bool loadPolygons(const QString& path, QString* error) {
        const QFileInfo sourceInfo(path);
        const QString canonicalPath = sourceInfo.absoluteFilePath();
        const qint64 sourceMtime = sourceInfo.exists() ? sourceInfo.lastModified().toMSecsSinceEpoch() : -1;
        if (sourceInfo.exists() && canonicalPath == polygonCachePath_ &&
            sourceInfo.size() == polygonCacheSize_ && sourceMtime == polygonCacheMtime_ &&
            !polygons_.isEmpty()) {
            return true;
        }
        polygons_.clear();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (error) {
                *error = QString::fromUtf8("无法打开结果 CSV：%1").arg(path);
            }
            update();
            return false;
        }
        const QStringList lines = QString::fromLocal8Bit(file.readAll()).split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        double pxMin = std::numeric_limits<double>::max();
        double pyMin = std::numeric_limits<double>::max();
        double pxMax = -std::numeric_limits<double>::max();
        double pyMax = -std::numeric_limits<double>::max();
        for (const QString& line : lines) {
            if (!line.contains("POLYGON", Qt::CaseInsensitive) || !line.contains("kept", Qt::CaseInsensitive)) {
                continue;
            }
            const QString wkt = line.mid(line.indexOf("POLYGON", 0, Qt::CaseInsensitive));
            const QVector<QPolygonF> rings = parsePolygonRings(wkt, &pxMin, &pyMin, &pxMax, &pyMax);
            if (!rings.isEmpty()) {
                polygons_ << rings;
            }
        }
        if (grid_.isEmpty() && !polygons_.isEmpty()) {
            xMin_ = pxMin;
            yMin_ = pyMin;
            xMax_ = pxMax;
            yMax_ = pyMax;
        }
        status_ = polygons_.isEmpty() ? QString::fromUtf8("未发现保留多边形") : QString::fromUtf8("结果多边形：%1 个").arg(polygons_.size());
        if (!polygons_.isEmpty()) {
            polygonCachePath_ = canonicalPath;
            polygonCacheSize_ = sourceInfo.size();
            polygonCacheMtime_ = sourceMtime;
        }
        update();
        return !polygons_.isEmpty();
    }

    void clearPolygons(const QString& status = QString()) {
        polygons_.clear();
        if (!status.isEmpty()) {
            status_ = status;
        } else if (!grid_.isEmpty()) {
            status_ = QString::fromUtf8("原始网格：%1 x %2").arg(nx_).arg(ny_);
        } else {
            status_ = QString();
        }
        update();
    }

    void clearData(const QString& status = QString()) {
        grid_.clear();
        polygons_.clear();
        gridImage_ = QImage();
        gridImageDirty_ = true;
        nx_ = 0;
        ny_ = 0;
        xMin_ = 0.0;
        yMin_ = 0.0;
        xMax_ = 1.0;
        yMax_ = 1.0;
        min_ = 0.0;
        max_ = 1.0;
        status_ = status;
        update();
    }

    bool hasVisibleContent() const {
        return (showGridLayer_ && !grid_.isEmpty()) ||
               (showPolygonLayer_ && !polygons_.isEmpty());
    }

    bool savePng(const QString& path) {
        QPixmap pixmap(size());
        render(&pixmap);
        return pixmap.save(path, "PNG");
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#FFFFFF"));
        painter.setPen(QPen(QColor("#EEEEEE"), 1));
        const int gridStep = qMax(20, qRound(48 * zoom_));
        const int xStart = qRound(std::fmod(panOffset_.x(), gridStep));
        const int yStart = qRound(std::fmod(panOffset_.y(), gridStep));
        for (int x = xStart; x < width(); x += gridStep) {
            painter.drawLine(x, 0, x, height());
        }
        for (int y = yStart; y < height(); y += gridStep) {
            painter.drawLine(0, y, width(), y);
        }

        QRectF canvas = rect().adjusted(24, 54, -24, -24);
        if (mode_ == Mode::Task) {
            drawTaskPreview(painter, canvas);
            return;
        }
        const QRectF contentCanvas = transformedCanvas(canvas);
        painter.save();
        painter.setClipRect(canvas);
        if (!grid_.isEmpty() && showGridLayer_) {
            drawGrid(painter, contentCanvas);
        }
        if (!polygons_.isEmpty() && showPolygonLayer_) {
            drawPolygons(painter, contentCanvas);
        }
        painter.restore();

        QFont title = painter.font();
        title.setPointSize(15);
        title.setBold(true);
        painter.setFont(title);
        painter.setPen(QColor("#666666"));
        const QString modeText = showGridLayer_ && showPolygonLayer_ ? uiText(QString::fromUtf8("叠加结果")) :
            (showGridLayer_ ? uiText(QString::fromUtf8("原始网格")) :
            (showPolygonLayer_ ? uiText(QString::fromUtf8("结果多边形")) : uiText(QString::fromUtf8("图层已关闭"))));
        painter.drawText(QPoint(24, 34), uiText(QString::fromUtf8("[视图区] %1")).arg(modeText));
        painter.setFont(QFont(painter.font().family(), 9));
        painter.setPen(QColor("#777777"));
        painter.drawText(QRect(width() - 280, 18, 250, 20), Qt::AlignRight, uiText(status_));
        painter.drawText(QRect(width() - 280, 38, 250, 20), Qt::AlignRight,
                         uiText(QString::fromUtf8("缩放 %1%")).arg(qRound(zoom_ * 100)));
        if (grid_.isEmpty() && polygons_.isEmpty()) {
            drawGuideCard(painter, canvas);
        } else if (!grid_.isEmpty() && polygons_.isEmpty() && showPolygonLayer_) {
            const QRectF hintRect(canvas.left() + 18, canvas.top() + 18, qMin(460.0, canvas.width() - 36), 54);
            painter.setPen(QPen(QColor("#C9A451"), 1));
            painter.setBrush(QColor(255, 250, 230, 230));
            painter.drawRoundedRect(hintRect, 3, 3);
            painter.setPen(QColor("#6B5A20"));
            painter.drawText(hintRect.adjusted(12, 8, -12, -8), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                             uiText(QString::fromUtf8("已载入原始网格，尚未运行追踪；当前不是处理后的叠加结果。")));
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (panMode_ && event->button() == Qt::LeftButton) {
            dragging_ = true;
            lastDragPos_ = event->pos();
            setCursor(Qt::ClosedHandCursor);
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (dragging_) {
            const QPointF delta = event->pos() - lastDragPos_;
            panOffset_ += delta;
            lastDragPos_ = event->pos();
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && dragging_) {
            dragging_ = false;
            setCursor(panMode_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        }
    }

    void wheelEvent(QWheelEvent* event) override {
        // 以鼠标位置为锚点缩放，局部核查时无需反复平移修正。
        const double oldZoom = zoom_;
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else if (event->angleDelta().y() < 0) {
            zoomOut();
        }
        if (!qFuzzyCompare(oldZoom, zoom_)) {
            const QPointF anchor = event->position();
            const QPointF center = QPointF(rect().center()) + panOffset_;
            const QPointF newCenter = anchor - (anchor - center) * (zoom_ / oldZoom);
            panOffset_ = newCenter - QPointF(rect().center());
            update();
        }
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            resetView();
        }
    }

private:
    void drawGuideCard(QPainter& painter, const QRectF& canvas) const {
        // 空状态三步引导：新用户不必猜第一步做什么。
        const double cardW = qMin(420.0, canvas.width() - 48.0);
        const double cardH = 132.0;
        const QRectF card(canvas.center().x() - cardW / 2.0,
                          canvas.center().y() - cardH / 2.0, cardW, cardH);
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor("#C8C8C8"), 1));
        painter.setBrush(QColor("#FFFFFF"));
        painter.drawRoundedRect(card, 3, 3);
        QFont bold = painter.font();
        bold.setPointSize(11);
        bold.setBold(true);
        painter.setFont(bold);
        painter.setPen(QColor("#005A9E"));
        painter.drawText(QRectF(card.left() + 20, card.top() + 12, card.width() - 40, 24),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         uiText(QString::fromUtf8("三步完成断层多边形追踪")));
        const QStringList steps = {
            uiText(QString::fromUtf8("打开 DAT 文件，或接入 GeoEast 数据库网格")),
            uiText(QString::fromUtf8("检查追踪参数（推荐预设已就绪）")),
            uiText(QString::fromUtf8("单击左下角「开始追踪」，随后开启结果图层")),
        };
        QFont normal = painter.font();
        normal.setPointSize(9);
        normal.setBold(false);
        painter.setFont(normal);
        for (int i = 0; i < steps.size(); ++i) {
            const double y = card.top() + 44 + i * 26;
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#DDECF8"));
            painter.drawEllipse(QRectF(card.left() + 20, y, 18, 18));
            painter.setPen(QColor("#005A9E"));
            painter.drawText(QRectF(card.left() + 20, y, 18, 18), Qt::AlignCenter,
                             QString::number(i + 1));
            painter.setPen(QColor("#444444"));
            painter.drawText(QRectF(card.left() + 46, y - 2, card.width() - 66, 22),
                             Qt::AlignLeft | Qt::AlignVCenter, steps.at(i));
        }
        painter.restore();
    }

    QColor heatColor(double value) const {
        const double t = qBound(0.0, (value - min_) / (max_ - min_), 1.0);
        const double gray = 1.0 - 0.86 * t;
        return QColor::fromRgbF(gray, gray, gray);
    }

    QRectF fittedRect(const QRectF& available) const {
        const double dataWidth = qMax(1e-9, xMax_ - xMin_);
        const double dataHeight = qMax(1e-9, yMax_ - yMin_);
        const double scale = qMin(available.width() / dataWidth, available.height() / dataHeight);
        const double w = dataWidth * scale;
        const double h = dataHeight * scale;
        return QRectF(available.center().x() - w / 2.0, available.center().y() - h / 2.0, w, h);
    }

    QRectF transformedCanvas(const QRectF& canvas) const {
        const QPointF center = canvas.center() + panOffset_;
        return QRectF(center.x() - canvas.width() * zoom_ / 2.0,
                      center.y() - canvas.height() * zoom_ / 2.0,
                      canvas.width() * zoom_,
                      canvas.height() * zoom_);
    }

    QPointF mapPoint(const QPointF& point, const QRectF& target) const {
        const double xRange = qMax(1e-9, xMax_ - xMin_);
        const double yRange = qMax(1e-9, yMax_ - yMin_);
        return QPointF(target.left() + (point.x() - xMin_) / xRange * target.width(),
                       target.bottom() - (point.y() - yMin_) / yRange * target.height());
    }

    void drawGrid(QPainter& painter, const QRectF& available) const {
        if (nx_ <= 0 || ny_ <= 0) {
            return;
        }
        if (gridImageDirty_ || gridImage_.size() != QSize(nx_, ny_)) {
            gridImage_ = QImage(nx_, ny_, QImage::Format_RGB32);
            for (int y = 0; y < ny_; ++y) {
                for (int x = 0; x < nx_; ++x) {
                    gridImage_.setPixelColor(x, ny_ - 1 - y, heatColor(grid_[y * nx_ + x]));
                }
            }
            gridImageDirty_ = false;
        }
        const QRectF target = fittedRect(available);
        // 缩小显示时平滑采样避免摩尔纹，放大时保留像元边界。
        painter.setRenderHint(QPainter::SmoothPixmapTransform, target.width() < nx_);
        painter.drawImage(target, gridImage_);
        painter.setPen(QColor("#222222"));
        painter.drawRect(target);
    }

    void drawPolygons(QPainter& painter, const QRectF& available) const {
        QColor color = color_;
        color.setAlphaF(alpha_);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(color, qMax(1.2, lineWidth_ * 42.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        const QRectF target = fittedRect(available);
        for (const QVector<QPolygonF>& polygon : polygons_) {
            for (const QPolygonF& ring : polygon) {
                QPolygonF mapped;
                for (const QPointF& point : ring) {
                    mapped << mapPoint(point, target);
                }
                painter.drawPolygon(mapped);
            }
        }
    }

    void drawTaskPreview(QPainter& painter, const QRectF& canvas) const {
        QFont titleFont = painter.font();
        titleFont.setPointSize(16);
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.setPen(QColor("#7A7A7A"));
        painter.drawText(QPoint(26, 38), uiText(QString::fromUtf8("[任务视图区] 结果查看")));

        QFont hintFont = painter.font();
        hintFont.setPointSize(10);
        hintFont.setBold(false);
        painter.setFont(hintFont);
        painter.setPen(QColor("#666666"));
        painter.drawText(rect().adjusted(26, 60, -26, -26), Qt::AlignLeft | Qt::AlignTop, taskLines_.join('\n'));

        const qreal guideWidth = qMin(440.0, qMax(310.0, canvas.width() - 100.0));
        const qreal guideHeight = languageCode_ == "en" ? 178.0 : 150.0;
        const QRectF guideRect(width() - guideWidth - 50, height() - guideHeight - 60,
                               guideWidth, guideHeight);
        if (guideRect.left() > 360 && guideRect.top() > 170) {
            painter.setPen(QPen(QColor("#C8C8C8"), 1));
            painter.setBrush(QColor("#FBFBFB"));
            painter.drawRect(guideRect);
            painter.setPen(QColor("#666666"));
            painter.drawText(guideRect.adjusted(12, 12, -12, -12),
                             Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                             uiText(QString::fromUtf8("查看逻辑：\n"
                                                       "1. 原始网格显示输入属性色图。\n"
                                                       "2. 结果多边形显示 CSV/WKT 结果文件。\n"
                                                       "3. 叠加结果用于核对断层响应贴合度。\n"
                                                       "4. 左侧复选框可独立开关图层。")));
        }

        painter.setPen(QColor("#8A8A8A"));
        painter.drawText(QRect(width() - 210, 18, 180, 20), Qt::AlignRight,
                         uiText(QString::fromUtf8("查看模式  缩放 %1%")).arg(qRound(zoom_ * 100)));
    }

    QString uiText(const QString& zh) const {
        return fpt_ui::translate(zh, languageCode_);
    }

    Mode mode_ = Mode::Task;
    QVector<double> grid_;
    QVector<QVector<QPolygonF>> polygons_;
    QString gridCachePath_;
    QString gridCacheFormat_;
    qint64 gridCacheSize_ = -1;
    qint64 gridCacheMtime_ = -1;
    QString polygonCachePath_;
    qint64 polygonCacheSize_ = -1;
    qint64 polygonCacheMtime_ = -1;
    mutable QImage gridImage_;
    mutable bool gridImageDirty_ = true;
    int nx_ = 0;
    int ny_ = 0;
    double min_ = 0.0;
    double max_ = 1.0;
    double xMin_ = 0.0;
    double yMin_ = 0.0;
    double xMax_ = 1.0;
    double yMax_ = 1.0;
    QColor color_ = QColor("#00FFFF");
    double lineWidth_ = 0.03;
    double alpha_ = 1.0;
    double zoom_ = 1.0;
    QPointF panOffset_ = QPointF(0, 0);
    QPoint lastDragPos_;
    bool panMode_ = true;
    bool dragging_ = false;
    bool showGridLayer_ = true;
    bool showPolygonLayer_ = true;
    QString status_ = QString::fromUtf8("等待数据");
    QStringList taskLines_;
    QString languageCode_ = "zh-CN";
};

class ColorBarWidget : public QWidget {
public:
    explicit ColorBarWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedWidth(78);
        setMinimumHeight(360);
    }

    void setLanguage(UiLanguage language) {
        languageCode_ = language == UiLanguage::English ? "en" : "zh-CN";
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#F0F0F0"));
        painter.setPen(QColor("#A0A0A0"));
        painter.drawLine(0, 0, 0, height());

        const QRect bar(width() / 2 - 8, 58, 16, qMax(120, height() - 124));
        QLinearGradient gradient(bar.left(), bar.bottom(), bar.left(), bar.top());
        gradient.setColorAt(0.00, QColor("#FFFFFF"));
        gradient.setColorAt(1.00, QColor("#242424"));
        painter.fillRect(bar, gradient);
        painter.setPen(QColor("#666666"));
        painter.drawRect(bar.adjusted(0, 0, -1, -1));

        painter.setPen(QColor("#333333"));
        QFont labelFont = painter.font();
        labelFont.setPointSize(8);
        painter.setFont(labelFont);
        const QString title = languageCode_ == "zh-CN" ? QString::fromUtf8("属性") : QString::fromUtf8("Attr.");
        painter.drawText(
            QRect(4, 8, width() - 8, 20),
            Qt::AlignCenter,
            title);
        painter.setPen(QColor("#333333"));
        painter.drawText(QRect(4, 34, width() - 8, 18), Qt::AlignCenter, QString::fromUtf8("1.0"));
        painter.setPen(QColor("#333333"));
        painter.drawText(QRect(4, bar.bottom() + 6, width() - 8, 18), Qt::AlignCenter, QString::fromUtf8("0.0"));
    }

private:
    QString languageCode_ = "zh-CN";
};

FaultPolyTraceWindow::FaultPolyTraceWindow(QWidget* parent)
    : QMainWindow(parent), process_(new QProcess(this)) {
    buildUi();
    loadSettings();
    refreshHistory();
    if (!isGeoEastMode()) {
        refreshAvailableResults();
    }
    updateTaskPreview();
    updateResultPanel();
    appendLog(QString::fromUtf8("[系统就绪] 等待参数输入及任务执行。"));
    connect(process_, &QProcess::readyReadStandardOutput, this, &FaultPolyTraceWindow::onReadyRead);
    connect(process_, &QProcess::readyReadStandardError, this, &FaultPolyTraceWindow::onReadyRead);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &FaultPolyTraceWindow::onFinished);
    connect(process_, &QProcess::errorOccurred, this, &FaultPolyTraceWindow::onProcessError);
}

void FaultPolyTraceWindow::buildUi() {
    setWindowTitle(QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪"));
    setMinimumSize(1100, 700);
    resize(1280, 780);
    setStyleSheet(fpt_ui::appStyle());

    QAction* openAction = new QAction(fpt_ui::toolIcon(fpt_ui::IconKind::Open), QString::fromUtf8("打开 DAT 文件"), this);
    openAction->setToolTip(QString::fromUtf8("选择外部断层属性网格文件 (Ctrl+O)"));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &FaultPolyTraceWindow::browseInput);
    runAction_ = new QAction(fpt_ui::toolIcon(fpt_ui::IconKind::Run), QString::fromUtf8("开始追踪"), this);
    runAction_->setToolTip(QString::fromUtf8("按当前参数运行断层多边形自动追踪 (F5)"));
    runAction_->setShortcut(QKeySequence(Qt::Key_F5));
    connect(runAction_, &QAction::triggered, this, &FaultPolyTraceWindow::runTrace);
    stopAction_ = new QAction(fpt_ui::toolIcon(fpt_ui::IconKind::Stop), QString::fromUtf8("停止"), this);
    stopAction_->setToolTip(QString::fromUtf8("终止当前正在运行的追踪任务 (Shift+F5)"));
    stopAction_->setEnabled(false);
    stopAction_->setShortcut(QKeySequence("Shift+F5"));
    connect(stopAction_, &QAction::triggered, this, &FaultPolyTraceWindow::stopTrace);
    QAction* pngAction = new QAction(fpt_ui::toolIcon(fpt_ui::IconKind::Image), QString::fromUtf8("导出PNG"), this);
    pngAction->setToolTip(QString::fromUtf8("导出当前视图区图像 (Ctrl+Shift+P)"));
    pngAction->setShortcut(QKeySequence("Ctrl+Shift+P"));
    connect(pngAction, &QAction::triggered, this, &FaultPolyTraceWindow::exportPng);
    QAction* saveTplAction = new QAction(fpt_ui::toolIcon(fpt_ui::IconKind::Preset), QString::fromUtf8("保存参数模板"), this);
    saveTplAction->setToolTip(QString::fromUtf8("保存当前算法参数模板 (Ctrl+S)"));
    saveTplAction->setShortcut(QKeySequence::Save);
    connect(saveTplAction, &QAction::triggered, this, &FaultPolyTraceWindow::saveTemplate);
    QAction* loadTplAction = new QAction(fpt_ui::toolIcon(fpt_ui::IconKind::Load), QString::fromUtf8("打开参数模板"), this);
    loadTplAction->setToolTip(QString::fromUtf8("读取已保存的参数模板 (Ctrl+Shift+O)"));
    loadTplAction->setShortcut(QKeySequence("Ctrl+Shift+O"));
    connect(loadTplAction, &QAction::triggered, this, &FaultPolyTraceWindow::loadTemplate);
    QAction* resetAction = new QAction(fpt_ui::toolIcon(fpt_ui::IconKind::Restore), QString::fromUtf8("恢复默认参数"), this);
    resetAction->setToolTip(QString::fromUtf8("恢复推荐默认参数"));
    connect(resetAction, &QAction::triggered, this, &FaultPolyTraceWindow::restoreDefaults);
    QAction* outputDirAction = new QAction(fpt_ui::toolIcon(fpt_ui::IconKind::Folder), QString::fromUtf8("打开输出目录"), this);
    outputDirAction->setToolTip(QString::fromUtf8("打开当前结果输出目录"));
    connect(outputDirAction, &QAction::triggered, this, &FaultPolyTraceWindow::openOutputDir);
    refreshResultsAction_ = new QAction(fpt_ui::toolIcon(fpt_ui::IconKind::Refresh), QString::fromUtf8("刷新结果列表"), this);
    refreshResultsAction_->setToolTip(QString::fromUtf8("重新读取输出目录或数据库中的已有结果 (F6)"));
    refreshResultsAction_->setShortcut(QKeySequence(Qt::Key_F6));
    connect(refreshResultsAction_, &QAction::triggered, this, &FaultPolyTraceWindow::refreshAvailableResults);
    QAction* helpAction = new QAction(fpt_ui::toolIcon(fpt_ui::IconKind::Help), QString::fromUtf8("帮助提示"), this);
    helpAction->setToolTip(QString::fromUtf8("查看独立软件使用说明"));
    connect(helpAction, &QAction::triggered, this, &FaultPolyTraceWindow::openHelp);
    QAction* userManualAction = new QAction(QString::fromUtf8("用户操作手册"), this);
    userManualAction->setToolTip(QString::fromUtf8("打开完整中文用户操作手册"));
    connect(userManualAction, &QAction::triggered, this, &FaultPolyTraceWindow::openUserManual);
    QAction* productManualAction = new QAction(QString::fromUtf8("产品说明书"), this);
    productManualAction->setToolTip(QString::fromUtf8("打开企业级用户操作手册与产品说明书"));
    connect(productManualAction, &QAction::triggered, this, &FaultPolyTraceWindow::openProductManual);
    QAction* projectStructureAction = new QAction(QString::fromUtf8("项目目录结构"), this);
    projectStructureAction->setToolTip(QString::fromUtf8("打开项目目录结构与文件作用说明"));
    connect(projectStructureAction, &QAction::triggered, this, &FaultPolyTraceWindow::openProjectStructureDoc);

    QMenu* fileMenu = menuBar()->addMenu(QString::fromUtf8("文件(&F)"));
    fileMenu->addAction(openAction);
    fileMenu->addAction(pngAction);
    fileMenu->addSeparator();
    fileMenu->addAction(saveTplAction);
    fileMenu->addAction(loadTplAction);
    fileMenu->addSeparator();
    QAction* saveLogAction = fileMenu->addAction(QString::fromUtf8("保存运行日志..."));
    saveLogAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(saveLogAction, &QAction::triggered, this, &FaultPolyTraceWindow::saveRunLog);

    QMenu* editMenu = menuBar()->addMenu(QString::fromUtf8("编辑(&E)"));
    editMenu->addAction(resetAction);
    editMenu->addSeparator();
    QAction* copySummaryAction = editMenu->addAction(QString::fromUtf8("复制当前结果摘要"));
    connect(copySummaryAction, &QAction::triggered, this, &FaultPolyTraceWindow::copyResultSummary);
    editMenu->addSeparator();
    QAction* copyLogAction = editMenu->addAction(QString::fromUtf8("复制运行日志"));
    connect(copyLogAction, &QAction::triggered, this, [this]() {
        QString logText = logEdit_->toPlainText();
        if (!pendingDebugLines_.isEmpty()) {
            logText += QString::fromUtf8("\n---- 详细日志（界面已折叠）----\n") +
                       pendingDebugLines_.join('\n');
        }
        QApplication::clipboard()->setText(logText);
        updateStatus(QString::fromUtf8("运行日志已复制到剪贴板"));
    });
    QAction* clearLogAction = editMenu->addAction(QString::fromUtf8("清空运行日志"));
    connect(clearLogAction, &QAction::triggered, this, [this]() {
        logEdit_->clear();
        pendingDebugLines_.clear();
        updateStatus(QString::fromUtf8("运行日志已清空"));
    });

    QMenu* viewMenu = menuBar()->addMenu(QString::fromUtf8("视图(&V)"));
    QAction* taskViewAction = viewMenu->addAction(QString::fromUtf8("任务预览"));
    connect(taskViewAction, &QAction::triggered, this, [this]() { setComboByKey(viewModeCombo_, QString::fromUtf8("任务预览")); });
    QAction* rawViewAction = viewMenu->addAction(QString::fromUtf8("原始网格"));
    connect(rawViewAction, &QAction::triggered, this, [this]() { setComboByKey(viewModeCombo_, QString::fromUtf8("原始网格")); });
    QAction* resultViewAction = viewMenu->addAction(QString::fromUtf8("结果多边形"));
    connect(resultViewAction, &QAction::triggered, this, [this]() { setComboByKey(viewModeCombo_, QString::fromUtf8("结果多边形")); });
    QAction* overlayViewAction = viewMenu->addAction(QString::fromUtf8("叠加结果"));
    connect(overlayViewAction, &QAction::triggered, this, [this]() { setComboByKey(viewModeCombo_, QString::fromUtf8("叠加结果")); });
    viewMenu->addSeparator();
    QAction* panAction = viewMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Pan), QString::fromUtf8("平移模式"));
    panAction->setCheckable(true);
    panAction->setChecked(true);
    connect(panAction, &QAction::toggled, this, [this](bool checked) {
        if (view_) {
            view_->setPanMode(checked);
        }
        updateStatus(checked ? QString::fromUtf8("视图区已切换为平移模式") : QString::fromUtf8("视图区已切换为选择模式"));
    });
    QAction* zoomInAction = viewMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::ZoomIn), QString::fromUtf8("放大"));
    connect(zoomInAction, &QAction::triggered, this, [this]() {
        if (view_) {
            view_->zoomIn();
        }
        updateStatus(QString::fromUtf8("视图区已放大"));
    });
    QAction* zoomOutAction = viewMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::ZoomOut), QString::fromUtf8("缩小"));
    connect(zoomOutAction, &QAction::triggered, this, [this]() {
        if (view_) {
            view_->zoomOut();
        }
        updateStatus(QString::fromUtf8("视图区已缩小"));
    });
    QAction* resetViewAction = viewMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Reset), QString::fromUtf8("重置视图"));
    resetViewAction->setShortcut(QKeySequence(Qt::Key_Home));
    connect(resetViewAction, &QAction::triggered, this, [this, panAction]() {
        if (view_) {
            view_->resetView();
        }
        panAction->setChecked(true);
        updateStatus(QString::fromUtf8("视图区已重置"));
    });

    QMenu* faultMenu = menuBar()->addMenu(QString::fromUtf8("断层(&L)"));
    faultMenu->addAction(runAction_);
    faultMenu->addAction(stopAction_);

    QMenu* layerMenu = menuBar()->addMenu(QString::fromUtf8("层位(&H)"));
    layerMenu->addAction(outputDirAction);

    QMenu* toolsMenu = menuBar()->addMenu(QString::fromUtf8("工具(&T)"));
    toolsMenu->addAction(outputDirAction);
    toolsMenu->addAction(resetAction);

    QMenu* languageMenu = menuBar()->addMenu(uiText("语言(&I)"));
    QActionGroup* languageGroup = new QActionGroup(this);
    languageGroup->setExclusive(true);
    languageChineseAction_ = languageMenu->addAction(uiText("简体中文"));
    languageChineseAction_->setCheckable(true);
    languageChineseAction_->setActionGroup(languageGroup);
    languageEnglishAction_ = languageMenu->addAction(uiText("English"));
    languageEnglishAction_->setCheckable(true);
    languageEnglishAction_->setActionGroup(languageGroup);
    connect(languageChineseAction_, &QAction::triggered, this, [this]() { setLanguage(UiLanguage::SimplifiedChinese); });
    connect(languageEnglishAction_, &QAction::triggered, this, [this]() { setLanguage(UiLanguage::English); });

    QMenu* helpMenu = menuBar()->addMenu(QString::fromUtf8("帮助(&H)"));
    helpMenu->addAction(helpAction);
    helpMenu->addAction(userManualAction);
    helpMenu->addAction(productManualAction);
    helpMenu->addAction(projectStructureAction);

    // 工具栏使用简短文案，菜单中仍保留完整名称。
    const auto setBarText = [](QAction* action, const QString& shortText, const QString& englishText) {
        if (action) {
            action->setIconText(shortText);
            action->setProperty("fptIconTextZh", shortText);
            action->setProperty("fptIconTextEn", englishText);
        }
    };
    setBarText(openAction, QString::fromUtf8("打开"), "Open");
    setBarText(saveTplAction, QString::fromUtf8("保存模板"), "Save");
    setBarText(loadTplAction, QString::fromUtf8("载入模板"), "Load");
    setBarText(panAction, QString::fromUtf8("平移"), "Pan");
    setBarText(zoomInAction, QString::fromUtf8("放大"), "Zoom In");
    setBarText(zoomOutAction, QString::fromUtf8("缩小"), "Zoom Out");
    setBarText(resetViewAction, QString::fromUtf8("重置"), "Reset");
    setBarText(runAction_, QString::fromUtf8("开始追踪"), "Trace");
    setBarText(stopAction_, QString::fromUtf8("停止"), "Stop");
    setBarText(resetAction, QString::fromUtf8("恢复默认"), "Defaults");
    setBarText(pngAction, QString::fromUtf8("导出PNG"), "PNG");
    setBarText(outputDirAction, QString::fromUtf8("输出目录"), "Output");
    setBarText(refreshResultsAction_, QString::fromUtf8("刷新结果"), "Results");
    setBarText(helpAction, QString::fromUtf8("帮助"), "Help");

    QToolBar* toolbar = addToolBar(QString::fromUtf8("工具栏"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(22, 22));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(openAction);
    toolbar->addAction(saveTplAction);
    toolbar->addAction(loadTplAction);
    toolbar->addSeparator();
    toolbar->addAction(panAction);
    toolbar->addAction(zoomInAction);
    toolbar->addAction(zoomOutAction);
    toolbar->addAction(resetViewAction);
    toolbar->addSeparator();
    toolbar->addAction(runAction_);
    toolbar->addAction(stopAction_);
    toolbar->addSeparator();
    toolbar->addAction(resetAction);
    toolbar->addAction(pngAction);
    toolbar->addSeparator();
    toolbar->addAction(outputDirAction);
    toolbar->addSeparator();
    toolbar->addAction(refreshResultsAction_);
    toolbar->addSeparator();
    toolbar->addAction(helpAction);

    QWidget* root = new QWidget(this);
    QHBoxLayout* rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QWidget* leftPane = new QWidget(root);
    leftPane->setMinimumWidth(420);
    leftPane->setMaximumWidth(760);
    leftPane->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(10, 10, 10, 8);
    leftLayout->setSpacing(8);

    QScrollArea* sidebarScroll = new QScrollArea(leftPane);
    sidebarScroll->setWidgetResizable(true);
    sidebarScroll->setFrameShape(QFrame::NoFrame);
    sidebarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget* sidebarContent = new QWidget(sidebarScroll);
    sidebarContent->setMinimumWidth(384);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebarContent);
    sidebarLayout->setContentsMargins(0, 8, 4, 0);
    sidebarLayout->setSpacing(8);

    QFrame* dataBox = new QFrame(sidebarContent);
    dataBox->setObjectName("sectionBox");
    QGridLayout* dataLayout = new QGridLayout(dataBox);
    configureFormGrid(dataLayout);
    QLabel* dataTitle = new QLabel(QString::fromUtf8("数据与输出"), dataBox);
    dataTitle->setObjectName("sectionTitle");
    dataLayout->addWidget(dataTitle, 0, 0, 1, 2);
    sourceModeCombo_ = new QComboBox(dataBox);
    sourceModeCombo_->setMinimumWidth(0);
    sourceModeCombo_->addItems({QString::fromUtf8("外部文件"), QString::fromUtf8("GeoEast数据库")});
    addRow(dataLayout, 1, QString::fromUtf8("数据源"), sourceModeCombo_, dataBox);

    sourceStack_ = new QStackedWidget(dataBox);
    QWidget* filePage = new QWidget(sourceStack_);
    QGridLayout* fileLayout = new QGridLayout(filePage);
    configureFormGrid(fileLayout);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    inputEdit_ = new QLineEdit(filePage);
    configurePathLineEdit(inputEdit_);
    inputEdit_->setToolTip(QString::fromUtf8("外部断层属性网格文件路径"));
    inputEdit_->setPlaceholderText(QString::fromUtf8("请选择平面断层属性文件"));
    QPushButton* inputButton = new QPushButton(QString::fromUtf8("浏览"), filePage);
    configureSmallButton(inputButton);
    connect(inputButton, &QPushButton::clicked, this, &FaultPolyTraceWindow::browseInput);
    QWidget* inputRow = new QWidget(filePage);
    QHBoxLayout* inputRowLayout = new QHBoxLayout(inputRow);
    inputRowLayout->setContentsMargins(0, 0, 0, 0);
    inputRowLayout->setSpacing(6);
    inputRowLayout->addWidget(inputEdit_, 1);
    inputRowLayout->addWidget(inputButton);
    addRow(fileLayout, 0, QString::fromUtf8("输入文件"), inputRow, filePage);
    inputFormatCombo_ = new QComboBox(filePage);
    inputFormatCombo_->setMinimumWidth(0);
    inputFormatCombo_->addItems({"auto", "matrix", "geoeast-dat"});
    addRow(fileLayout, 1, QString::fromUtf8("输入格式"), inputFormatCombo_, filePage);
    sourceStack_->addWidget(filePage);

    QWidget* geoEastPage = new QWidget(sourceStack_);
    QGridLayout* geoEastLayout = new QGridLayout(geoEastPage);
    configureFormGrid(geoEastLayout);
    geoEastLayout->setContentsMargins(0, 0, 0, 0);
    auto* guidedGeoEastGridCombo = new GuidedCatalogComboBox(geoEastPage);
    geoEastGridCombo_ = guidedGeoEastGridCombo;
    geoEastGridCombo_->setEditable(true);
    geoEastGridCombo_->setProperty("fptCatalogReady", false);
    if (!envValue("FPT_GEOEAST_GRID").isEmpty()) {
        geoEastGridCombo_->addItem(envValue("FPT_GEOEAST_GRID"));
        geoEastGridCombo_->setCurrentText(envValue("FPT_GEOEAST_GRID"));
    }
    if (geoEastGridCombo_->lineEdit()) {
        geoEastGridCombo_->lineEdit()->setPlaceholderText(QString::fromUtf8("请先配置项目并刷新网格"));
    }
    guidedGeoEastGridCombo->setBeforePopupHandler([this]() {
        if (geoEastGridCombo_ && geoEastGridCombo_->property("fptCatalogReady").toBool() &&
            geoEastGridCombo_->count() > 0) {
            return false;
        }
        QTimer::singleShot(0, this, [this]() {
            if (process_->state() != QProcess::NotRunning) {
                updateStatus(uiText(QString::fromUtf8("正在读取数据库，请稍候")));
                return;
            }
            if (!ensureGeoEastConfigReady(uiText(QString::fromUtf8("选择输入网格")), false, false)) {
                return;
            }
            updateStatus(uiText(QString::fromUtf8("输入网格列表为空，正在自动刷新...")));
            refreshGeoEastGrids();
        });
        return true;
    });
    addRow(geoEastLayout, 0, QString::fromUtf8("输入网格"), geoEastGridCombo_, geoEastPage);
    QWidget* geoEastActionRow = new QWidget(geoEastPage);
    QHBoxLayout* geoEastActionLayout = new QHBoxLayout(geoEastActionRow);
    geoEastActionLayout->setContentsMargins(0, 0, 0, 0);
    geoEastActionLayout->setSpacing(6);
    geoEastRefreshButton_ = new QPushButton(QString::fromUtf8("刷新网格"), geoEastActionRow);
    geoEastConfigButton_ = new QPushButton(QString::fromUtf8("数据库配置"), geoEastActionRow);
    geoEastDeleteButton_ = new QPushButton(QString::fromUtf8("删除结果"), geoEastActionRow);
    geoEastRefreshButton_->setToolTip(QString::fromUtf8("从配置的 GeoEast 数据库刷新当前项目下的输入网格列表"));
    geoEastConfigButton_->setToolTip(QString::fromUtf8("配置 GeoEast 根目录、数据根目录、数据库、项目和用户信息"));
    geoEastDeleteButton_->setToolTip(QString::fromUtf8("删除当前输出名对应的 GeoEast 结果多边形对象"));
    geoEastActionLayout->addWidget(geoEastRefreshButton_, 1);
    geoEastActionLayout->addWidget(geoEastConfigButton_, 1);
    geoEastActionLayout->addWidget(geoEastDeleteButton_, 1);
    geoEastLayout->addWidget(geoEastActionRow, 1, 0, 1, 2);
    connect(geoEastRefreshButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::refreshGeoEastGrids);
    connect(geoEastConfigButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::showGeoEastConfigDialog);
    connect(geoEastDeleteButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::deleteGeoEastResult);
    sourceStack_->addWidget(geoEastPage);
    dataLayout->addWidget(sourceStack_, 2, 0, 1, 2);

    geoEastRunnerEdit_ = new QLineEdit(geoEastRunnerPath(), dataBox);
    geoEastRootEdit_ = new QLineEdit(defaultGeoEastRoot(), dataBox);
    geoEastDataRootEdit_ = new QLineEdit(defaultGeoEastDataRoot(), dataBox);
    pgHostEdit_ = new QLineEdit(envValue("FPT_GEOEAST_PG_HOST").isEmpty() ? "127.0.0.1" : envValue("FPT_GEOEAST_PG_HOST"), dataBox);
    pgPortEdit_ = new QLineEdit(envValue("FPT_GEOEAST_PG_PORT").isEmpty() ? "5433" : envValue("FPT_GEOEAST_PG_PORT"), dataBox);
    databaseEdit_ = new QLineEdit(envValue("FPT_GEOEAST_DB").isEmpty() ? "ndp" : envValue("FPT_GEOEAST_DB"), dataBox);
    projectEdit_ = new QLineEdit(envValue("FPT_GEOEAST_PROJECT"), dataBox);
    projectEdit_->setPlaceholderText(QString::fromUtf8("当前 GeoEast 项目名，未识别时请填写"));
    userEdit_ = new QLineEdit(defaultUserName(), dataBox);
    userEdit_->setPlaceholderText(QString::fromUtf8("GeoEast 登录用户名"));
    passwordEdit_ = new QLineEdit(dataBox);
    passwordEdit_->setEchoMode(QLineEdit::Password);
    for (QLineEdit* internalConfigEdit : {
             geoEastRunnerEdit_,
             geoEastRootEdit_,
             geoEastDataRootEdit_,
             pgHostEdit_,
             pgPortEdit_,
             databaseEdit_,
             projectEdit_,
             userEdit_,
             passwordEdit_
         }) {
        internalConfigEdit->hide();
    }

    outputDirEdit_ = new QLineEdit(dataBox);
    configurePathLineEdit(outputDirEdit_);
    outputDirEdit_->setToolTip(QString::fromUtf8("结果文件输出目录"));
    QPushButton* outputButton = new QPushButton(QString::fromUtf8("浏览"), dataBox);
    configureSmallButton(outputButton);
    connect(outputButton, &QPushButton::clicked, this, &FaultPolyTraceWindow::browseOutputDir);
    QWidget* outputRow = new QWidget(dataBox);
    QHBoxLayout* outputRowLayout = new QHBoxLayout(outputRow);
    outputRowLayout->setContentsMargins(0, 0, 0, 0);
    outputRowLayout->setSpacing(6);
    outputRowLayout->addWidget(outputDirEdit_, 1);
    outputRowLayout->addWidget(outputButton);
    addRow(dataLayout, 3, QString::fromUtf8("输出目录"), outputRow, dataBox);
    outputNameEdit_ = new QLineEdit("FaultPolyTrace_Result", dataBox);
    outputNameEdit_->setMinimumWidth(0);
    addRow(dataLayout, 4, QString::fromUtf8("输出名称"), outputNameEdit_, dataBox);
    viewModeCombo_ = new QComboBox(dataBox);
    viewModeCombo_->setMinimumWidth(0);
    viewModeCombo_->addItems({QString::fromUtf8("任务预览"), QString::fromUtf8("叠加结果"), QString::fromUtf8("原始网格"), QString::fromUtf8("结果多边形")});
    addRow(dataLayout, 5, QString::fromUtf8("视图内容"), viewModeCombo_, dataBox);

    QGroupBox* styleBox = new QGroupBox(QString::fromUtf8("显示样式"), sidebarContent);
    QGridLayout* styleLayout = new QGridLayout(styleBox);
    configureFormGrid(styleLayout);
    colorEdit_ = new QLineEdit("#00FFFF", styleBox);
    configurePathLineEdit(colorEdit_);
    colorEdit_->setReadOnly(true);
    QPushButton* colorButton = new QPushButton(QString::fromUtf8("颜色"), styleBox);
    configureSmallButton(colorButton);
    connect(colorButton, &QPushButton::clicked, this, &FaultPolyTraceWindow::chooseColor);
    QWidget* colorRow = new QWidget(styleBox);
    QHBoxLayout* colorRowLayout = new QHBoxLayout(colorRow);
    colorRowLayout->setContentsMargins(0, 0, 0, 0);
    colorRowLayout->setSpacing(6);
    colorRowLayout->addWidget(colorEdit_, 1);
    colorRowLayout->addWidget(colorButton);
    addRow(styleLayout, 0, QString::fromUtf8("颜色"), colorRow, styleBox);
    lineWidthSpin_ = new QDoubleSpinBox(styleBox);
    lineWidthSpin_->setRange(0.01, 5.0);
    lineWidthSpin_->setDecimals(2);
    lineWidthSpin_->setValue(0.03);
    addRow(styleLayout, 1, QString::fromUtf8("线宽"), lineWidthSpin_, styleBox);
    alphaSpin_ = new QDoubleSpinBox(styleBox);
    alphaSpin_->setRange(0.05, 1.0);
    alphaSpin_->setDecimals(2);
    alphaSpin_->setValue(1.0);
    addRow(styleLayout, 2, QString::fromUtf8("透明度"), alphaSpin_, styleBox);
    QWidget* layerPanel = new QWidget(styleBox);
    layerPanel->setObjectName("layerPanel");
    QGridLayout* layerPanelLayout = new QGridLayout(layerPanel);
    layerPanelLayout->setContentsMargins(0, 0, 0, 0);
    layerPanelLayout->setHorizontalSpacing(8);
    layerPanelLayout->setVerticalSpacing(3);
    showGridLayerCheck_ = new fpt_ui::LayerToggle(layerPanel);
    showGridLayerCheck_->setObjectName("layerCheck");
    showGridLayerCheck_->setText(QString::fromUtf8("显示原始网格层"));
    showGridLayerCheck_->setToolTip(QString::fromUtf8("显示或隐藏输入断层属性网格。首次打开大 DAT 时会按需载入。"));
    showGridLayerCheck_->setChecked(false);
    showGridLayerCheck_->setAccentColor(QColor(QStringLiteral("#527A9F")));
    showPolygonLayerCheck_ = new fpt_ui::LayerToggle(layerPanel);
    showPolygonLayerCheck_->setObjectName("layerCheck");
    showPolygonLayerCheck_->setText(QString::fromUtf8("显示结果多边形层"));
    showPolygonLayerCheck_->setToolTip(QString::fromUtf8("显示或隐藏追踪结果多边形。没有结果时会提示先运行追踪或载入结果。"));
    showPolygonLayerCheck_->setChecked(false);
    showPolygonLayerCheck_->setAccentColor(QColor(colorEdit_->text()));
    layerHintLabel_ = new QLabel(QString::fromUtf8("同时开启两层即可叠加查看。"), layerPanel);
    layerHintLabel_->setObjectName("layerHint");
    layerHintLabel_->setWordWrap(true);
    layerPanelLayout->addWidget(showGridLayerCheck_, 0, 0, 1, 2);
    layerPanelLayout->addWidget(showPolygonLayerCheck_, 1, 0, 1, 2);
    layerPanelLayout->addWidget(layerHintLabel_, 2, 0, 1, 2);
    layerPanelLayout->setColumnStretch(0, 1);
    addRow(styleLayout, 3, QString::fromUtf8("图层"), layerPanel, styleBox);

    QGroupBox* resultBox = new QGroupBox(QString::fromUtf8("结果查看"), sidebarContent);
    QGridLayout* resultLayout = new QGridLayout(resultBox);
    resultLayout->setContentsMargins(10, 14, 10, 10);
    resultLayout->setHorizontalSpacing(6);
    resultLayout->setVerticalSpacing(6);
    resultSummaryLabel_ = new QLabel(QString::fromUtf8("尚未运行追踪任务"), resultBox);
    resultSummaryLabel_->setObjectName("resultSummary");
    resultSummaryLabel_->setWordWrap(true);
    resultPathLabel_ = new QLabel(QString::fromUtf8("结果文件将在运行完成后显示在这里。"), resultBox);
    resultPathLabel_->setObjectName("resultPath");
    resultPathLabel_->setWordWrap(true);
    resultPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    resultLayout->addWidget(resultSummaryLabel_, 0, 0, 1, 2);
    resultLayout->addWidget(resultPathLabel_, 1, 0, 1, 2);
    auto* guidedResultCombo = new GuidedCatalogComboBox(resultBox);
    resultCatalogCombo_ = guidedResultCombo;
    resultCatalogCombo_->setEditable(false);
    resultCatalogCombo_->addItem(QString::fromUtf8("请先运行追踪或刷新已有结果"), QString());
    resultCatalogCombo_->setToolTip(QString::fromUtf8("列出可用于查看或管理的已有结果。外部文件模式读取输出目录，GeoEast 模式读取当前项目中的结果多边形对象。"));
    guidedResultCombo->setBeforePopupHandler([this]() {
        bool hasAvailableResult = false;
        if (resultCatalogCombo_) {
            for (int i = 0; i < resultCatalogCombo_->count(); ++i) {
                if (!resultCatalogCombo_->itemData(i).toString().trimmed().isEmpty()) {
                    hasAvailableResult = true;
                    break;
                }
            }
        }
        if (hasAvailableResult) {
            return false;
        }
        QTimer::singleShot(0, this, [this]() {
            if (process_->state() != QProcess::NotRunning) {
                updateStatus(uiText(QString::fromUtf8("任务运行中，请稍候")));
                return;
            }
            if (isGeoEastMode()) {
                if (!ensureGeoEastConfigReady(uiText(QString::fromUtf8("选择已有结果")), false, false)) {
                    return;
                }
                updateStatus(uiText(QString::fromUtf8("已有结果列表为空，正在自动刷新...")));
                refreshAvailableResults();
                return;
            }

            refreshAvailableResults();
            if (!resultCatalogCombo_ || resultCatalogCombo_->currentData().toString().trimmed().isEmpty()) {
                QMessageBox::information(
                    this,
                    uiText(QString::fromUtf8("暂无可查看结果")),
                    uiText(QString::fromUtf8("当前输出目录中没有可查看的结果 CSV。\n\n"
                                             "请先选择输入数据并完成一次追踪，或检查“输出目录”后点击【刷新结果列表】。")));
                updateStatus(uiText(QString::fromUtf8("当前输出目录暂无结果，请先运行追踪或检查输出目录")));
            }
        });
        return true;
    });
    refreshResultsButton_ = new QPushButton(QString::fromUtf8("刷新结果列表"), resultBox);
    refreshResultsButton_->setToolTip(QString::fromUtf8("刷新已有结果列表"));
    loadSelectedResultButton_ = new QPushButton(QString::fromUtf8("显示所选结果"), resultBox);
    loadSelectedResultButton_->setObjectName("emphasisButton");
    loadSelectedResultButton_->setToolTip(QString::fromUtf8("载入当前选择的已有结果；GeoEast 数据库对象会同步为当前输出名称"));
    resultCatalogCombo_->setMinimumWidth(0);
    resultCatalogCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    configureElasticButton(refreshResultsButton_);
    configureElasticButton(loadSelectedResultButton_);
    resultLayout->addWidget(resultCatalogCombo_, 2, 0);
    resultLayout->addWidget(refreshResultsButton_, 2, 1);
    exportPngButton_ = new QPushButton(QString::fromUtf8("导出PNG"), resultBox);
    configureElasticButton(exportPngButton_);
    // V1.1：低频文件操作收进「打开文件」下拉菜单，按钮墙收敛为三个入口。
    openFilesButton_ = new QToolButton(resultBox);
    openFilesButton_->setObjectName("moreButton");
    openFilesButton_->setText(QString::fromUtf8("打开文件"));
    openFilesButton_->setPopupMode(QToolButton::InstantPopup);
    openFilesButton_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QMenu* openFilesMenu = new QMenu(openFilesButton_);
    openCsvMenuAction_ = openFilesMenu->addAction(QString::fromUtf8("结果 CSV"));
    openGeoJsonMenuAction_ = openFilesMenu->addAction(QString::fromUtf8("GeoJSON"));
    openDiagnosticsMenuAction_ = openFilesMenu->addAction(QString::fromUtf8("诊断 JSON"));
    openFilesMenu->addSeparator();
    openOutputDirMenuAction_ = openFilesMenu->addAction(QString::fromUtf8("输出目录"));
    openFilesButton_->setMenu(openFilesMenu);
    resultLayout->addWidget(loadSelectedResultButton_, 3, 0);
    resultLayout->addWidget(openFilesButton_, 3, 1);
    resultLayout->addWidget(exportPngButton_, 4, 0, 1, 2);
    resultLayout->setColumnStretch(0, 1);
    resultLayout->setColumnStretch(1, 1);
    connect(refreshResultsButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::refreshAvailableResults);
    connect(loadSelectedResultButton_, &QPushButton::clicked, this, [this]() {
        loadSelectedResultInternal(true);
    });
    connect(openCsvMenuAction_, &QAction::triggered, this, &FaultPolyTraceWindow::openResultCsv);
    connect(openGeoJsonMenuAction_, &QAction::triggered, this, &FaultPolyTraceWindow::openResultGeoJson);
    connect(openDiagnosticsMenuAction_, &QAction::triggered, this, &FaultPolyTraceWindow::openDiagnosticsJson);
    connect(openOutputDirMenuAction_, &QAction::triggered, this, &FaultPolyTraceWindow::openOutputDir);
    connect(exportPngButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::exportPng);

    QGroupBox* basicBox = new QGroupBox(QString::fromUtf8("追踪参数"), sidebarContent);
    QGridLayout* basicLayout = new QGridLayout(basicBox);
    configureFormGrid(basicLayout);
    presetCombo_ = new QComboBox(basicBox);
    presetCombo_->addItems(fpt_ui::presetKeys());
    addRow(basicLayout, 0, QString::fromUtf8("预设模板"), presetCombo_, basicBox);
    qualityModeCombo_ = new QComboBox(basicBox);
    fpt_ui::setComboItems(qualityModeCombo_, {
        QString::fromUtf8("正式处理"),
        QString::fromUtf8("质量诊断"),
        QString::fromUtf8("受控实验")
    }, languageCode());
    addRow(basicLayout, 1, QString::fromUtf8("质量模式"), qualityModeCombo_, basicBox);
    traceModeCombo_ = new QComboBox(basicBox);
    traceModeCombo_->addItems({"ridge-belt", "region"});
    addRow(basicLayout, 2, QString::fromUtf8("追踪模式"), traceModeCombo_, basicBox);
    thresholdModeCombo_ = new QComboBox(basicBox);
    thresholdModeCombo_->addItems({"percentile", "positive-percentile", "fixed"});
    addRow(basicLayout, 3, QString::fromUtf8("阈值模式"), thresholdModeCombo_, basicBox);
    percentileSpin_ = new QDoubleSpinBox(basicBox);
    percentileSpin_->setRange(0.0, 1.0);
    percentileSpin_->setDecimals(4);
    percentileSpin_->setSingleStep(0.005);
    percentileSpin_->setValue(0.98);
    addRow(basicLayout, 4, QString::fromUtf8("分位数"), percentileSpin_, basicBox);
    thresholdSpin_ = new QDoubleSpinBox(basicBox);
    thresholdSpin_->setRange(0.0, 1.0);
    thresholdSpin_->setDecimals(4);
    thresholdSpin_->setSingleStep(0.01);
    thresholdSpin_->setValue(0.95);
    addRow(basicLayout, 5, QString::fromUtf8("固定阈值"), thresholdSpin_, basicBox);
    minAreaSpin_ = new QDoubleSpinBox(basicBox);
    minAreaSpin_->setRange(0, 1000000000);
    minAreaSpin_->setDecimals(2);
    minAreaSpin_->setSingleStep(50.0);
    minAreaSpin_->setValue(120.0);
    addRow(basicLayout, 6, QString::fromUtf8("最小面积"), minAreaSpin_, basicBox);

    QGroupBox* topologyBox = new QGroupBox(QString::fromUtf8("形态与拓扑约束"), sidebarContent);
    QGridLayout* topologyLayout = new QGridLayout(topologyBox);
    configureFormGrid(topologyLayout);
    closeRadiusSpin_ = new QSpinBox(topologyBox);
    closeRadiusSpin_->setRange(0, 20);
    closeRadiusSpin_->setValue(1);
    closeRadiusSpin_->setSuffix(QString::fromUtf8(" 像元"));
    closeRadiusSpin_->setToolTip(QString::fromUtf8("形态学闭运算半径：调大可修复更宽的小断口，过大易把邻近断层连成一片"));
    addRow(topologyLayout, 0, QString::fromUtf8("闭合半径"), closeRadiusSpin_, topologyBox);
    beltRadiusSpin_ = new QSpinBox(topologyBox);
    beltRadiusSpin_->setRange(0, 20);
    beltRadiusSpin_->setValue(3);
    beltRadiusSpin_->setSuffix(QString::fromUtf8(" 像元"));
    beltRadiusSpin_->setToolTip(QString::fromUtf8("由骨架向两侧恢复断层带的半径：调大结果更宽，调小更贴中心线"));
    addRow(topologyLayout, 1, QString::fromUtf8("带宽半径"), beltRadiusSpin_, topologyBox);
    postBeltCloseRadiusSpin_ = new QSpinBox(topologyBox);
    postBeltCloseRadiusSpin_->setRange(0, 20);
    postBeltCloseRadiusSpin_->setValue(1);
    postBeltCloseRadiusSpin_->setSuffix(QString::fromUtf8(" 像元"));
    postBeltCloseRadiusSpin_->setToolTip(QString::fromUtf8("断层带成形后的闭合半径：修补带间小缝隙，0 最保守"));
    addRow(topologyLayout, 2, QString::fromUtf8("带后闭合"), postBeltCloseRadiusSpin_, topologyBox);
    attributeConstrainedBeltCheck_ = new QCheckBox(QString::fromUtf8("属性约束断层带"), topologyBox);
    attributeConstrainedBeltCheck_->setChecked(true);
    topologyLayout->addWidget(attributeConstrainedBeltCheck_, 3, 0, 1, 2);
    attributeEnvelopeRadiusSpin_ = new QSpinBox(topologyBox);
    attributeEnvelopeRadiusSpin_->setRange(0, 20);
    attributeEnvelopeRadiusSpin_->setValue(1);
    attributeEnvelopeRadiusSpin_->setSuffix(QString::fromUtf8(" 像元"));
    attributeEnvelopeRadiusSpin_->setToolTip(QString::fromUtf8("原始属性包络半径：限制断层带向背景外扩，调大更宽松"));
    addRow(topologyLayout, 4, QString::fromUtf8("属性包络半径"), attributeEnvelopeRadiusSpin_, topologyBox);
    maxGapDistanceSpin_ = new QSpinBox(topologyBox);
    maxGapDistanceSpin_->setRange(0, 1000);
    maxGapDistanceSpin_->setValue(12);
    maxGapDistanceSpin_->setSuffix(QString::fromUtf8(" 像元"));
    maxGapDistanceSpin_->setToolTip(QString::fromUtf8("断点补连的最大端点距离：调大可跨越更长缺口，过大易误连邻近断层"));
    addRow(topologyLayout, 5, QString::fromUtf8("最大断距"), maxGapDistanceSpin_, topologyBox);
    maxGapAngleSpin_ = new QDoubleSpinBox(topologyBox);
    maxGapAngleSpin_->setRange(0, 180);
    maxGapAngleSpin_->setDecimals(1);
    maxGapAngleSpin_->setValue(55.0);
    maxGapAngleSpin_->setSuffix(QString::fromUtf8(" °"));
    maxGapAngleSpin_->setToolTip(QString::fromUtf8("补连允许的最大方向差：调大能容纳更弯的断层，过大易跨脉络误连"));
    addRow(topologyLayout, 6, QString::fromUtf8("最大夹角"), maxGapAngleSpin_, topologyBox);
    smoothIterationsSpin_ = new QSpinBox(topologyBox);
    smoothIterationsSpin_->setRange(0, 5);
    smoothIterationsSpin_->setValue(1);
    smoothIterationsSpin_->setSuffix(QString::fromUtf8(" 次"));
    smoothIterationsSpin_->setToolTip(QString::fromUtf8("Chaikin 平滑次数：0 保留像元台阶，次数越多边界越圆滑"));
    addRow(topologyLayout, 7, QString::fromUtf8("平滑次数"), smoothIterationsSpin_, topologyBox);
    simplifyToleranceSpin_ = new QDoubleSpinBox(topologyBox);
    simplifyToleranceSpin_->setRange(0, 1000);
    simplifyToleranceSpin_->setDecimals(2);
    simplifyToleranceSpin_->setSingleStep(1.0);
    simplifyToleranceSpin_->setValue(2.0);
    simplifyToleranceSpin_->setSuffix(QString::fromUtf8(" 像元"));
    simplifyToleranceSpin_->setToolTip(QString::fromUtf8("Douglas-Peucker 简化容差：调大顶点更少，过大会损失形态细节"));
    addRow(topologyLayout, 8, QString::fromUtf8("简化容差"), simplifyToleranceSpin_, topologyBox);

    QGroupBox* branchBox = new QGroupBox(QString::fromUtf8("枝干细节与修补"), sidebarContent);
    QGridLayout* branchLayout = new QGridLayout(branchBox);
    configureFormGrid(branchLayout);
    weakPercentileSpin_ = new QDoubleSpinBox(branchBox);
    weakPercentileSpin_->setRange(0.0, 0.9999);
    weakPercentileSpin_->setDecimals(4);
    weakPercentileSpin_->setSingleStep(0.005);
    weakPercentileSpin_->setValue(0.965);
    addRow(branchLayout, 0, QString::fromUtf8("弱响应分位"), weakPercentileSpin_, branchBox);
    weakConnectRadiusSpin_ = new QSpinBox(branchBox);
    weakConnectRadiusSpin_->setRange(0, 20);
    weakConnectRadiusSpin_->setValue(1);
    addRow(branchLayout, 1, QString::fromUtf8("弱延伸半径"), weakConnectRadiusSpin_, branchBox);
    minBranchLengthSpin_ = new QSpinBox(branchBox);
    minBranchLengthSpin_->setRange(0, 1000);
    minBranchLengthSpin_->setValue(4);
    addRow(branchLayout, 2, QString::fromUtf8("最短分支"), minBranchLengthSpin_, branchBox);
    minJunctionBranchLengthSpin_ = new QSpinBox(branchBox);
    minJunctionBranchLengthSpin_->setRange(1, 1000);
    minJunctionBranchLengthSpin_->setValue(2);
    addRow(branchLayout, 3, QString::fromUtf8("交切短枝"), minJunctionBranchLengthSpin_, branchBox);
    minJunctionBranchMeanSpin_ = new QDoubleSpinBox(branchBox);
    minJunctionBranchMeanSpin_->setRange(0.0, 1.0);
    minJunctionBranchMeanSpin_->setDecimals(4);
    minJunctionBranchMeanSpin_->setSingleStep(0.01);
    minJunctionBranchMeanSpin_->setValue(0.94);
    addRow(branchLayout, 4, QString::fromUtf8("交切均值"), minJunctionBranchMeanSpin_, branchBox);
    residualTraceCheck_ = new QCheckBox(QString::fromUtf8("启用残余二次追踪"), branchBox);
    residualTraceCheck_->setChecked(false);
    branchLayout->addWidget(residualTraceCheck_, 5, 0, 1, 2);
    residualTraceMinLengthSpin_ = new QSpinBox(branchBox);
    residualTraceMinLengthSpin_->setRange(1, 1000);
    residualTraceMinLengthSpin_->setValue(6);
    addRow(branchLayout, 6, QString::fromUtf8("补追最短"), residualTraceMinLengthSpin_, branchBox);
    residualTraceMaxCellsSpin_ = new QSpinBox(branchBox);
    residualTraceMaxCellsSpin_->setRange(0, 1000000);
    residualTraceMaxCellsSpin_->setValue(80);
    addRow(branchLayout, 7, QString::fromUtf8("补追上限"), residualTraceMaxCellsSpin_, branchBox);

    QGroupBox* antiMergeBox = new QGroupBox(QString::fromUtf8("防黏连参数"), sidebarContent);
    QGridLayout* antiMergeLayout = new QGridLayout(antiMergeBox);
    configureFormGrid(antiMergeLayout);
    rootAntiMergeCheck_ = new QCheckBox(QString::fromUtf8("启用根系防黏连"), antiMergeBox);
    rootAntiMergeCheck_->setChecked(true);
    antiMergeLayout->addWidget(rootAntiMergeCheck_, 0, 0, 1, 2);
    rootAntiMergeRadiusSpin_ = new QSpinBox(antiMergeBox);
    rootAntiMergeRadiusSpin_->setRange(0, 10);
    rootAntiMergeRadiusSpin_->setValue(1);
    addRow(antiMergeLayout, 1, QString::fromUtf8("根系半径"), rootAntiMergeRadiusSpin_, antiMergeBox);
    rootAntiMergeSupportRatioSpin_ = new QDoubleSpinBox(antiMergeBox);
    rootAntiMergeSupportRatioSpin_->setRange(0.0, 1.0);
    rootAntiMergeSupportRatioSpin_->setDecimals(3);
    rootAntiMergeSupportRatioSpin_->setSingleStep(0.01);
    rootAntiMergeSupportRatioSpin_->setValue(0.25);
    addRow(antiMergeLayout, 2, QString::fromUtf8("支撑比例"), rootAntiMergeSupportRatioSpin_, antiMergeBox);
    rootAntiMergeMinPartCellsSpin_ = new QSpinBox(antiMergeBox);
    rootAntiMergeMinPartCellsSpin_->setRange(0, 1000000);
    rootAntiMergeMinPartCellsSpin_->setSpecialValueText(QString::fromUtf8("自动"));
    rootAntiMergeMinPartCellsSpin_->setValue(0);
    addRow(antiMergeLayout, 3, QString::fromUtf8("主体点数"), rootAntiMergeMinPartCellsSpin_, antiMergeBox);
    rootAntiMergeMinPartRatioSpin_ = new QDoubleSpinBox(antiMergeBox);
    rootAntiMergeMinPartRatioSpin_->setRange(0.0, 0.5);
    rootAntiMergeMinPartRatioSpin_->setDecimals(3);
    rootAntiMergeMinPartRatioSpin_->setSingleStep(0.01);
    rootAntiMergeMinPartRatioSpin_->setValue(0.12);
    addRow(antiMergeLayout, 4, QString::fromUtf8("主体比例"), rootAntiMergeMinPartRatioSpin_, antiMergeBox);
    rootAntiMergeWeakGuardThresholdSpin_ = new QDoubleSpinBox(antiMergeBox);
    rootAntiMergeWeakGuardThresholdSpin_->setRange(0.0, 1.0);
    rootAntiMergeWeakGuardThresholdSpin_->setDecimals(3);
    rootAntiMergeWeakGuardThresholdSpin_->setSingleStep(0.01);
    rootAntiMergeWeakGuardThresholdSpin_->setValue(0.66);
    addRow(antiMergeLayout, 5, QString::fromUtf8("弱保护阈值"), rootAntiMergeWeakGuardThresholdSpin_, antiMergeBox);
    rootAntiMergeWeakGuardRadiusSpin_ = new QSpinBox(antiMergeBox);
    rootAntiMergeWeakGuardRadiusSpin_->setRange(0, 20);
    rootAntiMergeWeakGuardRadiusSpin_->setValue(0);
    addRow(antiMergeLayout, 6, QString::fromUtf8("弱保护半径"), rootAntiMergeWeakGuardRadiusSpin_, antiMergeBox);
    localAntiMergeCheck_ = new QCheckBox(QString::fromUtf8("启用局部桥防黏连"), antiMergeBox);
    localAntiMergeCheck_->setChecked(false);
    antiMergeLayout->addWidget(localAntiMergeCheck_, 7, 0, 1, 2);
    localAntiMergeLowThresholdSpin_ = new QDoubleSpinBox(antiMergeBox);
    localAntiMergeLowThresholdSpin_->setRange(0.0, 1.0);
    localAntiMergeLowThresholdSpin_->setDecimals(4);
    localAntiMergeLowThresholdSpin_->setSingleStep(0.01);
    localAntiMergeLowThresholdSpin_->setValue(0.05);
    addRow(antiMergeLayout, 8, QString::fromUtf8("低值阈值"), localAntiMergeLowThresholdSpin_, antiMergeBox);
    localAntiMergeBridgeRadiusSpin_ = new QSpinBox(antiMergeBox);
    localAntiMergeBridgeRadiusSpin_->setRange(0, 20);
    localAntiMergeBridgeRadiusSpin_->setValue(2);
    addRow(antiMergeLayout, 9, QString::fromUtf8("桥半径"), localAntiMergeBridgeRadiusSpin_, antiMergeBox);
    localAntiMergeMinPartCellsSpin_ = new QSpinBox(antiMergeBox);
    localAntiMergeMinPartCellsSpin_->setRange(1, 1000000);
    localAntiMergeMinPartCellsSpin_->setValue(80);
    addRow(antiMergeLayout, 10, QString::fromUtf8("局部主体"), localAntiMergeMinPartCellsSpin_, antiMergeBox);
    localAntiMergeMinPartRatioSpin_ = new QDoubleSpinBox(antiMergeBox);
    localAntiMergeMinPartRatioSpin_->setRange(0.0, 0.5);
    localAntiMergeMinPartRatioSpin_->setDecimals(3);
    localAntiMergeMinPartRatioSpin_->setSingleStep(0.01);
    localAntiMergeMinPartRatioSpin_->setValue(0.08);
    addRow(antiMergeLayout, 11, QString::fromUtf8("局部比例"), localAntiMergeMinPartRatioSpin_, antiMergeBox);
    localAntiMergeMinBridgeCellsSpin_ = new QSpinBox(antiMergeBox);
    localAntiMergeMinBridgeCellsSpin_->setRange(1, 1000000);
    localAntiMergeMinBridgeCellsSpin_->setValue(6);
    addRow(antiMergeLayout, 12, QString::fromUtf8("桥点下限"), localAntiMergeMinBridgeCellsSpin_, antiMergeBox);
    localAntiMergeMinLowRatioSpin_ = new QDoubleSpinBox(antiMergeBox);
    localAntiMergeMinLowRatioSpin_->setRange(0.0, 1.0);
    localAntiMergeMinLowRatioSpin_->setDecimals(3);
    localAntiMergeMinLowRatioSpin_->setSingleStep(0.01);
    localAntiMergeMinLowRatioSpin_->setValue(0.55);
    addRow(antiMergeLayout, 13, QString::fromUtf8("低值比例"), localAntiMergeMinLowRatioSpin_, antiMergeBox);

    QGroupBox* historyBox = new QGroupBox(QString::fromUtf8("历史记录"), sidebarContent);
    QGridLayout* historyLayout = new QGridLayout(historyBox);
    configureFormGrid(historyLayout);
    historyCombo_ = new QComboBox(historyBox);
    addRow(historyLayout, 0, QString::fromUtf8("任务"), historyCombo_, historyBox);
    QPushButton* loadHistoryButton = new QPushButton(QString::fromUtf8("载入历史"), historyBox);
    connect(loadHistoryButton, &QPushButton::clicked, this, &FaultPolyTraceWindow::loadHistory);
    historyLayout->addWidget(loadHistoryButton, 1, 0, 1, 2);

    // 侧栏分区顺序与 GeoEast 插件保持一致：数据与显示样式常驻首屏，
    // 其后是追踪参数与默认折叠的高级参数，最后是结果查看与历史记录。
    sidebarLayout->addWidget(dataBox);
    sidebarLayout->addWidget(styleBox);
    sidebarLayout->addWidget(basicBox);
    sidebarLayout->addWidget(topologyBox);
    sidebarLayout->addWidget(branchBox);
    sidebarLayout->addWidget(antiMergeBox);
    sidebarLayout->addWidget(resultBox);
    sidebarLayout->addWidget(historyBox);
    sidebarLayout->addStretch(1);

    // 高级参数组默认折叠：预设模板已覆盖常规场景。
    auto makeCollapsible = [this](QGroupBox* box, bool expanded) {
        box->setCheckable(true);
        box->setChecked(expanded);
        box->setToolTip(uiText(QString::fromUtf8("单击标题旁的勾选框展开或收起高级参数")));
        auto applyVisibility = [box](bool on) {
            const QList<QWidget*> children =
                box->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
            for (QWidget* child : children) {
                child->setVisible(on);
            }
            // 收起时压到标题高度，避免留下空白分组框。
            box->setMaximumHeight(on ? QWIDGETSIZE_MAX : box->fontMetrics().height() + 9);
        };
        applyVisibility(expanded);
        connect(box, &QGroupBox::toggled, this, [this, applyVisibility](bool on) {
            applyVisibility(on);
            updateOptionControls();
        });
    };
    makeCollapsible(topologyBox, false);
    makeCollapsible(branchBox, false);
    makeCollapsible(antiMergeBox, false);
    sidebarScroll->setWidget(sidebarContent);
    leftLayout->addWidget(sidebarScroll, 1);

    runButton_ = new QPushButton(QString::fromUtf8("开始追踪"), leftPane);
    runButton_->setObjectName("runButton");
    stopButton_ = new QPushButton(QString::fromUtf8("停止"), leftPane);
    stopButton_->setObjectName("stopButton");
    stopButton_->setEnabled(false);
    connect(runButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::runTrace);
    connect(stopButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::stopTrace);
    QHBoxLayout* actionLayout = new QHBoxLayout;
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(10);
    actionLayout->addWidget(runButton_, 2);
    actionLayout->addWidget(stopButton_, 1);
    leftLayout->addLayout(actionLayout);

    QWidget* right = new QWidget(root);
    QVBoxLayout* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // V1.1：追踪完成横幅——完成后聚合“下一步”入口。
    doneBanner_ = new QFrame(right);
    doneBanner_->setObjectName("doneBanner");
    doneBanner_->hide();
    QHBoxLayout* bannerLayout = new QHBoxLayout(doneBanner_);
    bannerLayout->setContentsMargins(12, 6, 8, 6);
    bannerLayout->setSpacing(8);
    doneBannerLabel_ = new QLabel(doneBanner_);
    doneBannerLabel_->setObjectName("doneBannerText");
    bannerLayout->addWidget(doneBannerLabel_, 1);
    QPushButton* bannerPngButton = new QPushButton(uiText(QString::fromUtf8("导出PNG")), doneBanner_);
    bannerPngButton->setObjectName("linkButton");
    connect(bannerPngButton, &QPushButton::clicked, this, &FaultPolyTraceWindow::exportPng);
    bannerLayout->addWidget(bannerPngButton);
    QPushButton* bannerDirButton = new QPushButton(uiText(QString::fromUtf8("打开输出目录")), doneBanner_);
    bannerDirButton->setObjectName("linkButton");
    connect(bannerDirButton, &QPushButton::clicked, this, &FaultPolyTraceWindow::openOutputDir);
    bannerLayout->addWidget(bannerDirButton);
    QPushButton* bannerCloseButton = new QPushButton(QStringLiteral("\u2715"), doneBanner_);
    bannerCloseButton->setObjectName("linkButton");
    bannerCloseButton->setFixedWidth(24);
    connect(bannerCloseButton, &QPushButton::clicked, doneBanner_, &QWidget::hide);
    bannerLayout->addWidget(bannerCloseButton);
    rightLayout->addWidget(doneBanner_);
    QWidget* viewStrip = new QWidget(right);
    QHBoxLayout* viewStripLayout = new QHBoxLayout(viewStrip);
    viewStripLayout->setContentsMargins(0, 0, 0, 0);
    viewStripLayout->setSpacing(0);
    view_ = new AttributeView(right);
    colorBarWidget_ = new ColorBarWidget(viewStrip);
    viewStripLayout->addWidget(view_, 1);
    viewStripLayout->addWidget(colorBarWidget_);
    QWidget* logPanel = new QWidget(right);
    QVBoxLayout* logLayout = new QVBoxLayout(logPanel);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->setSpacing(0);
    QWidget* logHeaderRow = new QWidget(logPanel);
    QHBoxLayout* logHeaderLayout = new QHBoxLayout(logHeaderRow);
    logHeaderLayout->setContentsMargins(0, 0, 8, 0);
    logHeaderLayout->setSpacing(8);
    QLabel* logHeader = new QLabel(QString::fromUtf8("运行日志与输出信息"), logHeaderRow);
    logHeader->setObjectName("logHeader");
    logHeader->setFixedHeight(24);
    verboseLogCheck_ = new QCheckBox(QString::fromUtf8("详细日志"), logHeaderRow);
    verboseLogCheck_->setChecked(false);
    verboseLogCheck_->setToolTip(QString::fromUtf8("勾选后显示后台程序的原始英文输出，便于排查问题"));
    connect(verboseLogCheck_, &QCheckBox::toggled, this, [this](bool on) {
        if (on && !pendingDebugLines_.isEmpty()) {
            QString html;
            for (const QString& line : pendingDebugLines_) {
                html += fpt_ui::logLineStyle(line).html;
            }
            pendingDebugLines_.clear();
            logEdit_->moveCursor(QTextCursor::End);
            logEdit_->insertHtml(html);
            logEdit_->moveCursor(QTextCursor::End);
        }
    });
    logHeaderLayout->addWidget(logHeader, 1);
    logHeaderLayout->addWidget(verboseLogCheck_);
    logEdit_ = new QTextEdit(logPanel);
    logEdit_->document()->setMaximumBlockCount(3000);
    logEdit_->setReadOnly(true);
    logEdit_->setFixedHeight(86);
    logLayout->addWidget(logHeaderRow);
    logLayout->addWidget(logEdit_);
    rightLayout->addWidget(viewStrip, 1);
    rightLayout->addWidget(logPanel);
    right->setMinimumWidth(540);
    mainSplitter_ = new QSplitter(Qt::Horizontal, root);
    mainSplitter_->setObjectName("mainSplitter");
    mainSplitter_->setChildrenCollapsible(false);
    mainSplitter_->setHandleWidth(5);
    mainSplitter_->addWidget(leftPane);
    mainSplitter_->addWidget(right);
    mainSplitter_->setStretchFactor(0, 0);
    mainSplitter_->setStretchFactor(1, 1);
    mainSplitter_->setSizes({420, 860});
    rootLayout->addWidget(mainSplitter_, 1);
    setCentralWidget(root);

    statusLabel_ = new QLabel(QString::fromUtf8("就绪"), this);
    elapsedLabel_ = new QLabel(QString::fromUtf8("耗时: --"), this);
    statusBar()->addWidget(statusLabel_, 1);
    busyBar_ = new QProgressBar(this);
    busyBar_->setRange(0, 0);
    busyBar_->setFixedWidth(120);
    busyBar_->setTextVisible(false);
    busyBar_->hide();
    statusBar()->addPermanentWidget(busyBar_);
    statusBar()->addPermanentWidget(elapsedLabel_);

    connect(inputEdit_, &QLineEdit::editingFinished, this, [this]() {
        if (showGridLayerCheck_ && showGridLayerCheck_->isChecked()) {
            refreshInputPreview();
        } else {
            updateTaskPreview();
            updateResultPanel();
        }
    });
    connect(inputFormatCombo_, &QComboBox::currentTextChanged, this, [this]() {
        if (showGridLayerCheck_ && showGridLayerCheck_->isChecked()) {
            refreshInputPreview();
        } else {
            updateTaskPreview();
            updateResultPanel();
        }
    });
    connect(sourceModeCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::updateDataSourceMode);
    connect(viewModeCombo_, &QComboBox::currentTextChanged, this, [this]() {
        const QString viewKey = comboKey(viewModeCombo_);
        if (viewKey == QString::fromUtf8("任务预览")) {
            view_->setMode(AttributeView::Mode::Task);
        } else if (viewKey == QString::fromUtf8("原始网格")) {
            view_->setMode(AttributeView::Mode::Raw);
            showGridLayerCheck_->blockSignals(true);
            showPolygonLayerCheck_->blockSignals(true);
            showGridLayerCheck_->setChecked(true);
            showPolygonLayerCheck_->setChecked(false);
            showGridLayerCheck_->blockSignals(false);
            showPolygonLayerCheck_->blockSignals(false);
        } else if (viewKey == QString::fromUtf8("结果多边形")) {
            view_->setMode(AttributeView::Mode::Result);
            showGridLayerCheck_->blockSignals(true);
            showPolygonLayerCheck_->blockSignals(true);
            showGridLayerCheck_->setChecked(false);
            showPolygonLayerCheck_->setChecked(true);
            showGridLayerCheck_->blockSignals(false);
            showPolygonLayerCheck_->blockSignals(false);
        } else {
            view_->setMode(AttributeView::Mode::Overlay);
            showGridLayerCheck_->blockSignals(true);
            showPolygonLayerCheck_->blockSignals(true);
            showGridLayerCheck_->setChecked(true);
            showPolygonLayerCheck_->setChecked(true);
            showGridLayerCheck_->blockSignals(false);
            showPolygonLayerCheck_->blockSignals(false);
        }
        updateLayerVisibility();
        updateTaskPreview();
    });
    auto styleChanged = [this]() {
        view_->setStyle(QColor(colorEdit_->text()), lineWidthSpin_->value(), alphaSpin_->value());
        updateTaskPreview();
    };
    connect(lineWidthSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, styleChanged);
    connect(alphaSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, styleChanged);
    connect(showGridLayerCheck_, &QAbstractButton::toggled, this, &FaultPolyTraceWindow::updateLayerVisibility);
    connect(showPolygonLayerCheck_, &QAbstractButton::toggled, this, &FaultPolyTraceWindow::updateLayerVisibility);
    connect(presetCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::applyPreset);
    // V1.1：任何参数被手动改动后，模板下拉自动显示“自定义”状态。
    const auto markCustomized = [this]() {
        if (presetSyncPending_) {
            return;
        }
        presetSyncPending_ = true;
        QTimer::singleShot(0, this, [this]() {
            presetSyncPending_ = false;
            syncPresetIndicator();
        });
    };
    // 覆盖全部参数控件，含默认折叠的高级分组。
    for (QAbstractSpinBox* box : findChildren<QAbstractSpinBox*>()) {
        if (auto* dbl = qobject_cast<QDoubleSpinBox*>(box)) {
            connect(dbl, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, markCustomized);
        } else if (auto* integer = qobject_cast<QSpinBox*>(box)) {
            connect(integer, QOverload<int>::of(&QSpinBox::valueChanged), this, markCustomized);
        }
    }
    for (QComboBox* combo : {traceModeCombo_, thresholdModeCombo_}) {
        if (combo) {
            connect(combo, &QComboBox::currentTextChanged, this, markCustomized);
        }
    }
    for (QCheckBox* check : {attributeConstrainedBeltCheck_, rootAntiMergeCheck_,
                             localAntiMergeCheck_, residualTraceCheck_}) {
        if (check) {
            connect(check, &QCheckBox::toggled, this, markCustomized);
        }
    }
    connect(inputEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(inputEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::clearResultStateForNewInput);
    connect(outputDirEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(outputNameEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(outputDirEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::updateResultPanel);
    connect(outputNameEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::updateResultPanel);
    connect(outputDirEdit_, &QLineEdit::editingFinished, this, &FaultPolyTraceWindow::refreshAvailableResults);
    view_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        QAction* exportAct = menu.addAction(uiText(QString::fromUtf8("导出当前视图 PNG")));
        connect(exportAct, &QAction::triggered, this, &FaultPolyTraceWindow::exportPng);
        QAction* resetAct = menu.addAction(uiText(QString::fromUtf8("重置视图")));
        connect(resetAct, &QAction::triggered, this, [this]() { view_->resetView(); });
        QAction* dirAct = menu.addAction(uiText(QString::fromUtf8("打开输出目录")));
        connect(dirAct, &QAction::triggered, this, &FaultPolyTraceWindow::openOutputDir);
        menu.exec(view_->mapToGlobal(pos));
    });
    resultCatalogCombo_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(resultCatalogCombo_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        QAction* loadAct = menu.addAction(uiText(QString::fromUtf8("载入所选结果")));
        connect(loadAct, &QAction::triggered, this, [this]() { loadSelectedResultInternal(true); });
        QAction* refreshAct = menu.addAction(uiText(QString::fromUtf8("刷新结果列表")));
        connect(refreshAct, &QAction::triggered, this, &FaultPolyTraceWindow::refreshAvailableResults);
        menu.exec(resultCatalogCombo_->mapToGlobal(pos));
    });
    connect(resultCatalogCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        if (!resultCatalogCombo_ || resultCatalogCombo_->currentData().toString().trimmed().isEmpty()) {
            return;
        }
        // V1.1：选中即载入；图层开关只决定可见性。
        QTimer::singleShot(0, this, [this]() {
            loadSelectedResultInternal(false);
        });
    });
    connect(inputFormatCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(inputFormatCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::clearResultStateForNewInput);
    connect(presetCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(traceModeCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(thresholdModeCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(thresholdModeCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::updateOptionControls);
    connect(percentileSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(thresholdSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(minAreaSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(rootAntiMergeCheck_, &QCheckBox::toggled, this, &FaultPolyTraceWindow::updateOptionControls);
    connect(localAntiMergeCheck_, &QCheckBox::toggled, this, &FaultPolyTraceWindow::updateOptionControls);
    connect(residualTraceCheck_, &QCheckBox::toggled, this, &FaultPolyTraceWindow::updateOptionControls);
    connect(attributeConstrainedBeltCheck_, &QCheckBox::toggled, this, &FaultPolyTraceWindow::updateOptionControls);

    const QList<QSpinBox*> intParamSpins = {
        closeRadiusSpin_, weakConnectRadiusSpin_, beltRadiusSpin_, postBeltCloseRadiusSpin_,
        attributeEnvelopeRadiusSpin_, minBranchLengthSpin_, minJunctionBranchLengthSpin_,
        maxGapDistanceSpin_, rootAntiMergeRadiusSpin_, rootAntiMergeMinPartCellsSpin_,
        rootAntiMergeWeakGuardRadiusSpin_, localAntiMergeBridgeRadiusSpin_,
        localAntiMergeMinPartCellsSpin_, localAntiMergeMinBridgeCellsSpin_,
        residualTraceMinLengthSpin_, residualTraceMaxCellsSpin_, smoothIterationsSpin_
    };
    for (QSpinBox* spin : intParamSpins) {
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &FaultPolyTraceWindow::updateTaskPreview);
    }
    const QList<QDoubleSpinBox*> doubleParamSpins = {
        weakPercentileSpin_, minJunctionBranchMeanSpin_, maxGapAngleSpin_,
        rootAntiMergeSupportRatioSpin_, rootAntiMergeMinPartRatioSpin_,
        rootAntiMergeWeakGuardThresholdSpin_, localAntiMergeLowThresholdSpin_,
        localAntiMergeMinPartRatioSpin_, localAntiMergeMinLowRatioSpin_,
        simplifyToleranceSpin_
    };
    for (QDoubleSpinBox* spin : doubleParamSpins) {
        if (spin) {
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FaultPolyTraceWindow::updateTaskPreview);
        }
    }
    connect(attributeConstrainedBeltCheck_, &QCheckBox::toggled, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(rootAntiMergeCheck_, &QCheckBox::toggled, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(localAntiMergeCheck_, &QCheckBox::toggled, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(residualTraceCheck_, &QCheckBox::toggled, this, &FaultPolyTraceWindow::updateTaskPreview);
    auto* noWheelFilter = new NoWheelValueFilter(this);
    for (QAbstractSpinBox* spin : findChildren<QAbstractSpinBox*>()) {
        spin->installEventFilter(noWheelFilter);
        spin->setFocusPolicy(Qt::StrongFocus);
    }
    for (QComboBox* combo : findChildren<QComboBox*>()) {
        combo->installEventFilter(noWheelFilter);
        combo->setFocusPolicy(Qt::StrongFocus);
    }
    prepareLanguageMetadata();
    retranslateUi();
    updateOptionControls();
}

QString FaultPolyTraceWindow::coreRunnerPath() const {
    QDir app(QCoreApplication::applicationDirPath());
    const QString self = QFileInfo(QCoreApplication::applicationFilePath()).absoluteFilePath();
    const QString packagedInAppDir = app.absoluteFilePath("bin/faultpolytrace.exe");
    const QString local = app.absoluteFilePath("faultpolytrace.exe");
    QDir parent(app);
    QString packagedNextToAppDir;
    if (parent.cdUp()) {
        packagedNextToAppDir = parent.absoluteFilePath("bin/faultpolytrace.exe");
    }
    QDir root(repoRootFromAppDir());
    const QStringList candidates = {
        packagedInAppDir,
        packagedNextToAppDir,
        root.absoluteFilePath("build/faultpolytrace.exe"),
        root.absoluteFilePath("bin/faultpolytrace.exe"),
        local
    };
    for (const QString& path : candidates) {
        if (path.isEmpty()) {
            continue;
        }
        const QFileInfo info(path);
        if (info.exists() && info.absoluteFilePath().compare(self, Qt::CaseInsensitive) != 0) {
            return QDir::toNativeSeparators(path);
        }
    }
    return QDir::toNativeSeparators(root.absoluteFilePath("build/faultpolytrace.exe"));
}

QString FaultPolyTraceWindow::defaultInputPath() const {
    QDir app(QCoreApplication::applicationDirPath());
    const QString packagedInAppDir = app.absoluteFilePath("examples/synthetic/broken_ridge_fault_grid.txt");
    QDir parent(app);
    QString packagedNextToAppDir;
    if (parent.cdUp()) {
        packagedNextToAppDir = parent.absoluteFilePath("examples/synthetic/broken_ridge_fault_grid.txt");
    }
    QDir root(repoRootFromAppDir());
    for (const QString& path : {
             packagedInAppDir,
             packagedNextToAppDir,
             root.absoluteFilePath("examples/synthetic/broken_ridge_fault_grid.txt"),
             root.absoluteFilePath("examples/synthetic/ridge_fault_grid.txt")
         }) {
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            return QDir::toNativeSeparators(path);
        }
    }
    return QString();
}

QString FaultPolyTraceWindow::defaultOutputDir() const {
    QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (docs.isEmpty()) {
        docs = QCoreApplication::applicationDirPath();
    }
    const QString dir = QDir(docs).absoluteFilePath("FaultPolyTraceOutputs");
    QDir().mkpath(dir);
    return QDir::toNativeSeparators(dir);
}

QString FaultPolyTraceWindow::settingsPath() const {
    return QDir(appConfigDir()).absoluteFilePath("desktop.ini");
}

QString FaultPolyTraceWindow::historyPath() const {
    const QString dir = QDir(appConfigDir()).absoluteFilePath("history");
    QDir().mkpath(dir);
    return QDir(dir).absoluteFilePath("records.tsv");
}

QString FaultPolyTraceWindow::locateBundledFile(const QString& relativePath) const {
    const QString normalized = QDir::fromNativeSeparators(relativePath);
    QDir app(QCoreApplication::applicationDirPath());
    QDir parent(app);
    const QString parentPath = parent.cdUp() ? parent.absoluteFilePath(normalized) : QString();
    const QStringList candidates = {
        app.absoluteFilePath(normalized),
        parentPath,
        app.absoluteFilePath("FaultPolyTraceLauncher/" + normalized),
        QDir(repoRootFromAppDir()).absoluteFilePath(normalized),
        QDir(QDir::currentPath()).absoluteFilePath(normalized)
    };
    for (const QString& candidate : candidates) {
        if (!candidate.isEmpty() && QFileInfo::exists(candidate)) {
            return QDir::toNativeSeparators(candidate);
        }
    }
    return QString();
}

bool FaultPolyTraceWindow::openBundledDocument(const QString& relativePath, const QString& displayName) {
    const QString path = locateBundledFile(relativePath);
    if (path.isEmpty()) {
        QMessageBox::warning(
            this,
            uiText(QString::fromUtf8("文档未找到")),
            uiText(QString::fromUtf8("未找到【%1】。\n\n请确认当前发布包包含 docs 目录，或重新运行发布包/GeoEast 部署脚本。")).arg(displayName));
        appendLog(uiText(QString::fromUtf8("文档未找到：%1")).arg(relativePath));
        updateStatus(QString::fromUtf8("文档未找到，请检查发布包 docs 目录"));
        return false;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        QMessageBox::warning(
            this,
            uiText(QString::fromUtf8("无法打开文档")),
            uiText(QString::fromUtf8("系统未能打开【%1】。\n\n文件位置：%2\n\n请确认系统已关联 Markdown、PDF 或 Word 文档查看程序。")).arg(displayName, path));
        appendLog(uiText(QString::fromUtf8("无法打开文档：%1")).arg(path));
        updateStatus(QString::fromUtf8("无法打开文档，请检查系统文件关联"));
        return false;
    }
    updateStatus(uiText(QString::fromUtf8("已打开文档：%1")).arg(displayName));
    return true;
}

QString FaultPolyTraceWindow::outputBasePath() const {
    QString name = outputNameEdit_->text().trimmed();
    if (name.isEmpty()) {
        name = "FaultPolyTrace_Result";
    }
    if (name.endsWith(".csv", Qt::CaseInsensitive)) {
        name.chop(4);
    }
    QString dir = outputDirEdit_->text().trimmed();
    if (dir.isEmpty()) {
        dir = defaultOutputDir();
    }
    QDir().mkpath(dir);
    return QDir(dir).absoluteFilePath(name);
}

QString FaultPolyTraceWindow::outputCsvPath() const {
    return outputBasePath() + ".csv";
}

QString FaultPolyTraceWindow::defaultOfficialDatPath() const {
    return firstExistingPath({
        envValue("FPT_OFFICIAL_DAT"),
        envValue("FPT_INPUT_DAT")
    });
}

QString FaultPolyTraceWindow::geoEastRunnerPath() const {
    const QString fromEnv = envValue("FPT_GEOEAST_RUNNER");
    if (!fromEnv.isEmpty()) {
        return QDir::toNativeSeparators(fromEnv);
    }
    QDir app(QCoreApplication::applicationDirPath());
    QDir parent(app);
    QString parentBin;
    if (parent.cdUp()) {
        parentBin = parent.absoluteFilePath("bin/faultpolytrace_geoeast_runner.exe");
    }
    const QString root = defaultGeoEastRoot();
    const QStringList candidates = {
        app.absoluteFilePath("bin/faultpolytrace_geoeast_runner.exe"),
        app.absoluteFilePath("faultpolytrace_geoeast_runner.exe"),
        parentBin,
        QDir(repoRootFromAppDir()).absoluteFilePath("geoeast_plugin/bo_runner/faultpolytrace_geoeast_runner.exe"),
        root.isEmpty() ? QString() : QDir(root).absoluteFilePath("GeoEast-RC/bin/faultpolytrace_geoeast_runner.exe")
    };
    for (const QString& path : candidates) {
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            return QDir::toNativeSeparators(path);
        }
    }
    return QString();
}

QString FaultPolyTraceWindow::defaultGeoEastRoot() const {
    return detectGeoEastRoot();
}

QString FaultPolyTraceWindow::defaultGeoEastDataRoot() const {
    return detectGeoEastDataRoot(defaultGeoEastRoot());
}

bool FaultPolyTraceWindow::isGeoEastMode() const {
    return sourceModeCombo_ && comboKey(sourceModeCombo_) == QString::fromUtf8("GeoEast数据库");
}

QString FaultPolyTraceWindow::geoEastOutputName() const {
    QString name = outputNameEdit_->text().trimmed();
    if (name.isEmpty()) {
        name = "FaultPolyTrace_Result";
    }
    if (name.endsWith(".csv", Qt::CaseInsensitive)) {
        name.chop(4);
    }
    return name;
}

QStringList FaultPolyTraceWindow::geoEastConnectionArgs() const {
    QStringList args;
    args << "--db" << databaseEdit_->text().trimmed()
         << "--project" << projectEdit_->text().trimmed()
         << "--user" << userEdit_->text().trimmed();
    if (!passwordEdit_->text().isEmpty()) {
        args << "--password" << passwordEdit_->text();
    }
    return args;
}

QStringList FaultPolyTraceWindow::geoEastTraceArgs() const {
    QStringList args = geoEastConnectionArgs();
    args << "--grid" << geoEastGridCombo_->currentText().trimmed()
         << "--output" << geoEastOutputName();
    args << fpt_ui::traceArguments(currentTraceParameters());
    args << "--polygon-color" << colorEdit_->text()
         << "--polygon-linewidth" << QString::number(lineWidthSpin_->value(), 'g', 12)
         << "--view-grid-output" << outputBasePath() + ".grid.txt"
         << "--view-polygons-output" << outputCsvPath();
    return args;
}

void FaultPolyTraceWindow::configureGeoEastEnvironment(QProcessEnvironment& environment) const {
    const QString geoEastRoot = QDir::fromNativeSeparators(geoEastRootEdit_->text().trimmed());
    const QString ngp = geoEastRoot + "/NGP";
    const QString geoeast = geoEastRoot + "/GeoEast-RC";
    const QString pgHome = ngp + "/support/PostgreSQL/product";
    const QString dataRoot = QDir::fromNativeSeparators(geoEastDataRootEdit_->text().trimmed());
    const QString database = databaseEdit_->text().trimmed();

    environment.insert("NGP", QDir::toNativeSeparators(ngp));
    environment.insert("IECO", QDir::toNativeSeparators(ngp));
    environment.insert("NDPDIR", QDir::toNativeSeparators(ngp));
    environment.insert("NGPDIR", QDir::toNativeSeparators(ngp));
    environment.insert("GEOEAST", QDir::toNativeSeparators(geoeast));
    environment.insert("PG_HOME", QDir::toNativeSeparators(pgHome));
    environment.insert("NGP_DBDATA_PATH", QDir::toNativeSeparators(dataRoot + "/pgdata/" + database));
    environment.insert("NGPDATA", QDir::toNativeSeparators(dataRoot + "/DATA/data"));

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
    QString path = QDir::toNativeSeparators(pathItems.join(';'));
    const QString existingPath = environment.value("PATH");
    if (!existingPath.isEmpty()) {
        path += ';' + existingPath;
    }
    environment.insert("PATH", path);

    const QString networkDir = pgHome + "/network";
    QDir().mkpath(networkDir);
    QFile hostFile(networkDir + "/hostname.pg");
    if (hostFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        const QByteArray line = database.toLocal8Bit() + "=" +
            pgHostEdit_->text().trimmed().toLocal8Bit() + ":" +
            pgPortEdit_->text().trimmed().toLocal8Bit() + "\n";
        hostFile.write(line);
    }
}

void FaultPolyTraceWindow::startProcess(ProcessTask task, const QString& runner, const QStringList& args, const QString& statusText) {
    logEdit_->clear();
    processOutput_.clear();
    currentTask_ = task;
    appendLog("> " + runner + " " + quoteForLog(args));
    updateStatus(statusText);
    elapsedLabel_->setText(uiText(QString::fromUtf8("耗时: 计算中")));
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (task == ProcessTask::GeoEastTrace ||
        task == ProcessTask::RefreshGeoEastGrids ||
        task == ProcessTask::RefreshGeoEastResults ||
        task == ProcessTask::ExportGeoEastResult ||
        task == ProcessTask::DeleteGeoEastResult) {
        configureGeoEastEnvironment(environment);
    }
    process_->setProcessEnvironment(environment);
    setRunning(true);
    processStartMs_ = QDateTime::currentMSecsSinceEpoch();
    process_->start(runner, args);
}

QString FaultPolyTraceWindow::languageCode() const {
    return currentLanguage_ == UiLanguage::English ? "en" : "zh-CN";
}

QString FaultPolyTraceWindow::uiText(const char* zh) const {
    return uiText(QString::fromUtf8(zh));
}

QString FaultPolyTraceWindow::uiText(const QString& zh) const {
    return fpt_ui::translate(zh, languageCode());
}

void FaultPolyTraceWindow::setComboByKey(QComboBox* combo, const QString& key) {
    fpt_ui::setComboByKey(combo, key);
}

QString FaultPolyTraceWindow::comboKey(const QComboBox* combo) const {
    return fpt_ui::comboCurrentKey(combo);
}

void FaultPolyTraceWindow::prepareLanguageMetadata() {
    fpt_ui::captureTranslatableText(this);
    fpt_ui::setComboItems(sourceModeCombo_, {
        QString::fromUtf8("外部文件"),
        QString::fromUtf8("GeoEast数据库")
    }, languageCode());
    fpt_ui::setComboItems(viewModeCombo_, {
        QString::fromUtf8("任务预览"),
        QString::fromUtf8("叠加结果"),
        QString::fromUtf8("原始网格"),
        QString::fromUtf8("结果多边形")
    }, languageCode());
    fpt_ui::setComboItems(presetCombo_, {
        QString::fromUtf8("全覆盖优先"),
        QString::fromUtf8("贴合融合推荐"),
        QString::fromUtf8("防黏连增强"),
        QString::fromUtf8("均衡推荐"),
        QString::fromUtf8("保守高精度"),
        QString::fromUtf8("弱响应高召回"),
        QString::fromUtf8("宽断层带"),
        QString::fromUtf8("细碎断裂")
    }, languageCode());
}

void FaultPolyTraceWindow::setLanguage(UiLanguage language) {
    if (currentLanguage_ == language) {
        retranslateUi();
        return;
    }
    currentLanguage_ = language;
    retranslateUi();
    saveSettings();
    updateStatus(uiText(QString::fromUtf8("界面语言已切换")));
}

void FaultPolyTraceWindow::retranslateUi() {
    const QString translatedTitle = uiText("FaultPolyTrace 断层多边形自动追踪");
    QGuiApplication::setApplicationDisplayName(translatedTitle);
    setWindowTitle(translatedTitle);
    fpt_ui::applyTranslatableText(this, languageCode());
    fpt_ui::refreshComboItems(sourceModeCombo_, languageCode());
    fpt_ui::refreshComboItems(viewModeCombo_, languageCode());
    fpt_ui::refreshComboItems(presetCombo_, languageCode());
    fpt_ui::refreshComboItems(qualityModeCombo_, languageCode());
    fpt_ui::refreshComboItems(resultCatalogCombo_, languageCode());
    if (languageChineseAction_) {
        languageChineseAction_->setText(uiText("简体中文"));
        languageChineseAction_->setChecked(currentLanguage_ == UiLanguage::SimplifiedChinese);
    }
    if (languageEnglishAction_) {
        languageEnglishAction_->setText(uiText("English"));
        languageEnglishAction_->setChecked(currentLanguage_ == UiLanguage::English);
    }
    if (layerHintLabel_) {
        layerHintLabel_->setText(uiText(QString::fromUtf8("同时开启两层即可叠加查看。")));
    }
    if (showGridLayerCheck_) {
        showGridLayerCheck_->setText(uiText(QString::fromUtf8("显示原始网格层")));
        showGridLayerCheck_->setToolTip(uiText(QString::fromUtf8("显示或隐藏输入断层属性网格。首次打开大 DAT 时会按需载入。")));
    }
    if (showPolygonLayerCheck_) {
        showPolygonLayerCheck_->setText(uiText(QString::fromUtf8("显示结果多边形层")));
        showPolygonLayerCheck_->setToolTip(uiText(QString::fromUtf8("显示或隐藏追踪结果多边形。没有结果时会提示先运行追踪或载入结果。")));
    }
    if (rootAntiMergeMinPartCellsSpin_) {
        rootAntiMergeMinPartCellsSpin_->setSpecialValueText(uiText("自动"));
    }
    if (view_) {
        view_->setLanguage(currentLanguage_);
    }
    if (colorBarWidget_) {
        static_cast<ColorBarWidget*>(colorBarWidget_)->setLanguage(currentLanguage_);
    }
    if (logEdit_ && !logEdit_->toPlainText().isEmpty()) {
        QStringList sourceLines;
        for (const QString& line : logEdit_->toPlainText().split('\n')) {
            sourceLines << fpt_ui::canonical(line);
        }
        logEdit_->clear();
        for (const QString& line : sourceLines) {
            appendLog(line);
        }
    }
    updateTaskPreview();
    updateResultPanel();
    if (!isGeoEastMode()) {
        refreshAvailableResults();
    }
}

void FaultPolyTraceWindow::loadSettings() {
    QSettings settings(settingsPath(), QSettings::IniFormat);
    const int schemaVersion = settings.value("configSchemaVersion", 0).toInt();
    currentLanguage_ = fpt_ui::normalizeLanguageCode(settings.value("language", "zh-CN").toString()) == "en"
        ? UiLanguage::English
        : UiLanguage::SimplifiedChinese;
    setComboByKey(sourceModeCombo_, settings.value("sourceMode", comboKey(sourceModeCombo_)).toString());
    const QString savedInput = settingOrCurrent(settings, "input", QString(), schemaVersion);
    const QString bundledSampleInput = defaultInputPath();
    const bool legacySampleInput = schemaVersion < 3 &&
        !savedInput.isEmpty() &&
        !bundledSampleInput.isEmpty() &&
        QFileInfo(savedInput).absoluteFilePath().compare(QFileInfo(bundledSampleInput).absoluteFilePath(), Qt::CaseInsensitive) == 0;
    inputEdit_->setText(!legacySampleInput && QFileInfo::exists(savedInput) ? savedInput : QString());
    outputDirEdit_->setText(settings.value("outputDir", defaultOutputDir()).toString());
    outputNameEdit_->setText(settings.value("outputName", outputNameEdit_->text()).toString());
    inputFormatCombo_->setCurrentText(settings.value("inputFormat", "auto").toString());
    setComboByKey(viewModeCombo_, settings.value("viewMode", QString::fromUtf8("任务预览")).toString());
    setComboByKey(presetCombo_, settings.value("preset", comboKey(presetCombo_)).toString());
    setComboByKey(qualityModeCombo_, settings.value("qualityMode", QString::fromUtf8("正式处理")).toString());
    traceModeCombo_->setCurrentText(settings.value("traceMode", traceModeCombo_->currentText()).toString());
    thresholdModeCombo_->setCurrentText(settings.value("thresholdMode", thresholdModeCombo_->currentText()).toString());
    percentileSpin_->setValue(settings.value("percentile", percentileSpin_->value()).toDouble());
    thresholdSpin_->setValue(settings.value("threshold", thresholdSpin_->value()).toDouble());
    minAreaSpin_->setValue(settings.value("minArea", minAreaSpin_->value()).toDouble());
    closeRadiusSpin_->setValue(settings.value("closeRadius", closeRadiusSpin_->value()).toInt());
    weakPercentileSpin_->setValue(settings.value("weakPercentile", weakPercentileSpin_->value()).toDouble());
    weakConnectRadiusSpin_->setValue(settings.value("weakConnectRadius", weakConnectRadiusSpin_->value()).toInt());
    beltRadiusSpin_->setValue(settings.value("beltRadius", beltRadiusSpin_->value()).toInt());
    postBeltCloseRadiusSpin_->setValue(settings.value("postBeltCloseRadius", postBeltCloseRadiusSpin_->value()).toInt());
    attributeConstrainedBeltCheck_->setChecked(settings.value("attributeConstrainedBelt", attributeConstrainedBeltCheck_->isChecked()).toBool());
    attributeEnvelopeRadiusSpin_->setValue(settings.value("attributeEnvelopeRadius", attributeEnvelopeRadiusSpin_->value()).toInt());
    minBranchLengthSpin_->setValue(settings.value("minBranchLength", minBranchLengthSpin_->value()).toInt());
    minJunctionBranchLengthSpin_->setValue(settings.value("minJunctionBranchLength", minJunctionBranchLengthSpin_->value()).toInt());
    minJunctionBranchMeanSpin_->setValue(settings.value("minJunctionBranchMean", minJunctionBranchMeanSpin_->value()).toDouble());
    maxGapDistanceSpin_->setValue(settings.value("maxGapDistance", maxGapDistanceSpin_->value()).toInt());
    maxGapAngleSpin_->setValue(settings.value("maxGapAngle", maxGapAngleSpin_->value()).toDouble());
    rootAntiMergeCheck_->setChecked(settings.value("rootAntiMerge", rootAntiMergeCheck_->isChecked()).toBool());
    rootAntiMergeRadiusSpin_->setValue(settings.value("rootAntiMergeRadius", rootAntiMergeRadiusSpin_->value()).toInt());
    rootAntiMergeSupportRatioSpin_->setValue(settings.value("rootAntiMergeSupportRatio", rootAntiMergeSupportRatioSpin_->value()).toDouble());
    rootAntiMergeMinPartCellsSpin_->setValue(settings.value("rootAntiMergeMinPartCells", rootAntiMergeMinPartCellsSpin_->value()).toInt());
    rootAntiMergeMinPartRatioSpin_->setValue(settings.value("rootAntiMergeMinPartRatio", rootAntiMergeMinPartRatioSpin_->value()).toDouble());
    rootAntiMergeWeakGuardThresholdSpin_->setValue(settings.value("rootAntiMergeWeakGuardThreshold", rootAntiMergeWeakGuardThresholdSpin_->value()).toDouble());
    rootAntiMergeWeakGuardRadiusSpin_->setValue(settings.value("rootAntiMergeWeakGuardRadius", rootAntiMergeWeakGuardRadiusSpin_->value()).toInt());
    localAntiMergeCheck_->setChecked(settings.value("localAntiMerge", localAntiMergeCheck_->isChecked()).toBool());
    localAntiMergeLowThresholdSpin_->setValue(settings.value("localAntiMergeLowThreshold", localAntiMergeLowThresholdSpin_->value()).toDouble());
    localAntiMergeBridgeRadiusSpin_->setValue(settings.value("localAntiMergeBridgeRadius", localAntiMergeBridgeRadiusSpin_->value()).toInt());
    localAntiMergeMinPartCellsSpin_->setValue(settings.value("localAntiMergeMinPartCells", localAntiMergeMinPartCellsSpin_->value()).toInt());
    localAntiMergeMinPartRatioSpin_->setValue(settings.value("localAntiMergeMinPartRatio", localAntiMergeMinPartRatioSpin_->value()).toDouble());
    localAntiMergeMinBridgeCellsSpin_->setValue(settings.value("localAntiMergeMinBridgeCells", localAntiMergeMinBridgeCellsSpin_->value()).toInt());
    localAntiMergeMinLowRatioSpin_->setValue(settings.value("localAntiMergeMinLowRatio", localAntiMergeMinLowRatioSpin_->value()).toDouble());
    residualTraceCheck_->setChecked(settings.value("residualTrace", residualTraceCheck_->isChecked()).toBool());
    residualTraceMinLengthSpin_->setValue(settings.value("residualTraceMinLength", residualTraceMinLengthSpin_->value()).toInt());
    residualTraceMaxCellsSpin_->setValue(settings.value("residualTraceMaxCells", residualTraceMaxCellsSpin_->value()).toInt());
    smoothIterationsSpin_->setValue(settings.value("smoothIterations", smoothIterationsSpin_->value()).toInt());
    simplifyToleranceSpin_->setValue(settings.value("simplifyTolerance", simplifyToleranceSpin_->value()).toDouble());
    setPolygonColor(QColor(settings.value("color", colorEdit_->text()).toString()));
    lineWidthSpin_->setValue(settings.value("lineWidth", lineWidthSpin_->value()).toDouble());
    alphaSpin_->setValue(settings.value("alpha", alphaSpin_->value()).toDouble());
    if (showGridLayerCheck_) {
        showGridLayerCheck_->setChecked(false);
    }
    if (showPolygonLayerCheck_) {
        showPolygonLayerCheck_->setChecked(false);
    }
    const QString savedRunner = settingOrCurrent(settings, "geoEastRunner", geoEastRunnerEdit_->text(), schemaVersion).trimmed();
    geoEastRunnerEdit_->setText(QFileInfo::exists(savedRunner) ? savedRunner : geoEastRunnerPath());
    const QString savedRoot = settingOrCurrent(settings, "geoEastRoot", geoEastRootEdit_->text(), schemaVersion).trimmed();
    const QString effectiveRoot = QDir(savedRoot).exists() ? savedRoot : defaultGeoEastRoot();
    geoEastRootEdit_->setText(effectiveRoot);
    const QString savedDataRoot = settingOrCurrent(settings, "geoEastDataRoot", geoEastDataRootEdit_->text(), schemaVersion).trimmed();
    geoEastDataRootEdit_->setText(QDir(savedDataRoot).exists() ? savedDataRoot : detectGeoEastDataRoot(effectiveRoot));
    pgHostEdit_->setText(settings.value("pgHost", pgHostEdit_->text()).toString());
    pgPortEdit_->setText(settings.value("pgPort", pgPortEdit_->text()).toString());
    databaseEdit_->setText(settings.value("database", databaseEdit_->text()).toString());
    projectEdit_->setText(settingOrCurrent(settings, "project", projectEdit_->text(), schemaVersion));
    userEdit_->setText(settingOrCurrent(settings, "user", userEdit_->text(), schemaVersion));
    passwordEdit_->clear();
    settings.remove("password");
    pendingGeoEastGridRestore_ = settingOrCurrent(settings, "geoEastGrid", geoEastGridCombo_->currentText(), schemaVersion).trimmed();
    if (pendingGeoEastGridRestore_.startsWith(QString::fromUtf8("请先")) ||
        pendingGeoEastGridRestore_.startsWith(QStringLiteral("Configure"))) {
        pendingGeoEastGridRestore_.clear();
    }
    geoEastGridCombo_->clear();
    geoEastGridCombo_->setCurrentText(QString());
    geoEastGridCombo_->setProperty("fptCatalogReady", false);
    lastOutputCsv_ = settings.value("lastOutputCsv").toString();
    lastGeoEastOutput_ = settings.value("lastGeoEastOutput").toString();
    if (!isGeoEastMode() && (inputEdit_->text().trimmed().isEmpty() || !QFileInfo::exists(lastOutputCsv_))) {
        lastOutputCsv_.clear();
    }
    if (showGridLayerCheck_ && showPolygonLayerCheck_ &&
        !showGridLayerCheck_->isChecked() && !showPolygonLayerCheck_->isChecked()) {
        setComboByKey(viewModeCombo_, QString::fromUtf8("任务预览"));
    } else if (lastOutputCsv_.isEmpty()) {
        if (inputEdit_->text().trimmed().isEmpty()) {
            setComboByKey(viewModeCombo_, QString::fromUtf8("任务预览"));
        } else if (comboKey(viewModeCombo_) == QString::fromUtf8("叠加结果") ||
                   comboKey(viewModeCombo_) == QString::fromUtf8("结果多边形")) {
            setComboByKey(viewModeCombo_, QString::fromUtf8("原始网格"));
        }
    }
    inputEdit_->setCursorPosition(0);
    outputDirEdit_->setCursorPosition(0);
    inputEdit_->setToolTip(inputEdit_->text());
    outputDirEdit_->setToolTip(outputDirEdit_->text());
    if (mainSplitter_) {
        const QByteArray splitterState = settings.value("mainSplitterState").toByteArray();
        if (!splitterState.isEmpty()) {
            mainSplitter_->restoreState(splitterState);
        }
        QList<int> sizes = mainSplitter_->sizes();
        if (sizes.size() >= 2 && sizes.at(0) < 420) {
            mainSplitter_->setSizes({420, qMax(620, sizes.at(1))});
        }
    }
    updateOptionControls();
    updateDataSourceMode();
    updateLayerVisibility();
    retranslateUi();
}

void FaultPolyTraceWindow::saveSettings() const {
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue("configSchemaVersion", 4);
    settings.setValue("language", languageCode());
    settings.setValue("sourceMode", comboKey(sourceModeCombo_));
    settings.setValue("input", inputEdit_->text());
    settings.setValue("outputDir", outputDirEdit_->text());
    settings.setValue("outputName", outputNameEdit_->text());
    settings.setValue("inputFormat", inputFormatCombo_->currentText());
    settings.setValue("viewMode", comboKey(viewModeCombo_));
    settings.setValue("preset", comboKey(presetCombo_));
    settings.setValue("qualityMode", comboKey(qualityModeCombo_));
    settings.setValue("traceMode", traceModeCombo_->currentText());
    settings.setValue("thresholdMode", thresholdModeCombo_->currentText());
    settings.setValue("percentile", percentileSpin_->value());
    settings.setValue("threshold", thresholdSpin_->value());
    settings.setValue("minArea", minAreaSpin_->value());
    settings.setValue("closeRadius", closeRadiusSpin_->value());
    settings.setValue("weakPercentile", weakPercentileSpin_->value());
    settings.setValue("weakConnectRadius", weakConnectRadiusSpin_->value());
    settings.setValue("beltRadius", beltRadiusSpin_->value());
    settings.setValue("postBeltCloseRadius", postBeltCloseRadiusSpin_->value());
    settings.setValue("attributeConstrainedBelt", attributeConstrainedBeltCheck_->isChecked());
    settings.setValue("attributeEnvelopeRadius", attributeEnvelopeRadiusSpin_->value());
    settings.setValue("minBranchLength", minBranchLengthSpin_->value());
    settings.setValue("minJunctionBranchLength", minJunctionBranchLengthSpin_->value());
    settings.setValue("minJunctionBranchMean", minJunctionBranchMeanSpin_->value());
    settings.setValue("maxGapDistance", maxGapDistanceSpin_->value());
    settings.setValue("maxGapAngle", maxGapAngleSpin_->value());
    settings.setValue("rootAntiMerge", rootAntiMergeCheck_->isChecked());
    settings.setValue("rootAntiMergeRadius", rootAntiMergeRadiusSpin_->value());
    settings.setValue("rootAntiMergeSupportRatio", rootAntiMergeSupportRatioSpin_->value());
    settings.setValue("rootAntiMergeMinPartCells", rootAntiMergeMinPartCellsSpin_->value());
    settings.setValue("rootAntiMergeMinPartRatio", rootAntiMergeMinPartRatioSpin_->value());
    settings.setValue("rootAntiMergeWeakGuardThreshold", rootAntiMergeWeakGuardThresholdSpin_->value());
    settings.setValue("rootAntiMergeWeakGuardRadius", rootAntiMergeWeakGuardRadiusSpin_->value());
    settings.setValue("localAntiMerge", localAntiMergeCheck_->isChecked());
    settings.setValue("localAntiMergeLowThreshold", localAntiMergeLowThresholdSpin_->value());
    settings.setValue("localAntiMergeBridgeRadius", localAntiMergeBridgeRadiusSpin_->value());
    settings.setValue("localAntiMergeMinPartCells", localAntiMergeMinPartCellsSpin_->value());
    settings.setValue("localAntiMergeMinPartRatio", localAntiMergeMinPartRatioSpin_->value());
    settings.setValue("localAntiMergeMinBridgeCells", localAntiMergeMinBridgeCellsSpin_->value());
    settings.setValue("localAntiMergeMinLowRatio", localAntiMergeMinLowRatioSpin_->value());
    settings.setValue("residualTrace", residualTraceCheck_->isChecked());
    settings.setValue("residualTraceMinLength", residualTraceMinLengthSpin_->value());
    settings.setValue("residualTraceMaxCells", residualTraceMaxCellsSpin_->value());
    settings.setValue("smoothIterations", smoothIterationsSpin_->value());
    settings.setValue("simplifyTolerance", simplifyToleranceSpin_->value());
    settings.setValue("color", colorEdit_->text());
    settings.setValue("lineWidth", lineWidthSpin_->value());
    settings.setValue("alpha", alphaSpin_->value());
    if (showGridLayerCheck_) {
        settings.setValue("showGridLayer", showGridLayerCheck_->isChecked());
    }
    if (showPolygonLayerCheck_) {
        settings.setValue("showPolygonLayer", showPolygonLayerCheck_->isChecked());
    }
    if (mainSplitter_) {
        settings.setValue("mainSplitterState", mainSplitter_->saveState());
    }
    settings.setValue("geoEastRunner", geoEastRunnerEdit_->text());
    settings.setValue("geoEastRoot", geoEastRootEdit_->text());
    settings.setValue("geoEastDataRoot", geoEastDataRootEdit_->text());
    settings.setValue("pgHost", pgHostEdit_->text());
    settings.setValue("pgPort", pgPortEdit_->text());
    settings.setValue("database", databaseEdit_->text());
    settings.setValue("project", projectEdit_->text());
    settings.setValue("user", userEdit_->text());
    settings.remove("password");
    settings.setValue("geoEastGrid", geoEastGridCombo_->currentText());
    settings.setValue("lastOutputCsv", lastOutputCsv_);
    settings.setValue("lastGeoEastOutput", lastGeoEastOutput_);
}

void FaultPolyTraceWindow::updateTaskPreview() {
    if (!view_) {
        return;
    }
    const QString thresholdText = thresholdModeCombo_->currentText() == "fixed"
        ? uiText(QString::fromUtf8("固定阈值 %1")).arg(thresholdSpin_->value(), 0, 'f', 4)
        : uiText(QString::fromUtf8("分位数 %1")).arg(percentileSpin_->value(), 0, 'f', 4);
    const QString inputText = isGeoEastMode()
        ? geoEastGridCombo_->currentText().trimmed()
        : (inputEdit_->text().trimmed().isEmpty()
        ? uiText(QString::fromUtf8("未选择"))
        : QFileInfo(inputEdit_->text().trimmed()).fileName());
    const QString outputText = outputNameEdit_->text().trimmed().isEmpty()
        ? uiText(QString::fromUtf8("未命名"))
        : outputNameEdit_->text().trimmed();
    const QString outputDirText = outputDirEdit_->text().trimmed().isEmpty()
        ? uiText(QString::fromUtf8("默认输出目录"))
        : QDir::toNativeSeparators(outputDirEdit_->text().trimmed());
    const QString presetText = uiText(comboKey(presetCombo_));
    const QString rootAntiMergeText = rootAntiMergeCheck_->isChecked()
        ? uiText(QString::fromUtf8("启用"))
        : uiText(QString::fromUtf8("关闭"));
    const QString residualTraceText = residualTraceCheck_->isChecked()
        ? uiText(QString::fromUtf8("启用"))
        : uiText(QString::fromUtf8("关闭"));

    view_->setTaskSummary({
        isGeoEastMode()
            ? uiText(QString::fromUtf8("运行方式：GeoEast 数据库模式    数据库：%1    项目：%2")).arg(databaseEdit_->text(), projectEdit_->text())
            : uiText(QString::fromUtf8("运行方式：独立文件模式    输入格式：%1")).arg(inputFormatCombo_->currentText()),
        isGeoEastMode()
            ? uiText(QString::fromUtf8("输入网格：%1")).arg(inputText)
            : uiText(QString::fromUtf8("输入文件：%1")).arg(inputText),
        uiText(QString::fromUtf8("输出名称：%1")).arg(outputText),
        uiText(QString::fromUtf8("输出目录：%1")).arg(outputDirText),
        uiText(QString::fromUtf8("方案：%1    模式：%2    阈值：%3"))
            .arg(presetText, traceModeCombo_->currentText(), thresholdText),
        uiText(QString::fromUtf8("面积过滤：小于 %1 的多边形不输出    结果线宽：%2    透明度：%3"))
            .arg(minAreaSpin_->value(), 0, 'f', 2)
            .arg(lineWidthSpin_->value(), 0, 'f', 3)
            .arg(alphaSpin_->value(), 0, 'f', 2),
        uiText(QString::fromUtf8("弱响应：分位 %1 / 半径 %2    根系防黏连：%3    残余补追：%4"))
            .arg(weakPercentileSpin_->value(), 0, 'f', 4)
            .arg(weakConnectRadiusSpin_->value())
            .arg(rootAntiMergeText)
            .arg(residualTraceText),
        uiText(QString::fromUtf8("当前状态：等待参数输入及任务执行"))
    });
}

fpt_ui::TraceParameters FaultPolyTraceWindow::currentTraceParameters() const {
    fpt_ui::TraceParameters p;
    p.traceMode = traceModeCombo_->currentText();
    p.thresholdMode = thresholdModeCombo_->currentText();
    p.threshold = thresholdSpin_->value();
    p.percentile = percentileSpin_->value();
    p.minArea = minAreaSpin_->value();
    p.closeRadius = closeRadiusSpin_->value();
    p.weakPercentile = weakPercentileSpin_->value();
    p.weakConnectRadius = weakConnectRadiusSpin_->value();
    p.beltRadius = beltRadiusSpin_->value();
    p.postBeltCloseRadius = postBeltCloseRadiusSpin_->value();
    p.attributeConstrainedBelt = attributeConstrainedBeltCheck_->isChecked();
    p.attributeEnvelopeRadius = attributeEnvelopeRadiusSpin_->value();
    p.minBranchLength = minBranchLengthSpin_->value();
    p.minJunctionBranchLength = minJunctionBranchLengthSpin_->value();
    p.minJunctionBranchMean = minJunctionBranchMeanSpin_->value();
    p.maxGapDistance = maxGapDistanceSpin_->value();
    p.maxGapAngle = maxGapAngleSpin_->value();
    p.rootAntiMerge = rootAntiMergeCheck_->isChecked();
    p.rootAntiMergeRadius = rootAntiMergeRadiusSpin_->value();
    p.rootAntiMergeSupportRatio = rootAntiMergeSupportRatioSpin_->value();
    p.rootAntiMergeMinPartCells = rootAntiMergeMinPartCellsSpin_->value();
    p.rootAntiMergeMinPartRatio = rootAntiMergeMinPartRatioSpin_->value();
    p.rootAntiMergeWeakGuardThreshold = rootAntiMergeWeakGuardThresholdSpin_->value();
    p.rootAntiMergeWeakGuardRadius = rootAntiMergeWeakGuardRadiusSpin_->value();
    p.localAntiMerge = localAntiMergeCheck_->isChecked();
    p.localAntiMergeLowThreshold = localAntiMergeLowThresholdSpin_->value();
    p.localAntiMergeBridgeRadius = localAntiMergeBridgeRadiusSpin_->value();
    p.localAntiMergeMinPartCells = localAntiMergeMinPartCellsSpin_->value();
    p.localAntiMergeMinPartRatio = localAntiMergeMinPartRatioSpin_->value();
    p.localAntiMergeMinBridgeCells = localAntiMergeMinBridgeCellsSpin_->value();
    p.localAntiMergeMinLowRatio = localAntiMergeMinLowRatioSpin_->value();
    p.residualTrace = residualTraceCheck_->isChecked();
    p.residualTraceMinLength = residualTraceMinLengthSpin_->value();
    p.residualTraceMaxCells = residualTraceMaxCellsSpin_->value();
    p.simplifyTolerance = simplifyToleranceSpin_->value();
    p.smoothIterations = smoothIterationsSpin_->value();

    const QString mode = comboKey(qualityModeCombo_);
    if (mode == QString::fromUtf8("质量诊断")) {
        p.qualityMode = fpt_ui::QualityMode::Diagnostics;
    } else if (mode == QString::fromUtf8("受控实验")) {
        p.qualityMode = fpt_ui::QualityMode::Experimental;
    }
    return p;
}

QStringList FaultPolyTraceWindow::traceArgs() const {
    QStringList args;
    args << "--input" << inputEdit_->text().trimmed()
         << "--input-format" << inputFormatCombo_->currentText()
         << "--output" << outputCsvPath()
         << "--diagnostics" << outputBasePath() + ".diagnostics.json"
         << "--branches-output" << outputBasePath() + ".branches.csv"
         << "--topology-output" << outputBasePath() + ".topology.csv";
    args << fpt_ui::traceArguments(currentTraceParameters());
    return args;
}

void FaultPolyTraceWindow::browseInput() {
    const QString path = QFileDialog::getOpenFileName(this, uiText(QString::fromUtf8("选择平面断层属性网格")), QFileInfo(inputEdit_->text()).absolutePath(), uiText(QString::fromUtf8("网格文件 (*.txt *.dat *.csv);;所有文件 (*.*)")));
    if (!path.isEmpty()) {
        inputEdit_->setText(QDir::toNativeSeparators(path));
        if (showGridLayerCheck_ && showGridLayerCheck_->isChecked()) {
            refreshInputPreview();
        } else if (view_) {
            view_->clearData(QString::fromUtf8("已选择输入文件；勾选“显示原始网格层”后再载入图像"));
            setComboByKey(viewModeCombo_, QString::fromUtf8("任务预览"));
            updateStatus(QString::fromUtf8("已选择输入文件，未自动载入图像以保持界面流畅"));
        }
        refreshAvailableResults();
    }
}

void FaultPolyTraceWindow::browseOutputDir() {
    const QString path = QFileDialog::getExistingDirectory(this, uiText(QString::fromUtf8("选择输出目录")), outputDirEdit_->text());
    if (!path.isEmpty()) {
        outputDirEdit_->setText(QDir::toNativeSeparators(path));
    }
}

void FaultPolyTraceWindow::chooseColor() {
    const QColor color = QColorDialog::getColor(QColor(colorEdit_->text()), this, QString::fromUtf8("选择结果多边形颜色"));
    if (color.isValid()) {
        setPolygonColor(color);
        view_->setStyle(color, lineWidthSpin_->value(), alphaSpin_->value());
    }
}

void FaultPolyTraceWindow::updateDataSourceMode() {
    const bool geoEastMode = isGeoEastMode();
    if (sourceStack_) {
        sourceStack_->setCurrentIndex(geoEastMode ? 1 : 0);
    }
    if (inputFormatCombo_) {
        inputFormatCombo_->setEnabled(!geoEastMode);
    }
    if (geoEastRefreshButton_) {
        geoEastRefreshButton_->setEnabled(geoEastMode);
    }
    if (geoEastConfigButton_) {
        geoEastConfigButton_->setEnabled(geoEastMode);
    }
    if (geoEastDeleteButton_) {
        geoEastDeleteButton_->setEnabled(geoEastMode);
    }
    if (geoEastMode && geoEastGridCombo_ &&
        !geoEastGridCombo_->property("fptCatalogReady").toBool() &&
        !geoEastGridCombo_->property("fptCatalogLoading").toBool() &&
        process_ && process_->state() == QProcess::NotRunning &&
        missingGeoEastConfig(false, false).isEmpty()) {
        geoEastGridCombo_->setProperty("fptCatalogLoading", true);
        refreshResultsAfterGrids_ = true;
        QTimer::singleShot(80, this, [this]() {
            geoEastGridCombo_->setProperty("fptCatalogLoading", false);
            refreshGeoEastGrids();
        });
    }
    updateTaskPreview();
    updateResultPanel();
}

void FaultPolyTraceWindow::showGeoEastConfigDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("GeoEast 数据库与环境配置"));
    dialog.setWindowFlags((dialog.windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint) &
                          ~Qt::WindowMaximizeButtonHint &
                          ~Qt::WindowContextHelpButtonHint);
    dialog.resize(760, 380);
    dialog.setMinimumSize(720, 360);
    dialog.setStyleSheet(fpt_ui::configDialogStyle());

    QVBoxLayout* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame* headerFrame = new QFrame(&dialog);
    headerFrame->setObjectName("dialogHeader");
    QHBoxLayout* headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(16, 0, 10, 0);
    QLabel* header = new QLabel(QString::fromUtf8("GeoEast 连接配置"), headerFrame);
    header->setObjectName("dialogTitle");
    header->setFixedHeight(44);
    QPushButton* helpButton = new QPushButton(QString::fromUtf8("帮助"), headerFrame);
    helpButton->setObjectName("headerHelp");
    helpButton->setToolTip(QString::fromUtf8("查看每个配置项从哪里获取"));
    helpButton->setFixedWidth(64);
    headerLayout->addWidget(header, 1);
    headerLayout->addWidget(helpButton);
    root->addWidget(headerFrame);

    QWidget* body = new QWidget(&dialog);
    QVBoxLayout* bodyRoot = new QVBoxLayout(body);
    bodyRoot->setContentsMargins(16, 14, 16, 12);
    bodyRoot->setSpacing(10);

    QGridLayout* form = new QGridLayout();
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(8);
    bodyRoot->addLayout(form);

    auto addPathRow = [&](int row, const QString& label, QLineEdit* source, bool directory) {
        QLineEdit* edit = new QLineEdit(source->text(), &dialog);
        edit->setCursorPosition(0);
        edit->setToolTip(edit->text());
        QPushButton* browse = new QPushButton(QString::fromUtf8("浏览"), &dialog);
        configureSmallButton(browse);
        QWidget* holder = new QWidget(&dialog);
        QHBoxLayout* layout = new QHBoxLayout(holder);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);
        layout->addWidget(edit, 1);
        layout->addWidget(browse);
        form->addWidget(formLabel(label, &dialog), row, 0);
        form->addWidget(holder, row, 1, 1, 3);
        connect(browse, &QPushButton::clicked, &dialog, [this, edit, directory]() {
            QString path;
            if (directory) {
                path = QFileDialog::getExistingDirectory(this, uiText(QString::fromUtf8("选择目录")), edit->text());
            } else {
                path = QFileDialog::getOpenFileName(this, uiText(QString::fromUtf8("选择可执行文件")), QFileInfo(edit->text()).absolutePath(), uiText(QString::fromUtf8("程序 (*.exe);;所有文件 (*.*)")));
            }
            if (!path.isEmpty()) {
                edit->setText(QDir::toNativeSeparators(path));
            }
        });
        return edit;
    };

    QLineEdit* runnerCopy = addPathRow(0, QString::fromUtf8("执行程序"), geoEastRunnerEdit_, false);
    QLineEdit* rootCopy = addPathRow(1, QString::fromUtf8("GeoEast根目录"), geoEastRootEdit_, true);
    QLineEdit* dataRootCopy = addPathRow(2, QString::fromUtf8("数据根目录"), geoEastDataRootEdit_, true);

    QFrame* line = new QFrame(&dialog);
    line->setObjectName("line");
    bodyRoot->addWidget(line);

    QGridLayout* db = new QGridLayout();
    db->setHorizontalSpacing(8);
    db->setVerticalSpacing(8);
    bodyRoot->addLayout(db);

    QLineEdit* hostCopy = new QLineEdit(pgHostEdit_->text(), &dialog);
    QLineEdit* portCopy = new QLineEdit(pgPortEdit_->text(), &dialog);
    QLineEdit* databaseCopy = new QLineEdit(databaseEdit_->text(), &dialog);
    QLineEdit* projectCopy = new QLineEdit(projectEdit_->text(), &dialog);
    QLineEdit* userCopy = new QLineEdit(userEdit_->text(), &dialog);
    QLineEdit* passwordCopy = new QLineEdit(passwordEdit_->text(), &dialog);
    passwordCopy->setEchoMode(QLineEdit::Password);

    db->addWidget(formLabel(QString::fromUtf8("PG主机"), &dialog), 0, 0);
    db->addWidget(hostCopy, 0, 1);
    db->addWidget(formLabel(QString::fromUtf8("端口"), &dialog), 0, 2);
    db->addWidget(portCopy, 0, 3);
    db->addWidget(formLabel(QString::fromUtf8("数据库"), &dialog), 1, 0);
    db->addWidget(databaseCopy, 1, 1);
    db->addWidget(formLabel(QString::fromUtf8("项目"), &dialog), 1, 2);
    db->addWidget(projectCopy, 1, 3);
    db->addWidget(formLabel(QString::fromUtf8("用户"), &dialog), 2, 0);
    db->addWidget(userCopy, 2, 1);
    db->addWidget(formLabel(QString::fromUtf8("密码"), &dialog), 2, 2);
    db->addWidget(passwordCopy, 2, 3);
    db->setColumnStretch(1, 1);
    db->setColumnStretch(3, 1);

    QLabel* checkStatus = new QLabel(QString::fromUtf8("尚未检查配置。"), &dialog);
    checkStatus->setObjectName("checkStatus");
    bodyRoot->addWidget(checkStatus);
    root->addWidget(body, 1);

    QFrame* footer = new QFrame(&dialog);
    footer->setObjectName("dialogFooter");
    QHBoxLayout* buttons = new QHBoxLayout(footer);
    buttons->setContentsMargins(16, 10, 16, 10);
    QPushButton* autoDetect = new QPushButton(QString::fromUtf8("自动检测"), footer);
    QPushButton* check = new QPushButton(QString::fromUtf8("检查配置"), footer);
    buttons->addStretch(1);
    QPushButton* cancel = new QPushButton(QString::fromUtf8("取消"), footer);
    QPushButton* ok = new QPushButton(QString::fromUtf8("应用并保存配置"), footer);
    ok->setObjectName("primaryButton");
    buttons->insertWidget(0, autoDetect);
    buttons->insertWidget(1, check);
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    root->addWidget(footer);

    auto applyAutoDetection = [&]() {
        const QString rootPath = defaultGeoEastRoot();
        const QString dataRootPath = detectGeoEastDataRoot(rootPath);
        const QString runnerPath = geoEastRunnerPath();
        if (!runnerPath.isEmpty()) {
            runnerCopy->setText(runnerPath);
        }
        if (!rootPath.isEmpty()) {
            rootCopy->setText(rootPath);
        }
        if (!dataRootPath.isEmpty()) {
            dataRootCopy->setText(dataRootPath);
        }
        for (QLineEdit* pathEdit : {runnerCopy, rootCopy, dataRootCopy}) {
            pathEdit->setCursorPosition(0);
            pathEdit->setToolTip(pathEdit->text());
        }
        if (hostCopy->text().trimmed().isEmpty()) {
            hostCopy->setText("127.0.0.1");
        }
        if (portCopy->text().trimmed().isEmpty()) {
            portCopy->setText("5433");
        }
        if (databaseCopy->text().trimmed().isEmpty() || databaseCopy->text().trimmed() == "ndp") {
            databaseCopy->setText(detectGeoEastDatabaseName(dataRootCopy->text()));
        }
        if (userCopy->text().trimmed().isEmpty()) {
            userCopy->setText(defaultUserName());
        }
        checkStatus->setText(uiText(QString::fromUtf8("已执行自动检测。项目名无法安全推断，请按当前 GeoEast 项目填写后再检查配置。")));
    };

    auto currentIssues = [&]() {
        return validateGeoEastConfigValues(runnerCopy->text(), rootCopy->text(), dataRootCopy->text(),
                                           hostCopy->text(), portCopy->text(), databaseCopy->text(),
                                           projectCopy->text(), userCopy->text());
    };

    auto showCheckResult = [&]() -> bool {
        const QStringList issues = currentIssues();
        if (issues.isEmpty()) {
            checkStatus->setText(uiText(QString::fromUtf8("配置检查通过。可以刷新网格列表。")));
            QMessageBox::information(&dialog, uiText(QString::fromUtf8("配置检查通过")),
                                     uiText(QString::fromUtf8("GeoEast 数据库配置项完整。保存后可返回主界面刷新网格列表。")));
            return true;
        }
        checkStatus->setText(uiText(QString::fromUtf8("配置仍有 %1 项需要处理。").arg(issues.size())));
        QMessageBox::warning(&dialog, uiText(QString::fromUtf8("配置需要补充")),
                             uiText(QString::fromUtf8("请处理以下项目：\n\n- %1").arg(issues.join(QString::fromUtf8("\n- ")))));
        return false;
    };

    connect(helpButton, &QPushButton::clicked, &dialog, [&]() {
        QMessageBox::information(&dialog, uiText(QString::fromUtf8("GeoEast 配置帮助")), uiText(geoEastConfigHelpText()));
    });
    connect(autoDetect, &QPushButton::clicked, &dialog, applyAutoDetection);
    connect(check, &QPushButton::clicked, &dialog, showCheckResult);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dialog, [&]() {
        const QStringList issues = currentIssues();
        if (!issues.isEmpty()) {
            const int choice = QMessageBox::warning(
                &dialog,
                uiText(QString::fromUtf8("配置尚未完整")),
                uiText(QString::fromUtf8("以下配置可能导致刷新网格或写库失败：\n\n- %1\n\n建议返回修改。是否仍然保存？")
                    .arg(issues.join(QString::fromUtf8("\n- ")))),
                QMessageBox::Save | QMessageBox::Cancel,
                QMessageBox::Cancel);
            if (choice != QMessageBox::Save) {
                checkStatus->setText(uiText(QString::fromUtf8("配置未保存，请补齐后再应用。")));
                return;
            }
        }
        geoEastRunnerEdit_->setText(runnerCopy->text());
        geoEastRootEdit_->setText(rootCopy->text());
        geoEastDataRootEdit_->setText(dataRootCopy->text());
        pgHostEdit_->setText(hostCopy->text());
        pgPortEdit_->setText(portCopy->text());
        databaseEdit_->setText(databaseCopy->text());
        projectEdit_->setText(projectCopy->text());
        userEdit_->setText(userCopy->text());
        passwordEdit_->setText(passwordCopy->text());
        saveSettings();
        updateTaskPreview();
        updateStatus(QString::fromUtf8("GeoEast 数据库配置已保存"));
        dialog.accept();
        if (isGeoEastMode() && process_->state() == QProcess::NotRunning) {
            refreshResultsAfterGrids_ = true;
            QTimer::singleShot(0, this, &FaultPolyTraceWindow::refreshGeoEastGrids);
        }
    });

    fpt_ui::captureTranslatableText(&dialog);
    fpt_ui::applyTranslatableText(&dialog, languageCode());
    dialog.exec();
}

QStringList FaultPolyTraceWindow::missingGeoEastConfig(bool requireGrid, bool requireOutput) const {
    QStringList missing;
    const QString runner = geoEastRunnerEdit_ ? geoEastRunnerEdit_->text().trimmed() : QString();
    const QString geoEastRoot = geoEastRootEdit_ ? geoEastRootEdit_->text().trimmed() : QString();
    const QString dataRoot = geoEastDataRootEdit_ ? geoEastDataRootEdit_->text().trimmed() : QString();
    const QString database = databaseEdit_ ? databaseEdit_->text().trimmed() : QString();
    const QString project = projectEdit_ ? projectEdit_->text().trimmed() : QString();
    const QString user = userEdit_ ? userEdit_->text().trimmed() : QString();
    const QString grid = geoEastGridCombo_ ? geoEastGridCombo_->currentText().trimmed() : QString();
    const QString output = geoEastOutputName().trimmed();

    if (runner.isEmpty()) {
        missing << QString::fromUtf8("GeoEast 执行程序文件");
    } else if (!QFileInfo::exists(runner)) {
        missing << QString::fromUtf8("GeoEast 执行程序文件不存在：%1").arg(QDir::toNativeSeparators(runner));
    }
    if (geoEastRoot.isEmpty()) {
        missing << QString::fromUtf8("GeoEast 根目录");
    } else if (!QDir(geoEastRoot).exists()) {
        missing << QString::fromUtf8("GeoEast 根目录不存在：%1").arg(QDir::toNativeSeparators(geoEastRoot));
    }
    if (dataRoot.isEmpty()) {
        missing << QString::fromUtf8("GeoEast 数据根目录");
    } else if (!QDir(dataRoot).exists()) {
        missing << QString::fromUtf8("GeoEast 数据根目录不存在：%1").arg(QDir::toNativeSeparators(dataRoot));
    }
    if (database.isEmpty()) {
        missing << QString::fromUtf8("数据库名");
    }
    if (project.isEmpty()) {
        missing << QString::fromUtf8("项目名");
    }
    if (user.isEmpty()) {
        missing << QString::fromUtf8("用户名");
    }
    if (requireGrid && grid.isEmpty()) {
        missing << QString::fromUtf8("输入网格");
    }
    if (requireOutput && output.isEmpty()) {
        missing << QString::fromUtf8("输出多边形名称");
    }
    return missing;
}

bool FaultPolyTraceWindow::ensureGeoEastConfigReady(const QString& operation, bool requireGrid, bool requireOutput) {
    const QStringList missing = missingGeoEastConfig(requireGrid, requireOutput);
    if (missing.isEmpty()) {
        return true;
    }

    appendLog(QString::fromUtf8("无法执行【%1】：GeoEast 配置不完整。缺失或无效项：%2")
                  .arg(operation, missing.join(QString::fromUtf8("；"))));

    // 按缺失项路由：主面板字段直接定位聚焦，数据库项才进配置窗口。
    const bool missingOutputName = requireOutput && outputNameEdit_ &&
                                   outputNameEdit_->text().trimmed().isEmpty();
    const bool missingGridChoice = requireGrid && geoEastGridCombo_ &&
                                   geoEastGridCombo_->currentText().trimmed().isEmpty();
    const bool mainPanelFixable = missingOutputName || missingGridChoice;

    QMessageBox message(this);
    message.setIcon(QMessageBox::Warning);
    message.setWindowTitle(uiText(QString::fromUtf8("GeoEast 配置不完整")));
    message.setText(uiText(QString::fromUtf8("执行【%1】前需要先补齐 GeoEast 配置。").arg(operation)));
    message.setInformativeText(uiText(QString::fromUtf8("缺失或无效项：\n\n- %1\n\n请点击【打开数据库配置】补齐后再执行。保存一次后，下次打开会自动记住。")
                                   .arg(missing.join(QString::fromUtf8("\n- ")))));
    QPushButton* fixButton = nullptr;
    if (mainPanelFixable) {
        fixButton = message.addButton(uiText(QString::fromUtf8("去填写")), QMessageBox::AcceptRole);
    }
    QPushButton* configButton = message.addButton(uiText(QString::fromUtf8("打开数据库配置")), QMessageBox::AcceptRole);
    message.addButton(uiText(QString::fromUtf8("取消")), QMessageBox::RejectRole);
    message.exec();
    if (fixButton && message.clickedButton() == fixButton) {
        QWidget* target = missingOutputName ? static_cast<QWidget*>(outputNameEdit_)
                                            : static_cast<QWidget*>(geoEastGridCombo_);
        if (target) {
            target->setFocus(Qt::OtherFocusReason);
            target->setStyleSheet(QStringLiteral("border: 1px solid #D93025;"));
            QTimer::singleShot(2600, target, [target]() { target->setStyleSheet(QString()); });
        }
    } else if (message.clickedButton() == configButton) {
        showGeoEastConfigDialog();
    }
    updateStatus(QString::fromUtf8("GeoEast 配置不完整，已停止操作"));
    return false;
}

void FaultPolyTraceWindow::refreshGeoEastGrids() {
    if (process_->state() != QProcess::NotRunning) {
        updateStatus(QString::fromUtf8("当前任务尚未完成，暂不能刷新网格列表"));
        return;
    }
    saveSettings();
    if (!ensureGeoEastConfigReady(QString::fromUtf8("刷新网格列表"), false, false)) {
        return;
    }
    const QString runner = geoEastRunnerEdit_->text().trimmed();
    QStringList args = geoEastConnectionArgs();
    args << "--list-grids";
    startProcess(ProcessTask::RefreshGeoEastGrids, runner, args, QString::fromUtf8("正在刷新 GeoEast 网格列表..."));
}

QStringList FaultPolyTraceWindow::localResultCandidates() const {
    QString dir = outputDirEdit_ ? outputDirEdit_->text().trimmed() : QString();
    if (dir.isEmpty()) {
        dir = defaultOutputDir();
    }

    QStringList candidates;
    const QString expected = outputCsvPath();
    if (QFileInfo::exists(expected)) {
        candidates << QFileInfo(expected).absoluteFilePath();
    }

    QDir outputDir(dir);
    const QFileInfoList files = outputDir.entryInfoList(QStringList() << "*.csv", QDir::Files, QDir::Time);
    for (const QFileInfo& file : files) {
        const QString path = file.absoluteFilePath();
        if (!candidates.contains(path, Qt::CaseInsensitive)) {
            candidates << path;
        }
    }

    return candidates;
}

void FaultPolyTraceWindow::refreshAvailableResults() {
    if (!resultCatalogCombo_) {
        return;
    }
    if (process_->state() != QProcess::NotRunning) {
        updateStatus(QString::fromUtf8("任务运行中，暂不能刷新已有结果"));
        return;
    }

    if (isGeoEastMode()) {
        saveSettings();
        if (!ensureGeoEastConfigReady(QString::fromUtf8("刷新已有结果"), false, false)) {
            return;
        }
        const QString runner = geoEastRunnerEdit_->text().trimmed();
        QStringList args = geoEastConnectionArgs();
        args << "--list-fault-polygons";
        startProcess(ProcessTask::RefreshGeoEastResults, runner, args, QString::fromUtf8("正在刷新 GeoEast 结果列表..."));
        return;
    }

    const QString currentPath = resultCatalogCombo_->currentData().toString();
    resultCatalogCombo_->blockSignals(true);
    resultCatalogCombo_->clear();
    const QStringList candidates = localResultCandidates();
    for (const QString& path : candidates) {
        resultCatalogCombo_->addItem(QFileInfo(path).fileName(), path);
    }
    if (resultCatalogCombo_->count() == 0) {
        resultCatalogCombo_->addItem(QString::fromUtf8("输出目录暂无结果；点击此处查看引导"), QString());
        resultCatalogCombo_->setEnabled(true);
        if (loadSelectedResultButton_) {
            loadSelectedResultButton_->setEnabled(false);
        }
    } else {
        resultCatalogCombo_->setEnabled(true);
        if (loadSelectedResultButton_) {
            loadSelectedResultButton_->setEnabled(true);
        }
        const int index = resultCatalogCombo_->findData(currentPath);
        if (index >= 0) {
            resultCatalogCombo_->setCurrentIndex(index);
        }
    }
    resultCatalogCombo_->blockSignals(false);
    updateStatus(QString::fromUtf8("已刷新输出目录结果，共 %1 个").arg(candidates.size()));
    if (resultCatalogCombo_->currentData().toString().size() > 0 &&
        (!showPolygonLayerCheck_ || showPolygonLayerCheck_->isChecked())) {
        QTimer::singleShot(0, this, [this]() {
            loadSelectedResultInternal(false);
        });
    }
    updateResultPanel();
}

void FaultPolyTraceWindow::loadSelectedResult() {
    loadSelectedResultInternal(true);
}

void FaultPolyTraceWindow::loadSelectedResultInternal(bool userInitiated) {
    if (!resultCatalogCombo_) {
        return;
    }
    const QString selectedData = resultCatalogCombo_->currentData().toString();
    const QString selectedText = resultCatalogCombo_->currentText().trimmed();
    if (selectedText.isEmpty() || selectedData.isEmpty()) {
        updateStatus(QString::fromUtf8("暂无可载入结果，请先运行追踪或刷新已有结果列表"));
        if (userInitiated) {
            QMessageBox::information(this,
                                     uiText(QString::fromUtf8("没有可载入的结果")),
                                     uiText(QString::fromUtf8("请先运行追踪，或点击【刷新结果列表】读取输出目录/GeoEast 数据库中的已有结果。")));
        }
        return;
    }

    if (isGeoEastMode()) {
        if (process_->state() != QProcess::NotRunning) {
            updateStatus(QString::fromUtf8("任务运行中，暂不能载入 GeoEast 历史结果"));
            return;
        }
        if (!ensureGeoEastConfigReady(QString::fromUtf8("载入已有结果"), true, false)) {
            return;
        }
        outputNameEdit_->setText(selectedData);
        lastGeoEastOutput_ = selectedData;
        saveSettings();
        const QString runner = geoEastRunnerEdit_->text().trimmed();
        QStringList args = geoEastConnectionArgs();
        args << "--grid" << geoEastGridCombo_->currentText().trimmed()
             << "--output" << selectedData
             << "--validate-grid"
             << "--view-grid-output" << outputBasePath() + ".grid.txt"
             << "--export-fault-polygons-output" << outputCsvPath();
        startProcess(ProcessTask::ExportGeoEastResult, runner, args, QString::fromUtf8("正在导出 GeoEast 历史结果..."));
        return;
    }

    if (!QFileInfo::exists(selectedData)) {
        updateStatus(QString::fromUtf8("结果文件不存在，请刷新结果列表"));
        if (userInitiated) {
            QMessageBox::warning(this, uiText(QString::fromUtf8("结果文件不存在")), QDir::toNativeSeparators(selectedData));
        }
        refreshAvailableResults();
        return;
    }
    lastOutputCsv_ = selectedData;
    const QFileInfo file(selectedData);
    outputDirEdit_->setText(file.absolutePath());
    outputNameEdit_->setText(file.completeBaseName());
    if (inputEdit_->text().trimmed().isEmpty()) {
        view_->clearData(QString::fromUtf8("已载入结果多边形；如需叠加，请再选择对应原始网格"));
        refreshResultPreview();
        setComboByKey(viewModeCombo_, QString::fromUtf8("结果多边形"));
    } else {
        refreshInputPreview();
        refreshResultPreview();
        setComboByKey(viewModeCombo_, QString::fromUtf8("叠加结果"));
    }
    updateStatus(QString::fromUtf8("已载入已有结果：%1").arg(file.fileName()));
    updateResultPanel();
}

void FaultPolyTraceWindow::deleteGeoEastResult() {
    if (process_->state() != QProcess::NotRunning) {
        updateStatus(QString::fromUtf8("已有任务正在运行，暂不能删除结果"));
        return;
    }
    saveSettings();
    if (!ensureGeoEastConfigReady(QString::fromUtf8("删除当前结果"), false, true)) {
        return;
    }
    const QString outputName = geoEastOutputName();
    // 中文按钮 + 红色“删除”，默认焦点在取消，防误触回车（与插件一致）。
    QMessageBox confirmBox(QMessageBox::Warning,
        uiText(QString::fromUtf8("确认删除 GeoEast 结果")),
        uiText(QString::fromUtf8("将删除当前项目中的结果多边形对象：\n\n%1\n\n该操作不会删除输入网格，也不会删除本地输出文件。是否继续？")).arg(outputName),
        QMessageBox::NoButton, this);
    QPushButton* deleteButton = confirmBox.addButton(uiText(QString::fromUtf8("删除")), QMessageBox::DestructiveRole);
    deleteButton->setStyleSheet("background:#D93025; color:#FFFFFF; border:1px solid #B42318; font-weight:bold;");
    QPushButton* cancelButton = confirmBox.addButton(uiText(QString::fromUtf8("取消")), QMessageBox::RejectRole);
    confirmBox.setDefaultButton(cancelButton);
    confirmBox.exec();
    if (confirmBox.clickedButton() != deleteButton) {
        return;
    }
    const QString runner = geoEastRunnerEdit_->text().trimmed();
    QStringList args = geoEastConnectionArgs();
    args << "--output" << outputName << "--delete-output";
    startProcess(ProcessTask::DeleteGeoEastResult, runner, args, QString::fromUtf8("正在删除 GeoEast 结果对象..."));
}

void FaultPolyTraceWindow::refreshInputPreview() {
    if (isGeoEastMode()) {
        updateStatus(QString::fromUtf8("GeoEast 模式下请先运行追踪或刷新网格；运行完成后会生成结果视图文件"));
        return;
    }
    if (inputEdit_->text().trimmed().isEmpty() || !QFileInfo::exists(inputEdit_->text().trimmed())) {
        view_->clearData(QString::fromUtf8("请选择输入文件"));
        setComboByKey(viewModeCombo_, QString::fromUtf8("任务预览"));
        updateStatus(QString::fromUtf8("请选择输入文件"));
        updateResultPanel();
        return;
    }
    QString error;
    updateStatus(QString::fromUtf8("正在载入原始网格..."));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    const bool loaded = view_->loadGrid(inputEdit_->text().trimmed(), inputFormatCombo_->currentText(), &error);
    QApplication::restoreOverrideCursor();
    if (loaded) {
        view_->clearPolygons(QString::fromUtf8("已载入原始网格，尚未运行追踪"));
        if (lastOutputCsv_.isEmpty() &&
            (comboKey(viewModeCombo_) == QString::fromUtf8("叠加结果") ||
             comboKey(viewModeCombo_) == QString::fromUtf8("结果多边形") ||
             comboKey(viewModeCombo_) == QString::fromUtf8("任务预览"))) {
            setComboByKey(viewModeCombo_, QString::fromUtf8("原始网格"));
        }
        updateResultPanel();
        updateStatus(QString::fromUtf8("原始网格已载入，尚未运行追踪"));
    } else if (!error.isEmpty()) {
        appendLog(error);
        updateStatus(QString::fromUtf8("原始网格载入失败"));
    }
}

void FaultPolyTraceWindow::refreshResultPreview() {
    if (lastOutputCsv_.isEmpty() || !QFileInfo::exists(lastOutputCsv_)) {
        updateResultPanel();
        return;
    }
    QString error;
    updateStatus(QString::fromUtf8("正在载入结果多边形..."));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    const bool loaded = view_->loadPolygons(lastOutputCsv_, &error);
    QApplication::restoreOverrideCursor();
    if (!loaded && !error.isEmpty()) {
        appendLog(error);
        updateStatus(QString::fromUtf8("结果多边形载入失败"));
    } else if (loaded) {
        updateStatus(QString::fromUtf8("结果多边形已载入"));
    }
    updateResultPanel();
}

void FaultPolyTraceWindow::clearResultStateForNewInput() {
    if (process_ && process_->state() != QProcess::NotRunning) {
        return;
    }
    if (!lastOutputCsv_.isEmpty()) {
        lastOutputCsv_.clear();
    }
    if (view_) {
        view_->clearPolygons(inputEdit_->text().trimmed().isEmpty()
            ? QString::fromUtf8("请选择输入文件")
            : QString::fromUtf8("已选择输入文件，尚未运行追踪"));
    }
    if (inputEdit_->text().trimmed().isEmpty()) {
        setComboByKey(viewModeCombo_, QString::fromUtf8("任务预览"));
    } else if (comboKey(viewModeCombo_) == QString::fromUtf8("叠加结果") ||
               comboKey(viewModeCombo_) == QString::fromUtf8("结果多边形")) {
        setComboByKey(viewModeCombo_, QString::fromUtf8("原始网格"));
    }
    updateResultPanel();
}

void FaultPolyTraceWindow::runTrace() {
    if (doneBanner_) {
        doneBanner_->hide();
    }
    if (process_->state() != QProcess::NotRunning) {
        updateStatus(QString::fromUtf8("已有任务正在运行，请等待完成或先停止任务"));
        return;
    }
    if (isGeoEastMode()) {
        saveSettings();
        if (!ensureGeoEastConfigReady(QString::fromUtf8("开始追踪"), true, true)) {
            return;
        }
        const QString runner = geoEastRunnerEdit_->text().trimmed();
        lastOutputCsv_.clear();
        if (view_) {
            view_->clearPolygons(QString::fromUtf8("正在计算，等待结果生成"));
        }
        updateResultPanel();
        startProcess(ProcessTask::GeoEastTrace, runner, geoEastTraceArgs(), QString::fromUtf8("正在连接 GeoEast 并追踪断层多边形..."));
        return;
    }

    const QString runner = coreRunnerPath();
    if (!QFileInfo::exists(runner)) {
        QMessageBox::warning(this, uiText(QString::fromUtf8("算法程序不存在")), runner);
        return;
    }
    const QString self = QFileInfo(QCoreApplication::applicationFilePath()).absoluteFilePath();
    const QString resolvedRunner = QFileInfo(runner).absoluteFilePath();
    if (resolvedRunner.compare(self, Qt::CaseInsensitive) == 0) {
        QMessageBox::warning(this,
                             uiText(QString::fromUtf8("算法程序定位错误")),
                             uiText(QString::fromUtf8("独立软件不能把自身作为核心算法程序启动。请确认发布包中存在 bin/faultpolytrace.exe。")));
        return;
    }
    if (inputEdit_->text().trimmed().isEmpty() || !QFileInfo::exists(inputEdit_->text().trimmed())) {
        QMessageBox::warning(this, uiText(QString::fromUtf8("输入文件无效")), uiText(QString::fromUtf8("请选择可读取的外部网格文件。")));
        return;
    }
    logEdit_->clear();
    lastOutputCsv_.clear();
    if (view_) {
        view_->clearPolygons(QString::fromUtf8("正在计算，等待结果生成"));
    }
    updateResultPanel();
    saveSettings();
    startProcess(ProcessTask::ExternalTrace, runner, traceArgs(), QString::fromUtf8("正在追踪断层多边形..."));
}

void FaultPolyTraceWindow::stopTrace() {
    if (process_->state() != QProcess::NotRunning) {
        process_->terminate();
    }
}

void FaultPolyTraceWindow::onReadyRead() {
    const QString out = QString::fromLocal8Bit(process_->readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(process_->readAllStandardError());
    if (!out.isEmpty()) {
        processOutput_ += out;
        appendLog(out);
    }
    if (!err.isEmpty()) {
        processOutput_ += err;
        appendLog(err);
    }
}

void FaultPolyTraceWindow::onFinished(int exitCode, QProcess::ExitStatus status) {
    onReadyRead();
    setRunning(false);
    const qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - processStartMs_;
    elapsedLabel_->setText(uiText(QString::fromUtf8("耗时: %1")).arg(elapsedText(elapsedMs)));
    const ProcessTask finishedTask = currentTask_;
    currentTask_ = ProcessTask::None;

    if (finishedTask == ProcessTask::RefreshGeoEastGrids) {
        if (exitCode == 0 && status == QProcess::NormalExit) {
            QStringList names;
            for (const QString& rawLine : processOutput_.split('\n')) {
                const QString line = rawLine.trimmed();
                if (line.isEmpty() || line.startsWith("BOMapGrid") || line.startsWith("Total:") ||
                    line.startsWith("load ") || line.startsWith("Env ") || line.startsWith("GeoEast ") ||
                    line.startsWith("Connecting ") || line.startsWith(">") || line.contains(':')) {
                    continue;
                }
                if (!line.contains(' ')) {
                    names << line;
                }
            }
            if (!names.isEmpty()) {
                const QString current = pendingGeoEastGridRestore_.isEmpty()
                    ? geoEastGridCombo_->currentText()
                    : pendingGeoEastGridRestore_;
                pendingGeoEastGridRestore_.clear();
                geoEastGridCombo_->clear();
                geoEastGridCombo_->addItems(names);
                const int index = geoEastGridCombo_->findText(current);
                if (index >= 0) {
                    geoEastGridCombo_->setCurrentIndex(index);
                }
            } else {
                geoEastGridCombo_->clear();
            }
            geoEastGridCombo_->setProperty("fptCatalogReady", true);
            updateStatus(QString::fromUtf8("GeoEast 网格刷新完成，共 %1 个").arg(names.size()));
            saveSettings();
            if (refreshResultsAfterGrids_) {
                refreshResultsAfterGrids_ = false;
                QTimer::singleShot(0, this, &FaultPolyTraceWindow::refreshAvailableResults);
            }
        } else {
            refreshResultsAfterGrids_ = false;
            geoEastGridCombo_->setProperty("fptCatalogReady", false);
            updateStatus(QString::fromUtf8("GeoEast 网格刷新失败，请查看日志"));
        }
        updateResultPanel();
        return;
    }

    if (finishedTask == ProcessTask::RefreshGeoEastResults) {
        if (exitCode == 0 && status == QProcess::NormalExit) {
            QStringList names;
            for (const QString& rawLine : processOutput_.split('\n')) {
                const QString line = rawLine.trimmed();
                if (line.isEmpty() || line.startsWith("BOFaultPolygon") || line.startsWith("Total:") ||
                    line.startsWith("load ") || line.startsWith("Env ") || line.startsWith("GeoEast ") ||
                    line.startsWith("Connecting ") || line.startsWith(">") || line.contains(':')) {
                    continue;
                }
                if (!line.contains(' ')) {
                    names << line;
                }
            }
            const QString current = resultCatalogCombo_ ? resultCatalogCombo_->currentData().toString() : QString();
            if (resultCatalogCombo_) {
                resultCatalogCombo_->blockSignals(true);
                resultCatalogCombo_->clear();
                for (const QString& name : names) {
                    resultCatalogCombo_->addItem(name, name);
                }
                if (names.isEmpty()) {
                    resultCatalogCombo_->addItem(QString::fromUtf8("当前项目暂无结果；点击此处重新刷新"), QString());
                    resultCatalogCombo_->setEnabled(true);
                    if (loadSelectedResultButton_) {
                        loadSelectedResultButton_->setEnabled(false);
                    }
                } else {
                    resultCatalogCombo_->setEnabled(true);
                    if (loadSelectedResultButton_) {
                        loadSelectedResultButton_->setEnabled(true);
                    }
                    const int index = resultCatalogCombo_->findData(current.isEmpty() ? geoEastOutputName() : current);
                    if (index >= 0) {
                        resultCatalogCombo_->setCurrentIndex(index);
                    }
                }
                resultCatalogCombo_->blockSignals(false);
            }
            updateStatus(QString::fromUtf8("GeoEast 结果刷新完成，共 %1 个").arg(names.size()));
            saveSettings();
            if (!names.isEmpty() && resultCatalogCombo_ && !resultCatalogCombo_->currentData().toString().trimmed().isEmpty() &&
                (!showPolygonLayerCheck_ || showPolygonLayerCheck_->isChecked())) {
                QTimer::singleShot(0, this, [this]() {
                    loadSelectedResultInternal(false);
                });
            }
        } else {
            updateStatus(QString::fromUtf8("GeoEast 结果刷新失败，请查看日志"));
        }
        updateResultPanel();
        return;
    }

    if (finishedTask == ProcessTask::DeleteGeoEastResult) {
        updateStatus(exitCode == 0 && status == QProcess::NormalExit
            ? QString::fromUtf8("GeoEast 结果对象已删除或原本不存在")
            : QString::fromUtf8("GeoEast 结果删除失败，请查看日志"));
        if (exitCode == 0 && status == QProcess::NormalExit) {
            lastGeoEastOutput_.clear();
            refreshAvailableResults();
        }
        updateResultPanel();
        return;
    }

    if (exitCode == 0 && status == QProcess::NormalExit) {
        lastOutputCsv_ = outputCsvPath();
        if (!QFileInfo::exists(lastOutputCsv_) || QFileInfo(lastOutputCsv_).size() <= 0) {
            appendLog(QString::fromUtf8("运行结束，但未生成结果 CSV：%1").arg(QDir::toNativeSeparators(lastOutputCsv_)));
            appendLog(QString::fromUtf8("请检查输入格式、输出目录写入权限和上方算法日志。"));
            updateResultPanel();
            updateStatus(QString::fromUtf8("运行结束但未找到结果 CSV"));
            return;
        }
        if (finishedTask == ProcessTask::GeoEastTrace) {
            lastGeoEastOutput_ = geoEastOutputName();
            appendLog(QString::fromUtf8("GeoEast 数据库写入对象：BOFaultPolygon/%1").arg(lastGeoEastOutput_));
            QString gridError;
            if (view_->loadGrid(outputBasePath() + ".grid.txt", "matrix", &gridError)) {
                appendLog(QString::fromUtf8("原始网格视图文件：%1").arg(QDir::toNativeSeparators(outputBasePath() + ".grid.txt")));
            } else if (!gridError.isEmpty()) {
                appendLog(gridError);
            }
        } else if (finishedTask == ProcessTask::ExportGeoEastResult) {
            lastGeoEastOutput_ = geoEastOutputName();
            appendLog(QString::fromUtf8("GeoEast 历史结果已导出：BOFaultPolygon/%1").arg(lastGeoEastOutput_));
            QString gridError;
            if (view_->loadGrid(outputBasePath() + ".grid.txt", "matrix", &gridError)) {
                appendLog(QString::fromUtf8("原始网格视图文件：%1").arg(QDir::toNativeSeparators(outputBasePath() + ".grid.txt")));
            } else if (!gridError.isEmpty()) {
                appendLog(gridError);
            }
        }
        appendLog(QString::fromUtf8("结果 CSV：%1").arg(QDir::toNativeSeparators(lastOutputCsv_)));
        QString geoJsonPath;
        QString geoJsonError;
        if (writeGeoJson(lastOutputCsv_, &geoJsonPath, &geoJsonError)) {
            appendLog(QString::fromUtf8("GeoJSON 文件：%1").arg(QDir::toNativeSeparators(geoJsonPath)));
        } else if (!geoJsonError.isEmpty()) {
            appendLog(geoJsonError);
        }
        appendLog(QString::fromUtf8("诊断 JSON：%1").arg(QDir::toNativeSeparators(outputBasePath() + ".diagnostics.json")));
        appendLog(QString::fromUtf8("拓扑 CSV：%1").arg(QDir::toNativeSeparators(outputBasePath() + ".topology.csv")));
        appendLog(QString::fromUtf8("枝干 CSV：%1").arg(QDir::toNativeSeparators(outputBasePath() + ".branches.csv")));
        refreshResultPreview();
        if (finishedTask == ProcessTask::ExportGeoEastResult && inputEdit_->text().trimmed().isEmpty()) {
            setComboByKey(viewModeCombo_, QString::fromUtf8("结果多边形"));
        } else {
            setComboByKey(viewModeCombo_, QString::fromUtf8("叠加结果"));
        }
        appendHistory(elapsedMs);
        saveSettings();
        updateResultPanel();
        updateStatus(finishedTask == ProcessTask::GeoEastTrace
            ? QString::fromUtf8("GeoEast 追踪完成，结果已写入数据库并显示结果视图")
            : (finishedTask == ProcessTask::ExportGeoEastResult
                ? QString::fromUtf8("GeoEast 历史结果已导出并载入")
                : QString::fromUtf8("追踪完成，已切换到叠加结果")));
        if (doneBanner_ && doneBannerLabel_ &&
            (finishedTask == ProcessTask::ExternalTrace || finishedTask == ProcessTask::GeoEastTrace)) {
            const int polygonCount = view_ ? view_->polygonCount() : 0;
            doneBannerLabel_->setText(uiText(QString::fromUtf8("追踪完成：%1 个多边形，耗时 %2"))
                                          .arg(polygonCount)
                                          .arg(elapsedText(elapsedMs)));
            doneBanner_->show();
        }
        refreshAvailableResults();
    } else {
        updateResultPanel();
        updateStatus(QString::fromUtf8("运行失败，请查看日志"));
    }
}

void FaultPolyTraceWindow::onProcessError(QProcess::ProcessError error) {
    appendLog(QString::fromUtf8("进程错误：%1").arg(static_cast<int>(error)));
    setRunning(false);
    updateStatus(QString::fromUtf8("进程启动或运行异常"));
}

void FaultPolyTraceWindow::setRunning(bool running) {
    runButton_->setEnabled(!running);
    runButton_->setText(running ? uiText(QString::fromUtf8("追踪中…"))
                                : uiText(QString::fromUtf8("开始追踪")));
    stopButton_->setEnabled(running);
    if (busyBar_) {
        busyBar_->setVisible(running);
    }
    if (geoEastRefreshButton_) {
        geoEastRefreshButton_->setEnabled(!running && isGeoEastMode());
    }
    if (geoEastConfigButton_) {
        geoEastConfigButton_->setEnabled(!running && isGeoEastMode());
    }
    if (geoEastDeleteButton_) {
        geoEastDeleteButton_->setEnabled(!running && isGeoEastMode());
    }
    if (refreshResultsButton_) {
        refreshResultsButton_->setEnabled(!running);
    }
    if (loadSelectedResultButton_) {
        loadSelectedResultButton_->setEnabled(!running && resultCatalogCombo_ && resultCatalogCombo_->currentData().toString().size() > 0);
    }
    if (runAction_) {
        runAction_->setEnabled(!running);
    }
    if (stopAction_) {
        stopAction_->setEnabled(running);
    }
    updateResultPanel();
}

void FaultPolyTraceWindow::appendLog(const QString& text) {
    // 与 GeoEast 插件共用行级着色规则：错误红、完成绿、后台输出灰。
    QString html;
    for (const QString& line : text.split('\n', Qt::SkipEmptyParts)) {
        const QString localizedLine = uiText(fpt_ui::canonical(line));
        const fpt_ui::LogLineStyle style = fpt_ui::logLineStyle(localizedLine);
        if (style.isDebug && verboseLogCheck_ && !verboseLogCheck_->isChecked()) {
            pendingDebugLines_ << fpt_ui::canonical(line);
            if (pendingDebugLines_.size() > 2000) {
                pendingDebugLines_.removeFirst();
            }
            continue;
        }
        html += style.html;
    }
    if (!html.isEmpty()) {
        logEdit_->moveCursor(QTextCursor::End);
        logEdit_->insertHtml(html);
        logEdit_->moveCursor(QTextCursor::End);
    }
}

void FaultPolyTraceWindow::setPolygonColor(const QColor& color) {
    if (!colorEdit_) {
        return;
    }
    const QColor effective = color.isValid() ? color : QColor(0, 255, 255);
    const QString hex = effective.name(QColor::HexRgb).toUpper();
    colorEdit_->setText(hex);
    const QString textColor = effective.lightness() < 128 ? "#FFFFFF" : "#000000";
    colorEdit_->setStyleSheet(QString("background-color:%1; color:%2; font-weight:bold;")
        .arg(hex, textColor));
    if (showPolygonLayerCheck_) {
        showPolygonLayerCheck_->setAccentColor(effective);
    }
}

void FaultPolyTraceWindow::updateStatus(const QString& text) {
    statusLabel_->setText(QDateTime::currentDateTime().toString("HH:mm:ss  ") + uiText(text));
}

void FaultPolyTraceWindow::updateOptionControls() {
    const bool fixedThreshold = thresholdModeCombo_ && thresholdModeCombo_->currentText() == "fixed";
    if (thresholdSpin_) {
        thresholdSpin_->setEnabled(fixedThreshold);
    }
    if (percentileSpin_) {
        percentileSpin_->setEnabled(!fixedThreshold);
    }

    const bool attributeEnabled = attributeConstrainedBeltCheck_ && attributeConstrainedBeltCheck_->isChecked();
    if (attributeEnvelopeRadiusSpin_) {
        attributeEnvelopeRadiusSpin_->setEnabled(attributeEnabled);
    }

    const bool rootEnabled = rootAntiMergeCheck_ && rootAntiMergeCheck_->isChecked();
    for (QWidget* widget : {
             static_cast<QWidget*>(rootAntiMergeRadiusSpin_),
             static_cast<QWidget*>(rootAntiMergeSupportRatioSpin_),
             static_cast<QWidget*>(rootAntiMergeMinPartCellsSpin_),
             static_cast<QWidget*>(rootAntiMergeMinPartRatioSpin_),
             static_cast<QWidget*>(rootAntiMergeWeakGuardThresholdSpin_),
             static_cast<QWidget*>(rootAntiMergeWeakGuardRadiusSpin_)}) {
        if (widget) {
            widget->setEnabled(rootEnabled);
        }
    }

    const bool localEnabled = localAntiMergeCheck_ && localAntiMergeCheck_->isChecked();
    for (QWidget* widget : {
             static_cast<QWidget*>(localAntiMergeLowThresholdSpin_),
             static_cast<QWidget*>(localAntiMergeBridgeRadiusSpin_),
             static_cast<QWidget*>(localAntiMergeMinPartCellsSpin_),
             static_cast<QWidget*>(localAntiMergeMinPartRatioSpin_),
             static_cast<QWidget*>(localAntiMergeMinBridgeCellsSpin_),
             static_cast<QWidget*>(localAntiMergeMinLowRatioSpin_)}) {
        if (widget) {
            widget->setEnabled(localEnabled);
        }
    }

    const bool residualEnabled = residualTraceCheck_ && residualTraceCheck_->isChecked();
    for (QWidget* widget : {
             static_cast<QWidget*>(residualTraceMinLengthSpin_),
             static_cast<QWidget*>(residualTraceMaxCellsSpin_)}) {
        if (widget) {
            widget->setEnabled(residualEnabled);
        }
    }
}

void FaultPolyTraceWindow::updateLayerVisibility() {
    if (!view_ || !showGridLayerCheck_ || !showPolygonLayerCheck_) {
        return;
    }
    const bool showGrid = showGridLayerCheck_->isChecked();
    const bool showPolygons = showPolygonLayerCheck_->isChecked();
    if (viewModeCombo_) {
        if (!showGrid && !showPolygons) {
            QSignalBlocker blocker(viewModeCombo_);
            setComboByKey(viewModeCombo_, QString::fromUtf8("任务预览"));
            view_->setMode(AttributeView::Mode::Task);
        } else if (showGrid && showPolygons) {
            QSignalBlocker blocker(viewModeCombo_);
            setComboByKey(viewModeCombo_, QString::fromUtf8("叠加结果"));
            view_->setMode(AttributeView::Mode::Overlay);
        } else if (showGrid) {
            QSignalBlocker blocker(viewModeCombo_);
            setComboByKey(viewModeCombo_, QString::fromUtf8("原始网格"));
            view_->setMode(AttributeView::Mode::Raw);
        } else if (showPolygons) {
            QSignalBlocker blocker(viewModeCombo_);
            setComboByKey(viewModeCombo_, QString::fromUtf8("结果多边形"));
            view_->setMode(AttributeView::Mode::Result);
        }
    }
    view_->setLayerVisible(showGrid, showPolygons);
    if (showGrid && !isGeoEastMode() && !inputEdit_->text().trimmed().isEmpty() &&
        QFileInfo::exists(inputEdit_->text().trimmed())) {
        refreshInputPreview();
    }
    if (showPolygons && !lastOutputCsv_.trimmed().isEmpty() && QFileInfo::exists(lastOutputCsv_)) {
        refreshResultPreview();
    }
    updateTaskPreview();
}

void FaultPolyTraceWindow::syncPresetIndicator() {
    if (!presetCombo_ || presetSyncGuard_) {
        return;
    }
    const QString key = comboKey(presetCombo_);
    if (key.isEmpty() || key == QString::fromUtf8("自定义参数")) {
        return;
    }
    // 质量模式属于运行方式而非预设内容，比对前对齐，避免切换诊断模式被误判为改参数。
    fpt_ui::TraceParameters current = currentTraceParameters();
    fpt_ui::TraceParameters preset = fpt_ui::presetParameters(fpt_ui::canonical(key));
    preset.qualityMode = current.qualityMode;
    const bool matches = fpt_ui::traceArguments(current) == fpt_ui::traceArguments(preset);
    const int customIndex = presetCombo_->findData(QString::fromUtf8("自定义参数"));
    presetSyncGuard_ = true;
    if (!matches) {
        int index = customIndex;
        if (index < 0) {
            presetCombo_->addItem(uiText(QString::fromUtf8("自定义参数")), QString::fromUtf8("自定义参数"));
            index = presetCombo_->count() - 1;
        }
        presetCombo_->setCurrentIndex(index);
        presetCombo_->setToolTip(uiText(QString::fromUtf8("参数已手动调整；重新选择任一预设可整体恢复")));
    } else if (customIndex >= 0 && presetCombo_->currentIndex() == customIndex) {
        presetCombo_->removeItem(customIndex);
    }
    presetSyncGuard_ = false;
}

void FaultPolyTraceWindow::applyPreset(const QString& preset) {
    // “自定义参数”是同步指示项，不对应任何共享预设，选中它时不改动参数。
    const bool applicable = !presetSyncGuard_ &&
        fpt_ui::canonical(preset) != QString::fromUtf8("自定义参数");
    if (applicable) {
        presetSyncGuard_ = true;
        const int customIndex = presetCombo_ ? presetCombo_->findData(QString::fromUtf8("自定义参数")) : -1;
        if (customIndex >= 0 && presetCombo_->currentIndex() != customIndex) {
            presetCombo_->removeItem(customIndex);
        }
        presetSyncGuard_ = false;
        const fpt_ui::TraceParameters shared =
            fpt_ui::presetParameters(fpt_ui::canonical(preset));
        applyTraceParameters(shared);
    }
}

void FaultPolyTraceWindow::applyTraceParameters(const fpt_ui::TraceParameters& shared) {
    traceModeCombo_->setCurrentText(shared.traceMode);
    thresholdModeCombo_->setCurrentText(shared.thresholdMode);
    thresholdSpin_->setValue(shared.threshold);
    percentileSpin_->setValue(shared.percentile);
    minAreaSpin_->setValue(shared.minArea);
    closeRadiusSpin_->setValue(shared.closeRadius);
    weakPercentileSpin_->setValue(shared.weakPercentile);
    weakConnectRadiusSpin_->setValue(shared.weakConnectRadius);
    beltRadiusSpin_->setValue(shared.beltRadius);
    postBeltCloseRadiusSpin_->setValue(shared.postBeltCloseRadius);
    attributeConstrainedBeltCheck_->setChecked(shared.attributeConstrainedBelt);
    attributeEnvelopeRadiusSpin_->setValue(shared.attributeEnvelopeRadius);
    minBranchLengthSpin_->setValue(shared.minBranchLength);
    minJunctionBranchLengthSpin_->setValue(shared.minJunctionBranchLength);
    minJunctionBranchMeanSpin_->setValue(shared.minJunctionBranchMean);
    maxGapDistanceSpin_->setValue(shared.maxGapDistance);
    maxGapAngleSpin_->setValue(shared.maxGapAngle);
    rootAntiMergeCheck_->setChecked(shared.rootAntiMerge);
    rootAntiMergeRadiusSpin_->setValue(shared.rootAntiMergeRadius);
    rootAntiMergeSupportRatioSpin_->setValue(shared.rootAntiMergeSupportRatio);
    rootAntiMergeMinPartCellsSpin_->setValue(shared.rootAntiMergeMinPartCells);
    rootAntiMergeMinPartRatioSpin_->setValue(shared.rootAntiMergeMinPartRatio);
    rootAntiMergeWeakGuardThresholdSpin_->setValue(shared.rootAntiMergeWeakGuardThreshold);
    rootAntiMergeWeakGuardRadiusSpin_->setValue(shared.rootAntiMergeWeakGuardRadius);
    localAntiMergeCheck_->setChecked(shared.localAntiMerge);
    localAntiMergeLowThresholdSpin_->setValue(shared.localAntiMergeLowThreshold);
    localAntiMergeBridgeRadiusSpin_->setValue(shared.localAntiMergeBridgeRadius);
    localAntiMergeMinPartCellsSpin_->setValue(shared.localAntiMergeMinPartCells);
    localAntiMergeMinPartRatioSpin_->setValue(shared.localAntiMergeMinPartRatio);
    localAntiMergeMinBridgeCellsSpin_->setValue(shared.localAntiMergeMinBridgeCells);
    localAntiMergeMinLowRatioSpin_->setValue(shared.localAntiMergeMinLowRatio);
    residualTraceCheck_->setChecked(shared.residualTrace);
    residualTraceMinLengthSpin_->setValue(shared.residualTraceMinLength);
    residualTraceMaxCellsSpin_->setValue(shared.residualTraceMaxCells);
    simplifyToleranceSpin_->setValue(shared.simplifyTolerance);
    smoothIterationsSpin_->setValue(shared.smoothIterations);
    if (shared.qualityMode == fpt_ui::QualityMode::Diagnostics) {
        setComboByKey(qualityModeCombo_, QString::fromUtf8("质量诊断"));
    } else if (shared.qualityMode == fpt_ui::QualityMode::Experimental) {
        setComboByKey(qualityModeCombo_, QString::fromUtf8("受控实验"));
    } else {
        setComboByKey(qualityModeCombo_, QString::fromUtf8("正式处理"));
    }
    updateOptionControls();
    updateTaskPreview();
}

void FaultPolyTraceWindow::showOverlayResult() {
    if (lastOutputCsv_.isEmpty() || !QFileInfo::exists(lastOutputCsv_)) {
        QMessageBox::information(this,
                                 uiText(QString::fromUtf8("尚未生成结果")),
                                 uiText(QString::fromUtf8("请先选择输入文件并点击【开始追踪】。运行完成后才能查看处理后的叠加结果。")));
        return;
    }
    refreshResultPreview();
    setComboByKey(viewModeCombo_, QString::fromUtf8("叠加结果"));
    updateStatus(QString::fromUtf8("正在查看叠加结果"));
}

void FaultPolyTraceWindow::openResultCsv() {
    const QString path = lastOutputCsv_;
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this,
                             uiText(QString::fromUtf8("结果文件不存在")),
                             uiText(QString::fromUtf8("请先完成一次追踪，或从历史记录载入已有结果。")));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void FaultPolyTraceWindow::openResultGeoJson() {
    const QString csvPath = lastOutputCsv_;
    const QFileInfo csvInfo(csvPath);
    const QString path = csvInfo.absolutePath() + "/" + csvInfo.completeBaseName() + ".geojson";
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this,
                             uiText(QString::fromUtf8("GeoJSON 不存在")),
                             uiText(QString::fromUtf8("请先完成一次追踪。GeoJSON 会在结果 CSV 生成后自动写出。")));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void FaultPolyTraceWindow::openDiagnosticsJson() {
    const QString csvPath = lastOutputCsv_;
    const QFileInfo csvInfo(csvPath);
    const QString path = csvInfo.absolutePath() + "/" + csvInfo.completeBaseName() + ".diagnostics.json";
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this,
                             uiText(QString::fromUtf8("诊断 JSON 不存在")),
                             uiText(QString::fromUtf8("请先完成一次追踪。诊断 JSON 会随结果文件一起生成。")));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void FaultPolyTraceWindow::exportPng() {
    if (!view_->hasVisibleContent()) {
        QMessageBox::information(
            this,
            uiText(QString::fromUtf8("没有可导出的图层")),
            uiText(QString::fromUtf8("请先载入并显示原始网格或结果多边形图层，再导出当前视图 PNG。")));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, uiText(QString::fromUtf8("导出当前视图 PNG")), QDir(outputDirEdit_->text()).absoluteFilePath("FaultPolyTrace_View.png"), uiText(QString::fromUtf8("PNG 图像 (*.png);;所有文件 (*.*)")));
    if (path.isEmpty()) {
        return;
    }
    if (view_->savePng(path)) {
        appendLog(QString::fromUtf8("PNG 已导出：%1").arg(QDir::toNativeSeparators(path)));
    }
}

void FaultPolyTraceWindow::saveTemplate() {
    const QString path = QFileDialog::getSaveFileName(
        this,
        uiText(QString::fromUtf8("保存参数模板")),
        QDir(outputDirEdit_->text()).absoluteFilePath("FaultPolyTrace_参数模板.json"),
        uiText(QString::fromUtf8("参数模板 (*.json);;所有文件 (*.*)")));
    if (path.isEmpty()) {
        return;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, uiText(QString::fromUtf8("保存失败")),
                             uiText(QString::fromUtf8("无法写入参数模板：%1")).arg(QDir::toNativeSeparators(path)));
        return;
    }
    file.write(QJsonDocument(fpt_ui::traceParametersToJson(currentTraceParameters(), comboKey(presetCombo_)))
                   .toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        QMessageBox::warning(this, uiText(QString::fromUtf8("保存失败")),
                             uiText(QString::fromUtf8("参数模板未能安全写入磁盘。")));
        return;
    }
    appendLog(QString::fromUtf8("参数模板已保存：%1").arg(QDir::toNativeSeparators(path)));
    updateStatus(QString::fromUtf8("参数模板已保存"));
}

void FaultPolyTraceWindow::loadTemplate() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        uiText(QString::fromUtf8("打开参数模板")),
        outputDirEdit_->text(),
        uiText(QString::fromUtf8("参数模板 (*.json);;所有文件 (*.*)")));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, uiText(QString::fromUtf8("打开失败")),
                             uiText(QString::fromUtf8("无法读取参数模板：%1")).arg(QDir::toNativeSeparators(path)));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        QMessageBox::warning(this, uiText(QString::fromUtf8("模板无效")),
                             uiText(QString::fromUtf8("JSON 解析失败：%1")).arg(parseError.errorString()));
        return;
    }
    fpt_ui::TraceParameters parameters = currentTraceParameters();
    QString preset;
    QString validationError;
    if (!fpt_ui::traceParametersFromJson(document.object(), &parameters, &preset, &validationError)) {
        QMessageBox::warning(this, uiText(QString::fromUtf8("模板无效")), uiText(validationError));
        return;
    }
    if (!preset.isEmpty() && fpt_ui::presetKeys().contains(preset)) {
        QSignalBlocker blocker(presetCombo_);
        setComboByKey(presetCombo_, preset);
    }
    applyTraceParameters(parameters);
    saveSettings();
    appendLog(QString::fromUtf8("参数模板已载入：%1").arg(QDir::toNativeSeparators(path)));
    updateStatus(QString::fromUtf8("参数模板已载入并通过校验"));
}

void FaultPolyTraceWindow::saveRunLog() {
    // 保存时带上界面折叠的后台输出，排障不缺原文。
    QString logText = logEdit_ ? logEdit_->toPlainText().trimmed() : QString();
    if (!pendingDebugLines_.isEmpty()) {
        logText += QString::fromUtf8("\n---- 详细日志（界面已折叠）----\n") +
                   pendingDebugLines_.join('\n');
    }
    if (logText.isEmpty()) {
        QMessageBox::information(this, uiText(QString::fromUtf8("日志为空")),
                                 uiText(QString::fromUtf8("当前没有可保存的运行日志。")));
        return;
    }
    const QString suggested = QDir(outputDirEdit_->text()).absoluteFilePath(
        QString("FaultPolyTrace_%1.log").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(
        this, uiText(QString::fromUtf8("保存运行日志")), suggested,
        uiText(QString::fromUtf8("日志文件 (*.log *.txt);;所有文件 (*.*)")));
    if (path.isEmpty()) {
        return;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(logText.toUtf8()) < 0 || !file.commit()) {
        QMessageBox::warning(this, uiText(QString::fromUtf8("保存失败")),
                             uiText(QString::fromUtf8("无法保存运行日志：%1")).arg(QDir::toNativeSeparators(path)));
        return;
    }
    updateStatus(QString::fromUtf8("运行日志已保存"));
}

void FaultPolyTraceWindow::copyResultSummary() {
    const QString source = isGeoEastMode()
        ? QString::fromUtf8("GeoEast数据库：%1 / %2 / BOMapGrid/%3")
              .arg(databaseEdit_->text().trimmed(), projectEdit_->text().trimmed(), geoEastGridCombo_->currentText().trimmed())
        : QString::fromUtf8("外部文件：%1").arg(QDir::toNativeSeparators(inputEdit_->text().trimmed()));
    const QString result = isGeoEastMode() && !lastGeoEastOutput_.trimmed().isEmpty()
        ? QString::fromUtf8("BOFaultPolygon/%1").arg(lastGeoEastOutput_.trimmed())
        : QDir::toNativeSeparators(lastOutputCsv_.trimmed());
    const QString summary = QString::fromUtf8(
        "FaultPolyTrace 结果摘要\n数据源：%1\n结果：%2\n预设模板：%3\n质量模式：%4\n"
        "显示样式：%5，线宽 %6，透明度 %7\n%8\n%9")
        .arg(source)
        .arg(result.isEmpty() ? uiText(QString::fromUtf8("尚未生成结果")) : result)
        .arg(comboKey(presetCombo_))
        .arg(comboKey(qualityModeCombo_))
        .arg(colorEdit_->text())
        .arg(QString::number(lineWidthSpin_->value(), 'f', 3))
        .arg(QString::number(alphaSpin_->value(), 'f', 2))
        .arg(elapsedLabel_ ? elapsedLabel_->text() : QString())
        .arg(resultSummaryLabel_ ? resultSummaryLabel_->text() : QString());
    QApplication::clipboard()->setText(summary);
    updateStatus(QString::fromUtf8("当前结果摘要已复制"));
}

void FaultPolyTraceWindow::restoreDefaults() {
    setComboByKey(presetCombo_, QString::fromUtf8("防黏连增强"));
    applyPreset(comboKey(presetCombo_));
    thresholdModeCombo_->setCurrentText("percentile");
    setPolygonColor(QColor("#00FFFF"));
    lineWidthSpin_->setValue(0.03);
    alphaSpin_->setValue(1.0);
    traceModeCombo_->setCurrentText("ridge-belt");
    updateOptionControls();
    updateTaskPreview();
    updateResultPanel();
    updateStatus(QString::fromUtf8("已恢复推荐算法与显示参数，输入文件和输出位置保持不变"));
}

void FaultPolyTraceWindow::appendHistory(qint64 elapsedMs) {
    QFile file(historyPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    QStringList fields = {
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"),
        inputEdit_->text().trimmed(),
        lastOutputCsv_,
        QString::number(elapsedMs),
        comboKey(presetCombo_)
    };
    for (QString& field : fields) {
        field.replace('\t', ' ');
        field.replace('\n', ' ');
    }
    stream << fields.join('\t') << "\n";
    refreshHistory();
}

void FaultPolyTraceWindow::refreshHistory() {
    historyCombo_->clear();
    QFile file(historyPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        historyCombo_->addItem(QString::fromUtf8("暂无历史记录"), QString());
        return;
    }
    const QStringList lines = QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
    for (int i = lines.size() - 1; i >= qMax(0, lines.size() - 30); --i) {
        const QStringList parts = lines[i].split('\t');
        if (parts.size() < 5) {
            continue;
        }
        historyCombo_->addItem(QString::fromUtf8("%1  %2").arg(parts[0], QFileInfo(parts[2]).fileName()), lines[i]);
    }
    if (historyCombo_->count() == 0) {
        historyCombo_->addItem(QString::fromUtf8("暂无历史记录"), QString());
    }
}

void FaultPolyTraceWindow::loadHistory() {
    const QString line = historyCombo_->currentData().toString();
    const QStringList parts = line.split('\t');
    if (parts.size() < 5) {
        return;
    }
    inputEdit_->setText(parts[1]);
    QFileInfo out(parts[2]);
    outputDirEdit_->setText(out.absolutePath());
    outputNameEdit_->setText(out.completeBaseName());
    setComboByKey(presetCombo_, parts[4]);
    lastOutputCsv_ = parts[2];
    refreshInputPreview();
    refreshResultPreview();
    setComboByKey(viewModeCombo_, QString::fromUtf8("叠加结果"));
    updateResultPanel();
}

void FaultPolyTraceWindow::openOutputDir() {
    const QString dir = outputDirEdit_->text().trimmed().isEmpty() ? defaultOutputDir() : outputDirEdit_->text().trimmed();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void FaultPolyTraceWindow::updateResultPanel() {
    if (!resultSummaryLabel_ || !resultPathLabel_) {
        return;
    }

    const bool hasCurrentResult = !lastOutputCsv_.trimmed().isEmpty();
    const QString csvPath = hasCurrentResult ? lastOutputCsv_ : outputCsvPath();
    const QFileInfo csvInfo(csvPath);
    const QString base = csvInfo.absolutePath() + "/" + csvInfo.completeBaseName();
    const QString geoJsonPath = base + ".geojson";
    const QString diagnosticsPath = base + ".diagnostics.json";
    const bool hasCsv = hasCurrentResult && csvInfo.exists() && csvInfo.size() > 0;
    const bool hasGeoJson = hasCurrentResult && QFileInfo::exists(geoJsonPath);
    const bool hasDiagnostics = hasCurrentResult && QFileInfo::exists(diagnosticsPath);
    const bool running = process_ && process_->state() != QProcess::NotRunning;
    const bool hasInput = isGeoEastMode()
        ? !geoEastGridCombo_->currentText().trimmed().isEmpty()
        : (!inputEdit_->text().trimmed().isEmpty() && QFileInfo::exists(inputEdit_->text().trimmed()));

    if (running) {
        resultSummaryLabel_->setText(uiText(QString::fromUtf8("正在计算，结果文件生成后会自动显示。")));
    } else if (hasCsv) {
        resultSummaryLabel_->setText(isGeoEastMode() && !lastGeoEastOutput_.isEmpty()
            ? uiText(QString::fromUtf8("结果已写入 GeoEast 数据库，并已生成本地视图文件。"))
            : uiText(QString::fromUtf8("最近结果已生成，可在中间视图区查看或打开结果文件。")));
    } else if (isGeoEastMode() && !lastGeoEastOutput_.isEmpty()) {
        resultSummaryLabel_->setText(uiText(QString::fromUtf8("已选择 GeoEast 数据库结果；当前没有同名本地视图缓存。")));
    } else if (!lastOutputCsv_.isEmpty()) {
        resultSummaryLabel_->setText(uiText(QString::fromUtf8("运行结束，但未找到结果 CSV。请检查日志和输出目录。")));
    } else if (hasInput) {
        resultSummaryLabel_->setText(uiText(QString::fromUtf8("已载入原始网格，尚未运行追踪；当前不是处理后的叠加结果。")));
    } else {
        resultSummaryLabel_->setText(uiText(QString::fromUtf8("请选择输入文件或 GeoEast 网格，然后点击【开始追踪】。")));
    }

    if (hasCsv && isGeoEastMode() && !lastGeoEastOutput_.isEmpty()) {
        resultPathLabel_->setText(uiText(QString::fromUtf8("GeoEast对象：BOFaultPolygon/%1\n本地CSV：%2"))
            .arg(lastGeoEastOutput_, QDir::toNativeSeparators(csvPath)));
    } else if (isGeoEastMode() && !lastGeoEastOutput_.isEmpty()) {
        resultPathLabel_->setText(uiText(QString::fromUtf8("GeoEast对象：BOFaultPolygon/%1\n本地缓存：%2"))
            .arg(lastGeoEastOutput_, QDir::toNativeSeparators(csvPath)));
    } else {
        resultPathLabel_->setText(hasCsv
            ? uiText(QString::fromUtf8("CSV：%1")).arg(QDir::toNativeSeparators(csvPath))
            : uiText(QString::fromUtf8("预计输出：%1")).arg(QDir::toNativeSeparators(csvPath)));
    }
    resultPathLabel_->setToolTip(QDir::toNativeSeparators(csvPath));

    if (openCsvMenuAction_) {
        openCsvMenuAction_->setEnabled(hasCsv);
    }
    if (openGeoJsonMenuAction_) {
        openGeoJsonMenuAction_->setEnabled(hasGeoJson);
    }
    if (openDiagnosticsMenuAction_) {
        openDiagnosticsMenuAction_->setEnabled(hasDiagnostics);
    }
    if (exportPngButton_) {
        exportPngButton_->setEnabled(true);
    }
    if (loadSelectedResultButton_) {
        loadSelectedResultButton_->setEnabled(resultCatalogCombo_ && !resultCatalogCombo_->currentData().toString().isEmpty() && !running);
    }
}

void FaultPolyTraceWindow::openHelp() {
    QMessageBox::information(this, uiText(QString::fromUtf8("FaultPolyTrace 帮助")),
                             uiText(QString::fromUtf8("FaultPolyTrace 独立软件支持两种数据源。\n\n"
                                                       "外部文件流程：\n"
                                                       "1. 数据源选择【外部文件】。\n"
                                                       "2. 选择 DAT、矩阵文本或 GeoEast DAT 文件。\n"
                                                       "3. 设置输出目录、输出名称和追踪参数。\n"
                                                       "4. 点击【开始追踪】，完成后自动切换到【叠加结果】。\n\n"
                                                       "GeoEast 数据库流程：\n"
                                                       "1. 数据源选择【GeoEast数据库】。\n"
                                                       "2. 点击【数据库配置】确认 GeoEast 根目录、数据根目录、数据库、项目和用户。\n"
                                                       "3. 点击【刷新网格】读取 GeoEast 输入网格列表。\n"
                                                       "4. 设置输出多边形名称后点击【开始追踪】。\n"
                                                       "5. 结果写入 GeoEast 数据库，同时在本软件生成结果视图文件用于查看。\n\n"
                                                       "在左侧【结果查看】中打开 CSV、GeoJSON、诊断 JSON 或输出目录，也可导出当前 PNG。\n\n"
                                                       "完整操作说明请打开【帮助】>【用户操作手册】。")));
}

void FaultPolyTraceWindow::openUserManual() {
    openBundledDocument(QString::fromUtf8("docs/用户操作手册.md"), uiText(QString::fromUtf8("用户操作手册")));
}

void FaultPolyTraceWindow::openProductManual() {
    const QString pdfPath = locateBundledFile(QString::fromUtf8("docs/FaultPolyTrace企业级用户操作手册与产品说明书_V1.0.1.pdf"));
    if (!pdfPath.isEmpty()) {
        if (QDesktopServices::openUrl(QUrl::fromLocalFile(pdfPath))) {
            updateStatus(uiText(QString::fromUtf8("已打开文档：%1")).arg(uiText(QString::fromUtf8("产品说明书"))));
            return;
        }
    }
    openBundledDocument(QString::fromUtf8("docs/企业级用户操作手册与产品说明书.md"), uiText(QString::fromUtf8("产品说明书")));
}

void FaultPolyTraceWindow::openProjectStructureDoc() {
    openBundledDocument(QString::fromUtf8("docs/项目目录结构与文件说明.md"), uiText(QString::fromUtf8("项目目录结构")));
}

void FaultPolyTraceWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}

bool FaultPolyTraceWindow::writeGeoJson(const QString& csvPath, QString* geoJsonPath, QString* errorMessage) const {
    QFile csv(csvPath);
    if (!csv.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8("无法读取 CSV：%1").arg(csvPath);
        }
        return false;
    }
    const QString outPath = QFileInfo(csvPath).absolutePath() + "/" + QFileInfo(csvPath).completeBaseName() + ".geojson";
    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&out);
    stream.setCodec("UTF-8");
    stream << "{\n  \"type\": \"FeatureCollection\",\n  \"features\": [\n";
    QRegularExpression ringRe("\\(([^()]+)\\)");
    bool first = true;
    for (const QString& line : QString::fromLocal8Bit(csv.readAll()).split('\n', Qt::SkipEmptyParts)) {
        if (!line.contains("POLYGON", Qt::CaseInsensitive) || !line.contains("kept", Qt::CaseInsensitive)) {
            continue;
        }
        const int wktStart = line.indexOf("POLYGON", 0, Qt::CaseInsensitive);
        const QStringList fields = line.left(wktStart).remove('"').split(',');
        if (fields.size() < 7) {
            continue;
        }
        const QString wkt = line.mid(wktStart).remove('"');
        QStringList rings;
        QRegularExpressionMatchIterator it = ringRe.globalMatch(wkt);
        while (it.hasNext()) {
            QStringList points;
            for (const QString& point : it.next().captured(1).split(',', Qt::SkipEmptyParts)) {
                const QStringList xy = point.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (xy.size() >= 2) {
                    points << QString("[%1,%2]").arg(xy[0], xy[1]);
                }
            }
            if (points.size() >= 4) {
                rings << "[" + points.join(",") + "]";
            }
        }
        if (rings.isEmpty()) {
            continue;
        }
        if (!first) {
            stream << ",\n";
        }
        first = false;
        stream << "    {\"type\":\"Feature\",\"properties\":{\"id\":" << fields[0].trimmed()
               << ",\"area\":" << fields[2].trimmed()
               << ",\"perimeter\":" << fields[3].trimmed()
               << ",\"cell_count\":" << fields[4].trimmed()
               << "},\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[" << rings.join(",") << "]}}";
    }
    stream << "\n  ]\n}\n";
    if (geoJsonPath) {
        *geoJsonPath = outPath;
    }
    return true;
}
