from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise AssertionError(f"函数未闭合: {signature}")


def test_shared_parameter_model_is_built_by_both_qt_entries():
    header = read("src/ui/TraceParameterModel.h")
    source = read("src/ui/TraceParameterModel.cpp")
    desktop_pro = read("standalone_app/qt_desktop/FaultPolyTraceDesktop.pro")
    launcher_pro = read("geoeast_plugin/qt_launcher/FaultPolyTraceLauncher.pro")

    assert "enum class QualityMode" in header
    assert "TraceParameters presetParameters" in header
    assert "QStringList traceArguments" in header
    assert "防黏连增强" in source
    for project in (desktop_pro, launcher_pro):
        assert "TraceParameterModel.cpp" in project
        assert "TraceParameterModel.h" in project


def test_both_qt_entries_use_shared_presets_instead_of_private_tables():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    assert "fpt_ui::presetParameters" in desktop
    assert "fpt_ui::presetParameters" in launcher

    desktop_body = function_body(desktop, "void FaultPolyTraceWindow::applyPreset")
    launcher_body = function_body(launcher, "void FaultTraceDialog::applyParameterPreset")
    for body in (desktop_body, launcher_body):
        assert body.count("fpt_ui::presetParameters") == 1
        assert "return;" not in body
        assert "setLocalAntiMergePreset" not in body
        assert 'presetKey == QString::fromUtf8("全覆盖优先")' not in body


def test_shared_serializer_owns_complete_trace_parameter_contract():
    header = read("src/ui/TraceParameterModel.h")
    source = read("src/ui/TraceParameterModel.cpp")

    required_fields = (
        "minGapSupportRatio",
        "separateBranchBelts",
        "rootAntiMergeMaxBridgeValue",
        "residualPercentile",
        "residualFillRadius",
        "residualEdgeFillRadius",
        "residualEdgeFillMinValueRatio",
        "residualEdgeFillMaxPatchSpan",
        "residualMinCells",
        "residualCoveredGuardRadius",
    )
    for field in required_fields:
        assert field in header

    required_switches = (
        "--min-gap-support-ratio",
        "--separate-branch-belts",
        "--root-anti-merge-max-bridge-value",
        "--root-anti-merge-radius",
        "--local-anti-merge-low-threshold",
        "--residual-percentile",
        "--residual-fill-radius",
        "--residual-edge-fill-radius",
        "--residual-edge-fill-min-value-ratio",
        "--residual-edge-fill-max-patch-span",
        "--residual-min-cells",
        "--residual-trace-min-length",
        "--residual-trace-max-cells",
        "--residual-covered-guard-radius",
        "--branch-attribution-shadow",
        "--weak-path-gate",
        "--local-polygon-quality",
    )
    for switch in required_switches:
        assert switch in source


def test_parameter_templates_use_one_versioned_validated_json_contract():
    header = read("src/ui/TraceParameterModel.h")
    model = read("src/ui/TraceParameterModel.cpp")
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    assert "traceParametersToJson" in header
    assert "traceParametersFromJson" in header
    assert 'object["format"] = "FaultPolyTrace.TraceParameters"' in model
    assert 'object["schemaVersion"] = 1' in model
    assert "参数模板版本不受支持" in model
    assert "参数“%1”的类型或取值范围无效" in model
    for source in (desktop, launcher):
        assert "fpt_ui::traceParametersToJson" in source
        assert "fpt_ui::traceParametersFromJson" in source
        assert "QJsonParseError" in source
        assert "QSaveFile" in source


def test_both_qt_entries_expose_productivity_actions_and_shortcuts():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    for source, owner in (
        (desktop, "FaultPolyTraceWindow"),
        (launcher, "FaultTraceDialog"),
    ):
        assert f"void {owner}::saveRunLog()" in source
        assert f"void {owner}::copyResultSummary()" in source
        assert "保存运行日志..." in source
        assert "复制当前结果摘要" in source
        assert 'QKeySequence(Qt::Key_F5)' in source
        assert 'QKeySequence("Shift+F5")' in source
        assert 'QKeySequence("Ctrl+Shift+P")' in source
        assert "QKeySequence(Qt::Key_Home)" in source


def test_geoeast_view_opacity_is_persisted_but_never_written_as_geometry_style():
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")
    runner = read("geoeast_plugin/bo_runner/main.cpp")

    assert "polygonOpacitySpin_" in launcher
    assert 'settings.setValue("polygonOpacity"' in launcher
    assert 'settings.value("polygonOpacity"' in launcher
    assert "color.setAlphaF(polygonOpacity_)" in launcher
    assert "不修改数据库几何" in launcher
    assert '"--polygon-opacity"' not in launcher
    assert '"--polygon-opacity"' not in runner


def test_both_qt_entries_delegate_trace_serialization_to_shared_model():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    desktop_trace = function_body(desktop, "QStringList FaultPolyTraceWindow::traceArgs")
    desktop_geoeast = function_body(desktop, "QStringList FaultPolyTraceWindow::geoEastTraceArgs")
    launcher_trace = function_body(launcher, "QStringList FaultTraceDialog::traceArgs")
    for body in (desktop_trace, desktop_geoeast, launcher_trace):
        assert "fpt_ui::traceArguments" in body
        assert '"--threshold"' not in body


def test_geoeast_runner_accepts_all_quality_mode_switches():
    runner = read("geoeast_plugin/bo_runner/main.cpp")
    for switch in (
        "--branch-attribution-shadow",
        "--no-branch-attribution-shadow",
        "--weak-path-gate",
        "--no-weak-path-gate",
        "--local-polygon-quality",
        "--no-local-polygon-quality",
    ):
        assert switch in runner


def test_database_password_is_never_persisted_in_plaintext():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    assert 'settings.setValue("password"' not in desktop
    assert 'settings.setValue("password"' not in launcher
    assert 'settings.value("password"' not in desktop
    assert 'settings.value("password"' not in launcher


def test_launcher_configuration_uses_per_user_writable_location():
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    assert "QStandardPaths::AppConfigLocation" in launcher
    assert 'applicationDirPath() + "/faultpolytrace_launcher.ini"' not in launcher


def test_process_logs_redact_database_password_values():
    for path in (
        "standalone_app/qt_desktop/FaultPolyTraceWindow.cpp",
        "geoeast_plugin/qt_launcher/FaultTraceDialog.cpp",
    ):
        source = read(path)
        assert 'arg == "--password"' in source
        assert 'QString::fromUtf8("<已隐藏>")' in source


def test_geoeast_loaded_result_style_updates_without_rerunning_trace():
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    assert "void FaultTraceDialog::setPolygonColor" in launcher
    assert "updateStatusContext();" in function_body(
        launcher, "void FaultTraceDialog::setPolygonColor"
    )
    assert "polygonLineWidthSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged)" in launcher
    assert "updateStatusContext();" in launcher


def test_both_qt_entries_block_blank_view_png_export():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    assert "bool hasVisibleContent() const" in desktop
    assert "bool hasVisibleContent() const" in launcher
    for source, signature in (
        (desktop, "void FaultPolyTraceWindow::exportPng"),
        (launcher, "void FaultTraceDialog::exportResultViewPng"),
    ):
        body = function_body(source, signature)
        assert "hasVisibleContent()" in body
        assert "没有可导出的图层" in body
        assert "请先载入并显示原始网格或结果多边形图层" in body
