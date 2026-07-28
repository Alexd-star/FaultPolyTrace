#pragma once

#include <QString>

namespace fpt_ui {

// 运行日志的行级呈现规则：独立软件与 GeoEast 插件共用，
// 保证同一条后台输出在两端的着色与折叠判定完全一致。
struct LogLineStyle {
    QString html;        // 已转义并着色的一行 HTML（含结尾换行）
    bool isDebug =false; // 纯英文后台输出，界面默认折叠
};

// 依据行内容判定级别并生成着色 HTML：
// 含错误或失败字样为红色，含完成、成功、就绪为绿色，
// 不含中文且不以 "> " 开头的视为后台调试输出，灰色并标记为可折叠。
LogLineStyle logLineStyle(const QString& line);

void appendBoundedProcessOutput(
    QString& destination,
    const QString& chunk,
    int maximumCharacters = 4 * 1024 * 1024);

QString boundedProcessLogChunk(
    const QString& chunk,
    int maximumCharacters = 512 * 1024);

}  // namespace fpt_ui
