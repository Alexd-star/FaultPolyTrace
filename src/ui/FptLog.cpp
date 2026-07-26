#include "FptLog.h"

#include <QRegularExpression>

namespace fpt_ui {

namespace {

// 正则只编译一次，进程刷屏时不会反复构造。
const QRegularExpression& hanPattern() {
    static const QRegularExpression pattern(QStringLiteral("[\\x{4e00}-\\x{9fff}]"));
    return pattern;
}

}  // namespace

LogLineStyle logLineStyle(const QString& line) {
    LogLineStyle style;
    style.isDebug = !line.startsWith(QLatin1Char('>')) && !line.contains(hanPattern());

    QString color = QStringLiteral("#333333");
    if (line.contains(QString::fromUtf8("错误")) || line.contains(QString::fromUtf8("失败")) ||
        line.contains(QLatin1String("error"), Qt::CaseInsensitive) ||
        line.contains(QLatin1String("failed"), Qt::CaseInsensitive)) {
        color = QStringLiteral("#A3211B");
    } else if (line.contains(QString::fromUtf8("完成")) || line.contains(QString::fromUtf8("成功")) ||
               line.contains(QString::fromUtf8("就绪"))) {
        color = QStringLiteral("#1F7A1F");
    } else if (style.isDebug) {
        color = QStringLiteral("#8A8A8A");
    }

    style.html = QStringLiteral("<span style=\"color:%1;\">%2</span><br>")
                     .arg(color, line.toHtmlEscaped());
    return style;
}

}  // namespace fpt_ui
