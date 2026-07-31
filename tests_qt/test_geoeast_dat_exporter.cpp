#include "GeoEastDatExporter.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <iostream>

namespace {

bool writeUtf8(const QString& path, const QString& text) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << text;
    stream.flush();
    return stream.status() == QTextStream::Ok;
}

int exportOne(const QString& csvPath, const QString& datPath, const QString& name) {
    fpt_ui::GeoEastDatExportStats stats;
    QString error;
    if (!fpt_ui::exportGeoEastFaultPolygonDat(csvPath, datPath, name, &stats, &error)) {
        std::cerr << error.toLocal8Bit().constData() << '\n';
        return 1;
    }
    std::cout << "polygons=" << stats.polygonCount
              << " points=" << stats.pointCount
              << " omitted_holes=" << stats.omittedInteriorRingCount
              << " name=" << stats.faultPolygonName.toLatin1().constData()
              << " sha256=" << stats.sha256.toLatin1().constData()
              << " fmt=" << stats.formatDefinitionPath.toLocal8Bit().constData() << '\n';
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() == 4) {
        return exportOne(args[1], args[2], args[3]);
    }

    QTemporaryDir temp;
    if (!temp.isValid()) {
        std::cerr << "temporary directory unavailable\n";
        return 2;
    }
    const QString csvPath = temp.filePath("source.csv");
    const QString datPath = temp.filePath("lc_result.dat");
    const QString csv = QStringLiteral(
        "id,status,area,perimeter,cell_count,mean_value,max_value,wkt\n"
        "1,kept,100,40,4,1,1,\"POLYGON((0 0,10 0,10 10,0 10,0 0),(2 2,3 2,3 3,2 3,2 2))\"\n"
        "2,filtered,25,20,4,0,0,\"POLYGON((20 20,25 20,25 25,20 25,20 20))\"\n");
    if (!writeUtf8(csvPath, csv)) {
        std::cerr << "failed to write fixture\n";
        return 3;
    }

    fpt_ui::GeoEastDatExportStats stats;
    QString error;
    if (!fpt_ui::exportGeoEastFaultPolygonDat(
            csvPath, datPath, QStringLiteral("lc result!*"), &stats, &error)) {
        std::cerr << error.toLocal8Bit().constData() << '\n';
        return 4;
    }
    if (stats.polygonCount != 1 || stats.pointCount != 5 ||
        stats.omittedInteriorRingCount != 1 || stats.faultPolygonName != QStringLiteral("lc_result") ||
        !QFile::exists(stats.formatDefinitionPath)) {
        std::cerr << "unexpected export statistics\n";
        return 5;
    }

    QFile formatFile(stats.formatDefinitionPath);
    if (!formatFile.open(QIODevice::ReadOnly) ||
        !formatFile.readAll().contains("<KEYNAME>faultPolygonName</KEYNAME>")) {
        std::cerr << "missing or invalid GeoEast format companion\n";
        return 10;
    }

    QFile dat(datPath);
    if (!dat.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "failed to reopen DAT\n";
        return 6;
    }
    const QList<QByteArray> lines = dat.readAll().split('\n');
    if (lines.size() < 7 ||
        lines[0] != "#faultPolygonName x y connectionFlag faultLevel faultPointType faultPolygonType closeType" ||
        lines[1] != "lc_result 0.0000 0.0000 1 0 0 1 1" ||
        lines[5] != "lc_result 0.0000 0.0000 1 0 0 1 1") {
        std::cerr << "unexpected DAT content\n";
        return 7;
    }

    const QString invalidCsvPath = temp.filePath("invalid.csv");
    if (!writeUtf8(
            invalidCsvPath,
            QStringLiteral(
                "id,status,area,perimeter,cell_count,mean_value,max_value,wkt\n"
                "1,kept,0,0,3,0,0,\"POLYGON((0 0,1 1,2 2,0 0))\"\n"))) {
        return 8;
    }
    error.clear();
    if (fpt_ui::exportGeoEastFaultPolygonDat(
            invalidCsvPath, temp.filePath("invalid.dat"), QStringLiteral("bad"), nullptr, &error) ||
        error.isEmpty()) {
        std::cerr << "degenerate polygon was not rejected\n";
        return 9;
    }

    std::cout << "GeoEast DAT exporter tests passed\n";
    return 0;
}
