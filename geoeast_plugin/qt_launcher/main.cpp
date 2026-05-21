#include "FaultTraceDialog.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("FaultPolyTrace");
    app.setOrganizationName("BGPCup2026");

    FaultTraceDialog dialog;
    dialog.show();
    return app.exec();
}
