#include "FaultPolyTraceWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLockFile>
#include <QPainter>
#include <QProcess>
#include <QProgressBar>
#include <QScreen>
#include <QSplashScreen>
#include <QStandardPaths>
#include <QStringList>
#include <QTranslator>

#include <cstdio>
#include <cwchar>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QPixmap createStartupPixmap(const QIcon& icon) {
    QPixmap pixmap(500, 230);
    pixmap.fill(QColor("#F7F9FC"));

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(QRect(0, 0, pixmap.width(), 68), QColor("#0F4D92"));
    painter.fillRect(QRect(0, 68, pixmap.width(), 1), QColor("#D7E0EA"));

    const QPixmap appIcon = icon.pixmap(44, 44);
    if (!appIcon.isNull()) {
        painter.drawPixmap(26, 12, appIcon);
    }

    QFont titleFont = painter.font();
    titleFont.setPointSize(17);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(Qt::white);
    painter.drawText(QRect(84, 10, 386, 30), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("FaultPolyTrace"));

    QFont headerDetailFont = painter.font();
    headerDetailFont.setPointSize(9);
    headerDetailFont.setBold(false);
    painter.setFont(headerDetailFont);
    painter.setPen(QColor("#DCEBFA"));
    painter.drawText(QRect(84, 39, 386, 20), Qt::AlignLeft | Qt::AlignVCenter,
                     QString::fromUtf8("断层多边形自动追踪"));

    QFont bodyTitleFont = painter.font();
    bodyTitleFont.setPointSize(12);
    bodyTitleFont.setBold(true);
    painter.setFont(bodyTitleFont);
    painter.setPen(QColor("#202A35"));
    painter.drawText(QRect(30, 92, 440, 26), Qt::AlignLeft | Qt::AlignVCenter,
                     QString::fromUtf8("正在准备工作界面"));

    QFont detailFont = painter.font();
    detailFont.setPointSize(9);
    detailFont.setBold(false);
    painter.setFont(detailFont);
    painter.setPen(QColor("#5F6B78"));
    painter.drawText(QRect(30, 121, 440, 42), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                     QString::fromUtf8("正在加载显示组件并恢复本机设置，完成后将自动进入主界面。"));
    painter.setPen(QColor("#8A95A2"));
    painter.drawText(QRect(30, 197, 440, 18), Qt::AlignLeft | Qt::AlignVCenter,
                     QString::fromUtf8("无需重复点击，请稍候。"));
    return pixmap;
}

bool splashEnabled() {
    const QByteArray value = qgetenv("FPT_DISABLE_SPLASH").trimmed().toLower();
    return value != "1" && value != "true" && value != "yes";
}

void centerSplash(QSplashScreen* splash) {
    if (!splash) {
        return;
    }
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen) {
        splash->move(screen->availableGeometry().center() - splash->rect().center());
    }
}

#ifdef Q_OS_WIN
BOOL CALLBACK activateFaultPolyTraceWindow(HWND handle, LPARAM) {
    if (!IsWindowVisible(handle)) {
        return TRUE;
    }
    wchar_t title[256] = {};
    if (GetWindowTextW(handle, title, 256) <= 0 || wcsstr(title, L"FaultPolyTrace") == nullptr) {
        return TRUE;
    }
    if (IsIconic(handle)) {
        ShowWindow(handle, SW_RESTORE);
    }
    SetForegroundWindow(handle);
    return FALSE;
}
#endif

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
    app.setApplicationDisplayName(QString::fromUtf8("FaultPolyTrace 断层多边形自动追踪"));
    const QIcon applicationIcon = locateApplicationIcon();
    app.setWindowIcon(applicationIcon);

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

    if (hasCliArguments(app.arguments())) {
        return forwardToCoreRunner(app.arguments());
    }

    const QString lockRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QLockFile guiLock(QDir(lockRoot).absoluteFilePath(QStringLiteral("FaultPolyTrace.gui.lock")));
    guiLock.setStaleLockTime(10000);
    if (!guiLock.tryLock(0)) {
#ifdef Q_OS_WIN
        EnumWindows(activateFaultPolyTraceWindow, 0);
#endif
        return 0;
    }

    std::unique_ptr<QSplashScreen> splash;
    if (splashEnabled()) {
        splash = std::make_unique<QSplashScreen>(createStartupPixmap(applicationIcon));
        splash->setWindowFlag(Qt::WindowStaysOnTopHint, true);
        splash->setWindowFlag(Qt::WindowDoesNotAcceptFocus, true);
        QProgressBar* progress = new QProgressBar(splash.get());
        progress->setGeometry(30, 174, 440, 6);
        progress->setRange(0, 0);
        progress->setTextVisible(false);
        progress->setStyleSheet(QStringLiteral(
            "QProgressBar{border:0;background:#D9E1EA;}"
            "QProgressBar::chunk{background:#0F4D92;width:48px;}"));
        centerSplash(splash.get());
        splash->show();
        app.processEvents(QEventLoop::AllEvents, 25);
    }

    FaultPolyTraceWindow window;
    window.show();
    app.processEvents(QEventLoop::AllEvents, 25);
    if (splash) {
        splash->finish(&window);
    }
    return app.exec();
}
