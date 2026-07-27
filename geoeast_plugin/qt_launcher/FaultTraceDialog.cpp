#include "FaultTraceDialog.h"
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
#include <QClipboard>
#include <QColor>
#include <QColorDialog>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QStandardPaths>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QTextEdit>
#include <QTextCursor>
#include <QTextStream>
#include <QToolBar>
#include <QToolButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <functional>
#include <cmath>
#include <limits>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace {

#ifdef Q_OS_WIN
DWORD findProcessIdByExecutable(const QString& executableName) {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    DWORD result = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (QString::fromWCharArray(entry.szExeFile).compare(executableName, Qt::CaseInsensitive) == 0) {
                result = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

bool isProcessAlive(qint64 pid) {
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!process) {
        return false;
    }
    DWORD exitCode = 0;
    const bool alive = GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(process);
    return alive;
}

struct WindowSearchContext {
    DWORD processId = 0;
    HWND window = nullptr;
};

BOOL CALLBACK findVisibleWindow(HWND window, LPARAM data) {
    auto* context = reinterpret_cast<WindowSearchContext*>(data);
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == context->processId && IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr) {
        context->window = window;
        return FALSE;
    }
    return TRUE;
}

bool activateApplicationWindow(const QString& executableName) {
    const DWORD processId = findProcessIdByExecutable(executableName);
    if (!processId) {
        return false;
    }
    WindowSearchContext context;
    context.processId = processId;
    EnumWindows(findVisibleWindow, reinterpret_cast<LPARAM>(&context));
    if (!context.window) {
        return false;
    }
    ShowWindow(context.window, SW_RESTORE);
    SetForegroundWindow(context.window);
    return true;
}
#else
bool isProcessAlive(qint64) { return false; }
bool activateApplicationWindow(const QString&) { return false; }
#endif

QString launcherSettingsPath() {
    QString directory = QString::fromLocal8Bit(qgetenv("FPT_CONFIG_ROOT")).trimmed();
    if (directory.isEmpty()) {
        directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    }
    if (directory.isEmpty()) {
        directory = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("config");
    }
    QDir().mkpath(directory);
    return QDir(directory).filePath("faultpolytrace_launcher.ini");
}

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

QString locateBundledPath(const QString& relativePath) {
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

QString envOrDefault(const char* name, const QString& fallback) {
    const QByteArray value = qgetenv(name);
    return value.isEmpty() ? fallback : QString::fromLocal8Bit(value);
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

    const QString currentRoot = geoEastRootFromPath(QDir::currentPath());
    if (!currentRoot.isEmpty()) {
        return currentRoot;
    }

    const QString userProfile = envValue("USERPROFILE");
    const QStringList candidates = {
        userProfile.isEmpty() ? QString() : QDir(userProfile).absoluteFilePath("GeoEast"),
        "C:/GeoEast",
        "D:/GeoEast",
        "C:/Program Files/GeoEast"
    };
    return firstExistingPath(candidates);
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
        "1. 执行程序文件：通常位于 GeoEast-RC\\bin\\faultpolytrace_geoeast_runner.exe，GeoEast 插件部署后会自动检测。\n"
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

QString defaultOfficialDatFromEnvironment() {
    const QStringList candidates = {
        envValue("FPT_OFFICIAL_DAT"),
        envValue("FPT_INPUT_DAT")
    };
    return firstExistingPath(candidates);
}

QString settingOrCurrent(const QSettings& settings, const QString& key, const QString& current, int schemaVersion) {
    return schemaVersion < 2 ? current : settings.value(key, current).toString();
}

QString userExportDirectory() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }
    const QString path = QDir(base).absoluteFilePath("FaultPolyTrace");
    QDir().mkpath(path);
    return path;
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

QString sanitizeFileComponent(const QString& text, const QString& fallback) {
    QString value = text.trimmed();
    if (value.isEmpty()) {
        value = fallback;
    }
    value.replace(QRegularExpression("[\\\\/:*?\"<>|\\s]+"), "_");
    value = value.left(80);
    return value.isEmpty() ? fallback : value;
}



class RenderViewWidget : public QWidget {
public:
    explicit RenderViewWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(560, 420);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMouseTracking(true);
        setCursor(Qt::OpenHandCursor);
    }

    void setPanMode(bool enabled) {
        panMode_ = enabled;
        setCursor(enabled ? Qt::OpenHandCursor : Qt::ArrowCursor);
        update();
    }

    void zoomIn() {
        zoom_ = qMin(3.0, zoom_ * 1.25);
        update();
    }

    void zoomOut() {
        zoom_ = qMax(0.5, zoom_ / 1.25);
        update();
    }

    void resetView() {
        panMode_ = true;
        dragging_ = false;
        zoom_ = 1.0;
        panOffset_ = QPointF(0, 0);
        setCursor(Qt::OpenHandCursor);
        update();
    }

    void setContext(const QString& database,
                    const QString& project,
                    const QString& grid,
                    const QString& output,
                    const QString& preset,
                    const QString& traceMode,
                    const QString& thresholdMode,
                    double percentile,
                    double threshold,
                    double minArea,
                    const QColor& polygonColor,
                    double polygonLineWidth,
                    double polygonOpacity) {
        database_ = database;
        project_ = project;
        gridName_ = grid;
        output_ = output;
        preset_ = preset;
        traceMode_ = traceMode;
        thresholdMode_ = thresholdMode;
        percentile_ = percentile;
        threshold_ = threshold;
        minArea_ = minArea;
        polygonColor_ = polygonColor.isValid() ? polygonColor : QColor("#00FFFF");
        polygonLineWidth_ = polygonLineWidth;
        polygonOpacity_ = qBound(0.05, polygonOpacity, 1.0);
        update();
    }

    void setLayerVisibility(bool showInputLayer, bool showResultLayer) {
        showInputLayer_ = showInputLayer;
        showResultLayer_ = showResultLayer;
        update();
    }

    void setLanguage(UiLanguage language) {
        languageCode_ = language == UiLanguage::English ? "en" : "zh-CN";
        update();
    }

    void setActivity(const QString& activity) {
        activity_ = activity;
        update();
    }

    void setCoordinateCallback(std::function<void(const QPointF&)> callback) {
        coordinateCallback_ = std::move(callback);
    }

    bool loadGridFile(const QString& path, QString* error) {
        const QFileInfo sourceInfo(path);
        const QString canonicalPath = sourceInfo.absoluteFilePath();
        const qint64 sourceMtime = sourceInfo.exists() ? sourceInfo.lastModified().toMSecsSinceEpoch() : -1;
        if (sourceInfo.exists() && canonicalPath == gridCachePath_ &&
            sourceInfo.size() == gridCacheSize_ && sourceMtime == gridCacheMtime_ &&
            nx_ > 0 && ny_ > 0 && !grid_.isEmpty()) {
            return true;
        }
        gridImageDirty_ = true;
        grid_.clear();
        nx_ = 0;
        ny_ = 0;
        xMin_ = 0.0;
        yMin_ = 0.0;
        xMax_ = 1.0;
        yMax_ = 1.0;
        valueMin_ = 0.0;
        valueMax_ = 1.0;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (error) {
                *error = QString::fromUtf8("无法打开原始网格导出文件：%1").arg(path);
            }
            update();
            return false;
        }

        const QStringList lines = QString::fromLocal8Bit(file.readAll())
            .split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        if (lines.size() < 4) {
            if (error) {
                *error = QString::fromUtf8("原始网格导出文件格式不完整。");
            }
            update();
            return false;
        }

        auto splitNumbers = [](const QString& text) {
            QVector<double> values;
            QRegularExpression numberRe("[-+]?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?");
            QRegularExpressionMatchIterator it = numberRe.globalMatch(text);
            while (it.hasNext()) {
                values << it.next().captured(0).toDouble();
            }
            return values;
        };

        const QVector<double> dims = splitNumbers(lines[0]);
        const QVector<double> origin = splitNumbers(lines[1]);
        const QVector<double> spacing = splitNumbers(lines[2]);
        if (dims.size() < 2 || origin.size() < 2 || spacing.size() < 2) {
            if (error) {
                *error = QString::fromUtf8("原始网格导出文件头无效。");
            }
            update();
            return false;
        }

        nx_ = qRound(dims[0]);
        ny_ = qRound(dims[1]);
        if (nx_ <= 0 || ny_ <= 0) {
            if (error) {
                *error = QString::fromUtf8("原始网格尺寸无效。");
            }
            update();
            return false;
        }

        grid_.reserve(nx_ * ny_);
        for (int i = 3; i < lines.size(); ++i) {
            const QVector<double> values = splitNumbers(lines[i]);
            for (double value : values) {
                grid_ << value;
            }
        }
        if (grid_.size() < nx_ * ny_) {
            if (error) {
                *error = QString::fromUtf8("原始网格数值数量不足。");
            }
            update();
            return false;
        }
        grid_.resize(nx_ * ny_);

        const double dx = spacing[0] == 0.0 ? 1.0 : spacing[0];
        const double dy = spacing[1] == 0.0 ? 1.0 : spacing[1];
        xMin_ = origin[0];
        yMin_ = origin[1];
        xMax_ = xMin_ + dx * qMax(1, nx_ - 1);
        yMax_ = yMin_ + dy * qMax(1, ny_ - 1);

        valueMin_ = std::numeric_limits<double>::max();
        valueMax_ = -std::numeric_limits<double>::max();
        for (double value : grid_) {
            if (!std::isfinite(value)) {
                continue;
            }
            valueMin_ = qMin(valueMin_, value);
            valueMax_ = qMax(valueMax_, value);
        }
        if (valueMin_ == std::numeric_limits<double>::max()) {
            valueMin_ = 0.0;
            valueMax_ = 1.0;
        }
        if (qFuzzyCompare(valueMin_, valueMax_)) {
            valueMax_ = valueMin_ + 1.0;
        }
        dataStatus_ = QString::fromUtf8("原始网格：%1 x %2").arg(nx_).arg(ny_);
        gridCachePath_ = canonicalPath;
        gridCacheSize_ = sourceInfo.size();
        gridCacheMtime_ = sourceMtime;
        update();
        return true;
    }

    bool loadPolygonCsv(const QString& path, QString* error) {
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
                *error = QString::fromUtf8("无法打开结果多边形导出文件：%1").arg(path);
            }
            update();
            return false;
        }

        const QStringList lines = QString::fromLocal8Bit(file.readAll())
            .split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        QRegularExpression numberRe("[-+]?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?");
        double pxMin = std::numeric_limits<double>::max();
        double pyMin = std::numeric_limits<double>::max();
        double pxMax = -std::numeric_limits<double>::max();
        double pyMax = -std::numeric_limits<double>::max();
        int keptPolygonCount = 0;

        auto parseRing = [&](const QString& ringText) {
            QRegularExpressionMatchIterator it = numberRe.globalMatch(ringText);
            QVector<double> numbers;
            while (it.hasNext()) {
                numbers << it.next().captured(0).toDouble();
            }

            QPolygonF ring;
            for (int i = 0; i + 1 < numbers.size(); i += 2) {
                const QPointF point(numbers[i], numbers[i + 1]);
                ring << point;
                pxMin = qMin(pxMin, point.x());
                pyMin = qMin(pyMin, point.y());
                pxMax = qMax(pxMax, point.x());
                pyMax = qMax(pyMax, point.y());
            }
            if (ring.size() >= 3) {
                polygons_ << ring;
            }
        };

        for (const QString& line : lines) {
            if (!line.contains("POLYGON", Qt::CaseInsensitive) ||
                !line.contains("kept", Qt::CaseInsensitive)) {
                continue;
            }
            const QString wkt = line.mid(line.indexOf("POLYGON", 0, Qt::CaseInsensitive));
            const int start = wkt.indexOf("((");
            const int end = wkt.lastIndexOf("))");
            if (start < 0 || end <= start + 2) {
                continue;
            }
            ++keptPolygonCount;
            const QString body = wkt.mid(start + 2, end - start - 2);
            const QStringList rings = body.split("),(", Qt::SkipEmptyParts);
            for (const QString& ring : rings) {
                parseRing(ring);
            }
        }

        if (grid_.isEmpty() && !polygons_.isEmpty()) {
            xMin_ = pxMin;
            yMin_ = pyMin;
            xMax_ = pxMax;
            yMax_ = pyMax;
        }
        dataStatus_ = polygons_.isEmpty()
            ? QString::fromUtf8("未发现保留多边形")
            : QString::fromUtf8("结果多边形：%1 个，显示环：%2 条").arg(keptPolygonCount).arg(polygons_.size());
        if (!polygons_.isEmpty()) {
            polygonCachePath_ = canonicalPath;
            polygonCacheSize_ = sourceInfo.size();
            polygonCacheMtime_ = sourceMtime;
        }
        update();
        return !polygons_.isEmpty();
    }

    bool hasVisibleContent() const {
        return (showInputLayer_ && !grid_.isEmpty()) ||
               (showResultLayer_ && !polygons_.isEmpty());
    }

    QString dataStatus() const {
        return dataStatus_;
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

        painter.setPen(QPen(QColor("#F1F1F1"), 1));
        const int step = qMax(25, qRound(50 * zoom_));
        for (int x = 0; x < width(); x += step) {
            painter.drawLine(x, 0, x, height());
        }
        for (int y = 0; y < height(); y += step) {
            painter.drawLine(0, y, width(), y);
        }

        painter.setPen(QPen(QColor("#E7E7E7"), 1));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));

        const QRectF canvas = rect().adjusted(24, 56, -24, -24);
        if (!grid_.isEmpty() || !polygons_.isEmpty()) {
            const QRectF contentCanvas = transformedCanvas(canvas);
            painter.save();
            painter.setClipRect(canvas);
            if (!grid_.isEmpty() && showInputLayer_) {
                drawGrid(painter, contentCanvas);
            }
            if (!polygons_.isEmpty() && showResultLayer_) {
                drawPolygons(painter, contentCanvas);
            }
            painter.restore();
        }

        QFont titleFont = painter.font();
        titleFont.setPointSize(16);
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.setPen(QColor("#7A7A7A"));
        const QString viewTitle = showInputLayer_ && showResultLayer_
            ? uiText(QString::fromUtf8("[结果视图区] 原始网格 + 结果多边形"))
            : (showInputLayer_ ? uiText(QString::fromUtf8("[结果视图区] 原始网格"))
            : (showResultLayer_ ? uiText(QString::fromUtf8("[结果视图区] 结果多边形"))
            : uiText(QString::fromUtf8("[结果视图区] 图层显示已关闭"))));
        painter.drawText(QPoint(26, 38), viewTitle);

        QFont hintFont = painter.font();
        hintFont.setPointSize(10);
        hintFont.setBold(false);
        if (grid_.isEmpty() && polygons_.isEmpty()) {
            drawGuideCard(painter, rect().adjusted(24, 56, -24, -24));
        }

        painter.setFont(hintFont);
        painter.setPen(QColor("#8A8A8A"));
        painter.drawText(QRect(width() - 210, 18, 180, 20), Qt::AlignRight,
                         uiText(QString::fromUtf8("%1  缩放 %2%"))
                             .arg(panMode_ ? uiText(QString::fromUtf8("平移模式")) : uiText(QString::fromUtf8("查看模式")))
                             .arg(qRound(zoom_ * 100)));
        painter.setFont(hintFont);
        painter.setPen(QColor("#666666"));
        painter.drawText(QRect(width() - 360, 38, 330, 20), Qt::AlignRight,
                         dataStatus_.isEmpty() ? uiText(activity_) : uiText(dataStatus_));
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (panMode_ && event->button() == Qt::LeftButton) {
            dragging_ = true;
            lastDragPos_ = event->pos();
            setCursor(Qt::ClosedHandCursor);
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (coordinateCallback_) {
            coordinateCallback_(viewToWorld(event->pos()));
        }
        if (dragging_) {
            const QPointF delta = event->pos() - lastDragPos_;
            panOffset_ += delta;
            lastDragPos_ = event->pos();
            update();
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && dragging_) {
            dragging_ = false;
            setCursor(panMode_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        }
        QWidget::mouseReleaseEvent(event);
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
        QWidget::wheelEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            resetView();
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void leaveEvent(QEvent* event) override {
        if (coordinateCallback_) {
            coordinateCallback_(QPointF(-1.0, -1.0));
        }
        QWidget::leaveEvent(event);
    }

private:
    void drawGuideCard(QPainter& painter, const QRectF& canvas) const {
        // 空状态三步引导：数据库配置 → 选择网格 → 开始追踪。
        const double cardW = qMin(430.0, canvas.width() - 48.0);
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
                         uiText(QString::fromUtf8("三步完成工区断层多边形追踪")));
        const QStringList steps = {
            uiText(QString::fromUtf8("确认数据库连接与当前工区（自动检测可一键填充）")),
            uiText(QString::fromUtf8("刷新并选择输入 BOMapGrid，填写输出多边形名称")),
            uiText(QString::fromUtf8("单击「开始追踪」，完成后开启结果图层查看")),
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
        const double t = qBound(0.0, (value - valueMin_) / (valueMax_ - valueMin_), 1.0);
        const double gray = 1.0 - 0.86 * t;
        return QColor::fromRgbF(gray, gray, gray);
    }

    QRectF transformedCanvas(const QRectF& canvas) const {
        const QPointF center = canvas.center() + panOffset_;
        return QRectF(center.x() - canvas.width() * zoom_ / 2.0,
                      center.y() - canvas.height() * zoom_ / 2.0,
                      canvas.width() * zoom_,
                      canvas.height() * zoom_);
    }

    QRectF fittedRect(const QRectF& available) const {
        const double dataWidth = qMax(1e-9, xMax_ - xMin_);
        const double dataHeight = qMax(1e-9, yMax_ - yMin_);
        const double scale = qMin(available.width() / dataWidth, available.height() / dataHeight);
        const double width = dataWidth * scale;
        const double height = dataHeight * scale;
        return QRectF(available.center().x() - width / 2.0,
                      available.center().y() - height / 2.0,
                      width,
                      height);
    }

    QPointF mapPoint(const QPointF& point, const QRectF& target) const {
        const double xRange = qMax(1e-9, xMax_ - xMin_);
        const double yRange = qMax(1e-9, yMax_ - yMin_);
        return QPointF(target.left() + (point.x() - xMin_) / xRange * target.width(),
                       target.bottom() - (point.y() - yMin_) / yRange * target.height());
    }

    QPointF viewToWorld(const QPointF& point) const {
        if (grid_.isEmpty() && polygons_.isEmpty()) {
            return QPointF(point.x() / zoom_, point.y() / zoom_);
        }
        const QRectF canvas = rect().adjusted(24, 56, -24, -24);
        const QRectF target = fittedRect(transformedCanvas(canvas));
        if (!target.contains(point)) {
            return QPointF(-1.0, -1.0);
        }
        const double x = xMin_ + (point.x() - target.left()) / target.width() * (xMax_ - xMin_);
        const double y = yMin_ + (target.bottom() - point.y()) / target.height() * (yMax_ - yMin_);
        return QPointF(x, y);
    }

    void drawGrid(QPainter& painter, const QRectF& available) const {
        if (nx_ <= 0 || ny_ <= 0) {
            return;
        }
        // 网格图像缓存：平移/缩放只重绘，不重建百万级像素。
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
        painter.setRenderHint(QPainter::SmoothPixmapTransform, target.width() < nx_);
        painter.drawImage(target, gridImage_);
        painter.setPen(QColor("#222222"));
        painter.drawRect(target);
    }

    void drawPolygons(QPainter& painter, const QRectF& available) const {
        QColor color = polygonColor_;
        color.setAlphaF(polygonOpacity_);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(color, qMax(1.2, polygonLineWidth_ * 42.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        const QRectF target = fittedRect(available);
        for (const QPolygonF& polygon : polygons_) {
            QPolygonF mapped;
            for (const QPointF& point : polygon) {
                mapped << mapPoint(point, target);
            }
            painter.drawPolygon(mapped);
        }
    }

    QString uiText(const QString& zh) const {
        return fpt_ui::translate(zh, languageCode_);
    }

    double zoom_ = 1.0;
    bool panMode_ = true;
    bool dragging_ = false;
    bool showInputLayer_ = true;
    bool showResultLayer_ = true;
    QPointF panOffset_;
    QPointF lastDragPos_;
    QVector<double> grid_;
    QVector<QPolygonF> polygons_;
    QString gridCachePath_;
    qint64 gridCacheSize_ = -1;
    qint64 gridCacheMtime_ = -1;
    QString polygonCachePath_;
    qint64 polygonCacheSize_ = -1;
    qint64 polygonCacheMtime_ = -1;
    mutable QImage gridImage_;
    mutable bool gridImageDirty_ = true;
    int nx_ = 0;
    int ny_ = 0;
    double valueMin_ = 0.0;
    double valueMax_ = 1.0;
    double xMin_ = 0.0;
    double yMin_ = 0.0;
    double xMax_ = 1.0;
    double yMax_ = 1.0;
    QString database_;
    QString project_;
    QString gridName_;
    QString output_;
    QString preset_ = QString::fromUtf8("防黏连增强");
    QString traceMode_ = "ridge-belt";
    QString thresholdMode_ = "percentile";
    QString activity_ = QString::fromUtf8("等待运行");
    QString dataStatus_;
    QString languageCode_ = "zh-CN";
    double percentile_ = 0.98;
    double threshold_ = 0.95;
    double minArea_ = 120.0;
    QColor polygonColor_ = QColor("#00FFFF");
    double polygonLineWidth_ = 0.03;
    double polygonOpacity_ = 1.0;
    std::function<void(const QPointF&)> coordinateCallback_;
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

        const QRect barRect(width() / 2 - 8, 58, 16, qMax(120, height() - 124));
        painter.setPen(QColor("#555555"));
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

        QLinearGradient gradient(barRect.topLeft(), barRect.bottomLeft());
        gradient.setColorAt(0.00, QColor("#242424"));
        gradient.setColorAt(1.00, QColor("#FFFFFF"));
        painter.fillRect(barRect, gradient);
        painter.setPen(QColor("#333333"));
        painter.drawRect(barRect.adjusted(0, 0, -1, -1));

        painter.setPen(QColor("#333333"));
        painter.drawText(QRect(4, barRect.bottom() + 6, width() - 8, 18), Qt::AlignCenter, "0.0");
    }

private:
    QString languageCode_ = "zh-CN";
};

QLabel* makeFormLabel(const QString& text, QWidget* parent) {
    QLabel* label = new QLabel(text, parent);
    label->setObjectName("formLabel");
    label->setMinimumWidth(88);
    label->setMaximumWidth(126);
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return label;
}

void tuneControl(QWidget* widget) {
    widget->setMinimumHeight(22);
    widget->setMaximumHeight(26);
}

QGridLayout* makeGroupLayout(QGroupBox* box) {
    QGridLayout* layout = new QGridLayout(box);
    layout->setContentsMargins(12, 16, 12, 10);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(7);
    layout->setColumnStretch(0, 0);
    layout->setColumnStretch(1, 1);
    return layout;
}

void addFormRow(QGridLayout* layout, int row, const QString& label, QWidget* editor, QWidget* parent) {
    tuneControl(editor);
    layout->addWidget(makeFormLabel(label, parent), row, 0);
    layout->addWidget(editor, row, 1);
}

} // namespace

FaultTraceDialog::FaultTraceDialog(QWidget* parent) : QDialog(parent), process_(new QProcess(this)) {
    buildUi();
    loadSettings();
    updateStatusContext();

    connect(process_, &QProcess::readyReadStandardOutput, this, &FaultTraceDialog::onReadyRead);
    connect(process_, &QProcess::readyReadStandardError, this, &FaultTraceDialog::onReadyRead);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FaultTraceDialog::onProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, &FaultTraceDialog::onProcessError);

    startupDiscoveryScheduled_ = true;
    QTimer::singleShot(120, this, [this]() {
        startupDiscoveryScheduled_ = false;
        if (!missingGeoEastConfig(false, false).isEmpty()) {
            appendStatus(uiText(QString::fromUtf8("请先完成数据库配置；选择输入网格时可打开配置引导")));
            return;
        }
        refreshOutputsAfterGrids_ = true;
        appendStatus(uiText(QString::fromUtf8("正在自动读取当前项目的数据列表...")));
        refreshGrids();
    });
}

void FaultTraceDialog::buildUi() {
    setWindowTitle(QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪"));
    setWindowFlags((windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint) & ~Qt::WindowContextHelpButtonHint);
    resize(980, 720);

    runnerEdit_ = new QLineEdit(defaultRunnerPath(), this);
    runnerEdit_->setMinimumWidth(0);
    runnerEdit_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    QPushButton* browseRunnerButton = new QPushButton(QString::fromUtf8("浏览"), this);
    connect(browseRunnerButton, &QPushButton::clicked, this, &FaultTraceDialog::browseRunner);

    const QString detectedGeoEastRoot = detectGeoEastRoot();
    geoEastRootEdit_ = new QLineEdit(detectedGeoEastRoot, this);
    geoEastRootEdit_->setMinimumWidth(0);
    geoEastRootEdit_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    geoEastRootEdit_->setPlaceholderText(QString::fromUtf8("自动识别失败时请手动选择 GeoEast 安装根目录"));
    geoEastDataRootEdit_ = new QLineEdit(detectGeoEastDataRoot(detectedGeoEastRoot), this);
    geoEastDataRootEdit_->setMinimumWidth(0);
    geoEastDataRootEdit_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    geoEastDataRootEdit_->setPlaceholderText(QString::fromUtf8("自动识别失败时请手动选择 GeoEastData 根目录"));
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
    projectEdit_ = new QLineEdit(envValue("FPT_GEOEAST_PROJECT"), this);
    projectEdit_->setPlaceholderText(QString::fromUtf8("当前 GeoEast 项目名，未识别时请填写"));
    userEdit_ = new QLineEdit(defaultUserName(), this);
    userEdit_->setPlaceholderText(QString::fromUtf8("GeoEast 登录用户名"));
    passwordEdit_ = new QLineEdit("", this);
    passwordEdit_->setEchoMode(QLineEdit::Password);

    QFormLayout* connectionLayout = new QFormLayout(connectionBox);
    connectionLayout->addRow(QString::fromUtf8("执行程序"), runnerLayout);
    connectionLayout->addRow(QString::fromUtf8("GeoEast 根目录"), geoEastRootLayout);
    connectionLayout->addRow(QString::fromUtf8("GeoEast 数据根目录"), geoEastDataRootLayout);
    connectionLayout->addRow(QString::fromUtf8("PG 主机/端口"), pgLayout);
    connectionLayout->addRow(QString::fromUtf8("数据库"), databaseEdit_);
    connectionLayout->addRow(QString::fromUtf8("项目"), projectEdit_);
    connectionLayout->addRow(QString::fromUtf8("用户"), userEdit_);
    connectionLayout->addRow(QString::fromUtf8("密码"), passwordEdit_);

    QGroupBox* dataBox = new QGroupBox(QString::fromUtf8("输入输出"), this);
    auto* guidedGridCombo = new GuidedCatalogComboBox(this);
    gridCombo_ = guidedGridCombo;
    gridCombo_->setEditable(true);
    gridCombo_->setProperty("fptCatalogReady", false);
    const QString defaultGrid = envValue("FPT_GEOEAST_GRID");
    if (!defaultGrid.isEmpty()) {
        gridCombo_->addItem(defaultGrid);
        gridCombo_->setCurrentText(defaultGrid);
    }
    if (gridCombo_->lineEdit()) {
        gridCombo_->lineEdit()->setPlaceholderText(QString::fromUtf8("请先配置项目并刷新网格"));
    }
    guidedGridCombo->setBeforePopupHandler([this]() {
        if (gridCombo_ && gridCombo_->property("fptCatalogReady").toBool() && gridCombo_->count() > 0) {
            return false;
        }
        QTimer::singleShot(0, this, [this]() {
            if (process_->state() != QProcess::NotRunning) {
                appendStatus(uiText(QString::fromUtf8("正在读取数据库，请稍候")));
                return;
            }
            if (!ensureGeoEastConfigReady(uiText(QString::fromUtf8("选择输入网格")), false, false)) {
                return;
            }
            appendStatus(uiText(QString::fromUtf8("输入网格列表为空，正在自动刷新...")));
            refreshGrids();
        });
        return true;
    });
    outputEdit_ = new QLineEdit(envOrDefault("FPT_GEOEAST_OUTPUT", "FaultPolyTrace_Result"), this);
    polygonColorEdit_ = new QLineEdit(envOrDefault("FPT_POLYGON_COLOR", "#00FFFF"), this);
    polygonColorEdit_->setMinimumWidth(0);
    polygonColorEdit_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    polygonColorEdit_->setReadOnly(true);
    polygonColorButton_ = new QPushButton(QString::fromUtf8("选择颜色"), this);
    polygonLineWidthSpin_ = new QDoubleSpinBox(this);
    polygonLineWidthSpin_->setRange(0.01, 5.0);
    polygonLineWidthSpin_->setDecimals(2);
    polygonLineWidthSpin_->setSingleStep(0.005);
    polygonLineWidthSpin_->setValue(0.025);
    polygonLineWidthSpin_->setToolTip(QString::fromUtf8("设置结果多边形在当前视图中的显示线宽"));
    polygonOpacitySpin_ = new QDoubleSpinBox(this);
    polygonOpacitySpin_->setRange(0.05, 1.0);
    polygonOpacitySpin_->setDecimals(2);
    polygonOpacitySpin_->setSingleStep(0.05);
    polygonOpacitySpin_->setValue(1.0);
    polygonOpacitySpin_->setToolTip(QString::fromUtf8("设置结果多边形在当前视图中的透明度，不修改数据库几何"));
    connect(polygonColorButton_, &QPushButton::clicked, this, &FaultTraceDialog::choosePolygonColor);
    connect(polygonLineWidthSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { updateStatusContext(); });
    connect(polygonOpacitySpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { updateStatusContext(); });
    setPolygonColor(QColor(polygonColorEdit_->text()));
    refreshButton_ = new QPushButton(QString::fromUtf8("刷新网格列表"), this);
    connect(refreshButton_, &QPushButton::clicked, this, &FaultTraceDialog::refreshGrids);

    importDatCheck_ = new QCheckBox(QString::fromUtf8("从 DAT 导入/覆盖输入网格"), this);
    importDatCheck_->setChecked(false);
    datFileEdit_ = new QLineEdit(defaultOfficialDatPath(), this);
    datFileEdit_->setMinimumWidth(0);
    datFileEdit_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    datFileEdit_->setPlaceholderText(QString::fromUtf8("可选：选择需要导入或覆盖到 GeoEast 的 DAT 文件"));
    QPushButton* browseDatButton = new QPushButton(QString::fromUtf8("浏览"), this);
    overwriteGridCheck_ = new QCheckBox(QString::fromUtf8("覆盖同名 GeoEast 网格"), this);
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
    styleLayout->addWidget(new QLabel(QString::fromUtf8("透明度"), this));
    styleLayout->addWidget(polygonOpacitySpin_);
    dataLayout->addRow(QString::fromUtf8("显示样式"), styleLayout);
    dataLayout->addRow(importDatCheck_);
    dataLayout->addRow(QString::fromUtf8("DAT 文件"), datLayout);
    dataLayout->addRow(overwriteGridCheck_);

    QGroupBox* paramsBox = new QGroupBox(QString::fromUtf8("追踪参数"), this);
    presetCombo_ = new QComboBox(this);
    presetCombo_->addItems(fpt_ui::presetKeys());
    qualityModeCombo_ = new QComboBox(this);
    fpt_ui::setComboItems(qualityModeCombo_, {
        QString::fromUtf8("正式处理"),
        QString::fromUtf8("质量诊断"),
        QString::fromUtf8("受控实验")
    }, languageCode());

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
    rootAntiMergeSupportRatioSpin_->setValue(0.25);
    rootAntiMergeMinPartCellsSpin_ = new QSpinBox(this);
    rootAntiMergeMinPartCellsSpin_->setRange(0, 1000000);
    rootAntiMergeMinPartCellsSpin_->setSpecialValueText(QString::fromUtf8("自动"));
    rootAntiMergeMinPartCellsSpin_->setValue(0);
    rootAntiMergeMinPartRatioSpin_ = new QDoubleSpinBox(this);
    rootAntiMergeMinPartRatioSpin_->setRange(0.0, 0.5);
    rootAntiMergeMinPartRatioSpin_->setDecimals(3);
    rootAntiMergeMinPartRatioSpin_->setSingleStep(0.01);
    rootAntiMergeMinPartRatioSpin_->setValue(0.12);
    rootAntiMergeWeakGuardThresholdSpin_ = new QDoubleSpinBox(this);
    rootAntiMergeWeakGuardThresholdSpin_->setRange(0.0, 1.0);
    rootAntiMergeWeakGuardThresholdSpin_->setDecimals(3);
    rootAntiMergeWeakGuardThresholdSpin_->setSingleStep(0.01);
    rootAntiMergeWeakGuardThresholdSpin_->setValue(0.66);
    rootAntiMergeWeakGuardRadiusSpin_ = new QSpinBox(this);
    rootAntiMergeWeakGuardRadiusSpin_->setRange(0, 20);
    rootAntiMergeWeakGuardRadiusSpin_->setValue(0);
    localAntiMergeCheck_ = new QCheckBox(QString::fromUtf8("局部桥防黏连"), this);
    localAntiMergeCheck_->setChecked(false);
    localAntiMergeLowThresholdSpin_ = new QDoubleSpinBox(this);
    localAntiMergeLowThresholdSpin_->setRange(0.0, 1.0);
    localAntiMergeLowThresholdSpin_->setDecimals(4);
    localAntiMergeLowThresholdSpin_->setSingleStep(0.01);
    localAntiMergeLowThresholdSpin_->setValue(0.05);
    localAntiMergeBridgeRadiusSpin_ = new QSpinBox(this);
    localAntiMergeBridgeRadiusSpin_->setRange(0, 20);
    localAntiMergeBridgeRadiusSpin_->setValue(2);
    localAntiMergeMinPartCellsSpin_ = new QSpinBox(this);
    localAntiMergeMinPartCellsSpin_->setRange(1, 1000000);
    localAntiMergeMinPartCellsSpin_->setValue(80);
    localAntiMergeMinPartRatioSpin_ = new QDoubleSpinBox(this);
    localAntiMergeMinPartRatioSpin_->setRange(0.0, 0.5);
    localAntiMergeMinPartRatioSpin_->setDecimals(3);
    localAntiMergeMinPartRatioSpin_->setSingleStep(0.01);
    localAntiMergeMinPartRatioSpin_->setValue(0.08);
    localAntiMergeMinBridgeCellsSpin_ = new QSpinBox(this);
    localAntiMergeMinBridgeCellsSpin_->setRange(1, 1000000);
    localAntiMergeMinBridgeCellsSpin_->setValue(6);
    localAntiMergeMinLowRatioSpin_ = new QDoubleSpinBox(this);
    localAntiMergeMinLowRatioSpin_->setRange(0.0, 1.0);
    localAntiMergeMinLowRatioSpin_->setDecimals(3);
    localAntiMergeMinLowRatioSpin_->setSingleStep(0.01);
    localAntiMergeMinLowRatioSpin_->setValue(0.55);
    simplifyToleranceSpin_ = new QDoubleSpinBox(this);
    simplifyToleranceSpin_->setRange(0.0, 1000.0);
    simplifyToleranceSpin_->setDecimals(2);
    simplifyToleranceSpin_->setSingleStep(1.0);
    simplifyToleranceSpin_->setValue(5.0);
    smoothIterationsSpin_ = new QSpinBox(this);
    smoothIterationsSpin_->setRange(0, 5);
    smoothIterationsSpin_->setValue(1);
    residualTraceCheck_ = new QCheckBox(QString::fromUtf8("残余二次追踪"), this);
    residualTraceCheck_->setChecked(false);
    residualTraceMinLengthSpin_ = new QSpinBox(this);
    residualTraceMinLengthSpin_->setRange(1, 1000);
    residualTraceMinLengthSpin_->setValue(6);
    residualTraceMaxCellsSpin_ = new QSpinBox(this);
    residualTraceMaxCellsSpin_->setRange(0, 1000000);
    residualTraceMaxCellsSpin_->setValue(80);
    connect(presetCombo_, &QComboBox::currentTextChanged, this, &FaultTraceDialog::applyParameterPreset);
    // V1.1：任何参数手动改动后，模板下拉自动显示“自定义”状态。
    {
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
        const QList<QDoubleSpinBox*> doubleSpins = findChildren<QDoubleSpinBox*>();
        for (QDoubleSpinBox* spin : doubleSpins) {
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, markCustomized);
        }
        const QList<QSpinBox*> intSpins = findChildren<QSpinBox*>();
        for (QSpinBox* spin : intSpins) {
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, markCustomized);
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
    }

    {
        setWindowTitle(QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪"));
        setMinimumSize(1120, 700);
        resize(1320, 780);

        connectionBox->hide();
        dataBox->hide();
        paramsBox->hide();

        polygonColorButton_->setText(QString());
        polygonColorButton_->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
        polygonColorButton_->setIconSize(QSize(12, 12));
        polygonColorButton_->setToolTip(QString::fromUtf8("选择结果多边形颜色"));
        polygonColorButton_->setFixedWidth(24);
        polygonLineWidthSpin_->setFixedWidth(64);
        polygonOpacitySpin_->setFixedWidth(64);
        browseDatButton->setText(QString::fromUtf8("浏览"));
        browseDatButton->setMinimumWidth(70);
        browseDatButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        refreshButton_->setText(QString::fromUtf8("刷新网格列表"));
        refreshButton_->setToolTip(QString::fromUtf8("从 GeoEast 数据库读取可用输入网格列表"));
        refreshButton_->setIcon(fpt_ui::toolIcon(fpt_ui::IconKind::Reset));
        refreshButton_->setIconSize(QSize(18, 18));

        setStyleSheet(fpt_ui::appStyle());

        auto openLocalFile = [this](const QString& relativePath) {
            const QString path = locateBundledPath(relativePath);
            if (path.isEmpty()) {
                appendLog(QString::fromUtf8("文件不存在：%1。请确认 docs 目录已随发布包或 GeoEast 便携启动器部署。").arg(relativePath));
                return;
            }
            if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
                appendLog(QString::fromUtf8("无法打开文件：%1").arg(QDir::toNativeSeparators(path)));
            }
        };
        auto openLocalDir = [this](const QString& relativePath) {
            const QString path = QDir(repoRootFromAppDir()).absoluteFilePath(relativePath);
            if (!QDir(path).exists()) {
                QDir().mkpath(path);
            }
            if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
                appendLog(QString::fromUtf8("无法打开目录：%1").arg(QDir::toNativeSeparators(path)));
            }
        };
        auto renderView = [this]() -> RenderViewWidget* {
            return static_cast<RenderViewWidget*>(renderViewWidget_);
        };

        QMenuBar* menuBar = new QMenuBar(this);
        menuBar->setNativeMenuBar(false);
        QMenu* geoEastMenu = menuBar->addMenu(QString::fromUtf8("GeoEast(G)"));
        QAction* configAction = geoEastMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Database), QString::fromUtf8("数据库配置"));
        configAction->setToolTip(QString::fromUtf8("配置 GeoEast 根目录、数据根目录、数据库、项目、用户和执行程序"));
        connect(configAction, &QAction::triggered, this, &FaultTraceDialog::showDatabaseConfigDialog);
        refreshGridAction_ = geoEastMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Refresh), QString::fromUtf8("刷新网格"));
        refreshGridAction_->setToolTip(QString::fromUtf8("刷新 GeoEast 当前项目下的输入网格列表"));
        connect(refreshGridAction_, &QAction::triggered, this, &FaultTraceDialog::refreshGrids);
        deleteOutputAction_ = geoEastMenu->addAction(QString::fromUtf8("删除当前结果"));
        connect(deleteOutputAction_, &QAction::triggered, this, &FaultTraceDialog::deleteOutput);
        geoEastMenu->addSeparator();
        openBasemapAction_ = geoEastMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Basemap), QString::fromUtf8("打开工区底图"));
        connect(openBasemapAction_, &QAction::triggered, this, &FaultTraceDialog::openGeoEastBasemap);
        openMappingAction_ = geoEastMenu->addAction(QString::fromUtf8("打开平面成图"));
        connect(openMappingAction_, &QAction::triggered, this, &FaultTraceDialog::openGeoEastMapping);
        geoEastMenu->addSeparator();
        QAction* exitAction = geoEastMenu->addAction(QString::fromUtf8("关闭启动器"));
        connect(exitAction, &QAction::triggered, this, &FaultTraceDialog::close);

        QMenu* fileMenu = menuBar->addMenu(QString::fromUtf8("文件(F)"));
        QAction* openAction = fileMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Open), QString::fromUtf8("打开 DAT 文件..."));
        openAction->setText(QString::fromUtf8("打开DAT"));
        openAction->setToolTip(QString::fromUtf8("打开或选择 DAT 文件"));
        openAction->setShortcut(QKeySequence::Open);
        connect(openAction, &QAction::triggered, this, &FaultTraceDialog::browseDatFile);
        QAction* saveAction = fileMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Save), QString::fromUtf8("保存当前配置"));
        saveAction->setText(QString::fromUtf8("保存界面设置"));
        saveAction->setToolTip(QString::fromUtf8("保存当前界面配置和参数"));
        saveAction->setShortcut(QKeySequence("Ctrl+Alt+S"));
        connect(saveAction, &QAction::triggered, this, &FaultTraceDialog::saveCurrentPreset);
        QAction* outputDirAction = fileMenu->addAction(QString::fromUtf8("打开输出目录"));
        connect(outputDirAction, &QAction::triggered, this, []() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(userExportDirectory()));
        });
        fileMenu->addSeparator();
        fileMenu->addAction(exitAction);

        QMenu* editMenu = menuBar->addMenu(QString::fromUtf8("编辑(E)"));
        QAction* copyLogAction = editMenu->addAction(QString::fromUtf8("复制日志"));
        connect(copyLogAction, &QAction::triggered, this, [this]() {
            QString logText = logEdit_ ? logEdit_->toPlainText() : QString();
            if (!pendingDebugLines_.isEmpty()) {
                logText += QString::fromUtf8("\n---- 详细日志（界面已折叠）----\n") +
                           pendingDebugLines_.join('\n');
            }
            QApplication::clipboard()->setText(logText);
            appendLog(QString::fromUtf8("日志已复制到剪贴板。"));
        });
        QAction* clearLogAction = editMenu->addAction(QString::fromUtf8("清空日志"));
        connect(clearLogAction, &QAction::triggered, this, [this]() {
            if (logEdit_) {
                logEdit_->clear();
            }
            appendStatus(QString::fromUtf8("日志已清空"));
            pendingDebugLines_.clear();
        });
        QAction* saveLogAction = editMenu->addAction(QString::fromUtf8("保存运行日志..."));
        saveLogAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
        connect(saveLogAction, &QAction::triggered, this, &FaultTraceDialog::saveRunLog);
        editMenu->addSeparator();
        QAction* copySummaryAction = editMenu->addAction(QString::fromUtf8("复制当前结果摘要"));
        connect(copySummaryAction, &QAction::triggered, this, &FaultTraceDialog::copyResultSummary);

        QMenu* viewMenu = menuBar->addMenu(QString::fromUtf8("视图(V)"));
        QAction* panAction = viewMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Pan), QString::fromUtf8("平移模式"));
        panAction->setText(QString::fromUtf8("平移"));
        panAction->setCheckable(true);
        panAction->setChecked(true);
        connect(panAction, &QAction::toggled, this, [this, renderView](bool checked) {
            if (renderView()) {
                renderView()->setPanMode(checked);
            }
            appendStatus(checked ? QString::fromUtf8("视图区已切换为平移模式") : QString::fromUtf8("视图区已切换为选择模式"));
        });
        QAction* zoomInAction = viewMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::ZoomIn), QString::fromUtf8("放大"));
        connect(zoomInAction, &QAction::triggered, this, [this, renderView]() {
            if (renderView()) {
                renderView()->zoomIn();
            }
            appendStatus(QString::fromUtf8("视图区已放大"));
        });
        QAction* zoomOutAction = viewMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::ZoomOut), QString::fromUtf8("缩小"));
        connect(zoomOutAction, &QAction::triggered, this, [this, renderView]() {
            if (renderView()) {
                renderView()->zoomOut();
            }
            appendStatus(QString::fromUtf8("视图区已缩小"));
        });
        QAction* resetViewAction = viewMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Reset), QString::fromUtf8("重置视图"));
        resetViewAction->setText(QString::fromUtf8("重置"));
        resetViewAction->setShortcut(QKeySequence(Qt::Key_Home));
        connect(resetViewAction, &QAction::triggered, this, [this, renderView, panAction]() {
            if (renderView()) {
                renderView()->resetView();
            }
            panAction->setChecked(true);
            appendStatus(QString::fromUtf8("视图区已重置"));
        });
        viewMenu->addSeparator();
        showInputLayerAction_ = viewMenu->addAction(QString::fromUtf8("显示原始网格层"));
        showInputLayerAction_->setCheckable(true);
        showInputLayerAction_->setChecked(false);
        connect(showInputLayerAction_, &QAction::toggled, this, [this](bool checked) {
            if (showInputLayerCheck_ && showInputLayerCheck_->isChecked() != checked) {
                showInputLayerCheck_->setChecked(checked);
                return;
            }
            updateLayerVisibility();
        });
        showResultLayerAction_ = viewMenu->addAction(QString::fromUtf8("显示结果多边形层"));
        showResultLayerAction_->setCheckable(true);
        showResultLayerAction_->setChecked(false);
        connect(showResultLayerAction_, &QAction::toggled, this, [this](bool checked) {
            if (showResultLayerCheck_ && showResultLayerCheck_->isChecked() != checked) {
                showResultLayerCheck_->setChecked(checked);
                return;
            }
            updateLayerVisibility();
        });
        viewMenu->addSeparator();
        loadResultViewAction_ = viewMenu->addAction(QString::fromUtf8("重新载入结果视图"));
        connect(loadResultViewAction_, &QAction::triggered, this, &FaultTraceDialog::loadResultView);
        exportViewAction_ = viewMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Image), QString::fromUtf8("导出当前视图 PNG"));
        exportViewAction_->setShortcut(QKeySequence("Ctrl+Shift+P"));
        connect(exportViewAction_, &QAction::triggered, this, &FaultTraceDialog::exportResultViewPng);

        QMenu* faultMenu = menuBar->addMenu(QString::fromUtf8("断层(L)"));
        runTraceAction_ = faultMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Run), QString::fromUtf8("开始追踪"));
        runTraceAction_->setShortcut(QKeySequence(Qt::Key_F5));
        connect(runTraceAction_, &QAction::triggered, this, &FaultTraceDialog::runTrace);
        stopTraceAction_ = faultMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Stop), QString::fromUtf8("停止追踪"));
        stopTraceAction_->setEnabled(false);
        stopTraceAction_->setShortcut(QKeySequence("Shift+F5"));
        connect(stopTraceAction_, &QAction::triggered, this, &FaultTraceDialog::stopProcess);
        faultMenu->addSeparator();
        QAction* colorAction = faultMenu->addAction(QString::fromUtf8("选择结果颜色..."));
        connect(colorAction, &QAction::triggered, this, &FaultTraceDialog::choosePolygonColor);

        QMenu* horizonMenu = menuBar->addMenu(QString::fromUtf8("层位(H)"));
        QAction* horizonInfoAction = horizonMenu->addAction(QString::fromUtf8("打开叠合查看指南"));
        connect(horizonInfoAction, &QAction::triggered, this, [this, openLocalFile]() {
            appendLog(QString::fromUtf8("本工具直接处理平面断层属性网格，不修改层位解释对象；正在打开 GeoEast 叠合查看指南。"));
            openLocalFile("docs/GeoEast使用指南.md");
        });

        QMenu* toolMenu = menuBar->addMenu(QString::fromUtf8("工具(T)"));
        QAction* savePresetAction = toolMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Preset), QString::fromUtf8("保存参数模板..."));
        savePresetAction->setShortcut(QKeySequence::Save);
        connect(savePresetAction, &QAction::triggered, this, &FaultTraceDialog::saveParameterTemplate);
        QAction* loadPresetAction = toolMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Open), QString::fromUtf8("打开参数模板..."));
        loadPresetAction->setShortcut(QKeySequence("Ctrl+Shift+O"));
        connect(loadPresetAction, &QAction::triggered, this, &FaultTraceDialog::loadParameterTemplate);
        restoreDefaultsAction_ = toolMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Restore), QString::fromUtf8("恢复默认配置"));
        restoreDefaultsAction_->setText(QString::fromUtf8("恢复默认"));
        restoreDefaultsAction_->setToolTip(QString::fromUtf8("恢复推荐算法参数、显示样式和默认输出名，不清空数据库连接信息"));
        connect(restoreDefaultsAction_, &QAction::triggered, this, &FaultTraceDialog::restoreDefaults);
        QAction* userManualToolAction = toolMenu->addAction(QString::fromUtf8("打开用户操作手册"));
        connect(userManualToolAction, &QAction::triggered, this, [openLocalFile]() { openLocalFile("docs/用户操作手册.md"); });
        QAction* packageGuideAction = toolMenu->addAction(QString::fromUtf8("打开异机运行说明"));
        connect(packageGuideAction, &QAction::triggered, this, [openLocalFile]() { openLocalFile("docs/评审交付与异机运行说明.md"); });

        QMenu* languageMenu = menuBar->addMenu(uiText("语言(L)"));
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

        QMenu* helpMenu = menuBar->addMenu(QString::fromUtf8("帮助(H)"));
        QAction* helpAction = helpMenu->addAction(fpt_ui::toolIcon(fpt_ui::IconKind::Help), QString::fromUtf8("帮助提示"));
        connect(helpAction, &QAction::triggered, this, &FaultTraceDialog::showHelpHint);
        QAction* manualAction = helpMenu->addAction(QString::fromUtf8("用户操作手册"));
        connect(manualAction, &QAction::triggered, this, [openLocalFile]() { openLocalFile("docs/用户操作手册.md"); });
        QAction* projectStructureAction = helpMenu->addAction(QString::fromUtf8("项目目录结构"));
        connect(projectStructureAction, &QAction::triggered, this, [openLocalFile]() { openLocalFile("docs/项目目录结构与文件说明.md"); });
        QAction* aboutAction = helpMenu->addAction(QString::fromUtf8("关于 FaultPolyTrace"));
        connect(aboutAction, &QAction::triggered, this, [this]() {
            QMessageBox::information(this, uiText(QString::fromUtf8("关于 FaultPolyTrace")),
                                     uiText(QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪\n\n"
                                                       "定位：平面断层属性到闭合断层多边形的自动追踪工具。\n"
                                                       "入口：可独立运行，也可通过 GeoEast 菜单调用执行程序。")));
        });

        // 工具栏使用简短文案，菜单中仍保留完整名称。
        const auto setBarText = [](QAction* action, const QString& shortText, const QString& englishText) {
            if (action) {
                action->setIconText(shortText);
                action->setProperty("fptIconTextZh", shortText);
                action->setProperty("fptIconTextEn", englishText);
            }
        };
        setBarText(openAction, QString::fromUtf8("打开"), "Open");
        setBarText(saveAction, QString::fromUtf8("保存设置"), "Save");
        setBarText(panAction, QString::fromUtf8("平移"), "Pan");
        setBarText(zoomInAction, QString::fromUtf8("放大"), "Zoom In");
        setBarText(zoomOutAction, QString::fromUtf8("缩小"), "Zoom Out");
        setBarText(resetViewAction, QString::fromUtf8("重置"), "Reset");
        setBarText(runTraceAction_, QString::fromUtf8("开始追踪"), "Trace");
        setBarText(stopTraceAction_, QString::fromUtf8("停止"), "Stop");
        setBarText(configAction, QString::fromUtf8("数据库"), "Database");
        setBarText(refreshGridAction_, QString::fromUtf8("刷新网格"), "Grids");
        setBarText(restoreDefaultsAction_, QString::fromUtf8("恢复默认"), "Defaults");
        setBarText(exportViewAction_, QString::fromUtf8("导出PNG"), "PNG");
        setBarText(openBasemapAction_, QString::fromUtf8("工区底图"), "Basemap");
        setBarText(helpAction, QString::fromUtf8("帮助"), "Help");

        QToolBar* toolbar = new QToolBar(this);
        toolbar->setMovable(false);
        toolbar->setFloatable(false);
        toolbar->setIconSize(QSize(22, 22));
        toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->addAction(openAction);
        toolbar->addAction(saveAction);
        toolbar->addSeparator();
        toolbar->addAction(panAction);
        toolbar->addAction(zoomInAction);
        toolbar->addAction(zoomOutAction);
        toolbar->addAction(resetViewAction);
        toolbar->addSeparator();
        toolbar->addAction(runTraceAction_);
        toolbar->addAction(stopTraceAction_);
        toolbar->addSeparator();
        toolbar->addAction(configAction);
        toolbar->addAction(refreshGridAction_);
        toolbar->addAction(restoreDefaultsAction_);
        toolbar->addSeparator();
        toolbar->addAction(exportViewAction_);
        toolbar->addAction(openBasemapAction_);
        toolbar->addSeparator();
        toolbar->addAction(helpAction);
        QWidget* toolbarSpacer = new QWidget(this);
        toolbarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        toolbar->addWidget(toolbarSpacer);

        QFrame* sidebar = new QFrame(this);
        sidebar->setObjectName("sidebar");
        sidebar->setMinimumWidth(400);
        sidebar->setMaximumWidth(600);
        sidebar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        QVBoxLayout* sidebarRoot = new QVBoxLayout(sidebar);
        sidebarRoot->setContentsMargins(0, 0, 0, 0);
        sidebarRoot->setSpacing(0);

        QFrame* sidebarHeader = new QFrame(sidebar);
        sidebarHeader->setObjectName("sidebarHeader");
        sidebarHeader->setFixedHeight(34);
        QHBoxLayout* sidebarHeaderLayout = new QHBoxLayout(sidebarHeader);
        sidebarHeaderLayout->setContentsMargins(12, 0, 8, 0);
        QLabel* sidebarTitle = new QLabel(QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪"), sidebarHeader);
        sidebarTitle->setObjectName("sidebarTitle");
        sidebarHeaderLayout->addWidget(sidebarTitle, 1);
        sidebarRoot->addWidget(sidebarHeader);

        QScrollArea* sidebarScroll = new QScrollArea(sidebar);
        sidebarScroll->setObjectName("sidebarScroll");
        sidebarScroll->setWidgetResizable(true);
        sidebarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        QWidget* sidebarContent = new QWidget(sidebarScroll);
        sidebarContent->setObjectName("sidebarContent");
        QVBoxLayout* sidebarContentLayout = new QVBoxLayout(sidebarContent);
        sidebarContentLayout->setContentsMargins(10, 12, 10, 10);
        sidebarContentLayout->setSpacing(12);

        QGroupBox* dataGroup = new QGroupBox(QString::fromUtf8("数据管理"), sidebarContent);
        QGridLayout* dataLayoutNew = makeGroupLayout(dataGroup);
        addFormRow(dataLayoutNew, 0, QString::fromUtf8("输入网格"), gridCombo_, dataGroup);
        addFormRow(dataLayoutNew, 1, QString::fromUtf8("输出多边形"), outputEdit_, dataGroup);
        QWidget* colorWidget = new QWidget(dataGroup);
        QHBoxLayout* colorLayout = new QHBoxLayout(colorWidget);
        colorLayout->setContentsMargins(0, 0, 0, 0);
        colorLayout->setSpacing(5);
        colorLayout->addWidget(polygonColorButton_);
        colorLayout->addWidget(polygonColorEdit_, 1);
        addFormRow(dataLayoutNew, 2, QString::fromUtf8("颜色"), colorWidget, dataGroup);
        QWidget* lineStyleWidget = new QWidget(dataGroup);
        QHBoxLayout* lineStyleLayout = new QHBoxLayout(lineStyleWidget);
        lineStyleLayout->setContentsMargins(0, 0, 0, 0);
        lineStyleLayout->setSpacing(5);
        lineStyleLayout->addWidget(new QLabel(QString::fromUtf8("线宽"), lineStyleWidget));
        lineStyleLayout->addWidget(polygonLineWidthSpin_);
        lineStyleLayout->addSpacing(4);
        lineStyleLayout->addWidget(new QLabel(QString::fromUtf8("透明度"), lineStyleWidget));
        lineStyleLayout->addWidget(polygonOpacitySpin_);
        lineStyleLayout->addStretch(1);
        addFormRow(dataLayoutNew, 3, QString::fromUtf8("线条"), lineStyleWidget, dataGroup);
        QWidget* datLineWidget = new QWidget(dataGroup);
        QHBoxLayout* datLineLayout = new QHBoxLayout(datLineWidget);
        datLineLayout->setContentsMargins(0, 0, 0, 0);
        datLineLayout->setSpacing(5);
        datLineLayout->addWidget(browseDatButton);
        datLineLayout->addWidget(datFileEdit_, 1);
        addFormRow(dataLayoutNew, 4, QString::fromUtf8("DAT 文件"), datLineWidget, dataGroup);
        dataLayoutNew->addWidget(overwriteGridCheck_, 5, 0, 1, 2);
        dataLayoutNew->addWidget(importDatCheck_, 6, 0, 1, 2);
        dataLayoutNew->addWidget(refreshButton_, 7, 0, 1, 2);

        QGroupBox* resultGroup = new QGroupBox(QString::fromUtf8("结果查看"), sidebarContent);
        QGridLayout* resultLayout = makeGroupLayout(resultGroup);
        QWidget* layerPanel = new QWidget(resultGroup);
        layerPanel->setObjectName("layerPanel");
        QVBoxLayout* layerPanelLayout = new QVBoxLayout(layerPanel);
        layerPanelLayout->setContentsMargins(0, 0, 0, 4);
        layerPanelLayout->setSpacing(4);
        layerTitleLabel_ = new QLabel(QString::fromUtf8("显示图层"), layerPanel);
        layerTitleLabel_->setObjectName("layerPanelTitle");
        showInputLayerCheck_ = new fpt_ui::LayerToggle(layerPanel);
        showInputLayerCheck_->setObjectName("layerCheck");
        showInputLayerCheck_->setText(QString::fromUtf8("原始网格"));
        showInputLayerCheck_->setToolTip(QString::fromUtf8("显示或隐藏输入断层属性网格。GeoEast 模式会按需导出当前 BOMapGrid 视图。"));
        showInputLayerCheck_->setChecked(false);
        showInputLayerCheck_->setAccentColor(QColor(QStringLiteral("#527A9F")));
        showResultLayerCheck_ = new fpt_ui::LayerToggle(layerPanel);
        showResultLayerCheck_->setObjectName("layerCheck");
        showResultLayerCheck_->setText(QString::fromUtf8("结果多边形"));
        showResultLayerCheck_->setToolTip(QString::fromUtf8("显示或隐藏追踪结果多边形。没有结果时请先追踪或选择已有结果。"));
        showResultLayerCheck_->setChecked(false);
        showResultLayerCheck_->setAccentColor(QColor(polygonColorEdit_->text()));
        layerHintLabel_ = new QLabel(QString::fromUtf8("同时开启两层即可叠加查看。"), layerPanel);
        layerHintLabel_->setObjectName("layerHint");
        layerHintLabel_->setWordWrap(true);
        layerPanelLayout->addWidget(layerTitleLabel_);
        layerPanelLayout->addWidget(showInputLayerCheck_);
        layerPanelLayout->addWidget(showResultLayerCheck_);
        layerPanelLayout->addWidget(layerHintLabel_);
        resultLayout->addWidget(layerPanel, 0, 0, 1, 2);
        auto* guidedOutputCombo = new GuidedCatalogComboBox(resultGroup);
        outputCatalogCombo_ = guidedOutputCombo;
        outputCatalogCombo_->setProperty("fptCatalogReady", false);
        outputCatalogCombo_->addItem(QString::fromUtf8("请先配置项目并刷新结果"), QString());
        outputCatalogCombo_->setToolTip(QString::fromUtf8("当前 GeoEast 项目中已有的结果多边形对象。选择后可同步为当前输出名称，便于查看、覆盖或删除。"));
        guidedOutputCombo->setBeforePopupHandler([this]() {
            bool hasAvailableResult = false;
            if (outputCatalogCombo_) {
                for (int i = 0; i < outputCatalogCombo_->count(); ++i) {
                    if (!outputCatalogCombo_->itemData(i).toString().trimmed().isEmpty()) {
                        hasAvailableResult = true;
                        break;
                    }
                }
            }
            if (hasAvailableResult && outputCatalogCombo_->property("fptCatalogReady").toBool()) {
                return false;
            }
            QTimer::singleShot(0, this, [this]() {
                if (process_->state() != QProcess::NotRunning) {
                    appendStatus(uiText(QString::fromUtf8("正在读取数据库，请稍候")));
                    return;
                }
                if (!ensureGeoEastConfigReady(uiText(QString::fromUtf8("选择已有结果")), false, false)) {
                    return;
                }
                appendStatus(uiText(QString::fromUtf8("已有结果列表为空，正在自动刷新...")));
                refreshOutputs();
            });
            return true;
        });
        refreshOutputsButton_ = new QPushButton(QString::fromUtf8("刷新结果列表"), resultGroup);
        refreshOutputsButton_->setToolTip(QString::fromUtf8("刷新当前项目中的 BOFaultPolygon 结果列表。"));
        loadSelectedOutputButton_ = new QPushButton(QString::fromUtf8("显示所选结果"), resultGroup);
        loadSelectedOutputButton_->setObjectName("emphasisButton");
        loadSelectedOutputButton_->setToolTip(QString::fromUtf8("导出并显示当前选择的结果多边形，同时叠加当前输入网格。"));
        resultLayout->addWidget(outputCatalogCombo_, 1, 0, 1, 2);
        resultLayout->addWidget(refreshOutputsButton_, 2, 0);
        resultLayout->addWidget(loadSelectedOutputButton_, 2, 1);
        loadResultViewButton_ = new QPushButton(QString::fromUtf8("重新载入结果"), resultGroup);
        loadResultViewButton_->setToolTip(QString::fromUtf8("重新读取本次计算生成的原始网格和结果多边形文件。"));
        exportViewButton_ = new QPushButton(QString::fromUtf8("导出视图 PNG"), resultGroup);
        exportViewButton_->setToolTip(QString::fromUtf8("导出当前结果视图 PNG 图片。"));
        resultLayout->addWidget(loadResultViewButton_, 3, 0);
        resultLayout->addWidget(exportViewButton_, 3, 1);
        openBasemapButton_ = new QPushButton(QString::fromUtf8("打开工区底图"), resultGroup);
        openBasemapButton_->setToolTip(QString::fromUtf8("启动 GeoEast 自带 GeoBasemap，在官方图层树中查看原始网格和结果多边形。"));
        openMappingButton_ = new QPushButton(QString::fromUtf8("打开平面成图"), resultGroup);
        openMappingButton_->setToolTip(QString::fromUtf8("启动 GeoEast 自带 GeoMapping，在官方图层树中查看原始网格和结果多边形。"));
        resultLayout->addWidget(openBasemapButton_, 4, 0);
        resultLayout->addWidget(openMappingButton_, 4, 1);
        QPushButton* mappingGuideButton = new QPushButton(QString::fromUtf8("查看步骤"), resultGroup);
        mappingGuideButton->setToolTip(QString::fromUtf8("打开 GeoEast 平面成图叠加查看操作指南"));
        deleteOutputButton_ = new QPushButton(QString::fromUtf8("删除当前结果"), resultGroup);
        deleteOutputButton_->setObjectName("dangerButton");
        deleteOutputButton_->setToolTip(QString::fromUtf8("删除当前输出名称对应的结果多边形对象"));
        resultLayout->addWidget(mappingGuideButton, 5, 0);
        resultLayout->addWidget(deleteOutputButton_, 5, 1);
        QLabel* displayHint = new QLabel(QString::fromUtf8("平台复核：在工区底图或平面成图中勾选网格与结果图层。"), resultGroup);
        displayHint->setWordWrap(true);
        displayHint->setObjectName("displayHint");
        resultLayout->addWidget(displayHint, 6, 0, 1, 2);
        connect(showInputLayerCheck_, &QAbstractButton::toggled, this, [this](bool checked) {
            if (showInputLayerAction_ && showInputLayerAction_->isChecked() != checked) {
                showInputLayerAction_->setChecked(checked);
                return;
            }
            updateLayerVisibility();
        });
        connect(showResultLayerCheck_, &QAbstractButton::toggled, this, [this](bool checked) {
            if (showResultLayerAction_ && showResultLayerAction_->isChecked() != checked) {
                showResultLayerAction_->setChecked(checked);
                return;
            }
            updateLayerVisibility();
        });
        connect(mappingGuideButton, &QPushButton::clicked, this, [this, openLocalFile]() {
            appendLog(QString::fromUtf8("正在打开 GeoEast 叠加查看步骤。官方图层显示和关闭请在 GeoEast 图层树中操作。"));
            openLocalFile("docs/GeoEast使用指南.md");
        });
        connect(refreshOutputsButton_, &QPushButton::clicked, this, &FaultTraceDialog::refreshOutputs);
        connect(loadSelectedOutputButton_, &QPushButton::clicked, this, [this]() {
            loadSelectedOutputInternal(true);
        });
        connect(loadResultViewButton_, &QPushButton::clicked, this, &FaultTraceDialog::loadResultView);
        connect(exportViewButton_, &QPushButton::clicked, this, &FaultTraceDialog::exportResultViewPng);
        connect(openBasemapButton_, &QPushButton::clicked, this, &FaultTraceDialog::openGeoEastBasemap);
        connect(openMappingButton_, &QPushButton::clicked, this, &FaultTraceDialog::openGeoEastMapping);
        connect(deleteOutputButton_, &QPushButton::clicked, this, &FaultTraceDialog::deleteOutput);
        connect(outputCatalogCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
            if (!outputCatalogCombo_ || outputCatalogCombo_->currentData().toString().trimmed().isEmpty()) {
                return;
            }
            if (!showResultLayerCheck_ || showResultLayerCheck_->isChecked()) {
                QTimer::singleShot(0, this, [this]() {
                    loadSelectedOutputInternal(false);
                });
            } else {
                outputEdit_->setText(outputCatalogCombo_->currentData().toString().trimmed());
                updateStatusContext();
            }
        });

        QGroupBox* traceGroup = new QGroupBox(QString::fromUtf8("追踪参数"), sidebarContent);
        QGridLayout* traceLayoutNew = makeGroupLayout(traceGroup);
        addFormRow(traceLayoutNew, 0, QString::fromUtf8("预设模板"), presetCombo_, traceGroup);
        addFormRow(traceLayoutNew, 1, QString::fromUtf8("质量模式"), qualityModeCombo_, traceGroup);
        addFormRow(traceLayoutNew, 2, QString::fromUtf8("追踪模式"), traceModeCombo_, traceGroup);
        addFormRow(traceLayoutNew, 3, QString::fromUtf8("阈值模式"), thresholdModeCombo_, traceGroup);
        addFormRow(traceLayoutNew, 4, QString::fromUtf8("分位数"), percentileSpin_, traceGroup);
        addFormRow(traceLayoutNew, 5, QString::fromUtf8("固定阈值"), thresholdSpin_, traceGroup);
        addFormRow(traceLayoutNew, 6, QString::fromUtf8("最小面积"), minAreaSpin_, traceGroup);

        QGroupBox* morphologyGroup = new QGroupBox(QString::fromUtf8("形态与拓扑约束"), sidebarContent);
        QGridLayout* morphologyLayoutNew = makeGroupLayout(morphologyGroup);
        addFormRow(morphologyLayoutNew, 0, QString::fromUtf8("闭运算半径"), closeRadiusSpin_, morphologyGroup);
        addFormRow(morphologyLayoutNew, 1, QString::fromUtf8("最大断距"), maxGapDistanceSpin_, morphologyGroup);
        addFormRow(morphologyLayoutNew, 2, QString::fromUtf8("平滑次数"), smoothIterationsSpin_, morphologyGroup);
        addFormRow(morphologyLayoutNew, 3, QString::fromUtf8("最大夹角"), maxGapAngleSpin_, morphologyGroup);
        addFormRow(morphologyLayoutNew, 4, QString::fromUtf8("防黏连支撑"), rootAntiMergeSupportRatioSpin_, morphologyGroup);
        addFormRow(morphologyLayoutNew, 5, QString::fromUtf8("简化容差"), simplifyToleranceSpin_, morphologyGroup);
        morphologyLayoutNew->addWidget(attributeConstrainedBeltCheck_, 6, 0);
        morphologyLayoutNew->addWidget(attributeEnvelopeRadiusSpin_, 6, 1);
        morphologyLayoutNew->addWidget(rootAntiMergeCheck_, 7, 0);
        morphologyLayoutNew->addWidget(rootAntiMergeRadiusSpin_, 7, 1);

        QGroupBox* detailsGroup = new QGroupBox(QString::fromUtf8("枝干细节与修补"), sidebarContent);
        QGridLayout* detailsLayoutNew = makeGroupLayout(detailsGroup);
        addFormRow(detailsLayoutNew, 0, QString::fromUtf8("弱拓扑分位"), weakPercentileSpin_, detailsGroup);
        addFormRow(detailsLayoutNew, 1, QString::fromUtf8("弱延伸半径"), weakConnectRadiusSpin_, detailsGroup);
        addFormRow(detailsLayoutNew, 2, QString::fromUtf8("带宽半径"), beltRadiusSpin_, detailsGroup);
        addFormRow(detailsLayoutNew, 3, QString::fromUtf8("带后闭合"), postBeltCloseRadiusSpin_, detailsGroup);
        addFormRow(detailsLayoutNew, 4, QString::fromUtf8("最短分支"), minBranchLengthSpin_, detailsGroup);
        addFormRow(detailsLayoutNew, 5, QString::fromUtf8("交切短枝"), minJunctionBranchLengthSpin_, detailsGroup);
        addFormRow(detailsLayoutNew, 6, QString::fromUtf8("短枝均值"), minJunctionBranchMeanSpin_, detailsGroup);
        addFormRow(detailsLayoutNew, 7, QString::fromUtf8("补追最短"), residualTraceMinLengthSpin_, detailsGroup);
        addFormRow(detailsLayoutNew, 8, QString::fromUtf8("补追上限"), residualTraceMaxCellsSpin_, detailsGroup);
        detailsLayoutNew->addWidget(residualTraceCheck_, 9, 0, 1, 2);

        QGroupBox* expertGroup = new QGroupBox(QString::fromUtf8("专家防黏连参数"), sidebarContent);
        QGridLayout* expertLayoutNew = makeGroupLayout(expertGroup);
        addFormRow(expertLayoutNew, 0, QString::fromUtf8("根系主体"), rootAntiMergeMinPartCellsSpin_, expertGroup);
        addFormRow(expertLayoutNew, 1, QString::fromUtf8("主体比例"), rootAntiMergeMinPartRatioSpin_, expertGroup);
        addFormRow(expertLayoutNew, 2, QString::fromUtf8("弱保护阈值"), rootAntiMergeWeakGuardThresholdSpin_, expertGroup);
        addFormRow(expertLayoutNew, 3, QString::fromUtf8("弱保护半径"), rootAntiMergeWeakGuardRadiusSpin_, expertGroup);
        expertLayoutNew->addWidget(localAntiMergeCheck_, 4, 0, 1, 2);
        addFormRow(expertLayoutNew, 5, QString::fromUtf8("低值阈值"), localAntiMergeLowThresholdSpin_, expertGroup);
        addFormRow(expertLayoutNew, 6, QString::fromUtf8("桥半径"), localAntiMergeBridgeRadiusSpin_, expertGroup);
        addFormRow(expertLayoutNew, 7, QString::fromUtf8("局部主体"), localAntiMergeMinPartCellsSpin_, expertGroup);
        addFormRow(expertLayoutNew, 8, QString::fromUtf8("局部比例"), localAntiMergeMinPartRatioSpin_, expertGroup);
        addFormRow(expertLayoutNew, 9, QString::fromUtf8("桥像元"), localAntiMergeMinBridgeCellsSpin_, expertGroup);
        addFormRow(expertLayoutNew, 10, QString::fromUtf8("低值占比"), localAntiMergeMinLowRatioSpin_, expertGroup);
        // V1.1：按任务流排序——数据 → 参数 → 高级 → 结果查看。
        sidebarContentLayout->addWidget(dataGroup);
        sidebarContentLayout->addWidget(traceGroup);
        sidebarContentLayout->addWidget(morphologyGroup);
        sidebarContentLayout->addWidget(detailsGroup);
        sidebarContentLayout->addWidget(expertGroup);
        sidebarContentLayout->addWidget(resultGroup);
        sidebarContentLayout->addStretch(1);

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
        makeCollapsible(morphologyGroup, false);
        makeCollapsible(detailsGroup, false);
        makeCollapsible(expertGroup, false);

        sidebarScroll->setWidget(sidebarContent);
        sidebarRoot->addWidget(sidebarScroll, 1);

        QFrame* actionDock = new QFrame(sidebar);
        actionDock->setObjectName("actionDock");
        actionDock->setFixedHeight(50);
        QHBoxLayout* actionLayout = new QHBoxLayout(actionDock);
        actionLayout->setContentsMargins(12, 10, 12, 10);
        actionLayout->setSpacing(10);
        runButton_ = new QPushButton(QString::fromUtf8("开始追踪"), actionDock);
        runButton_->setObjectName("runButton");
        stopButton_ = new QPushButton(QString::fromUtf8("停止"), actionDock);
        stopButton_->setObjectName("stopButton");
        stopButton_->setEnabled(false);
        actionLayout->addWidget(runButton_, 2);
        actionLayout->addWidget(stopButton_, 1);
        sidebarRoot->addWidget(actionDock);
        connect(runButton_, &QPushButton::clicked, this, &FaultTraceDialog::runTrace);
        connect(stopButton_, &QPushButton::clicked, this, &FaultTraceDialog::stopProcess);

        QFrame* workspace = new QFrame(this);
        workspace->setObjectName("workspace");
        QVBoxLayout* workspaceLayout = new QVBoxLayout(workspace);
        workspaceLayout->setContentsMargins(0, 0, 0, 0);
        workspaceLayout->setSpacing(0);

        QFrame* canvasRow = new QFrame(workspace);
        canvasRow->setObjectName("canvasRow");
        QHBoxLayout* canvasLayout = new QHBoxLayout(canvasRow);
        canvasLayout->setContentsMargins(0, 0, 0, 0);
        canvasLayout->setSpacing(0);
        RenderViewWidget* renderViewWidget = new RenderViewWidget(canvasRow);
        renderViewWidget_ = renderViewWidget;
        renderViewWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(renderViewWidget, &QWidget::customContextMenuRequested, this,
                [this, renderViewWidget](const QPoint& pos) {
            QMenu menu(this);
            QAction* exportAct = menu.addAction(uiText(QString::fromUtf8("导出当前视图 PNG")));
            connect(exportAct, &QAction::triggered, this, &FaultTraceDialog::exportResultViewPng);
            QAction* resetAct = menu.addAction(uiText(QString::fromUtf8("重置视图")));
            connect(resetAct, &QAction::triggered, this,
                    [renderViewWidget]() { renderViewWidget->resetView(); });
            QAction* basemapAct = menu.addAction(uiText(QString::fromUtf8("打开工区底图")));
            connect(basemapAct, &QAction::triggered, this, &FaultTraceDialog::openGeoEastBasemap);
            menu.exec(renderViewWidget->mapToGlobal(pos));
        });
        renderViewWidget->setCoordinateCallback([this](const QPointF& point) {
            if (!coordinateLabel_) {
                return;
            }
            if (point.x() < 0.0 || point.y() < 0.0) {
                coordinateLabel_->setText(uiText(QString::fromUtf8("视图 X: --   |   Y: --")));
                return;
            }
            coordinateLabel_->setText(
                uiText(QString::fromUtf8("视图 X: %1   |   Y: %2"))
                    .arg(point.x(), 0, 'f', 1)
                    .arg(point.y(), 0, 'f', 1));
        });
        canvasLayout->addWidget(renderViewWidget, 1);
        colorBarWidget_ = new ColorBarWidget(canvasRow);
        canvasLayout->addWidget(colorBarWidget_);

        // V1.1：追踪完成横幅——聚合“下一步”入口。
        doneBanner_ = new QFrame(workspace);
        doneBanner_->setObjectName("doneBanner");
        doneBanner_->hide();
        QHBoxLayout* bannerLayout = new QHBoxLayout(doneBanner_);
        bannerLayout->setContentsMargins(12, 6, 8, 6);
        bannerLayout->setSpacing(8);
        doneBannerLabel_ = new QLabel(doneBanner_);
        doneBannerLabel_->setObjectName("doneBannerText");
        bannerLayout->addWidget(doneBannerLabel_, 1);
        QPushButton* bannerExportButton = new QPushButton(QString::fromUtf8("导出PNG"), doneBanner_);
        bannerExportButton->setObjectName("linkButton");
        connect(bannerExportButton, &QPushButton::clicked, this, &FaultTraceDialog::exportResultViewPng);
        bannerLayout->addWidget(bannerExportButton);
        QPushButton* bannerBasemapButton = new QPushButton(QString::fromUtf8("打开工区底图"), doneBanner_);
        bannerBasemapButton->setObjectName("linkButton");
        connect(bannerBasemapButton, &QPushButton::clicked, this, &FaultTraceDialog::openGeoEastBasemap);
        bannerLayout->addWidget(bannerBasemapButton);
        QPushButton* bannerCloseButton = new QPushButton(QStringLiteral("✕"), doneBanner_);
        bannerCloseButton->setObjectName("linkButton");
        bannerCloseButton->setFixedWidth(24);
        connect(bannerCloseButton, &QPushButton::clicked, doneBanner_, &QWidget::hide);
        bannerLayout->addWidget(bannerCloseButton);
        workspaceLayout->addWidget(doneBanner_);
        workspaceLayout->addWidget(canvasRow, 1);

        QFrame* logDock = new QFrame(workspace);
        logDock->setObjectName("logDock");
        logDock->setFixedHeight(120);
        QVBoxLayout* logLayout = new QVBoxLayout(logDock);
        logLayout->setContentsMargins(0, 0, 0, 0);
        logLayout->setSpacing(0);
        QWidget* logHeaderRow = new QWidget(logDock);
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
                    html += QString("<span style=\"color:#9CA3AF;\">%1</span><br>")
                                .arg(line.toHtmlEscaped());
                }
                pendingDebugLines_.clear();
                logEdit_->moveCursor(QTextCursor::End);
                logEdit_->insertHtml(html);
                logEdit_->moveCursor(QTextCursor::End);
            }
        });
        logHeaderLayout->addWidget(logHeader, 1);
        logHeaderLayout->addWidget(verboseLogCheck_);
        logEdit_ = new QTextEdit(logDock);
        logEdit_->setObjectName("logEdit");
        logEdit_->setReadOnly(true);
        logEdit_->setLineWrapMode(QTextEdit::NoWrap);
        logEdit_->document()->setMaximumBlockCount(3000);
        logLayout->addWidget(logHeaderRow);
        logLayout->addWidget(logEdit_, 1);
        workspaceLayout->addWidget(logDock);

        workspace->setMinimumWidth(520);
        mainSplitter_ = new QSplitter(Qt::Horizontal, this);
        mainSplitter_->setObjectName("mainSplitter");
        mainSplitter_->setChildrenCollapsible(false);
        mainSplitter_->setHandleWidth(5);
        mainSplitter_->addWidget(sidebar);
        mainSplitter_->addWidget(workspace);
        mainSplitter_->setStretchFactor(0, 0);
        mainSplitter_->setStretchFactor(1, 1);
        mainSplitter_->setSizes({420, 860});

        QFrame* statusBar = new QFrame(this);
        statusBar->setObjectName("statusBar");
        statusBar->setFixedHeight(27);
        QHBoxLayout* statusLayout = new QHBoxLayout(statusBar);
        statusLayout->setContentsMargins(12, 0, 12, 0);
        statusLayout->setSpacing(16);
        coordinateLabel_ = new QLabel(QString::fromUtf8("视图 X: --   |   Y: --"), statusBar);
        connectionLabel_ = new QLabel(QString::fromUtf8("数据库连接待验证"), statusBar);
        projectStatusLabel_ = new QLabel(statusBar);
        gridStatusLabel_ = new QLabel(statusBar);
        statusLabel_ = new QLabel(QString::fromUtf8("就绪"), statusBar);
        for (QLabel* label : {coordinateLabel_, connectionLabel_, projectStatusLabel_, gridStatusLabel_, statusLabel_}) {
            label->setObjectName("statusText");
        }
        // V1.1：状态段之间加竖分隔线，长名称省略显示并给完整 tooltip。
        auto makeStatusSep = [statusBar]() {
            QFrame* sep = new QFrame(statusBar);
            sep->setObjectName("statusSep");
            sep->setFixedSize(1, 14);
            return sep;
        };
        busyBar_ = new QProgressBar(statusBar);
        busyBar_->setRange(0, 0);
        busyBar_->setFixedWidth(110);
        busyBar_->setTextVisible(false);
        busyBar_->hide();
        statusLayout->addWidget(coordinateLabel_);
        statusLayout->addWidget(busyBar_);
        statusLayout->addStretch(1);
        statusLayout->addWidget(connectionLabel_);
        statusLayout->addWidget(makeStatusSep());
        statusLayout->addWidget(projectStatusLabel_);
        statusLayout->addWidget(makeStatusSep());
        statusLayout->addWidget(gridStatusLabel_);
        statusLayout->addWidget(makeStatusSep());
        statusLayout->addWidget(statusLabel_);

        QVBoxLayout* rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);
        rootLayout->addWidget(menuBar);
        rootLayout->addWidget(toolbar);
        rootLayout->addWidget(mainSplitter_, 1);
        rootLayout->addWidget(statusBar);

        if (attributeConstrainedBeltCheck_) {
            connect(attributeConstrainedBeltCheck_, &QCheckBox::toggled,
                    this, &FaultTraceDialog::updateOptionControls);
        }
        connect(projectEdit_, &QLineEdit::textChanged, this, &FaultTraceDialog::updateStatusContext);
        connect(databaseEdit_, &QLineEdit::textChanged, this, &FaultTraceDialog::updateStatusContext);
        connect(gridCombo_, &QComboBox::currentTextChanged, this, &FaultTraceDialog::updateStatusContext);
        connect(gridCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
            if (gridCombo_ && !gridCombo_->currentText().trimmed().isEmpty() &&
                (!showInputLayerCheck_ || showInputLayerCheck_->isChecked())) {
                QTimer::singleShot(0, this, &FaultTraceDialog::loadSelectedGridView);
            }
        });
        if (gridCombo_ && gridCombo_->lineEdit()) {
            connect(gridCombo_->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
                if (gridCombo_ && !gridCombo_->currentText().trimmed().isEmpty() &&
                    (!showInputLayerCheck_ || showInputLayerCheck_->isChecked())) {
                    QTimer::singleShot(0, this, &FaultTraceDialog::loadSelectedGridView);
                }
            });
        }
        connect(outputEdit_, &QLineEdit::textChanged, this, &FaultTraceDialog::updateStatusContext);
        connect(traceModeCombo_, &QComboBox::currentTextChanged, this, &FaultTraceDialog::updateStatusContext);
        connect(thresholdModeCombo_, &QComboBox::currentTextChanged, this, &FaultTraceDialog::updateOptionControls);
        connect(rootAntiMergeCheck_, &QCheckBox::toggled, this, &FaultTraceDialog::updateOptionControls);
        connect(localAntiMergeCheck_, &QCheckBox::toggled, this, &FaultTraceDialog::updateOptionControls);
        connect(residualTraceCheck_, &QCheckBox::toggled, this, &FaultTraceDialog::updateOptionControls);
        connect(importDatCheck_, &QCheckBox::toggled, this, &FaultTraceDialog::updateOptionControls);

        presetCombo_->setCurrentText(QString::fromUtf8("防黏连增强"));
        applyParameterPreset(QString::fromUtf8("防黏连增强"));
        updateStatusContext();
        appendLog(QString::fromUtf8("[系统就绪] 等待参数输入及任务执行..."));
        if (projectEdit_ && !projectEdit_->text().trimmed().isEmpty()) {
            appendLog(QString::fromUtf8("加载项目配置：%1 成功。").arg(projectEdit_->text().trimmed()));
        } else {
            appendLog(QString::fromUtf8("未检测到当前项目名，请先在【数据库配置】中填写项目，再刷新网格列表。"));
        }
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
    }
}

QString FaultTraceDialog::defaultRunnerPath() const {
    const QString fromEnv = envValue("FPT_GEOEAST_RUNNER");
    if (!fromEnv.isEmpty()) {
        return QDir::toNativeSeparators(fromEnv);
    }
    QDir appDir(QCoreApplication::applicationDirPath());
    QDir parentDir(appDir);
    QString parentRunner;
    if (parentDir.cdUp()) {
        parentRunner = parentDir.absoluteFilePath("faultpolytrace_geoeast_runner.exe");
    }
    const QString geoEastRoot = detectGeoEastRoot();
    const QString repoRunner = QDir(repoRootFromAppDir()).absoluteFilePath("geoeast_plugin/bo_runner/faultpolytrace_geoeast_runner.exe");
    const QString detected = firstExistingPath({
        appDir.absoluteFilePath("faultpolytrace_geoeast_runner.exe"),
        appDir.absoluteFilePath("bin/faultpolytrace_geoeast_runner.exe"),
        parentRunner,
        geoEastRoot.isEmpty() ? QString() : QDir(geoEastRoot).absoluteFilePath("GeoEast-RC/bin/faultpolytrace_geoeast_runner.exe"),
        repoRunner
    });
    return detected.isEmpty() ? QString() : detected;
}

QString FaultTraceDialog::defaultOfficialDatPath() const {
    return defaultOfficialDatFromEnvironment();
}

void FaultTraceDialog::browseRunner() {
    const QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("选择 GeoEast 执行程序"), QFileInfo(runnerEdit_->text()).absolutePath(),
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

void FaultTraceDialog::showDatabaseConfigDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("GeoEast 数据库配置"));
    dialog.setWindowFlags((dialog.windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint) &
                          ~Qt::WindowMaximizeButtonHint &
                          ~Qt::WindowContextHelpButtonHint);
    dialog.setModal(true);
    dialog.resize(760, 380);
    dialog.setMinimumSize(720, 360);
    dialog.setStyleSheet(fpt_ui::configDialogStyle());

    QVBoxLayout* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame* headerFrame = new QFrame(&dialog);
    headerFrame->setObjectName("dialogHeader");
    
    QHBoxLayout* headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(0, 0, 10, 0);
    QLabel* header = new QLabel(QString::fromUtf8("GeoEast 连接配置"), headerFrame);
    header->setObjectName("dialogHeader");
    header->setFixedHeight(36);
    QPushButton* helpButton = new QPushButton(QString::fromUtf8("帮助"), headerFrame);
    helpButton->setToolTip(QString::fromUtf8("查看每个配置项从哪里获取"));
    helpButton->setFixedWidth(64);
    headerLayout->addWidget(header, 1);
    headerLayout->addWidget(helpButton);
    root->addWidget(headerFrame);

    QWidget* body = new QWidget(&dialog);
    QVBoxLayout* bodyRoot = new QVBoxLayout(body);
    bodyRoot->setContentsMargins(16, 14, 16, 12);
    bodyRoot->setSpacing(10);

    QGridLayout* bodyLayout = new QGridLayout();
    bodyLayout->setHorizontalSpacing(8);
    bodyLayout->setVerticalSpacing(8);
    bodyRoot->addLayout(bodyLayout);

    QLineEdit* runnerCopy = new QLineEdit(runnerEdit_->text(), body);
    QLineEdit* geoEastRootCopy = new QLineEdit(geoEastRootEdit_->text(), body);
    QLineEdit* dataRootCopy = new QLineEdit(geoEastDataRootEdit_->text(), body);
    QLineEdit* hostCopy = new QLineEdit(pgHostEdit_->text(), body);
    QLineEdit* portCopy = new QLineEdit(pgPortEdit_->text(), body);
    QLineEdit* databaseCopy = new QLineEdit(databaseEdit_->text(), body);
    QLineEdit* projectCopy = new QLineEdit(projectEdit_->text(), body);
    QLineEdit* userCopy = new QLineEdit(userEdit_->text(), body);
    QLineEdit* passwordCopy = new QLineEdit(passwordEdit_->text(), body);
    passwordCopy->setEchoMode(QLineEdit::Password);
    for (QLineEdit* pathEdit : {runnerCopy, geoEastRootCopy, dataRootCopy}) {
        pathEdit->setCursorPosition(0);
        pathEdit->setToolTip(pathEdit->text());
    }

    auto addPathRow = [&](int row, const QString& labelText, QLineEdit* edit, const QString& dialogTitle, bool directory) {
        QLabel* label = new QLabel(labelText, body);
        label->setFixedWidth(132);
        QPushButton* browse = new QPushButton(QString::fromUtf8("浏览..."), body);
        browse->setMinimumWidth(80);
        browse->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        connect(browse, &QPushButton::clicked, &dialog, [this, edit, dialogTitle, directory]() {
            QString path;
            if (directory) {
                path = QFileDialog::getExistingDirectory(this, uiText(dialogTitle), edit->text());
            } else {
                path = QFileDialog::getOpenFileName(this, uiText(dialogTitle), QFileInfo(edit->text()).absolutePath(),
                                                   uiText(QString::fromUtf8("可执行文件 (*.exe);;所有文件 (*.*)")));
            }
            if (!path.isEmpty()) {
                edit->setText(QDir::toNativeSeparators(path));
            }
        });
        bodyLayout->addWidget(label, row, 0);
        bodyLayout->addWidget(edit, row, 1, 1, 3);
        bodyLayout->addWidget(browse, row, 4);
    };

    addPathRow(0, QString::fromUtf8("执行程序文件:"), runnerCopy, QString::fromUtf8("选择 GeoEast 执行程序"), false);
    addPathRow(1, QString::fromUtf8("GeoEast 根目录:"), geoEastRootCopy, QString::fromUtf8("选择 GeoEast 安装根目录"), true);
    addPathRow(2, QString::fromUtf8("GeoEast 数据根目录:"), dataRootCopy, QString::fromUtf8("选择 GeoEastData 根目录"), true);

    QFrame* line = new QFrame(body);
    line->setObjectName("line");
    bodyRoot->addWidget(line);

    QGridLayout* dbLayout = new QGridLayout();
    dbLayout->setHorizontalSpacing(8);
    dbLayout->setVerticalSpacing(8);
    bodyRoot->addLayout(dbLayout);

    dbLayout->addWidget(new QLabel(QString::fromUtf8("PG 主机:"), body), 0, 0);
    dbLayout->addWidget(hostCopy, 0, 1);
    dbLayout->addWidget(new QLabel(QString::fromUtf8("端口:"), body), 0, 2);
    dbLayout->addWidget(portCopy, 0, 3, 1, 2);
    dbLayout->addWidget(new QLabel(QString::fromUtf8("数据库:"), body), 1, 0);
    dbLayout->addWidget(databaseCopy, 1, 1);
    dbLayout->addWidget(new QLabel(QString::fromUtf8("项目:"), body), 1, 2);
    dbLayout->addWidget(projectCopy, 1, 3, 1, 2);
    dbLayout->addWidget(new QLabel(QString::fromUtf8("用户:"), body), 2, 0);
    dbLayout->addWidget(userCopy, 2, 1);
    dbLayout->addWidget(new QLabel(QString::fromUtf8("密码:"), body), 2, 2);
    dbLayout->addWidget(passwordCopy, 2, 3, 1, 2);
    dbLayout->setColumnStretch(1, 1);
    dbLayout->setColumnStretch(3, 1);

    QLabel* checkStatus = new QLabel(QString::fromUtf8("尚未检查配置。"), body);
    checkStatus->setObjectName("checkStatus");
    bodyRoot->addWidget(checkStatus);
    root->addWidget(body, 1);

    QFrame* footer = new QFrame(&dialog);
    footer->setObjectName("dialogFooter");
    QHBoxLayout* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(15, 10, 15, 10);
    QPushButton* autoDetectButton = new QPushButton(QString::fromUtf8("自动检测"), footer);
    QPushButton* checkButton = new QPushButton(QString::fromUtf8("检查配置"), footer);
    footerLayout->addWidget(autoDetectButton);
    footerLayout->addWidget(checkButton);
    footerLayout->addStretch(1);
    QPushButton* cancelButton = new QPushButton(QString::fromUtf8("取消"), footer);
    QPushButton* applyButton = new QPushButton(QString::fromUtf8("应用并保存配置"), footer);
    applyButton->setObjectName("applyButton");
    footerLayout->addWidget(cancelButton);
    footerLayout->addWidget(applyButton);
    root->addWidget(footer);

    auto applyAutoDetection = [&]() {
        const QString rootPath = detectGeoEastRoot();
        const QString dataRootPath = detectGeoEastDataRoot(rootPath);
        const QString runnerPath = defaultRunnerPath();
        if (!runnerPath.isEmpty()) {
            runnerCopy->setText(runnerPath);
        }
        if (!rootPath.isEmpty()) {
            geoEastRootCopy->setText(rootPath);
        }
        if (!dataRootPath.isEmpty()) {
            dataRootCopy->setText(dataRootPath);
        }
        for (QLineEdit* pathEdit : {runnerCopy, geoEastRootCopy, dataRootCopy}) {
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
        return validateGeoEastConfigValues(runnerCopy->text(), geoEastRootCopy->text(), dataRootCopy->text(),
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
    connect(autoDetectButton, &QPushButton::clicked, &dialog, applyAutoDetection);
    connect(checkButton, &QPushButton::clicked, &dialog, showCheckResult);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(applyButton, &QPushButton::clicked, &dialog, [&]() {
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
        runnerEdit_->setText(runnerCopy->text());
        geoEastRootEdit_->setText(geoEastRootCopy->text());
        geoEastDataRootEdit_->setText(dataRootCopy->text());
        pgHostEdit_->setText(hostCopy->text());
        pgPortEdit_->setText(portCopy->text());
        databaseEdit_->setText(databaseCopy->text());
        projectEdit_->setText(projectCopy->text());
        userEdit_->setText(userCopy->text());
        passwordEdit_->setText(passwordCopy->text());
        saveSettings();
        updateStatusContext();
        appendLog(QString::fromUtf8("GeoEast 连接配置已保存。"));
        dialog.accept();
        if (process_->state() == QProcess::NotRunning) {
            refreshOutputsAfterGrids_ = true;
            QTimer::singleShot(0, this, &FaultTraceDialog::refreshGrids);
        }
    });

    fpt_ui::captureTranslatableText(&dialog);
    fpt_ui::applyTranslatableText(&dialog, languageCode());
    dialog.exec();
}

void FaultTraceDialog::saveCurrentPreset() {
    saveSettings();
    updateStatusContext();
    appendLog(QString::fromUtf8("当前参数和界面配置已保存。"));
}

void FaultTraceDialog::saveParameterTemplate() {
    const QString suggested = QDir(userExportDirectory()).absoluteFilePath("FaultPolyTrace_参数模板.json");
    const QString path = QFileDialog::getSaveFileName(
        this,
        uiText(QString::fromUtf8("保存参数模板")),
        suggested,
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
    appendStatus(QString::fromUtf8("参数模板已保存"));
}

void FaultTraceDialog::loadParameterTemplate() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        uiText(QString::fromUtf8("打开参数模板")),
        userExportDirectory(),
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
    appendStatus(QString::fromUtf8("参数模板已载入并通过校验"));
}

void FaultTraceDialog::saveRunLog() {
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
    const QString suggested = QDir(userExportDirectory()).absoluteFilePath(
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
    appendStatus(QString::fromUtf8("运行日志已保存"));
}

void FaultTraceDialog::copyResultSummary() {
    const QString selectedResult = outputCatalogCombo_ && !outputCatalogCombo_->currentData().toString().trimmed().isEmpty()
        ? outputCatalogCombo_->currentData().toString().trimmed()
        : (outputEdit_ ? outputEdit_->text().trimmed() : QString());
    const QString viewStatus = renderViewWidget_
        ? static_cast<RenderViewWidget*>(renderViewWidget_)->dataStatus()
        : QString();
    const QString summary = QString::fromUtf8(
        "FaultPolyTrace 结果摘要\n数据库：%1\n项目：%2\n输入网格：%3\n结果对象：BOFaultPolygon/%4\n"
        "预设模板：%5\n质量模式：%6\n显示样式：%7，线宽 %8，透明度 %9\n视图状态：%10")
        .arg(databaseEdit_->text().trimmed())
        .arg(projectEdit_->text().trimmed())
        .arg(gridCombo_->currentText().trimmed())
        .arg(selectedResult)
        .arg(comboKey(presetCombo_))
        .arg(comboKey(qualityModeCombo_))
        .arg(polygonColorEdit_->text())
        .arg(QString::number(polygonLineWidthSpin_->value(), 'f', 3))
        .arg(QString::number(polygonOpacitySpin_->value(), 'f', 2))
        .arg(viewStatus.isEmpty() ? uiText(QString::fromUtf8("尚未载入图层")) : uiText(viewStatus));
    QApplication::clipboard()->setText(summary);
    appendStatus(QString::fromUtf8("当前结果摘要已复制"));
}

void FaultTraceDialog::restoreDefaults() {
    outputEdit_->setText("FaultPolyTrace_Result");
    setComboByKey(presetCombo_, QString::fromUtf8("防黏连增强"));
    applyParameterPreset(comboKey(presetCombo_));
    traceModeCombo_->setCurrentText("ridge-belt");
    thresholdModeCombo_->setCurrentText("percentile");
    thresholdSpin_->setValue(0.95);
    percentileSpin_->setValue(0.98);
    minAreaSpin_->setValue(120.0);
    setPolygonColor(QColor("#00FFFF"));
    polygonLineWidthSpin_->setValue(0.03);
    polygonOpacitySpin_->setValue(1.0);
    importDatCheck_->setChecked(false);
    overwriteGridCheck_->setChecked(true);
    if (showInputLayerCheck_) {
        showInputLayerCheck_->setChecked(false);
    }
    if (showResultLayerCheck_) {
        showResultLayerCheck_->setChecked(false);
    }
    updateOptionControls();
    updateLayerVisibility();
    saveSettings();
    appendLog(QString::fromUtf8("已恢复推荐默认配置：防黏连增强、青色结果线、默认输出名。数据库连接信息已保留。"));
    appendStatus(QString::fromUtf8("已恢复默认配置"));
}

void FaultTraceDialog::showHelpHint() {
    appendLog(QString::fromUtf8("帮助文档：docs/用户操作手册.md；异机交付说明：docs/评审交付与异机运行说明.md。"));
    QMessageBox::information(this,
                             uiText(QString::fromUtf8("FaultPolyTrace 帮助提示")),
                             uiText(QString::fromUtf8("常用流程：\n"
                                               "1. 在【数据库配置】中确认 GeoEast 根目录、数据根目录、项目、用户和执行程序。\n"
                                                "2. 点击【刷新网格列表】读取 GeoEast 中的输入网格。\n"
                                                "3. 设置输入网格、输出多边形、结果颜色和必要参数。\n"
                                                "4. 点击【开始追踪】，完成后本窗口会显示原始网格和结果多边形。\n"
                                                "5. 如需用 GeoEast 官方窗口查看，点击【打开工区底图】或【打开平面成图】，再在图层树中勾选 Grids 和 Fault Polygons。\n"
                                                "6. 如需移除旧结果，在【结果查看】中点击【删除当前结果】。\n\n"
                                                "详细说明见 docs/用户操作手册.md 和 docs/评审交付与异机运行说明.md。")));
}

QString FaultTraceDialog::languageCode() const {
    return currentLanguage_ == UiLanguage::English ? "en" : "zh-CN";
}

QString FaultTraceDialog::uiText(const char* zh) const {
    return uiText(QString::fromUtf8(zh));
}

QString FaultTraceDialog::uiText(const QString& zh) const {
    return fpt_ui::translate(zh, languageCode());
}

void FaultTraceDialog::setComboByKey(QComboBox* combo, const QString& key) {
    fpt_ui::setComboByKey(combo, key);
}

QString FaultTraceDialog::comboKey(const QComboBox* combo) const {
    return fpt_ui::comboCurrentKey(combo);
}

void FaultTraceDialog::prepareLanguageMetadata() {
    fpt_ui::captureTranslatableText(this);
    const QStringList presetKeys = {
        QString::fromUtf8("全覆盖优先"),
        QString::fromUtf8("贴合融合推荐"),
        QString::fromUtf8("防黏连增强"),
        QString::fromUtf8("均衡推荐"),
        QString::fromUtf8("保守高精度"),
        QString::fromUtf8("弱响应高召回"),
        QString::fromUtf8("宽断层带"),
        QString::fromUtf8("细碎断裂")
    };
    fpt_ui::setComboItems(presetCombo_, presetKeys, languageCode());
}

void FaultTraceDialog::setLanguage(UiLanguage language) {
    if (currentLanguage_ == language) {
        retranslateUi();
        return;
    }
    currentLanguage_ = language;
    retranslateUi();
    saveSettings();
    appendStatus(uiText(QString::fromUtf8("界面语言已切换")));
}

void FaultTraceDialog::retranslateUi() {
    setWindowTitle(uiText("FaultPolyTrace 断层多边形自动追踪"));
    fpt_ui::applyTranslatableText(this, languageCode());
    fpt_ui::refreshComboItems(presetCombo_, languageCode());
    fpt_ui::refreshComboItems(qualityModeCombo_, languageCode());
    fpt_ui::refreshComboItems(outputCatalogCombo_, languageCode());
    if (refreshOutputsButton_) {
        refreshOutputsButton_->setText(uiText(QString::fromUtf8("刷新结果列表")));
    }
    if (loadSelectedOutputButton_) {
        loadSelectedOutputButton_->setText(uiText(QString::fromUtf8("显示所选结果")));
    }
    if (layerTitleLabel_) {
        layerTitleLabel_->setText(uiText(QString::fromUtf8("显示图层")));
    }
    if (layerHintLabel_) {
        layerHintLabel_->setText(uiText(QString::fromUtf8("同时开启两层即可叠加查看。")));
    }
    if (showInputLayerCheck_) {
        showInputLayerCheck_->setText(uiText(QString::fromUtf8("原始网格")));
        showInputLayerCheck_->setToolTip(uiText(QString::fromUtf8("显示或隐藏输入断层属性网格。GeoEast 模式会按需导出当前 BOMapGrid 视图。")));
    }
    if (showResultLayerCheck_) {
        showResultLayerCheck_->setText(uiText(QString::fromUtf8("结果多边形")));
        showResultLayerCheck_->setToolTip(uiText(QString::fromUtf8("显示或隐藏追踪结果多边形。没有结果时请先追踪或选择已有结果。")));
    }
    if (languageChineseAction_) {
        languageChineseAction_->setText(uiText("简体中文"));
        languageChineseAction_->setChecked(currentLanguage_ == UiLanguage::SimplifiedChinese);
    }
    if (languageEnglishAction_) {
        languageEnglishAction_->setText(uiText("English"));
        languageEnglishAction_->setChecked(currentLanguage_ == UiLanguage::English);
    }
    if (rootAntiMergeMinPartCellsSpin_) {
        rootAntiMergeMinPartCellsSpin_->setSpecialValueText(uiText("自动"));
    }
    if (renderViewWidget_) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setLanguage(currentLanguage_);
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
    updateStatusContext();
}

void FaultTraceDialog::loadSettings() {
    QSettings settings(launcherSettingsPath(), QSettings::IniFormat);
    const int schemaVersion = settings.value("configSchemaVersion", 0).toInt();
    currentLanguage_ = fpt_ui::normalizeLanguageCode(settings.value("language", "zh-CN").toString()) == "en"
        ? UiLanguage::English
        : UiLanguage::SimplifiedChinese;
    const QString savedRunner = settingOrCurrent(settings, "runner", runnerEdit_->text(), schemaVersion).trimmed();
    runnerEdit_->setText(QFileInfo::exists(savedRunner) ? savedRunner : defaultRunnerPath());
    const QString savedRoot = settingOrCurrent(settings, "geoEastRoot", geoEastRootEdit_->text(), schemaVersion).trimmed();
    const QString effectiveRoot = QDir(savedRoot).exists() ? savedRoot : detectGeoEastRoot();
    geoEastRootEdit_->setText(effectiveRoot);
    const QString savedDataRoot = settingOrCurrent(settings, "geoEastDataRoot", geoEastDataRootEdit_->text(), schemaVersion).trimmed();
    geoEastDataRootEdit_->setText(QDir(savedDataRoot).exists() ? savedDataRoot : detectGeoEastDataRoot(effectiveRoot));
    pgHostEdit_->setText(settings.value("pgHost", pgHostEdit_->text()).toString());
    pgPortEdit_->setText(settings.value("pgPort", pgPortEdit_->text()).toString());
    databaseEdit_->setText(settings.value("database", databaseEdit_->text()).toString());
    projectEdit_->setText(settingOrCurrent(settings, "project", projectEdit_->text(), schemaVersion));
    userEdit_->setText(settingOrCurrent(settings, "user", userEdit_->text(), schemaVersion));
    setComboByKey(presetCombo_, settings.value("preset", comboKey(presetCombo_)).toString());
    setComboByKey(qualityModeCombo_, settings.value("qualityMode", QString::fromUtf8("正式处理")).toString());
    pendingGridRestore_ = settingOrCurrent(settings, "grid", gridCombo_->currentText(), schemaVersion).trimmed();
    if (pendingGridRestore_.startsWith(QString::fromUtf8("请先")) ||
        pendingGridRestore_.startsWith(QStringLiteral("Configure"))) {
        pendingGridRestore_.clear();
    }
    gridCombo_->clear();
    gridCombo_->setCurrentText(QString());
    gridCombo_->setProperty("fptCatalogReady", false);
    const QString savedOutput = settings.value("output", outputEdit_->text()).toString().trimmed();
    outputEdit_->setText(savedOutput.isEmpty() || savedOutput.startsWith(QString::fromUtf8("请先")) ||
                         savedOutput.startsWith(QString::fromUtf8("当前项目")) ||
                         savedOutput.startsWith(QStringLiteral("Configure"))
                             ? QStringLiteral("FaultPolyTrace_Result")
                             : savedOutput);
    const QString savedDat = settingOrCurrent(settings, "datFile", datFileEdit_->text(), schemaVersion).trimmed();
    const bool restoreDat = settings.value("importDat", importDatCheck_->isChecked()).toBool() &&
                            QFileInfo::exists(savedDat);
    datFileEdit_->setText(restoreDat ? savedDat : QString());
    importDatCheck_->setChecked(restoreDat);
    overwriteGridCheck_->setChecked(settings.value("overwriteGrid", overwriteGridCheck_->isChecked()).toBool());
    polygonColorEdit_->setText(settings.value("polygonColor", polygonColorEdit_->text()).toString());
    setPolygonColor(QColor(polygonColorEdit_->text()));
    polygonLineWidthSpin_->setValue(settings.value("polygonLineWidth", polygonLineWidthSpin_->value()).toDouble());
    polygonOpacitySpin_->setValue(settings.value("polygonOpacity", polygonOpacitySpin_->value()).toDouble());
    traceModeCombo_->setCurrentText(settings.value("traceMode", traceModeCombo_->currentText()).toString());
    thresholdModeCombo_->setCurrentText(settings.value("thresholdMode", thresholdModeCombo_->currentText()).toString());
    thresholdSpin_->setValue(settings.value("threshold", thresholdSpin_->value()).toDouble());
    percentileSpin_->setValue(settings.value("percentile", percentileSpin_->value()).toDouble());
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
    simplifyToleranceSpin_->setValue(settings.value("simplifyTolerance", simplifyToleranceSpin_->value()).toDouble());
    smoothIterationsSpin_->setValue(settings.value("smoothIterations", smoothIterationsSpin_->value()).toInt());
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
    if (showInputLayerCheck_) {
        showInputLayerCheck_->setChecked(false);
    }
    if (showResultLayerCheck_) {
        showResultLayerCheck_->setChecked(false);
    }
    if (mainSplitter_) {
        const QByteArray splitterState = settings.value("mainSplitterState").toByteArray();
        if (!splitterState.isEmpty()) {
            mainSplitter_->restoreState(splitterState);
        }
        QList<int> sizes = mainSplitter_->sizes();
        if (sizes.size() >= 2 && sizes.at(0) < 400) {
            mainSplitter_->setSizes({420, qMax(620, sizes.at(1))});
        }
    }
    updateOptionControls();
    updateLayerVisibility();
    updateStatusContext();
    retranslateUi();
}

void FaultTraceDialog::saveSettings() const {
    QSettings settings(launcherSettingsPath(), QSettings::IniFormat);
    settings.setValue("configSchemaVersion", 4);
    settings.setValue("language", languageCode());
    settings.setValue("runner", runnerEdit_->text());
    settings.setValue("geoEastRoot", geoEastRootEdit_->text());
    settings.setValue("geoEastDataRoot", geoEastDataRootEdit_->text());
    settings.setValue("pgHost", pgHostEdit_->text());
    settings.setValue("pgPort", pgPortEdit_->text());
    settings.setValue("database", databaseEdit_->text());
    settings.setValue("project", projectEdit_->text());
    settings.setValue("user", userEdit_->text());
    settings.setValue("preset", comboKey(presetCombo_));
    settings.setValue("qualityMode", comboKey(qualityModeCombo_));
    settings.setValue("grid", gridCombo_->currentText());
    settings.setValue("output", outputEdit_->text());
    settings.setValue("datFile", datFileEdit_->text());
    settings.setValue("importDat", importDatCheck_->isChecked());
    settings.setValue("overwriteGrid", overwriteGridCheck_->isChecked());
    settings.setValue("polygonColor", polygonColorEdit_->text());
    settings.setValue("polygonLineWidth", polygonLineWidthSpin_->value());
    settings.setValue("polygonOpacity", polygonOpacitySpin_->value());
    settings.setValue("traceMode", traceModeCombo_->currentText());
    settings.setValue("thresholdMode", thresholdModeCombo_->currentText());
    settings.setValue("threshold", thresholdSpin_->value());
    settings.setValue("percentile", percentileSpin_->value());
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
    settings.setValue("simplifyTolerance", simplifyToleranceSpin_->value());
    settings.setValue("smoothIterations", smoothIterationsSpin_->value());
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
    if (showInputLayerCheck_) {
        settings.setValue("showInputLayer", showInputLayerCheck_->isChecked());
    }
    if (showResultLayerCheck_) {
        settings.setValue("showResultLayer", showResultLayerCheck_->isChecked());
    }
    if (mainSplitter_) {
        settings.setValue("mainSplitterState", mainSplitter_->saveState());
    }
}

QStringList FaultTraceDialog::missingGeoEastConfig(bool requireGrid, bool requireOutput) const {
    QStringList missing;
    const QString runner = runnerEdit_ ? runnerEdit_->text().trimmed() : QString();
    const QString geoEastRoot = geoEastRootEdit_ ? geoEastRootEdit_->text().trimmed() : QString();
    const QString dataRoot = geoEastDataRootEdit_ ? geoEastDataRootEdit_->text().trimmed() : QString();
    const QString database = databaseEdit_ ? databaseEdit_->text().trimmed() : QString();
    const QString project = projectEdit_ ? projectEdit_->text().trimmed() : QString();
    const QString user = userEdit_ ? userEdit_->text().trimmed() : QString();
    const QString grid = gridCombo_ ? gridCombo_->currentText().trimmed() : QString();
    const QString output = outputEdit_ ? outputEdit_->text().trimmed() : QString();

    if (runner.isEmpty()) {
        missing << QString::fromUtf8("执行程序文件");
    } else if (!QFileInfo::exists(runner)) {
        missing << QString::fromUtf8("执行程序文件不存在：%1").arg(QDir::toNativeSeparators(runner));
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

bool FaultTraceDialog::ensureGeoEastConfigReady(const QString& operation, bool requireGrid, bool requireOutput) {
    const QStringList missing = missingGeoEastConfig(requireGrid, requireOutput);
    if (missing.isEmpty()) {
        return true;
    }

    appendLog(QString::fromUtf8("无法执行【%1】：GeoEast 配置不完整。缺失或无效项：%2")
                  .arg(operation, missing.join(QString::fromUtf8("；"))));

    // V1.1：按缺失项路由——主面板字段直接定位聚焦，数据库项才进配置窗口。
    const bool missingOutputName = requireOutput && outputEdit_ && outputEdit_->text().trimmed().isEmpty();
    const bool missingGridChoice = requireGrid && gridCombo_ && gridCombo_->currentText().trimmed().isEmpty();
    const bool mainPanelFixable = missingOutputName || missingGridChoice;

    QMessageBox message(this);
    message.setIcon(QMessageBox::Warning);
    message.setWindowTitle(uiText(QString::fromUtf8("GeoEast 配置不完整")));
    message.setText(uiText(QString::fromUtf8("执行【%1】前需要先补齐 GeoEast 配置。")).arg(operation));
    message.setInformativeText(uiText(QString::fromUtf8("缺失或无效项：\n\n- %1"))
                                   .arg(missing.join(QString::fromUtf8("\n- "))));
    QPushButton* fixButton = nullptr;
    QPushButton* configButton = nullptr;
    if (mainPanelFixable) {
        fixButton = message.addButton(uiText(QString::fromUtf8("去填写")), QMessageBox::AcceptRole);
    }
    if (!mainPanelFixable || missing.size() > (missingOutputName ? 1 : 0) + (missingGridChoice ? 1 : 0)) {
        configButton = message.addButton(uiText(QString::fromUtf8("打开数据库配置")), QMessageBox::AcceptRole);
    }
    message.addButton(uiText(QString::fromUtf8("取消")), QMessageBox::RejectRole);
    message.exec();
    if (fixButton && message.clickedButton() == fixButton) {
        QWidget* target = missingOutputName ? static_cast<QWidget*>(outputEdit_)
                                            : static_cast<QWidget*>(gridCombo_);
        if (target) {
            target->setFocus(Qt::OtherFocusReason);
            const QString highlight = QStringLiteral("border: 1px solid #D93025;");
            target->setStyleSheet(highlight);
            QTimer::singleShot(2600, target, [target]() { target->setStyleSheet(QString()); });
        }
    } else if (configButton && message.clickedButton() == configButton) {
        showDatabaseConfigDialog();
    }
    appendStatus(QString::fromUtf8("GeoEast 配置不完整，已停止操作"));
    return false;
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
    if (showResultLayerCheck_) {
        showResultLayerCheck_->setAccentColor(effective);
    }
    updateStatusContext();
}

void FaultTraceDialog::updateOptionControls() {
    const bool fixedThreshold = thresholdModeCombo_ && thresholdModeCombo_->currentText() == "fixed";
    if (thresholdSpin_) {
        thresholdSpin_->setEnabled(fixedThreshold);
    }
    if (percentileSpin_) {
        percentileSpin_->setEnabled(!fixedThreshold);
    }

    const bool attributeEnabled = attributeConstrainedBeltCheck_ &&
                                  attributeConstrainedBeltCheck_->isChecked();
    if (attributeEnvelopeRadiusSpin_) {
        attributeEnvelopeRadiusSpin_->setEnabled(attributeEnabled);
    }

    const bool importDat = importDatCheck_ && importDatCheck_->isChecked();
    if (datFileEdit_) {
        datFileEdit_->setEnabled(importDat);
    }
    if (overwriteGridCheck_) {
        overwriteGridCheck_->setEnabled(importDat);
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

    updateStatusContext();
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
         << "--user" << userEdit_->text().trimmed();
    if (!passwordEdit_->text().isEmpty()) {
        args << "--password" << passwordEdit_->text();
    }
    return args;
}

QString FaultTraceDialog::previewOutputDir() const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("FaultPolyTracePreviewCache");
    }
    const QString db = sanitizeFileComponent(databaseEdit_ ? databaseEdit_->text() : QString(), "database");
    const QString project = sanitizeFileComponent(projectEdit_ ? projectEdit_->text() : QString(), "project");
    return QDir(base).absoluteFilePath(QString("GeoEastPreview/%1/%2").arg(db, project));
}

QString FaultTraceDialog::previewGridPath() const {
    const QString grid = sanitizeFileComponent(gridCombo_ ? gridCombo_->currentText() : QString(), "grid");
    const QString output = sanitizeFileComponent(outputEdit_ ? outputEdit_->text() : QString(), "output");
    return QDir(previewOutputDir()).absoluteFilePath(QString("%1_%2_grid.txt").arg(grid, output));
}

QString FaultTraceDialog::previewPolygonsPath() const {
    const QString grid = sanitizeFileComponent(gridCombo_ ? gridCombo_->currentText() : QString(), "grid");
    const QString output = sanitizeFileComponent(outputEdit_ ? outputEdit_->text() : QString(), "output");
    return QDir(previewOutputDir()).absoluteFilePath(QString("%1_%2_polygons.csv").arg(grid, output));
}

bool FaultTraceDialog::launchGeoEastApp(const QString& exeName, const QString& displayName) {
    if (activateApplicationWindow(exeName)) {
        appendStatus(QString::fromUtf8("已切换到正在运行的 %1").arg(displayName));
        return true;
    }
    const QString geoEastRoot = QDir::fromNativeSeparators(geoEastRootEdit_ ? geoEastRootEdit_->text().trimmed() : QString());
    const QString exePath = QDir(geoEastRoot).absoluteFilePath("GeoEast-RC/bin/" + exeName);
    if (!QFileInfo::exists(exePath)) {
        QMessageBox::warning(this, uiText(QString::fromUtf8("GeoEast 程序不存在")),
                             uiText(QString::fromUtf8("未找到 %1：\n%2").arg(displayName, QDir::toNativeSeparators(exePath))));
        return false;
    }

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    configureGeoEastEnvironment(environment);
    process.setProgram(QDir::toNativeSeparators(exePath));
    process.setWorkingDirectory(QFileInfo(exePath).absolutePath());
    process.setProcessEnvironment(environment);
    qint64 pid = 0;
    appendStatus(QString::fromUtf8("正在打开 %1...").arg(displayName));
    if (!process.startDetached(&pid)) {
        QMessageBox::warning(this, uiText(QString::fromUtf8("启动失败")),
                             uiText(QString::fromUtf8("无法启动 %1。请确认 GeoEast 环境和 license 可用。").arg(displayName)));
        return false;
    }

    appendLog(QString::fromUtf8("已发送 %1 启动请求（进程 %2）：%3").arg(displayName).arg(pid).arg(QDir::toNativeSeparators(exePath)));
    appendStatus(QString::fromUtf8("正在确认 %1 是否可用...").arg(displayName));
    QTimer::singleShot(1400, this, [this, pid, exeName, displayName]() {
        if (isProcessAlive(pid) || activateApplicationWindow(exeName)) {
            appendStatus(QString::fromUtf8("已打开 %1，按官方图层树查看结果").arg(displayName));
            appendLog(QString::fromUtf8("官方窗口中请在图层树加载 Grids/%1 与 Fault Polygons/%2。")
                          .arg(gridCombo_ ? gridCombo_->currentText().trimmed() : QString(),
                               outputEdit_ ? outputEdit_->text().trimmed() : QString()));
            return;
        }

        const bool hostActivated = activateApplicationWindow(QStringLiteral("geoeast.exe"));
        appendStatus(QString::fromUtf8("%1 需要由 GeoEast 当前工区启动").arg(displayName));
        QMessageBox::information(
            this,
            uiText(QString::fromUtf8("请从 GeoEast 打开工区视图")),
            uiText(QString::fromUtf8("%1 需要 GeoEast 当前工区的会话信息，无法作为普通独立程序直接启动。%2\n\n请在 GeoEast 顶部工具栏点击【工区底图】或【平面成图】，再在图层树中选择当前网格和结果。插件内的结果视图区仍可直接查看两类图层。"))
                .arg(displayName, hostActivated ? QString::fromUtf8("已切换回 GeoEast 主窗口。")
                                                : QString::fromUtf8("请先打开并登录 GeoEast。")));
    });
    return true;
}

fpt_ui::TraceParameters FaultTraceDialog::currentTraceParameters() const {
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

QStringList FaultTraceDialog::traceArgs() const {
    QStringList args = commonConnectionArgs();
    QDir().mkpath(previewOutputDir());
    if (importDatCheck_->isChecked()) {
        args << "--import-geoeast-dat" << datFileEdit_->text().trimmed();
        if (overwriteGridCheck_->isChecked()) {
            args << "--overwrite-grid";
        }
    }
    args << "--grid" << gridCombo_->currentText().trimmed()
         << "--output" << outputEdit_->text().trimmed();
    args << fpt_ui::traceArguments(currentTraceParameters());
    args << "--polygon-color" << polygonColorEdit_->text()
         << "--polygon-linewidth" << QString::number(polygonLineWidthSpin_->value(), 'g', 12)
         << "--view-grid-output" << previewGridPath()
         << "--view-polygons-output" << previewPolygonsPath();
    return args;
}

void FaultTraceDialog::syncPresetIndicator() {
    if (!presetCombo_ || presetSyncGuard_) {
        return;
    }
    const QString key = comboKey(presetCombo_);
    if (key.isEmpty() || key == QString::fromUtf8("自定义参数")) {
        return;
    }
    // 质量模式属于运行方式而非预设内容，比对前对齐。
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

void FaultTraceDialog::applyParameterPreset(const QString& presetName) {
    // “自定义参数”是同步指示项，不对应任何共享预设，选中它时不改动参数。
    const bool applicable = !presetSyncGuard_ &&
        fpt_ui::canonical(presetName) != QString::fromUtf8("自定义参数");
    if (applicable) {
        presetSyncGuard_ = true;
        const int customIndex = presetCombo_ ? presetCombo_->findData(QString::fromUtf8("自定义参数")) : -1;
        if (customIndex >= 0 && presetCombo_->currentIndex() != customIndex) {
            presetCombo_->removeItem(customIndex);
        }
        presetSyncGuard_ = false;
        const fpt_ui::TraceParameters shared =
            fpt_ui::presetParameters(fpt_ui::canonical(presetName));
        applyTraceParameters(shared);
    }
}

void FaultTraceDialog::applyTraceParameters(const fpt_ui::TraceParameters& shared) {
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
    updateStatusContext();
}

void FaultTraceDialog::refreshGrids() {
    if (process_->state() != QProcess::NotRunning) {
        appendStatus(QString::fromUtf8("当前任务尚未完成，暂不能刷新网格列表"));
        return;
    }
    saveSettings();
    if (!ensureGeoEastConfigReady(QString::fromUtf8("刷新网格列表"), false, false)) {
        return;
    }
    const QString runner = runnerEdit_->text().trimmed();

    refreshingGrids_ = true;
    refreshingOutputs_ = false;
    exportingOutputView_ = false;
    exportingGridView_ = false;
    deletingOutput_ = false;
    logEdit_->clear();
    processOutput_.clear();
    QStringList args = commonConnectionArgs();
    args << "--list-grids";
    appendStatus(QString::fromUtf8("正在刷新 GeoEast 网格列表..."));
    if (renderViewWidget_) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("正在刷新 GeoEast 网格列表"));
    }
    appendLog("> " + runner + " " + quoteForLog(args));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    configureGeoEastEnvironment(environment);
    process_->setProcessEnvironment(environment);
    setRunning(true);
    process_->start(runner, args);
}

void FaultTraceDialog::refreshOutputs() {
    if (process_->state() != QProcess::NotRunning) {
        appendStatus(QString::fromUtf8("当前任务尚未完成，暂不能刷新结果列表"));
        return;
    }
    saveSettings();
    if (!ensureGeoEastConfigReady(QString::fromUtf8("刷新结果列表"), false, false)) {
        return;
    }
    const QString runner = runnerEdit_->text().trimmed();

    refreshingGrids_ = false;
    refreshingOutputs_ = true;
    exportingOutputView_ = false;
    exportingGridView_ = false;
    deletingOutput_ = false;
    logEdit_->clear();
    processOutput_.clear();
    QStringList args = commonConnectionArgs();
    args << "--list-fault-polygons";
    appendStatus(QString::fromUtf8("正在刷新 GeoEast 结果列表..."));
    if (renderViewWidget_) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("正在刷新 GeoEast 结果列表"));
    }
    appendLog("> " + runner + " " + quoteForLog(args));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    configureGeoEastEnvironment(environment);
    process_->setProcessEnvironment(environment);
    setRunning(true);
    process_->start(runner, args);
}

void FaultTraceDialog::loadSelectedGridView() {
    if (!gridCombo_ || gridCombo_->currentText().trimmed().isEmpty()) {
        return;
    }
    if (process_->state() != QProcess::NotRunning) {
        appendStatus(QString::fromUtf8("任务运行中，暂不能载入输入网格"));
        return;
    }
    saveSettings();
    if (!ensureGeoEastConfigReady(QString::fromUtf8("显示输入网格"), true, false)) {
        return;
    }

    const QString runner = runnerEdit_->text().trimmed();
    refreshingGrids_ = false;
    refreshingOutputs_ = false;
    exportingOutputView_ = false;
    exportingGridView_ = true;
    deletingOutput_ = false;
    QStringList args = commonConnectionArgs();
    args << "--grid" << gridCombo_->currentText().trimmed()
         << "--validate-grid"
         << "--view-grid-output" << previewGridPath();
    appendStatus(QString::fromUtf8("正在载入输入网格：%1").arg(gridCombo_->currentText().trimmed()));
    if (renderViewWidget_) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("正在载入输入网格"));
    }
    appendLog("> " + runner + " " + quoteForLog(args));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    configureGeoEastEnvironment(environment);
    process_->setProcessEnvironment(environment);
    setRunning(true);
    process_->start(runner, args);
}

void FaultTraceDialog::loadSelectedOutput() {
    loadSelectedOutputInternal(true);
}

void FaultTraceDialog::loadSelectedOutputInternal(bool userInitiated) {
    if (!outputCatalogCombo_) {
        return;
    }
    const QString selected = outputCatalogCombo_->currentData().toString();
    if (selected.trimmed().isEmpty()) {
        appendStatus(QString::fromUtf8("暂无可显示结果，请先运行追踪或刷新结果列表"));
        if (userInitiated) {
            QMessageBox::information(this,
                                     uiText(QString::fromUtf8("没有可选择的结果")),
                                     uiText(QString::fromUtf8("请先运行追踪，或点击【刷新结果列表】读取当前项目中的结果多边形对象。")));
        }
        return;
    }
    if (process_->state() != QProcess::NotRunning) {
        appendStatus(QString::fromUtf8("任务运行中，暂不能载入已有结果"));
        return;
    }
    saveSettings();
    if (!ensureGeoEastConfigReady(QString::fromUtf8("载入已有结果"), true, false)) {
        return;
    }
    outputEdit_->setText(selected);
    const QString runner = runnerEdit_->text().trimmed();
    refreshingGrids_ = false;
    refreshingOutputs_ = false;
    exportingOutputView_ = true;
    exportingGridView_ = false;
    deletingOutput_ = false;
    QStringList args = commonConnectionArgs();
    args << "--grid" << gridCombo_->currentText().trimmed()
         << "--output" << selected
         << "--validate-grid"
         << "--view-grid-output" << previewGridPath()
         << "--export-fault-polygons-output" << previewPolygonsPath();
    appendStatus(QString::fromUtf8("正在载入已有结果对象：%1").arg(selected));
    if (renderViewWidget_) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("正在导出并载入已有结果对象"));
    }
    appendLog("> " + runner + " " + quoteForLog(args));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    configureGeoEastEnvironment(environment);
    process_->setProcessEnvironment(environment);
    setRunning(true);
    process_->start(runner, args);
}

void FaultTraceDialog::runTrace() {
    if (doneBanner_) {
        doneBanner_->hide();
    }
    if (process_->state() != QProcess::NotRunning) {
        appendStatus(QString::fromUtf8("已有任务正在运行，请等待完成或先停止任务"));
        return;
    }
    saveSettings();
    if (!ensureGeoEastConfigReady(QString::fromUtf8("开始追踪"), true, true)) {
        return;
    }
    const QString runner = runnerEdit_->text().trimmed();

    refreshingGrids_ = false;
    refreshingOutputs_ = false;
    exportingOutputView_ = false;
    exportingGridView_ = false;
    deletingOutput_ = false;
    logEdit_->clear();
    QStringList args = traceArgs();
    appendStatus(QString::fromUtf8("正在追踪断层多边形..."));
    if (renderViewWidget_) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("正在追踪断层多边形"));
    }
    appendLog("> " + runner + " " + quoteForLog(args));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    configureGeoEastEnvironment(environment);
    process_->setProcessEnvironment(environment);
    setRunning(true);
    process_->start(runner, args);
}

void FaultTraceDialog::deleteOutput() {
    if (doneBanner_) {
        doneBanner_->hide();
    }
    if (process_->state() != QProcess::NotRunning) {
        appendStatus(QString::fromUtf8("已有任务正在运行，暂不能删除结果"));
        return;
    }
    const QString outputName = outputEdit_->text().trimmed();
    saveSettings();
    if (!ensureGeoEastConfigReady(QString::fromUtf8("删除当前结果"), false, true)) {
        return;
    }
    // V1.1：中文按钮 + 红色“删除”且默认焦点在“取消”，防误触回车。
    QMessageBox confirmBox(QMessageBox::Warning,
        uiText(QString::fromUtf8("确认删除 GeoEast 结果")),
        uiText(QString::fromUtf8("将删除当前项目中的结果多边形对象：\n\n%1\n\n该操作不会删除输入网格，也不会删除其它解释对象。是否继续？")).arg(outputName),
        QMessageBox::NoButton, this);
    QPushButton* deleteButton = confirmBox.addButton(uiText(QString::fromUtf8("删除")), QMessageBox::DestructiveRole);
    deleteButton->setStyleSheet("background:#D93025; color:#FFFFFF; border:1px solid #B42318; font-weight:600;");
    QPushButton* cancelButton = confirmBox.addButton(uiText(QString::fromUtf8("取消")), QMessageBox::RejectRole);
    confirmBox.setDefaultButton(cancelButton);
    confirmBox.exec();
    if (confirmBox.clickedButton() != deleteButton) {
        return;
    }
    const QString runner = runnerEdit_->text().trimmed();

    refreshingGrids_ = false;
    refreshingOutputs_ = false;
    exportingOutputView_ = false;
    exportingGridView_ = false;
    deletingOutput_ = true;
    logEdit_->clear();
    QStringList args = commonConnectionArgs();
    args << "--output" << outputName << "--delete-output";
    appendStatus(QString::fromUtf8("正在删除 GeoEast 结果对象..."));
    if (renderViewWidget_) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("正在删除 GeoEast 结果对象"));
    }
    appendLog("> " + runner + " " + quoteForLog(args));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    configureGeoEastEnvironment(environment);
    process_->setProcessEnvironment(environment);
    setRunning(true);
    process_->start(runner, args);
}

void FaultTraceDialog::loadResultView() {
    if (!renderViewWidget_) {
        return;
    }

    QString error;
    RenderViewWidget* view = static_cast<RenderViewWidget*>(renderViewWidget_);
    const QString gridPath = previewGridPath();
    const QString polygonPath = previewPolygonsPath();
    view->setActivity(QString::fromUtf8("正在构建结果视图"));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    const bool gridLoaded = view->loadGridFile(gridPath, &error);
    if (!gridLoaded) {
        appendLog(error);
    } else {
        appendLog(QString::fromUtf8("已载入原始网格视图：%1").arg(QDir::toNativeSeparators(gridPath)));
    }

    error.clear();
    const bool polygonsLoaded = view->loadPolygonCsv(polygonPath, &error);
    QApplication::restoreOverrideCursor();
    if (!polygonsLoaded) {
        appendLog(error);
    } else {
        appendLog(QString::fromUtf8("已载入结果多边形视图：%1").arg(QDir::toNativeSeparators(polygonPath)));
    }

    if (gridLoaded || polygonsLoaded) {
        view->setActivity(QString::fromUtf8("结果视图已载入"));
        appendStatus(QString::fromUtf8("结果视图已载入"));
    } else {
        view->setActivity(QString::fromUtf8("未找到结果视图文件，请先运行追踪"));
        appendStatus(QString::fromUtf8("未找到结果视图文件"));
    }
}

void FaultTraceDialog::exportResultViewPng() {
    if (!renderViewWidget_) {
        return;
    }
    RenderViewWidget* view = static_cast<RenderViewWidget*>(renderViewWidget_);
    if (!view->hasVisibleContent()) {
        QMessageBox::information(
            this,
            uiText(QString::fromUtf8("没有可导出的图层")),
            uiText(QString::fromUtf8("请先载入并显示原始网格或结果多边形图层，再导出当前视图 PNG。")));
        appendStatus(QString::fromUtf8("请先显示可导出的图层"));
        return;
    }
    QDir().mkpath(previewOutputDir());
    const QString fileName = QString("%1_%2_%3.png")
        .arg(sanitizeFileComponent(gridCombo_ ? gridCombo_->currentText() : QString(), "grid"),
             sanitizeFileComponent(outputEdit_ ? outputEdit_->text() : QString(), "output"),
             QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    const QString path = QDir(previewOutputDir()).absoluteFilePath(fileName);
    if (view->savePng(path)) {
        appendLog(QString::fromUtf8("结果视图 PNG 已导出：%1").arg(QDir::toNativeSeparators(path)));
        appendStatus(QString::fromUtf8("结果视图 PNG 已导出"));
    } else {
        appendLog(QString::fromUtf8("结果视图 PNG 导出失败：%1").arg(QDir::toNativeSeparators(path)));
        appendStatus(QString::fromUtf8("结果视图 PNG 导出失败"));
    }
}

void FaultTraceDialog::openGeoEastBasemap() {
    launchGeoEastApp("GeoBasemap.exe", QString::fromUtf8("GeoBasemap 工区底图"));
}

void FaultTraceDialog::openGeoEastMapping() {
    launchGeoEastApp("GeoMapping.exe", QString::fromUtf8("GeoMapping 平面成图"));
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
        processOutput_ += output;
        appendLog(output);
    }
    if (!error.isEmpty()) {
        processOutput_ += error;
        appendLog(error);
    }
}

void FaultTraceDialog::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    onReadyRead();
    setRunning(false);

    if (refreshingGrids_ && exitCode == 0 && status == QProcess::NormalExit) {
        const QString text = processOutput_;
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
        const QString current = pendingGridRestore_.isEmpty() ? gridCombo_->currentText() : pendingGridRestore_;
        pendingGridRestore_.clear();
        gridCombo_->blockSignals(true);
        gridCombo_->clear();
        if (!names.isEmpty()) {
            gridCombo_->addItems(names);
            int index = gridCombo_->findText(current);
            if (index >= 0) {
                gridCombo_->setCurrentIndex(index);
            }
        }
        gridCombo_->setProperty("fptCatalogReady", true);
        gridCombo_->blockSignals(false);
        appendStatus(QString::fromUtf8("网格对象刷新完成"));
        if (renderViewWidget_) {
            static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("网格对象刷新完成"));
        }
        refreshingGrids_ = false;
        if (!names.isEmpty() && (!showInputLayerCheck_ || showInputLayerCheck_->isChecked())) {
            QTimer::singleShot(0, this, &FaultTraceDialog::loadSelectedGridView);
        }
        if (refreshOutputsAfterGrids_) {
            refreshOutputsAfterGrids_ = false;
            QTimer::singleShot(0, this, &FaultTraceDialog::refreshOutputs);
        }
        return;
    }

    if (refreshingGrids_) {
        appendStatus(QString::fromUtf8("网格对象刷新失败，请检查数据库配置和日志"));
        gridCombo_->setProperty("fptCatalogReady", false);
        refreshingGrids_ = false;
        refreshOutputsAfterGrids_ = false;
        return;
    }

    if (refreshingOutputs_ && exitCode == 0 && status == QProcess::NormalExit) {
        const QString text = processOutput_;
        QStringList names;
        for (const QString& rawLine : text.split('\n')) {
            const QString line = rawLine.trimmed();
            if (line.isEmpty() || line.startsWith("BOFaultPolygon") || line.startsWith("Total:") ||
                line.startsWith("load ") || line.startsWith("Env ") || line.startsWith("GeoEast ") ||
                line.startsWith("Connecting ") || line.startsWith(">")) {
                continue;
            }
            if (!line.contains(' ') && !line.contains(':')) {
                names << line;
            }
        }
        if (outputCatalogCombo_) {
            const QString current = outputCatalogCombo_->currentData().toString().isEmpty()
                ? outputEdit_->text().trimmed()
                : outputCatalogCombo_->currentData().toString();
            outputCatalogCombo_->blockSignals(true);
            outputCatalogCombo_->clear();
            for (const QString& name : names) {
                outputCatalogCombo_->addItem(name, name);
            }
            if (names.isEmpty()) {
                outputCatalogCombo_->addItem(QString::fromUtf8("当前项目暂无结果；点击此处重新刷新"), QString());
                outputCatalogCombo_->setEnabled(true);
                if (loadSelectedOutputButton_) {
                    loadSelectedOutputButton_->setEnabled(false);
                }
            } else {
                outputCatalogCombo_->setEnabled(true);
                if (loadSelectedOutputButton_) {
                    loadSelectedOutputButton_->setEnabled(true);
                }
                const int index = outputCatalogCombo_->findData(current);
                if (index >= 0) {
                    outputCatalogCombo_->setCurrentIndex(index);
                }
            }
            outputCatalogCombo_->blockSignals(false);
            outputCatalogCombo_->setProperty("fptCatalogReady", true);
        }
        appendStatus(QString::fromUtf8("结果对象刷新完成，共 %1 个").arg(names.size()));
        if (renderViewWidget_) {
            static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("结果对象刷新完成"));
        }
        refreshingOutputs_ = false;
        if (!names.isEmpty() && outputCatalogCombo_ && !outputCatalogCombo_->currentData().toString().trimmed().isEmpty() &&
            (!showResultLayerCheck_ || showResultLayerCheck_->isChecked())) {
            QTimer::singleShot(0, this, [this]() {
                loadSelectedOutputInternal(false);
            });
        }
        return;
    }

    if (refreshingOutputs_) {
        appendStatus(QString::fromUtf8("结果对象刷新失败，请查看日志"));
        if (outputCatalogCombo_) {
            outputCatalogCombo_->setProperty("fptCatalogReady", false);
        }
        refreshingOutputs_ = false;
        return;
    }

    if (exportingGridView_) {
        if (exitCode == 0 && status == QProcess::NormalExit) {
            QString error;
            RenderViewWidget* view = static_cast<RenderViewWidget*>(renderViewWidget_);
            if (view && view->loadGridFile(previewGridPath(), &error)) {
                appendLog(QString::fromUtf8("已载入原始网格视图：%1").arg(QDir::toNativeSeparators(previewGridPath())));
                view->setActivity(QString::fromUtf8("原始网格已载入"));
                appendStatus(QString::fromUtf8("原始网格已载入"));
            } else {
                if (!error.isEmpty()) {
                    appendLog(error);
                }
                appendStatus(QString::fromUtf8("原始网格载入失败，请查看日志"));
            }
        } else {
            appendStatus(QString::fromUtf8("原始网格导出失败，请查看日志"));
        }
        exportingGridView_ = false;
        return;
    }

    if (exportingOutputView_) {
        if (exitCode == 0 && status == QProcess::NormalExit) {
            appendLog(QString::fromUtf8("GeoEast 历史结果已导出：BOFaultPolygon/%1").arg(outputEdit_->text().trimmed()));
            appendStatus(QString::fromUtf8("已有结果已载入"));
            loadResultView();
        } else {
            appendStatus(QString::fromUtf8("已有结果载入失败，请查看日志"));
            if (renderViewWidget_) {
                static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("已有结果载入失败"));
            }
        }
        exportingOutputView_ = false;
        return;
    }

    if (deletingOutput_) {
        if (exitCode == 0 && status == QProcess::NormalExit) {
            appendStatus(QString::fromUtf8("结果对象已删除或原本不存在"));
            if (renderViewWidget_) {
                static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("结果对象已删除或原本不存在"));
            }
            QTimer::singleShot(0, this, &FaultTraceDialog::refreshOutputs);
        } else {
            appendStatus(QString::fromUtf8("删除失败，请查看日志"));
            if (renderViewWidget_) {
                static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("删除失败，请查看日志"));
            }
        }
        deletingOutput_ = false;
        return;
    }

    if (exitCode == 0 && status == QProcess::NormalExit) {
        appendLog(QString::fromUtf8("GeoEast 数据库写入对象：BOFaultPolygon/%1").arg(outputEdit_->text().trimmed()));
        appendStatus(QString::fromUtf8("追踪完成，结果已写入 GeoEast 数据库"));
        if (renderViewWidget_) {
            static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("追踪完成，结果已写入 GeoEast 数据库"));
        }
        // 完成后自动打开两个图层，避免画布仍停在“图层显示已关闭”。
        if (showInputLayerCheck_ && showResultLayerCheck_) {
            QSignalBlocker blockInput(showInputLayerCheck_);
            QSignalBlocker blockResult(showResultLayerCheck_);
            showInputLayerCheck_->setChecked(true);
            showResultLayerCheck_->setChecked(true);
        }
        if (showInputLayerAction_) {
            showInputLayerAction_->setChecked(true);
        }
        if (showResultLayerAction_) {
            showResultLayerAction_->setChecked(true);
        }
        updateLayerVisibility();
        if (doneBanner_ && doneBannerLabel_) {
            doneBannerLabel_->setText(uiText(QString::fromUtf8("追踪完成：结果已写入 BOFaultPolygon/%1，可叠加检查或到工区底图复核"))
                                          .arg(outputEdit_ ? outputEdit_->text().trimmed() : QString()));
            doneBanner_->show();
        }
        loadResultView();
        QTimer::singleShot(0, this, &FaultTraceDialog::refreshOutputs);
    } else {
        appendStatus(QString::fromUtf8("运行失败，请查看日志"));
        if (renderViewWidget_) {
            static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("运行失败，请查看日志"));
        }
    }
    refreshingGrids_ = false;
    refreshingOutputs_ = false;
    exportingOutputView_ = false;
    exportingGridView_ = false;
    deletingOutput_ = false;
}

void FaultTraceDialog::onProcessError(QProcess::ProcessError error) {
    appendLog(QString::fromUtf8("进程错误：%1").arg(static_cast<int>(error)));
    appendStatus(QString::fromUtf8("进程启动或运行异常"));
    if (renderViewWidget_) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("进程启动或运行异常"));
    }
    setRunning(false);
}

void FaultTraceDialog::setRunning(bool running) {
    refreshButton_->setEnabled(!running);
    if (loadResultViewButton_) {
        loadResultViewButton_->setEnabled(!running);
    }
    if (exportViewButton_) {
        exportViewButton_->setEnabled(!running);
    }
    if (openBasemapButton_) {
        openBasemapButton_->setEnabled(!running);
    }
    if (openMappingButton_) {
        openMappingButton_->setEnabled(!running);
    }
    if (deleteOutputButton_) {
        deleteOutputButton_->setEnabled(!running);
    }
    if (refreshOutputsButton_) {
        refreshOutputsButton_->setEnabled(!running);
    }
    if (loadSelectedOutputButton_) {
        loadSelectedOutputButton_->setEnabled(!running && outputCatalogCombo_ && !outputCatalogCombo_->currentData().toString().isEmpty());
    }
    runButton_->setEnabled(!running);
    runButton_->setText(running ? uiText(QString::fromUtf8("追踪中…"))
                                : uiText(QString::fromUtf8("开始追踪")));
    stopButton_->setEnabled(running);
    if (busyBar_) {
        busyBar_->setVisible(running);
    }
    if (refreshGridAction_) {
        refreshGridAction_->setEnabled(!running);
    }
    if (deleteOutputAction_) {
        deleteOutputAction_->setEnabled(!running);
    }
    if (restoreDefaultsAction_) {
        restoreDefaultsAction_->setEnabled(!running);
    }
    if (loadResultViewAction_) {
        loadResultViewAction_->setEnabled(!running);
    }
    if (exportViewAction_) {
        exportViewAction_->setEnabled(!running);
    }
    if (openBasemapAction_) {
        openBasemapAction_->setEnabled(!running);
    }
    if (openMappingAction_) {
        openMappingAction_->setEnabled(!running);
    }
    if (runTraceAction_) {
        runTraceAction_->setEnabled(!running);
    }
    if (stopTraceAction_) {
        stopTraceAction_->setEnabled(running);
    }
    if (connectionLabel_) {
        connectionLabel_->setText(running ? uiText(QString::fromUtf8("任务运行中")) : uiText(QString::fromUtf8("数据库连接待验证")));
    }
    updateStatusContext();
}

void FaultTraceDialog::appendLog(const QString& text) {
    // 与独立软件共用行级着色规则；后台英文输出默认折叠到“详细日志”。
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

void FaultTraceDialog::appendStatus(const QString& text) {
    statusLabel_->setText(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " +
                          uiText(fpt_ui::canonical(text)));
    updateStatusContext();
}

void FaultTraceDialog::updateLayerVisibility() {
    const bool showInput = !showInputLayerCheck_ || showInputLayerCheck_->isChecked();
    const bool showResult = !showResultLayerCheck_ || showResultLayerCheck_->isChecked();

    if (showInputLayerAction_ && showInputLayerAction_->isChecked() != showInput) {
        showInputLayerAction_->setChecked(showInput);
    }
    if (showResultLayerAction_ && showResultLayerAction_->isChecked() != showResult) {
        showResultLayerAction_->setChecked(showResult);
    }
    if (renderViewWidget_) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setLayerVisibility(showInput, showResult);
    }
    if (process_ && process_->state() == QProcess::NotRunning) {
        if (showResult && outputCatalogCombo_ && !outputCatalogCombo_->currentData().toString().trimmed().isEmpty()) {
            QTimer::singleShot(0, this, [this]() {
                loadSelectedOutputInternal(false);
            });
        } else if (showInput && gridCombo_ && !gridCombo_->currentText().trimmed().isEmpty()) {
            QTimer::singleShot(0, this, &FaultTraceDialog::loadSelectedGridView);
        }
    }

    updateStatusContext();
}

void FaultTraceDialog::updateStatusContext() {
    // V1.1：长名称按 24 个字符省略，完整内容放 tooltip。
    const auto elide = [](const QString& text) {
        return text.size() > 24 ? text.left(23) + QChar(0x2026) : text;
    };
    if (projectStatusLabel_) {
        const QString project = projectEdit_ ? projectEdit_->text().trimmed() : QString();
        projectStatusLabel_->setText(uiText(QString::fromUtf8("项目: %1")).arg(elide(project)));
        projectStatusLabel_->setToolTip(project);
    }
    if (gridStatusLabel_) {
        const QString grid = gridCombo_ ? gridCombo_->currentText().trimmed() : QString();
        gridStatusLabel_->setText(uiText(QString::fromUtf8("网格: %1")).arg(elide(grid)));
        gridStatusLabel_->setToolTip(grid);
    }
    if (connectionLabel_ && process_ && process_->state() == QProcess::NotRunning) {
        const QString db = databaseEdit_ ? databaseEdit_->text().trimmed() : QString();
        connectionLabel_->setText(db.isEmpty()
            ? QString::fromUtf8("\u25CF ") + uiText(QString::fromUtf8("数据库未配置"))
            : QString::fromUtf8("\u25CF ") + uiText(QString::fromUtf8("数据库: %1")).arg(elide(db)));
        connectionLabel_->setStyleSheet(db.isEmpty() ? "color:#B54708;" : "color:#1A7F37;");
        connectionLabel_->setToolTip(db);
    }
    if (renderViewWidget_) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setContext(
            databaseEdit_ ? databaseEdit_->text().trimmed() : QString(),
            projectEdit_ ? projectEdit_->text().trimmed() : QString(),
            gridCombo_ ? gridCombo_->currentText().trimmed() : QString(),
            outputEdit_ ? outputEdit_->text().trimmed() : QString(),
            presetCombo_ ? comboKey(presetCombo_) : QString(),
            traceModeCombo_ ? traceModeCombo_->currentText() : QString(),
            thresholdModeCombo_ ? thresholdModeCombo_->currentText() : QString(),
            percentileSpin_ ? percentileSpin_->value() : 0.0,
            thresholdSpin_ ? thresholdSpin_->value() : 0.0,
            minAreaSpin_ ? minAreaSpin_->value() : 0.0,
            polygonColorEdit_ ? QColor(polygonColorEdit_->text()) : QColor("#00FFFF"),
            polygonLineWidthSpin_ ? polygonLineWidthSpin_->value() : 0.03,
            polygonOpacitySpin_ ? polygonOpacitySpin_->value() : 1.0);
        static_cast<RenderViewWidget*>(renderViewWidget_)->setLayerVisibility(
            !showInputLayerCheck_ || showInputLayerCheck_->isChecked(),
            !showResultLayerCheck_ || showResultLayerCheck_->isChecked());
    }
}

void FaultTraceDialog::closeEvent(QCloseEvent* event) {
    saveSettings();
    QDialog::closeEvent(event);
}
