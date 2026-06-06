#include "UiLocalization.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QPushButton>
#include <QWidget>

namespace {

QMap<QString, QString> buildEnglishTextMap() {
    QMap<QString, QString> map;
    auto add = [&](const char* zh, const char* en) {
        map.insert(QString::fromUtf8(zh), QString::fromUtf8(en));
    };

    add("FaultPolyTrace 断层多边形自动追踪", "FaultPolyTrace Fault Polygon Auto Tracing");
    add("GeoEast(G)", "GeoEast(G)");
    add("文件(F)", "File(F)");
    add("文件(&F)", "File(&F)");
    add("编辑(E)", "Edit(E)");
    add("编辑(&E)", "Edit(&E)");
    add("视图(V)", "View(V)");
    add("视图(&V)", "View(&V)");
    add("断层(L)", "Fault(L)");
    add("断层(&L)", "Fault(&L)");
    add("层位(H)", "Horizon(H)");
    add("层位(&H)", "Horizon(&H)");
    add("工具(T)", "Tools(T)");
    add("工具(&T)", "Tools(&T)");
    add("帮助(H)", "Help(H)");
    add("帮助(&H)", "Help(&H)");
    add("语言(L)", "Language(L)");
    add("语言(&I)", "Language(&I)");
    add("简体中文", "Simplified Chinese");
    add("English", "English");

    add("数据库配置", "Database Configuration");
    add("刷新网格", "Refresh Grids");
    add("刷新网格列表", "Refresh Grid List");
    add("刷新结果列表", "Refresh Result List");
    add("显示所选结果", "Display Selected Result");
    add("删除当前结果", "Delete Current Result");
    add("打开工区底图", "Open Basemap");
    add("打开平面成图", "Open GeoMapping");
    add("关闭启动器", "Close Launcher");
    add("打开 DAT 文件...", "Open DAT File...");
    add("打开 DAT 文件", "Open DAT File");
    add("打开DAT", "Open DAT");
    add("保存当前配置", "Save Current Settings");
    add("保存配置", "Save Settings");
    add("打开输出目录", "Open Output Folder");
    add("复制日志", "Copy Log");
    add("清空日志", "Clear Log");
    add("平移模式", "Pan Mode");
    add("平移", "Pan");
    add("放大", "Zoom In");
    add("缩小", "Zoom Out");
    add("重置视图", "Reset View");
    add("重置", "Reset");
    add("显示原始网格层", "Show Input Grid Layer");
    add("显示结果多边形层", "Show Result Polygon Layer");
    add("重新载入结果视图", "Reload Result View");
    add("导出当前视图 PNG", "Export Current View PNG");
    add("开始追踪", "Start Trace");
    add("停止追踪", "Stop Trace");
    add("停止", "Stop");
    add("选择结果颜色...", "Choose Result Color...");
    add("打开叠合查看指南", "Open Overlay Viewing Guide");
    add("保存当前参数", "Save Current Parameters");
    add("恢复默认配置", "Restore Default Settings");
    add("恢复默认参数", "Restore Default Parameters");
    add("恢复默认", "Restore Defaults");
    add("打开用户操作手册", "Open User Manual");
    add("打开异机运行说明", "Open Deployment Guide");
    add("帮助提示", "Help Tips");
    add("用户操作手册", "User Manual");
    add("产品说明书", "Product Manual");
    add("项目目录结构", "Project Structure");
    add("打开完整中文用户操作手册", "Open the full Chinese user manual");
    add("打开企业级用户操作手册与产品说明书", "Open the enterprise user manual and product manual");
    add("打开项目目录结构与文件作用说明", "Open project directory structure and file purpose documentation");
    add("关于 FaultPolyTrace", "About FaultPolyTrace");
    add("工具栏", "Toolbar");

    add("数据管理", "Data Management");
    add("数据与输出", "Data and Output");
    add("数据源", "Data Source");
    add("外部文件", "External File");
    add("GeoEast数据库", "GeoEast Database");
    add("输入网格", "Input Grid");
    add("输入文件", "Input File");
    add("输入格式", "Input Format");
    add("输出目录", "Output Folder");
    add("输出名称", "Output Name");
    add("输出多边形", "Output Polygon");
    add("视图内容", "View Content");
    add("任务预览", "Task Preview");
    add("原始网格", "Input Grid");
    add("结果多边形", "Result Polygons");
    add("叠加结果", "Overlay Result");
    add("图层已关闭", "Layers Hidden");
    add("[视图区] %1", "[View] %1");
    add("缩放 %1%", "Zoom %1%");
    add("请选择输入网格，或载入历史结果。", "Select an input grid or load a historical result.");
    add("已载入原始网格，尚未运行追踪；当前不是处理后的叠加结果。", "The input grid is loaded, but tracing has not run yet. This is not a processed overlay result.");
    add("[任务视图区] 结果查看", "[Task View] Result Review");
    add("查看逻辑：\n"
        "1. 原始网格显示输入属性色图。\n"
        "2. 结果多边形显示 CSV/WKT 结果文件。\n"
        "3. 叠加结果用于核对断层响应贴合度。\n"
        "4. 左侧复选框可独立开关图层。",
        "Viewing logic:\n"
        "1. Input Grid shows the attribute color map.\n"
        "2. Result Polygons shows the CSV/WKT result file.\n"
        "3. Overlay Result checks how well polygons follow fault responses.\n"
        "4. Use the left checkboxes to show or hide layers independently.");
    add("查看模式  缩放 %1%", "View Mode  Zoom %1%");
    add("显示样式", "Display Style");
    add("颜色", "Color");
    add("线宽", "Line Width");
    add("透明度", "Opacity");
    add("结果样式 / 线宽", "Result Style / Width");
    add("DAT 文件", "DAT File");
    add("覆盖同名 GeoEast 网格", "Overwrite Same-name GeoEast Grid");
    add("从 DAT 导入/覆盖输入网格", "Import / Overwrite Input Grid from DAT");
    add("结果查看", "Result View");
    add("重新载入", "Reload");
    add("刷新结果", "Refresh Results");
    add("选为当前", "Set as Current");
    add("浏览", "Browse");
    add("查看叠加", "View Overlay");
    add("打开CSV", "Open CSV");
    add("GeoJSON", "GeoJSON");
    add("诊断JSON", "Diagnostics JSON");
    add("导出PNG", "Export PNG");
    add("查看步骤", "View Steps");
    add("载入历史", "Load History");
    add("历史记录", "History");
    add("任务", "Task");

    add("追踪参数", "Trace Parameters");
    add("预设模板", "Preset");
    add("追踪模式", "Trace Mode");
    add("阈值模式", "Threshold Mode");
    add("分位数", "Percentile");
    add("固定阈值", "Fixed Threshold");
    add("最小面积", "Min Area");
    add("平滑次数", "Smoothing");
    add("形态与拓扑约束", "Shape and Topology");
    add("闭运算半径", "Closing Radius");
    add("闭合半径", "Closing Radius");
    add("最大断距", "Max Gap");
    add("最大夹角", "Max Angle");
    add("防黏连支撑", "Anti-merge Support");
    add("简化容差", "Simplify Tolerance");
    add("属性约束断层带", "Attribute-constrained Belt");
    add("属性包络半径", "Attribute Envelope Radius");
    add("根系防黏连", "Root Anti-merge");
    add("带宽半径", "Belt Radius");
    add("带后闭合", "Post-belt Closing");
    add("枝干细节与修补", "Branch Details and Repair");
    add("弱拓扑分位", "Weak Topology Percentile");
    add("弱响应分位", "Weak Response Percentile");
    add("弱延伸半径", "Weak Extension Radius");
    add("最短分支", "Min Branch");
    add("交切短枝", "Junction Short Branch");
    add("交切均值", "Junction Mean");
    add("短枝均值", "Short Branch Mean");
    add("补追最短", "Residual Min Length");
    add("补追上限", "Residual Max Cells");
    add("残余二次追踪", "Residual Secondary Trace");
    add("启用残余二次追踪", "Enable Residual Secondary Trace");
    add("专家防黏连参数", "Expert Anti-merge Parameters");
    add("防黏连参数", "Anti-merge Parameters");
    add("启用根系防黏连", "Enable Root Anti-merge");
    add("根系半径", "Root Radius");
    add("根系主体", "Root Main Cells");
    add("支撑比例", "Support Ratio");
    add("主体点数", "Main Cells");
    add("主体比例", "Main Part Ratio");
    add("弱保护阈值", "Weak Guard Threshold");
    add("弱保护半径", "Weak Guard Radius");
    add("启用局部桥防黏连", "Enable Local Bridge Anti-merge");
    add("局部桥防黏连", "Local Bridge Anti-merge");
    add("低值阈值", "Low-value Threshold");
    add("桥半径", "Bridge Radius");
    add("局部主体", "Local Main Cells");
    add("局部比例", "Local Ratio");
    add("桥像元", "Bridge Cells");
    add("桥点下限", "Bridge Cell Minimum");
    add("低值占比", "Low-value Ratio");
    add("低值比例", "Low-value Ratio");

    add("▶ 开始追踪", "▶ Start Trace");
    add("■ 停止", "■ Stop");
    add("运行日志与输出信息", "Run Log and Output");
    add("视图 X: --   |   Y: --", "View X: --   |   Y: --");
    add("数据库连接待验证", "Database Connection Pending");
    add("就绪", "Ready");
    add("界面语言已切换", "Interface language changed");
    add("耗时: --", "Elapsed: --");
    add("耗时: 计算中", "Elapsed: Running");
    add("耗时: %1", "Elapsed: %1");
    add("自动", "Auto");
    add("暂无历史记录", "No History");
    add("尚未运行追踪任务", "No trace task has run yet");
    add("结果文件将在运行完成后显示在这里。", "Result files will be shown here after the run completes.");
    add("正在计算，结果文件生成后会自动显示。", "Running. Result files will be shown automatically when generated.");
    add("最近结果已生成，可在中间视图区查看或打开结果文件。", "The latest result is ready. Review it in the center view or open the result files.");
    add("运行结束，但未找到结果 CSV。请检查日志和输出目录。", "The run finished, but no result CSV was found. Check the log and output folder.");
    add("尚未生成结果。运行完成后将自动切换到叠加结果。", "No result yet. The view will switch to overlay after the run completes.");
    add("请选择输入文件或 GeoEast 网格，然后点击【开始追踪】。", "Select an input file or GeoEast grid, then click [Start Trace].");
    add("CSV：%1", "CSV: %1");
    add("预计输出：%1", "Expected Output: %1");

    add("未配置", "Not Configured");
    add("未选择", "Not Selected");
    add("请选择平面断层属性文件", "Select a planar fault attribute file");
    add("未命名", "Unnamed");
    add("默认输出目录", "Default Output Folder");
    add("等待运行", "Waiting");
    add("任务运行中", "Task Running");
    add("数据库未配置", "Database Not Configured");
    add("正在操作项目: %1", "Project: %1");
    add("当前网格: %1", "Current Grid: %1");
    add("数据库连接待验证: %1", "Database Pending: %1");
    add("[结果视图区] 原始网格 + 结果多边形", "[Result View] Input Grid + Result Polygons");
    add("[结果视图区] 原始网格", "[Result View] Input Grid");
    add("[结果视图区] 结果多边形", "[Result View] Result Polygons");
    add("[结果视图区] 图层显示已关闭", "[Result View] Layers Hidden");
    add("固定阈值 %1", "Fixed Threshold %1");
    add("分位数 %1", "Percentile %1");
    add("数据库：%1    项目：%2", "Database: %1    Project: %2");
    add("输入网格：%1", "Input Grid: %1");
    add("输入文件：%1", "Input File: %1");
    add("输出多边形：%1", "Output Polygon: %1");
    add("输出名称：%1", "Output Name: %1");
    add("输出目录：%1", "Output Folder: %1");
    add("方案：%1    模式：%2    阈值：%3", "Preset: %1    Mode: %2    Threshold: %3");
    add("面积过滤：小于 %1 的多边形不输出    结果线宽：%2", "Area Filter: polygons below %1 are excluded    Line Width: %2");
    add("弱响应：分位 %1 / 半径 %2    根系防黏连：%3    残余补追：%4", "Weak Response: percentile %1 / radius %2    Root Anti-merge: %3    Residual Trace: %4");
    add("当前状态：%1", "Status: %1");
    add("结果查看：追踪完成后自动显示原始网格和结果多边形；如需复查，可在左侧重新载入结果视图。",
        "Result Review: the input grid and result polygons are shown automatically after tracing. Use Reload in the sidebar to review them again.");
    add("官方查看：点击【打开工区底图】或【打开平面成图】，在 GeoEast 图层树中加载 Grids 和 Fault Polygons。",
        "Official Review: open Basemap or GeoMapping, then load Grids and Fault Polygons in the GeoEast layer tree.");
    add("属性", "Attribute");
    add("视图 X: --   |   Y: --", "View X: --   |   Y: --");
    add("视图 X: %1   |   Y: %2", "View X: %1   |   Y: %2");
    add("[系统就绪] 等待参数输入及任务执行...", "[System Ready] Waiting for parameters and task execution...");
    add("[系统就绪] 等待参数输入及任务执行。", "[System Ready] Waiting for parameters and task execution.");
    add("未检测到当前项目名，请先在【数据库配置】中填写项目，再刷新网格列表。",
        "No current project was detected. Enter the project in [Database Configuration], then refresh the grid list.");
    add("已选择输入文件，尚未运行追踪", "Input file selected. Tracing has not run yet.");
    add("已载入原始网格，尚未运行追踪", "Input grid loaded. Tracing has not run yet.");
    add("原始网格已载入，尚未运行追踪", "Input grid loaded. Tracing has not run yet.");
    add("正在计算，等待结果生成", "Running. Waiting for results.");
    add("已恢复推荐算法与显示参数，输入文件和输出位置保持不变", "Recommended algorithm and display parameters restored. Input file and output location were kept unchanged.");
    add("已恢复推荐算法与显示参数", "Recommended algorithm and display parameters restored.");
    add("尚未生成结果", "No Result Yet");
    add("请先选择输入文件并点击【开始追踪】。运行完成后才能查看处理后的叠加结果。", "Select an input file and click [Start Trace] first. The processed overlay can be viewed after the run completes.");
    add("结果文件不存在", "Result File Not Found");
    add("请先完成一次追踪，或从历史记录载入已有结果。", "Run tracing once or load an existing result from history first.");
    add("GeoJSON 不存在", "GeoJSON Not Found");
    add("请先完成一次追踪。GeoJSON 会在结果 CSV 生成后自动写出。", "Run tracing first. GeoJSON is written automatically after the result CSV is generated.");
    add("诊断 JSON 不存在", "Diagnostics JSON Not Found");
    add("请先完成一次追踪。诊断 JSON 会随结果文件一起生成。", "Run tracing first. Diagnostics JSON is generated with the result files.");
    add("当前状态：等待参数输入及任务执行", "Status: waiting for parameters and task execution");
    add("运行方式：GeoEast 数据库模式    数据库：%1    项目：%2", "Mode: GeoEast Database    Database: %1    Project: %2");
    add("运行方式：独立文件模式    输入格式：%1", "Mode: External File    Input Format: %1");
    add("启用", "Enabled");
    add("关闭", "Disabled");
    add("%1  缩放 %2%", "%1  Zoom %2%");
    add("查看模式", "View Mode");
    add("原始网格：%1 x %2", "Input Grid: %1 x %2");
    add("结果多边形：%1 个，显示环：%2 条", "Result Polygons: %1, Rings: %2");
    add("结果多边形：%1 个", "Result Polygons: %1");
    add("未发现保留多边形", "No Kept Polygons Found");

    add("配置 GeoEast 根目录、数据根目录、数据库、项目、用户和执行程序", "Configure GeoEast root, data root, database, project, user, and execution program");
    add("刷新 GeoEast 当前项目下的输入网格列表", "Refresh input grids in the current GeoEast project");
    add("打开或选择 DAT 文件", "Open or select a DAT file");
    add("保存当前界面配置和参数", "Save current UI settings and parameters");
    add("选择结果多边形颜色", "Choose result polygon color");
    add("从 GeoEast 数据库读取可用输入网格列表", "Read available input grids from the GeoEast database");
    add("控制结果视图区是否显示本次追踪生成的原始网格图层。", "Show or hide the input grid layer generated for this trace.");
    add("控制结果视图区是否显示本次追踪生成的结果多边形图层。", "Show or hide the result polygon layer generated for this trace.");
    add("重新读取本次计算生成的原始网格和结果多边形文件。", "Reload the generated input grid and result polygon files.");
    add("导出当前结果视图 PNG 图片。", "Export the current result view as a PNG image.");
    add("启动 GeoEast 自带 GeoBasemap，在官方图层树中查看原始网格和结果多边形。", "Open GeoEast GeoBasemap to inspect the input grid and result polygons in the official layer tree.");
    add("启动 GeoEast 自带 GeoMapping，在官方图层树中查看原始网格和结果多边形。", "Open GeoEast GeoMapping to inspect the input grid and result polygons in the official layer tree.");
    add("打开 GeoEast 平面成图叠加查看操作指南", "Open the GeoEast overlay viewing guide");
    add("删除当前输出名称对应的结果多边形对象", "Delete the result polygon object with the current output name");
    add("提示：本窗口显示本次计算结果；官方复核请在工区底图或平面成图中勾选网格和断层多边形图层。", "Tip: this window shows the current computation result. For official review, select the grid and fault polygon layers in Basemap or GeoMapping.");

    add("全覆盖优先", "Coverage First");
    add("贴合融合推荐", "Fit and Merge Recommended");
    add("防黏连增强", "Anti-merge Enhanced");
    add("均衡推荐", "Balanced Recommended");
    add("保守高精度", "Conservative High Precision");
    add("弱响应高召回", "Weak-response High Recall");
    add("宽断层带", "Wide Fault Belt");
    add("细碎断裂", "Fragmented Faults");
    add("点击刷新网格或输入 GeoEast 网格名", "Click refresh or enter a GeoEast grid name");
    add("请先配置项目并刷新网格", "Configure the project and refresh grids first");
    add("请先配置项目并刷新结果", "Configure the project and refresh results first");
    add("请先运行追踪或刷新已有结果", "Run tracing or refresh existing results first");
    add("当前项目暂无结果；点击此处重新刷新", "No result in the current project; click to refresh");
    add("输出目录暂无结果；点击此处查看引导", "No result in the output folder; click for guidance");
    add("选择输入网格", "Select Input Grid");
    add("选择已有结果", "Select Existing Result");
    add("正在读取数据库，请稍候", "Reading the database. Please wait.");
    add("任务运行中，请稍候", "A task is running. Please wait.");
    add("输入网格列表为空，正在自动刷新...", "The input grid list is empty. Refreshing automatically...");
    add("已有结果列表为空，正在自动刷新...", "The result list is empty. Refreshing automatically...");
    add("暂无可查看结果", "No Result Available");
    add("当前输出目录中没有可查看的结果 CSV。\n\n"
        "请先选择输入数据并完成一次追踪，或检查“输出目录”后点击【刷新结果】。",
        "There is no result CSV in the current output folder.\n\n"
        "Select input data and complete a trace, or check the Output Folder and click [Refresh Results].");
    add("当前输出目录暂无结果，请先运行追踪或检查输出目录", "No result in the current output folder. Run tracing or check the output folder.");
    add("刷新当前项目中的 BOFaultPolygon 结果列表。", "Refresh BOFaultPolygon results in the current project.");
    add("把选中的已有结果多边形对象设为当前输出名称。", "Use the selected result polygon object as the current output name.");
    add("可选：选择需要导入或覆盖到 GeoEast 的 DAT 文件", "Optional: choose a DAT file to import or overwrite in GeoEast");
    add("外部断层属性网格文件路径", "External fault attribute grid file path");
    add("结果文件输出目录", "Result file output folder");
    add("选择外部断层属性网格文件", "Select an external fault attribute grid file");
    add("选择平面断层属性网格", "Select Fault Attribute Grid");
    add("选择输出目录", "Select Output Folder");
    add("选择目录", "Select Folder");
    add("选择可执行文件", "Select Executable File");
    add("网格文件 (*.txt *.dat *.csv);;所有文件 (*.*)", "Grid Files (*.txt *.dat *.csv);;All Files (*.*)");
    add("程序 (*.exe);;所有文件 (*.*)", "Programs (*.exe);;All Files (*.*)");
    add("PNG 图像 (*.png);;所有文件 (*.*)", "PNG Images (*.png);;All Files (*.*)");
    add("参数模板 (*.json);;所有文件 (*.*)", "Parameter Templates (*.json);;All Files (*.*)");
    add("FaultPolyTrace 帮助", "FaultPolyTrace Help");
    add("FaultPolyTrace 独立软件支持两种数据源。\n\n"
        "外部文件流程：\n"
        "1. 数据源选择【外部文件】。\n"
        "2. 选择 DAT、矩阵文本或 GeoEast DAT 文件。\n"
        "3. 设置输出目录、输出名称和追踪参数。\n"
        "4. 点击【开始追踪】，完成后自动切换到【叠加结果】。\n\n"
        "GeoEast 数据库流程：\n"
        "1. 数据源选择【GeoEast数据库】。\n"
        "2. 点击【数据库配置】确认 GeoEast 根目录、数据根目录、数据库、项目和用户。\n"
        "3. 点击【刷新网格】读取 GeoEast 输入网格列表。\n"
        "4. 设置输出多边形名称后点击【开始追踪】。\n"
        "5. 结果写入 GeoEast 数据库，同时在本软件生成结果视图文件用于查看。\n\n"
        "在左侧【结果查看】中打开 CSV、GeoJSON、诊断 JSON 或输出目录，也可导出当前 PNG。",
        "FaultPolyTrace standalone supports two data sources.\n\n"
        "External file workflow:\n"
        "1. Select [External File] as the data source.\n"
        "2. Choose a DAT, matrix text, or GeoEast DAT file.\n"
        "3. Set the output folder, output name, and trace parameters.\n"
        "4. Click [Start Trace]. The view switches to [Overlay Result] after completion.\n\n"
        "GeoEast database workflow:\n"
        "1. Select [GeoEast Database] as the data source.\n"
        "2. Click [Database Configuration] to confirm GeoEast root, data root, database, project, and user.\n"
        "3. Click [Refresh Grids] to read the GeoEast input grid list.\n"
        "4. Set the output polygon name and click [Start Trace].\n"
        "5. Results are written to the GeoEast database, and review files are generated in this application.\n\n"
        "Use [Result View] on the left to open CSV, GeoJSON, diagnostics JSON, or the output folder. You can also export the current PNG.");
    add("FaultPolyTrace 独立软件支持两种数据源。\n\n"
        "外部文件流程：\n"
        "1. 数据源选择【外部文件】。\n"
        "2. 选择 DAT、矩阵文本或 GeoEast DAT 文件。\n"
        "3. 设置输出目录、输出名称和追踪参数。\n"
        "4. 点击【开始追踪】，完成后自动切换到【叠加结果】。\n\n"
        "GeoEast 数据库流程：\n"
        "1. 数据源选择【GeoEast数据库】。\n"
        "2. 点击【数据库配置】确认 GeoEast 根目录、数据根目录、数据库、项目和用户。\n"
        "3. 点击【刷新网格】读取 GeoEast 输入网格列表。\n"
        "4. 设置输出多边形名称后点击【开始追踪】。\n"
        "5. 结果写入 GeoEast 数据库，同时在本软件生成结果视图文件用于查看。\n\n"
        "在左侧【结果查看】中打开 CSV、GeoJSON、诊断 JSON 或输出目录，也可导出当前 PNG。\n\n"
        "完整操作说明请打开【帮助】>【用户操作手册】。",
        "FaultPolyTrace standalone supports two data sources.\n\n"
        "External file workflow:\n"
        "1. Select [External File] as the data source.\n"
        "2. Choose a DAT, matrix text, or GeoEast DAT file.\n"
        "3. Set the output folder, output name, and trace parameters.\n"
        "4. Click [Start Trace]. The view switches to [Overlay Result] after completion.\n\n"
        "GeoEast database workflow:\n"
        "1. Select [GeoEast Database] as the data source.\n"
        "2. Click [Database Configuration] to confirm GeoEast root, data root, database, project, and user.\n"
        "3. Click [Refresh Grids] to read the GeoEast input grid list.\n"
        "4. Set the output polygon name and click [Start Trace].\n"
        "5. Results are written to the GeoEast database, and review files are generated in this application.\n\n"
        "Use [Result View] on the left to open CSV, GeoJSON, diagnostics JSON, or the output folder. You can also export the current PNG.\n\n"
        "For complete instructions, open [Help] > [User Manual].");
    add("文档未找到", "Document Not Found");
    add("未找到【%1】。\n\n请确认当前发布包包含 docs 目录，或重新运行发布包/GeoEast 部署脚本。",
        "%1 was not found.\n\nConfirm that the current release package contains the docs directory, or rerun the release package / GeoEast deployment script.");
    add("文档未找到：%1", "Document not found: %1");
    add("文档未找到，请检查发布包 docs 目录", "Document not found. Check the release package docs directory.");
    add("无法打开文档", "Cannot Open Document");
    add("系统未能打开【%1】。\n\n文件位置：%2\n\n请确认系统已关联 Markdown、PDF 或 Word 文档查看程序。",
        "The system could not open %1.\n\nFile location: %2\n\nConfirm that Markdown, PDF, or Word document viewers are associated on this system.");
    add("无法打开文档：%1", "Cannot open document: %1");
    add("无法打开文档，请检查系统文件关联", "Cannot open document. Check system file associations.");
    add("已打开文档：%1", "Opened document: %1");
    add("视图区已放大", "View zoomed in");
    add("视图区已缩小", "View zoomed out");
    add("视图区已重置", "View reset");
    add("GeoEast 数据库配置已保存", "GeoEast database configuration saved");
    add("GeoEast 配置不完整，已停止操作", "GeoEast configuration is incomplete. Operation stopped.");
    add("GeoEast 模式下请先运行追踪或刷新网格；运行完成后会生成结果视图文件", "In GeoEast mode, run tracing or refresh grids first. Result view files are generated after completion.");
    add("请选择输入文件", "Select an input file");
    add("原始网格已载入", "Input grid loaded");
    add("原始网格载入失败", "Failed to load input grid");
    add("GeoEast 网格刷新完成，共 %1 个", "GeoEast grid refresh completed: %1 grid(s)");
    add("GeoEast 网格刷新失败，请查看日志", "GeoEast grid refresh failed. Check the log.");
    add("运行结束但未找到结果 CSV", "Run finished but no result CSV was found");
    add("GeoEast 追踪完成，结果已写入数据库并显示结果视图", "GeoEast tracing completed. Results were written to the database and displayed.");
    add("追踪完成，已切换到叠加结果", "Tracing completed. Switched to overlay result.");
    add("运行失败，请查看日志", "Run failed. Check the log.");
    add("进程启动或运行异常", "Process startup or runtime error");
    add("正在查看叠加结果", "Viewing overlay result");
    add("算法程序不存在", "Algorithm executable not found");
    add("输入文件无效", "Invalid input file");
    add("请选择可读取的外部网格文件。", "Select a readable external grid file.");
    add("结果文件不存在", "Result file does not exist");
    add("GeoJSON 不存在", "GeoJSON does not exist");
    add("诊断 JSON 不存在", "Diagnostics JSON does not exist");
    add("导出当前视图 PNG", "Export Current View PNG");
    add("保存参数模板", "Save Parameter Template");
    add("打开参数模板", "Open Parameter Template");
    add("PNG 已导出：%1", "PNG exported: %1");
    add("按当前参数运行断层多边形自动追踪", "Run fault polygon tracing with current parameters");
    add("终止当前正在运行的追踪任务", "Terminate the running trace task");
    add("导出当前视图区图像", "Export the current view image");
    add("保存当前算法参数模板", "Save the current algorithm parameter template");
    add("读取已保存的参数模板", "Load a saved parameter template");
    add("恢复推荐默认参数", "Restore recommended default parameters");
    add("打开当前结果输出目录", "Open the current result output folder");
    add("查看独立软件使用说明", "View standalone software usage tips");
    add("从配置的 GeoEast 数据库刷新当前项目下的输入网格列表", "Refresh input grids in the current project from the configured GeoEast database");
    add("配置 GeoEast 根目录、数据根目录、数据库、项目和用户信息", "Configure GeoEast root, data root, database, project, and user information");
    add("删除当前输出名对应的 GeoEast 结果多边形对象", "Delete the GeoEast result polygon object matching the current output name");

    return map;
}

const QMap<QString, QString>& englishTextMap() {
    static const QMap<QString, QString> map = buildEnglishTextMap();
    return map;
}

bool isEnglish(const QString& languageCode) {
    const QString code = fpt_ui::normalizeLanguageCode(languageCode);
    return code == "en";
}

void rememberText(QObject* object, const char* propertyName, const QString& value) {
    if (!object || value.isEmpty()) {
        return;
    }
    if (!object->property(propertyName).isValid()) {
        object->setProperty(propertyName, fpt_ui::canonical(value));
    }
}

} // namespace

namespace fpt_ui {

QString normalizeLanguageCode(const QString& code) {
    const QString value = code.trimmed();
    if (value.compare("en", Qt::CaseInsensitive) == 0 ||
        value.compare("en-US", Qt::CaseInsensitive) == 0 ||
        value.compare("en_US", Qt::CaseInsensitive) == 0 ||
        value.compare("English", Qt::CaseInsensitive) == 0) {
        return "en";
    }
    return "zh-CN";
}

QString translate(const QString& zh, const QString& languageCode) {
    const QString key = canonical(zh);
    if (!isEnglish(languageCode)) {
        return key;
    }
    const auto it = englishTextMap().constFind(key);
    return it == englishTextMap().constEnd() ? key : it.value();
}

QString canonical(const QString& text) {
    const QMap<QString, QString>& map = englishTextMap();
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        if (it.value() == text) {
            return it.key();
        }
    }
    return text;
}

void captureTranslatableText(QObject* root) {
    if (!root) {
        return;
    }
    const QList<QObject*> objects = root->findChildren<QObject*>();
    for (QObject* object : objects) {
        if (QAction* action = qobject_cast<QAction*>(object)) {
            rememberText(action, "fptTextZh", action->text());
            rememberText(action, "fptToolTipZh", action->toolTip());
        }
        if (QMenu* menu = qobject_cast<QMenu*>(object)) {
            rememberText(menu, "fptTitleZh", menu->title());
        }
        if (QLabel* label = qobject_cast<QLabel*>(object)) {
            rememberText(label, "fptTextZh", label->text());
        } else if (QPushButton* button = qobject_cast<QPushButton*>(object)) {
            rememberText(button, "fptTextZh", button->text());
        } else if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(object)) {
            rememberText(checkBox, "fptTextZh", checkBox->text());
        } else if (QGroupBox* groupBox = qobject_cast<QGroupBox*>(object)) {
            rememberText(groupBox, "fptTitleZh", groupBox->title());
        }
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(object)) {
            rememberText(lineEdit, "fptPlaceholderZh", lineEdit->placeholderText());
        }
        if (QWidget* widget = qobject_cast<QWidget*>(object)) {
            rememberText(widget, "fptToolTipZh", widget->toolTip());
        }
    }
}

void applyTranslatableText(QObject* root, const QString& languageCode) {
    if (!root) {
        return;
    }
    const QList<QObject*> objects = root->findChildren<QObject*>();
    for (QObject* object : objects) {
        auto translated = [&](QObject* target, const char* propertyName) {
            return translate(target->property(propertyName).toString(), languageCode);
        };
        if (QAction* action = qobject_cast<QAction*>(object)) {
            if (action->property("fptTextZh").isValid()) {
                action->setText(translated(action, "fptTextZh"));
            }
            if (action->property("fptToolTipZh").isValid()) {
                action->setToolTip(translated(action, "fptToolTipZh"));
            }
        }
        if (QMenu* menu = qobject_cast<QMenu*>(object)) {
            if (menu->property("fptTitleZh").isValid()) {
                menu->setTitle(translated(menu, "fptTitleZh"));
            }
        }
        if (QLabel* label = qobject_cast<QLabel*>(object)) {
            if (label->property("fptTextZh").isValid()) {
                label->setText(translated(label, "fptTextZh"));
            }
        } else if (QPushButton* button = qobject_cast<QPushButton*>(object)) {
            if (button->property("fptTextZh").isValid()) {
                button->setText(translated(button, "fptTextZh"));
            }
        } else if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(object)) {
            if (checkBox->property("fptTextZh").isValid()) {
                checkBox->setText(translated(checkBox, "fptTextZh"));
            }
        } else if (QGroupBox* groupBox = qobject_cast<QGroupBox*>(object)) {
            if (groupBox->property("fptTitleZh").isValid()) {
                groupBox->setTitle(translated(groupBox, "fptTitleZh"));
            }
        }
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(object)) {
            if (lineEdit->property("fptPlaceholderZh").isValid()) {
                lineEdit->setPlaceholderText(translated(lineEdit, "fptPlaceholderZh"));
            }
        }
        if (QWidget* widget = qobject_cast<QWidget*>(object)) {
            if (widget->property("fptToolTipZh").isValid()) {
                widget->setToolTip(translated(widget, "fptToolTipZh"));
            }
        }
    }
}

void setComboItems(QComboBox* combo, const QStringList& keys, const QString& languageCode) {
    if (!combo) {
        return;
    }
    const QString currentKey = comboCurrentKey(combo);
    combo->blockSignals(true);
    combo->clear();
    for (const QString& key : keys) {
        combo->addItem(translate(key, languageCode), key);
    }
    combo->blockSignals(false);
    setComboByKey(combo, currentKey.isEmpty() && !keys.isEmpty() ? keys.first() : currentKey);
}

void refreshComboItems(QComboBox* combo, const QString& languageCode) {
    if (!combo) {
        return;
    }
    const QString currentKey = comboCurrentKey(combo);
    combo->blockSignals(true);
    for (int i = 0; i < combo->count(); ++i) {
        const QString key = combo->itemData(i).toString().isEmpty()
            ? canonical(combo->itemText(i))
            : combo->itemData(i).toString();
        combo->setItemData(i, key);
        combo->setItemText(i, translate(key, languageCode));
    }
    combo->blockSignals(false);
    setComboByKey(combo, currentKey);
}

QString comboCurrentKey(const QComboBox* combo) {
    if (!combo) {
        return QString();
    }
    const QString key = combo->currentData().toString();
    return key.isEmpty() ? canonical(combo->currentText()) : key;
}

void setComboByKey(QComboBox* combo, const QString& key) {
    if (!combo || key.isEmpty()) {
        return;
    }
    const QString normalized = canonical(key);
    for (int i = 0; i < combo->count(); ++i) {
        const QString itemKey = combo->itemData(i).toString().isEmpty()
            ? canonical(combo->itemText(i))
            : combo->itemData(i).toString();
        if (itemKey == normalized || combo->itemText(i) == normalized) {
            combo->setCurrentIndex(i);
            return;
        }
    }
    combo->setCurrentText(key);
}

} // namespace fpt_ui
