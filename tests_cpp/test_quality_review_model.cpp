#include "../src/ui/QualityReviewModel.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

QString writeFile(const QTemporaryDir& directory,
                  const QString& name,
                  const QByteArray& content) {
    const QString path = directory.filePath(name);
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "Cannot create temporary diagnostics file.");
    require(file.write(content) == content.size(), "Cannot write temporary diagnostics file.");
    return path;
}

QByteArray validDiagnostics(const QByteArray& evidence) {
    return QByteArray(R"JSON({
  "format": "FaultPolyTrace.RunDiagnostics",
  "schemaVersion": 2,
  "parameters": {"localPolygonQualityShadow": true},
  "grid": {"nx": 100, "ny": 80},
  "results": {"polygonCount": 214, "keptPolygonCount": 214},
  "topology": {"openRings": 0, "selfIntersectingPolygons": 0, "polygonIntersections": 0},
  "timing": {"elapsedMs": 650},
  "localPolygonQuality": {
    "detected": 4,
    "accepted": 1,
    "review": 1,
    "rejected": 2,
    "truncatedEvidence": 0,
    "candidateEvidence": )JSON") + evidence + QByteArray(R"JSON(
  }
})JSON");
}

void testParseFilterAndAggregate(const QTemporaryDir& directory) {
    const QByteArray evidence = R"JSON([
      {
        "objectIndex": 8,
        "polygonId": 172,
        "type": "low_support_bridge",
        "decision": "review",
        "rejectionReason": "insufficient_evidence",
        "window": {"minGridX": 10, "maxGridX": 14, "minGridY": 20, "maxGridY": 23},
        "supportRatio": 0.45,
        "areaLossRatio": 0.02,
        "strongCoverageLossRatio": 0.0,
        "weakCoverageLossRatio": 0.001,
        "backgroundLeakageDelta": 0.0,
        "stabilityJaccard": 0.98
      },
      {
        "objectIndex": 8,
        "polygonId": 172,
        "type": "low_support_bridge",
        "decision": "review",
        "rejectionReason": "insufficient_evidence",
        "window": {"minGridX": 13, "maxGridX": 18, "minGridY": 21, "maxGridY": 26},
        "supportRatio": 0.55,
        "areaLossRatio": 0.03,
        "stabilityJaccard": 0.96
      },
      {
        "objectIndex": 9,
        "polygonId": 67,
        "type": "local_bulge",
        "decision": "accepted",
        "rejectionReason": "none",
        "window": {"minGridX": 30, "maxGridX": 40, "minGridY": 31, "maxGridY": 45},
        "supportRatio": 0.82,
        "stabilityJaccard": 0.99
      },
      {
        "objectIndex": 10,
        "polygonId": 59,
        "type": "natural_end_cap",
        "decision": "rejected",
        "rejectionReason": "no_meaningful_change",
        "window": {"minGridX": 50, "maxGridX": 54, "minGridY": 60, "maxGridY": 64},
        "supportRatio": 0.70,
        "stabilityJaccard": 1.0
      }
    ])JSON";

    fpt_ui::QualityReviewModel model;
    QString error;
    require(model.loadDiagnostics(
                writeFile(directory, "valid.json", validDiagnostics(evidence)), &error),
            "Valid diagnostics must load.");
    require(error.isEmpty(), "Successful load must clear the error message.");
    require(model.summary().shadowMode, "Shadow-mode flag must be parsed.");
    require(model.summary().keptPolygonCount == 214, "Kept-polygon count must be parsed.");
    require(model.items().size() == 3, "Low-support bridge candidates must aggregate per object.");
    const auto& aggregate = model.items().at(0);
    require(aggregate.gridWindow.left() == 10 && aggregate.gridWindow.right() == 18,
            "Aggregated bridge window must cover every candidate for the object.");
    require(aggregate.gridWindow.top() == 20 && aggregate.gridWindow.bottom() == 26,
            "Aggregated bridge window must preserve the full vertical range.");
    require(aggregate.supportRatio == 0.55, "Aggregate must retain the strongest support ratio.");
    require(aggregate.stabilityJaccard == 0.96, "Aggregate must retain the least stable score.");
    require(model.filteredIndexes("review").size() == 1, "Review filter must be exact.");
    require(model.filteredIndexes("accepted").size() == 1, "Accepted filter must be exact.");
    require(model.filteredIndexes("rejected").size() == 1, "Rejected filter must be exact.");
    require(model.filteredIndexes("all").size() == 3, "All filter must retain every item.");
}

void testInvalidAndUnsupportedFiles(const QTemporaryDir& directory) {
    fpt_ui::QualityReviewModel model;
    QString error;
    require(!model.loadDiagnostics(writeFile(directory, "invalid.json", "{broken"), &error),
            "Malformed JSON must be rejected.");
    require(!error.isEmpty(), "Malformed JSON must produce a useful error.");

    const QByteArray unsupported = R"JSON({
      "format": "FaultPolyTrace.RunDiagnostics",
      "schemaVersion": 99
    })JSON";
    require(!model.loadDiagnostics(writeFile(directory, "unsupported.json", unsupported), &error),
            "Unsupported schema versions must be rejected.");

    const QByteArray wrongFormat = R"JSON({
      "format": "Unrelated.Diagnostics",
      "schemaVersion": 2
    })JSON";
    require(!model.loadDiagnostics(writeFile(directory, "wrong-format.json", wrongFormat), &error),
            "Unrelated JSON files must be rejected.");
}

void testBoundsAndEmptyCandidates(const QTemporaryDir& directory) {
    const QByteArray evidence = R"JSON([
      {
        "objectIndex": 1,
        "type": "local_bulge",
        "decision": "review",
        "window": {"minGridX": 98, "maxGridX": 105, "minGridY": 10, "maxGridY": 12}
      },
      {
        "objectIndex": 2,
        "type": "local_bulge",
        "decision": "review",
        "window": {"minGridX": 1, "maxGridX": 4, "minGridY": 2, "maxGridY": 5}
      }
    ])JSON";

    fpt_ui::QualityReviewModel model;
    QString error;
    require(model.loadDiagnostics(
                writeFile(directory, "bounds.json", validDiagnostics(evidence)), &error),
            "A file with one valid candidate must still load.");
    require(model.items().size() == 1, "Out-of-grid candidates must not enter the view model.");
    require(model.items().front().objectIndex == 2, "The valid candidate must be retained.");

    require(model.loadDiagnostics(
                writeFile(directory, "empty.json", validDiagnostics("[]")), &error),
            "An empty quality result is valid.");
    require(model.items().isEmpty(), "Empty evidence must produce an empty model.");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    try {
        QTemporaryDir directory;
        require(directory.isValid(), "Cannot create temporary test directory.");
        testParseFilterAndAggregate(directory);
        testInvalidAndUnsupportedFiles(directory);
        testBoundsAndEmptyCandidates(directory);
        std::cout << "test_quality_review_model: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_quality_review_model: FAIL: " << error.what() << "\n";
        return 1;
    }
}
