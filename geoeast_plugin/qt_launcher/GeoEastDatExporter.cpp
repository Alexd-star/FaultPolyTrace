#include "GeoEastDatExporter.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointF>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>
#include <QTextStream>
#include <QTemporaryFile>
#include <QVector>

#include <cmath>
#include <limits>

namespace fpt_ui {
namespace {

constexpr int kFaultLevel = 0;
constexpr int kFaultPointTypeUpperWall = 0;
constexpr int kFaultPolygonTypeNormal = 1;
constexpr int kCloseTypeClosed = 1;
constexpr int kCoordinatePrecision = 4;

struct ParsedPolygons {
    QVector<QVector<QPointF>> exteriorRings;
    int omittedInteriorRingCount = 0;
};

bool samePoint(const QPointF& a, const QPointF& b, double tolerance = 1e-6) {
    return std::abs(a.x() - b.x()) <= tolerance &&
           std::abs(a.y() - b.y()) <= tolerance;
}

QString pointKeyAtDatPrecision(const QPointF& point) {
    return QString::number(point.x(), 'f', kCoordinatePrecision) + QLatin1Char('|') +
           QString::number(point.y(), 'f', kCoordinatePrecision);
}

bool parseRingCoordinates(
    const QString& ringText,
    int sourceLine,
    QVector<QPointF>* output,
    QString* error) {
    static const QRegularExpression numberRe(
        QStringLiteral("[-+]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][-+]?\\d+)?"));

    QVector<QPointF> points;
    const QStringList vertices = ringText.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& vertex : vertices) {
        QVector<double> numbers;
        QRegularExpressionMatchIterator matches = numberRe.globalMatch(vertex);
        while (matches.hasNext()) {
            bool ok = false;
            const double value = matches.next().captured(0).toDouble(&ok);
            if (!ok || !std::isfinite(value)) {
                if (error) {
                    *error = QString::fromUtf8("第 %1 行含有无效或非有限坐标。")
                                 .arg(sourceLine);
                }
                return false;
            }
            numbers.push_back(value);
        }
        if (numbers.size() != 2) {
            if (error) {
                *error = QString::fromUtf8("第 %1 行的 WKT 顶点不是严格的二维 X/Y 坐标：%2")
                             .arg(sourceLine)
                             .arg(vertex.trimmed());
            }
            return false;
        }
        const QPointF point(numbers[0], numbers[1]);
        if (points.isEmpty() || !samePoint(points.back(), point)) {
            points.push_back(point);
        }
    }

    while (points.size() >= 3 && samePoint(points[points.size() - 1], points[points.size() - 2])) {
        points.removeLast();
    }
    if (points.size() >= 2 && samePoint(points.front(), points.back())) {
        points.removeLast();
    }

    QVector<QPointF> rounded;
    rounded.reserve(points.size() + 1);
    for (const QPointF& point : points) {
        const double x = QString::number(point.x(), 'f', kCoordinatePrecision).toDouble();
        const double y = QString::number(point.y(), 'f', kCoordinatePrecision).toDouble();
        const QPointF roundedPoint(x, y);
        if (rounded.isEmpty() || !samePoint(rounded.back(), roundedPoint, 1e-12)) {
            rounded.push_back(roundedPoint);
        }
    }
    if (rounded.size() >= 2 && samePoint(rounded.front(), rounded.back(), 1e-12)) {
        rounded.removeLast();
    }

    QStringList uniqueKeys;
    uniqueKeys.reserve(rounded.size());
    for (const QPointF& point : rounded) {
        const QString key = pointKeyAtDatPrecision(point);
        if (!uniqueKeys.contains(key)) {
            uniqueKeys.push_back(key);
        }
    }
    if (uniqueKeys.size() < 3) {
        if (error) {
            *error = QString::fromUtf8("第 %1 行的外环在 DAT 四位精度下少于 3 个不同顶点。")
                         .arg(sourceLine);
        }
        return false;
    }

    long double twiceArea = 0.0L;
    for (int i = 0; i < rounded.size(); ++i) {
        const QPointF& a = rounded[i];
        const QPointF& b = rounded[(i + 1) % rounded.size()];
        twiceArea += static_cast<long double>(a.x()) * static_cast<long double>(b.y()) -
                     static_cast<long double>(b.x()) * static_cast<long double>(a.y());
    }
    if (std::abs(twiceArea) <= 1e-8L) {
        if (error) {
            *error = QString::fromUtf8("第 %1 行的外环面积为零，无法作为断层多边形导出。")
                         .arg(sourceLine);
        }
        return false;
    }

    rounded.push_back(rounded.front());
    *output = rounded;
    return true;
}

bool extractPolygonRings(
    const QString& line,
    int sourceLine,
    QVector<QVector<QPointF>>* rings,
    QString* error) {
    const int polygonIndex = line.indexOf(QStringLiteral("POLYGON"), 0, Qt::CaseInsensitive);
    if (polygonIndex < 0) {
        if (error) {
            *error = QString::fromUtf8("第 %1 行没有 POLYGON WKT。")
                         .arg(sourceLine);
        }
        return false;
    }
    if (polygonIndex >= 5 &&
        line.mid(qMax(0, polygonIndex - 5), 5).compare(QStringLiteral("MULTI"), Qt::CaseInsensitive) == 0) {
        if (error) {
            *error = QString::fromUtf8("第 %1 行为 MULTIPOLYGON；GeoEast BOFaultPolygon DAT 需要逐个 POLYGON。")
                         .arg(sourceLine);
        }
        return false;
    }

    const int outerOpen = line.indexOf(QLatin1Char('('), polygonIndex + 7);
    if (outerOpen < 0) {
        if (error) {
            *error = QString::fromUtf8("第 %1 行的 POLYGON WKT 缺少左括号。")
                         .arg(sourceLine);
        }
        return false;
    }

    int depth = 0;
    int ringStart = -1;
    bool polygonClosed = false;
    for (int i = outerOpen; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('(')) {
            ++depth;
            if (depth == 2) {
                ringStart = i + 1;
            } else if (depth > 2) {
                if (error) {
                    *error = QString::fromUtf8("第 %1 行的 POLYGON WKT 括号层级无效。")
                                 .arg(sourceLine);
                }
                return false;
            }
        } else if (ch == QLatin1Char(')')) {
            if (depth == 2) {
                if (ringStart < 0 || i <= ringStart) {
                    if (error) {
                        *error = QString::fromUtf8("第 %1 行包含空多边形环。")
                                     .arg(sourceLine);
                    }
                    return false;
                }
                QVector<QPointF> ring;
                if (!parseRingCoordinates(line.mid(ringStart, i - ringStart), sourceLine, &ring, error)) {
                    return false;
                }
                rings->push_back(ring);
                ringStart = -1;
            }
            --depth;
            if (depth < 0) {
                if (error) {
                    *error = QString::fromUtf8("第 %1 行的 POLYGON WKT 右括号过多。")
                                 .arg(sourceLine);
                }
                return false;
            }
            if (depth == 0) {
                polygonClosed = true;
                break;
            }
        }
    }
    if (!polygonClosed || depth != 0 || rings->isEmpty()) {
        if (error) {
            *error = QString::fromUtf8("第 %1 行的 POLYGON WKT 未正确闭合。")
                         .arg(sourceLine);
        }
        return false;
    }
    return true;
}

bool readKeptPolygonCsv(const QString& path, ParsedPolygons* parsed, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QString::fromUtf8("无法打开 GeoEast 结果回读文件：%1\n%2")
                         .arg(QFileInfo(path).absoluteFilePath(), file.errorString());
        }
        return false;
    }

    int sourceLine = 0;
    bool sawCsvHeader = false;
    while (!file.atEnd()) {
        ++sourceLine;
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (!sawCsvHeader) {
            sawCsvHeader = line.contains(QStringLiteral("status"), Qt::CaseInsensitive) &&
                           line.contains(QStringLiteral("wkt"), Qt::CaseInsensitive);
            if (sawCsvHeader) {
                continue;
            }
        }

        const int polygonIndex = line.indexOf(QStringLiteral("POLYGON"), 0, Qt::CaseInsensitive);
        if (polygonIndex < 0) {
            continue;
        }
        const QString prefix = line.left(polygonIndex);
        static const QRegularExpression keptField(
            QStringLiteral("(?:^|,)\\s*kept\\s*(?:,|$)"),
            QRegularExpression::CaseInsensitiveOption);
        if (!keptField.match(prefix).hasMatch()) {
            continue;
        }

        QVector<QVector<QPointF>> rings;
        if (!extractPolygonRings(line, sourceLine, &rings, error)) {
            return false;
        }
        parsed->exteriorRings.push_back(rings.front());
        parsed->omittedInteriorRingCount += qMax(0, rings.size() - 1);
    }

    if (!sawCsvHeader) {
        if (error) {
            *error = QString::fromUtf8("结果回读文件不是 FaultPolyTrace 多边形 CSV：缺少 status/wkt 表头。");
        }
        return false;
    }
    if (parsed->exteriorRings.isEmpty()) {
        if (error) {
            *error = QString::fromUtf8("结果回读文件中没有可导出的 kept POLYGON 外环。");
        }
        return false;
    }
    return true;
}

bool parseInteger(const QString& text, int* value) {
    bool ok = false;
    const qlonglong parsed = text.toLongLong(&ok);
    if (!ok || parsed < std::numeric_limits<int>::min() ||
        parsed > std::numeric_limits<int>::max()) {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

bool writeFormatDefinitionCompanion(
    const QString& datPath,
    QString* formatPath,
    QString* error) {
    QFile resource(QStringLiteral(":/faultpolytrace/formats/default_full.fmt"));
    if (!resource.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QString::fromUtf8("程序内置的 GeoEast default_full.fmt 资源不可用。");
        }
        return false;
    }

    const QFileInfo datInfo(datPath);
    const QString companionPath = datInfo.dir().absoluteFilePath(
        datInfo.completeBaseName() + QStringLiteral(".fmt"));
    QSaveFile output(companionPath);
    const QByteArray formatBytes = resource.readAll();
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text) ||
        output.write(formatBytes) != formatBytes.size() || !output.commit()) {
        if (error) {
            *error = QString::fromUtf8("无法写出 GeoEast 格式定义文件：%1\n%2")
                         .arg(companionPath, output.errorString());
        }
        return false;
    }
    if (formatPath) {
        *formatPath = companionPath;
    }
    return true;
}

}  // namespace

QString normalizedGeoEastFaultPolygonName(const QString& requestedName) {
    QString name = requestedName.trimmed();
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]+")), QStringLiteral("_"));
    name.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    while (name.startsWith(QLatin1Char('_'))) {
        name.remove(0, 1);
    }
    while (name.endsWith(QLatin1Char('_'))) {
        name.chop(1);
    }
    if (name.isEmpty()) {
        name = QStringLiteral("lc_result");
    }
    return name.left(16);
}

bool validateGeoEastFaultPolygonDat(
    const QString& datPath,
    const QString& expectedFaultPolygonName,
    int expectedPolygonCount,
    qint64 expectedPointCount,
    QString* error) {
    QFile file(datPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QString::fromUtf8("无法重新打开 DAT 进行校验：%1\n%2")
                         .arg(QFileInfo(datPath).absoluteFilePath(), file.errorString());
        }
        return false;
    }

    const QString expectedHeader = QStringLiteral(
        "#faultPolygonName x y connectionFlag faultLevel faultPointType faultPolygonType closeType");
    const QString header = QString::fromLatin1(file.readLine()).trimmed();
    if (header != expectedHeader) {
        if (error) {
            *error = QString::fromUtf8("DAT 首行表头不符合 GeoEast default_full.fmt 八列定义。");
        }
        return false;
    }

    int lineNumber = 1;
    int currentFlag = 0;
    int polygonCount = 0;
    int currentPointCount = 0;
    qint64 totalPointCount = 0;
    QPointF firstPoint;
    QPointF lastPoint;
    auto finishSegment = [&]() -> bool {
        if (currentFlag == 0) {
            return true;
        }
        if (currentPointCount < 4 || !samePoint(firstPoint, lastPoint, 1e-12)) {
            if (error) {
                *error = QString::fromUtf8("connectionFlag=%1 的环未闭合或少于 4 行（含重复首点）。")
                             .arg(currentFlag);
            }
            return false;
        }
        ++polygonCount;
        return true;
    };

    while (!file.atEnd()) {
        ++lineNumber;
        const QString line = QString::fromLatin1(file.readLine()).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QStringList columns = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (columns.size() != 8) {
            if (error) {
                *error = QString::fromUtf8("DAT 第 %1 行不是 8 列，而是 %2 列。")
                             .arg(lineNumber)
                             .arg(columns.size());
            }
            return false;
        }
        if (columns[0] != expectedFaultPolygonName) {
            if (error) {
                *error = QString::fromUtf8("DAT 第 %1 行的 faultPolygonName 不一致。")
                             .arg(lineNumber);
            }
            return false;
        }

        bool xOk = false;
        bool yOk = false;
        const double x = columns[1].toDouble(&xOk);
        const double y = columns[2].toDouble(&yOk);
        int flag = 0;
        int level = 0;
        int pointType = 0;
        int polygonType = 0;
        int closeType = 0;
        if (!xOk || !yOk || !std::isfinite(x) || !std::isfinite(y) ||
            !parseInteger(columns[3], &flag) || !parseInteger(columns[4], &level) ||
            !parseInteger(columns[5], &pointType) || !parseInteger(columns[6], &polygonType) ||
            !parseInteger(columns[7], &closeType)) {
            if (error) {
                *error = QString::fromUtf8("DAT 第 %1 行含有无效数值。")
                             .arg(lineNumber);
            }
            return false;
        }
        if (flag <= 0 || level != kFaultLevel || pointType != kFaultPointTypeUpperWall ||
            polygonType != kFaultPolygonTypeNormal || closeType != kCloseTypeClosed) {
            if (error) {
                *error = QString::fromUtf8("DAT 第 %1 行的标志字段不符合 0/0/1/1 正断层闭合约定。")
                             .arg(lineNumber);
            }
            return false;
        }

        const QPointF point(x, y);
        if (flag != currentFlag) {
            if (!finishSegment()) {
                return false;
            }
            if (flag != currentFlag + 1) {
                if (error) {
                    *error = QString::fromUtf8("DAT 第 %1 行的 connectionFlag 不连续；期望 %2，实际 %3。")
                                 .arg(lineNumber)
                                 .arg(currentFlag + 1)
                                 .arg(flag);
                }
                return false;
            }
            currentFlag = flag;
            currentPointCount = 0;
            firstPoint = point;
        }
        lastPoint = point;
        ++currentPointCount;
        ++totalPointCount;
    }
    if (!finishSegment()) {
        return false;
    }

    if (polygonCount != expectedPolygonCount || totalPointCount != expectedPointCount) {
        if (error) {
            *error = QString::fromUtf8("DAT 写出后计数不一致：多边形 %1/%2，点行 %3/%4。")
                         .arg(polygonCount)
                         .arg(expectedPolygonCount)
                         .arg(totalPointCount)
                         .arg(expectedPointCount);
        }
        return false;
    }
    return true;
}

bool exportGeoEastFaultPolygonDat(
    const QString& polygonCsvPath,
    const QString& outputDatPath,
    const QString& requestedFaultPolygonName,
    GeoEastDatExportStats* stats,
    QString* error) {
    ParsedPolygons parsed;
    if (!readKeptPolygonCsv(polygonCsvPath, &parsed, error)) {
        return false;
    }

    const QString name = normalizedGeoEastFaultPolygonName(requestedFaultPolygonName);
    QTemporaryFile staged(
        QDir(QFileInfo(outputDatPath).absolutePath()).absoluteFilePath(
            QStringLiteral(".faultpolytrace-geoeast-dat-XXXXXX.tmp")));
    staged.setAutoRemove(true);
    if (!staged.open()) {
        if (error) {
            *error = QString::fromUtf8("无法在目标目录创建 DAT 校验暂存文件：%1\n%2")
                         .arg(QFileInfo(outputDatPath).absolutePath(), staged.errorString());
        }
        return false;
    }

    QTextStream stream(&staged);
    stream.setCodec("UTF-8");
    stream << "#faultPolygonName x y connectionFlag faultLevel faultPointType faultPolygonType closeType\n";
    qint64 pointCount = 0;
    for (int ringIndex = 0; ringIndex < parsed.exteriorRings.size(); ++ringIndex) {
        const int connectionFlag = ringIndex + 1;
        for (const QPointF& point : parsed.exteriorRings[ringIndex]) {
            stream << name << ' '
                   << QString::number(point.x(), 'f', kCoordinatePrecision) << ' '
                   << QString::number(point.y(), 'f', kCoordinatePrecision) << ' '
                   << connectionFlag << ' '
                   << kFaultLevel << ' '
                   << kFaultPointTypeUpperWall << ' '
                   << kFaultPolygonTypeNormal << ' '
                   << kCloseTypeClosed << '\n';
            ++pointCount;
        }
    }
    stream.flush();
    const bool stagedWriteOk = stream.status() == QTextStream::Ok && staged.flush();
    const QString stagedPath = staged.fileName();
    staged.close();
    if (!stagedWriteOk) {
        if (error) {
            *error = QString::fromUtf8("DAT 暂存写入失败：%1\n%2")
                         .arg(stagedPath, staged.errorString());
        }
        return false;
    }

    if (!validateGeoEastFaultPolygonDat(
            stagedPath,
            name,
            parsed.exteriorRings.size(),
            pointCount,
            error)) {
        return false;
    }

    QFile stagedInput(stagedPath);
    QSaveFile output(outputDatPath);
    if (!stagedInput.open(QIODevice::ReadOnly) ||
        !output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = QString::fromUtf8("无法创建最终 DAT：%1\n%2")
                         .arg(QFileInfo(outputDatPath).absoluteFilePath(), output.errorString());
        }
        return false;
    }
    while (!stagedInput.atEnd()) {
        const QByteArray block = stagedInput.read(1024 * 1024);
        if (block.isEmpty() && stagedInput.error() != QFile::NoError) {
            if (error) {
                *error = QString::fromUtf8("读取已校验 DAT 暂存文件失败：%1")
                             .arg(stagedInput.errorString());
            }
            output.cancelWriting();
            return false;
        }
        if (output.write(block) != block.size()) {
            if (error) {
                *error = QString::fromUtf8("最终 DAT 原子写入失败：%1")
                             .arg(output.errorString());
            }
            output.cancelWriting();
            return false;
        }
    }
    if (!output.commit()) {
        if (error) {
            *error = QString::fromUtf8("最终 DAT 原子提交失败：%1")
                         .arg(output.errorString());
        }
        return false;
    }

    if (!validateGeoEastFaultPolygonDat(
            outputDatPath,
            name,
            parsed.exteriorRings.size(),
            pointCount,
            error)) {
        return false;
    }

    QString formatDefinitionPath;
    if (!writeFormatDefinitionCompanion(outputDatPath, &formatDefinitionPath, error)) {
        return false;
    }

    QFile hashFile(outputDatPath);
    QString hash;
    if (hashFile.open(QIODevice::ReadOnly)) {
        hash = QString::fromLatin1(
            QCryptographicHash::hash(hashFile.readAll(), QCryptographicHash::Sha256).toHex());
    }

    if (stats) {
        stats->faultPolygonName = name;
        stats->polygonCount = parsed.exteriorRings.size();
        stats->pointCount = pointCount;
        stats->omittedInteriorRingCount = parsed.omittedInteriorRingCount;
        stats->sha256 = hash;
        stats->formatDefinitionPath = formatDefinitionPath;
    }
    return true;
}

}  // namespace fpt_ui
