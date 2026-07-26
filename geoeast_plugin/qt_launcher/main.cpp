#include "FaultTraceDialog.h"

#include <QApplication>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QTranslator>
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    QApplication app(argc, argv);
    app.setApplicationName("FaultPolyTrace");
    app.setOrganizationName("BGPCup2026");
    app.setWindowIcon(locateApplicationIcon());

    // 加载 Qt 中文翻译，使系统对话框（颜色/文件选择）随界面显示中文。
    QTranslator qtTranslator;
    const QStringList translationDirs = {
        QCoreApplication::applicationDirPath() + "/translations",
        QLibraryInfo::location(QLibraryInfo::TranslationsPath),
    };
    for (const QString& dir : translationDirs) {
        if (qtTranslator.load(QStringLiteral("qt_zh_CN"), dir)) {
            app.installTranslator(&qtTranslator);
            break;
        }
    }

    FaultTraceDialog dialog;
    dialog.show();
    return app.exec();
}
