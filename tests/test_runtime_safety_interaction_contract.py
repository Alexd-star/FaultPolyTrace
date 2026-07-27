from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def test_shared_parameter_safety_rejects_zero_and_warns_about_conflicting_enhancements():
    model = read("src/ui/TraceParameterModel.cpp")

    assert "TraceParameterValidation validateTraceParameters" in model
    assert "分位数 0 不是关闭阈值" in model
    assert "最小面积必须大于 0" in model
    assert "p.percentile < 0.80" in model
    assert "p.rootAntiMerge && p.localAntiMerge && p.residualTrace" in model
    assert "根系防黏连、局部桥分裂和弱响应补追不能同时启用" in model
    assert "局部桥分裂会把低属性短桥两侧重新分组" in model
    assert "弱响应补追会额外检查主结果之外" in model


def test_all_trace_entry_points_apply_parameter_safety_before_starting():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")
    core = read("src/core/FaultTracer.cpp")

    for source in (desktop, launcher):
        assert "confirmTraceParametersForRun()" in source
        assert "fpt_ui::validateTraceParameters" in source
        assert "确认高负载参数" in source
    assert "kNoAreaFilterMaxGridCells" in core
    assert '"minArea=0 is disabled for large grids' in core
    assert "kCombinedOptionalEnhancementsMaxGridCells" in core
    assert '"rootAntiMerge, localAntiMerge, and residualTrace cannot all be enabled' in core


def test_advanced_sections_use_disclosure_arrows_not_algorithm_checkboxes():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    for source in (desktop, launcher):
        assert 'setObjectName("advancedSectionToggle")' in source
        assert "setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow)" in source
        assert "此操作不会启用、停用或重新运行算法" in source
        assert "box->setCheckable(true)" not in source


def test_stop_is_a_cancellation_boundary_and_never_continues_task_chain():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    for source in (desktop, launcher):
        assert "cancelRequested_ = true" in source
        assert "process_->kill()" in source
        assert "if (cancelRequested_)" in source
        assert "未执行后续刷新、导出或结果载入" in source
        assert "process_->terminate()" not in source


def test_catalog_and_layer_loading_are_cached_and_do_not_repeat_on_every_click():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")
    controls = read("src/ui/FptControls.cpp")

    assert 'property("fptCatalogReady").toBool()' in desktop
    assert 'property("fptCatalogLoading").toBool()' in desktop
    assert "loadedGeoEastGrid_" in desktop
    assert "loadedGeoEastOutput_" in desktop
    assert "pendingGeoEastResultRestore_" in desktop

    assert 'property("fptCatalogReady").toBool()' in launcher
    assert 'property("fptCatalogLoading").toBool()' in launcher
    assert "loadedGridName_" in launcher
    assert "loadedOutputName_" in launcher
    assert "pendingOutputRestore_" in launcher
    assert "suppressLayerAutoLoad_" in launcher
    assert "const QString completedOutput" in launcher
    assert "const bool tracing = running && !refreshingGrids_" in launcher
    assert "const QString completedOutput" in desktop
    assert "currentTask_ == ProcessTask::ExternalTrace || currentTask_ == ProcessTask::GeoEastTrace" in desktop
    assert "showPolygons && resultCatalogCombo_" in desktop
    assert "(showGrid || showPolygons) && resultCatalogCombo_" not in desktop
    assert "visualKnobPosition" in controls
    assert "isChecked() ? 1.0 : 0.0" in controls


def test_failed_layer_loads_roll_back_visible_toggles_and_catalog_tasks_are_serialized():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    assert "disableGridLayer" in desktop
    assert "disablePolygonLayer" in desktop
    assert "QSignalBlocker gridSignals(geoEastGridCombo_)" in desktop
    assert "layerLoadScheduled" in desktop

    assert "disableInputLayer" in launcher
    assert "disableResultLayer" in launcher
    assert "drawActivityState" in launcher
    assert "if (refreshOutputsAfterGrids_)" in launcher
    assert "else if (!names.isEmpty() && (!showInputLayerCheck_" in launcher
    assert "layerLoadScheduled" in launcher


def test_legacy_dangerous_settings_are_migrated_and_reported():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    for source in (desktop, launcher):
        assert "startupNotices_" in source
        assert "检测到旧版分位数设置" in source
        assert "检测到旧版最小面积设置" in source
        assert 'settings.setValue("configSchemaVersion", 5)' in source


def test_layer_actions_have_user_facing_precondition_guidance():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    for source in (desktop, launcher):
        assert "没有可显示的结果" in source
        assert "任务正在运行" in source
        assert "请等待当前任务完成或先点击【停止】" in source
    assert "显示原始网格前，请先在数据源区域选择" in desktop
    assert "网格列表尚未读取" in launcher
    assert "尚未生成本地视图文件" in launcher


def test_desktop_startup_is_fast_non_modal_centered_and_single_instance():
    main = read("standalone_app/qt_desktop/main.cpp")
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    localization = read("src/ui/UiLocalization.cpp")

    assert "std::make_unique<QSplashScreen>" in main
    assert 'qgetenv("FPT_DISABLE_SPLASH")' in main
    assert "QLockFile guiLock" in main
    assert "splash->finish(&window)" in main
    assert main.index("centerSplash(splash.get())") < main.index("splash->show()")
    assert "Sleep(" not in main
    assert "msleep(" not in main
    assert "QTimer::singleShot(0, this, [this]()" in desktop
    assert "chineseTextByEnglish" in localization
    assert "if (map.contains(text))" in localization


def test_preview_io_is_streamed_and_redundant_repaints_are_skipped():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")
    core_io = read("src/io/AsciiGridIO.cpp")

    desktop_grid = desktop[desktop.index("bool loadGrid("):desktop.index("bool loadPolygons(")]
    desktop_polygons = desktop[desktop.index("bool loadPolygons("):desktop.index("void clearPolygons(")]
    launcher_grid = launcher[launcher.index("bool loadGridFile("):launcher.index("bool loadPolygonCsv(")]
    launcher_polygons = launcher[launcher.index("bool loadPolygonCsv("):launcher.index("bool hasVisibleContent(")]

    assert "QVector<GridSample> samples" in desktop_grid
    assert "file.seek(0)" not in desktop_grid[desktop_grid.index("} else {"):]
    assert "file.readAll()" not in desktop_polygons
    assert "splitByteNumbers(file.readLine())" in launcher_grid
    assert "file.readAll()" not in launcher_grid
    assert "file.readAll()" not in launcher_polygons
    assert "pubsetbuf" in core_io
    assert "cellCount > std::numeric_limits<int>::max()" in desktop_grid
    assert "cellCount > std::numeric_limits<int>::max()" in launcher_grid
    assert "showGridLayer_ == showGrid" in desktop
    assert "showInputLayer_ == showInputLayer" in launcher


def test_preview_pan_zoom_reuses_parsed_samples_and_cached_polygon_paths():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")
    packaging = read("scripts/打包发布包.ps1")

    desktop_draw = desktop[desktop.index("void drawPolygons("):desktop.index("void drawTaskPreview(")]
    launcher_draw = launcher[launcher.index("void drawPolygons("):launcher.index("QString uiText(", launcher.index("void drawPolygons("))]

    assert "parseGeoEastPreviewSample" in desktop
    assert "splitNumbers(file.readLine())" not in desktop[desktop.index("} else {"):desktop.index("min_ = std::numeric_limits<double>::max()")]
    for source, draw in ((desktop, desktop_draw), (launcher, launcher_draw)):
        assert "QPainterPath polygonPath_" in source
        assert "polygonPath_.addPolygon" in source
        assert "painter.drawPath(polygonPath_)" in draw
        assert "pen.setCosmetic(true)" in draw
        assert "QPolygonF mapped" not in draw

    assert '"official_root_antimerge_benchmark.csv"' in packaging
    assert 'Copy-Item -LiteralPath (Join-Path $repoRoot "examples\\outputs\\official")' not in packaging


def test_customer_builds_ship_only_the_required_simplified_chinese_qt_translation():
    for script_path in ("scripts/build_desktop_app.ps1", "scripts/build_qt_launcher.ps1"):
        script = read(script_path)
        assert "function Install-RequiredQtTranslations" in script
        assert 'translations\\qt_zh_CN.qm' in script
        assert 'Remove-Item -LiteralPath $destination -Recurse -Force' in script
        assert "Install-RequiredQtTranslations" in script[script.index("if (Test-Path -LiteralPath $windeployQt)"):]
