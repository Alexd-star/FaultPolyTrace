#pragma once

#include <QString>
#include <QtGlobal>

namespace fpt_ui {

struct GeoEastDatExportStats {
    QString faultPolygonName;
    int polygonCount = 0;
    qint64 pointCount = 0;
    int omittedInteriorRingCount = 0;
    QString sha256;
    QString formatDefinitionPath;
};

// Keep the dataset name portable and compatible with GeoEast's legacy fields.
QString normalizedGeoEastFaultPolygonName(const QString& requestedName);

// Converts the kept POLYGON rows written by FaultPolyTrace/BO Runner into the
// official full-column FaultPolygon DAT format, writes the matching GeoEast
// .fmt definition beside it, and validates the completed file.
bool exportGeoEastFaultPolygonDat(
    const QString& polygonCsvPath,
    const QString& outputDatPath,
    const QString& requestedFaultPolygonName,
    GeoEastDatExportStats* stats,
    QString* error);

bool validateGeoEastFaultPolygonDat(
    const QString& datPath,
    const QString& expectedFaultPolygonName,
    int expectedPolygonCount,
    qint64 expectedPointCount,
    QString* error);

}  // namespace fpt_ui
