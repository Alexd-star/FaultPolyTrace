#include "FaultPolyTraceWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString locateCoreRunner() {
    QDir app(QCoreApplication::applicationDirPath());
    const QString self = QFileInfo(QCoreApplication::applicationFilePath()).absoluteFilePath();
    QDir parent(app);
    QString parentBin;
    if (parent.cdUp()) {
        parentBin = parent.absoluteFilePath("bin/faultpolytrace.exe");
    }
    QDir repo(app);
    QString repoBuild;
    QString repoBin;
    for (int i = 0; i < 6; ++i) {
        if (repo.exists("src") && repo.exists("examples")) {
            repoBuild = repo.absoluteFilePath("build/faultpolytrace.exe");
            repoBin = repo.absoluteFilePath("bin/faultpolytrace.exe");
            break;
        }
        if (!repo.cdUp()) {
            break;
        }
    }
    const QStringList candidates = {
        app.absoluteFilePath("bin/faultpolytrace.exe"),
        app.absoluteFilePath("faultpolytrace.exe"),
        parentBin,
        repoBuild,
        repoBin
    };
    for (const QString& path : candidates) {
        const QFileInfo info(path);
        if (info.exists() && info.absoluteFilePath().compare(self, Qt::CaseInsensitive) != 0) {
            return info.absoluteFilePath();
        }
    }
    return QString();
}

bool hasCliArguments(const QStringList& args) {
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i).startsWith("--")) {
            return true;
        }
    }
    return false;
}

int forwardToCoreRunner(const QStringList& args) {
#ifdef Q_OS_WIN
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$", "r", stdin);
    }
#endif
    const QString runner = locateCoreRunner();
    if (runner.isEmpty()) {
        std::fprintf(stderr, "Cannot find core runner: bin/faultpolytrace.exe\n");
        return 2;
    }

    QStringList coreArgs = args;
    coreArgs.removeFirst();
    QProcess process;
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start(runner, coreArgs);
    if (!process.waitForStarted()) {
        std::fprintf(stderr, "Cannot start core runner: %s\n", qPrintable(runner));
        return 2;
    }
    process.waitForFinished(-1);
    return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : 1;
}

}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("FaultPolyTrace");
    app.setOrganizationName("BGPCup2026");
    app.setApplicationDisplayName(QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪"));

    if (hasCliArguments(app.arguments())) {
        return forwardToCoreRunner(app.arguments());
    }

    FaultPolyTraceWindow window;
    window.show();
    return app.exec();
}
