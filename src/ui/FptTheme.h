#pragma once

#include <QString>

namespace fpt_ui {

// V1.1 统一视觉主题：独立软件与 GeoEast 插件共用一份样式来源，
// 换肤只需修改本文件。颜色体系为分层中性色 + Fluent 蓝强调。
namespace theme {

// ---- 设计令牌 -----------------------------------------------------------
inline const char* kCanvas = "#F7F8FA";      // 窗体/画布背景
inline const char* kChrome = "#F2F3F5";      // 菜单栏/工具栏/日志头
inline const char* kCard = "#FFFFFF";        // 参数卡片与输入区
inline const char* kBorder = "#C9CDD4";      // 控件描边
inline const char* kBorderSoft = "#E3E6EA";  // 卡片/分隔描边
inline const char* kInk = "#1F2328";         // 正文
inline const char* kSub = "#6B7280";         // 次级文字
inline const char* kMuted = "#9CA3AF";       // 弱化/禁用文字
inline const char* kAccent = "#0078D7";      // 主强调蓝
inline const char* kAccentDark = "#0A5EA8";  // 按压蓝
inline const char* kAccentDeep = "#005A9E";  // 品牌深蓝（对话框头部）
inline const char* kDanger = "#B42318";      // 危险红
inline const char* kDangerHover = "#D92D20";
inline const char* kOk = "#1A7F37";          // 成功绿

}  // namespace theme

// 主窗口/主对话框统一样式表（独立软件 QMainWindow 与插件 QDialog 通用）。
QString appStyle();

// 数据库配置等二级对话框样式表（深蓝头部 + 白色正文 + 白色脚部）。
QString configDialogStyle();

}  // namespace fpt_ui
