#include "FaultTraceDialog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QStringList>

namespace {

QIcon locateApplicationIcon() {
    QDir app(QCoreApplication::applicationDirPath());
    QDir repo(app);
    QString repoIcon;
    for (int i = 0; i < 6; ++i) {
        if (repo.exists("assets/icons/faultpolytrace.ico")) {
            repoIcon = repo.absoluteFilePath("assets/icons/faultpolytrace.ico");
            break;
        }
        if (!repo.cdUp()) {
            break;
        }
    }
    const QStringList candidates = {
        app.absoluteFilePath("faultpolytrace.ico"),
        app.absoluteFilePath("faultpolytrace.png"),
        repoIcon
    };
    for (const QString& path : candidates) {
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            return QIcon(path);
        }
    }
    return QIcon();
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("FaultPolyTrace");
    app.setOrganizationName("BGPCup2026");
    app.setWindowIcon(locateApplicationIcon());

    FaultTraceDialog dialog;
    dialog.show();
    return app.exec();
}
