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


def test_toolbar_icon_text_has_independent_compact_english_labels():
    localization = read("src/ui/UiLocalization.cpp")
    standalone = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    plugin = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")
    assert '"fptIconTextEn"' in localization
    assert 'setProperty("fptIconTextEn", englishText)' in standalone
    assert 'setProperty("fptIconTextEn", englishText)' in plugin
    assert 'QString::fromUtf8("保存模板"), "Save"' in standalone
    assert 'QString::fromUtf8("刷新网格"), "Grids"' in plugin


def test_dynamic_runtime_text_supports_placeholder_translation():
    localization = read("src/ui/UiLocalization.cpp")
    standalone = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    plugin = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")
    assert "QString translateTemplate" in localization
    assert 'add("已刷新输出目录结果，共 %1 个"' in localization
    assert "uiText(fpt_ui::canonical(line))" in standalone
    assert "uiText(fpt_ui::canonical(line))" in plugin
    assert "uiText(fpt_ui::canonical(text))" in plugin


def test_runtime_configuration_dialogs_apply_current_language():
    localization = read("src/ui/UiLocalization.cpp")
    standalone = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    plugin = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")
    assert "objects.prepend(root);" in localization
    assert '"fptWindowTitleZh"' in localization
    for source in (standalone, plugin):
        assert "fpt_ui::captureTranslatableText(&dialog);" in source
        assert "fpt_ui::applyTranslatableText(&dialog, languageCode());" in source


def test_english_sidebar_buttons_are_not_clipped_by_old_fixed_widths():
    standalone = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    plugin = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")
    assert "showOverlayLayerButton_->setFixedWidth(72)" not in standalone
    assert "browseDatButton->setFixedWidth(52)" not in plugin


def test_spinbox_suffixes_follow_the_selected_language():
    localization = read("src/ui/UiLocalization.cpp")
    assert 'add(" 像元", " cells")' in localization
    assert 'add(" 次", " iterations")' in localization
    assert '"fptSuffixZh"' in localization
