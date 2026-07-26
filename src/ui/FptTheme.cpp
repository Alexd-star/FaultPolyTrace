#include "FptTheme.h"

namespace fpt_ui {

// V1.1 保持 V1.0.1 的经典灰视觉：配色、控件尺寸、圆角与边框全部沿用原样式，
// 仅把两个程序重复的样式块收拢到本文件，并为新增控件补上同一语汇的规则。
QString appStyle() {
    return QString::fromUtf8(R"(
        /* ---- 基础层 ---- */
        QMainWindow, QDialog, QWidget {
            background: #E8E8E8; color: #222222;
            font-family: "Microsoft YaHei", "Segoe UI", Tahoma, sans-serif; font-size: 12px;
        }
        QMenuBar { background: #F0F0F0; border-bottom: 1px solid #D0D0D0; padding: 2px 8px; }
        QMenuBar::item { padding: 4px 8px; background: transparent; border-radius: 2px; }
        QMenuBar::item:selected { background: #CCE8FF; border: 1px solid #99D1FF; }
        QMenu { background: #F8F8F8; border: 1px solid #A8A8A8; }
        QMenu::item { padding: 5px 28px 5px 24px; }
        QMenu::item:selected { background: #DDECF8; }
        QMenu::item:disabled { color: #999999; }
        QToolBar {
            background: #F0F0F0; border: 0; border-bottom: 1px solid #A0A0A0;
            spacing: 4px; padding: 4px 10px;
        }
        QToolButton {
            min-width: 26px; min-height: 24px; background: transparent;
            border: 1px solid transparent; border-radius: 3px; padding: 2px 6px;
        }
        QToolButton:hover { background: #CCE8FF; border-color: #99D1FF; }
        QToolButton:pressed { background: #B3DCFF; border-color: #7FBFF2; }
        QToolButton:checked { background: #CCE8FF; border-color: #6AA9D8; }
        QToolButton:disabled { color: #999999; }

        /* ---- 分区与侧栏 ---- */
        QGroupBox {
            background: #F0F0F0; border: 1px solid #B5B5B5; border-radius: 3px;
            margin-top: 14px; padding-top: 10px; font-weight: normal;
        }
        QGroupBox::title {
            subcontrol-origin: margin; left: 8px; padding: 0 5px;
            background: #F0F0F0; color: #005A9E; font-weight: bold;
        }
        QFrame#sectionBox { background: #F0F0F0; border: 1px solid #B5B5B5; border-radius: 3px; }
        QFrame#sidebar { background: #F0F0F0; border-right: 1px solid #999999; }
        QFrame#sidebarHeader { background: #EBECEE; border-bottom: 1px solid #A0A0A0; }
        QLabel#sidebarTitle { font-size: 12px; font-weight: bold; color: #111111; }
        QScrollArea#sidebarScroll { border: 0; background: #F0F0F0; }
        QWidget#sidebarContent { background: #F0F0F0; }
        QLabel { background: transparent; }
        QLabel#sectionTitle { color: #005A9E; font-weight: bold; padding: 0 0 4px 0; }
        QLabel#subSectionTitle { color: #005A9E; font-weight: bold; }
        QLabel#formLabel { color: #222222; }
        QLabel#layerPanelTitle { color: #005A9E; font-weight: bold; }
        QLabel#layerHint { color: #666666; font-size: 11px; }
        QLabel#displayHint {
            color: #555555; background: #F7F7F7; border: 1px solid #D0D0D0;
            border-radius: 2px; padding: 5px 6px; line-height: 140%;
        }
        QWidget#layerPanel { background: transparent; border: 0; }
        QCheckBox#layerCheck { background: transparent; min-height: 22px; padding: 2px 0; }
        QCheckBox:disabled { color: #999999; }

        /* ---- 输入控件 ---- */
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            background: #FFFFFF; border: 1px solid #A0A0A0; border-radius: 2px;
            min-height: 22px; padding: 1px 5px; selection-background-color: #0078D7;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: #0078D7; }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {
            background: #F0F0F0; color: #888888; border-color: #C4C4C4;
        }

        /* ---- 按钮 ---- */
        QPushButton {
            background: #E1E1E1; border: 1px solid #A0A0A0; border-radius: 2px;
            min-height: 22px; padding: 2px 10px; color: #222222;
        }
        QPushButton:hover { background: #E5F1FB; border-color: #0078D7; }
        QPushButton:pressed { background: #CCE4F7; }
        QPushButton:disabled { background: #D8D8D8; color: #777777; border-color: #B8B8B8; }
        QPushButton#emphasisButton {
            background: #F2F8FD; border-color: #5B9BD5; color: #124B73; font-weight: bold;
        }
        QPushButton#emphasisButton:hover { background: #E5F1FB; border-color: #0078D7; }
        QPushButton#emphasisButton:disabled { background: #D8D8D8; color: #777777; border-color: #B8B8B8; }
        QPushButton#dangerButton { color: #7A1F1F; }
        QPushButton#dangerButton:hover:enabled { background: #FDECEC; border-color: #C94040; }
        QPushButton#dangerButton:disabled { color: #777777; background: #D8D8D8; border-color: #B8B8B8; }
        QPushButton#runButton {
            background: #0078D7; border: 1px solid #005A9E; color: white;
            font-weight: bold; min-height: 28px;
        }
        QPushButton#runButton:hover { background: #106EBE; }
        QPushButton#runButton:pressed { background: #0A5EA8; }
        QPushButton#stopButton { background: #E1E1E1; color: #333333; font-weight: bold; min-height: 28px; }
        QPushButton#stopButton:hover:enabled { background: #FDECEC; border-color: #C94040; }
        QPushButton#stopButton:disabled, QPushButton#runButton:disabled {
            background: #D8D8D8; color: #777777; border-color: #B8B8B8;
        }
        QPushButton#linkButton, QToolButton#linkButton {
            background: transparent; border: 0; color: #005A9E; padding: 2px 6px; min-height: 22px;
        }
        QPushButton#linkButton:hover, QToolButton#linkButton:hover { background: #DDECF8; border-radius: 2px; }
        QToolButton#moreButton {
            background: #E1E1E1; border: 1px solid #A0A0A0; border-radius: 2px;
            min-height: 22px; padding: 2px 10px; color: #222222;
        }
        QToolButton#moreButton:hover { background: #E5F1FB; border-color: #0078D7; }
        QToolButton#moreButton:disabled { background: #D8D8D8; color: #777777; border-color: #B8B8B8; }

        /* ---- 引导卡与完成横幅（沿用经典灰语汇） ---- */
        QFrame#guideCard { background: #FFFFFF; border: 1px solid #C8C8C8; border-radius: 3px; }
        QLabel#guideTitle { color: #005A9E; font-weight: bold; }
        QLabel#guideStep { color: #444444; }
        QFrame#doneBanner { background: #EDF6ED; border: 1px solid #A9C8A9; border-radius: 2px; }
        QLabel#doneBannerText { color: #1F5C1F; font-weight: bold; background: transparent; }
        QFrame#doneBanner QPushButton#linkButton { color: #005A9E; background: transparent; }

        /* ---- 日志与状态 ---- */
        QLabel#logHeader {
            background: #EAEAEA; border-top: 1px solid #A8A8A8; border-bottom: 1px solid #C0C0C0;
            color: #444444; font-weight: bold; padding: 4px 10px;
        }
        QTextEdit {
            background: #F8F9FA; border: 0; border-top: 1px solid #B8B8B8;
            color: #333333; font-family: Consolas, "Courier New", monospace; font-size: 12px;
        }
        QTextEdit#logEdit {
            background: #F8F9FA; border: 0; color: #333333;
            font-family: Consolas, "Courier New", monospace; font-size: 12px;
        }
        QStatusBar { background: #F0F0F0; border-top: 1px solid #A0A0A0; color: #555555; }
        QStatusBar QLabel { color: #555555; }
        QFrame#statusBar { background: #F0F0F0; border-top: 1px solid #A0A0A0; }
        QLabel#statusText { color: #555555; font-size: 12px; }
        QFrame#statusSep { background: #C0C0C0; min-width: 1px; max-width: 1px; }
        QProgressBar {
            border: 1px solid #A0A0A0; border-radius: 2px; background: #FFFFFF;
            max-height: 12px; text-align: center; font-size: 10px;
        }
        QProgressBar::chunk { background: #0078D7; }

        /* ---- 坞区与结果摘要 ---- */
        QFrame#actionDock { background: #E8E8E8; border-top: 1px solid #A0A0A0; }
        QFrame#workspace { background: #FFFFFF; }
        QFrame#logDock { background: #F8F9FA; border-top: 1px solid #A0A0A0; }
        QLabel#resultSummary { color: #333333; font-weight: bold; }
        QLabel#resultPath { color: #555555; background: #F7F7F7; border: 1px solid #CCCCCC; padding: 4px; }
        QLabel#currentShown { color: #1F5C1F; font-size: 11px; }

        /* ---- 分割线与滚动条 ---- */
        QSplitter::handle { background: #C6C6C6; width: 5px; }
        QSplitter::handle:hover { background: #6AA9D8; }
        QToolTip { background: #FFFFE1; color: #222222; border: 1px solid #A0A0A0; padding: 3px 6px; }
    )");
}

QString configDialogStyle() {
    return QString::fromUtf8(R"(
        QDialog {
            background: #F0F0F0; color: #222222;
            font-family: "Microsoft YaHei", "Segoe UI", Tahoma, sans-serif; font-size: 12px;
        }
        QLabel { background: transparent; }
        QFrame#dialogHeader { background: #005A9E; }
        QFrame#dialogHeader QLabel { color: white; background: transparent; }
        QLabel#dialogTitle { color: white; font-weight: bold; padding-left: 16px; }
        QLabel#dialogHeader { background: #005A9E; color: white; font-weight: bold; padding-left: 16px; }
        QFrame#dialogFooter { background: #EAEAEA; border-top: 1px solid #C0C0C0; }
        QLabel#checkStatus { color: #555555; padding: 2px 0; }
        QLabel#hintLabel { color: #666666; font-size: 11px; }
        QLabel#checkOk { color: #1F7A1F; }
        QLabel#checkFail { color: #A3211B; }
        QFrame#line { background: #D0D0D0; min-height: 1px; max-height: 1px; }
        QGroupBox {
            background: #F0F0F0; border: 1px solid #B5B5B5; border-radius: 3px;
            margin-top: 14px; padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin; left: 8px; padding: 0 5px;
            background: #F0F0F0; color: #005A9E; font-weight: bold;
        }
        QLineEdit, QComboBox, QSpinBox {
            background: #FFFFFF; border: 1px solid #A0A0A0; border-radius: 2px;
            min-height: 22px; padding: 1px 5px; selection-background-color: #0078D7;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: #0078D7; }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled {
            background: #F0F0F0; color: #888888; border-color: #C4C4C4;
        }
        QLineEdit#fieldError { border: 1px solid #D93025; }
        QPushButton {
            background: #E1E1E1; border: 1px solid #A0A0A0; border-radius: 2px;
            min-height: 24px; padding: 2px 12px; color: #222222;
        }
        QPushButton:hover { background: #E5F1FB; border-color: #0078D7; }
        QPushButton:pressed { background: #CCE4F7; }
        QPushButton:disabled { background: #D8D8D8; color: #777777; border-color: #B8B8B8; }
        QPushButton#primaryButton, QPushButton#applyButton {
            background: #0078D7; border-color: #005A9E; color: white; font-weight: bold;
        }
        QPushButton#primaryButton:hover, QPushButton#applyButton:hover { background: #106EBE; }
        QPushButton#primaryButton:disabled, QPushButton#applyButton:disabled {
            background: #D8D8D8; color: #777777; border-color: #B8B8B8;
        }
        QTextEdit {
            background: #F8F9FA; border: 1px solid #C0C0C0;
            font-family: Consolas, "Courier New", monospace; color: #333333;
        }
        QToolTip { background: #FFFFE1; color: #222222; border: 1px solid #A0A0A0; padding: 3px 6px; }
    )");
}

}  // namespace fpt_ui
