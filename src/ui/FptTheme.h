#pragma once

#include <QString>

namespace fpt_ui {

// 独立软件与 GeoEast 插件共用的样式来源：沿用 V1.0.1 的经典灰视觉，
// 两个程序原先重复的四处样式块在此合并，换肤只需修改本文件。
namespace theme {

// ---- 设计令牌（与 V1.0.1 取值一致） ------------------------------------
inline const char* kWindow = "#E8E8E8";       // 窗体背景
inline const char* kPanel = "#F0F0F0";        // 侧栏与分区面板
inline const char* kChrome = "#F0F0F0";       // 菜单栏/工具栏
inline const char* kButton = "#E1E1E1";       // 常规按钮
inline const char* kBorder = "#A0A0A0";       // 控件描边
inline const char* kBorderSoft = "#B5B5B5";   // 分区描边
inline const char* kInk = "#222222";          // 正文
inline const char* kSub = "#555555";          // 次级文字
inline const char* kMuted = "#666666";        // 弱化文字
inline const char* kAccent = "#0078D7";       // 主强调蓝
inline const char* kAccentDeep = "#005A9E";   // 分区标题与品牌深蓝
inline const char* kHover = "#CCE8FF";        // 工具栏悬停
inline const char* kDanger = "#7A1F1F";       // 危险操作文字
inline const char* kDangerHover = "#C94040";
inline const char* kOk = "#1F5C1F";           // 完成提示

}  // namespace theme

// 主窗口/主对话框统一样式表（独立软件 QMainWindow 与插件 QDialog 通用）。
QString appStyle();

// 数据库配置等二级对话框样式表（深蓝头部 + 面板灰正文 + 灰色脚部）。
QString configDialogStyle();

}  // namespace fpt_ui
