#include "FptTheme.h"

namespace fpt_ui {

QString appStyle() {
    return QString::fromUtf8(R"(
        /* ---- 基础层 ---- */
        QMainWindow, QDialog, QWidget {
            background: #F7F8FA; color: #1F2328;
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif; font-size: 12px;
        }
        QMenuBar { background: #F2F3F5; border-bottom: 1px solid #E3E6EA; padding: 2px 4px; }
        QMenuBar::item { padding: 4px 10px; background: transparent; border-radius: 4px; }
        QMenuBar::item:selected { background: #E4EEF9; }
        QMenu { background: #FFFFFF; border: 1px solid #D8DCE1; border-radius: 6px; padding: 4px; }
        QMenu::item { padding: 5px 28px 5px 24px; border-radius: 4px; }
        QMenu::item:selected { background: #E4EEF9; }
        QMenu::item:disabled { color: #9CA3AF; }
        QMenu::separator { height: 1px; background: #E3E6EA; margin: 4px 8px; }
        QToolBar { background: #F2F3F5; border-bottom: 1px solid #E3E6EA; spacing: 4px; padding: 4px 6px; }
        QToolButton { background: transparent; border: 1px solid transparent; border-radius: 4px; padding: 3px 6px; }
        QToolButton:hover { background: #E4EEF9; border-color: #B8D4EE; }
        QToolButton:pressed { background: #C4DFF6; }
        QToolButton:checked { background: #CCE8FF; border-color: #0078D7; }
        QToolButton:disabled { color: #9CA3AF; }

        /* ---- 卡片分区 ---- */
        QGroupBox {
            background: #FFFFFF; border: 1px solid #E3E6EA; border-radius: 6px;
            margin-top: 20px; padding: 10px 8px 8px 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin; left: 2px; top: 0; padding: 0;
            background: transparent; color: #1F2328; font-weight: 600;
        }
        QFrame#sectionBox { background: #FFFFFF; border: 1px solid #E3E6EA; border-radius: 6px; }
        QLabel { background: transparent; }
        QLabel#sectionTitle { color: #1F2328; font-weight: 600; padding: 0 0 4px 0; }
        QLabel#subSectionTitle { color: #6B7280; font-size: 11px; font-weight: 600; }
        QLabel#viewTitle {
            color: #1F2328; font-size: 15px; font-weight: 600;
            border-left: 3px solid #0078D7; padding-left: 8px; background: transparent;
        }
        QLabel#viewMeta { color: #6B7280; font-size: 11px; }
        QLabel#layerHint { color: #6B7280; font-size: 11px; }
        QWidget#layerPanel { background: transparent; border: 0; }
        QCheckBox { spacing: 6px; }
        QCheckBox#layerCheck { background: transparent; min-height: 22px; padding: 0; }
        QCheckBox:disabled { color: #9CA3AF; }

        /* ---- 输入控件 ---- */
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            background: #FFFFFF; border: 1px solid #C9CDD4; border-radius: 4px;
            min-height: 26px; padding: 2px 8px;
            selection-background-color: #0078D7; selection-color: white;
        }
        QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover { border-color: #A8B0BA; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: #0078D7; }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {
            background: #F5F6F8; color: #9CA3AF; border-color: #E0E3E8;
        }
        QComboBox QAbstractItemView {
            background: #FFFFFF; border: 1px solid #D8DCE1; border-radius: 4px;
            selection-background-color: #E4EEF9; selection-color: #1F2328; outline: 0;
        }

        /* ---- 按钮三级体系 ---- */
        QPushButton {
            background: #FFFFFF; border: 1px solid #C9CDD4; border-radius: 4px;
            min-height: 26px; padding: 2px 12px;
        }
        QPushButton:hover { background: #F0F6FC; border-color: #0078D7; }
        QPushButton:pressed { background: #E4EEF9; }
        QPushButton:disabled { background: #F5F6F8; color: #9CA3AF; border-color: #E0E3E8; }
        QPushButton#emphasisButton { background: #F2F8FD; border-color: #5B9BD5; color: #124B73; font-weight: 600; }
        QPushButton#emphasisButton:hover { background: #E5F1FB; border-color: #0078D7; }
        QPushButton#emphasisButton:disabled { background: #F5F6F8; color: #9CA3AF; border-color: #E0E3E8; }
        QPushButton#runButton {
            background: #0078D7; color: white; border: 1px solid #0067BC;
            font-weight: 600; min-height: 32px; border-radius: 4px;
        }
        QPushButton#runButton:hover { background: #1683DE; }
        QPushButton#runButton:pressed { background: #0A5EA8; }
        QPushButton#runButton:disabled { background: #B3CCE0; color: #F0F4F8; border-color: #B3CCE0; }
        QPushButton#stopButton { background: #FFFFFF; border: 1px solid #C9CDD4; color: #B42318; font-weight: 600; }
        QPushButton#stopButton:hover:enabled { background: #FDF1F0; border-color: #D92D20; }
        QPushButton#stopButton:disabled { color: #9CA3AF; background: #F5F6F8; border-color: #E0E3E8; }
        QPushButton#dangerButton { background: #FFFFFF; border: 1px solid #C9CDD4; color: #B42318; }
        QPushButton#dangerButton:hover:enabled { background: #FDF1F0; border-color: #D92D20; color: #B42318; }
        QPushButton#dangerButton:disabled { color: #9CA3AF; background: #F5F6F8; border-color: #E0E3E8; }
        QPushButton#linkButton, QToolButton#linkButton {
            background: transparent; border: 0; color: #0B62A4; padding: 2px 6px; min-height: 22px;
        }
        QPushButton#linkButton:hover, QToolButton#linkButton:hover { background: #EAF3FB; border-radius: 4px; }
        QToolButton#moreButton {
            background: #FFFFFF; border: 1px solid #C9CDD4; border-radius: 4px;
            min-height: 26px; padding: 2px 10px;
        }
        QToolButton#moreButton:hover { background: #F0F6FC; border-color: #0078D7; }
        QToolButton#moreButton::menu-indicator { subcontrol-position: right center; subcontrol-origin: padding; left: -4px; }

        /* ---- 色块按钮 ---- */
        QToolButton#colorSwatch { border: 1px solid #C9CDD4; border-radius: 4px; min-width: 28px; min-height: 24px; }
        QToolButton#colorSwatch:hover { border-color: #0078D7; }
        QLabel#colorHex { color: #6B7280; font-family: Consolas, "Courier New"; }

        /* ---- 空状态引导卡与完成横幅 ---- */
        QFrame#guideCard { background: #FFFFFF; border: 1px solid #E3E6EA; border-radius: 8px; }
        QLabel#guideTitle { font-size: 14px; font-weight: 600; color: #1F2328; }
        QLabel#guideStep { color: #374151; font-size: 12px; }
        QLabel#guideStepDone { color: #1A7F37; font-size: 12px; }
        QFrame#doneBanner { background: #E8F5E9; border: 1px solid #C5E4C9; border-left: 4px solid #1A7F37; border-radius: 6px; }
        QLabel#doneBannerText { color: #14532D; font-weight: 600; background: transparent; }
        QFrame#doneBanner QPushButton#linkButton { color: #0B62A4; background: transparent; }

        /* ---- 日志与状态 ---- */
        QLabel#logHeader {
            background: #F2F3F5; border-top: 1px solid #E3E6EA; border-bottom: 1px solid #E3E6EA;
            padding-left: 10px; color: #57606A; font-size: 11px; font-weight: 600;
        }
        QTextEdit { background: #FAFBFC; border: 0; border-top: 1px solid #E3E6EA;
                    font-family: Consolas, "Courier New"; font-size: 11px; color: #374151; }
        QStatusBar { background: #F2F3F5; border-top: 1px solid #E3E6EA; color: #57606A; }
        QStatusBar QLabel { color: #57606A; }
        QProgressBar { border: 1px solid #C9CDD4; border-radius: 3px; background: #FFFFFF;
                       max-height: 12px; text-align: center; font-size: 10px; }
        QProgressBar::chunk { background: #0078D7; border-radius: 2px; }

        /* ---- 结果摘要 ---- */
        QLabel#resultSummary { color: #1F2328; font-weight: 600; }
        QLabel#resultPath { color: #57606A; background: #FAFBFC; border: 1px solid #E3E6EA;
                            border-radius: 4px; padding: 4px; }
        QLabel#currentShown { color: #1A7F37; font-size: 11px; }

        /* ---- 插件坞区 ---- */
        QFrame#actionDock { background: #F2F3F5; border-top: 1px solid #E3E6EA; }
        QFrame#workspace { background: #FFFFFF; }
        QFrame#logDock { background: #FAFBFC; border-top: 1px solid #E3E6EA; }
        QTextEdit#logEdit { background: #FAFBFC; border: 0; color: #374151;
                            font-family: Consolas, "Courier New", monospace; font-size: 11px; }
        QFrame#statusBar { background: #F2F3F5; border-top: 1px solid #E3E6EA; }
        QLabel#statusText { color: #57606A; font-size: 11px; }
        QFrame#statusSep { background: #D8DCE1; min-width: 1px; max-width: 1px; }

        /* ---- 滚动条与分割线 ---- */
        QSplitter::handle { background: #E3E6EA; width: 5px; }
        QSplitter::handle:hover { background: #6AA9D8; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
        QScrollBar::handle:vertical { background: #C9CDD4; border-radius: 5px; min-height: 30px; }
        QScrollBar::handle:vertical:hover { background: #A8B0BA; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
        QScrollBar::handle:horizontal { background: #C9CDD4; border-radius: 5px; min-width: 30px; }
        QScrollBar::handle:horizontal:hover { background: #A8B0BA; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QScrollArea { border: 0; }
        QToolTip { background: #1F2328; color: #FFFFFF; border: 0; padding: 4px 8px; font-size: 11px; }
    )");
}

QString configDialogStyle() {
    return QString::fromUtf8(R"(
        QDialog { background: #FFFFFF; color: #1F2328;
                  font-family: "Microsoft YaHei", "Segoe UI", sans-serif; font-size: 12px; }
        QLabel { background: transparent; }
        QFrame#dialogHeader { background: #005A9E; }
        QFrame#dialogHeader QLabel { color: #FFFFFF; background: transparent; }
        QLabel#dialogTitle { color: #FFFFFF; font-size: 13px; font-weight: 600; }
        QPushButton#headerHelp {
            background: transparent; border: 1px solid rgba(255,255,255,0.55);
            color: #FFFFFF; border-radius: 4px; padding: 2px 12px; min-height: 22px;
        }
        QPushButton#headerHelp:hover { background: rgba(255,255,255,0.15); }
        QFrame#dialogFooter { background: #FFFFFF; border-top: 1px solid #E5E7EB; }
        QGroupBox { background: #FFFFFF; border: 1px solid #E3E6EA; border-radius: 6px;
                    margin-top: 20px; padding: 10px 8px 8px 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 2px; top: 0;
                           background: transparent; color: #1F2328; font-weight: 600; }
        QLineEdit, QComboBox, QSpinBox {
            background: #FFFFFF; border: 1px solid #C9CDD4; border-radius: 4px;
            min-height: 26px; padding: 2px 8px;
            selection-background-color: #0078D7; selection-color: white;
        }
        QLineEdit:hover, QComboBox:hover, QSpinBox:hover { border-color: #A8B0BA; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: #0078D7; }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled {
            background: #F5F6F8; color: #9CA3AF; border-color: #E0E3E8; }
        QLineEdit#fieldError { border: 1px solid #D93025; }
        QComboBox QAbstractItemView {
            background: #FFFFFF; border: 1px solid #D8DCE1; border-radius: 4px;
            selection-background-color: #E4EEF9; selection-color: #1F2328; outline: 0; }
        QPushButton { background: #FFFFFF; border: 1px solid #C9CDD4; border-radius: 4px;
                      min-height: 26px; padding: 2px 12px; }
        QPushButton:hover { background: #F0F6FC; border-color: #0078D7; }
        QPushButton:pressed { background: #E4EEF9; }
        QPushButton:disabled { background: #F5F6F8; color: #9CA3AF; border-color: #E0E3E8; }
        QPushButton#primaryButton, QPushButton#applyButton {
            background: #0078D7; color: white; border: 1px solid #0067BC; font-weight: 600; }
        QPushButton#primaryButton:hover, QPushButton#applyButton:hover { background: #1683DE; }
        QPushButton#primaryButton:pressed, QPushButton#applyButton:pressed { background: #0A5EA8; }
        QPushButton#primaryButton:disabled, QPushButton#applyButton:disabled {
            background: #B3CCE0; color: #F0F4F8; border-color: #B3CCE0; }
        QLabel#hintLabel { color: #6B7280; font-size: 11px; }
        QLabel#checkStatus { color: #6B7280; padding: 2px 0; }
        QFrame#line { background: #E3E6EA; min-height: 1px; max-height: 1px; }
        QLabel#checkOk { color: #1A7F37; }
        QLabel#checkFail { color: #B42318; }
        QTextEdit { background: #FAFBFC; border: 1px solid #E3E6EA; border-radius: 4px;
                    font-family: Consolas, "Courier New"; font-size: 11px; color: #374151; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
        QScrollBar::handle:vertical { background: #C9CDD4; border-radius: 5px; min-height: 30px; }
        QScrollBar::handle:vertical:hover { background: #A8B0BA; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QToolTip { background: #1F2328; color: #FFFFFF; border: 0; padding: 4px 8px; font-size: 11px; }
    )");
}

}  // namespace fpt_ui
