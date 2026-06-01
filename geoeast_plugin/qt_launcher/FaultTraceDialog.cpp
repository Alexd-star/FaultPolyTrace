#include "FaultTraceDialog.h"

#include <QAction>
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
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
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
#include <QStandardPaths>
#include <QTextEdit>
#include <QTextCursor>
#include <QTextStream>
#include <QToolBar>
#include <QUrl>
#include <QVector>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <limits>

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

QString firstExistingFile(const QStringList& paths, const QString& fallback = QString()) {
    for (const QString& path : paths) {
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            return QDir::toNativeSeparators(path);
        }
    }
    return QDir::toNativeSeparators(fallback);
}

QString firstExistingDir(const QStringList& paths, const QString& fallback = QString()) {
    for (const QString& path : paths) {
        if (!path.isEmpty() && QDir(path).exists()) {
            return QDir::toNativeSeparators(path);
        }
    }
    return QDir::toNativeSeparators(fallback);
}

QString productConfigDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty()) {
        dir = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("config");
    }
    QDir().mkpath(dir);
    return dir;
}

QString defaultGeoEastRootPath() {
    const QString explicitRoot = envOrDefault("FPT_GEOEAST_ROOT", "");
    if (!explicitRoot.isEmpty()) {
        return QDir::toNativeSeparators(explicitRoot);
    }
    const QString geoeastEnv = envOrDefault("GEOEAST", "");
    if (!geoeastEnv.isEmpty()) {
        QDir dir(geoeastEnv);
        if (dir.dirName().compare("GeoEast-RC", Qt::CaseInsensitive) == 0 && dir.cdUp()) {
            return QDir::toNativeSeparators(dir.absolutePath());
        }
    }
    return firstExistingDir({
        "C:/GeoEast",
        "D:/GeoEast"
    });
}

QString defaultGeoEastDataRootPath() {
    const QString fromEnv = envOrDefault("FPT_GEOEAST_DATA_ROOT", "");
    if (!fromEnv.isEmpty()) {
        return QDir::toNativeSeparators(fromEnv);
    }
    const QString root = defaultGeoEastRootPath();
    if (!root.isEmpty()) {
        return firstExistingDir({
            QDir(root).absoluteFilePath("GeoEastData"),
            QDir(root).absoluteFilePath("Data/GeoEastData")
        });
    }
    return QString();
}

QString defaultUserName() {
    return envOrDefault("FPT_GEOEAST_USER", envOrDefault("USERNAME", envOrDefault("USER", "")));
}

QString elapsedText(qint64 elapsedMs) {
    return QString::fromUtf8("%1 ms (%2 s)")
        .arg(elapsedMs)
        .arg(elapsedMs / 1000.0, 0, 'f', elapsedMs < 1000 ? 3 : 2);
}

QString quoteForLog(const QStringList& args) {
    QStringList quoted;
    for (const QString& arg : args) {
        quoted << (arg.contains(' ') ? QString("\"%1\"").arg(arg) : arg);
    }
    return quoted.join(' ');
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
    enum class ViewMode {
        Raw,
        Result,
        Overlay
    };

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
        zoom_ = 1.0;
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
        grid_ = grid;
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

    void setActivity(const QString& activity) {
        activity_ = activity;
        update();
    }

    void setViewMode(ViewMode mode) {
        viewMode_ = mode;
        update();
    }

    bool loadGridFile(const QString& path, const QString& formatHint, QString* errorMessage = nullptr) {
        gridValues_.clear();
        gridNx_ = 0;
        gridNy_ = 0;
        gridMin_ = 0.0;
        gridMax_ = 1.0;
        gridXMin_ = 0.0;
        gridYMin_ = 0.0;
        gridXMax_ = 1.0;
        gridYMax_ = 1.0;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (errorMessage) {
                *errorMessage = QString::fromUtf8("无法打开输入文件：%1").arg(path);
            }
            update();
            return false;
        }

        const QString text = QString::fromLocal8Bit(file.readAll());
        const QStringList lines = text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        if (lines.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QString::fromUtf8("输入文件为空。");
            }
            update();
            return false;
        }

        auto splitNumbers = [](const QString& line) {
            QVector<double> numbers;
            for (const QString& token : line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)) {
                bool ok = false;
                const double value = token.toDouble(&ok);
                if (ok) {
                    numbers.push_back(value);
                }
            }
            return numbers;
        };

        const QString format = formatHint.trimmed().toLower();
        const QVector<double> first = splitNumbers(lines.value(0));
        const QVector<double> second = splitNumbers(lines.value(1));
        const QVector<double> third = splitNumbers(lines.value(2));
        const bool matrixLike = (format == "matrix") ||
            (format == "auto" && first.size() == 2 && second.size() == 2 && third.size() == 2 && lines.size() > 3);

        if (matrixLike) {
            const int nx = qRound(first[0]);
            const int ny = qRound(first[1]);
            if (nx <= 0 || ny <= 0) {
                if (errorMessage) {
                    *errorMessage = QString::fromUtf8("矩阵网格头中的行列数无效。");
                }
                update();
                return false;
            }
            QVector<double> values;
            values.reserve(nx * ny);
            for (int i = 3; i < lines.size(); ++i) {
                const QVector<double> rowValues = splitNumbers(lines[i]);
                for (double value : rowValues) {
                    values.push_back(value);
                }
            }
            if (values.size() < nx * ny) {
                if (errorMessage) {
                    *errorMessage = QString::fromUtf8("矩阵网格数值数量不足，无法预览。");
                }
                update();
                return false;
            }
            values.resize(nx * ny);
            gridNx_ = nx;
            gridNy_ = ny;
            gridValues_ = values;
            gridXMin_ = second[0];
            gridYMin_ = second[1];
            const double dx = third[0] == 0.0 ? 1.0 : third[0];
            const double dy = third[1] == 0.0 ? 1.0 : third[1];
            gridXMax_ = gridXMin_ + dx * qMax(1, nx - 1);
            gridYMax_ = gridYMin_ + dy * qMax(1, ny - 1);
        } else {
            struct DatSample {
                double line = 0.0;
                double cmp = 0.0;
                double x = 0.0;
                double y = 0.0;
                double value = 0.0;
            };
            QVector<DatSample> samples;
            QVector<double> lineKeys;
            QVector<double> cmpKeys;
            for (const QString& line : lines) {
                const QVector<double> values = splitNumbers(line);
                if (values.size() < 5) {
                    continue;
                }
                DatSample sample{values[0], values[1], values[2], values[3], values[4]};
                samples.push_back(sample);
                lineKeys.push_back(sample.line);
                cmpKeys.push_back(sample.cmp);
            }
            auto sortUnique = [](QVector<double>& values) {
                std::sort(values.begin(), values.end());
                values.erase(std::unique(values.begin(), values.end(), [](double a, double b) {
                    return qAbs(a - b) < 1e-9;
                }), values.end());
            };
            sortUnique(lineKeys);
            sortUnique(cmpKeys);
            if (samples.isEmpty() || lineKeys.isEmpty() || cmpKeys.isEmpty()) {
                if (errorMessage) {
                    *errorMessage = QString::fromUtf8("未识别到 GeoEast DAT 五列采样数据。");
                }
                update();
                return false;
            }
            gridNx_ = cmpKeys.size();
            gridNy_ = lineKeys.size();
            gridValues_.fill(0.0, gridNx_ * gridNy_);
            gridXMin_ = std::numeric_limits<double>::max();
            gridYMin_ = std::numeric_limits<double>::max();
            gridXMax_ = -std::numeric_limits<double>::max();
            gridYMax_ = -std::numeric_limits<double>::max();
            for (const DatSample& sample : samples) {
                const int row = std::lower_bound(lineKeys.begin(), lineKeys.end(), sample.line) - lineKeys.begin();
                const int col = std::lower_bound(cmpKeys.begin(), cmpKeys.end(), sample.cmp) - cmpKeys.begin();
                if (row >= 0 && row < gridNy_ && col >= 0 && col < gridNx_) {
                    gridValues_[row * gridNx_ + col] = sample.value;
                }
                gridXMin_ = qMin(gridXMin_, sample.x);
                gridYMin_ = qMin(gridYMin_, sample.y);
                gridXMax_ = qMax(gridXMax_, sample.x);
                gridYMax_ = qMax(gridYMax_, sample.y);
            }
            if (qFuzzyCompare(gridXMin_, gridXMax_) || qFuzzyCompare(gridYMin_, gridYMax_)) {
                gridXMin_ = 0.0;
                gridYMin_ = 0.0;
                gridXMax_ = qMax(1, gridNx_ - 1);
                gridYMax_ = qMax(1, gridNy_ - 1);
            }
        }

        gridMin_ = std::numeric_limits<double>::max();
        gridMax_ = -std::numeric_limits<double>::max();
        for (double value : gridValues_) {
            gridMin_ = qMin(gridMin_, value);
            gridMax_ = qMax(gridMax_, value);
        }
        if (qFuzzyCompare(gridMin_, gridMax_)) {
            gridMax_ = gridMin_ + 1.0;
        }
        activity_ = QString::fromUtf8("已载入原始网格预览：%1 x %2").arg(gridNx_).arg(gridNy_);
        update();
        return true;
    }

    bool loadPolygonCsv(const QString& path, QString* errorMessage = nullptr) {
        polygonRings_.clear();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (errorMessage) {
                *errorMessage = QString::fromUtf8("无法打开结果 CSV：%1").arg(path);
            }
            update();
            return false;
        }
        const QString text = QString::fromLocal8Bit(file.readAll());
        const QStringList lines = text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        QRegularExpression numberRe("[-+]?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?");
        double pxMin = std::numeric_limits<double>::max();
        double pyMin = std::numeric_limits<double>::max();
        double pxMax = -std::numeric_limits<double>::max();
        double pyMax = -std::numeric_limits<double>::max();
        for (const QString& line : lines) {
            if (!line.contains("POLYGON", Qt::CaseInsensitive) || !line.contains("kept", Qt::CaseInsensitive)) {
                continue;
            }
            const int start = line.indexOf("POLYGON", 0, Qt::CaseInsensitive);
            const QString wkt = line.mid(start);
            QVector<double> numbers;
            QRegularExpressionMatchIterator iterator = numberRe.globalMatch(wkt);
            while (iterator.hasNext()) {
                bool ok = false;
                const double value = iterator.next().captured(0).toDouble(&ok);
                if (ok) {
                    numbers.push_back(value);
                }
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
                polygonRings_ << ring;
            }
        }
        if (gridValues_.isEmpty() && !polygonRings_.isEmpty()) {
            gridXMin_ = pxMin;
            gridYMin_ = pyMin;
            gridXMax_ = pxMax;
            gridYMax_ = pyMax;
        }
        activity_ = polygonRings_.isEmpty()
            ? QString::fromUtf8("结果 CSV 已读取，但未发现保留多边形")
            : QString::fromUtf8("已载入结果多边形：%1 个").arg(polygonRings_.size());
        update();
        return !polygonRings_.isEmpty();
    }

    void setCoordinateCallback(std::function<void(const QPointF&)> callback) {
        coordinateCallback_ = std::move(callback);
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

        const QRectF viewRect = rect().adjusted(26, 60, -74, -34);
        if (!gridValues_.isEmpty()) {
            drawGridPreview(painter, viewRect);
        }
        if (!polygonRings_.isEmpty() && (viewMode_ == ViewMode::Result || viewMode_ == ViewMode::Overlay)) {
            drawPolygonPreview(painter, viewRect);
        }

        QFont titleFont = painter.font();
        titleFont.setPointSize(16);
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.setPen(QColor("#7A7A7A"));
        const QString viewTitle = viewMode_ == ViewMode::Raw
            ? QString::fromUtf8("[渲染视图区] 原始网格属性")
            : (viewMode_ == ViewMode::Result ? QString::fromUtf8("[渲染视图区] 断层多边形结果")
                                              : QString::fromUtf8("[渲染视图区] 原始属性与结果叠加"));
        painter.drawText(QPoint(26, 38), viewTitle);

        QFont hintFont = painter.font();
        hintFont.setPointSize(10);
        hintFont.setBold(false);
        painter.setFont(hintFont);
        painter.setPen(QColor("#666666"));
        const QString thresholdText = thresholdMode_ == "fixed"
            ? QString::fromUtf8("固定阈值 %1").arg(threshold_, 0, 'f', 4)
            : QString::fromUtf8("分位数 %1").arg(percentile_, 0, 'f', 4);
        const QStringList lines = {
            QString::fromUtf8("数据库：%1    项目：%2").arg(database_.isEmpty() ? QString::fromUtf8("未配置") : database_,
                                                        project_.isEmpty() ? QString::fromUtf8("未配置") : project_),
            QString::fromUtf8("输入网格：%1").arg(grid_.isEmpty() ? QString::fromUtf8("未选择") : grid_),
            QString::fromUtf8("输出多边形：%1").arg(output_.isEmpty() ? QString::fromUtf8("未命名") : output_),
            QString::fromUtf8("方案：%1    模式：%2    阈值：%3").arg(preset_, traceMode_, thresholdText),
            QString::fromUtf8("面积过滤：小于 %1 的多边形不输出    结果线宽：%2").arg(minArea_, 0, 'f', 2).arg(polygonLineWidth_, 0, 'f', 3),
            QString::fromUtf8("当前状态：%1").arg(activity_.isEmpty() ? QString::fromUtf8("等待运行") : activity_)
        };
        if (gridValues_.isEmpty() && polygonRings_.isEmpty()) {
            painter.drawText(rect().adjusted(26, 60, -26, -26), Qt::AlignLeft | Qt::AlignTop, lines.join('\n'));
        }

        const QRectF previewRect(width() - 300, height() - 190, 250, 130);
        if (gridValues_.isEmpty() && polygonRings_.isEmpty() && previewRect.left() > 360 && previewRect.top() > 170) {
            painter.setPen(QPen(QColor("#C8C8C8"), 1));
            painter.setBrush(QColor("#FBFBFB"));
            painter.drawRect(previewRect);
            painter.setPen(QPen(QColor("#E6E6E6"), 1));
            for (int i = 1; i < 5; ++i) {
                const qreal x = previewRect.left() + previewRect.width() * i / 5.0;
                painter.drawLine(QPointF(x, previewRect.top()), QPointF(x, previewRect.bottom()));
            }
            for (int i = 1; i < 4; ++i) {
                const qreal y = previewRect.top() + previewRect.height() * i / 4.0;
                painter.drawLine(QPointF(previewRect.left(), y), QPointF(previewRect.right(), y));
            }
            painter.setPen(QPen(polygonColor_, qMax(1.5, polygonLineWidth_ * 40.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPainterPath path;
            path.moveTo(previewRect.left() + 24, previewRect.bottom() - 28);
            path.cubicTo(previewRect.left() + 80, previewRect.top() + 88,
                         previewRect.left() + 142, previewRect.top() + 36,
                         previewRect.right() - 28, previewRect.top() + 24);
            painter.drawPath(path);
            painter.setFont(hintFont);
            painter.setPen(QColor("#666666"));
            painter.drawText(previewRect.adjusted(8, 8, -8, -8), Qt::AlignLeft | Qt::AlignTop,
                             QString::fromUtf8("样式预览"));
        }

        painter.setPen(QColor("#8A8A8A"));
        painter.drawText(QRect(width() - 210, 18, 180, 20), Qt::AlignRight,
                         QString::fromUtf8("%1  缩放 %2%")
                             .arg(panMode_ ? QString::fromUtf8("平移模式") : QString::fromUtf8("查看模式"))
                             .arg(qRound(zoom_ * 100)));
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (coordinateCallback_) {
            coordinateCallback_(QPointF(event->pos().x() / zoom_, event->pos().y() / zoom_));
        }
        QWidget::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent* event) override {
        if (coordinateCallback_) {
            coordinateCallback_(QPointF(-1.0, -1.0));
        }
        QWidget::leaveEvent(event);
    }

private:
    QColor heatColor(double value) const {
        const double t = qBound(0.0, (value - gridMin_) / (gridMax_ - gridMin_), 1.0);
        if (t < 0.25) {
            const double k = t / 0.25;
            return QColor::fromRgbF(1.0, k, 0.0);
        }
        if (t < 0.50) {
            const double k = (t - 0.25) / 0.25;
            return QColor::fromRgbF(1.0 - k, 1.0, 0.0);
        }
        if (t < 0.75) {
            const double k = (t - 0.50) / 0.25;
            return QColor::fromRgbF(0.0, 1.0, k);
        }
        const double k = (t - 0.75) / 0.25;
        return QColor::fromRgbF(0.0, 1.0 - k, 1.0);
    }

    QRectF fittedDataRect(const QRectF& available) const {
        if (available.width() <= 10 || available.height() <= 10) {
            return available;
        }
        const double dataWidth = qMax(1e-9, qAbs(gridXMax_ - gridXMin_));
        const double dataHeight = qMax(1e-9, qAbs(gridYMax_ - gridYMin_));
        const double scale = qMin(available.width() / dataWidth, available.height() / dataHeight) * zoom_;
        const double width = dataWidth * scale;
        const double height = dataHeight * scale;
        return QRectF(available.center().x() - width / 2.0,
                      available.center().y() - height / 2.0,
                      width,
                      height);
    }

    QPointF mapDataPoint(const QPointF& point, const QRectF& dataRect) const {
        const double xRange = qMax(1e-9, gridXMax_ - gridXMin_);
        const double yRange = qMax(1e-9, gridYMax_ - gridYMin_);
        const double x = dataRect.left() + (point.x() - gridXMin_) / xRange * dataRect.width();
        const double y = dataRect.bottom() - (point.y() - gridYMin_) / yRange * dataRect.height();
        return QPointF(x, y);
    }

    void drawGridPreview(QPainter& painter, const QRectF& available) const {
        if (gridNx_ <= 0 || gridNy_ <= 0 || gridValues_.isEmpty() || viewMode_ == ViewMode::Result) {
            return;
        }
        QImage image(gridNx_, gridNy_, QImage::Format_RGB32);
        for (int y = 0; y < gridNy_; ++y) {
            for (int x = 0; x < gridNx_; ++x) {
                image.setPixelColor(x, gridNy_ - 1 - y, heatColor(gridValues_[y * gridNx_ + x]));
            }
        }
        const QRectF dataRect = fittedDataRect(available);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(dataRect, image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor("#222222"), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(dataRect);
    }

    void drawPolygonPreview(QPainter& painter, const QRectF& available) const {
        const QRectF dataRect = fittedDataRect(available);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(polygonColor_, qMax(1.2, polygonLineWidth_ * 42.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        for (const QPolygonF& ring : polygonRings_) {
            QPolygonF mapped;
            mapped.reserve(ring.size());
            for (const QPointF& point : ring) {
                mapped << mapDataPoint(point, dataRect);
            }
            painter.drawPolygon(mapped);
        }
    }

    double zoom_ = 1.0;
    bool panMode_ = false;
    ViewMode viewMode_ = ViewMode::Overlay;
    QString database_;
    QString project_;
    QString grid_;
    QString output_;
    QString preset_ = QString::fromUtf8("防黏连增强");
    QString traceMode_ = "ridge-belt";
    QString thresholdMode_ = "percentile";
    QString activity_ = QString::fromUtf8("等待运行");
    double percentile_ = 0.98;
    double threshold_ = 0.95;
    double minArea_ = 120.0;
    QColor polygonColor_ = QColor("#00FFFF");
    double polygonLineWidth_ = 0.03;
    int gridNx_ = 0;
    int gridNy_ = 0;
    QVector<double> gridValues_;
    double gridMin_ = 0.0;
    double gridMax_ = 1.0;
    double gridXMin_ = 0.0;
    double gridYMin_ = 0.0;
    double gridXMax_ = 1.0;
    double gridYMax_ = 1.0;
    QVector<QPolygonF> polygonRings_;
    std::function<void(const QPointF&)> coordinateCallback_;
};

class ColorBarWidget : public QWidget {
public:
    explicit ColorBarWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedWidth(56);
        setMinimumHeight(360);
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
        painter.drawText(QRect(4, 8, 48, 28), Qt::AlignCenter | Qt::TextWordWrap, QString::fromUtf8("属性\n1.0"));

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
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowMinimizeButtonHint |
                   Qt::WindowCloseButtonHint);
    buildUi();
    loadSettings();
    ensureProductDefaults();
    updateRunMode();
    updateStatusContext();

    connect(process_, &QProcess::readyReadStandardOutput, this, &FaultTraceDialog::onReadyRead);
    connect(process_, &QProcess::readyReadStandardError, this, &FaultTraceDialog::onReadyRead);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FaultTraceDialog::onProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, &FaultTraceDialog::onProcessError);
}

void FaultTraceDialog::buildUi() {
    setWindowTitle(QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪"));
    resize(980, 720);

    runModeCombo_ = new QComboBox(this);
    runModeCombo_->addItem(QString::fromUtf8("独立文件模式"));
    runModeCombo_->addItem(QString::fromUtf8("GeoEast 集成模式"));

    coreRunnerEdit_ = new QLineEdit(defaultCoreRunnerPath(), this);
    QPushButton* browseCoreRunnerButton = new QPushButton(QString::fromUtf8("浏览"), this);
    connect(browseCoreRunnerButton, &QPushButton::clicked, this, &FaultTraceDialog::browseCoreRunner);

    runnerEdit_ = new QLineEdit(defaultRunnerPath(), this);
    QPushButton* browseRunnerButton = new QPushButton(QString::fromUtf8("浏览"), this);
    connect(browseRunnerButton, &QPushButton::clicked, this, &FaultTraceDialog::browseRunner);

    geoEastRootEdit_ = new QLineEdit(defaultGeoEastRootPath(), this);
    geoEastDataRootEdit_ = new QLineEdit(defaultGeoEastDataRootPath(), this);
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
    projectEdit_ = new QLineEdit(envOrDefault("FPT_GEOEAST_PROJECT", ""), this);
    userEdit_ = new QLineEdit(defaultUserName(), this);
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

    standaloneInputEdit_ = new QLineEdit(defaultStandaloneInputPath(), this);
    standaloneInputEdit_->setPlaceholderText(QString::fromUtf8("选择外部网格文件，支持矩阵 TXT/DAT 或 GeoEast DAT"));
    QPushButton* browseStandaloneInputButton = new QPushButton(QString::fromUtf8("浏览"), this);
    connect(browseStandaloneInputButton, &QPushButton::clicked, this, &FaultTraceDialog::browseStandaloneInput);
    standaloneOutputDirEdit_ = new QLineEdit(defaultStandaloneOutputDir(), this);
    QPushButton* browseStandaloneOutputButton = new QPushButton(QString::fromUtf8("浏览"), this);
    connect(browseStandaloneOutputButton, &QPushButton::clicked, this, &FaultTraceDialog::browseStandaloneOutputDir);
    standaloneOutputNameEdit_ = new QLineEdit("FaultPolyTrace_Result", this);
    inputFormatCombo_ = new QComboBox(this);
    inputFormatCombo_->addItem("auto");
    inputFormatCombo_->addItem("matrix");
    inputFormatCombo_->addItem("geoeast-dat");
    previewModeCombo_ = new QComboBox(this);
    previewModeCombo_->addItem(QString::fromUtf8("叠加视图"));
    previewModeCombo_->addItem(QString::fromUtf8("原始网格"));
    previewModeCombo_->addItem(QString::fromUtf8("结果多边形"));

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
        setMinimumSize(1024, 680);
        resize(1280, 760);

        connectionBox->hide();
        dataBox->hide();
        paramsBox->hide();

        polygonColorButton_->setText("v");
        polygonColorButton_->setToolTip(QString::fromUtf8("选择结果多边形颜色"));
        polygonColorButton_->setFixedWidth(24);
        polygonLineWidthSpin_->setFixedWidth(64);
        browseDatButton->setText(QString::fromUtf8("浏览"));
        browseDatButton->setFixedWidth(52);
        refreshButton_->setText(QString::fromUtf8("刷新网格"));
        refreshButton_->setToolTip(QString::fromUtf8("从 GeoEast 数据库刷新 BOMapGrid 列表"));
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
            const QString path = QDir(repoRootFromAppDir()).absoluteFilePath(relativePath);
            if (!QFileInfo::exists(path)) {
                appendLog(QString::fromUtf8("文件不存在：%1").arg(QDir::toNativeSeparators(path)));
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
        QAction* configAction = geoEastMenu->addAction(makeToolIcon(ToolIconKind::Database), QString::fromUtf8("数据库与环境底层配置..."));
        connect(configAction, &QAction::triggered, this, &FaultTraceDialog::showDatabaseConfigDialog);
        refreshGridAction_ = geoEastMenu->addAction(makeToolIcon(ToolIconKind::Database), QString::fromUtf8("刷新网格列表"));
        connect(refreshGridAction_, &QAction::triggered, this, &FaultTraceDialog::refreshGrids);
        geoEastMenu->addSeparator();
        QAction* exitAction = geoEastMenu->addAction(QString::fromUtf8("关闭启动器"));
        connect(exitAction, &QAction::triggered, this, &FaultTraceDialog::close);

        QMenu* fileMenu = menuBar->addMenu(QString::fromUtf8("文件(F)"));
        QAction* openAction = fileMenu->addAction(makeToolIcon(ToolIconKind::Open), QString::fromUtf8("打开输入数据..."));
        connect(openAction, &QAction::triggered, this, &FaultTraceDialog::browseInputData);
        QAction* saveAction = fileMenu->addAction(makeToolIcon(ToolIconKind::Save), QString::fromUtf8("保存当前配置"));
        connect(saveAction, &QAction::triggered, this, &FaultTraceDialog::saveCurrentPreset);
        QAction* saveTemplateFileAction = fileMenu->addAction(QString::fromUtf8("导出参数模板..."));
        connect(saveTemplateFileAction, &QAction::triggered, this, &FaultTraceDialog::saveParameterTemplate);
        QAction* loadTemplateFileAction = fileMenu->addAction(QString::fromUtf8("导入参数模板..."));
        connect(loadTemplateFileAction, &QAction::triggered, this, &FaultTraceDialog::loadParameterTemplate);
        QAction* outputDirAction = fileMenu->addAction(QString::fromUtf8("打开输出目录"));
        connect(outputDirAction, &QAction::triggered, this, [this, openLocalDir]() {
            if (runModeCombo_ && runModeCombo_->currentIndex() == 0 && !standaloneOutputDirEdit_->text().trimmed().isEmpty()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(standaloneOutputDirEdit_->text().trimmed()));
            } else {
                openLocalDir("examples/outputs");
            }
        });
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
        connect(resetViewAction, &QAction::triggered, this, [this, renderView, panAction]() {
            if (renderView()) {
                renderView()->resetView();
            }
            panAction->setChecked(false);
            appendStatus(QString::fromUtf8("视图区已重置"));
        });
        QAction* refreshPreviewAction = viewMenu->addAction(QString::fromUtf8("刷新输入预览"));
        connect(refreshPreviewAction, &QAction::triggered, this, &FaultTraceDialog::refreshInputPreview);

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
        QAction* restoreDefaultAction = toolMenu->addAction(makeToolIcon(ToolIconKind::Reset), QString::fromUtf8("恢复默认配置"));
        connect(restoreDefaultAction, &QAction::triggered, this, &FaultTraceDialog::restoreDefaultConfiguration);
        QAction* userManualToolAction = toolMenu->addAction(QString::fromUtf8("打开用户操作手册"));
        connect(userManualToolAction, &QAction::triggered, this, [openLocalFile]() { openLocalFile("docs/用户操作手册.md"); });
        QAction* packageGuideAction = toolMenu->addAction(QString::fromUtf8("打开异机运行说明"));
        connect(packageGuideAction, &QAction::triggered, this, [openLocalFile]() { openLocalFile("docs/评审交付与异机运行说明.md"); });

        QMenu* helpMenu = menuBar->addMenu(QString::fromUtf8("帮助(H)"));
        QAction* helpAction = helpMenu->addAction(makeToolIcon(ToolIconKind::Help), QString::fromUtf8("帮助提示"));
        connect(helpAction, &QAction::triggered, this, &FaultTraceDialog::showHelpHint);
        QAction* manualAction = helpMenu->addAction(QString::fromUtf8("用户操作手册"));
        connect(manualAction, &QAction::triggered, this, [openLocalFile]() { openLocalFile("docs/用户操作手册.md"); });
        QAction* aboutAction = helpMenu->addAction(QString::fromUtf8("关于 FaultPolyTrace"));
        connect(aboutAction, &QAction::triggered, this, [this]() {
            QMessageBox::information(this, QString::fromUtf8("关于 FaultPolyTrace"),
                                     QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪\n\n"
                                                       "定位：平面断层属性到闭合断层多边形的自动追踪工具。\n"
                                                       "入口：可独立运行，也可通过 GeoEast 菜单或 Qt 启动器调用 GeoEast runner。"));
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
        toolbar->addWidget(refreshButton_);
        toolbar->addAction(savePresetAction);
        QWidget* toolbarSpacer = new QWidget(this);
        toolbarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        toolbar->addWidget(toolbarSpacer);
        toolbar->addAction(helpAction);

        QFrame* sidebar = new QFrame(this);
        sidebar->setObjectName("sidebar");
        sidebar->setFixedWidth(345);
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
        addFormRow(dataLayoutNew, 0, QString::fromUtf8("运行模式"), runModeCombo_, dataGroup);
        QWidget* coreRunnerLineWidget = new QWidget(dataGroup);
        QHBoxLayout* coreRunnerLineLayout = new QHBoxLayout(coreRunnerLineWidget);
        coreRunnerLineLayout->setContentsMargins(0, 0, 0, 0);
        coreRunnerLineLayout->setSpacing(5);
        coreRunnerLineLayout->addWidget(coreRunnerEdit_, 1);
        coreRunnerLineLayout->addWidget(browseCoreRunnerButton);
        browseCoreRunnerButton->setFixedWidth(52);
        addFormRow(dataLayoutNew, 1, QString::fromUtf8("算法程序"), coreRunnerLineWidget, dataGroup);
        addFormRow(dataLayoutNew, 2, QString::fromUtf8("输入网格"), gridCombo_, dataGroup);
        addFormRow(dataLayoutNew, 3, QString::fromUtf8("输出多边形"), outputEdit_, dataGroup);
        QWidget* colorLineWidget = new QWidget(dataGroup);
        QHBoxLayout* colorLineLayout = new QHBoxLayout(colorLineWidget);
        colorLineLayout->setContentsMargins(0, 0, 0, 0);
        colorLineLayout->setSpacing(5);
        colorLineLayout->addWidget(polygonColorEdit_, 1);
        colorLineLayout->addWidget(polygonColorButton_);
        colorLineLayout->addWidget(polygonLineWidthSpin_);
        addFormRow(dataLayoutNew, 4, QString::fromUtf8("结果样式 / 线宽"), colorLineWidget, dataGroup);
        QWidget* datLineWidget = new QWidget(dataGroup);
        QHBoxLayout* datLineLayout = new QHBoxLayout(datLineWidget);
        datLineLayout->setContentsMargins(0, 0, 0, 0);
        datLineLayout->setSpacing(5);
        datLineLayout->addWidget(datFileEdit_, 1);
        datLineLayout->addWidget(browseDatButton);
        addFormRow(dataLayoutNew, 5, QString::fromUtf8("DAT 文件"), datLineWidget, dataGroup);
        QWidget* inputLineWidget = new QWidget(dataGroup);
        QHBoxLayout* inputLineLayout = new QHBoxLayout(inputLineWidget);
        inputLineLayout->setContentsMargins(0, 0, 0, 0);
        inputLineLayout->setSpacing(5);
        inputLineLayout->addWidget(standaloneInputEdit_, 1);
        inputLineLayout->addWidget(browseStandaloneInputButton);
        browseStandaloneInputButton->setFixedWidth(52);
        addFormRow(dataLayoutNew, 6, QString::fromUtf8("外部文件"), inputLineWidget, dataGroup);
        QWidget* outLineWidget = new QWidget(dataGroup);
        QHBoxLayout* outLineLayout = new QHBoxLayout(outLineWidget);
        outLineLayout->setContentsMargins(0, 0, 0, 0);
        outLineLayout->setSpacing(5);
        outLineLayout->addWidget(standaloneOutputDirEdit_, 1);
        outLineLayout->addWidget(browseStandaloneOutputButton);
        browseStandaloneOutputButton->setFixedWidth(52);
        addFormRow(dataLayoutNew, 7, QString::fromUtf8("输出目录"), outLineWidget, dataGroup);
        addFormRow(dataLayoutNew, 8, QString::fromUtf8("输出名称"), standaloneOutputNameEdit_, dataGroup);
        addFormRow(dataLayoutNew, 9, QString::fromUtf8("输入格式"), inputFormatCombo_, dataGroup);
        addFormRow(dataLayoutNew, 10, QString::fromUtf8("视图内容"), previewModeCombo_, dataGroup);
        dataLayoutNew->addWidget(overwriteGridCheck_, 11, 0, 1, 2);
        dataLayoutNew->addWidget(importDatCheck_, 12, 0, 1, 2);
        sidebarContentLayout->addWidget(dataGroup);

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
                coordinateLabel_->setText(QString::fromUtf8("视图 X: --   |   Y: --"));
                return;
            }
            coordinateLabel_->setText(QString::fromUtf8("视图 X: %1   |   Y: %2")
                                          .arg(point.x(), 0, 'f', 1)
                                          .arg(point.y(), 0, 'f', 1));
        });
        canvasLayout->addWidget(renderViewWidget, 1);
        canvasLayout->addWidget(new ColorBarWidget(canvasRow));
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

        QHBoxLayout* mainFramework = new QHBoxLayout;
        mainFramework->setContentsMargins(0, 0, 0, 0);
        mainFramework->setSpacing(0);
        mainFramework->addWidget(sidebar);
        mainFramework->addWidget(workspace, 1);

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
        elapsedLabel_ = new QLabel(QString::fromUtf8("耗时: --"), statusBar);
        statusLabel_ = new QLabel(QString::fromUtf8("就绪"), statusBar);
        for (QLabel* label : {coordinateLabel_, connectionLabel_, projectStatusLabel_, gridStatusLabel_, elapsedLabel_, statusLabel_}) {
            label->setObjectName("statusText");
        }
        statusLayout->addWidget(coordinateLabel_);
        statusLayout->addStretch(1);
        statusLayout->addWidget(connectionLabel_);
        statusLayout->addWidget(projectStatusLabel_);
        statusLayout->addWidget(gridStatusLabel_);
        statusLayout->addWidget(elapsedLabel_);
        statusLayout->addWidget(statusLabel_);

        QVBoxLayout* rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);
        rootLayout->addWidget(menuBar);
        rootLayout->addWidget(toolbar);
        rootLayout->addLayout(mainFramework, 1);
        rootLayout->addWidget(statusBar);

        connect(projectEdit_, &QLineEdit::textChanged, this, &FaultTraceDialog::updateStatusContext);
        connect(databaseEdit_, &QLineEdit::textChanged, this, &FaultTraceDialog::updateStatusContext);
        connect(gridCombo_, &QComboBox::currentTextChanged, this, &FaultTraceDialog::updateStatusContext);
        connect(outputEdit_, &QLineEdit::textChanged, this, &FaultTraceDialog::updateStatusContext);
        connect(runModeCombo_, &QComboBox::currentTextChanged, this, &FaultTraceDialog::updateRunMode);
        connect(previewModeCombo_, &QComboBox::currentTextChanged, this, &FaultTraceDialog::updatePreviewMode);
        connect(standaloneInputEdit_, &QLineEdit::editingFinished, this, &FaultTraceDialog::refreshInputPreview);
        connect(inputFormatCombo_, &QComboBox::currentTextChanged, this, &FaultTraceDialog::refreshInputPreview);
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
        appendLog(QString::fromUtf8("加载项目配置：%1 成功。").arg(projectEdit_->text()));
    }
}

QString FaultTraceDialog::defaultRunnerPath() const {
    const QString fromEnv = envOrDefault("FPT_GEOEAST_RUNNER", "");
    if (!fromEnv.isEmpty()) {
        return QDir::toNativeSeparators(fromEnv);
    }
    QDir appDir(QCoreApplication::applicationDirPath());
    const QString localRunner = appDir.absoluteFilePath("faultpolytrace_geoeast_runner.exe");
    if (QFileInfo::exists(localRunner)) {
        return QDir::toNativeSeparators(localRunner);
    }
    QDir parentDir(appDir);
    if (parentDir.cdUp()) {
        const QString parentRunner = parentDir.absoluteFilePath("faultpolytrace_geoeast_runner.exe");
        if (QFileInfo::exists(parentRunner)) {
            return QDir::toNativeSeparators(parentRunner);
        }
    }
    QDir root(repoRootFromAppDir());
    const QString candidate = root.absoluteFilePath("geoeast_plugin/bo_runner/faultpolytrace_geoeast_runner.exe");
    return QFileInfo::exists(candidate) ? QDir::toNativeSeparators(candidate) : candidate;
}

QString FaultTraceDialog::defaultOfficialDatPath() const {
    const QString fromEnv = envOrDefault("FPT_OFFICIAL_DAT", "");
    if (!fromEnv.isEmpty()) {
        return QDir::toNativeSeparators(fromEnv);
    }
    return defaultStandaloneInputPath();
}

QString FaultTraceDialog::defaultCoreRunnerPath() const {
    const QString fromEnv = envOrDefault("FPT_CORE_RUNNER", "");
    if (!fromEnv.isEmpty()) {
        return QDir::toNativeSeparators(fromEnv);
    }
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString appLocal = appDir.absoluteFilePath("faultpolytrace.exe");
    QDir parentDir(appDir);
    QString parentBin;
    if (parentDir.cdUp()) {
        parentBin = parentDir.absoluteFilePath("bin/faultpolytrace.exe");
    }
    const QDir root(repoRootFromAppDir());
    return firstExistingFile({
        appLocal,
        parentBin,
        root.absoluteFilePath("build/faultpolytrace.exe"),
        root.absoluteFilePath("bin/faultpolytrace.exe")
    }, root.absoluteFilePath("build/faultpolytrace.exe"));
}

QString FaultTraceDialog::defaultStandaloneInputPath() const {
    const QString fromEnv = envOrDefault("FPT_INPUT", "");
    if (!fromEnv.isEmpty()) {
        return QDir::toNativeSeparators(fromEnv);
    }
    const QDir appDir(QCoreApplication::applicationDirPath());
    QDir parentDir(appDir);
    QString packagedExample;
    if (parentDir.cdUp()) {
        packagedExample = parentDir.absoluteFilePath("examples/synthetic/broken_ridge_fault_grid.txt");
    }
    const QDir root(repoRootFromAppDir());
    return firstExistingFile({
        packagedExample,
        root.absoluteFilePath("examples/synthetic/broken_ridge_fault_grid.txt"),
        root.absoluteFilePath("examples/synthetic/ridge_fault_grid.txt")
    });
}

QString FaultTraceDialog::defaultStandaloneOutputDir() const {
    const QString fromEnv = envOrDefault("FPT_OUTPUT_DIR", "");
    if (!fromEnv.isEmpty()) {
        QDir().mkpath(fromEnv);
        return QDir::toNativeSeparators(fromEnv);
    }
    QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (docs.isEmpty()) {
        docs = QCoreApplication::applicationDirPath();
    }
    const QString dir = QDir(docs).absoluteFilePath("FaultPolyTraceOutputs");
    QDir().mkpath(dir);
    return QDir::toNativeSeparators(dir);
}

void FaultTraceDialog::browseRunner() {
    const QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("选择 GeoEast Runner"), QFileInfo(runnerEdit_->text()).absolutePath(),
        QString::fromUtf8("可执行文件 (*.exe);;所有文件 (*.*)"));
    if (!path.isEmpty()) {
        runnerEdit_->setText(QDir::toNativeSeparators(path));
    }
}

void FaultTraceDialog::browseCoreRunner() {
    const QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("选择独立算法程序"), QFileInfo(coreRunnerEdit_->text()).absolutePath(),
        QString::fromUtf8("可执行文件 (*.exe);;所有文件 (*.*)"));
    if (!path.isEmpty()) {
        coreRunnerEdit_->setText(QDir::toNativeSeparators(path));
    }
}

void FaultTraceDialog::browseInputData() {
    if (runModeCombo_ && runModeCombo_->currentIndex() == 0) {
        browseStandaloneInput();
    } else {
        browseDatFile();
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

void FaultTraceDialog::browseStandaloneInput() {
    const QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("选择外部平面断层属性网格"), QFileInfo(standaloneInputEdit_->text()).absolutePath(),
        QString::fromUtf8("网格文件 (*.txt *.dat *.csv);;所有文件 (*.*)"));
    if (!path.isEmpty()) {
        standaloneInputEdit_->setText(QDir::toNativeSeparators(path));
        refreshInputPreview();
    }
}

void FaultTraceDialog::browseStandaloneOutputDir() {
    const QString path = QFileDialog::getExistingDirectory(
        this, QString::fromUtf8("选择独立软件输出目录"), standaloneOutputDirEdit_->text());
    if (!path.isEmpty()) {
        standaloneOutputDirEdit_->setText(QDir::toNativeSeparators(path));
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
    dialog.setWindowTitle(QString::fromUtf8("GeoEast 环境与数据库底层配置"));
    dialog.setModal(true);
    dialog.resize(600, 324);
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
            padding: 8px 15px;
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
    QLabel* header = new QLabel(QString::fromUtf8("GeoEast 环境与数据库底层配置"), &dialog);
    header->setObjectName("dialogHeader");
    header->setFixedHeight(34);
    root->addWidget(header);

    QWidget* body = new QWidget(&dialog);
    QGridLayout* bodyLayout = new QGridLayout(body);
    bodyLayout->setContentsMargins(15, 15, 15, 12);
    bodyLayout->setHorizontalSpacing(8);
    bodyLayout->setVerticalSpacing(8);

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

    addPathRow(0, QString::fromUtf8("Runner 可执行文件:"), runnerCopy, QString::fromUtf8("选择 GeoEast Runner"), false);
    addPathRow(1, QString::fromUtf8("GeoEast 根目录:"), geoEastRootCopy, QString::fromUtf8("选择 GeoEast 安装根目录"), true);
    addPathRow(2, QString::fromUtf8("GeoEast 数据根目录:"), dataRootCopy, QString::fromUtf8("选择 GeoEastData 根目录"), true);

    QFrame* line = new QFrame(body);
    line->setObjectName("line");
    bodyLayout->addWidget(line, 3, 0, 1, 5);

    bodyLayout->addWidget(new QLabel(QString::fromUtf8("PG 主机:"), body), 4, 0);
    bodyLayout->addWidget(hostCopy, 4, 1);
    bodyLayout->addWidget(new QLabel(QString::fromUtf8("端口:"), body), 4, 2);
    bodyLayout->addWidget(portCopy, 4, 3, 1, 2);
    bodyLayout->addWidget(new QLabel(QString::fromUtf8("数据库:"), body), 5, 0);
    bodyLayout->addWidget(databaseCopy, 5, 1);
    bodyLayout->addWidget(new QLabel(QString::fromUtf8("项目:"), body), 5, 2);
    bodyLayout->addWidget(projectCopy, 5, 3, 1, 2);
    bodyLayout->addWidget(new QLabel(QString::fromUtf8("用户:"), body), 6, 0);
    bodyLayout->addWidget(userCopy, 6, 1);
    bodyLayout->addWidget(new QLabel(QString::fromUtf8("密码:"), body), 6, 2);
    bodyLayout->addWidget(passwordCopy, 6, 3, 1, 2);
    bodyLayout->setColumnStretch(1, 1);
    bodyLayout->setColumnStretch(3, 1);
    root->addWidget(body, 1);

    QFrame* footer = new QFrame(&dialog);
    footer->setStyleSheet("QFrame { background: #EAEAEA; border-top: 1px solid #C0C0C0; }");
    QHBoxLayout* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(15, 10, 15, 10);
    footerLayout->addStretch(1);
    QPushButton* cancelButton = new QPushButton(QString::fromUtf8("取消"), footer);
    QPushButton* applyButton = new QPushButton(QString::fromUtf8("应用并保存配置"), footer);
    applyButton->setObjectName("applyButton");
    footerLayout->addWidget(cancelButton);
    footerLayout->addWidget(applyButton);
    root->addWidget(footer);

    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(applyButton, &QPushButton::clicked, &dialog, [&]() {
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
        appendLog(QString::fromUtf8("底层配置已更新并保存。"));
        dialog.accept();
    });

    dialog.exec();
}

void FaultTraceDialog::saveCurrentPreset() {
    saveSettings();
    updateStatusContext();
    appendLog(QString::fromUtf8("当前参数和界面配置已保存：%1").arg(QDir::toNativeSeparators(settingsFilePath())));
}

QJsonObject FaultTraceDialog::parameterTemplateJson() const {
    QJsonObject object;
    object["version"] = "1.0";
    object["preset"] = presetCombo_->currentText();
    object["traceMode"] = traceModeCombo_->currentText();
    object["thresholdMode"] = thresholdModeCombo_->currentText();
    object["threshold"] = thresholdSpin_->value();
    object["percentile"] = percentileSpin_->value();
    object["minArea"] = minAreaSpin_->value();
    object["closeRadius"] = closeRadiusSpin_->value();
    object["weakPercentile"] = weakPercentileSpin_->value();
    object["weakConnectRadius"] = weakConnectRadiusSpin_->value();
    object["beltRadius"] = beltRadiusSpin_->value();
    object["postBeltCloseRadius"] = postBeltCloseRadiusSpin_->value();
    object["attributeConstrainedBelt"] = attributeConstrainedBeltCheck_->isChecked();
    object["attributeEnvelopeRadius"] = attributeEnvelopeRadiusSpin_->value();
    object["minBranchLength"] = minBranchLengthSpin_->value();
    object["minJunctionBranchLength"] = minJunctionBranchLengthSpin_->value();
    object["minJunctionBranchMean"] = minJunctionBranchMeanSpin_->value();
    object["maxGapDistance"] = maxGapDistanceSpin_->value();
    object["maxGapAngle"] = maxGapAngleSpin_->value();
    object["rootAntiMerge"] = rootAntiMergeCheck_->isChecked();
    object["rootAntiMergeRadius"] = rootAntiMergeRadiusSpin_->value();
    object["rootAntiMergeSupportRatio"] = rootAntiMergeSupportRatioSpin_->value();
    object["rootAntiMergeMinPartCells"] = rootAntiMergeMinPartCellsSpin_->value();
    object["rootAntiMergeMinPartRatio"] = rootAntiMergeMinPartRatioSpin_->value();
    object["rootAntiMergeWeakGuardThreshold"] = rootAntiMergeWeakGuardThresholdSpin_->value();
    object["rootAntiMergeWeakGuardRadius"] = rootAntiMergeWeakGuardRadiusSpin_->value();
    object["localAntiMerge"] = localAntiMergeCheck_->isChecked();
    object["localAntiMergeLowThreshold"] = localAntiMergeLowThresholdSpin_->value();
    object["localAntiMergeBridgeRadius"] = localAntiMergeBridgeRadiusSpin_->value();
    object["localAntiMergeMinPartCells"] = localAntiMergeMinPartCellsSpin_->value();
    object["localAntiMergeMinPartRatio"] = localAntiMergeMinPartRatioSpin_->value();
    object["localAntiMergeMinBridgeCells"] = localAntiMergeMinBridgeCellsSpin_->value();
    object["localAntiMergeMinLowRatio"] = localAntiMergeMinLowRatioSpin_->value();
    object["simplifyTolerance"] = simplifyToleranceSpin_->value();
    object["smoothIterations"] = smoothIterationsSpin_->value();
    object["residualTrace"] = residualTraceCheck_->isChecked();
    object["residualTraceMinLength"] = residualTraceMinLengthSpin_->value();
    object["residualTraceMaxCells"] = residualTraceMaxCellsSpin_->value();
    return object;
}

void FaultTraceDialog::applyParameterTemplateJson(const QJsonObject& object) {
    auto setDouble = [&](const char* key, QDoubleSpinBox* widget) {
        if (object.contains(key) && widget) {
            widget->setValue(object.value(key).toDouble(widget->value()));
        }
    };
    auto setInt = [&](const char* key, QSpinBox* widget) {
        if (object.contains(key) && widget) {
            widget->setValue(object.value(key).toInt(widget->value()));
        }
    };
    auto setBool = [&](const char* key, QCheckBox* widget) {
        if (object.contains(key) && widget) {
            widget->setChecked(object.value(key).toBool(widget->isChecked()));
        }
    };
    if (object.contains("preset")) {
        presetCombo_->setCurrentText(object.value("preset").toString(presetCombo_->currentText()));
    }
    if (object.contains("traceMode")) {
        traceModeCombo_->setCurrentText(object.value("traceMode").toString(traceModeCombo_->currentText()));
    }
    if (object.contains("thresholdMode")) {
        thresholdModeCombo_->setCurrentText(object.value("thresholdMode").toString(thresholdModeCombo_->currentText()));
    }
    setDouble("threshold", thresholdSpin_);
    setDouble("percentile", percentileSpin_);
    setDouble("minArea", minAreaSpin_);
    setInt("closeRadius", closeRadiusSpin_);
    setDouble("weakPercentile", weakPercentileSpin_);
    setInt("weakConnectRadius", weakConnectRadiusSpin_);
    setInt("beltRadius", beltRadiusSpin_);
    setInt("postBeltCloseRadius", postBeltCloseRadiusSpin_);
    setBool("attributeConstrainedBelt", attributeConstrainedBeltCheck_);
    setInt("attributeEnvelopeRadius", attributeEnvelopeRadiusSpin_);
    setInt("minBranchLength", minBranchLengthSpin_);
    setInt("minJunctionBranchLength", minJunctionBranchLengthSpin_);
    setDouble("minJunctionBranchMean", minJunctionBranchMeanSpin_);
    setInt("maxGapDistance", maxGapDistanceSpin_);
    setDouble("maxGapAngle", maxGapAngleSpin_);
    setBool("rootAntiMerge", rootAntiMergeCheck_);
    setInt("rootAntiMergeRadius", rootAntiMergeRadiusSpin_);
    setDouble("rootAntiMergeSupportRatio", rootAntiMergeSupportRatioSpin_);
    setInt("rootAntiMergeMinPartCells", rootAntiMergeMinPartCellsSpin_);
    setDouble("rootAntiMergeMinPartRatio", rootAntiMergeMinPartRatioSpin_);
    setDouble("rootAntiMergeWeakGuardThreshold", rootAntiMergeWeakGuardThresholdSpin_);
    setInt("rootAntiMergeWeakGuardRadius", rootAntiMergeWeakGuardRadiusSpin_);
    setBool("localAntiMerge", localAntiMergeCheck_);
    setDouble("localAntiMergeLowThreshold", localAntiMergeLowThresholdSpin_);
    setInt("localAntiMergeBridgeRadius", localAntiMergeBridgeRadiusSpin_);
    setInt("localAntiMergeMinPartCells", localAntiMergeMinPartCellsSpin_);
    setDouble("localAntiMergeMinPartRatio", localAntiMergeMinPartRatioSpin_);
    setInt("localAntiMergeMinBridgeCells", localAntiMergeMinBridgeCellsSpin_);
    setDouble("localAntiMergeMinLowRatio", localAntiMergeMinLowRatioSpin_);
    setDouble("simplifyTolerance", simplifyToleranceSpin_);
    setInt("smoothIterations", smoothIterationsSpin_);
    setBool("residualTrace", residualTraceCheck_);
    setInt("residualTraceMinLength", residualTraceMinLengthSpin_);
    setInt("residualTraceMaxCells", residualTraceMaxCellsSpin_);
    updateOptionControls();
    updateStatusContext();
}

void FaultTraceDialog::saveParameterTemplate() {
    const QString path = QFileDialog::getSaveFileName(
        this, QString::fromUtf8("导出参数模板"), QDir(defaultStandaloneOutputDir()).absoluteFilePath("FaultPolyTrace_参数模板.json"),
        QString::fromUtf8("参数模板 (*.json);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QString::fromUtf8("无法保存模板"), path);
        return;
    }
    file.write(QJsonDocument(parameterTemplateJson()).toJson(QJsonDocument::Indented));
    appendLog(QString::fromUtf8("参数模板已导出：%1").arg(QDir::toNativeSeparators(path)));
}

void FaultTraceDialog::loadParameterTemplate() {
    const QString path = QFileDialog::getOpenFileName(
        this, QString::fromUtf8("导入参数模板"), defaultStandaloneOutputDir(),
        QString::fromUtf8("参数模板 (*.json);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QString::fromUtf8("无法读取模板"), path);
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        QMessageBox::warning(this, QString::fromUtf8("模板格式错误"), QString::fromUtf8("请选择 FaultPolyTrace 参数模板 JSON 文件。"));
        return;
    }
    applyParameterTemplateJson(document.object());
    saveSettings();
    appendLog(QString::fromUtf8("参数模板已导入：%1").arg(QDir::toNativeSeparators(path)));
}

void FaultTraceDialog::restoreDefaultConfiguration() {
    runModeCombo_->setCurrentText(QString::fromUtf8("独立文件模式"));
    coreRunnerEdit_->setText(defaultCoreRunnerPath());
    runnerEdit_->setText(defaultRunnerPath());
    geoEastRootEdit_->setText(defaultGeoEastRootPath());
    geoEastDataRootEdit_->setText(defaultGeoEastDataRootPath());
    databaseEdit_->setText("ndp");
    projectEdit_->setText("");
    userEdit_->setText(defaultUserName());
    gridCombo_->setCurrentText("test7_fault_attribute_bo");
    outputEdit_->setText("FaultPolyTrace_Result");
    standaloneInputEdit_->setText(defaultStandaloneInputPath());
    standaloneOutputDirEdit_->setText(defaultStandaloneOutputDir());
    standaloneOutputNameEdit_->setText("FaultPolyTrace_Result");
    inputFormatCombo_->setCurrentText("auto");
    previewModeCombo_->setCurrentText(QString::fromUtf8("叠加视图"));
    setPolygonColor(QColor("#00FFFF"));
    polygonLineWidthSpin_->setValue(0.03);
    presetCombo_->setCurrentText(QString::fromUtf8("防黏连增强"));
    applyParameterPreset(QString::fromUtf8("防黏连增强"));
    saveSettings();
    updateRunMode();
    refreshInputPreview();
    appendLog(QString::fromUtf8("已恢复默认配置。"));
}

void FaultTraceDialog::showHelpHint() {
    appendLog(QString::fromUtf8("帮助文档：docs/用户操作手册.md；异机交付说明：docs/评审交付与异机运行说明.md。"));
    QMessageBox::information(this,
                             QString::fromUtf8("FaultPolyTrace 帮助提示"),
                             QString::fromUtf8("常用流程：\n"
                                               "1. 在【数据库与环境底层配置】中确认 GeoEast 根目录、项目、用户和 Runner。\n"
                                               "2. 点击【刷新网格】读取 GeoEast 中的 BOMapGrid 对象。\n"
                                               "3. 设置输入网格、输出多边形、结果颜色和必要参数。\n"
                                               "4. 点击【开始追踪】，完成后到 GeoEast 底图叠加查看结果。\n\n"
                                               "详细说明见 docs/用户操作手册.md 和 docs/评审交付与异机运行说明.md。"));
}

QString FaultTraceDialog::settingsFilePath() const {
    return QDir(productConfigDir()).absoluteFilePath("faultpolytrace_launcher.ini");
}

void FaultTraceDialog::loadSettings() {
    const QString newPath = settingsFilePath();
    const QString legacyPath = QCoreApplication::applicationDirPath() + "/faultpolytrace_launcher.ini";
    const QString readPath = QFileInfo::exists(newPath) ? newPath : legacyPath;
    QSettings settings(readPath, QSettings::IniFormat);
    runModeCombo_->setCurrentText(settings.value("runMode", runModeCombo_->currentText()).toString());
    coreRunnerEdit_->setText(settings.value("coreRunner", coreRunnerEdit_->text()).toString());
    runnerEdit_->setText(settings.value("runner", runnerEdit_->text()).toString());
    geoEastRootEdit_->setText(settings.value("geoEastRoot", geoEastRootEdit_->text()).toString());
    geoEastDataRootEdit_->setText(settings.value("geoEastDataRoot", geoEastDataRootEdit_->text()).toString());
    pgHostEdit_->setText(settings.value("pgHost", pgHostEdit_->text()).toString());
    pgPortEdit_->setText(settings.value("pgPort", pgPortEdit_->text()).toString());
    databaseEdit_->setText(settings.value("database", databaseEdit_->text()).toString());
    projectEdit_->setText(settings.value("project", projectEdit_->text()).toString());
    userEdit_->setText(settings.value("user", userEdit_->text()).toString());
    presetCombo_->setCurrentText(settings.value("preset", presetCombo_->currentText()).toString());
    gridCombo_->setCurrentText(settings.value("grid", gridCombo_->currentText()).toString());
    outputEdit_->setText(settings.value("output", outputEdit_->text()).toString());
    datFileEdit_->setText(settings.value("datFile", datFileEdit_->text()).toString());
    standaloneInputEdit_->setText(settings.value("standaloneInput", standaloneInputEdit_->text()).toString());
    standaloneOutputDirEdit_->setText(settings.value("standaloneOutputDir", standaloneOutputDirEdit_->text()).toString());
    standaloneOutputNameEdit_->setText(settings.value("standaloneOutputName", standaloneOutputNameEdit_->text()).toString());
    inputFormatCombo_->setCurrentText(settings.value("inputFormat", inputFormatCombo_->currentText()).toString());
    previewModeCombo_->setCurrentText(settings.value("previewMode", previewModeCombo_->currentText()).toString());
    lastStandaloneOutputCsv_ = settings.value("lastStandaloneOutputCsv", lastStandaloneOutputCsv_).toString();
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
    updateOptionControls();
    updateStatusContext();
}

void FaultTraceDialog::saveSettings() const {
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    settings.setValue("runMode", runModeCombo_->currentText());
    settings.setValue("coreRunner", coreRunnerEdit_->text());
    settings.setValue("runner", runnerEdit_->text());
    settings.setValue("geoEastRoot", geoEastRootEdit_->text());
    settings.setValue("geoEastDataRoot", geoEastDataRootEdit_->text());
    settings.setValue("pgHost", pgHostEdit_->text());
    settings.setValue("pgPort", pgPortEdit_->text());
    settings.setValue("database", databaseEdit_->text());
    settings.setValue("project", projectEdit_->text());
    settings.setValue("user", userEdit_->text());
    settings.setValue("preset", presetCombo_->currentText());
    settings.setValue("grid", gridCombo_->currentText());
    settings.setValue("output", outputEdit_->text());
    settings.setValue("datFile", datFileEdit_->text());
    settings.setValue("standaloneInput", standaloneInputEdit_->text());
    settings.setValue("standaloneOutputDir", standaloneOutputDirEdit_->text());
    settings.setValue("standaloneOutputName", standaloneOutputNameEdit_->text());
    settings.setValue("inputFormat", inputFormatCombo_->currentText());
    settings.setValue("previewMode", previewModeCombo_->currentText());
    settings.setValue("lastStandaloneOutputCsv", lastStandaloneOutputCsv_);
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

    const bool standalone = runModeCombo_ && runModeCombo_->currentIndex() == 0;
    if (standalone) {
        for (QWidget* widget : {
                 static_cast<QWidget*>(gridCombo_),
                 static_cast<QWidget*>(outputEdit_),
                 static_cast<QWidget*>(datFileEdit_),
                 static_cast<QWidget*>(overwriteGridCheck_),
                 static_cast<QWidget*>(importDatCheck_)}) {
            if (widget) {
                widget->setEnabled(false);
            }
        }
    }

    updateStatusContext();
}

void FaultTraceDialog::updateRunMode() {
    const bool standalone = runModeCombo_ && runModeCombo_->currentIndex() == 0;
    for (QWidget* widget : {
             static_cast<QWidget*>(coreRunnerEdit_),
             static_cast<QWidget*>(standaloneInputEdit_),
             static_cast<QWidget*>(standaloneOutputDirEdit_),
             static_cast<QWidget*>(standaloneOutputNameEdit_),
             static_cast<QWidget*>(inputFormatCombo_),
             static_cast<QWidget*>(previewModeCombo_)}) {
        if (widget) {
            widget->setEnabled(standalone);
        }
    }
    for (QWidget* widget : {
             static_cast<QWidget*>(gridCombo_),
             static_cast<QWidget*>(outputEdit_),
             static_cast<QWidget*>(importDatCheck_),
             static_cast<QWidget*>(datFileEdit_),
             static_cast<QWidget*>(overwriteGridCheck_),
             static_cast<QWidget*>(runnerEdit_),
             static_cast<QWidget*>(geoEastRootEdit_),
             static_cast<QWidget*>(geoEastDataRootEdit_),
             static_cast<QWidget*>(pgHostEdit_),
             static_cast<QWidget*>(pgPortEdit_),
             static_cast<QWidget*>(databaseEdit_),
             static_cast<QWidget*>(projectEdit_),
             static_cast<QWidget*>(userEdit_),
             static_cast<QWidget*>(passwordEdit_)}) {
        if (widget) {
            widget->setEnabled(!standalone);
        }
    }
    refreshButton_->setEnabled(!standalone && process_->state() == QProcess::NotRunning);
    updateOptionControls();
    updatePreviewMode();
    if (standalone) {
        refreshInputPreview();
    }
}

void FaultTraceDialog::updatePreviewMode() {
    if (!renderViewWidget_ || !previewModeCombo_) {
        return;
    }
    auto* view = static_cast<RenderViewWidget*>(renderViewWidget_);
    const QString text = previewModeCombo_->currentText();
    if (text == QString::fromUtf8("原始网格")) {
        view->setViewMode(RenderViewWidget::ViewMode::Raw);
    } else if (text == QString::fromUtf8("结果多边形")) {
        view->setViewMode(RenderViewWidget::ViewMode::Result);
    } else {
        view->setViewMode(RenderViewWidget::ViewMode::Overlay);
    }
}

void FaultTraceDialog::refreshInputPreview() {
    if (!renderViewWidget_ || !standaloneInputEdit_) {
        return;
    }
    const QString input = standaloneInputEdit_->text().trimmed();
    if (input.isEmpty() || !QFileInfo::exists(input)) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("请选择可读取的外部网格文件"));
        updateStatusContext();
        return;
    }
    QString error;
    if (static_cast<RenderViewWidget*>(renderViewWidget_)->loadGridFile(input, inputFormatCombo_->currentText(), &error)) {
        appendStatus(QString::fromUtf8("原始网格预览已刷新"));
        loadLatestStandaloneResult();
    } else if (!error.isEmpty()) {
        appendLog(error);
        appendStatus(QString::fromUtf8("原始网格预览失败"));
    }
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

QString FaultTraceDialog::currentStandaloneOutputCsv() const {
    QString outputName = standaloneOutputNameEdit_ ? standaloneOutputNameEdit_->text().trimmed() : QString();
    if (outputName.isEmpty()) {
        outputName = "FaultPolyTrace_Result";
    }
    if (!outputName.endsWith(".csv", Qt::CaseInsensitive)) {
        outputName += ".csv";
    }
    QString outputDir = standaloneOutputDirEdit_ ? standaloneOutputDirEdit_->text().trimmed() : QString();
    if (outputDir.isEmpty()) {
        outputDir = defaultStandaloneOutputDir();
    }
    QDir().mkpath(outputDir);
    return QDir(outputDir).absoluteFilePath(outputName);
}

QStringList FaultTraceDialog::standaloneTraceArgs() const {
    const QString outputCsv = currentStandaloneOutputCsv();
    const QFileInfo outputInfo(outputCsv);
    QStringList args;
    args << "--input" << standaloneInputEdit_->text().trimmed()
         << "--input-format" << inputFormatCombo_->currentText()
         << "--output" << outputCsv
         << "--diagnostics" << outputInfo.absolutePath() + "/" + outputInfo.completeBaseName() + ".diagnostics.json"
         << "--branches-output" << outputInfo.absolutePath() + "/" + outputInfo.completeBaseName() + ".branches.csv"
         << "--topology-output" << outputInfo.absolutePath() + "/" + outputInfo.completeBaseName() + ".topology.csv"
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
         << "--min-area" << QString::number(minAreaSpin_->value(), 'g', 12);
    args << (attributeConstrainedBeltCheck_->isChecked() ? "--attribute-constrained-belt" : "--no-attribute-constrained-belt");
    args << (rootAntiMergeCheck_->isChecked() ? "--root-anti-merge" : "--no-root-anti-merge");
    args << (localAntiMergeCheck_->isChecked() ? "--local-anti-merge" : "--no-local-anti-merge");
    args << (residualTraceCheck_->isChecked() ? "--residual-trace" : "--no-residual-trace");
    return args;
}

void FaultTraceDialog::applyParameterPreset(const QString& presetName) {
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
        setRootAntiMergePreset(false);
        residualTraceCheck_->setChecked(true);
        simplifyToleranceSpin_->setValue(1.5);
        smoothIterationsSpin_->setValue(0);
        residualTraceMinLengthSpin_->setValue(5);
        residualTraceMaxCellsSpin_->setValue(120);
    } else if (presetName == QString::fromUtf8("防黏连增强")) {
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
        setRootAntiMergePreset(false);
        residualTraceCheck_->setChecked(false);
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
        setRootAntiMergePreset(false);
        residualTraceCheck_->setChecked(true);
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
        setRootAntiMergePreset(false);
        residualTraceCheck_->setChecked(false);
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
        setRootAntiMergePreset(false);
        residualTraceCheck_->setChecked(false);
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
    if (runModeCombo_ && runModeCombo_->currentIndex() == 0) {
        refreshInputPreview();
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
    if (renderViewWidget_) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("正在刷新 GeoEast 网格对象"));
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
    standaloneRun_ = runModeCombo_ && runModeCombo_->currentIndex() == 0;
    const QString runner = standaloneRun_ ? coreRunnerEdit_->text().trimmed() : runnerEdit_->text().trimmed();
    if (!QFileInfo::exists(runner)) {
        QMessageBox::warning(this, QString::fromUtf8("可执行文件不存在"), runner);
        return;
    }
    if (standaloneRun_) {
        if (standaloneInputEdit_->text().trimmed().isEmpty() || !QFileInfo::exists(standaloneInputEdit_->text().trimmed())) {
            QMessageBox::warning(this, QString::fromUtf8("输入文件无效"), QString::fromUtf8("请选择可读取的外部网格文件。"));
            return;
        }
        if (standaloneOutputNameEdit_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, QString::fromUtf8("输出名称为空"), QString::fromUtf8("请填写输出结果名称。"));
            return;
        }
    } else {
        if (gridCombo_->currentText().trimmed().isEmpty() || outputEdit_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, QString::fromUtf8("参数不完整"), QString::fromUtf8("输入网格和输出多边形名称不能为空。"));
            return;
        }
    }

    refreshingGrids_ = false;
    logEdit_->clear();
    QStringList args = standaloneRun_ ? standaloneTraceArgs() : traceArgs();
    appendStatus(standaloneRun_ ? QString::fromUtf8("正在独立追踪断层多边形...") : QString::fromUtf8("正在追踪断层多边形..."));
    if (elapsedLabel_) {
        elapsedLabel_->setText(QString::fromUtf8("耗时: 计算中"));
    }
    if (renderViewWidget_) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("正在追踪断层多边形"));
    }
    appendLog("> " + runner + " " + quoteForLog(args));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (!standaloneRun_) {
        configureGeoEastEnvironment(environment);
    }
    process_->setProcessEnvironment(environment);
    setRunning(true);
    processStartMs_ = QDateTime::currentMSecsSinceEpoch();
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
    const qint64 elapsedMs = processStartMs_ > 0 ? QDateTime::currentMSecsSinceEpoch() - processStartMs_ : 0;
    if (elapsedLabel_ && processStartMs_ > 0) {
        elapsedLabel_->setText(QString::fromUtf8("耗时: %1").arg(elapsedText(elapsedMs)));
    }

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
        return;
    }

    if (exitCode == 0 && status == QProcess::NormalExit) {
        if (standaloneRun_) {
            lastStandaloneOutputCsv_ = currentStandaloneOutputCsv();
            saveSettings();
            loadLatestStandaloneResult();
            appendStatus(QString::fromUtf8("独立追踪完成，结果已保存"));
            appendLog(QString::fromUtf8("结果文件：%1").arg(QDir::toNativeSeparators(lastStandaloneOutputCsv_)));
        } else {
            appendStatus(QString::fromUtf8("追踪完成，结果已写入 GeoEast 数据库"));
        }
        if (renderViewWidget_) {
            static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(
                standaloneRun_ ? QString::fromUtf8("追踪完成，已载入独立结果")
                               : QString::fromUtf8("追踪完成，结果已写入 GeoEast 数据库"));
        }
    } else {
        appendStatus(QString::fromUtf8("运行失败，请查看日志"));
        if (renderViewWidget_) {
            static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("运行失败，请查看日志"));
        }
    }
    refreshingGrids_ = false;
}

void FaultTraceDialog::onProcessError(QProcess::ProcessError error) {
    appendLog(QString::fromUtf8("进程错误：%1").arg(static_cast<int>(error)));
    appendStatus(QString::fromUtf8("进程启动或运行异常"));
    if (elapsedLabel_ && processStartMs_ > 0) {
        elapsedLabel_->setText(QString::fromUtf8("耗时: %1").arg(elapsedText(QDateTime::currentMSecsSinceEpoch() - processStartMs_)));
    }
    if (renderViewWidget_) {
        static_cast<RenderViewWidget*>(renderViewWidget_)->setActivity(QString::fromUtf8("进程启动或运行异常"));
    }
    setRunning(false);
}

void FaultTraceDialog::setRunning(bool running) {
    const bool standalone = runModeCombo_ && runModeCombo_->currentIndex() == 0;
    refreshButton_->setEnabled(!running && !standalone);
    runButton_->setEnabled(!running);
    stopButton_->setEnabled(running);
    if (refreshGridAction_) {
        refreshGridAction_->setEnabled(!running);
    }
    if (runTraceAction_) {
        runTraceAction_->setEnabled(!running);
    }
    if (stopTraceAction_) {
        stopTraceAction_->setEnabled(running);
    }
    if (connectionLabel_) {
        connectionLabel_->setText(running ? QString::fromUtf8("任务运行中") : QString::fromUtf8("数据库连接待验证"));
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

void FaultTraceDialog::loadLatestStandaloneResult() {
    if (!renderViewWidget_) {
        return;
    }
    QString result = lastStandaloneOutputCsv_;
    if (result.isEmpty()) {
        result = currentStandaloneOutputCsv();
    }
    if (QFileInfo::exists(result)) {
        QString error;
        if (!static_cast<RenderViewWidget*>(renderViewWidget_)->loadPolygonCsv(result, &error) && !error.isEmpty()) {
            appendLog(error);
        }
    }
}

void FaultTraceDialog::ensureProductDefaults() {
    if (runModeCombo_ && runModeCombo_->currentText().isEmpty()) {
        runModeCombo_->setCurrentText(QString::fromUtf8("独立文件模式"));
    }
    if (coreRunnerEdit_ && coreRunnerEdit_->text().trimmed().isEmpty()) {
        coreRunnerEdit_->setText(defaultCoreRunnerPath());
    }
    if (standaloneOutputDirEdit_ && standaloneOutputDirEdit_->text().trimmed().isEmpty()) {
        standaloneOutputDirEdit_->setText(defaultStandaloneOutputDir());
    }
    if (standaloneOutputNameEdit_ && standaloneOutputNameEdit_->text().trimmed().isEmpty()) {
        standaloneOutputNameEdit_->setText("FaultPolyTrace_Result");
    }
    if (standaloneInputEdit_ && standaloneInputEdit_->text().trimmed().isEmpty()) {
        standaloneInputEdit_->setText(defaultStandaloneInputPath());
    }
    if (projectEdit_ && projectEdit_->text().trimmed().isEmpty()) {
        projectEdit_->setPlaceholderText(QString::fromUtf8("GeoEast 工区项目名，集成运行时自动带入或手动填写"));
    }
}

void FaultTraceDialog::updateStatusContext() {
    if (projectStatusLabel_) {
        if (runModeCombo_ && runModeCombo_->currentIndex() == 0) {
            projectStatusLabel_->setText(QString::fromUtf8("独立输出: %1").arg(standaloneOutputDirEdit_ ? standaloneOutputDirEdit_->text().trimmed() : QString()));
        } else {
            projectStatusLabel_->setText(QString::fromUtf8("正在操作项目: %1").arg(projectEdit_ ? projectEdit_->text().trimmed() : QString()));
        }
    }
    if (gridStatusLabel_) {
        if (runModeCombo_ && runModeCombo_->currentIndex() == 0) {
            gridStatusLabel_->setText(QString::fromUtf8("当前文件: %1").arg(standaloneInputEdit_ ? QFileInfo(standaloneInputEdit_->text()).fileName() : QString()));
        } else {
            gridStatusLabel_->setText(QString::fromUtf8("当前网格: %1").arg(gridCombo_ ? gridCombo_->currentText().trimmed() : QString()));
        }
    }
    if (connectionLabel_ && process_ && process_->state() == QProcess::NotRunning) {
        if (runModeCombo_ && runModeCombo_->currentIndex() == 0) {
            connectionLabel_->setText(QString::fromUtf8("独立文件模式"));
        } else {
            const QString db = databaseEdit_ ? databaseEdit_->text().trimmed() : QString();
            connectionLabel_->setText(db.isEmpty() ? QString::fromUtf8("数据库未配置") : QString::fromUtf8("数据库连接待验证: %1").arg(db));
        }
    }
    if (renderViewWidget_) {
        const bool standalone = runModeCombo_ && runModeCombo_->currentIndex() == 0;
        static_cast<RenderViewWidget*>(renderViewWidget_)->setContext(
            standalone ? QString::fromUtf8("独立文件") : (databaseEdit_ ? databaseEdit_->text().trimmed() : QString()),
            standalone ? QString::fromUtf8("本机处理") : (projectEdit_ ? projectEdit_->text().trimmed() : QString()),
            standalone ? (standaloneInputEdit_ ? QFileInfo(standaloneInputEdit_->text()).fileName() : QString()) : (gridCombo_ ? gridCombo_->currentText().trimmed() : QString()),
            standalone ? QFileInfo(currentStandaloneOutputCsv()).fileName() : (outputEdit_ ? outputEdit_->text().trimmed() : QString()),
            presetCombo_ ? presetCombo_->currentText() : QString(),
            traceModeCombo_ ? traceModeCombo_->currentText() : QString(),
            thresholdModeCombo_ ? thresholdModeCombo_->currentText() : QString(),
            percentileSpin_ ? percentileSpin_->value() : 0.0,
            thresholdSpin_ ? thresholdSpin_->value() : 0.0,
            minAreaSpin_ ? minAreaSpin_->value() : 0.0,
            polygonColorEdit_ ? QColor(polygonColorEdit_->text()) : QColor("#00FFFF"),
            polygonLineWidthSpin_ ? polygonLineWidthSpin_->value() : 0.03);
    }
}
