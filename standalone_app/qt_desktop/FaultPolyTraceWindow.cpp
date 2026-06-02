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
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTextEdit>
#include <QTextCursor>
#include <QTextStream>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
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

QLabel* formLabel(const QString& text, QWidget* parent) {
    QLabel* label = new QLabel(text, parent);
    label->setFixedWidth(76);
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
    layout->setColumnMinimumWidth(0, 76);
    layout->setColumnStretch(1, 1);
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

    bool loadGrid(const QString& path, const QString& format, QString* error) {
        grid_.clear();
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
        const QStringList lines = QString::fromLocal8Bit(file.readAll()).split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
        if (lines.isEmpty()) {
            if (error) {
                *error = QString::fromUtf8("输入文件为空。");
            }
            update();
            return false;
        }

        const QVector<double> first = splitNumbers(lines.value(0));
        const QVector<double> second = splitNumbers(lines.value(1));
        const QVector<double> third = splitNumbers(lines.value(2));
        const QString fmt = format.toLower();
        const bool matrixLike = fmt == "matrix" ||
            (fmt == "auto" && first.size() == 2 && second.size() == 2 && third.size() == 2 && lines.size() > 3);

        if (matrixLike) {
            nx_ = qRound(first[0]);
            ny_ = qRound(first[1]);
            if (nx_ <= 0 || ny_ <= 0) {
                if (error) {
                    *error = QString::fromUtf8("矩阵网格头无效。");
                }
                return false;
            }
            grid_.reserve(nx_ * ny_);
            for (int i = 3; i < lines.size(); ++i) {
                for (double value : splitNumbers(lines[i])) {
                    grid_ << value;
                }
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
            xMax_ = xMin_ + dx * qMax(1, nx_ - 1);
            yMax_ = yMin_ + dy * qMax(1, ny_ - 1);
        } else {
            struct Sample {
                double line;
                double cmp;
                double x;
                double y;
                double value;
            };
            QVector<Sample> samples;
            QVector<double> lineKeys;
            QVector<double> cmpKeys;
            for (const QString& line : lines) {
                const QVector<double> values = splitNumbers(line);
                if (values.size() < 5) {
                    continue;
                }
                samples << Sample{values[0], values[1], values[2], values[3], values[4]};
                lineKeys << values[0];
                cmpKeys << values[1];
            }
            auto uniqueSort = [](QVector<double>& values) {
                std::sort(values.begin(), values.end());
                values.erase(std::unique(values.begin(), values.end(), [](double a, double b) {
                    return qAbs(a - b) < 1e-9;
                }), values.end());
            };
            uniqueSort(lineKeys);
            uniqueSort(cmpKeys);
            if (samples.isEmpty() || lineKeys.isEmpty() || cmpKeys.isEmpty()) {
                if (error) {
                    *error = QString::fromUtf8("未识别到 GeoEast DAT 五列采样数据。");
                }
                return false;
            }
            nx_ = cmpKeys.size();
            ny_ = lineKeys.size();
            grid_.fill(0.0, nx_ * ny_);
            xMin_ = yMin_ = std::numeric_limits<double>::max();
            xMax_ = yMax_ = -std::numeric_limits<double>::max();
            for (const Sample& sample : samples) {
                const int row = std::lower_bound(lineKeys.begin(), lineKeys.end(), sample.line) - lineKeys.begin();
                const int col = std::lower_bound(cmpKeys.begin(), cmpKeys.end(), sample.cmp) - cmpKeys.begin();
                if (row >= 0 && row < ny_ && col >= 0 && col < nx_) {
                    grid_[row * nx_ + col] = sample.value;
                }
                xMin_ = qMin(xMin_, sample.x);
                yMin_ = qMin(yMin_, sample.y);
                xMax_ = qMax(xMax_, sample.x);
                yMax_ = qMax(yMax_, sample.y);
            }
            if (qFuzzyCompare(xMin_, xMax_) || qFuzzyCompare(yMin_, yMax_)) {
                xMin_ = 0.0;
                yMin_ = 0.0;
                xMax_ = qMax(1, nx_ - 1);
                yMax_ = qMax(1, ny_ - 1);
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
        QRegularExpression numberRe("[-+]?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?");
        double pxMin = std::numeric_limits<double>::max();
        double pyMin = std::numeric_limits<double>::max();
        double pxMax = -std::numeric_limits<double>::max();
        double pyMax = -std::numeric_limits<double>::max();
        for (const QString& line : lines) {
            if (!line.contains("POLYGON", Qt::CaseInsensitive) || !line.contains("kept", Qt::CaseInsensitive)) {
                continue;
            }
            const QString wkt = line.mid(line.indexOf("POLYGON", 0, Qt::CaseInsensitive));
            QRegularExpressionMatchIterator iterator = numberRe.globalMatch(wkt);
            QVector<double> numbers;
            while (iterator.hasNext()) {
                numbers << iterator.next().captured(0).toDouble();
            }
            QPolygonF polygon;
            for (int i = 0; i + 1 < numbers.size(); i += 2) {
                const QPointF point(numbers[i], numbers[i + 1]);
                polygon << point;
                pxMin = qMin(pxMin, point.x());
                pyMin = qMin(pyMin, point.y());
                pxMax = qMax(pxMax, point.x());
                pyMax = qMax(pyMax, point.y());
            }
            if (polygon.size() >= 3) {
                polygons_ << polygon;
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
        for (int x = 0; x < width(); x += 48) {
            painter.drawLine(x, 0, x, height());
        }
        for (int y = 0; y < height(); y += 48) {
            painter.drawLine(0, y, width(), y);
        }

        QRectF canvas = rect().adjusted(24, 54, -24, -24);
        if (mode_ == Mode::Task) {
            drawTaskPreview(painter, canvas);
            return;
        }
        if (!grid_.isEmpty() && mode_ != Mode::Result) {
            drawGrid(painter, canvas);
        }
        if (!polygons_.isEmpty() && mode_ != Mode::Raw) {
            drawPolygons(painter, canvas);
        }

        QFont title = painter.font();
        title.setPointSize(15);
        title.setBold(true);
        painter.setFont(title);
        painter.setPen(QColor("#666666"));
        const QString modeText = mode_ == Mode::Raw ? QString::fromUtf8("原始网格") :
            (mode_ == Mode::Result ? QString::fromUtf8("结果多边形") : QString::fromUtf8("叠加结果"));
        painter.drawText(QPoint(24, 34), QString::fromUtf8("[视图区] %1").arg(modeText));
        painter.setFont(QFont(painter.font().family(), 9));
        painter.setPen(QColor("#777777"));
        painter.drawText(QRect(width() - 280, 18, 250, 20), Qt::AlignRight, status_);
        if (grid_.isEmpty() && polygons_.isEmpty()) {
            painter.drawText(canvas, Qt::AlignCenter, QString::fromUtf8("请选择输入网格，或载入历史结果。"));
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
        QColor color = color_;
        color.setAlphaF(alpha_);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(color, qMax(1.2, lineWidth_ * 42.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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

    void drawTaskPreview(QPainter& painter, const QRectF& canvas) const {
        QFont titleFont = painter.font();
        titleFont.setPointSize(16);
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.setPen(QColor("#7A7A7A"));
        painter.drawText(QPoint(26, 38), QString::fromUtf8("[任务视图区] GeoEast Basemap 参数预览"));

        QFont hintFont = painter.font();
        hintFont.setPointSize(10);
        hintFont.setBold(false);
        painter.setFont(hintFont);
        painter.setPen(QColor("#666666"));
        painter.drawText(rect().adjusted(26, 60, -26, -26), Qt::AlignLeft | Qt::AlignTop, taskLines_.join('\n'));

        const QRectF previewRect(width() - 300, height() - 190, 250, 130);
        if (previewRect.left() > 360 && previewRect.top() > 170) {
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
            QColor color = color_;
            color.setAlphaF(alpha_);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(color, qMax(1.5, lineWidth_ * 40.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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
                         QString::fromUtf8("查看模式  缩放 100%"));
    }

    Mode mode_ = Mode::Task;
    QVector<double> grid_;
    QVector<QPolygonF> polygons_;
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
    QString status_ = QString::fromUtf8("等待数据");
    QStringList taskLines_;
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

        const QRect bar(width() / 2 - 8, 34, 16, qMax(120, height() - 84));
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
        painter.drawText(QRect(4, bar.top() - 20, width() - 8, 18), Qt::AlignCenter, QString::fromUtf8("1.0"));
        painter.setPen(QColor("#FF0000"));
        painter.drawText(QRect(4, bar.bottom() + 6, width() - 8, 18), Qt::AlignCenter, QString::fromUtf8("0.0"));
    }
};

FaultPolyTraceWindow::FaultPolyTraceWindow(QWidget* parent)
    : QMainWindow(parent), process_(new QProcess(this)) {
    buildUi();
    loadSettings();
    refreshHistory();
    refreshInputPreview();
    updateTaskPreview();
    connect(process_, &QProcess::readyReadStandardOutput, this, &FaultPolyTraceWindow::onReadyRead);
    connect(process_, &QProcess::readyReadStandardError, this, &FaultPolyTraceWindow::onReadyRead);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &FaultPolyTraceWindow::onFinished);
    connect(process_, &QProcess::errorOccurred, this, &FaultPolyTraceWindow::onProcessError);
}

void FaultPolyTraceWindow::buildUi() {
    setWindowTitle(QString::fromUtf8("FaultPolyTrace"));
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
        QTextEdit { background: #F8F9FA; border-top: 1px solid #B8B8B8; font-family: Consolas, "Courier New"; }
    )"));

    QAction* openAction = new QAction(style()->standardIcon(QStyle::SP_DirOpenIcon), QString::fromUtf8("打开 DAT 文件"), this);
    openAction->setToolTip(QString::fromUtf8("选择外部断层属性网格文件"));
    connect(openAction, &QAction::triggered, this, &FaultPolyTraceWindow::browseInput);
    QAction* runAction = new QAction(style()->standardIcon(QStyle::SP_MediaPlay), QString::fromUtf8("开始追踪"), this);
    runAction->setToolTip(QString::fromUtf8("按当前参数运行断层多边形自动追踪"));
    connect(runAction, &QAction::triggered, this, &FaultPolyTraceWindow::runTrace);
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

    QMenu* geoEastMenu = menuBar()->addMenu(QString::fromUtf8("GeoEast(&G)"));
    geoEastMenu->addAction(helpAction);

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

    QMenu* faultMenu = menuBar()->addMenu(QString::fromUtf8("断层(&L)"));
    faultMenu->addAction(runAction);

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
    toolbar->addAction(resetAction);
    toolbar->addAction(pngAction);
    toolbar->addSeparator();
    toolbar->addAction(loadTplAction);
    toolbar->addAction(outputDirAction);
    toolbar->addSeparator();
    toolbar->addAction(helpAction);

    QWidget* root = new QWidget(this);
    QHBoxLayout* rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QWidget* leftPane = new QWidget(root);
    leftPane->setFixedWidth(372);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(10, 10, 10, 8);
    leftLayout->setSpacing(8);

    QScrollArea* sidebarScroll = new QScrollArea(leftPane);
    sidebarScroll->setWidgetResizable(true);
    sidebarScroll->setFrameShape(QFrame::NoFrame);
    sidebarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget* sidebarContent = new QWidget(sidebarScroll);
    sidebarContent->setMinimumWidth(336);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebarContent);
    sidebarLayout->setContentsMargins(0, 0, 4, 0);
    sidebarLayout->setSpacing(8);

    QGroupBox* dataBox = new QGroupBox(QString::fromUtf8("数据与输出"), sidebarContent);
    QGridLayout* dataLayout = new QGridLayout(dataBox);
    configureFormGrid(dataLayout);
    inputEdit_ = new QLineEdit(dataBox);
    inputEdit_->setToolTip(QString::fromUtf8("外部断层属性网格文件路径"));
    QPushButton* inputButton = new QPushButton(QString::fromUtf8("浏览"), dataBox);
    connect(inputButton, &QPushButton::clicked, this, &FaultPolyTraceWindow::browseInput);
    QWidget* inputRow = new QWidget(dataBox);
    QHBoxLayout* inputRowLayout = new QHBoxLayout(inputRow);
    inputRowLayout->setContentsMargins(0, 0, 0, 0);
    inputRowLayout->addWidget(inputEdit_, 1);
    inputRowLayout->addWidget(inputButton);
    addRow(dataLayout, 0, QString::fromUtf8("输入文件"), inputRow, dataBox);
    inputFormatCombo_ = new QComboBox(dataBox);
    inputFormatCombo_->addItems({"auto", "matrix", "geoeast-dat"});
    addRow(dataLayout, 1, QString::fromUtf8("输入格式"), inputFormatCombo_, dataBox);
    outputDirEdit_ = new QLineEdit(dataBox);
    outputDirEdit_->setToolTip(QString::fromUtf8("结果文件输出目录"));
    QPushButton* outputButton = new QPushButton(QString::fromUtf8("浏览"), dataBox);
    connect(outputButton, &QPushButton::clicked, this, &FaultPolyTraceWindow::browseOutputDir);
    QWidget* outputRow = new QWidget(dataBox);
    QHBoxLayout* outputRowLayout = new QHBoxLayout(outputRow);
    outputRowLayout->setContentsMargins(0, 0, 0, 0);
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
    connect(colorButton, &QPushButton::clicked, this, &FaultPolyTraceWindow::chooseColor);
    QWidget* colorRow = new QWidget(styleBox);
    QHBoxLayout* colorRowLayout = new QHBoxLayout(colorRow);
    colorRowLayout->setContentsMargins(0, 0, 0, 0);
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
    sidebarLayout->addWidget(styleBox);

    QGroupBox* basicBox = new QGroupBox(QString::fromUtf8("基础追踪参数"), sidebarContent);
    QGridLayout* basicLayout = new QGridLayout(basicBox);
    configureFormGrid(basicLayout);
    presetCombo_ = new QComboBox(basicBox);
    presetCombo_->addItems({QString::fromUtf8("防黏连增强"), QString::fromUtf8("高召回"), QString::fromUtf8("保守高精度")});
    addRow(basicLayout, 0, QString::fromUtf8("预设"), presetCombo_, basicBox);
    thresholdModeCombo_ = new QComboBox(basicBox);
    thresholdModeCombo_->addItems({"percentile", "positive-percentile", "fixed"});
    addRow(basicLayout, 1, QString::fromUtf8("阈值模式"), thresholdModeCombo_, basicBox);
    percentileSpin_ = new QDoubleSpinBox(basicBox);
    percentileSpin_->setRange(0.0, 1.0);
    percentileSpin_->setDecimals(4);
    percentileSpin_->setValue(0.98);
    addRow(basicLayout, 2, QString::fromUtf8("分位数"), percentileSpin_, basicBox);
    thresholdSpin_ = new QDoubleSpinBox(basicBox);
    thresholdSpin_->setRange(0.0, 1.0);
    thresholdSpin_->setDecimals(4);
    thresholdSpin_->setValue(0.95);
    addRow(basicLayout, 3, QString::fromUtf8("固定阈值"), thresholdSpin_, basicBox);
    minAreaSpin_ = new QDoubleSpinBox(basicBox);
    minAreaSpin_->setRange(0, 1000000000);
    minAreaSpin_->setDecimals(2);
    minAreaSpin_->setValue(120.0);
    addRow(basicLayout, 4, QString::fromUtf8("最小面积"), minAreaSpin_, basicBox);
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
    maxGapDistanceSpin_ = new QSpinBox(topologyBox);
    maxGapDistanceSpin_->setRange(0, 1000);
    maxGapDistanceSpin_->setValue(12);
    addRow(topologyLayout, 2, QString::fromUtf8("最大断距"), maxGapDistanceSpin_, topologyBox);
    maxGapAngleSpin_ = new QDoubleSpinBox(topologyBox);
    maxGapAngleSpin_->setRange(0, 180);
    maxGapAngleSpin_->setValue(55.0);
    addRow(topologyLayout, 3, QString::fromUtf8("最大夹角"), maxGapAngleSpin_, topologyBox);
    smoothIterationsSpin_ = new QSpinBox(topologyBox);
    smoothIterationsSpin_->setRange(0, 5);
    smoothIterationsSpin_->setValue(1);
    addRow(topologyLayout, 4, QString::fromUtf8("平滑次数"), smoothIterationsSpin_, topologyBox);
    simplifyToleranceSpin_ = new QDoubleSpinBox(topologyBox);
    simplifyToleranceSpin_->setRange(0, 1000);
    simplifyToleranceSpin_->setValue(2.0);
    addRow(topologyLayout, 5, QString::fromUtf8("简化容差"), simplifyToleranceSpin_, topologyBox);
    sidebarLayout->addWidget(topologyBox);

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
    logEdit_ = new QTextEdit(right);
    logEdit_->setReadOnly(true);
    logEdit_->setFixedHeight(120);
    rightLayout->addWidget(viewStrip, 1);
    rightLayout->addWidget(logEdit_);
    rootLayout->addWidget(leftPane);
    rootLayout->addWidget(right, 1);
    setCentralWidget(root);

    statusLabel_ = new QLabel(QString::fromUtf8("就绪"), this);
    elapsedLabel_ = new QLabel(QString::fromUtf8("耗时: --"), this);
    statusBar()->addWidget(statusLabel_, 1);
    statusBar()->addPermanentWidget(elapsedLabel_);

    connect(inputEdit_, &QLineEdit::editingFinished, this, &FaultPolyTraceWindow::refreshInputPreview);
    connect(inputFormatCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::refreshInputPreview);
    connect(viewModeCombo_, &QComboBox::currentTextChanged, this, [this]() {
        if (viewModeCombo_->currentText() == QString::fromUtf8("任务预览")) {
            view_->setMode(AttributeView::Mode::Task);
        } else if (viewModeCombo_->currentText() == QString::fromUtf8("原始网格")) {
            view_->setMode(AttributeView::Mode::Raw);
        } else if (viewModeCombo_->currentText() == QString::fromUtf8("结果多边形")) {
            view_->setMode(AttributeView::Mode::Result);
        } else {
            view_->setMode(AttributeView::Mode::Overlay);
        }
        updateTaskPreview();
    });
    auto styleChanged = [this]() {
        view_->setStyle(QColor(colorEdit_->text()), lineWidthSpin_->value(), alphaSpin_->value());
        updateTaskPreview();
    };
    connect(lineWidthSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, styleChanged);
    connect(alphaSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, styleChanged);
    connect(presetCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::applyPreset);
    connect(inputEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(outputDirEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(outputNameEdit_, &QLineEdit::textChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(inputFormatCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(presetCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(thresholdModeCombo_, &QComboBox::currentTextChanged, this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(percentileSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(thresholdSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FaultPolyTraceWindow::updateTaskPreview);
    connect(minAreaSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FaultPolyTraceWindow::updateTaskPreview);
}

QString FaultPolyTraceWindow::coreRunnerPath() const {
    QDir app(QCoreApplication::applicationDirPath());
    const QString local = app.absoluteFilePath("faultpolytrace.exe");
    QDir parent(app);
    QString packaged;
    if (parent.cdUp()) {
        packaged = parent.absoluteFilePath("bin/faultpolytrace.exe");
    }
    QDir root(repoRootFromAppDir());
    for (const QString& path : {local, packaged, root.absoluteFilePath("build/faultpolytrace.exe"), root.absoluteFilePath("bin/faultpolytrace.exe")}) {
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            return QDir::toNativeSeparators(path);
        }
    }
    return QDir::toNativeSeparators(root.absoluteFilePath("build/faultpolytrace.exe"));
}

QString FaultPolyTraceWindow::defaultInputPath() const {
    QDir app(QCoreApplication::applicationDirPath());
    QDir parent(app);
    QString packaged;
    if (parent.cdUp()) {
        packaged = parent.absoluteFilePath("examples/synthetic/broken_ridge_fault_grid.txt");
    }
    QDir root(repoRootFromAppDir());
    for (const QString& path : {packaged, root.absoluteFilePath("examples/synthetic/broken_ridge_fault_grid.txt"), root.absoluteFilePath("examples/synthetic/ridge_fault_grid.txt")}) {
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

void FaultPolyTraceWindow::loadSettings() {
    QSettings settings(settingsPath(), QSettings::IniFormat);
    inputEdit_->setText(settings.value("input", defaultInputPath()).toString());
    outputDirEdit_->setText(settings.value("outputDir", defaultOutputDir()).toString());
    outputNameEdit_->setText(settings.value("outputName", outputNameEdit_->text()).toString());
    inputFormatCombo_->setCurrentText(settings.value("inputFormat", "auto").toString());
    viewModeCombo_->setCurrentText(QString::fromUtf8("任务预览"));
    presetCombo_->setCurrentText(settings.value("preset", presetCombo_->currentText()).toString());
    thresholdModeCombo_->setCurrentText(settings.value("thresholdMode", thresholdModeCombo_->currentText()).toString());
    percentileSpin_->setValue(settings.value("percentile", percentileSpin_->value()).toDouble());
    thresholdSpin_->setValue(settings.value("threshold", thresholdSpin_->value()).toDouble());
    minAreaSpin_->setValue(settings.value("minArea", minAreaSpin_->value()).toDouble());
    closeRadiusSpin_->setValue(settings.value("closeRadius", closeRadiusSpin_->value()).toInt());
    beltRadiusSpin_->setValue(settings.value("beltRadius", beltRadiusSpin_->value()).toInt());
    maxGapDistanceSpin_->setValue(settings.value("maxGapDistance", maxGapDistanceSpin_->value()).toInt());
    maxGapAngleSpin_->setValue(settings.value("maxGapAngle", maxGapAngleSpin_->value()).toDouble());
    smoothIterationsSpin_->setValue(settings.value("smoothIterations", smoothIterationsSpin_->value()).toInt());
    simplifyToleranceSpin_->setValue(settings.value("simplifyTolerance", simplifyToleranceSpin_->value()).toDouble());
    colorEdit_->setText(settings.value("color", colorEdit_->text()).toString());
    lineWidthSpin_->setValue(settings.value("lineWidth", lineWidthSpin_->value()).toDouble());
    alphaSpin_->setValue(settings.value("alpha", alphaSpin_->value()).toDouble());
    lastOutputCsv_ = settings.value("lastOutputCsv").toString();
}

void FaultPolyTraceWindow::saveSettings() const {
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue("input", inputEdit_->text());
    settings.setValue("outputDir", outputDirEdit_->text());
    settings.setValue("outputName", outputNameEdit_->text());
    settings.setValue("inputFormat", inputFormatCombo_->currentText());
    settings.setValue("viewMode", viewModeCombo_->currentText());
    settings.setValue("preset", presetCombo_->currentText());
    settings.setValue("thresholdMode", thresholdModeCombo_->currentText());
    settings.setValue("percentile", percentileSpin_->value());
    settings.setValue("threshold", thresholdSpin_->value());
    settings.setValue("minArea", minAreaSpin_->value());
    settings.setValue("closeRadius", closeRadiusSpin_->value());
    settings.setValue("beltRadius", beltRadiusSpin_->value());
    settings.setValue("maxGapDistance", maxGapDistanceSpin_->value());
    settings.setValue("maxGapAngle", maxGapAngleSpin_->value());
    settings.setValue("smoothIterations", smoothIterationsSpin_->value());
    settings.setValue("simplifyTolerance", simplifyToleranceSpin_->value());
    settings.setValue("color", colorEdit_->text());
    settings.setValue("lineWidth", lineWidthSpin_->value());
    settings.setValue("alpha", alphaSpin_->value());
    settings.setValue("lastOutputCsv", lastOutputCsv_);
}

void FaultPolyTraceWindow::updateTaskPreview() {
    if (!view_) {
        return;
    }
    const QString thresholdText = thresholdModeCombo_->currentText() == "fixed"
        ? QString::fromUtf8("固定阈值 %1").arg(thresholdSpin_->value(), 0, 'f', 4)
        : QString::fromUtf8("分位数 %1").arg(percentileSpin_->value(), 0, 'f', 4);
    const QString inputText = inputEdit_->text().trimmed().isEmpty()
        ? QString::fromUtf8("未选择")
        : QFileInfo(inputEdit_->text().trimmed()).fileName();
    const QString outputText = outputNameEdit_->text().trimmed().isEmpty()
        ? QString::fromUtf8("未命名")
        : outputNameEdit_->text().trimmed();
    const QString outputDirText = outputDirEdit_->text().trimmed().isEmpty()
        ? QString::fromUtf8("默认输出目录")
        : QDir::toNativeSeparators(outputDirEdit_->text().trimmed());

    view_->setTaskSummary({
        QString::fromUtf8("运行方式：独立文件模式    输入格式：%1").arg(inputFormatCombo_->currentText()),
        QString::fromUtf8("输入文件：%1").arg(inputText),
        QString::fromUtf8("输出名称：%1").arg(outputText),
        QString::fromUtf8("输出目录：%1").arg(outputDirText),
        QString::fromUtf8("方案：%1    模式：ridge-belt    阈值：%2").arg(presetCombo_->currentText(), thresholdText),
        QString::fromUtf8("面积过滤：小于 %1 的多边形不输出    结果线宽：%2")
            .arg(minAreaSpin_->value(), 0, 'f', 2)
            .arg(lineWidthSpin_->value(), 0, 'f', 3),
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
         << "--trace-mode" << "ridge-belt"
         << "--threshold-mode" << thresholdModeCombo_->currentText()
         << "--threshold" << QString::number(thresholdSpin_->value(), 'g', 12)
         << "--percentile" << QString::number(percentileSpin_->value(), 'g', 12)
         << "--min-area" << QString::number(minAreaSpin_->value(), 'g', 12)
         << "--close-radius" << QString::number(closeRadiusSpin_->value())
         << "--belt-radius" << QString::number(beltRadiusSpin_->value())
         << "--post-belt-close-radius" << "1"
         << "--weak-percentile" << "0.965"
         << "--weak-connect-radius" << "1"
         << "--attribute-constrained-belt"
         << "--attribute-envelope-radius" << "1"
         << "--min-branch-length" << "4"
         << "--min-junction-branch-length" << "2"
         << "--min-junction-branch-mean" << "0.94"
         << "--max-gap-distance" << QString::number(maxGapDistanceSpin_->value())
         << "--max-gap-angle" << QString::number(maxGapAngleSpin_->value(), 'g', 12)
         << "--min-gap-support-ratio" << "0.25"
         << "--no-separate-branch-belts"
         << "--root-anti-merge"
         << "--root-anti-merge-radius" << "1"
         << "--root-anti-merge-support-ratio" << "0.25"
         << "--root-anti-merge-max-bridge-value" << "1e-12"
         << "--root-anti-merge-min-part-cells" << "0"
         << "--root-anti-merge-min-part-ratio" << "0.12"
         << "--root-anti-merge-weak-guard-threshold" << "0.66"
         << "--root-anti-merge-weak-guard-radius" << "0"
         << "--no-local-anti-merge"
         << "--no-residual-trace"
         << "--residual-percentile" << "0.955"
         << "--residual-fill-radius" << "4"
         << "--residual-edge-fill-radius" << "2"
         << "--residual-edge-fill-min-value-ratio" << "0.97"
         << "--residual-edge-fill-max-patch-span" << "5"
         << "--residual-min-cells" << "2"
         << "--residual-trace-min-length" << "6"
         << "--residual-trace-max-cells" << "80"
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

void FaultPolyTraceWindow::refreshInputPreview() {
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
        return;
    }
    QString error;
    if (!view_->loadPolygons(path, &error) && !error.isEmpty()) {
        appendLog(error);
    }
}

void FaultPolyTraceWindow::runTrace() {
    if (process_->state() != QProcess::NotRunning) {
        return;
    }
    const QString runner = coreRunnerPath();
    if (!QFileInfo::exists(runner)) {
        QMessageBox::warning(this, QString::fromUtf8("算法程序不存在"), runner);
        return;
    }
    if (inputEdit_->text().trimmed().isEmpty() || !QFileInfo::exists(inputEdit_->text().trimmed())) {
        QMessageBox::warning(this, QString::fromUtf8("输入文件无效"), QString::fromUtf8("请选择可读取的外部网格文件。"));
        return;
    }
    logEdit_->clear();
    saveSettings();
    const QStringList args = traceArgs();
    appendLog("> " + runner + " " + quoteForLog(args));
    updateStatus(QString::fromUtf8("正在追踪断层多边形..."));
    elapsedLabel_->setText(QString::fromUtf8("耗时: 计算中"));
    setRunning(true);
    processStartMs_ = QDateTime::currentMSecsSinceEpoch();
    process_->start(runner, args);
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
        appendLog(out);
    }
    if (!err.isEmpty()) {
        appendLog(err);
    }
}

void FaultPolyTraceWindow::onFinished(int exitCode, QProcess::ExitStatus status) {
    onReadyRead();
    setRunning(false);
    const qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - processStartMs_;
    elapsedLabel_->setText(QString::fromUtf8("耗时: %1").arg(elapsedText(elapsedMs)));
    if (exitCode == 0 && status == QProcess::NormalExit) {
        lastOutputCsv_ = outputCsvPath();
        QString geoJsonPath;
        QString geoJsonError;
        if (writeGeoJson(lastOutputCsv_, &geoJsonPath, &geoJsonError)) {
            appendLog(QString::fromUtf8("GeoJSON 文件：%1").arg(QDir::toNativeSeparators(geoJsonPath)));
        } else if (!geoJsonError.isEmpty()) {
            appendLog(geoJsonError);
        }
        refreshResultPreview();
        appendHistory(elapsedMs);
        saveSettings();
        updateStatus(QString::fromUtf8("追踪完成"));
    } else {
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
}

void FaultPolyTraceWindow::appendLog(const QString& text) {
    logEdit_->moveCursor(QTextCursor::End);
    logEdit_->insertPlainText(text.endsWith('\n') ? text : text + "\n");
    logEdit_->moveCursor(QTextCursor::End);
}

void FaultPolyTraceWindow::updateStatus(const QString& text) {
    statusLabel_->setText(QDateTime::currentDateTime().toString("HH:mm:ss  ") + text);
}

void FaultPolyTraceWindow::applyPreset(const QString& preset) {
    if (preset == QString::fromUtf8("高召回")) {
        percentileSpin_->setValue(0.975);
        minAreaSpin_->setValue(80);
        beltRadiusSpin_->setValue(3);
        maxGapDistanceSpin_->setValue(14);
        simplifyToleranceSpin_->setValue(1.5);
        smoothIterationsSpin_->setValue(0);
    } else if (preset == QString::fromUtf8("保守高精度")) {
        percentileSpin_->setValue(0.985);
        minAreaSpin_->setValue(250);
        beltRadiusSpin_->setValue(2);
        maxGapDistanceSpin_->setValue(8);
        simplifyToleranceSpin_->setValue(3.0);
        smoothIterationsSpin_->setValue(1);
    } else {
        percentileSpin_->setValue(0.98);
        minAreaSpin_->setValue(120);
        beltRadiusSpin_->setValue(3);
        maxGapDistanceSpin_->setValue(12);
        simplifyToleranceSpin_->setValue(2.0);
        smoothIterationsSpin_->setValue(1);
    }
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
    obj["thresholdMode"] = thresholdModeCombo_->currentText();
    obj["percentile"] = percentileSpin_->value();
    obj["threshold"] = thresholdSpin_->value();
    obj["minArea"] = minAreaSpin_->value();
    obj["closeRadius"] = closeRadiusSpin_->value();
    obj["beltRadius"] = beltRadiusSpin_->value();
    obj["maxGapDistance"] = maxGapDistanceSpin_->value();
    obj["maxGapAngle"] = maxGapAngleSpin_->value();
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
    thresholdModeCombo_->setCurrentText(obj.value("thresholdMode").toString(thresholdModeCombo_->currentText()));
    percentileSpin_->setValue(obj.value("percentile").toDouble(percentileSpin_->value()));
    thresholdSpin_->setValue(obj.value("threshold").toDouble(thresholdSpin_->value()));
    minAreaSpin_->setValue(obj.value("minArea").toDouble(minAreaSpin_->value()));
    closeRadiusSpin_->setValue(obj.value("closeRadius").toInt(closeRadiusSpin_->value()));
    beltRadiusSpin_->setValue(obj.value("beltRadius").toInt(beltRadiusSpin_->value()));
    maxGapDistanceSpin_->setValue(obj.value("maxGapDistance").toInt(maxGapDistanceSpin_->value()));
    maxGapAngleSpin_->setValue(obj.value("maxGapAngle").toDouble(maxGapAngleSpin_->value()));
    smoothIterationsSpin_->setValue(obj.value("smoothIterations").toInt(smoothIterationsSpin_->value()));
    simplifyToleranceSpin_->setValue(obj.value("simplifyTolerance").toDouble(simplifyToleranceSpin_->value()));
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
}

void FaultPolyTraceWindow::openOutputDir() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(outputDirEdit_->text()));
}

void FaultPolyTraceWindow::openHelp() {
    QMessageBox::information(this, QString::fromUtf8("FaultPolyTrace 帮助"),
                             QString::fromUtf8("独立软件用于处理外部网格文件，不包含 GeoEast 数据库配置。\n\n"
                                               "基本流程：\n"
                                               "1. 选择 DAT、矩阵文本或 GeoEast DAT 文件。\n"
                                               "2. 设置输出目录、输出名称和追踪参数。\n"
                                               "3. 点击【开始追踪】。\n"
                                               "4. 在视图区切换原始网格、结果多边形和叠加结果。\n"
                                               "5. 结果会保存为 CSV、GeoJSON、诊断 JSON、拓扑 CSV 和枝干 CSV。"));
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
