#pragma once

#include <QIcon>

namespace fpt_ui {

// 独立软件与 GeoEast 插件共用的工具栏图标集：同一套彩色语汇，
// 两端相同功能使用相同图标，各自专有功能取用对应条目。
enum class IconKind {
    Open,       // 打开数据文件（黄色文件夹）
    Save,       // 保存配置或模板（蓝色软盘）
    Load,       // 读取模板（蓝色文件夹配上箭头）
    Preset,     // 参数模板（紫色清单配绿勾）
    Pan,        // 平移模式（绿色手掌）
    ZoomIn,     // 放大（蓝色放大镜配加号）
    ZoomOut,    // 缩小（蓝色放大镜配减号）
    Reset,      // 重置视图（蓝色回转箭头）
    Restore,    // 恢复默认参数（橙色回转箭头）
    Run,        // 开始追踪（蓝色三角）
    Stop,       // 停止（红色方块）
    Image,      // 导出图像（绿色图片）
    Folder,     // 打开目录（黄色实心文件夹）
    Database,   // 数据库配置（蓝色圆柱）
    Refresh,    // 刷新列表（青色双箭头）
    Basemap,    // 工区底图（棕色地图）
    Help        // 帮助（红色问号）
};

// 生成 24×24 逻辑尺寸图标，按当前设备像素比渲染，高分屏不发虚。
QIcon toolIcon(IconKind kind);

}  // namespace fpt_ui
