#include "FaultTraceDialog.h"
#include "UiLocalization.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QMenu>
#include <QMenuBar>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTextCursor>
#include <QToolBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <functional>
#include <cmath>
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

bool isLegacyDeveloperDefault(const QString& value) {
    const QString normalized = QDir::fromNativeSeparators(value).toLower();
    return normalized.contains("baidunetdiskdownload/bgpcup2025") ||
           normalized.contains("documents/bgpcup/faultpolytrace/geoeast_plugin/bo_runner") ||
           normalized == "bgpcup2026_test7" ||
           normalized == "test7_fault_attribute_bo" ||
           normalized == "test7_fault_attribute";
}

QString settingOrCurrent(const QSettings& settings, const QString& key, const QString& current, int schemaVersion) {
    const QString value = settings.value(key, current).toString();
    return schemaVersion < 2 && isLegacyDeveloperDefault(value) ? current : value;
}

QString quoteForLog(const QStringList& args) {
    QStringList quoted;
    for (const QString& arg : args) {
        quoted << (arg.contains(' ') ? QString("\"%1\"").arg(arg) : arg);
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

enum class ToolIconKind {
    Open,
    Save,
    Pan,
    ZoomIn,
    ZoomOut,
    Database,
    Preset,
    Help,
    Reset,
    Run,
    Stop
};

QIcon makeToolIcon(ToolIconKind kind) {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor("#4A4A4A"), 1.4));

    const QColor blue("#1D7FC1");
    const QColor darkBlue("#005A9E");
    const QColor green("#2E8B57");
    const QColor yellow("#F4B844");
    const QColor red("#C94040");
    const QColor purple("#6A5ACD");

    switch (kind) {
    case ToolIconKind::Open:
        painter.setPen(QPen(QColor("#C58A1D"), 1.2));
        painter.setBrush(yellow);
        painter.drawRoundedRect(QRectF(3, 8, 18, 11), 2, 2);
        painter.setBrush(QColor("#FFD36A"));
        painter.drawRoundedRect(QRectF(3, 6, 8, 5), 1.5, 1.5);
        break;
    case ToolIconKind::Save:
        painter.setPen(QPen(darkBlue, 1.2));
        painter.setBrush(QColor("#3F8ED0"));
        painter.drawRoundedRect(QRectF(5, 4, 14, 16), 1.5, 1.5);
        painter.setBrush(QColor("#EAF4FF"));
        painter.drawRect(QRectF(8, 5.5, 7, 4.5));
        painter.drawRect(QRectF(8, 14, 8, 5));
        break;
    case ToolIconKind::Pan:
        painter.setPen(QPen(green, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(8, 12), QPointF(8, 7));
        painter.drawLine(QPointF(11, 12), QPointF(11, 5));
        painter.drawLine(QPointF(14, 13), QPointF(14, 7));
        painter.drawLine(QPointF(17, 14), QPointF(17, 9));
        painter.drawLine(QPointF(7, 13), QPointF(11, 19));
        painter.drawLine(QPointF(11, 19), QPointF(18, 18));
        painter.drawLine(QPointF(18, 18), QPointF(19, 13));
        break;
    case ToolIconKind::ZoomIn:
    case ToolIconKind::ZoomOut:
        painter.setPen(QPen(blue, 2.0));
        painter.setBrush(QColor("#EAF6FF"));
        painter.drawEllipse(QRectF(4, 4, 11, 11));
        painter.drawLine(QPointF(13, 13), QPointF(20, 20));
        painter.setPen(QPen(darkBlue, 1.7, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(7, 9.5), QPointF(12, 9.5));
        if (kind == ToolIconKind::ZoomIn) {
            painter.drawLine(QPointF(9.5, 7), QPointF(9.5, 12));
        }
        break;
    case ToolIconKind::Database:
        painter.setPen(QPen(darkBlue, 1.3));
        painter.setBrush(QColor("#DFF0FF"));
        painter.drawEllipse(QRectF(5, 4, 14, 5));
        painter.drawRect(QRectF(5, 6.5, 14, 11));
        painter.drawEllipse(QRectF(5, 15, 14, 5));
        painter.drawLine(QPointF(5, 9.5), QPointF(19, 9.5));
        painter.drawLine(QPointF(5, 13), QPointF(19, 13));
        break;
    case ToolIconKind::Preset:
        painter.setPen(QPen(purple, 1.4));
        painter.setBrush(QColor("#EEEAFE"));
        painter.drawRoundedRect(QRectF(5, 4, 14, 16), 2, 2);
        painter.setPen(QPen(purple, 1.6, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(8, 9), QPointF(16, 9));
        painter.drawLine(QPointF(8, 13), QPointF(16, 13));
        painter.setPen(QPen(green, 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(8, 17), QPointF(10.5, 19));
        painter.drawLine(QPointF(10.5, 19), QPointF(17, 12));
        break;
    case ToolIconKind::Help:
        painter.setPen(QPen(QColor("#DC143C"), 1.5));
        painter.setBrush(QColor("#FFF5F7"));
        painter.drawEllipse(QRectF(4, 4, 16, 16));
        painter.setPen(QPen(QColor("#DC143C"), 2.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawText(QRect(4, 4, 16, 16), Qt::AlignCenter, "?");
        break;
    case ToolIconKind::Reset:
        painter.setPen(QPen(blue, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawArc(QRectF(5, 5, 14, 14), 35 * 16, 285 * 16);
        painter.drawLine(QPointF(6, 8), QPointF(5, 4));
        painter.drawLine(QPointF(6, 8), QPointF(10, 7));
        break;
    case ToolIconKind::Run:
        painter.setPen(Qt::NoPen);
        painter.setBrush(blue);
        painter.drawPolygon(QPolygonF() << QPointF(7, 5) << QPointF(19, 12) << QPointF(7, 19));
        break;
    case ToolIconKind::Stop:
        painter.setPen(Qt::NoPen);
        painter.setBrush(red);
        painter.drawRect(QRectF(7, 7, 10, 10));
        break;
    }

    return QIcon(pixmap);
}

class RenderViewWidget : public QWidget {
public:
    explicit RenderViewWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(560, 420);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMouseTracking(true);
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
        panMode_ = false;
        dragging_ = false;
        zoom_ = 1.0;
        panOffset_ = QPointF(0, 0);
        setCursor(Qt::ArrowCursor);
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
                    double polygonLineWidth) {
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
        update();
        return true;
    }

    bool loadPolygonCsv(const QString& path, QString* error) {
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
        update();
        return !polygons_.isEmpty();
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
            painter.setFont(hintFont);
            painter.setPen(QColor("#666666"));
            const QString thresholdText = thresholdMode_ == "fixed"
                ? uiText(QString::fromUtf8("固定阈值 %1")).arg(threshold_, 0, 'f', 4)
                : uiText(QString::fromUtf8("分位数 %1")).arg(percentile_, 0, 'f', 4);
            const QStringList lines = {
                uiText(QString::fromUtf8("数据库：%1    项目：%2")).arg(database_.isEmpty() ? uiText(QString::fromUtf8("未配置")) : database_,
                                                            project_.isEmpty() ? uiText(QString::fromUtf8("未配置")) : project_),
                uiText(QString::fromUtf8("输入网格：%1")).arg(gridName_.isEmpty() ? uiText(QString::fromUtf8("未选择")) : gridName_),
                uiText(QString::fromUtf8("输出多边形：%1")).arg(output_.isEmpty() ? uiText(QString::fromUtf8("未命名")) : output_),
                uiText(QString::fromUtf8("方案：%1    模式：%2    阈值：%3")).arg(uiText(preset_), traceMode_, thresholdText),
                uiText(QString::fromUtf8("面积过滤：小于 %1 的多边形不输出    结果线宽：%2")).arg(minArea_, 0, 'f', 2).arg(polygonLineWidth_, 0, 'f', 3),
                uiText(QString::fromUtf8("结果查看：追踪完成后自动显示原始网格和结果多边形；如需复查，可在左侧重新载入结果视图。")),
                uiText(QString::fromUtf8("官方查看：点击【打开工区底图】或【打开平面成图】，在 GeoEast 图层树中加载 Grids 和 Fault Polygons。")),
                uiText(QString::fromUtf8("当前状态：%1")).arg(activity_.isEmpty() ? uiText(QString::fromUtf8("等待运行")) : uiText(activity_))
            };
            painter.drawText(rect().adjusted(26, 62, -26, -26), Qt::AlignLeft | Qt::AlignTop, lines.join('\n'));
        }

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
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else if (event->angleDelta().y() < 0) {
            zoomOut();
        }
        QWidget::wheelEvent(event);
    }

    void leaveEvent(QEvent* event) override {
        if (coordinateCallback_) {
            coordinateCallback_(QPointF(-1.0, -1.0));
        }
        QWidget::leaveEvent(event);
    }

private:
    QColor heatColor(double value) const {
        const double t = qBound(0.0, (value - valueMin_) / (valueMax_ - valueMin_), 1.0);
        if (t < 0.25) {
            return QColor::fromRgbF(1.0, t / 0.25, 0.0);
        }
        if (t < 0.50) {
            return QColor::fromRgbF(1.0 - (t - 0.25) / 0.25, 1.0, 0.0);
        }
        if (t < 0.75) {
            return QColor::fromRgbF(0.0, 1.0, (t - 0.50) / 0.25);
        }
        return QColor::fromRgbF(0.0, 1.0 - (t - 0.75) / 0.25, 1.0);
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
        QImage image(nx_, ny_, QImage::Format_RGB32);
        for (int y = 0; y < ny_; ++y) {
            for (int x = 0; x < nx_; ++x) {
                image.setPixelColor(x, ny_ - 1 - y, heatColor(grid_[y * nx_ + x]));
            }
        }
        const QRectF target = fittedRect(available);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(target, image);
        painter.setPen(QColor("#222222"));
        painter.drawRect(target);
    }

    void drawPolygons(QPainter& painter, const QRectF& available) const {
        QColor color = polygonColor_;
        color.setAlphaF(1.0);
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
    bool panMode_ = false;
    bool dragging_ = false;
    bool showInputLayer_ = true;
    bool showResultLayer_ = true;
    QPointF panOffset_;
    QPointF lastDragPos_;
    QVector<double> grid_;
    QVector<QPolygonF> polygons_;
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
    std::function<void(const QPointF&)> coordinateCallback_;
};

class ColorBarWidget : public QWidget {
public:
    explicit ColorBarWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedWidth(56);
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

        const QRect barRect(20, 34, 18, qMax(120, height() - 88));
        painter.setPen(QColor("#555555"));
        QFont labelFont = painter.font();
        labelFont.setPointSize(8);
        painter.setFont(labelFont);
        painter.drawText(
            QRect(4, 8, 48, 28),
            Qt::AlignCenter | Qt::TextWordWrap,
            fpt_ui::translate(QString::fromUtf8("属性"), languageCode_) + "\n1.0");

        QLinearGradient gradient(barRect.topLeft(), barRect.bottomLeft());
        gradient.setColorAt(0.00, QColor("#0B35FF"));
        gradient.setColorAt(0.25, QColor("#00D7FF"));
        gradient.setColorAt(0.50, QColor("#00E84A"));
        gradient.setColorAt(0.75, QColor("#FFE600"));
        gradient.setColorAt(1.00, QColor("#FF0000"));
        painter.fillRect(barRect, gradient);
        painter.setPen(QColor("#333333"));
        painter.drawRect(barRect.adjusted(0, 0, -1, -1));

        painter.setPen(QColor("#DC143C"));
        painter.drawText(QRect(4, barRect.bottom() + 6, 48, 16), Qt::AlignCenter, "0.0");
    }

private:
    QString languageCode_ = "zh-CN";
};

QLabel* makeFormLabel(const QString& text, QWidget* parent) {
    QLabel* label = new QLabel(text, parent);
    label->setObjectName("formLabel");
    label->setMinimumWidth(112);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return label;
}

void tuneControl(QWidget* widget) {
    widget->setMinimumHeight(22);
    widget->setMaximumHeight(26);
}

QGridLayout* makeGroupLayout(QGroupBox* box) {
    QGridLayout* layout = new QGridLayout(box);
    layout->setContentsMargins(12, 12, 12, 10);
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
}

void FaultTraceDialog::buildUi() {
    setWindowTitle(QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪"));
    setWindowFlags((windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint) & ~Qt::WindowContextHelpButtonHint);
    resize(980, 720);

    runnerEdit_ = new QLineEdit(defaultRunnerPath(), this);
    QPushButton* browseRunnerButton = new QPushButton(QString::fromUtf8("浏览"), this);
    connect(browseRunnerButton, &QPushButton::clicked, this, &FaultTraceDialog::browseRunner);

    const QString detectedGeoEastRoot = detectGeoEastRoot();
    geoEastRootEdit_ = new QLineEdit(detectedGeoEastRoot, this);
    geoEastRootEdit_->setPlaceholderText(QString::fromUtf8("自动识别失败时请手动选择 GeoEast 安装根目录"));
    geoEastDataRootEdit_ = new QLineEdit(detectGeoEastDataRoot(detectedGeoEastRoot), this);
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
    const QString defaultGrid = envValue("FPT_GEOEAST_GRID");
    if (!defaultGrid.isEmpty()) {
        gridCombo_->addItem(defaultGrid);
        gridCombo_->setCurrentText(defaultGrid);
    }
    if (gridCombo_->lineEdit()) {
        gridCombo_->lineEdit()->setPlaceholderText(QString::fromUtf8("请先配置项目并刷新网格"));
    }
    guidedGridCombo->setBeforePopupHandler([this]() {
        if ((gridCombo_ && gridCombo_->count() > 0) ||
            (gridCombo_ && !gridCombo_->currentText().trimmed().isEmpty())) {
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
    polygonColorEdit_->setReadOnly(true);
    polygonColorButton_ = new QPushButton(QString::fromUtf8("选择颜色"), this);
    polygonLineWidthSpin_ = new QDoubleSpinBox(this);
    polygonLineWidthSpin_->setRange(0.01, 5.0);
    polygonLineWidthSpin_->setDecimals(2);
    polygonLineWidthSpin_->setSingleStep(0.005);
    polygonLineWidthSpin_->setValue(0.025);
    connect(polygonColorButton_, &QPushButton::clicked, this, &FaultTraceDialog::choosePolygonColor);
    setPolygonColor(QColor(polygonColorEdit_->text()));
    refreshButton_ = new QPushButton(QString::fromUtf8("刷新网格列表"), this);
    connect(refreshButton_, &QPushButton::clicked, this, &FaultTraceDialog::refreshGrids);

    importDatCheck_ = new QCheckBox(QString::fromUtf8("从 DAT 导入/覆盖输入网格"), this);
    importDatCheck_->setChecked(false);
    datFileEdit_ = new QLineEdit(defaultOfficialDatPath(), this);
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
    dataLayout->addRow(QString::fromUtf8("结果样式"), styleLayout);
    dataLayout->addRow(importDatCheck_);
    dataLayout->addRow(QString::fromUtf8("DAT 文件"), datLayout);
    dataLayout->addRow(overwriteGridCheck_);

    QGroupBox* paramsBox = new QGroupBox(QString::fromUtf8("追踪参数"), this);
    presetCombo_ = new QComboBox(this);
    presetCombo_->addItem(QString::fromUtf8("全覆盖优先"));
    presetCombo_->addItem(QString::fromUtf8("贴合融合推荐"));
    presetCombo_->addItem(QString::fromUtf8("防黏连增强"));
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

    {
        setWindowTitle(QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪"));
        setMinimumSize(1120, 700);
        resize(1320, 780);

        connectionBox->hide();
        dataBox->hide();
        paramsBox->hide();

        polygonColorButton_->setText("v");
        polygonColorButton_->setToolTip(QString::fromUtf8("选择结果多边形颜色"));
        polygonColorButton_->setFixedWidth(24);
        polygonLineWidthSpin_->setFixedWidth(64);
        browseDatButton->setText(QString::fromUtf8("浏览"));
        browseDatButton->setFixedWidth(52);
        refreshButton_->setText(QString::fromUtf8("刷新网格列表"));
        refreshButton_->setToolTip(QString::fromUtf8("从 GeoEast 数据库读取可用输入网格列表"));
        refreshButton_->setIcon(makeToolIcon(ToolIconKind::Reset));
        refreshButton_->setIconSize(QSize(18, 18));

        setStyleSheet(QString::fromUtf8(R"(
            QDialog {
                background: #E6E7E8;
                color: #222222;
                font-family: "Microsoft YaHei", "Segoe UI", Tahoma, sans-serif;
                font-size: 12px;
            }
            QMenuBar {
                background: #F0F0F0;
                border-bottom: 1px solid #D0D0D0;
                padding: 2px 8px;
            }
            QMenuBar::item {
                padding: 4px 8px;
                background: transparent;
                border-radius: 2px;
            }
            QMenuBar::item:selected {
                background: #CCE8FF;
                border: 1px solid #99D1FF;
            }
            QToolBar {
                background: #F0F0F0;
                border: 0;
                border-bottom: 1px solid #A0A0A0;
                spacing: 4px;
                padding: 4px 10px;
            }
            QToolButton {
                min-width: 26px;
                min-height: 24px;
                border: 1px solid transparent;
                border-radius: 3px;
                padding: 2px 6px;
            }
            QToolButton:hover {
                background: #CCE8FF;
                border-color: #99D1FF;
            }
            QFrame#sidebar {
                background: #F0F0F0;
                border-right: 1px solid #999999;
            }
            QSplitter::handle {
                background: #C6C6C6;
                width: 5px;
            }
            QSplitter::handle:hover {
                background: #6AA9D8;
            }
            QFrame#sidebarHeader {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FAFAFA, stop:1 #E4E4E4);
                border-bottom: 1px solid #A0A0A0;
            }
            QLabel#sidebarTitle {
                font-weight: bold;
                color: #111111;
            }
            QScrollArea#sidebarScroll {
                border: 0;
                background: #F0F0F0;
            }
            QWidget#sidebarContent {
                background: #F0F0F0;
            }
            QGroupBox {
                background: #F0F0F0;
                border: 1px solid #B5B5B5;
                border-radius: 3px;
                margin-top: 9px;
                padding-top: 5px;
                font-weight: normal;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 8px;
                padding: 0 5px;
                color: #005A9E;
                font-weight: bold;
            }
            QLabel#formLabel {
                color: #222222;
            }
            QLabel#displayHint {
                color: #555555;
                background: #F7F7F7;
                border: 1px solid #D0D0D0;
                border-radius: 2px;
                padding: 5px 6px;
                line-height: 140%;
            }
            QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
                background: #FFFFFF;
                border: 1px solid #A0A0A0;
                border-radius: 2px;
                min-height: 22px;
                padding: 1px 5px;
                selection-background-color: #0078D7;
            }
            QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
                border-color: #0078D7;
            }
            QPushButton {
                background: #E1E1E1;
                border: 1px solid #A0A0A0;
                border-radius: 2px;
                min-height: 22px;
                padding: 2px 10px;
                color: #222222;
            }
            QPushButton:hover {
                background: #E5F1FB;
                border-color: #0078D7;
            }
            QPushButton#dangerButton {
                color: #7A1F1F;
            }
            QPushButton#dangerButton:hover {
                background: #FDECEC;
                border-color: #C94040;
            }
            QPushButton#runButton {
                background: #0078D7;
                border-color: #005A9E;
                color: white;
                font-weight: bold;
                min-height: 28px;
            }
            QPushButton#runButton:hover {
                background: #106EBE;
            }
            QPushButton#stopButton {
                background: #E1E1E1;
                color: #333333;
                font-weight: bold;
                min-height: 28px;
            }
            QPushButton#stopButton:disabled, QPushButton#runButton:disabled {
                background: #D8D8D8;
                color: #777777;
                border-color: #B8B8B8;
            }
            QFrame#actionDock {
                background: #E8E8E8;
                border-top: 1px solid #A0A0A0;
            }
            QFrame#workspace {
                background: #FFFFFF;
            }
            QFrame#logDock {
                background: #F8F9FA;
                border-top: 1px solid #A0A0A0;
            }
            QLabel#logHeader {
                background: #EAEAEA;
                border-bottom: 1px solid #C0C0C0;
                color: #444444;
                font-weight: bold;
                padding: 4px 10px;
            }
            QTextEdit#logEdit {
                background: #F8F9FA;
                border: 0;
                color: #333333;
                font-family: Consolas, "Courier New", monospace;
                font-size: 12px;
            }
            QFrame#statusBar {
                background: #F0F0F0;
                border-top: 1px solid #A0A0A0;
            }
            QLabel#statusText {
                color: #555555;
                font-size: 11px;
            }
        )"));

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
        QAction* configAction = geoEastMenu->addAction(makeToolIcon(ToolIconKind::Database), QString::fromUtf8("数据库配置"));
        configAction->setToolTip(QString::fromUtf8("配置 GeoEast 根目录、数据根目录、数据库、项目、用户和执行程序"));
        connect(configAction, &QAction::triggered, this, &FaultTraceDialog::showDatabaseConfigDialog);
        refreshGridAction_ = geoEastMenu->addAction(makeToolIcon(ToolIconKind::Database), QString::fromUtf8("刷新网格"));
        refreshGridAction_->setToolTip(QString::fromUtf8("刷新 GeoEast 当前项目下的输入网格列表"));
        connect(refreshGridAction_, &QAction::triggered, this, &FaultTraceDialog::refreshGrids);
        deleteOutputAction_ = geoEastMenu->addAction(QString::fromUtf8("删除当前结果"));
        connect(deleteOutputAction_, &QAction::triggered, this, &FaultTraceDialog::deleteOutput);
        geoEastMenu->addSeparator();
        openBasemapAction_ = geoEastMenu->addAction(QString::fromUtf8("打开工区底图"));
        connect(openBasemapAction_, &QAction::triggered, this, &FaultTraceDialog::openGeoEastBasemap);
        openMappingAction_ = geoEastMenu->addAction(QString::fromUtf8("打开平面成图"));
        connect(openMappingAction_, &QAction::triggered, this, &FaultTraceDialog::openGeoEastMapping);
        geoEastMenu->addSeparator();
        QAction* exitAction = geoEastMenu->addAction(QString::fromUtf8("关闭启动器"));
        connect(exitAction, &QAction::triggered, this, &FaultTraceDialog::close);

        QMenu* fileMenu = menuBar->addMenu(QString::fromUtf8("文件(F)"));
        QAction* openAction = fileMenu->addAction(makeToolIcon(ToolIconKind::Open), QString::fromUtf8("打开 DAT 文件..."));
        openAction->setText(QString::fromUtf8("打开DAT"));
        openAction->setToolTip(QString::fromUtf8("打开或选择 DAT 文件"));
        connect(openAction, &QAction::triggered, this, &FaultTraceDialog::browseDatFile);
        QAction* saveAction = fileMenu->addAction(makeToolIcon(ToolIconKind::Save), QString::fromUtf8("保存当前配置"));
        saveAction->setText(QString::fromUtf8("保存配置"));
        saveAction->setToolTip(QString::fromUtf8("保存当前界面配置和参数"));
        connect(saveAction, &QAction::triggered, this, &FaultTraceDialog::saveCurrentPreset);
        QAction* outputDirAction = fileMenu->addAction(QString::fromUtf8("打开输出目录"));
        connect(outputDirAction, &QAction::triggered, this, [openLocalDir]() { openLocalDir("examples/outputs"); });
        fileMenu->addSeparator();
        fileMenu->addAction(exitAction);

        QMenu* editMenu = menuBar->addMenu(QString::fromUtf8("编辑(E)"));
        QAction* copyLogAction = editMenu->addAction(QString::fromUtf8("复制日志"));
        connect(copyLogAction, &QAction::triggered, this, [this]() {
            QApplication::clipboard()->setText(logEdit_ ? logEdit_->toPlainText() : QString());
            appendLog(QString::fromUtf8("日志已复制到剪贴板。"));
        });
        QAction* clearLogAction = editMenu->addAction(QString::fromUtf8("清空日志"));
        connect(clearLogAction, &QAction::triggered, this, [this]() {
            if (logEdit_) {
                logEdit_->clear();
            }
            appendStatus(QString::fromUtf8("日志已清空"));
        });

        QMenu* viewMenu = menuBar->addMenu(QString::fromUtf8("视图(V)"));
        QAction* panAction = viewMenu->addAction(makeToolIcon(ToolIconKind::Pan), QString::fromUtf8("平移模式"));
        panAction->setText(QString::fromUtf8("平移"));
        panAction->setCheckable(true);
        connect(panAction, &QAction::toggled, this, [this, renderView](bool checked) {
            if (renderView()) {
                renderView()->setPanMode(checked);
            }
            appendStatus(checked ? QString::fromUtf8("视图区已切换为平移模式") : QString::fromUtf8("视图区已切换为选择模式"));
        });
        QAction* zoomInAction = viewMenu->addAction(makeToolIcon(ToolIconKind::ZoomIn), QString::fromUtf8("放大"));
        connect(zoomInAction, &QAction::triggered, this, [this, renderView]() {
            if (renderView()) {
                renderView()->zoomIn();
            }
            appendStatus(QString::fromUtf8("视图区已放大"));
        });
        QAction* zoomOutAction = viewMenu->addAction(makeToolIcon(ToolIconKind::ZoomOut), QString::fromUtf8("缩小"));
        connect(zoomOutAction, &QAction::triggered, this, [this, renderView]() {
            if (renderView()) {
                renderView()->zoomOut();
            }
            appendStatus(QString::fromUtf8("视图区已缩小"));
        });
        QAction* resetViewAction = viewMenu->addAction(makeToolIcon(ToolIconKind::Reset), QString::fromUtf8("重置视图"));
        resetViewAction->setText(QString::fromUtf8("重置"));
        connect(resetViewAction, &QAction::triggered, this, [this, renderView, panAction]() {
            if (renderView()) {
                renderView()->resetView();
            }
            panAction->setChecked(false);
            appendStatus(QString::fromUtf8("视图区已重置"));
        });
        viewMenu->addSeparator();
        showInputLayerAction_ = viewMenu->addAction(QString::fromUtf8("显示原始网格层"));
        showInputLayerAction_->setCheckable(true);
        showInputLayerAction_->setChecked(true);
        connect(showInputLayerAction_, &QAction::toggled, this, [this](bool checked) {
            if (showInputLayerCheck_ && showInputLayerCheck_->isChecked() != checked) {
                showInputLayerCheck_->setChecked(checked);
                return;
            }
            updateLayerVisibility();
        });
        showResultLayerAction_ = viewMenu->addAction(QString::fromUtf8("显示结果多边形层"));
        showResultLayerAction_->setCheckable(true);
        showResultLayerAction_->setChecked(true);
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
        exportViewAction_ = viewMenu->addAction(QString::fromUtf8("导出当前视图 PNG"));
        connect(exportViewAction_, &QAction::triggered, this, &FaultTraceDialog::exportResultViewPng);

        QMenu* faultMenu = menuBar->addMenu(QString::fromUtf8("断层(L)"));
        runTraceAction_ = faultMenu->addAction(makeToolIcon(ToolIconKind::Run), QString::fromUtf8("开始追踪"));
        connect(runTraceAction_, &QAction::triggered, this, &FaultTraceDialog::runTrace);
        stopTraceAction_ = faultMenu->addAction(makeToolIcon(ToolIconKind::Stop), QString::fromUtf8("停止追踪"));
        stopTraceAction_->setEnabled(false);
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
        QAction* savePresetAction = toolMenu->addAction(makeToolIcon(ToolIconKind::Preset), QString::fromUtf8("保存当前参数"));
        connect(savePresetAction, &QAction::triggered, this, &FaultTraceDialog::saveCurrentPreset);
        restoreDefaultsAction_ = toolMenu->addAction(makeToolIcon(ToolIconKind::Reset), QString::fromUtf8("恢复默认配置"));
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
        QAction* helpAction = helpMenu->addAction(makeToolIcon(ToolIconKind::Help), QString::fromUtf8("帮助提示"));
        connect(helpAction, &QAction::triggered, this, &FaultTraceDialog::showHelpHint);
        QAction* manualAction = helpMenu->addAction(QString::fromUtf8("用户操作手册"));
        connect(manualAction, &QAction::triggered, this, [openLocalFile]() { openLocalFile("docs/用户操作手册.md"); });
        QAction* projectStructureAction = helpMenu->addAction(QString::fromUtf8("项目目录结构"));
        connect(projectStructureAction, &QAction::triggered, this, [openLocalFile]() { openLocalFile("docs/项目目录结构与文件说明.md"); });
        QAction* aboutAction = helpMenu->addAction(QString::fromUtf8("关于 FaultPolyTrace"));
        connect(aboutAction, &QAction::triggered, this, [this]() {
            QMessageBox::information(this, QString::fromUtf8("关于 FaultPolyTrace"),
                                     QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪\n\n"
                                                       "定位：平面断层属性到闭合断层多边形的自动追踪工具。\n"
                                                       "入口：可独立运行，也可通过 GeoEast 菜单调用执行程序。"));
        });

        QToolBar* toolbar = new QToolBar(this);
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
        toolbar->addAction(configAction);
        toolbar->addAction(refreshGridAction_);
        toolbar->addAction(restoreDefaultsAction_);
        toolbar->addSeparator();
        QWidget* toolbarSpacer = new QWidget(this);
        toolbarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        toolbar->addWidget(toolbarSpacer);

        QFrame* sidebar = new QFrame(this);
        sidebar->setObjectName("sidebar");
        sidebar->setMinimumWidth(330);
        sidebar->setMaximumWidth(620);
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
        QWidget* colorLineWidget = new QWidget(dataGroup);
        QHBoxLayout* colorLineLayout = new QHBoxLayout(colorLineWidget);
        colorLineLayout->setContentsMargins(0, 0, 0, 0);
        colorLineLayout->setSpacing(5);
        colorLineLayout->addWidget(polygonColorEdit_, 1);
        colorLineLayout->addWidget(polygonColorButton_);
        colorLineLayout->addWidget(polygonLineWidthSpin_);
        addFormRow(dataLayoutNew, 2, QString::fromUtf8("结果样式 / 线宽"), colorLineWidget, dataGroup);
        QWidget* datLineWidget = new QWidget(dataGroup);
        QHBoxLayout* datLineLayout = new QHBoxLayout(datLineWidget);
        datLineLayout->setContentsMargins(0, 0, 0, 0);
        datLineLayout->setSpacing(5);
        datLineLayout->addWidget(datFileEdit_, 1);
        datLineLayout->addWidget(browseDatButton);
        addFormRow(dataLayoutNew, 3, QString::fromUtf8("DAT 文件"), datLineWidget, dataGroup);
        dataLayoutNew->addWidget(overwriteGridCheck_, 4, 0, 1, 2);
        dataLayoutNew->addWidget(importDatCheck_, 5, 0, 1, 2);
        dataLayoutNew->addWidget(refreshButton_, 6, 0, 1, 2);
        sidebarContentLayout->addWidget(dataGroup);

        QGroupBox* resultGroup = new QGroupBox(QString::fromUtf8("结果查看"), sidebarContent);
        QGridLayout* resultLayout = makeGroupLayout(resultGroup);
        showInputLayerCheck_ = new QCheckBox(QString::fromUtf8("显示原始网格层"), resultGroup);
        showInputLayerCheck_->setToolTip(QString::fromUtf8("控制结果视图区是否显示本次追踪生成的原始网格图层。"));
        showInputLayerCheck_->setChecked(true);
        showResultLayerCheck_ = new QCheckBox(QString::fromUtf8("显示结果多边形层"), resultGroup);
        showResultLayerCheck_->setToolTip(QString::fromUtf8("控制结果视图区是否显示本次追踪生成的结果多边形图层。"));
        showResultLayerCheck_->setChecked(true);
        resultLayout->addWidget(showInputLayerCheck_, 0, 0);
        resultLayout->addWidget(showResultLayerCheck_, 0, 1);
        auto* guidedOutputCombo = new GuidedCatalogComboBox(resultGroup);
        outputCatalogCombo_ = guidedOutputCombo;
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
            if (hasAvailableResult) {
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
        refreshOutputsButton_ = new QPushButton(QString::fromUtf8("刷新结果"), resultGroup);
        refreshOutputsButton_->setToolTip(QString::fromUtf8("刷新当前项目中的 BOFaultPolygon 结果列表。"));
        loadSelectedOutputButton_ = new QPushButton(QString::fromUtf8("选为当前"), resultGroup);
        loadSelectedOutputButton_->setToolTip(QString::fromUtf8("把选中的已有结果多边形对象设为当前输出名称。"));
        resultLayout->addWidget(outputCatalogCombo_, 1, 0, 1, 2);
        resultLayout->addWidget(refreshOutputsButton_, 2, 0);
        resultLayout->addWidget(loadSelectedOutputButton_, 2, 1);
        loadResultViewButton_ = new QPushButton(QString::fromUtf8("重新载入"), resultGroup);
        loadResultViewButton_->setToolTip(QString::fromUtf8("重新读取本次计算生成的原始网格和结果多边形文件。"));
        exportViewButton_ = new QPushButton(QString::fromUtf8("导出PNG"), resultGroup);
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
        QLabel* displayHint = new QLabel(QString::fromUtf8("提示：本窗口显示本次计算结果；官方复核请在工区底图或平面成图中勾选网格和断层多边形图层。"), resultGroup);
        displayHint->setWordWrap(true);
        displayHint->setObjectName("displayHint");
        resultLayout->addWidget(displayHint, 6, 0, 1, 2);
        connect(showInputLayerCheck_, &QCheckBox::toggled, this, [this](bool checked) {
            if (showInputLayerAction_ && showInputLayerAction_->isChecked() != checked) {
                showInputLayerAction_->setChecked(checked);
                return;
            }
            updateLayerVisibility();
        });
        connect(showResultLayerCheck_, &QCheckBox::toggled, this, [this](bool checked) {
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
        connect(loadSelectedOutputButton_, &QPushButton::clicked, this, &FaultTraceDialog::loadSelectedOutput);
        connect(loadResultViewButton_, &QPushButton::clicked, this, &FaultTraceDialog::loadResultView);
        connect(exportViewButton_, &QPushButton::clicked, this, &FaultTraceDialog::exportResultViewPng);
        connect(openBasemapButton_, &QPushButton::clicked, this, &FaultTraceDialog::openGeoEastBasemap);
        connect(openMappingButton_, &QPushButton::clicked, this, &FaultTraceDialog::openGeoEastMapping);
        connect(deleteOutputButton_, &QPushButton::clicked, this, &FaultTraceDialog::deleteOutput);
        sidebarContentLayout->addWidget(resultGroup);

        QGroupBox* traceGroup = new QGroupBox(QString::fromUtf8("追踪参数"), sidebarContent);
        QGridLayout* traceLayoutNew = makeGroupLayout(traceGroup);
        addFormRow(traceLayoutNew, 0, QString::fromUtf8("预设模板"), presetCombo_, traceGroup);
        addFormRow(traceLayoutNew, 1, QString::fromUtf8("追踪模式"), traceModeCombo_, traceGroup);
        addFormRow(traceLayoutNew, 2, QString::fromUtf8("阈值模式"), thresholdModeCombo_, traceGroup);
        addFormRow(traceLayoutNew, 3, QString::fromUtf8("分位数"), percentileSpin_, traceGroup);
        addFormRow(traceLayoutNew, 4, QString::fromUtf8("固定阈值"), thresholdSpin_, traceGroup);
        addFormRow(traceLayoutNew, 5, QString::fromUtf8("平滑次数"), smoothIterationsSpin_, traceGroup);
        sidebarContentLayout->addWidget(traceGroup);

        QGroupBox* morphologyGroup = new QGroupBox(QString::fromUtf8("形态与拓扑约束"), sidebarContent);
        QGridLayout* morphologyLayoutNew = makeGroupLayout(morphologyGroup);
        addFormRow(morphologyLayoutNew, 0, QString::fromUtf8("闭运算半径"), closeRadiusSpin_, morphologyGroup);
        addFormRow(morphologyLayoutNew, 1, QString::fromUtf8("最大断距"), maxGapDistanceSpin_, morphologyGroup);
        addFormRow(morphologyLayoutNew, 2, QString::fromUtf8("最小面积"), minAreaSpin_, morphologyGroup);
        addFormRow(morphologyLayoutNew, 3, QString::fromUtf8("最大夹角"), maxGapAngleSpin_, morphologyGroup);
        addFormRow(morphologyLayoutNew, 4, QString::fromUtf8("防黏连支撑"), rootAntiMergeSupportRatioSpin_, morphologyGroup);
        addFormRow(morphologyLayoutNew, 5, QString::fromUtf8("简化容差"), simplifyToleranceSpin_, morphologyGroup);
        morphologyLayoutNew->addWidget(attributeConstrainedBeltCheck_, 6, 0);
        morphologyLayoutNew->addWidget(attributeEnvelopeRadiusSpin_, 6, 1);
        morphologyLayoutNew->addWidget(rootAntiMergeCheck_, 7, 0);
        morphologyLayoutNew->addWidget(rootAntiMergeRadiusSpin_, 7, 1);
        sidebarContentLayout->addWidget(morphologyGroup);

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
        sidebarContentLayout->addWidget(detailsGroup);

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
        sidebarContentLayout->addWidget(expertGroup);
        sidebarContentLayout->addStretch(1);

        sidebarScroll->setWidget(sidebarContent);
        sidebarRoot->addWidget(sidebarScroll, 1);

        QFrame* actionDock = new QFrame(sidebar);
        actionDock->setObjectName("actionDock");
        actionDock->setFixedHeight(50);
        QHBoxLayout* actionLayout = new QHBoxLayout(actionDock);
        actionLayout->setContentsMargins(12, 10, 12, 10);
        actionLayout->setSpacing(10);
        runButton_ = new QPushButton(QString::fromUtf8("▶ 开始追踪"), actionDock);
        runButton_->setObjectName("runButton");
        stopButton_ = new QPushButton(QString::fromUtf8("■ 停止"), actionDock);
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
        workspaceLayout->addWidget(canvasRow, 1);

        QFrame* logDock = new QFrame(workspace);
        logDock->setObjectName("logDock");
        logDock->setFixedHeight(120);
        QVBoxLayout* logLayout = new QVBoxLayout(logDock);
        logLayout->setContentsMargins(0, 0, 0, 0);
        logLayout->setSpacing(0);
        QLabel* logHeader = new QLabel(QString::fromUtf8("运行日志与输出信息"), logDock);
        logHeader->setObjectName("logHeader");
        logHeader->setFixedHeight(24);
        logEdit_ = new QTextEdit(logDock);
        logEdit_->setObjectName("logEdit");
        logEdit_->setReadOnly(true);
        logEdit_->setLineWrapMode(QTextEdit::NoWrap);
        logLayout->addWidget(logHeader);
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
        mainSplitter_->setSizes({400, 800});

        QFrame* statusBar = new QFrame(this);
        statusBar->setObjectName("statusBar");
        statusBar->setFixedHeight(25);
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
        statusLayout->addWidget(coordinateLabel_);
        statusLayout->addStretch(1);
        statusLayout->addWidget(connectionLabel_);
        statusLayout->addWidget(projectStatusLabel_);
        statusLayout->addWidget(gridStatusLabel_);
        statusLayout->addWidget(statusLabel_);

        QVBoxLayout* rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);
        rootLayout->addWidget(menuBar);
        rootLayout->addWidget(toolbar);
        rootLayout->addWidget(mainSplitter_, 1);
        rootLayout->addWidget(statusBar);

        connect(projectEdit_, &QLineEdit::textChanged, this, &FaultTraceDialog::updateStatusContext);
        connect(databaseEdit_, &QLineEdit::textChanged, this, &FaultTraceDialog::updateStatusContext);
        connect(gridCombo_, &QComboBox::currentTextChanged, this, &FaultTraceDialog::updateStatusContext);
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
    dialog.resize(680, 350);
    dialog.setMinimumSize(660, 340);
    dialog.setStyleSheet(QString::fromUtf8(R"(
        QDialog {
            background: #F0F0F0;
            color: #222222;
            font-family: "Microsoft YaHei", "Segoe UI", Tahoma, sans-serif;
            font-size: 12px;
        }
        QLabel#dialogHeader {
            background: #005A9E;
            color: white;
            font-weight: bold;
            padding-left: 16px;
        }
        QLabel#checkStatus {
            color: #555555;
            padding: 2px 0;
        }
        QLineEdit {
            background: #FFFFFF;
            border: 1px solid #A0A0A0;
            border-radius: 2px;
            min-height: 22px;
            padding: 1px 5px;
        }
        QLineEdit:focus {
            border-color: #0078D7;
        }
        QPushButton {
            background: #E1E1E1;
            border: 1px solid #A0A0A0;
            border-radius: 2px;
            min-height: 24px;
            padding: 2px 12px;
        }
        QPushButton:hover {
            background: #E5F1FB;
            border-color: #0078D7;
        }
        QPushButton#applyButton {
            background: #0078D7;
            border-color: #005A9E;
            color: white;
            font-weight: bold;
        }
        QPushButton#applyButton:hover {
            background: #106EBE;
        }
        QFrame#line {
            background: #D0D0D0;
            min-height: 1px;
            max-height: 1px;
        }
    )"));

    QVBoxLayout* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame* headerFrame = new QFrame(&dialog);
    headerFrame->setObjectName("dialogHeader");
    headerFrame->setStyleSheet("QFrame#dialogHeader { background: #005A9E; }");
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

    auto addPathRow = [&](int row, const QString& labelText, QLineEdit* edit, const QString& dialogTitle, bool directory) {
        QLabel* label = new QLabel(labelText, body);
        label->setFixedWidth(132);
        QPushButton* browse = new QPushButton(QString::fromUtf8("浏览..."), body);
        browse->setFixedWidth(64);
        connect(browse, &QPushButton::clicked, &dialog, [this, edit, dialogTitle, directory]() {
            QString path;
            if (directory) {
                path = QFileDialog::getExistingDirectory(this, dialogTitle, edit->text());
            } else {
                path = QFileDialog::getOpenFileName(this, dialogTitle, QFileInfo(edit->text()).absolutePath(),
                                                   QString::fromUtf8("可执行文件 (*.exe);;所有文件 (*.*)"));
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
    footer->setStyleSheet("QFrame { background: #EAEAEA; border-top: 1px solid #C0C0C0; }");
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
        checkStatus->setText(QString::fromUtf8("已执行自动检测。项目名无法安全推断，请按当前 GeoEast 项目填写后再检查配置。"));
    };

    auto currentIssues = [&]() {
        return validateGeoEastConfigValues(runnerCopy->text(), geoEastRootCopy->text(), dataRootCopy->text(),
                                           hostCopy->text(), portCopy->text(), databaseCopy->text(),
                                           projectCopy->text(), userCopy->text());
    };

    auto showCheckResult = [&]() -> bool {
        const QStringList issues = currentIssues();
        if (issues.isEmpty()) {
            checkStatus->setText(QString::fromUtf8("配置检查通过。可以刷新网格列表。"));
            QMessageBox::information(&dialog, QString::fromUtf8("配置检查通过"),
                                     QString::fromUtf8("GeoEast 数据库配置项完整。保存后可返回主界面刷新网格列表。"));
            return true;
        }
        checkStatus->setText(QString::fromUtf8("配置仍有 %1 项需要处理。").arg(issues.size()));
        QMessageBox::warning(&dialog, QString::fromUtf8("配置需要补充"),
                             QString::fromUtf8("请处理以下项目：\n\n- %1").arg(issues.join(QString::fromUtf8("\n- "))));
        return false;
    };

    connect(helpButton, &QPushButton::clicked, &dialog, [&]() {
        QMessageBox::information(&dialog, QString::fromUtf8("GeoEast 配置帮助"), geoEastConfigHelpText());
    });
    connect(autoDetectButton, &QPushButton::clicked, &dialog, applyAutoDetection);
    connect(checkButton, &QPushButton::clicked, &dialog, showCheckResult);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(applyButton, &QPushButton::clicked, &dialog, [&]() {
        const QStringList issues = currentIssues();
        if (!issues.isEmpty()) {
            const int choice = QMessageBox::warning(
                &dialog,
                QString::fromUtf8("配置尚未完整"),
                QString::fromUtf8("以下配置可能导致刷新网格或写库失败：\n\n- %1\n\n建议返回修改。是否仍然保存？")
                    .arg(issues.join(QString::fromUtf8("\n- "))),
                QMessageBox::Save | QMessageBox::Cancel,
                QMessageBox::Cancel);
            if (choice != QMessageBox::Save) {
                checkStatus->setText(QString::fromUtf8("配置未保存，请补齐后再应用。"));
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

    dialog.exec();
}

void FaultTraceDialog::saveCurrentPreset() {
    saveSettings();
    updateStatusContext();
    appendLog(QString::fromUtf8("当前参数和界面配置已保存。"));
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
    importDatCheck_->setChecked(false);
    overwriteGridCheck_->setChecked(true);
    if (showInputLayerCheck_) {
        showInputLayerCheck_->setChecked(true);
    }
    if (showResultLayerCheck_) {
        showResultLayerCheck_->setChecked(true);
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
                             QString::fromUtf8("FaultPolyTrace 帮助提示"),
                             QString::fromUtf8("常用流程：\n"
                                               "1. 在【数据库配置】中确认 GeoEast 根目录、数据根目录、项目、用户和执行程序。\n"
                                                "2. 点击【刷新网格列表】读取 GeoEast 中的输入网格。\n"
                                                "3. 设置输入网格、输出多边形、结果颜色和必要参数。\n"
                                                "4. 点击【开始追踪】，完成后本窗口会显示原始网格和结果多边形。\n"
                                                "5. 如需用 GeoEast 官方窗口查看，点击【打开工区底图】或【打开平面成图】，再在图层树中勾选 Grids 和 Fault Polygons。\n"
                                                "6. 如需移除旧结果，在【结果查看】中点击【删除当前结果】。\n\n"
                                                "详细说明见 docs/用户操作手册.md 和 docs/评审交付与异机运行说明.md。"));
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
    fpt_ui::refreshComboItems(outputCatalogCombo_, languageCode());
    if (refreshOutputsButton_) {
        refreshOutputsButton_->setText(uiText(QString::fromUtf8("刷新结果")));
    }
    if (loadSelectedOutputButton_) {
        loadSelectedOutputButton_->setText(uiText(QString::fromUtf8("选为当前")));
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
        QStringList translatedLines;
        for (const QString& line : logEdit_->toPlainText().split('\n')) {
            translatedLines << uiText(line);
        }
        logEdit_->setPlainText(translatedLines.join('\n'));
        logEdit_->moveCursor(QTextCursor::End);
    }
    updateStatusContext();
}

void FaultTraceDialog::loadSettings() {
    QSettings settings(QCoreApplication::applicationDirPath() + "/faultpolytrace_launcher.ini", QSettings::IniFormat);
    const int schemaVersion = settings.value("configSchemaVersion", 0).toInt();
    currentLanguage_ = fpt_ui::normalizeLanguageCode(settings.value("language", "zh-CN").toString()) == "en"
        ? UiLanguage::English
        : UiLanguage::SimplifiedChinese;
    runnerEdit_->setText(settingOrCurrent(settings, "runner", runnerEdit_->text(), schemaVersion));
    geoEastRootEdit_->setText(settingOrCurrent(settings, "geoEastRoot", geoEastRootEdit_->text(), schemaVersion));
    geoEastDataRootEdit_->setText(settingOrCurrent(settings, "geoEastDataRoot", geoEastDataRootEdit_->text(), schemaVersion));
    pgHostEdit_->setText(settings.value("pgHost", pgHostEdit_->text()).toString());
    pgPortEdit_->setText(settings.value("pgPort", pgPortEdit_->text()).toString());
    databaseEdit_->setText(settings.value("database", databaseEdit_->text()).toString());
    projectEdit_->setText(settingOrCurrent(settings, "project", projectEdit_->text(), schemaVersion));
    userEdit_->setText(settingOrCurrent(settings, "user", userEdit_->text(), schemaVersion));
    setComboByKey(presetCombo_, settings.value("preset", comboKey(presetCombo_)).toString());
    gridCombo_->setCurrentText(settingOrCurrent(settings, "grid", gridCombo_->currentText(), schemaVersion));
    outputEdit_->setText(settings.value("output", outputEdit_->text()).toString());
    datFileEdit_->setText(settingOrCurrent(settings, "datFile", datFileEdit_->text(), schemaVersion));
    importDatCheck_->setChecked(settings.value("importDat", importDatCheck_->isChecked()).toBool());
    overwriteGridCheck_->setChecked(settings.value("overwriteGrid", overwriteGridCheck_->isChecked()).toBool());
    polygonColorEdit_->setText(settings.value("polygonColor", polygonColorEdit_->text()).toString());
    setPolygonColor(QColor(polygonColorEdit_->text()));
    polygonLineWidthSpin_->setValue(settings.value("polygonLineWidth", polygonLineWidthSpin_->value()).toDouble());
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
        showInputLayerCheck_->setChecked(settings.value("showInputLayer", showInputLayerCheck_->isChecked()).toBool());
    }
    if (showResultLayerCheck_) {
        showResultLayerCheck_->setChecked(settings.value("showResultLayer", showResultLayerCheck_->isChecked()).toBool());
    }
    if (mainSplitter_) {
        const QByteArray splitterState = settings.value("mainSplitterState").toByteArray();
        if (!splitterState.isEmpty()) {
            mainSplitter_->restoreState(splitterState);
        }
    }
    updateOptionControls();
    updateLayerVisibility();
    updateStatusContext();
    retranslateUi();
}

void FaultTraceDialog::saveSettings() const {
    QSettings settings(QCoreApplication::applicationDirPath() + "/faultpolytrace_launcher.ini", QSettings::IniFormat);
    settings.setValue("configSchemaVersion", 2);
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
    settings.setValue("grid", gridCombo_->currentText());
    settings.setValue("output", outputEdit_->text());
    settings.setValue("datFile", datFileEdit_->text());
    settings.setValue("importDat", importDatCheck_->isChecked());
    settings.setValue("overwriteGrid", overwriteGridCheck_->isChecked());
    settings.setValue("polygonColor", polygonColorEdit_->text());
    settings.setValue("polygonLineWidth", polygonLineWidthSpin_->value());
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

    QMessageBox message(this);
    message.setIcon(QMessageBox::Warning);
    message.setWindowTitle(QString::fromUtf8("GeoEast 配置不完整"));
    message.setText(QString::fromUtf8("执行【%1】前需要先补齐 GeoEast 配置。").arg(operation));
    message.setInformativeText(QString::fromUtf8("缺失或无效项：\n\n- %1\n\n请点击【打开数据库配置】补齐后再执行。保存一次后，下次打开会自动记住。")
                                   .arg(missing.join(QString::fromUtf8("\n- "))));
    QPushButton* configButton = message.addButton(QString::fromUtf8("打开数据库配置"), QMessageBox::AcceptRole);
    message.addButton(QString::fromUtf8("稍后处理"), QMessageBox::RejectRole);
    message.exec();
    if (message.clickedButton() == configButton) {
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
         << "--user" << userEdit_->text().trimmed()
         << "--password" << passwordEdit_->text();
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
    const QString geoEastRoot = QDir::fromNativeSeparators(geoEastRootEdit_ ? geoEastRootEdit_->text().trimmed() : QString());
    const QString exePath = QDir(geoEastRoot).absoluteFilePath("GeoEast-RC/bin/" + exeName);
    if (!QFileInfo::exists(exePath)) {
        QMessageBox::warning(this, QString::fromUtf8("GeoEast 程序不存在"),
                             QString::fromUtf8("未找到 %1：\n%2").arg(displayName, QDir::toNativeSeparators(exePath)));
        return false;
    }

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    configureGeoEastEnvironment(environment);
    process.setProgram(QDir::toNativeSeparators(exePath));
    process.setWorkingDirectory(QFileInfo(exePath).absolutePath());
    process.setProcessEnvironment(environment);
    if (!process.startDetached()) {
        QMessageBox::warning(this, QString::fromUtf8("启动失败"),
                             QString::fromUtf8("无法启动 %1。请确认 GeoEast 环境和 license 可用。").arg(displayName));
        return false;
    }

    appendLog(QString::fromUtf8("已启动 %1：%2").arg(displayName, QDir::toNativeSeparators(exePath)));
    appendLog(QString::fromUtf8("官方窗口中请在图层树加载 Grids/%1 与 Fault Polygons/%2；本插件无法从外部进程强制勾选 GeoEast 内部图层。")
                  .arg(gridCombo_ ? gridCombo_->currentText().trimmed() : QString(),
                       outputEdit_ ? outputEdit_->text().trimmed() : QString()));
    appendStatus(QString::fromUtf8("已打开 %1，按官方图层树查看结果").arg(displayName));
    return true;
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
         << "--root-anti-merge-min-part-cells" << QString::number(rootAntiMergeMinPartCellsSpin_->value())
         << "--root-anti-merge-min-part-ratio" << QString::number(rootAntiMergeMinPartRatioSpin_->value(), 'g', 12)
         << "--root-anti-merge-weak-guard-threshold" << QString::number(rootAntiMergeWeakGuardThresholdSpin_->value(), 'g', 12)
         << "--root-anti-merge-weak-guard-radius" << QString::number(rootAntiMergeWeakGuardRadiusSpin_->value())
         << "--local-anti-merge-low-threshold" << QString::number(localAntiMergeLowThresholdSpin_->value(), 'g', 12)
         << "--local-anti-merge-bridge-radius" << QString::number(localAntiMergeBridgeRadiusSpin_->value())
         << "--local-anti-merge-min-part-cells" << QString::number(localAntiMergeMinPartCellsSpin_->value())
         << "--local-anti-merge-min-part-ratio" << QString::number(localAntiMergeMinPartRatioSpin_->value(), 'g', 12)
         << "--local-anti-merge-min-bridge-cells" << QString::number(localAntiMergeMinBridgeCellsSpin_->value())
         << "--local-anti-merge-min-low-ratio" << QString::number(localAntiMergeMinLowRatioSpin_->value(), 'g', 12)
         << "--residual-percentile" << "0.955"
         << "--residual-fill-radius" << "4"
         << "--residual-edge-fill-radius" << "2"
         << "--residual-edge-fill-min-value-ratio" << "0.97"
         << "--residual-edge-fill-max-patch-span" << "5"
         << "--residual-min-cells" << "2"
         << "--residual-trace-min-length" << QString::number(residualTraceMinLengthSpin_->value())
         << "--residual-trace-max-cells" << QString::number(residualTraceMaxCellsSpin_->value())
         << "--residual-covered-guard-radius" << "0"
         << "--simplify-tolerance" << QString::number(simplifyToleranceSpin_->value(), 'g', 12)
         << "--smooth-iterations" << QString::number(smoothIterationsSpin_->value())
          << "--polygon-color" << polygonColorEdit_->text()
          << "--polygon-linewidth" << QString::number(polygonLineWidthSpin_->value(), 'g', 12)
          << "--min-area" << QString::number(minAreaSpin_->value(), 'g', 12)
          << "--view-grid-output" << previewGridPath()
          << "--view-polygons-output" << previewPolygonsPath();
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
    if (localAntiMergeCheck_->isChecked()) {
        args << "--local-anti-merge";
    } else {
        args << "--no-local-anti-merge";
    }
    if (residualTraceCheck_->isChecked()) {
        args << "--residual-trace";
    } else {
        args << "--no-residual-trace";
    }
    return args;
}

void FaultTraceDialog::applyParameterPreset(const QString& presetName) {
    const QString presetKey = fpt_ui::canonical(presetName);
    traceModeCombo_->setCurrentText("ridge-belt");
    thresholdModeCombo_->setCurrentText("percentile");
    thresholdSpin_->setValue(0.95);
    auto setLocalAntiMergePreset = [&](bool enabled) {
        localAntiMergeCheck_->setChecked(enabled);
        localAntiMergeLowThresholdSpin_->setValue(0.05);
        localAntiMergeBridgeRadiusSpin_->setValue(2);
        localAntiMergeMinPartCellsSpin_->setValue(80);
        localAntiMergeMinPartRatioSpin_->setValue(0.08);
        localAntiMergeMinBridgeCellsSpin_->setValue(6);
        localAntiMergeMinLowRatioSpin_->setValue(0.55);
    };
    auto setRootAntiMergePreset = [&](bool enabled) {
        rootAntiMergeCheck_->setChecked(enabled);
        rootAntiMergeRadiusSpin_->setValue(1);
        rootAntiMergeSupportRatioSpin_->setValue(0.25);
        rootAntiMergeMinPartCellsSpin_->setValue(0);
        rootAntiMergeMinPartRatioSpin_->setValue(0.12);
        rootAntiMergeWeakGuardThresholdSpin_->setValue(0.66);
        rootAntiMergeWeakGuardRadiusSpin_->setValue(0);
    };
    setLocalAntiMergePreset(false);
    setRootAntiMergePreset(false);

    if (presetKey == QString::fromUtf8("全覆盖优先")) {
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
        setRootAntiMergePreset(false);
        residualTraceCheck_->setChecked(true);
        simplifyToleranceSpin_->setValue(1.5);
        smoothIterationsSpin_->setValue(0);
        residualTraceMinLengthSpin_->setValue(5);
        residualTraceMaxCellsSpin_->setValue(120);
    } else if (presetKey == QString::fromUtf8("防黏连增强")) {
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
        setRootAntiMergePreset(true);
        residualTraceCheck_->setChecked(false);
        simplifyToleranceSpin_->setValue(2.0);
        smoothIterationsSpin_->setValue(1);
        residualTraceMinLengthSpin_->setValue(6);
        residualTraceMaxCellsSpin_->setValue(80);
    } else if (presetKey == QString::fromUtf8("保守高精度")) {
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
        setRootAntiMergePreset(false);
        residualTraceCheck_->setChecked(false);
        simplifyToleranceSpin_->setValue(5.0);
        smoothIterationsSpin_->setValue(1);
        residualTraceMinLengthSpin_->setValue(8);
        residualTraceMaxCellsSpin_->setValue(40);
    } else if (presetKey == QString::fromUtf8("弱响应高召回")) {
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
        setRootAntiMergePreset(false);
        residualTraceCheck_->setChecked(true);
        simplifyToleranceSpin_->setValue(2.0);
        smoothIterationsSpin_->setValue(1);
        residualTraceMinLengthSpin_->setValue(5);
        residualTraceMaxCellsSpin_->setValue(120);
    } else if (presetKey == QString::fromUtf8("宽断层带")) {
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
        setRootAntiMergePreset(false);
        residualTraceCheck_->setChecked(false);
        simplifyToleranceSpin_->setValue(6.0);
        smoothIterationsSpin_->setValue(1);
        residualTraceMinLengthSpin_->setValue(8);
        residualTraceMaxCellsSpin_->setValue(60);
    } else if (presetKey == QString::fromUtf8("细碎断裂")) {
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
        setRootAntiMergePreset(false);
        residualTraceCheck_->setChecked(false);
        simplifyToleranceSpin_->setValue(3.0);
        smoothIterationsSpin_->setValue(1);
        residualTraceMinLengthSpin_->setValue(4);
        residualTraceMaxCellsSpin_->setValue(80);
    } else if (presetKey == QString::fromUtf8("贴合融合推荐")) {
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
        setRootAntiMergePreset(false);
        residualTraceCheck_->setChecked(false);
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
        maxGapDistanceSpin_->setValue(12);
        maxGapAngleSpin_->setValue(55.0);
        setRootAntiMergePreset(false);
        residualTraceCheck_->setChecked(false);
        simplifyToleranceSpin_->setValue(2.0);
        smoothIterationsSpin_->setValue(1);
        residualTraceMinLengthSpin_->setValue(6);
        residualTraceMaxCellsSpin_->setValue(80);
    }
    updateOptionControls();
    updateStatusContext();
}

void FaultTraceDialog::refreshGrids() {
    if (process_->state() != QProcess::NotRunning) {
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
    deletingOutput_ = false;
    logEdit_->clear();
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
    deletingOutput_ = false;
    logEdit_->clear();
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

void FaultTraceDialog::loadSelectedOutput() {
    if (!outputCatalogCombo_) {
        return;
    }
    const QString selected = outputCatalogCombo_->currentData().toString();
    if (selected.trimmed().isEmpty()) {
        QMessageBox::information(this,
                                 QString::fromUtf8("没有可选择的结果"),
                                 QString::fromUtf8("请先运行追踪，或点击【刷新结果】读取当前项目中的结果多边形对象。"));
        return;
    }
    if (process_->state() != QProcess::NotRunning) {
        appendStatus(QString::fromUtf8("任务运行中，暂不能载入已有结果"));
        return;
    }
    saveSettings();
    if (!ensureGeoEastConfigReady(QString::fromUtf8("载入已有结果"), false, false)) {
        return;
    }
    outputEdit_->setText(selected);
    const QString runner = runnerEdit_->text().trimmed();
    refreshingGrids_ = false;
    refreshingOutputs_ = false;
    exportingOutputView_ = true;
    deletingOutput_ = false;
    logEdit_->clear();
    QStringList args = commonConnectionArgs();
    args << "--output" << selected
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
    if (process_->state() != QProcess::NotRunning) {
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
    if (process_->state() != QProcess::NotRunning) {
        return;
    }
    const QString outputName = outputEdit_->text().trimmed();
    saveSettings();
    if (!ensureGeoEastConfigReady(QString::fromUtf8("删除当前结果"), false, true)) {
        return;
    }
    const int choice = QMessageBox::warning(
        this,
        QString::fromUtf8("确认删除 GeoEast 结果"),
        QString::fromUtf8("将删除当前项目中的结果多边形对象：\n\n%1\n\n该操作不会删除输入网格，也不会删除其它解释对象。是否继续？").arg(outputName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (choice != QMessageBox::Yes) {
        return;
    }
    const QString runner = runnerEdit_->text().trimmed();

    refreshingGrids_ = false;
    refreshingOutputs_ = false;
    exportingOutputView_ = false;
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
    const bool gridLoaded = view->loadGridFile(gridPath, &error);
    if (!gridLoaded) {
        appendLog(error);
    } else {
        appendLog(QString::fromUtf8("已载入原始网格视图：%1").arg(QDir::toNativeSeparators(gridPath)));
    }

    error.clear();
    const bool polygonsLoaded = view->loadPolygonCsv(polygonPath, &error);
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
    QDir().mkpath(previewOutputDir());
    const QString fileName = QString("%1_%2_%3.png")
        .arg(sanitizeFileComponent(gridCombo_ ? gridCombo_->currentText() : QString(), "grid"),
             sanitizeFileComponent(outputEdit_ ? outputEdit_->text() : QString(), "output"),
             QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    const QString path = QDir(previewOutputDir()).absoluteFilePath(fileName);
    if (static_cast<RenderViewWidget*>(renderViewWidget_)->savePng(path)) {
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
        if (renderViewWidget_) {
            static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("网格对象刷新完成"));
        }
        refreshingGrids_ = false;
        if (refreshOutputsAfterGrids_) {
            refreshOutputsAfterGrids_ = false;
            QTimer::singleShot(0, this, &FaultTraceDialog::refreshOutputs);
        }
        return;
    }

    if (refreshingOutputs_ && exitCode == 0 && status == QProcess::NormalExit) {
        const QString text = logEdit_->toPlainText();
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
        }
        appendStatus(QString::fromUtf8("结果对象刷新完成，共 %1 个").arg(names.size()));
        if (renderViewWidget_) {
            static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("结果对象刷新完成"));
        }
        refreshingOutputs_ = false;
        return;
    }

    if (refreshingOutputs_) {
        appendStatus(QString::fromUtf8("结果对象刷新失败，请查看日志"));
        refreshingOutputs_ = false;
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
    stopButton_->setEnabled(running);
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
    logEdit_->moveCursor(QTextCursor::End);
    logEdit_->insertPlainText(text);
    if (!text.endsWith('\n')) {
        logEdit_->insertPlainText("\n");
    }
    logEdit_->moveCursor(QTextCursor::End);
}

void FaultTraceDialog::appendStatus(const QString& text) {
    statusLabel_->setText(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + text);
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

    updateStatusContext();
}

void FaultTraceDialog::updateStatusContext() {
    if (projectStatusLabel_) {
        projectStatusLabel_->setText(uiText(QString::fromUtf8("正在操作项目: %1")).arg(projectEdit_ ? projectEdit_->text().trimmed() : QString()));
    }
    if (gridStatusLabel_) {
        gridStatusLabel_->setText(uiText(QString::fromUtf8("当前网格: %1")).arg(gridCombo_ ? gridCombo_->currentText().trimmed() : QString()));
    }
    if (connectionLabel_ && process_ && process_->state() == QProcess::NotRunning) {
        const QString db = databaseEdit_ ? databaseEdit_->text().trimmed() : QString();
        connectionLabel_->setText(db.isEmpty() ? uiText(QString::fromUtf8("数据库未配置")) : uiText(QString::fromUtf8("数据库连接待验证: %1")).arg(db));
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
            polygonLineWidthSpin_ ? polygonLineWidthSpin_->value() : 0.03);
        static_cast<RenderViewWidget*>(renderViewWidget_)->setLayerVisibility(
            !showInputLayerCheck_ || showInputLayerCheck_->isChecked(),
            !showResultLayerCheck_ || showResultLayerCheck_->isChecked());
    }
}
