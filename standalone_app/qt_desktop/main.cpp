#include "FaultPolyTraceWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("FaultPolyTrace");
    app.setOrganizationName("BGPCup2026");
    app.setApplicationDisplayName(QString::fromUtf8("FaultPolyTrace"));

    FaultPolyTraceWindow window;
    window.show();
    return app.exec();
}
