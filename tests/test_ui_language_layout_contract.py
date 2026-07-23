from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def test_english_toolbar_uses_compact_product_labels():
    localization = read("src/ui/UiLocalization.cpp")
    assert 'add("打开 DAT 文件", "Open DAT")' in localization
    assert 'add("保存当前配置", "Save Settings")' in localization
    assert 'add("恢复默认参数", "Restore Defaults")' in localization
    assert 'add("打开输出目录", "Open Output")' in localization
    assert 'add("帮助提示", "Help")' in localization


def test_task_guide_wraps_long_english_text_in_a_responsive_box():
    window = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    assert "const qreal guideWidth = qMin(440.0" in window
    assert "Qt::TextWordWrap" in window


def test_application_display_name_tracks_translated_window_title():
    window = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    assert "QGuiApplication::setApplicationDisplayName(translatedTitle)" in window
