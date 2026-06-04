#include "FaultPolyTraceWindow.h"

#include <QAction>
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
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
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
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
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
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace {

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
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty()) {
        dir = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("config");
    }
    QDir().mkpath(dir);
    return dir;
}

QString elapsedText(qint64 ms) {
    return QString::fromUtf8("%1 ms (%2 s)").arg(ms).arg(ms / 1000.0, 0, 'f', ms < 1000 ? 3 : 2);
}

QString quoteForLog(const QStringList& args) {
    QStringList quoted;
    for (const QString& arg : args) {
        quoted << (arg.contains(' ') ? QString("\"%1\"").arg(arg) : arg);
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
    label->setFixedWidth(92);
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
    layout->setColumnMinimumWidth(0, 92);
    layout->setColumnStretch(1, 1);
}

void configureSmallButton(QPushButton* button) {
    button->setFixedWidth(56);
    button->setMinimumHeight(24);
}

enum class ToolIconKind {
    Pan,
    ZoomIn,
    ZoomOut,
    Reset
};

QIcon makeToolIcon(ToolIconKind kind) {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor blue("#1685D9");
    const QColor darkBlue("#005A9E");
    painter.setPen(QPen(darkBlue, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor("#EAF6FF"));

    if (kind == ToolIconKind::Pan) {
        painter.drawRoundedRect(QRectF(5, 5, 14, 14), 2, 2);
        painter.drawLine(QPointF(12, 3), QPointF(12, 21));
        painter.drawLine(QPointF(3, 12), QPointF(21, 12));
        painter.setBrush(blue);
        painter.drawPolygon(QPolygonF({QPointF(12, 1), QPointF(9, 5), QPointF(15, 5)}));
        painter.drawPolygon(QPolygonF({QPointF(12, 23), QPointF(9, 19), QPointF(15, 19)}));
        painter.drawPolygon(QPolygonF({QPointF(1, 12), QPointF(5, 9), QPointF(5, 15)}));
        painter.drawPolygon(QPolygonF({QPointF(23, 12), QPointF(19, 9), QPointF(19, 15)}));
    } else if (kind == ToolIconKind::ZoomIn || kind == ToolIconKind::ZoomOut) {
        painter.drawEllipse(QRectF(4, 4, 11, 11));
        painter.drawLine(QPointF(13, 13), QPointF(20, 20));
        painter.setPen(QPen(darkBlue, 1.8, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(7, 9.5), QPointF(12, 9.5));
        if (kind == ToolIconKind::ZoomIn) {
            painter.drawLine(QPointF(9.5, 7), QPointF(9.5, 12));
        }
    } else {
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(QRectF(5, 5, 14, 14), 40 * 16, 280 * 16);
        painter.setBrush(blue);
        painter.drawPolygon(QPolygonF({QPointF(17, 5), QPointF(21, 5), QPointF(20, 9)}));
        painter.drawLine(QPointF(8, 12), QPointF(16, 12));
    }

    return QIcon(pixmap);
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
    }

    void setMode(Mode mode) {
        mode_ = mode;
        update();
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
        panMode_ = false;
        dragging_ = false;
        setCursor(Qt::ArrowCursor);
        update();
    }

    bool loadGrid(const QString& path, const QString& format, QString* error) {
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
        update();
        return true;
    }

    bool loadPolygons(const QString& path, QString* error) {
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
        const QString modeText = showGridLayer_ && showPolygonLayer_ ? QString::fromUtf8("叠加结果") :
            (showGridLayer_ ? QString::fromUtf8("原始网格") :
            (showPolygonLayer_ ? QString::fromUtf8("结果多边形") : QString::fromUtf8("图层已关闭")));
        painter.drawText(QPoint(24, 34), QString::fromUtf8("[视图区] %1").arg(modeText));
        painter.setFont(QFont(painter.font().family(), 9));
        painter.setPen(QColor("#777777"));
        painter.drawText(QRect(width() - 280, 18, 250, 20), Qt::AlignRight, status_);
        painter.drawText(QRect(width() - 280, 38, 250, 20), Qt::AlignRight,
                         QString::fromUtf8("缩放 %1%").arg(qRound(zoom_ * 100)));
        if (grid_.isEmpty() && polygons_.isEmpty()) {
            painter.drawText(canvas, Qt::AlignCenter, QString::fromUtf8("请选择输入网格，或载入历史结果。"));
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
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else if (event->angleDelta().y() < 0) {
            zoomOut();
        }
    }

private:
    QColor heatColor(double value) const {
        const double t = qBound(0.0, (value - min_) / (max_ - min_), 1.0);
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
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
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
        painter.drawText(QPoint(26, 38), QString::fromUtf8("[任务视图区] 真实图层查看"));

        QFont hintFont = painter.font();
        hintFont.setPointSize(10);
        hintFont.setBold(false);
        painter.setFont(hintFont);
        painter.setPen(QColor("#666666"));
        painter.drawText(rect().adjusted(26, 60, -26, -26), Qt::AlignLeft | Qt::AlignTop, taskLines_.join('\n'));

        const QRectF guideRect(width() - 360, height() - 210, 310, 150);
        if (guideRect.left() > 360 && guideRect.top() > 170) {
            painter.setPen(QPen(QColor("#C8C8C8"), 1));
            painter.setBrush(QColor("#FBFBFB"));
            painter.drawRect(guideRect);
            painter.setPen(QColor("#666666"));
            painter.drawText(guideRect.adjusted(12, 12, -12, -12), Qt::AlignLeft | Qt::AlignTop,
                             QString::fromUtf8("查看逻辑：\n"
                                               "1. 原始网格显示真实输入属性色图。\n"
                                               "2. 结果多边形显示真实 CSV/WKT 输出。\n"
                                               "3. 叠加结果用于核对断层响应贴合度。\n"
                                               "4. 左侧复选框可独立开关图层。"));
        }

        painter.setPen(QColor("#8A8A8A"));
        painter.drawText(QRect(width() - 210, 18, 180, 20), Qt::AlignRight,
                         QString::fromUtf8("查看模式  缩放 %1%").arg(qRound(zoom_ * 100)));
    }

    Mode mode_ = Mode::Task;
    QVector<double> grid_;
    QVector<QVector<QPolygonF>> polygons_;
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
    bool panMode_ = false;
    bool dragging_ = false;
    bool showGridLayer_ = true;
    bool showPolygonLayer_ = true;
    QString status_ = QString::fromUtf8("等待数据");
    QStringList taskLines_;
};

class ColorBarWidget : public QWidget {
public:
    explicit ColorBarWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedWidth(64);
        setMinimumHeight(360);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#F0F0F0"));
        painter.setPen(QColor("#A0A0A0"));
        painter.drawLine(0, 0, 0, height());

        const QRect bar(width() / 2 - 8, 56, 16, qMax(120, height() - 118));
        QLinearGradient gradient(bar.left(), bar.bottom(), bar.left(), bar.top());
        gradient.setColorAt(0.00, QColor("#FF0000"));
        gradient.setColorAt(0.25, QColor("#FFFF00"));
        gradient.setColorAt(0.50, QColor("#00FF00"));
        gradient.setColorAt(0.75, QColor("#00FFFF"));
        gradient.setColorAt(1.00, QColor("#0000FF"));
        painter.fillRect(bar, gradient);
        painter.setPen(QColor("#666666"));
        painter.drawRect(bar.adjusted(0, 0, -1, -1));

        painter.setPen(QColor("#333333"));
        painter.drawText(QRect(4, 8, width() - 8, 18), Qt::AlignCenter, QString::fromUtf8("属性"));
        painter.setPen(QColor("#004CFF"));
        painter.drawText(QRect(4, 32, width() - 8, 18), Qt::AlignCenter, QString::fromUtf8("1.0"));
        painter.setPen(QColor("#FF0000"));
        painter.drawText(QRect(4, bar.bottom() + 8, width() - 8, 18), Qt::AlignCenter, QString::fromUtf8("0.0"));
    }
};

FaultPolyTraceWindow::FaultPolyTraceWindow(QWidget* parent)
    : QMainWindow(parent), process_(new QProcess(this)) {
    buildUi();
    loadSettings();
    refreshHistory();
    refreshInputPreview();
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
    setStyleSheet(QString::fromUtf8(R"(
        QMainWindow, QWidget { background: #E8E8E8; color: #222; font-family: "Microsoft YaHei", "Segoe UI"; font-size: 12px; }
        QMenuBar { background: #F2F2F2; border-bottom: 1px solid #B0B0B0; padding: 2px 4px; }
        QMenuBar::item { padding: 4px 10px; background: transparent; }
        QMenuBar::item:selected { background: #DDECF8; }
        QMenu { background: #F8F8F8; border: 1px solid #A8A8A8; }
        QMenu::item { padding: 5px 28px 5px 24px; }
        QMenu::item:selected { background: #DDECF8; }
        QToolBar { background: #ECECEC; border-bottom: 1px solid #B0B0B0; spacing: 5px; padding: 4px 6px; }
        QToolButton { background: transparent; border: 1px solid transparent; border-radius: 2px; padding: 3px; }
        QToolButton:hover { background: #DDECF8; border-color: #8CBCE8; }
        QGroupBox { background: #F0F0F0; border: 1px solid #B5B5B5; border-radius: 3px; margin-top: 10px; padding-top: 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 5px; color: #005A9E; font-weight: bold; }
        QLabel { background: transparent; }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { background: white; border: 1px solid #A0A0A0; border-radius: 2px; min-height: 23px; padding: 1px 5px; }
        QPushButton { background: #E1E1E1; border: 1px solid #A0A0A0; border-radius: 2px; min-height: 24px; padding: 2px 10px; }
        QPushButton:hover { background: #E5F1FB; border-color: #0078D7; }
        QPushButton#runButton { background: #0078D7; color: white; border-color: #005A9E; font-weight: bold; min-height: 30px; }
        QLabel#logHeader { background: #E6E6E6; border-top: 1px solid #A8A8A8; border-bottom: 1px solid #C8C8C8; padding-left: 10px; font-weight: bold; }
        QTextEdit { background: #F8F9FA; border-top: 1px solid #B8B8B8; font-family: Consolas, "Courier New"; }
        QLabel#resultSummary { color: #333333; font-weight: bold; }
        QLabel#resultPath { color: #555555; background: #F7F7F7; border: 1px solid #CCCCCC; padding: 4px; }
    )"));

    QAction* openAction = new QAction(style()->standardIcon(QStyle::SP_DirOpenIcon), QString::fromUtf8("打开 DAT 文件"), this);
    openAction->setToolTip(QString::fromUtf8("选择外部断层属性网格文件"));
    connect(openAction, &QAction::triggered, this, &FaultPolyTraceWindow::browseInput);
    runAction_ = new QAction(style()->standardIcon(QStyle::SP_MediaPlay), QString::fromUtf8("开始追踪"), this);
    runAction_->setToolTip(QString::fromUtf8("按当前参数运行断层多边形自动追踪"));
    connect(runAction_, &QAction::triggered, this, &FaultPolyTraceWindow::runTrace);
    stopAction_ = new QAction(style()->standardIcon(QStyle::SP_MediaStop), QString::fromUtf8("停止"), this);
    stopAction_->setToolTip(QString::fromUtf8("终止当前正在运行的追踪任务"));
    stopAction_->setEnabled(false);
    connect(stopAction_, &QAction::triggered, this, &FaultPolyTraceWindow::stopTrace);
    QAction* pngAction = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), QString::fromUtf8("导出PNG"), this);
    pngAction->setToolTip(QString::fromUtf8("导出当前视图区图像"));
    connect(pngAction, &QAction::triggered, this, &FaultPolyTraceWindow::exportPng);
    QAction* saveTplAction = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), QString::fromUtf8("保存当前配置"), this);
    saveTplAction->setToolTip(QString::fromUtf8("保存当前算法参数模板"));
    connect(saveTplAction, &QAction::triggered, this, &FaultPolyTraceWindow::saveTemplate);
    QAction* loadTplAction = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), QString::fromUtf8("打开配置"), this);
    loadTplAction->setToolTip(QString::fromUtf8("读取已保存的参数模板"));
    connect(loadTplAction, &QAction::triggered, this, &FaultPolyTraceWindow::loadTemplate);
    QAction* resetAction = new QAction(style()->standardIcon(QStyle::SP_BrowserReload), QString::fromUtf8("恢复默认参数"), this);
    resetAction->setToolTip(QString::fromUtf8("恢复推荐默认参数"));
    connect(resetAction, &QAction::triggered, this, &FaultPolyTraceWindow::restoreDefaults);
    QAction* outputDirAction = new QAction(style()->standardIcon(QStyle::SP_DirIcon), QString::fromUtf8("打开输出目录"), this);
    outputDirAction->setToolTip(QString::fromUtf8("打开当前结果输出目录"));
    connect(outputDirAction, &QAction::triggered, this, &FaultPolyTraceWindow::openOutputDir);
    QAction* helpAction = new QAction(style()->standardIcon(QStyle::SP_MessageBoxQuestion), QString::fromUtf8("帮助提示"), this);
    helpAction->setToolTip(QString::fromUtf8("查看独立软件使用说明"));
    connect(helpAction, &QAction::triggered, this, &FaultPolyTraceWindow::openHelp);

    QMenu* fileMenu = menuBar()->addMenu(QString::fromUtf8("文件(&F)"));
    fileMenu->addAction(openAction);
    fileMenu->addAction(pngAction);
    fileMenu->addSeparator();
    fileMenu->addAction(saveTplAction);
    fileMenu->addAction(loadTplAction);

    QMenu* editMenu = menuBar()->addMenu(QString::fromUtf8("编辑(&E)"));
    editMenu->addAction(resetAction);

    QMenu* viewMenu = menuBar()->addMenu(QString::fromUtf8("视图(&V)"));
    QAction* taskViewAction = viewMenu->addAction(QString::fromUtf8("任务预览"));
    connect(taskViewAction, &QAction::triggered, this, [this]() { viewModeCombo_->setCurrentText(QString::fromUtf8("任务预览")); });
    QAction* rawViewAction = viewMenu->addAction(QString::fromUtf8("原始网格"));
    connect(rawViewAction, &QAction::triggered, this, [this]() { viewModeCombo_->setCurrentText(QString::fromUtf8("原始网格")); });
    QAction* resultViewAction = viewMenu->addAction(QString::fromUtf8("结果多边形"));
    connect(resultViewAction, &QAction::triggered, this, [this]() { viewModeCombo_->setCurrentText(QString::fromUtf8("结果多边形")); });
    QAction* overlayViewAction = viewMenu->addAction(QString::fromUtf8("叠加结果"));
    connect(overlayViewAction, &QAction::triggered, this, [this]() { viewModeCombo_->setCurrentText(QString::fromUtf8("叠加结果")); });
    viewMenu->addSeparator();
    QAction* panAction = viewMenu->addAction(makeToolIcon(ToolIconKind::Pan), QString::fromUtf8("平移模式"));
    panAction->setCheckable(true);
    connect(panAction, &QAction::toggled, this, [this](bool checked) {
        if (view_) {
            view_->setPanMode(checked);
        }
        updateStatus(checked ? QString::fromUtf8("视图区已切换为平移模式") : QString::fromUtf8("视图区已切换为选择模式"));
    });
    QAction* zoomInAction = viewMenu->addAction(makeToolIcon(ToolIconKind::ZoomIn), QString::fromUtf8("放大"));
    connect(zoomInAction, &QAction::triggered, this, [this]() {
        if (view_) {
            view_->zoomIn();
        }
        updateStatus(QString::fromUtf8("视图区已放大"));
    });
    QAction* zoomOutAction = viewMenu->addAction(makeToolIcon(ToolIconKind::ZoomOut), QString::fromUtf8("缩小"));
    connect(zoomOutAction, &QAction::triggered, this, [this]() {
        if (view_) {
            view_->zoomOut();
        }
        updateStatus(QString::fromUtf8("视图区已缩小"));
    });
    QAction* resetViewAction = viewMenu->addAction(makeToolIcon(ToolIconKind::Reset), QString::fromUtf8("重置视图"));
    connect(resetViewAction, &QAction::triggered, this, [this, panAction]() {
        if (view_) {
            view_->resetView();
        }
        panAction->setChecked(false);
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

    QMenu* helpMenu = menuBar()->addMenu(QString::fromUtf8("帮助(&H)"));
    helpMenu->addAction(helpAction);

    QToolBar* toolbar = addToolBar(QString::fromUtf8("工具栏"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(20, 20));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(openAction);
    toolbar->addAction(saveTplAction);
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
    toolbar->addAction(helpAction);

    QWidget* root = new QWidget(this);
    QHBoxLayout* rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QWidget* leftPane = new QWidget(root);
    leftPane->setFixedWidth(392);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(10, 10, 10, 8);
    leftLayout->setSpacing(8);

    QScrollArea* sidebarScroll = new QScrollArea(leftPane);
    sidebarScroll->setWidgetResizable(true);
    sidebarScroll->setFrameShape(QFrame::NoFrame);
    sidebarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget* sidebarContent = new QWidget(sidebarScroll);
    sidebarContent->setMinimumWidth(356);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebarContent);
    sidebarLayout->setContentsMargins(0, 0, 4, 0);
    sidebarLayout->setSpacing(8);

    QGroupBox* dataBox = new QGroupBox(QString::fromUtf8("数据与输出"), sidebarContent);
    QGridLayout* dataLayout = new QGridLayout(dataBox);
    configureFormGrid(dataLayout);
    sourceModeCombo_ = new QComboBox(dataBox);
    sourceModeCombo_->addItems({QString::fromUtf8("外部文件"), QString::fromUtf8("GeoEast数据库")});
    addRow(dataLayout, 0, QString::fromUtf8("数据源"), sourceModeCombo_, dataBox);

    sourceStack_ = new QStackedWidget(dataBox);
    QWidget* filePage = new QWidget(sourceStack_);
    QGridLayout* fileLayout = new QGridLayout(filePage);
    configureFormGrid(fileLayout);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    inputEdit_ = new QLineEdit(filePage);
    inputEdit_->setToolTip(QString::fromUtf8("外部断层属性网格文件路径"));
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
    inputFormatCombo_->addItems({"auto", "matrix", "geoeast-dat"});
    addRow(fileLayout, 1, QString::fromUtf8("输入格式"), inputFormatCombo_, filePage);
    sourceStack_->addWidget(filePage);

    QWidget* geoEastPage = new QWidget(sourceStack_);
    QGridLayout* geoEastLayout = new QGridLayout(geoEastPage);
    configureFormGrid(geoEastLayout);
    geoEastLayout->setContentsMargins(0, 0, 0, 0);
    geoEastGridCombo_ = new QComboBox(geoEastPage);
    geoEastGridCombo_->setEditable(true);
    geoEastGridCombo_->addItems({"test7_fault_attribute_bo", "test7_fault_attribute", "test"});
    addRow(geoEastLayout, 0, QString::fromUtf8("输入网格"), geoEastGridCombo_, geoEastPage);
    QWidget* geoEastActionRow = new QWidget(geoEastPage);
    QHBoxLayout* geoEastActionLayout = new QHBoxLayout(geoEastActionRow);
    geoEastActionLayout->setContentsMargins(0, 0, 0, 0);
    geoEastActionLayout->setSpacing(6);
    geoEastRefreshButton_ = new QPushButton(QString::fromUtf8("刷新网格"), geoEastActionRow);
    geoEastConfigButton_ = new QPushButton(QString::fromUtf8("数据库配置"), geoEastActionRow);
    geoEastDeleteButton_ = new QPushButton(QString::fromUtf8("删除结果"), geoEastActionRow);
    geoEastActionLayout->addWidget(geoEastRefreshButton_, 1);
    geoEastActionLayout->addWidget(geoEastConfigButton_, 1);
    geoEastActionLayout->addWidget(geoEastDeleteButton_, 1);
    geoEastLayout->addWidget(geoEastActionRow, 1, 0, 1, 2);
    connect(geoEastRefreshButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::refreshGeoEastGrids);
    connect(geoEastConfigButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::showGeoEastConfigDialog);
    connect(geoEastDeleteButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::deleteGeoEastResult);
    sourceStack_->addWidget(geoEastPage);
    dataLayout->addWidget(sourceStack_, 1, 0, 1, 2);

    geoEastRunnerEdit_ = new QLineEdit(geoEastRunnerPath(), dataBox);
    geoEastRootEdit_ = new QLineEdit(defaultGeoEastRoot(), dataBox);
    geoEastDataRootEdit_ = new QLineEdit(defaultGeoEastDataRoot(), dataBox);
    pgHostEdit_ = new QLineEdit("127.0.0.1", dataBox);
    pgPortEdit_ = new QLineEdit("5433", dataBox);
    databaseEdit_ = new QLineEdit("ndp", dataBox);
    projectEdit_ = new QLineEdit(QString::fromLocal8Bit(qgetenv("FPT_GEOEAST_PROJECT")).isEmpty() ? "BGPCup2026_Test7" : QString::fromLocal8Bit(qgetenv("FPT_GEOEAST_PROJECT")), dataBox);
    userEdit_ = new QLineEdit(QString::fromLocal8Bit(qgetenv("FPT_GEOEAST_USER")).isEmpty() ? qEnvironmentVariable("USERNAME") : QString::fromLocal8Bit(qgetenv("FPT_GEOEAST_USER")), dataBox);
    passwordEdit_ = new QLineEdit(dataBox);
    passwordEdit_->setEchoMode(QLineEdit::Password);

    outputDirEdit_ = new QLineEdit(dataBox);
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
    addRow(dataLayout, 2, QString::fromUtf8("输出目录"), outputRow, dataBox);
    outputNameEdit_ = new QLineEdit("FaultPolyTrace_Result", dataBox);
    addRow(dataLayout, 3, QString::fromUtf8("输出名称"), outputNameEdit_, dataBox);
    viewModeCombo_ = new QComboBox(dataBox);
    viewModeCombo_->addItems({QString::fromUtf8("任务预览"), QString::fromUtf8("叠加结果"), QString::fromUtf8("原始网格"), QString::fromUtf8("结果多边形")});
    addRow(dataLayout, 4, QString::fromUtf8("视图内容"), viewModeCombo_, dataBox);
    sidebarLayout->addWidget(dataBox);

    QGroupBox* styleBox = new QGroupBox(QString::fromUtf8("显示样式"), sidebarContent);
    QGridLayout* styleLayout = new QGridLayout(styleBox);
    configureFormGrid(styleLayout);
    colorEdit_ = new QLineEdit("#00FFFF", styleBox);
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
    showGridLayerCheck_ = new QCheckBox(QString::fromUtf8("显示原始网格层"), styleBox);
    showGridLayerCheck_->setChecked(true);
    styleLayout->addWidget(showGridLayerCheck_, 3, 0, 1, 2);
    showPolygonLayerCheck_ = new QCheckBox(QString::fromUtf8("显示结果多边形层"), styleBox);
    showPolygonLayerCheck_->setChecked(true);
    styleLayout->addWidget(showPolygonLayerCheck_, 4, 0, 1, 2);
    sidebarLayout->addWidget(styleBox);

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
    resultLayout->addWidget(resultSummaryLabel_, 0, 0, 1, 3);
    resultLayout->addWidget(resultPathLabel_, 1, 0, 1, 3);
    showOverlayButton_ = new QPushButton(QString::fromUtf8("查看叠加"), resultBox);
    openCsvButton_ = new QPushButton(QString::fromUtf8("打开CSV"), resultBox);
    openGeoJsonButton_ = new QPushButton(QString::fromUtf8("GeoJSON"), resultBox);
    openDiagnosticsButton_ = new QPushButton(QString::fromUtf8("诊断JSON"), resultBox);
    openOutputDirButton_ = new QPushButton(QString::fromUtf8("输出目录"), resultBox);
    exportPngButton_ = new QPushButton(QString::fromUtf8("导出PNG"), resultBox);
    resultLayout->addWidget(showOverlayButton_, 2, 0);
    resultLayout->addWidget(openCsvButton_, 2, 1);
    resultLayout->addWidget(openGeoJsonButton_, 2, 2);
    resultLayout->addWidget(openDiagnosticsButton_, 3, 0);
    resultLayout->addWidget(openOutputDirButton_, 3, 1);
    resultLayout->addWidget(exportPngButton_, 3, 2);
    connect(showOverlayButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::showOverlayResult);
    connect(openCsvButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::openResultCsv);
    connect(openGeoJsonButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::openResultGeoJson);
    connect(openDiagnosticsButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::openDiagnosticsJson);
    connect(openOutputDirButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::openOutputDir);
    connect(exportPngButton_, &QPushButton::clicked, this, &FaultPolyTraceWindow::exportPng);
    sidebarLayout->addWidget(resultBox);

    QGroupBox* basicBox = new QGroupBox(QString::fromUtf8("追踪参数"), sidebarContent);
    QGridLayout* basicLayout = new QGridLayout(basicBox);
    configureFormGrid(basicLayout);
    presetCombo_ = new QComboBox(basicBox);
    presetCombo_->addItems({
        QString::fromUtf8("全覆盖优先"),
        QString::fromUtf8("贴合融合推荐"),
        QString::fromUtf8("防黏连增强"),
        QString::fromUtf8("均衡推荐"),
        QString::fromUtf8("保守高精度"),
        QString::fromUtf8("弱响应高召回"),
        QString::fromUtf8("宽断层带"),
        QString::fromUtf8("细碎断裂")
    });
    addRow(basicLayout, 0, QString::fromUtf8("预设模板"), presetCombo_, basicBox);
    traceModeCombo_ = new QComboBox(basicBox);
    traceModeCombo_->addItems({"ridge-belt", "region"});
    addRow(basicLayout, 1, QString::fromUtf8("追踪模式"), traceModeCombo_, basicBox);
    thresholdModeCombo_ = new QComboBox(basicBox);
    thresholdModeCombo_->addItems({"percentile", "positive-percentile", "fixed"});
    addRow(basicLayout, 2, QString::fromUtf8("阈值模式"), thresholdModeCombo_, basicBox);
    percentileSpin_ = new QDoubleSpinBox(basicBox);
    percentileSpin_->setRange(0.0, 1.0);
    percentileSpin_->setDecimals(4);
    percentileSpin_->setSingleStep(0.005);
    percentileSpin_->setValue(0.98);
    addRow(basicLayout, 3, QString::fromUtf8("分位数"), percentileSpin_, basicBox);
    thresholdSpin_ = new QDoubleSpinBox(basicBox);
    thresholdSpin_->setRange(0.0, 1.0);
    thresholdSpin_->setDecimals(4);
    thresholdSpin_->setSingleStep(0.01);
    thresholdSpin_->setValue(0.95);
    addRow(basicLayout, 4, QString::fromUtf8("固定阈值"), thresholdSpin_, basicBox);
    minAreaSpin_ = new QDoubleSpinBox(basicBox);
    minAreaSpin_->setRange(0, 1000000000);
    minAreaSpin_->setDecimals(2);
    minAreaSpin_->setSingleStep(50.0);
    minAreaSpin_->setValue(120.0);
    addRow(basicLayout, 5, QString::fromUtf8("最小面积"), minAreaSpin_, basicBox);
    sidebarLayout->addWidget(basicBox);

    QGroupBox* topologyBox = new QGroupBox(QString::fromUtf8("形态与拓扑约束"), sidebarContent);
    QGridLayout* topologyLayout = new QGridLayout(topologyBox);
    configureFormGrid(topologyLayout);
    closeRadiusSpin_ = new QSpinBox(topologyBox);
    closeRadiusSpin_->setRange(0, 20);
    closeRadiusSpin_->setValue(1);
    addRow(topologyLayout, 0, QString::fromUtf8("闭合半径"), closeRadiusSpin_, topologyBox);
    beltRadiusSpin_ = new QSpinBox(topologyBox);
    beltRadiusSpin_->setRange(0, 20);
    beltRadiusSpin_->setValue(3);
    addRow(topologyLayout, 1, QString::fromUtf8("带宽半径"), beltRadiusSpin_, topologyBox);
    postBeltCloseRadiusSpin_ = new QSpinBox(topologyBox);
    postBeltCloseRadiusSpin_->setRange(0, 20);
    postBeltCloseRadiusSpin_->setValue(1);
    addRow(topologyLayout, 2, QString::fromUtf8("带后闭合"), postBeltCloseRadiusSpin_, topologyBox);
    attributeConstrainedBeltCheck_ = new QCheckBox(QString::fromUtf8("属性约束断层带"), topologyBox);
    attributeConstrainedBeltCheck_->setChecked(true);
    topologyLayout->addWidget(attributeConstrainedBeltCheck_, 3, 0, 1, 2);
    attributeEnvelopeRadiusSpin_ = new QSpinBox(topologyBox);
    attributeEnvelopeRadiusSpin_->setRange(0, 20);
    attributeEnvelopeRadiusSpin_->setValue(1);
    addRow(topologyLayout, 4, QString::fromUtf8("属性包络半径"), attributeEnvelopeRadiusSpin_, topologyBox);
    maxGapDistanceSpin_ = new QSpinBox(topologyBox);
    maxGapDistanceSpin_->setRange(0, 1000);
    maxGapDistanceSpin_->setValue(12);
    addRow(topologyLayout, 5, QString::fromUtf8("最大断距"), maxGapDistanceSpin_, topologyBox);
    maxGapAngleSpin_ = new QDoubleSpinBox(topologyBox);
    maxGapAngleSpin_->setRange(0, 180);
    maxGapAngleSpin_->setDecimals(1);
    maxGapAngleSpin_->setValue(55.0);
    addRow(topologyLayout, 6, QString::fromUtf8("最大夹角"), maxGapAngleSpin_, topologyBox);
    smoothIterationsSpin_ = new QSpinBox(topologyBox);
    smoothIterationsSpin_->setRange(0, 5);
    smoothIterationsSpin_->setValue(1);
    addRow(topologyLayout, 7, QString::fromUtf8("平滑次数"), smoothIterationsSpin_, topologyBox);
    simplifyToleranceSpin_ = new QDoubleSpinBox(topologyBox);
    simplifyToleranceSpin_->setRange(0, 1000);
    simplifyToleranceSpin_->setDecimals(2);
    simplifyToleranceSpin_->setSingleStep(1.0);
    simplifyToleranceSpin_->setValue(2.0);
    addRow(topologyLayout, 8, QString::fromUtf8("简化容差"), simplifyToleranceSpin_, topologyBox);
    sidebarLayout->addWidget(topologyBox);

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
    sidebarLayout->addWidget(branchBox);

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
    sidebarLayout->addWidget(antiMergeBox);

    QGroupBox* historyBox = new QGroupBox(QString::fromUtf8("历史记录"), sidebarContent);
    QGridLayout* historyLayout = new QGridLayout(historyBox);
    configureFormGrid(historyLayout);
    historyCombo_ = new QComboBox(historyBox);
    addRow(historyLayout, 0, QString::fromUtf8("任务"), historyCombo_, historyBox);
    QPushButton* loadHistoryButton = new QPushButton(QString::fromUtf8("载入历史"), historyBox);
    connect(loadHistoryButton, &QPushButton::clicked, this, &FaultPolyTraceWindow::loadHistory);
    historyLayout->addWidget(loadHistoryButton, 1, 0, 1, 2);
    sidebarLayout->addWidget(historyBox);

    sidebarLayout->addStretch(1);
    sidebarScroll->setWidget(sidebarContent);
    leftLayout->addWidget(sidebarScroll, 1);

    runButton_ = new QPushButton(QString::fromUtf8("开始追踪"), leftPane);
    runButton_->setObjectName("runButton");
    stopButton_ = new QPushButton(QString::fromUtf8("停止"), leftPane);
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
    QWidget* viewStrip = new QWidget(right);
    QHBoxLayout* viewStripLayout = new QHBoxLayout(viewStrip);
    viewStripLayout->setContentsMargins(0, 0, 0, 0);
    viewStripLayout->setSpacing(0);
    view_ = new AttributeView(right);
    ColorBarWidget* colorBar = new ColorBarWidget(viewStrip);
    viewStripLayout->addWidget(view_, 1);
    viewStripLayout->addWidget(colorBar);
    QWidget* logPanel = new QWidget(right);
    QVBoxLayout* logLayout = new QVBoxLayout(logPanel);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->setSpacing(0);
    QLabel* logHeader = new QLabel(QString::fromUtf8("运行日志与输出信息"), logPanel);
    logHeader->setObjectName("logHeader");
    logHeader->setFixedHeight(24);
    logEdit_ = new QTextEdit(logPanel);
    logEdit_->setReadOnly(true);
    logEdit_->setFixedHeight(86);
    logLayout->addWidget(logHeader);
    logLayout->addWidget(logEdit_);
    rightLayout->addWidget(viewStrip, 1);
    rightLayout->addWidget(logPanel);
    rootLayout->addWidget(leftPane);
    rootLayout->addWidget(right, 1);
    setCentralWidget(root);

    statusLabel_ = new QLabel(QString::fromUtf8("就绪"), this);
    elapsedLabel_ = new QLabel(QString::fromUtf8("耗时: --"), this);
    statusBar()->addWidget(statusLabel_, 1);
    statusBar()->addPermanentWidget(elapsedLabel_);

    connect(inputEdit_, &QLineEdit::editingFinished, this, &FaultPolyTraceWindow::refreshInputPreview);
    connect(inputFormatCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::refreshInputPreview);
    connect(sourceModeCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::updateDataSourceMode);
    connect(viewModeCombo_, &QComboBox::currentTextChanged, this, [this]() {
        if (viewModeCombo_->currentText() == QString::fromUtf8("任务预览")) {
            view_->setMode(AttributeView::Mode::Task);
        } else if (viewModeCombo_->currentText() == QString::fromUtf8("原始网格")) {
            view_->setMode(AttributeView::Mode::Raw);
            showGridLayerCheck_->blockSignals(true);
            showPolygonLayerCheck_->blockSignals(true);
            showGridLayerCheck_->setChecked(true);
            showPolygonLayerCheck_->setChecked(false);
            showGridLayerCheck_->blockSignals(false);
            showPolygonLayerCheck_->blockSignals(false);
        } else if (viewModeCombo_->currentText() == QString::fromUtf8("结果多边形")) {
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
    connect(showGridLayerCheck_, &QCheckBox::toggled, this, &FaultPolyTraceWindow::updateLayerVisibility);
    connect(showPolygonLayerCheck_, &QCheckBox::toggled, this, &FaultPolyTraceWindow::updateLayerVisibility);
    connect(presetCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::applyPreset);
    connect(inputEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(outputDirEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(outputNameEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(outputDirEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::updateResultPanel);
    connect(outputNameEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::updateResultPanel);
    connect(inputFormatCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
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
    const QString path = "D:/BaiduNetdiskDownload/BGPCup2025/test.dat";
    return QFileInfo::exists(path) ? QDir::toNativeSeparators(path) : QString();
}

QString FaultPolyTraceWindow::geoEastRunnerPath() const {
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
    return QDir::toNativeSeparators(QDir(repoRootFromAppDir()).absoluteFilePath("geoeast_plugin/bo_runner/faultpolytrace_geoeast_runner.exe"));
}

QString FaultPolyTraceWindow::defaultGeoEastRoot() const {
    const QString fromEnv = QString::fromLocal8Bit(qgetenv("FPT_GEOEAST_ROOT")).trimmed();
    if (!fromEnv.isEmpty()) {
        return QDir::toNativeSeparators(fromEnv);
    }
    const QString ngp = QString::fromLocal8Bit(qgetenv("NGP")).trimmed();
    if (!ngp.isEmpty()) {
        QDir dir(QDir::fromNativeSeparators(ngp));
        if (dir.dirName().compare("NGP", Qt::CaseInsensitive) == 0 && dir.cdUp()) {
            return QDir::toNativeSeparators(dir.absolutePath());
        }
    }
    const QString userProfile = QString::fromLocal8Bit(qgetenv("USERPROFILE"));
    const QString local = QDir(userProfile).absoluteFilePath("GeoEast");
    if (QFileInfo::exists(local)) {
        return QDir::toNativeSeparators(local);
    }
    return QDir::toNativeSeparators(local);
}

QString FaultPolyTraceWindow::defaultGeoEastDataRoot() const {
    const QString fromEnv = QString::fromLocal8Bit(qgetenv("FPT_GEOEAST_DATA_ROOT")).trimmed();
    if (!fromEnv.isEmpty()) {
        return QDir::toNativeSeparators(fromEnv);
    }
    const QString dbData = QString::fromLocal8Bit(qgetenv("NGP_DBDATA_PATH")).trimmed();
    if (!dbData.isEmpty()) {
        QDir dir(QDir::fromNativeSeparators(dbData));
        if (dir.cdUp() && dir.cdUp()) {
            return QDir::toNativeSeparators(dir.absolutePath());
        }
    }
    const QString root = defaultGeoEastRoot();
    return root.isEmpty() ? QString() : QDir::toNativeSeparators(QDir(root).absoluteFilePath("GeoEast-RC/GeoEastData"));
}

bool FaultPolyTraceWindow::isGeoEastMode() const {
    return sourceModeCombo_ && sourceModeCombo_->currentText() == QString::fromUtf8("GeoEast数据库");
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
         << "--output" << geoEastOutputName()
         << "--trace-mode" << traceModeCombo_->currentText()
         << "--threshold-mode" << thresholdModeCombo_->currentText()
         << "--threshold" << QString::number(thresholdSpin_->value(), 'g', 12)
         << "--percentile" << QString::number(percentileSpin_->value(), 'g', 12)
         << "--min-area" << QString::number(minAreaSpin_->value(), 'g', 12)
         << "--close-radius" << QString::number(closeRadiusSpin_->value())
         << "--belt-radius" << QString::number(beltRadiusSpin_->value())
         << "--post-belt-close-radius" << QString::number(postBeltCloseRadiusSpin_->value())
         << "--weak-percentile" << QString::number(weakPercentileSpin_->value(), 'g', 12)
         << "--weak-connect-radius" << QString::number(weakConnectRadiusSpin_->value())
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
         << "--polygon-color" << colorEdit_->text()
         << "--polygon-linewidth" << QString::number(lineWidthSpin_->value(), 'g', 12)
         << "--view-grid-output" << outputBasePath() + ".grid.txt"
         << "--view-polygons-output" << outputCsvPath();

    args << (attributeConstrainedBeltCheck_->isChecked() ? "--attribute-constrained-belt" : "--no-attribute-constrained-belt");
    args << (rootAntiMergeCheck_->isChecked() ? "--root-anti-merge" : "--no-root-anti-merge");
    args << (localAntiMergeCheck_->isChecked() ? "--local-anti-merge" : "--no-local-anti-merge");
    args << (residualTraceCheck_->isChecked() ? "--residual-trace" : "--no-residual-trace");
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
    elapsedLabel_->setText(QString::fromUtf8("耗时: 计算中"));
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (task == ProcessTask::GeoEastTrace || task == ProcessTask::RefreshGeoEastGrids || task == ProcessTask::DeleteGeoEastResult) {
        configureGeoEastEnvironment(environment);
    }
    process_->setProcessEnvironment(environment);
    setRunning(true);
    processStartMs_ = QDateTime::currentMSecsSinceEpoch();
    process_->start(runner, args);
}

void FaultPolyTraceWindow::loadSettings() {
    QSettings settings(settingsPath(), QSettings::IniFormat);
    sourceModeCombo_->setCurrentText(settings.value("sourceMode", sourceModeCombo_->currentText()).toString());
    const QString savedInput = settings.value("input", defaultInputPath()).toString();
    inputEdit_->setText(QFileInfo::exists(savedInput) ? savedInput : defaultInputPath());
    outputDirEdit_->setText(settings.value("outputDir", defaultOutputDir()).toString());
    outputNameEdit_->setText(settings.value("outputName", outputNameEdit_->text()).toString());
    inputFormatCombo_->setCurrentText(settings.value("inputFormat", "auto").toString());
    viewModeCombo_->setCurrentText(settings.value("viewMode", QString::fromUtf8("任务预览")).toString());
    presetCombo_->setCurrentText(settings.value("preset", presetCombo_->currentText()).toString());
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
    colorEdit_->setText(settings.value("color", colorEdit_->text()).toString());
    lineWidthSpin_->setValue(settings.value("lineWidth", lineWidthSpin_->value()).toDouble());
    alphaSpin_->setValue(settings.value("alpha", alphaSpin_->value()).toDouble());
    geoEastRunnerEdit_->setText(settings.value("geoEastRunner", geoEastRunnerEdit_->text()).toString());
    geoEastRootEdit_->setText(settings.value("geoEastRoot", geoEastRootEdit_->text()).toString());
    geoEastDataRootEdit_->setText(settings.value("geoEastDataRoot", geoEastDataRootEdit_->text()).toString());
    pgHostEdit_->setText(settings.value("pgHost", pgHostEdit_->text()).toString());
    pgPortEdit_->setText(settings.value("pgPort", pgPortEdit_->text()).toString());
    databaseEdit_->setText(settings.value("database", databaseEdit_->text()).toString());
    projectEdit_->setText(settings.value("project", projectEdit_->text()).toString());
    userEdit_->setText(settings.value("user", userEdit_->text()).toString());
    passwordEdit_->setText(settings.value("password", QString()).toString());
    geoEastGridCombo_->setCurrentText(settings.value("geoEastGrid", geoEastGridCombo_->currentText()).toString());
    lastOutputCsv_ = settings.value("lastOutputCsv").toString();
    inputEdit_->setCursorPosition(0);
    outputDirEdit_->setCursorPosition(0);
    inputEdit_->setToolTip(inputEdit_->text());
    outputDirEdit_->setToolTip(outputDirEdit_->text());
    updateOptionControls();
    updateDataSourceMode();
}

void FaultPolyTraceWindow::saveSettings() const {
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue("sourceMode", sourceModeCombo_->currentText());
    settings.setValue("input", inputEdit_->text());
    settings.setValue("outputDir", outputDirEdit_->text());
    settings.setValue("outputName", outputNameEdit_->text());
    settings.setValue("inputFormat", inputFormatCombo_->currentText());
    settings.setValue("viewMode", viewModeCombo_->currentText());
    settings.setValue("preset", presetCombo_->currentText());
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
    settings.setValue("geoEastRunner", geoEastRunnerEdit_->text());
    settings.setValue("geoEastRoot", geoEastRootEdit_->text());
    settings.setValue("geoEastDataRoot", geoEastDataRootEdit_->text());
    settings.setValue("pgHost", pgHostEdit_->text());
    settings.setValue("pgPort", pgPortEdit_->text());
    settings.setValue("database", databaseEdit_->text());
    settings.setValue("project", projectEdit_->text());
    settings.setValue("user", userEdit_->text());
    settings.setValue("password", passwordEdit_->text());
    settings.setValue("geoEastGrid", geoEastGridCombo_->currentText());
    settings.setValue("lastOutputCsv", lastOutputCsv_);
}

void FaultPolyTraceWindow::updateTaskPreview() {
    if (!view_) {
        return;
    }
    const QString thresholdText = thresholdModeCombo_->currentText() == "fixed"
        ? QString::fromUtf8("固定阈值 %1").arg(thresholdSpin_->value(), 0, 'f', 4)
        : QString::fromUtf8("分位数 %1").arg(percentileSpin_->value(), 0, 'f', 4);
    const QString inputText = isGeoEastMode()
        ? geoEastGridCombo_->currentText().trimmed()
        : (inputEdit_->text().trimmed().isEmpty()
        ? QString::fromUtf8("未选择")
        : QFileInfo(inputEdit_->text().trimmed()).fileName());
    const QString outputText = outputNameEdit_->text().trimmed().isEmpty()
        ? QString::fromUtf8("未命名")
        : outputNameEdit_->text().trimmed();
    const QString outputDirText = outputDirEdit_->text().trimmed().isEmpty()
        ? QString::fromUtf8("默认输出目录")
        : QDir::toNativeSeparators(outputDirEdit_->text().trimmed());

    view_->setTaskSummary({
        isGeoEastMode()
            ? QString::fromUtf8("运行方式：GeoEast 数据库模式    数据库：%1    项目：%2").arg(databaseEdit_->text(), projectEdit_->text())
            : QString::fromUtf8("运行方式：独立文件模式    输入格式：%1").arg(inputFormatCombo_->currentText()),
        isGeoEastMode()
            ? QString::fromUtf8("输入网格：%1").arg(inputText)
            : QString::fromUtf8("输入文件：%1").arg(inputText),
        QString::fromUtf8("输出名称：%1").arg(outputText),
        QString::fromUtf8("输出目录：%1").arg(outputDirText),
        QString::fromUtf8("方案：%1    模式：%2    阈值：%3")
            .arg(presetCombo_->currentText(), traceModeCombo_->currentText(), thresholdText),
        QString::fromUtf8("面积过滤：小于 %1 的多边形不输出    结果线宽：%2")
            .arg(minAreaSpin_->value(), 0, 'f', 2)
            .arg(lineWidthSpin_->value(), 0, 'f', 3),
        QString::fromUtf8("弱响应：分位 %1 / 半径 %2    根系防黏连：%3    残余补追：%4")
            .arg(weakPercentileSpin_->value(), 0, 'f', 4)
            .arg(weakConnectRadiusSpin_->value())
            .arg(rootAntiMergeCheck_->isChecked() ? QString::fromUtf8("启用") : QString::fromUtf8("关闭"))
            .arg(residualTraceCheck_->isChecked() ? QString::fromUtf8("启用") : QString::fromUtf8("关闭")),
        QString::fromUtf8("当前状态：等待参数输入及任务执行")
    });
}

QStringList FaultPolyTraceWindow::traceArgs() const {
    QStringList args;
    args << "--input" << inputEdit_->text().trimmed()
         << "--input-format" << inputFormatCombo_->currentText()
         << "--output" << outputCsvPath()
         << "--diagnostics" << outputBasePath() + ".diagnostics.json"
         << "--branches-output" << outputBasePath() + ".branches.csv"
         << "--topology-output" << outputBasePath() + ".topology.csv"
         << "--trace-mode" << traceModeCombo_->currentText()
         << "--threshold-mode" << thresholdModeCombo_->currentText()
         << "--threshold" << QString::number(thresholdSpin_->value(), 'g', 12)
         << "--percentile" << QString::number(percentileSpin_->value(), 'g', 12)
         << "--min-area" << QString::number(minAreaSpin_->value(), 'g', 12)
         << "--close-radius" << QString::number(closeRadiusSpin_->value())
         << "--belt-radius" << QString::number(beltRadiusSpin_->value())
         << "--post-belt-close-radius" << QString::number(postBeltCloseRadiusSpin_->value())
         << "--weak-percentile" << QString::number(weakPercentileSpin_->value(), 'g', 12)
         << "--weak-connect-radius" << QString::number(weakConnectRadiusSpin_->value())
         << "--attribute-envelope-radius" << QString::number(attributeEnvelopeRadiusSpin_->value())
         << "--min-branch-length" << QString::number(minBranchLengthSpin_->value())
         << "--min-junction-branch-length" << QString::number(minJunctionBranchLengthSpin_->value())
         << "--min-junction-branch-mean" << QString::number(minJunctionBranchMeanSpin_->value(), 'g', 12)
         << "--max-gap-distance" << QString::number(maxGapDistanceSpin_->value())
         << "--max-gap-angle" << QString::number(maxGapAngleSpin_->value(), 'g', 12)
         << "--min-gap-support-ratio" << "0.25"
         << "--no-separate-branch-belts";

    args << (attributeConstrainedBeltCheck_->isChecked() ? "--attribute-constrained-belt" : "--no-attribute-constrained-belt");
    args << (rootAntiMergeCheck_->isChecked() ? "--root-anti-merge" : "--no-root-anti-merge")
         << "--root-anti-merge-radius" << QString::number(rootAntiMergeRadiusSpin_->value())
         << "--root-anti-merge-support-ratio" << QString::number(rootAntiMergeSupportRatioSpin_->value(), 'g', 12)
         << "--root-anti-merge-max-bridge-value" << "1e-12"
         << "--root-anti-merge-min-part-cells" << QString::number(rootAntiMergeMinPartCellsSpin_->value())
         << "--root-anti-merge-min-part-ratio" << QString::number(rootAntiMergeMinPartRatioSpin_->value(), 'g', 12)
         << "--root-anti-merge-weak-guard-threshold" << QString::number(rootAntiMergeWeakGuardThresholdSpin_->value(), 'g', 12)
         << "--root-anti-merge-weak-guard-radius" << QString::number(rootAntiMergeWeakGuardRadiusSpin_->value());

    args << (localAntiMergeCheck_->isChecked() ? "--local-anti-merge" : "--no-local-anti-merge")
         << "--local-anti-merge-low-threshold" << QString::number(localAntiMergeLowThresholdSpin_->value(), 'g', 12)
         << "--local-anti-merge-bridge-radius" << QString::number(localAntiMergeBridgeRadiusSpin_->value())
         << "--local-anti-merge-min-part-cells" << QString::number(localAntiMergeMinPartCellsSpin_->value())
         << "--local-anti-merge-min-part-ratio" << QString::number(localAntiMergeMinPartRatioSpin_->value(), 'g', 12)
         << "--local-anti-merge-min-bridge-cells" << QString::number(localAntiMergeMinBridgeCellsSpin_->value())
         << "--local-anti-merge-min-low-ratio" << QString::number(localAntiMergeMinLowRatioSpin_->value(), 'g', 12);

    args << (residualTraceCheck_->isChecked() ? "--residual-trace" : "--no-residual-trace")
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
         << "--smooth-iterations" << QString::number(smoothIterationsSpin_->value());
    return args;
}

void FaultPolyTraceWindow::browseInput() {
    const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("选择平面断层属性网格"), QFileInfo(inputEdit_->text()).absolutePath(), QString::fromUtf8("网格文件 (*.txt *.dat *.csv);;所有文件 (*.*)"));
    if (!path.isEmpty()) {
        inputEdit_->setText(QDir::toNativeSeparators(path));
        refreshInputPreview();
    }
}

void FaultPolyTraceWindow::browseOutputDir() {
    const QString path = QFileDialog::getExistingDirectory(this, QString::fromUtf8("选择输出目录"), outputDirEdit_->text());
    if (!path.isEmpty()) {
        outputDirEdit_->setText(QDir::toNativeSeparators(path));
    }
}

void FaultPolyTraceWindow::chooseColor() {
    const QColor color = QColorDialog::getColor(QColor(colorEdit_->text()), this, QString::fromUtf8("选择结果多边形颜色"));
    if (color.isValid()) {
        colorEdit_->setText(color.name(QColor::HexRgb).toUpper());
        colorEdit_->setStyleSheet(QString("background:%1; font-weight:bold;").arg(colorEdit_->text()));
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
    updateTaskPreview();
    updateResultPanel();
}

void FaultPolyTraceWindow::showGeoEastConfigDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("GeoEast 数据库与环境配置"));
    dialog.resize(720, 330);

    QVBoxLayout* root = new QVBoxLayout(&dialog);
    QGridLayout* form = new QGridLayout();
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(8);
    root->addLayout(form);

    auto addPathRow = [&](int row, const QString& label, QLineEdit* source, bool directory) {
        QLineEdit* edit = new QLineEdit(source->text(), &dialog);
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
                path = QFileDialog::getExistingDirectory(this, QString::fromUtf8("选择目录"), edit->text());
            } else {
                path = QFileDialog::getOpenFileName(this, QString::fromUtf8("选择可执行文件"), QFileInfo(edit->text()).absolutePath(), QString::fromUtf8("程序 (*.exe);;所有文件 (*.*)"));
            }
            if (!path.isEmpty()) {
                edit->setText(QDir::toNativeSeparators(path));
            }
        });
        return edit;
    };

    QLineEdit* runnerCopy = addPathRow(0, QString::fromUtf8("Runner"), geoEastRunnerEdit_, false);
    QLineEdit* rootCopy = addPathRow(1, QString::fromUtf8("GeoEast根目录"), geoEastRootEdit_, true);
    QLineEdit* dataRootCopy = addPathRow(2, QString::fromUtf8("数据根目录"), geoEastDataRootEdit_, true);

    QFrame* line = new QFrame(&dialog);
    line->setFrameShape(QFrame::HLine);
    root->addWidget(line);

    QGridLayout* db = new QGridLayout();
    db->setHorizontalSpacing(8);
    db->setVerticalSpacing(8);
    root->addLayout(db);

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

    QHBoxLayout* buttons = new QHBoxLayout();
    root->addLayout(buttons);
    buttons->addStretch(1);
    QPushButton* cancel = new QPushButton(QString::fromUtf8("取消"), &dialog);
    QPushButton* ok = new QPushButton(QString::fromUtf8("应用并保存配置"), &dialog);
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(ok, &QPushButton::clicked, &dialog, [&]() {
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
    });

    dialog.exec();
}

void FaultPolyTraceWindow::refreshGeoEastGrids() {
    if (process_->state() != QProcess::NotRunning) {
        return;
    }
    const QString runner = geoEastRunnerEdit_->text().trimmed();
    if (!QFileInfo::exists(runner)) {
        QMessageBox::warning(this, QString::fromUtf8("GeoEast Runner 不存在"), runner);
        return;
    }
    saveSettings();
    QStringList args = geoEastConnectionArgs();
    args << "--list-grids";
    startProcess(ProcessTask::RefreshGeoEastGrids, runner, args, QString::fromUtf8("正在刷新 GeoEast 网格对象..."));
}

void FaultPolyTraceWindow::deleteGeoEastResult() {
    if (process_->state() != QProcess::NotRunning) {
        return;
    }
    const QString outputName = geoEastOutputName();
    const int choice = QMessageBox::warning(
        this,
        QString::fromUtf8("确认删除 GeoEast 结果"),
        QString::fromUtf8("将删除当前项目中的 BOFaultPolygon 结果对象：\n\n%1\n\n该操作不会删除输入网格，也不会删除本地输出文件。是否继续？").arg(outputName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (choice != QMessageBox::Yes) {
        return;
    }
    const QString runner = geoEastRunnerEdit_->text().trimmed();
    if (!QFileInfo::exists(runner)) {
        QMessageBox::warning(this, QString::fromUtf8("GeoEast Runner 不存在"), runner);
        return;
    }
    saveSettings();
    QStringList args = geoEastConnectionArgs();
    args << "--output" << outputName << "--delete-output";
    startProcess(ProcessTask::DeleteGeoEastResult, runner, args, QString::fromUtf8("正在删除 GeoEast 结果对象..."));
}

void FaultPolyTraceWindow::refreshInputPreview() {
    if (isGeoEastMode()) {
        updateStatus(QString::fromUtf8("GeoEast 模式下请先运行追踪或刷新网格；运行完成后载入真实图层缓存"));
        return;
    }
    if (inputEdit_->text().trimmed().isEmpty() || !QFileInfo::exists(inputEdit_->text().trimmed())) {
        updateStatus(QString::fromUtf8("请选择输入文件"));
        return;
    }
    QString error;
    if (view_->loadGrid(inputEdit_->text().trimmed(), inputFormatCombo_->currentText(), &error)) {
        updateStatus(QString::fromUtf8("原始网格已载入"));
        refreshResultPreview();
    } else if (!error.isEmpty()) {
        appendLog(error);
        updateStatus(QString::fromUtf8("原始网格载入失败"));
    }
}

void FaultPolyTraceWindow::refreshResultPreview() {
    const QString path = !lastOutputCsv_.isEmpty() && QFileInfo::exists(lastOutputCsv_) ? lastOutputCsv_ : outputCsvPath();
    if (!QFileInfo::exists(path)) {
        updateResultPanel();
        return;
    }
    QString error;
    if (!view_->loadPolygons(path, &error) && !error.isEmpty()) {
        appendLog(error);
    }
    updateResultPanel();
}

void FaultPolyTraceWindow::runTrace() {
    if (process_->state() != QProcess::NotRunning) {
        return;
    }
    if (isGeoEastMode()) {
        const QString runner = geoEastRunnerEdit_->text().trimmed();
        if (!QFileInfo::exists(runner)) {
            QMessageBox::warning(this, QString::fromUtf8("GeoEast Runner 不存在"), runner);
            return;
        }
        if (geoEastGridCombo_->currentText().trimmed().isEmpty() || geoEastOutputName().trimmed().isEmpty()) {
            QMessageBox::warning(this, QString::fromUtf8("参数不完整"), QString::fromUtf8("GeoEast 输入网格和输出多边形名称不能为空。"));
            return;
        }
        lastOutputCsv_.clear();
        updateResultPanel();
        saveSettings();
        startProcess(ProcessTask::GeoEastTrace, runner, geoEastTraceArgs(), QString::fromUtf8("正在连接 GeoEast 并追踪断层多边形..."));
        return;
    }

    const QString runner = coreRunnerPath();
    if (!QFileInfo::exists(runner)) {
        QMessageBox::warning(this, QString::fromUtf8("算法程序不存在"), runner);
        return;
    }
    const QString self = QFileInfo(QCoreApplication::applicationFilePath()).absoluteFilePath();
    const QString resolvedRunner = QFileInfo(runner).absoluteFilePath();
    if (resolvedRunner.compare(self, Qt::CaseInsensitive) == 0) {
        QMessageBox::warning(this,
                             QString::fromUtf8("算法程序定位错误"),
                             QString::fromUtf8("独立软件不能把自身作为核心算法程序启动。请确认发布包中存在 bin/faultpolytrace.exe。"));
        return;
    }
    if (inputEdit_->text().trimmed().isEmpty() || !QFileInfo::exists(inputEdit_->text().trimmed())) {
        QMessageBox::warning(this, QString::fromUtf8("输入文件无效"), QString::fromUtf8("请选择可读取的外部网格文件。"));
        return;
    }
    logEdit_->clear();
    lastOutputCsv_.clear();
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
    elapsedLabel_->setText(QString::fromUtf8("耗时: %1").arg(elapsedText(elapsedMs)));
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
                const QString current = geoEastGridCombo_->currentText();
                geoEastGridCombo_->clear();
                geoEastGridCombo_->addItems(names);
                const int index = geoEastGridCombo_->findText(current);
                if (index >= 0) {
                    geoEastGridCombo_->setCurrentIndex(index);
                }
            }
            updateStatus(QString::fromUtf8("GeoEast 网格刷新完成，共 %1 个").arg(names.size()));
            saveSettings();
        } else {
            updateStatus(QString::fromUtf8("GeoEast 网格刷新失败，请查看日志"));
        }
        updateResultPanel();
        return;
    }

    if (finishedTask == ProcessTask::DeleteGeoEastResult) {
        updateStatus(exitCode == 0 && status == QProcess::NormalExit
            ? QString::fromUtf8("GeoEast 结果对象已删除或原本不存在")
            : QString::fromUtf8("GeoEast 结果删除失败，请查看日志"));
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
            QString gridError;
            if (view_->loadGrid(outputBasePath() + ".grid.txt", "matrix", &gridError)) {
                appendLog(QString::fromUtf8("真实原始网格缓存：%1").arg(QDir::toNativeSeparators(outputBasePath() + ".grid.txt")));
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
        viewModeCombo_->setCurrentText(QString::fromUtf8("叠加结果"));
        appendHistory(elapsedMs);
        saveSettings();
        updateResultPanel();
        updateStatus(finishedTask == ProcessTask::GeoEastTrace
            ? QString::fromUtf8("GeoEast 追踪完成，结果已写入数据库并载入真实视图")
            : QString::fromUtf8("追踪完成，已切换到叠加结果"));
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
    stopButton_->setEnabled(running);
    if (geoEastRefreshButton_) {
        geoEastRefreshButton_->setEnabled(!running && isGeoEastMode());
    }
    if (geoEastConfigButton_) {
        geoEastConfigButton_->setEnabled(!running && isGeoEastMode());
    }
    if (geoEastDeleteButton_) {
        geoEastDeleteButton_->setEnabled(!running && isGeoEastMode());
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
    logEdit_->moveCursor(QTextCursor::End);
    logEdit_->insertPlainText(text.endsWith('\n') ? text : text + "\n");
    logEdit_->moveCursor(QTextCursor::End);
}

void FaultPolyTraceWindow::updateStatus(const QString& text) {
    statusLabel_->setText(QDateTime::currentDateTime().toString("HH:mm:ss  ") + text);
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
    view_->setLayerVisible(showGridLayerCheck_->isChecked(), showPolygonLayerCheck_->isChecked());
    updateTaskPreview();
}

void FaultPolyTraceWindow::applyPreset(const QString& preset) {
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

    if (preset == QString::fromUtf8("全覆盖优先")) {
        percentileSpin_->setValue(0.975);
        minAreaSpin_->setValue(120);
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
        residualTraceCheck_->setChecked(true);
        residualTraceMinLengthSpin_->setValue(5);
        residualTraceMaxCellsSpin_->setValue(120);
        simplifyToleranceSpin_->setValue(1.5);
        smoothIterationsSpin_->setValue(0);
    } else if (preset == QString::fromUtf8("弱响应高召回") || preset == QString::fromUtf8("高召回")) {
        percentileSpin_->setValue(0.975);
        minAreaSpin_->setValue(80);
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
        maxGapDistanceSpin_->setValue(14);
        maxGapAngleSpin_->setValue(55.0);
        residualTraceCheck_->setChecked(true);
        residualTraceMinLengthSpin_->setValue(5);
        residualTraceMaxCellsSpin_->setValue(120);
        simplifyToleranceSpin_->setValue(1.5);
        smoothIterationsSpin_->setValue(0);
    } else if (preset == QString::fromUtf8("保守高精度")) {
        percentileSpin_->setValue(0.985);
        minAreaSpin_->setValue(250);
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
        residualTraceCheck_->setChecked(false);
        residualTraceMinLengthSpin_->setValue(8);
        residualTraceMaxCellsSpin_->setValue(40);
        simplifyToleranceSpin_->setValue(3.0);
        smoothIterationsSpin_->setValue(1);
    } else if (preset == QString::fromUtf8("宽断层带")) {
        percentileSpin_->setValue(0.97);
        minAreaSpin_->setValue(300);
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
        residualTraceCheck_->setChecked(false);
        residualTraceMinLengthSpin_->setValue(8);
        residualTraceMaxCellsSpin_->setValue(60);
        simplifyToleranceSpin_->setValue(6.0);
        smoothIterationsSpin_->setValue(1);
    } else if (preset == QString::fromUtf8("细碎断裂")) {
        percentileSpin_->setValue(0.98);
        minAreaSpin_->setValue(120);
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
        residualTraceCheck_->setChecked(false);
        residualTraceMinLengthSpin_->setValue(4);
        residualTraceMaxCellsSpin_->setValue(80);
        simplifyToleranceSpin_->setValue(3.0);
        smoothIterationsSpin_->setValue(1);
    } else if (preset == QString::fromUtf8("防黏连增强")) {
        percentileSpin_->setValue(0.98);
        minAreaSpin_->setValue(120);
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
        residualTraceMinLengthSpin_->setValue(6);
        residualTraceMaxCellsSpin_->setValue(80);
        simplifyToleranceSpin_->setValue(2.0);
        smoothIterationsSpin_->setValue(1);
    } else {
        percentileSpin_->setValue(0.98);
        minAreaSpin_->setValue(120);
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
        residualTraceCheck_->setChecked(false);
        residualTraceMinLengthSpin_->setValue(6);
        residualTraceMaxCellsSpin_->setValue(80);
        simplifyToleranceSpin_->setValue(2.0);
        smoothIterationsSpin_->setValue(1);
    }
    updateOptionControls();
    updateTaskPreview();
}

void FaultPolyTraceWindow::showOverlayResult() {
    refreshResultPreview();
    viewModeCombo_->setCurrentText(QString::fromUtf8("叠加结果"));
    updateStatus(QString::fromUtf8("正在查看叠加结果"));
}

void FaultPolyTraceWindow::openResultCsv() {
    const QString path = !lastOutputCsv_.isEmpty() ? lastOutputCsv_ : outputCsvPath();
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, QString::fromUtf8("结果文件不存在"), path);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void FaultPolyTraceWindow::openResultGeoJson() {
    const QString csvPath = !lastOutputCsv_.isEmpty() ? lastOutputCsv_ : outputCsvPath();
    const QFileInfo csvInfo(csvPath);
    const QString path = csvInfo.absolutePath() + "/" + csvInfo.completeBaseName() + ".geojson";
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, QString::fromUtf8("GeoJSON 不存在"), path);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void FaultPolyTraceWindow::openDiagnosticsJson() {
    const QString csvPath = !lastOutputCsv_.isEmpty() ? lastOutputCsv_ : outputCsvPath();
    const QFileInfo csvInfo(csvPath);
    const QString path = csvInfo.absolutePath() + "/" + csvInfo.completeBaseName() + ".diagnostics.json";
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, QString::fromUtf8("诊断 JSON 不存在"), path);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void FaultPolyTraceWindow::exportPng() {
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出当前视图 PNG"), QDir(outputDirEdit_->text()).absoluteFilePath("FaultPolyTrace_View.png"), QString::fromUtf8("PNG 图像 (*.png);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    if (view_->savePng(path)) {
        appendLog(QString::fromUtf8("PNG 已导出：%1").arg(QDir::toNativeSeparators(path)));
    }
}

void FaultPolyTraceWindow::saveTemplate() {
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("保存参数模板"), QDir(outputDirEdit_->text()).absoluteFilePath("FaultPolyTrace_参数模板.json"), QString::fromUtf8("参数模板 (*.json);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    QJsonObject obj;
    obj["preset"] = presetCombo_->currentText();
    obj["traceMode"] = traceModeCombo_->currentText();
    obj["thresholdMode"] = thresholdModeCombo_->currentText();
    obj["percentile"] = percentileSpin_->value();
    obj["threshold"] = thresholdSpin_->value();
    obj["minArea"] = minAreaSpin_->value();
    obj["closeRadius"] = closeRadiusSpin_->value();
    obj["weakPercentile"] = weakPercentileSpin_->value();
    obj["weakConnectRadius"] = weakConnectRadiusSpin_->value();
    obj["beltRadius"] = beltRadiusSpin_->value();
    obj["postBeltCloseRadius"] = postBeltCloseRadiusSpin_->value();
    obj["attributeConstrainedBelt"] = attributeConstrainedBeltCheck_->isChecked();
    obj["attributeEnvelopeRadius"] = attributeEnvelopeRadiusSpin_->value();
    obj["minBranchLength"] = minBranchLengthSpin_->value();
    obj["minJunctionBranchLength"] = minJunctionBranchLengthSpin_->value();
    obj["minJunctionBranchMean"] = minJunctionBranchMeanSpin_->value();
    obj["maxGapDistance"] = maxGapDistanceSpin_->value();
    obj["maxGapAngle"] = maxGapAngleSpin_->value();
    obj["rootAntiMerge"] = rootAntiMergeCheck_->isChecked();
    obj["rootAntiMergeRadius"] = rootAntiMergeRadiusSpin_->value();
    obj["rootAntiMergeSupportRatio"] = rootAntiMergeSupportRatioSpin_->value();
    obj["rootAntiMergeMinPartCells"] = rootAntiMergeMinPartCellsSpin_->value();
    obj["rootAntiMergeMinPartRatio"] = rootAntiMergeMinPartRatioSpin_->value();
    obj["rootAntiMergeWeakGuardThreshold"] = rootAntiMergeWeakGuardThresholdSpin_->value();
    obj["rootAntiMergeWeakGuardRadius"] = rootAntiMergeWeakGuardRadiusSpin_->value();
    obj["localAntiMerge"] = localAntiMergeCheck_->isChecked();
    obj["localAntiMergeLowThreshold"] = localAntiMergeLowThresholdSpin_->value();
    obj["localAntiMergeBridgeRadius"] = localAntiMergeBridgeRadiusSpin_->value();
    obj["localAntiMergeMinPartCells"] = localAntiMergeMinPartCellsSpin_->value();
    obj["localAntiMergeMinPartRatio"] = localAntiMergeMinPartRatioSpin_->value();
    obj["localAntiMergeMinBridgeCells"] = localAntiMergeMinBridgeCellsSpin_->value();
    obj["localAntiMergeMinLowRatio"] = localAntiMergeMinLowRatioSpin_->value();
    obj["residualTrace"] = residualTraceCheck_->isChecked();
    obj["residualTraceMinLength"] = residualTraceMinLengthSpin_->value();
    obj["residualTraceMaxCells"] = residualTraceMaxCellsSpin_->value();
    obj["smoothIterations"] = smoothIterationsSpin_->value();
    obj["simplifyTolerance"] = simplifyToleranceSpin_->value();
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    }
}

void FaultPolyTraceWindow::loadTemplate() {
    const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("打开参数模板"), outputDirEdit_->text(), QString::fromUtf8("参数模板 (*.json);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    presetCombo_->setCurrentText(obj.value("preset").toString(presetCombo_->currentText()));
    traceModeCombo_->setCurrentText(obj.value("traceMode").toString(traceModeCombo_->currentText()));
    thresholdModeCombo_->setCurrentText(obj.value("thresholdMode").toString(thresholdModeCombo_->currentText()));
    percentileSpin_->setValue(obj.value("percentile").toDouble(percentileSpin_->value()));
    thresholdSpin_->setValue(obj.value("threshold").toDouble(thresholdSpin_->value()));
    minAreaSpin_->setValue(obj.value("minArea").toDouble(minAreaSpin_->value()));
    closeRadiusSpin_->setValue(obj.value("closeRadius").toInt(closeRadiusSpin_->value()));
    weakPercentileSpin_->setValue(obj.value("weakPercentile").toDouble(weakPercentileSpin_->value()));
    weakConnectRadiusSpin_->setValue(obj.value("weakConnectRadius").toInt(weakConnectRadiusSpin_->value()));
    beltRadiusSpin_->setValue(obj.value("beltRadius").toInt(beltRadiusSpin_->value()));
    postBeltCloseRadiusSpin_->setValue(obj.value("postBeltCloseRadius").toInt(postBeltCloseRadiusSpin_->value()));
    attributeConstrainedBeltCheck_->setChecked(obj.value("attributeConstrainedBelt").toBool(attributeConstrainedBeltCheck_->isChecked()));
    attributeEnvelopeRadiusSpin_->setValue(obj.value("attributeEnvelopeRadius").toInt(attributeEnvelopeRadiusSpin_->value()));
    minBranchLengthSpin_->setValue(obj.value("minBranchLength").toInt(minBranchLengthSpin_->value()));
    minJunctionBranchLengthSpin_->setValue(obj.value("minJunctionBranchLength").toInt(minJunctionBranchLengthSpin_->value()));
    minJunctionBranchMeanSpin_->setValue(obj.value("minJunctionBranchMean").toDouble(minJunctionBranchMeanSpin_->value()));
    maxGapDistanceSpin_->setValue(obj.value("maxGapDistance").toInt(maxGapDistanceSpin_->value()));
    maxGapAngleSpin_->setValue(obj.value("maxGapAngle").toDouble(maxGapAngleSpin_->value()));
    rootAntiMergeCheck_->setChecked(obj.value("rootAntiMerge").toBool(rootAntiMergeCheck_->isChecked()));
    rootAntiMergeRadiusSpin_->setValue(obj.value("rootAntiMergeRadius").toInt(rootAntiMergeRadiusSpin_->value()));
    rootAntiMergeSupportRatioSpin_->setValue(obj.value("rootAntiMergeSupportRatio").toDouble(rootAntiMergeSupportRatioSpin_->value()));
    rootAntiMergeMinPartCellsSpin_->setValue(obj.value("rootAntiMergeMinPartCells").toInt(rootAntiMergeMinPartCellsSpin_->value()));
    rootAntiMergeMinPartRatioSpin_->setValue(obj.value("rootAntiMergeMinPartRatio").toDouble(rootAntiMergeMinPartRatioSpin_->value()));
    rootAntiMergeWeakGuardThresholdSpin_->setValue(obj.value("rootAntiMergeWeakGuardThreshold").toDouble(rootAntiMergeWeakGuardThresholdSpin_->value()));
    rootAntiMergeWeakGuardRadiusSpin_->setValue(obj.value("rootAntiMergeWeakGuardRadius").toInt(rootAntiMergeWeakGuardRadiusSpin_->value()));
    localAntiMergeCheck_->setChecked(obj.value("localAntiMerge").toBool(localAntiMergeCheck_->isChecked()));
    localAntiMergeLowThresholdSpin_->setValue(obj.value("localAntiMergeLowThreshold").toDouble(localAntiMergeLowThresholdSpin_->value()));
    localAntiMergeBridgeRadiusSpin_->setValue(obj.value("localAntiMergeBridgeRadius").toInt(localAntiMergeBridgeRadiusSpin_->value()));
    localAntiMergeMinPartCellsSpin_->setValue(obj.value("localAntiMergeMinPartCells").toInt(localAntiMergeMinPartCellsSpin_->value()));
    localAntiMergeMinPartRatioSpin_->setValue(obj.value("localAntiMergeMinPartRatio").toDouble(localAntiMergeMinPartRatioSpin_->value()));
    localAntiMergeMinBridgeCellsSpin_->setValue(obj.value("localAntiMergeMinBridgeCells").toInt(localAntiMergeMinBridgeCellsSpin_->value()));
    localAntiMergeMinLowRatioSpin_->setValue(obj.value("localAntiMergeMinLowRatio").toDouble(localAntiMergeMinLowRatioSpin_->value()));
    residualTraceCheck_->setChecked(obj.value("residualTrace").toBool(residualTraceCheck_->isChecked()));
    residualTraceMinLengthSpin_->setValue(obj.value("residualTraceMinLength").toInt(residualTraceMinLengthSpin_->value()));
    residualTraceMaxCellsSpin_->setValue(obj.value("residualTraceMaxCells").toInt(residualTraceMaxCellsSpin_->value()));
    smoothIterationsSpin_->setValue(obj.value("smoothIterations").toInt(smoothIterationsSpin_->value()));
    simplifyToleranceSpin_->setValue(obj.value("simplifyTolerance").toDouble(simplifyToleranceSpin_->value()));
    updateOptionControls();
    updateTaskPreview();
}

void FaultPolyTraceWindow::restoreDefaults() {
    inputEdit_->setText(defaultInputPath());
    outputDirEdit_->setText(defaultOutputDir());
    outputNameEdit_->setText("FaultPolyTrace_Result");
    presetCombo_->setCurrentText(QString::fromUtf8("防黏连增强"));
    applyPreset(presetCombo_->currentText());
    thresholdModeCombo_->setCurrentText("percentile");
    colorEdit_->setText("#00FFFF");
    lineWidthSpin_->setValue(0.03);
    alphaSpin_->setValue(1.0);
    traceModeCombo_->setCurrentText("ridge-belt");
    updateOptionControls();
    updateTaskPreview();
    refreshInputPreview();
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
        presetCombo_->currentText()
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
    presetCombo_->setCurrentText(parts[4]);
    lastOutputCsv_ = parts[2];
    refreshInputPreview();
    refreshResultPreview();
    viewModeCombo_->setCurrentText(QString::fromUtf8("叠加结果"));
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

    const QString csvPath = !lastOutputCsv_.isEmpty() ? lastOutputCsv_ : outputCsvPath();
    const QFileInfo csvInfo(csvPath);
    const QString base = csvInfo.absolutePath() + "/" + csvInfo.completeBaseName();
    const QString geoJsonPath = base + ".geojson";
    const QString diagnosticsPath = base + ".diagnostics.json";
    const bool hasCsv = csvInfo.exists() && csvInfo.size() > 0;
    const bool hasGeoJson = QFileInfo::exists(geoJsonPath);
    const bool hasDiagnostics = QFileInfo::exists(diagnosticsPath);
    const bool running = process_ && process_->state() != QProcess::NotRunning;

    if (running) {
        resultSummaryLabel_->setText(QString::fromUtf8("正在计算，结果文件生成后会自动显示。"));
    } else if (hasCsv) {
        resultSummaryLabel_->setText(QString::fromUtf8("最近结果已生成，可在中间视图区查看或打开结果文件。"));
    } else if (!lastOutputCsv_.isEmpty()) {
        resultSummaryLabel_->setText(QString::fromUtf8("运行结束，但未找到结果 CSV。请检查日志和输出目录。"));
    } else {
        resultSummaryLabel_->setText(QString::fromUtf8("尚未生成结果。运行完成后将自动切换到叠加结果。"));
    }

    resultPathLabel_->setText(hasCsv
        ? QString::fromUtf8("CSV：%1").arg(QDir::toNativeSeparators(csvPath))
        : QString::fromUtf8("预计输出：%1").arg(QDir::toNativeSeparators(csvPath)));
    resultPathLabel_->setToolTip(QDir::toNativeSeparators(csvPath));

    if (showOverlayButton_) {
        showOverlayButton_->setEnabled(hasCsv);
    }
    if (openCsvButton_) {
        openCsvButton_->setEnabled(hasCsv);
    }
    if (openGeoJsonButton_) {
        openGeoJsonButton_->setEnabled(hasGeoJson);
    }
    if (openDiagnosticsButton_) {
        openDiagnosticsButton_->setEnabled(hasDiagnostics);
    }
    if (openOutputDirButton_) {
        openOutputDirButton_->setEnabled(true);
    }
    if (exportPngButton_) {
        exportPngButton_->setEnabled(true);
    }
}

void FaultPolyTraceWindow::openHelp() {
    QMessageBox::information(this, QString::fromUtf8("FaultPolyTrace 帮助"),
                             QString::fromUtf8("FaultPolyTrace 独立软件支持两种数据源。\n\n"
                                               "外部文件流程：\n"
                                               "1. 数据源选择【外部文件】。\n"
                                               "2. 选择 DAT、矩阵文本或 GeoEast DAT 文件。\n"
                                               "3. 设置输出目录、输出名称和追踪参数。\n"
                                               "4. 点击【开始追踪】，完成后自动切换到【叠加结果】。\n\n"
                                               "GeoEast 数据库流程：\n"
                                               "1. 数据源选择【GeoEast数据库】。\n"
                                               "2. 点击【数据库配置】确认 GeoEast 根目录、数据根目录、数据库、项目和用户。\n"
                                               "3. 点击【刷新网格】读取 BOMapGrid 列表。\n"
                                               "4. 设置输出多边形名称后点击【开始追踪】。\n"
                                               "5. 结果写入 GeoEast 数据库，同时在本软件生成真实图层缓存用于查看。\n\n"
                                               "在左侧【结果查看】中打开 CSV、GeoJSON、诊断 JSON 或输出目录，也可导出当前 PNG。"));
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
