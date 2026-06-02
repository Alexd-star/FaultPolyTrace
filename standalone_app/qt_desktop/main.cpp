#include "FaultPolyTraceWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("FaultPolyTrace");
    app.setOrganizationName("BGPCup2026");
    app.setApplicationDisplayName(QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪"));

    FaultPolyTraceWindow window;
    window.show();
    return app.exec();
}
