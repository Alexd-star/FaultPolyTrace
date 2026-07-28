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


def test_product_manual_opens_the_current_bundled_document_without_legacy_version_lookup():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    section = desktop[desktop.index("void FaultPolyTraceWindow::openProductManual()"):desktop.index("void FaultPolyTraceWindow::openProjectStructureDoc()")]

    assert "docs/企业级用户操作手册与产品说明书.md" in section
    assert "V1.0.1" not in section


def test_polished_task_flow_keyboard_accessibility_and_text_presentation_are_preserved():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")
    localization = read("src/ui/UiLocalization.cpp")
    theme = read("src/ui/FptTheme.cpp")

    for source in (desktop, launcher):
        assert "setBuddy" in source
        assert "setAccessibleName" in source
        assert "setCursorPosition(0)" in source
        assert "editingFinished" in source
        assert "停止追踪" in source
        assert "导出 PNG" in source

    assert "QWidget::setTabOrder(sourceModeCombo_, inputEdit_)" in desktop
    assert "QWidget::setTabOrder(gridCombo_, refreshButton_)" in launcher
    assert "keepLineEditReadable(outputEdit_)" in launcher
    assert "configurePathLineEdit(outputNameEdit_)" in desktop

    data_position = launcher.index("sidebarContentLayout->addWidget(dataGroup)")
    result_position = launcher.index("sidebarContentLayout->addWidget(resultGroup)")
    trace_position = launcher.index("sidebarContentLayout->addWidget(traceGroup)")
    assert data_position < result_position < trace_position

    data_section = launcher[launcher.index("QGroupBox* dataGroup"):launcher.index("QGroupBox* resultGroup")]
    assert data_section.index("addWidget(importDatCheck_, 4") < data_section.index("addFormRow(dataLayoutNew, 5")
    assert data_section.index("addFormRow(dataLayoutNew, 5") < data_section.index("addWidget(overwriteGridCheck_, 6")

    assert 'add("导出 PNG", "Export PNG")' in localization
    assert '"fptAccessibleNameZh"' in localization
    assert '"fptAccessibleDescriptionZh"' in localization
    assert "QToolButton:focus" in theme
    assert "QCheckBox:focus" in theme
    assert "QPushButton:focus" in theme


def test_clean_machine_preflight_and_writable_user_storage_are_enforced():
    desktop = read("standalone_app/qt_desktop/FaultPolyTraceWindow.cpp")
    launcher = read("geoeast_plugin/qt_launcher/FaultTraceDialog.cpp")

    for source in (desktop, launcher):
        assert "ensureWritableDirectory" in source
        assert "QTemporaryFile probe" in source
        assert "QStandardPaths::AppLocalDataLocation" in source
        assert "QStandardPaths::TempLocation" in source
        storage_start = source.index("QString appConfigDir()") if "QString appConfigDir()" in source else source.index("QString launcherSettingsPath()")
        storage_end = source.index("\n}\n", storage_start) + 3
        storage_section = source[storage_start:storage_end]
        assert "QCoreApplication::applicationDirPath()" not in storage_section
        assert "outputNameValidationError" in source
        assert "输出名称无效" in source

    assert "输入文件不可读取" in desktop
    assert "输入文件与输出文件冲突" in desktop
    assert "QStorageInfo" in desktop
    assert "覆盖已有结果？" in desktop
    assert "本地缓存目录不可用" in launcher


def test_installer_and_plugin_deployment_cover_first_time_user_constraints():
    installer = read("packaging/windows/FaultPolyTrace.iss")
    demo = read("packaging/windows/02_运行离线演示.cmd")
    smoke = read("scripts/TestWindowsInstaller.ps1")
    plugin_install = read("packaging/submission/03_GeoEast插件/安装GeoEast插件.ps1")
    plugin_cmd = read("packaging/submission/03_GeoEast插件/安装GeoEast插件.cmd")

    assert "MinVersion=10.0" in installer
    assert "DisableWelcomePage=no" in installer
    assert "InfoBeforeFile=" in installer
    assert 'Filename: "{sys}\\notepad.exe"' in installer
    assert "GetFolderPath('MyDocuments')" in demo
    assert 'set "OUTDIR=outputs\\' not in demo
    assert "全新用户 安装目录" in smoke
    assert "中文 空格输出" in smoke
    assert "QT_QPA_PLATFORM = \"offscreen\"" in smoke
    assert "cleanProfileGuiStartup" in smoke
    assert "repairInstall" in smoke
    assert "('/DIR=\"' + $installDir + '\"')" in smoke
    assert "User result was removed during uninstall" in smoke
    assert "Get-Process geoeast" in plugin_install
    assert "GeoEastPluginBackups" in plugin_install
    assert "Get-FileHash -Algorithm SHA256" in plugin_install
    assert "C:\\GeoEast" in plugin_install and "D:\\GeoEast" in plugin_install
    assert "-ExecutionPolicy Bypass" in plugin_cmd
    packaging = read("scripts/打包发布包.ps1")
    assert "txt|cmd|md|json" in packaging
    submission = read("scripts/构建提交代码包.ps1")
    assert 'Filter "official_result.csv"' in submission
    assert "$smokeOfficialResult.DirectoryName" in submission


def test_windows_cli_preserves_unicode_paths_end_to_end():
    main = read("src/app/main.cpp")
    grid_io = read("src/io/AsciiGridIO.cpp")
    diagnostics_io = read("src/io/DiagnosticsIO.cpp")
    build = read("scripts/build.ps1")

    assert "CommandLineToArgvW" in main
    assert "WideCharToMultiByte(CP_UTF8" in main
    assert "SetConsoleOutputCP(CP_UTF8)" in main
    assert "prepareOutputDirectories(cli)" in main
    assert "std::filesystem::create_directories" in main
    assert "commandLineArgumentsUtf8" in main
    assert "MultiByteToWideChar(CP_UTF8" in grid_io
    assert "MultiByteToWideChar(CP_UTF8" in diagnostics_io
    assert "std::filesystem::path(wide)" in grid_io
    assert "std::filesystem::path(wide)" in diagnostics_io
    assert "-lshell32" in build
