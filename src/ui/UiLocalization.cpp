#include "UiLocalization.h"

#include <QAction>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QToolButton>
#include <QVector>
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

    add("打开", "Open");
    add("保存模板", "Save Template");
    add("载入模板", "Load Template");
    add("保存设置", "Save Settings");
    add("数据库", "Database");
    add("工区底图", "Basemap");
    add("帮助", "Help");

    add("数据库配置", "Database");
    add("刷新网格", "Refresh Grids");
    add("刷新网格列表", "Refresh Grid List");
    add("刷新结果列表", "Refresh Result List");
    add("显示所选结果", "Display Selected Result");
    add("重新载入结果", "Reload Result");
    add("导出视图 PNG", "Export View PNG");
    add("保存界面设置", "Save Interface Settings");
    add("线条", "Line Style");
    add("删除当前结果", "Delete Current Result");
    add("打开工区底图", "Open Basemap");
    add("请从 GeoEast 打开工区视图", "Open the Workspace View from GeoEast");
    add("%1 需要 GeoEast 当前工区的会话信息，无法作为普通独立程序直接启动。%2\n\n请在 GeoEast 顶部工具栏点击【工区底图】或【平面成图】，再在图层树中选择当前网格和结果。插件内的结果视图区仍可直接查看两类图层。",
        "%1 requires the current GeoEast workspace session and cannot be launched as a regular standalone program. %2\n\nUse Basemap or Mapping on the GeoEast toolbar, then select the current grid and result in the layer tree. Both layers remain directly available in the plugin result view.");
    add("已切换回 GeoEast 主窗口。", "The GeoEast main window is now active.");
    add("请先打开并登录 GeoEast。", "Open and sign in to GeoEast first.");
    add("已切换到正在运行的 %1", "Switched to the running %1");
    add("正在确认 %1 是否可用...", "Checking whether %1 is available...");
    add("%1 需要由 GeoEast 当前工区启动", "%1 must be launched from the current GeoEast workspace");
    add("打开平面成图", "Open GeoMapping");
    add("关闭启动器", "Close Launcher");
    add("打开 DAT 文件...", "Open DAT File...");
    add("打开 DAT 文件", "Open DAT");
    add("打开DAT", "Open DAT");
    add("保存当前配置", "Save Settings");
    add("保存配置", "Save Settings");
    add("保存参数模板", "Save Parameter Template");
    add("打开参数模板", "Open Parameter Template");
    add("打开输出目录", "Open Output");
    add("复制日志", "Copy Log");
    add("清空日志", "Clear Log");
    add("保存运行日志...", "Save Run Log...");
    add("保存运行日志", "Save Run Log");
    add("复制当前结果摘要", "Copy Current Result Summary");
    add("平移模式", "Pan Mode");
    add("平移", "Pan");
    add("放大", "Zoom In");
    add("缩小", "Zoom Out");
    add("重置视图", "Reset View");
    add("重置", "Reset");
    add("显示原始网格层", "Show Input Grid Layer");
    add("显示结果多边形层", "Show Result Polygon Layer");
    add("显示图层", "Visible Layers");
    add("图层", "Layers");
    add("同时勾选两层即为叠加查看。", "Check both layers to view the overlay.");
    add("同时开启两层即可叠加查看。", "Turn on both layers to view the overlay.");
    add("叠加查看", "Overlay View");
    add("平台复核：在工区底图或平面成图中勾选网格与结果图层。",
        "Platform review: enable the grid and result layers in GeoBasemap or GeoMapping.");
    add("重新载入结果视图", "Reload Result View");
    add("导出当前视图 PNG", "Export Current View PNG");
    add("开始追踪", "Start Trace");
    add("停止追踪", "Stop Trace");
    add("停止", "Stop");
    add("导出 PNG", "Export PNG");
    add("选择结果颜色...", "Choose Result Color...");
    add("选择结果颜色", "Choose Result Color");
    add("结果颜色值", "Result Color Value");
    add("浏览输入文件", "Browse Input File");
    add("浏览输出目录", "Browse Output Folder");
    add("浏览 DAT 文件", "Browse DAT File");
    add("已有结果", "Available Results");
    add("结果视图区", "Result View");
    add("关闭完成提示", "Dismiss Completion Message");
    add("重新载入视图", "Reload View");
    add("查看叠加步骤", "View Overlay Steps");
    add("线样式", "Line Style");
    add("输出名称无效", "Invalid Output Name");
    add("输出名称不能为空", "Output name cannot be empty.");
    add("输出名称不能超过 100 个字符", "Output name cannot exceed 100 characters.");
    add("输出名称不能包含 \\ / : * ? \" < > |，也不能以空格或句点结尾", "Output name cannot contain \\ / : * ? \" < > | or end with a space or period.");
    add("该名称是 Windows 保留名称，请换一个名称", "This is a reserved Windows name. Choose another name.");
    add("输入文件不可读取", "Input File Cannot Be Read");
    add("所选输入文件为空，请选择包含断层属性数据的有效文件。", "The selected input file is empty. Choose a valid file containing fault attribute data.");
    add("无法读取所选输入文件。请检查文件权限、网络连接，或确认文件没有被其他程序独占。\n\n%1", "The selected input file cannot be read. Check its permissions and network connection, and make sure another application has not locked it.\n\n%1");
    add("输出目录不可用", "Output Folder Is Unavailable");
    add("输入文件与输出文件冲突", "Input and Output Paths Conflict");
    add("输出文件不能覆盖输入文件。请修改输出目录或输出名称后再运行。", "An output file cannot overwrite the input file. Change the output folder or output name before running.");
    add("可用磁盘空间不足", "Insufficient Free Disk Space");
    add("输出目录所在磁盘剩余空间不足。当前可用 %1 MB，建议至少保留 %2 MB 后再运行。", "The output drive does not have enough free space. %1 MB is available; keep at least %2 MB free before running.");
    add("覆盖已有结果？", "Overwrite Existing Results?");
    add("以下结果文件已经存在：\n\n- %1\n\n继续运行将替换这些文件。输入文件不会被修改。", "The following result files already exist:\n\n- %1\n\nContinuing will replace these files. The input file will not be modified.");
    add("覆盖并继续", "Overwrite and Continue");
    add("本地缓存目录不可用", "Local Cache Folder Is Unavailable");
    add("目录为空", "The folder path is empty.");
    add("无法创建目录：%1", "Cannot create folder: %1");
    add("目录不可写：%1\n%2", "The folder is not writable: %1\n%2");
    add("打开叠合查看指南", "Open Overlay Viewing Guide");
    add("保存当前参数", "Save Current Parameters");
    add("保存参数模板...", "Save Parameter Template...");
    add("打开参数模板...", "Open Parameter Template...");
    add("保存参数模板", "Save Parameter Template");
    add("打开参数模板", "Open Parameter Template");
    add("参数模板 (*.json);;所有文件 (*.*)", "Parameter Templates (*.json);;All Files (*.*)");
    add("日志文件 (*.log *.txt);;所有文件 (*.*)", "Log Files (*.log *.txt);;All Files (*.*)");
    add("恢复默认配置", "Restore Default Settings");
    add("恢复默认参数", "Restore Defaults");
    add("恢复默认", "Restore Defaults");
    add("打开用户操作手册", "Open User Manual");
    add("打开异机运行说明", "Open Deployment Guide");
    add("帮助提示", "Help");
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
    add("显示样式 / 线宽", "Display Style / Line Width");
    add("颜色", "Color");
    add("线宽", "Line Width");
    add("透明度", "Opacity");
    add("设置结果多边形在当前视图中的显示线宽", "Set the display line width of result polygons in the current view");
    add("设置结果多边形在当前视图中的透明度，不修改数据库几何", "Set result polygon opacity in the current view without changing database geometry");
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
    add("质量模式", "Quality Mode");
    add("正式处理", "Production");
    add("质量诊断", "Diagnostics");
    add("受控实验", "Controlled Experiment");
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
    add("根系防黏连（推荐）", "Root Anti-merge (Recommended)");
    add("局部桥分裂（按需）", "Local Bridge Split (As Needed)");
    add("弱响应补追（按需）", "Weak Response Recovery (As Needed)");
    add("展开或收起参数区；此操作不会启用、停用或重新运行算法", "Expand or collapse this parameter section. This does not enable, disable, or rerun the algorithm.");
    add("主阈值分位数，0.98 表示保留属性值最高约 2% 的像元。0 不是关闭阈值。", "Primary threshold percentile. 0.98 keeps roughly the highest 2% of attribute cells. Zero does not disable thresholding.");
    add("过滤面积小于该值的碎片。必须大于 0；数值过小会保留大量微小对象。", "Filters fragments smaller than this area. The value must be above zero; very small values retain many tiny objects.");
    add("依据断层根系归属切断缺少属性支撑的连接，降低邻近断层误合并风险。分裂后的小片仍受最小面积过滤。", "Cuts unsupported connections using fault-root ownership to reduce false merging. Small pieces produced by splitting are still subject to the minimum-area filter.");
    add("检查局部低属性短桥并尝试分裂。仅适合明确存在局部黏连时使用，可能使小片因面积过滤而减少。", "Checks short low-attribute bridges and attempts a local split. Use only for clear local sticking; small pieces may then be removed by the area filter.");
    add("在正式主结果之外补充细长弱响应候选。可能增加耗时和碎片，不建议与所有防黏连功能同时开启。", "Adds thin weak-response candidates beyond the primary result. It may increase runtime and fragments and should not normally be enabled with every anti-merge option.");
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
    add("详细日志", "Verbose Log");
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
    add("正在停止…", "Stopping...");
    add("参数设置无效", "Invalid Parameters");
    add("参数设置无效，未启动追踪", "Invalid parameters. Tracing was not started.");
    add("请先修正以下设置，再开始追踪：\n\n- %1", "Correct the following settings before tracing:\n\n- %1");
    add("确认高负载参数", "Confirm High-load Parameters");
    add("当前参数可以运行，但存在以下影响：\n\n- %1\n\n建议先使用推荐预设。若这是有目的的对比实验，可继续运行。", "The parameters can run but have the following effects:\n\n- %1\n\nUse a recommended preset first. Continue only for an intentional comparison.");
    add("返回调整", "Adjust Parameters");
    add("继续运行", "Continue");
    add("追踪模式无效，请重新选择预设或恢复默认参数。", "The trace mode is invalid. Select a preset again or restore defaults.");
    add("阈值模式无效，请重新选择阈值模式。", "The threshold mode is invalid. Select a threshold mode again.");
    add("分位数模式要求分位数大于 0 且小于 1。分位数 0 不是关闭阈值，而是无效设置。", "Percentile modes require a value greater than 0 and less than 1. Zero does not disable thresholding and is invalid.");
    add("分位数低于 0.80 会让过多背景像元进入骨架追踪。请提高分位数，或改用固定阈值模式并依据属性范围设置阈值。", "A percentile below 0.80 admits too many background cells into skeleton tracing. Raise the percentile or use Fixed Threshold mode with a value based on the attribute range.");
    add("固定阈值必须是有限数值。", "The fixed threshold must be a finite number.");
    add("最小面积必须大于 0。设为 0 会保留全部微小碎片，可能造成结果数量和处理负载急剧增加。", "Minimum area must be greater than zero. Zero retains every tiny fragment and may sharply increase output size and processing load.");
    add("带宽半径必须至少为 1 个像元。", "Belt radius must be at least one cell.");
    add("最短分支必须至少为 1 个像元。", "Minimum branch length must be at least one cell.");
    add("启用弱响应补追时，补追分位数必须大于 0 且小于 1。", "Weak-response recovery requires a recovery percentile greater than 0 and less than 1.");
    add("当前分位数低于 0.90，活动像元和骨架分支可能显著增加。建议优先使用推荐预设，仅在明确了解数据分布时继续。", "The percentile is below 0.90, which may greatly increase active cells and skeleton branches. Prefer a recommended preset and continue only when the data distribution is understood.");
    add("固定阈值小于或等于 0，可能把大面积背景纳入追踪。请确认该阈值符合输入属性的数值范围。", "A fixed threshold at or below zero may include large background areas. Confirm that it matches the input attribute range.");
    add("弱响应分位不低于主分位数，补充层不会提供更弱的连续性信息，结果可能比预期更少。", "The weak-response percentile is not below the primary percentile, so it adds no weaker continuity information and may produce fewer results than expected.");
    add("补追分位数不低于主分位数，二次追踪通常无法补充弱响应断层。", "The recovery percentile is not below the primary percentile, so secondary tracing will usually add no weak-response faults.");
    add("补追上限为 0 表示不限制连通块大小，复杂网格上可能明显增加处理时间。", "A recovery limit of zero leaves component size unlimited and may substantially increase runtime on complex grids.");
    add("局部桥分裂与弱响应补追同时启用时，会先增加弱响应候选再执行局部分裂，处理时间和结果敏感性都会提高。", "Using local bridge splitting with weak-response recovery adds weak candidates before local splitting, increasing runtime and result sensitivity.");
    add("三个按需增强功能已同时启用。它们不是叠加后必然更优的质量开关，建议仅用于受控对比并逐项核验结果。", "All three optional enhancements are enabled. They are not cumulative quality switches and should be used only for controlled comparisons with item-by-item review.");
    add("当前使用受控实验模式，运行时间和输出对象可能与正式处理模式不同。", "Controlled Experiment mode is active. Runtime and output objects may differ from Production mode.");
    add("请等待当前任务完成或先点击【停止】，再加载其它图层。", "Wait for the current task to finish or click [Stop] before loading another layer.");
    add("任务运行中，暂不能加载其它图层", "A task is running. Other layers cannot be loaded yet.");
    add("请先选择输入文件", "Select an Input File First");
    add("显示原始网格前，请先在数据源区域选择可读取的断层属性文件。", "Select a readable fault attribute file in Data Source before displaying the input grid.");
    add("没有可显示的结果", "No Result to Display");
    add("请先运行追踪，或点击【刷新结果列表】并选择一个已有结果。", "Run tracing first, or click [Refresh Result List] and select an existing result.");
    add("网格列表尚未读取", "Grid List Not Loaded");
    add("当前项目没有可用网格；需要重新查询时请点击【刷新网格列表】", "No grid is available in the current project. Click [Refresh Grid List] to query again.");
    add("当前项目没有已有结果；需要重新查询时请点击【刷新结果列表】", "No existing result is available in the current project. Click [Refresh Result List] to query again.");
    add("正在读取当前项目的输入网格。读取完成后，请选择网格并再次打开原始网格图层。", "The input grid list is being loaded. Select a grid and turn on the input grid layer after loading finishes.");
    add("尚未生成本地视图文件", "Local View Files Not Available");
    add("请先运行追踪，或在结果列表中选择对象后点击【显示所选结果】。", "Run tracing first, or select an object in the result list and click [Display Selected Result].");
    add("原始网格载入失败", "Input Grid Load Failed");
    add("未能从 GeoEast 导出或解析所选网格。请检查当前项目、网格选择、数据库连接和日志。", "The selected grid could not be exported from GeoEast or parsed. Check the current project, grid selection, database connection, and log.");
    add("矩阵网格尺寸过大，无法安全载入视图。", "The matrix grid is too large to load safely in the view.");
    add("GeoEast DAT 网格尺寸过大，无法安全载入视图。", "The GeoEast DAT grid is too large to load safely in the view.");
    add("原始网格尺寸过大，无法安全载入视图。", "The input grid is too large to load safely in the view.");
    add("结果载入失败", "Result Load Failed");
    add("未能导出或解析所选结果。请确认输入网格与结果属于当前项目，并检查数据库连接和日志。", "The selected result could not be exported or parsed. Confirm that the grid and result belong to the current project, then check the database connection and log.");
    add("图层载入失败", "Layer Load Failed");
    add("执行程序未能完成图层载入。请检查执行程序路径、数据库连接、当前项目和运行日志。", "The executable could not complete layer loading. Check its path, the database connection, the current project, and the run log.");
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
    add("面积过滤：小于 %1 的多边形不输出    结果线宽：%2    透明度：%3", "Area Filter: polygons below %1 are excluded    Line Width: %2    Opacity: %3");
    add("弱响应：分位 %1 / 半径 %2    根系防黏连：%3    残余补追：%4", "Weak Response: percentile %1 / radius %2    Root Anti-merge: %3    Residual Trace: %4");
    add("当前状态：%1", "Status: %1");
    add("保存失败", "Save Failed");
    add("打开失败", "Open Failed");
    add("模板无效", "Invalid Template");
    add("日志为空", "Empty Log");
    add("当前没有可保存的运行日志。", "There is no run log to save.");
    add("无法写入参数模板：%1", "Cannot write parameter template: %1");
    add("无法读取参数模板：%1", "Cannot read parameter template: %1");
    add("参数模板未能安全写入磁盘。", "The parameter template could not be written safely to disk.");
    add("JSON 解析失败：%1", "JSON parsing failed: %1");
    add("无法保存运行日志：%1", "Cannot save run log: %1");
    add("参数模板已保存", "Parameter template saved");
    add("参数模板已载入并通过校验", "Parameter template loaded and validated");
    add("运行日志已保存", "Run log saved");
    add("当前结果摘要已复制", "Current result summary copied");
    add("尚未载入图层", "No layer loaded");
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
    add("追踪完成：%1 个多边形，耗时 %2", "Trace completed: %1 polygons in %2");
    add("追踪完成：结果已写入 BOFaultPolygon/%1，可叠加检查或到工区底图复核",
        "Trace completed: result written to BOFaultPolygon/%1. Review it as an overlay or in Basemap.");
    add("未发现保留多边形", "No Kept Polygons Found");

    add("配置 GeoEast 根目录、数据根目录、数据库、项目、用户和执行程序", "Configure GeoEast root, data root, database, project, user, and execution program");
    add("刷新 GeoEast 当前项目下的输入网格列表", "Refresh input grids in the current GeoEast project");
    add("打开或选择 DAT 文件", "Open or select a DAT file");
    add("保存当前界面配置和参数", "Save current UI settings and parameters");
    add("选择结果多边形颜色", "Choose result polygon color");
    add("从 GeoEast 数据库读取可用输入网格列表", "Read available input grids from the GeoEast database");
    add("控制结果视图区是否显示本次追踪生成的原始网格图层。", "Show or hide the input grid layer generated for this trace.");
    add("控制结果视图区是否显示本次追踪生成的结果多边形图层。", "Show or hide the result polygon layer generated for this trace.");
    add("显示或隐藏输入断层属性网格。首次打开大 DAT 时会按需载入。", "Show or hide the input fault-attribute grid. Large DAT files are loaded only when needed.");
    add("显示或隐藏输入断层属性网格。GeoEast 模式会按需导出当前 BOMapGrid 视图。", "Show or hide the input fault-attribute grid. GeoEast mode exports the current BOMapGrid view only when needed.");
    add("显示或隐藏追踪结果多边形。没有结果时会提示先运行追踪或载入结果。", "Show or hide traced result polygons. If no result exists, the software prompts you to run tracing or load a result first.");
    add("显示或隐藏追踪结果多边形。没有结果时请先追踪或选择已有结果。", "Show or hide traced result polygons. If no result exists, run tracing or select an existing result first.");
    add("同时显示原始网格和结果多边形，用于检查贴合关系。", "Show the input grid and result polygons together to inspect their fit.");
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
    add("没有可导出的图层", "No Layers to Export");
    add("请先载入并显示原始网格或结果多边形图层，再导出当前视图 PNG。", "Load and show the original grid or result polygon layer before exporting the current view as PNG.");
    add("请先显示可导出的图层", "Show an exportable layer first");
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

    // Shared validation and configuration text used by both product surfaces.
    add("执行程序文件未填写", "Executable path is empty");
    add("执行程序文件不存在：%1", "Executable does not exist: %1");
    add("GeoEast 根目录未填写", "GeoEast root is empty");
    add("GeoEast 根目录不存在：%1", "GeoEast root does not exist: %1");
    add("GeoEast 根目录应包含 NGP 和 GeoEast-RC 子目录", "GeoEast root must contain the NGP and GeoEast-RC folders");
    add("GeoEast 数据根目录未填写", "GeoEast data root is empty");
    add("GeoEast 数据根目录不存在：%1", "GeoEast data root does not exist: %1");
    add("PG 主机未填写", "PostgreSQL host is empty");
    add("PG 端口应为 1 到 65535 之间的数字", "PostgreSQL port must be a number from 1 to 65535");
    add("数据库名未填写", "Database name is empty");
    add("项目名未填写。请在 GeoEast 主控标题、打开项目窗口或项目管理工具中确认",
        "Project name is empty. Check the GeoEast main window title, Open Project dialog, or project manager.");
    add("用户名未填写", "User name is empty");
    add("<已隐藏>", "<hidden>");
    add("执行程序", "Executable");
    add("GeoEast根目录", "GeoEast Root");
    add("数据根目录", "Data Root");
    add("PG主机", "PG Host");
    add("PG 主机/端口", "PG Host / Port");
    add("端口", "Port");
    add("数据库", "Database");
    add("项目", "Project");
    add("用户", "User");
    add("密码", "Password");
    add("浏览...", "Browse...");
    add("执行程序文件:", "Executable:");
    add("GeoEast 根目录:", "GeoEast Root:");
    add("GeoEast 数据根目录:", "GeoEast Data Root:");
    add("PG 主机:", "PG Host:");
    add("端口:", "Port:");
    add("数据库:", "Database:");
    add("项目:", "Project:");
    add("用户:", "User:");
    add("密码:", "Password:");
    add("选择 GeoEast 执行程序", "Select GeoEast Executable");
    add("选择 GeoEast 安装根目录", "Select GeoEast Installation Root");
    add("选择 GeoEastData 根目录", "Select GeoEastData Root");
    add("可执行文件 (*.exe);;所有文件 (*.*)", "Executables (*.exe);;All Files (*.*)");
    add("输入输出", "Input and Output");
    add("选择颜色", "Choose Color");
    add("GeoEast 数据库与环境配置", "GeoEast Database and Environment Configuration");
    add("GeoEast 连接配置", "GeoEast Connection Configuration");
    add("查看每个配置项从哪里获取", "Learn where to find each configuration value");
    add("尚未检查配置。", "Configuration has not been checked.");
    add("自动检测", "Auto Detect");
    add("检查配置", "Check Configuration");
    add("取消", "Cancel");
    add("应用并保存配置", "Apply and Save");
    add("已执行自动检测。项目名无法安全推断，请按当前 GeoEast 项目填写后再检查配置。",
        "Automatic detection finished. Enter the current GeoEast project name, then check the configuration.");
    add("配置检查通过。可以刷新网格列表。", "Configuration is valid. You can now refresh the grid list.");
    add("配置检查通过", "Configuration Valid");
    add("GeoEast 数据库配置项完整。保存后可返回主界面刷新网格列表。",
        "The GeoEast database configuration is complete. Save it, then return to the main window and refresh the grid list.");
    add("配置仍有 %1 项需要处理。", "%1 configuration item(s) still require attention.");
    add("配置需要补充", "Configuration Incomplete");
    add("请处理以下项目：\n\n- %1", "Resolve the following item(s):\n\n- %1");
    add("GeoEast 配置帮助", "GeoEast Configuration Help");
    add("GeoEast 数据库配置用于让 FaultPolyTrace 读取 BOMapGrid，并把追踪结果写入 BOFaultPolygon。\n\n"
        "字段来源：\n"
        "1. 执行程序文件：通常位于 GeoEast-RC\\bin\\faultpolytrace_geoeast_runner.exe，发布包或 GeoEast 插件部署后会自动检测。\n"
        "2. GeoEast 根目录：包含 NGP 和 GeoEast-RC 两个子目录的安装根目录。\n"
        "3. GeoEast 数据根目录：GeoEastData 根目录，通常包含 pgdata 和 DATA 等数据目录。\n"
        "4. PG 主机和端口：本机 GeoEast 通常为 127.0.0.1 和 5433；如果连接远端数据库，请填写管理员提供的地址。\n"
        "5. 数据库：通常为 ndp，也可从 GeoEast 项目管理或数据源配置中确认。\n"
        "6. 项目：GeoEast 当前打开或需要处理的项目名，可在 GeoEast 主控标题、打开项目窗口或项目管理工具中查看。\n"
        "7. 用户：GeoEast 登录用户，通常与 Windows 用户名一致。\n\n"
        "建议流程：先点击【自动检测】，再点击【检查配置】。检查通过后点击【应用并保存配置】，然后返回主界面点击【刷新网格列表】。",
        "GeoEast database configuration lets FaultPolyTrace read BOMapGrid objects and write tracing results to BOFaultPolygon.\n\n"
        "Where to find each value:\n"
        "1. Executable: normally GeoEast-RC\\bin\\faultpolytrace_geoeast_runner.exe; it is detected after package or plugin deployment.\n"
        "2. GeoEast Root: the installation folder containing NGP and GeoEast-RC.\n"
        "3. GeoEast Data Root: the GeoEastData folder, normally containing pgdata and DATA.\n"
        "4. PG Host and Port: normally 127.0.0.1 and 5433 for local GeoEast; use administrator-provided values for a remote database.\n"
        "5. Database: normally ndp; confirm it in GeoEast project management or data-source settings.\n"
        "6. Project: the open or target GeoEast project, shown in the main title, Open Project dialog, or project manager.\n"
        "7. User: the GeoEast login user, commonly the Windows user name.\n\n"
        "Recommended sequence: click [Auto Detect], then [Check Configuration]. After validation, click [Apply and Save], return to the main window, and click [Refresh Grid List].");
    add("GeoEast 数据库配置用于让 FaultPolyTrace 读取 BOMapGrid，并把追踪结果写入 BOFaultPolygon。\n\n"
        "字段来源：\n"
        "1. 执行程序文件：通常位于 GeoEast-RC\\bin\\faultpolytrace_geoeast_runner.exe，GeoEast 插件部署后会自动检测。\n"
        "2. GeoEast 根目录：包含 NGP 和 GeoEast-RC 两个子目录的安装根目录。\n"
        "3. GeoEast 数据根目录：GeoEastData 根目录，通常包含 pgdata 和 DATA 等数据目录。\n"
        "4. PG 主机和端口：本机 GeoEast 通常为 127.0.0.1 和 5433；如果连接远端数据库，请填写管理员提供的地址。\n"
        "5. 数据库：通常为 ndp，也可从 GeoEast 项目管理或数据源配置中确认。\n"
        "6. 项目：GeoEast 当前打开或需要处理的项目名，可在 GeoEast 主控标题、打开项目窗口或项目管理工具中查看。\n"
        "7. 用户：GeoEast 登录用户，通常与 Windows 用户名一致。\n\n"
        "建议流程：先点击【自动检测】，再点击【检查配置】。检查通过后点击【应用并保存配置】，然后返回主界面点击【刷新网格列表】。",
        "GeoEast database configuration lets FaultPolyTrace read BOMapGrid objects and write tracing results to BOFaultPolygon.\n\n"
        "Where to find each value:\n"
        "1. Executable: normally GeoEast-RC\\bin\\faultpolytrace_geoeast_runner.exe; it is detected after plugin deployment.\n"
        "2. GeoEast Root: the installation folder containing NGP and GeoEast-RC.\n"
        "3. GeoEast Data Root: the GeoEastData folder, normally containing pgdata and DATA.\n"
        "4. PG Host and Port: normally 127.0.0.1 and 5433 for local GeoEast; use administrator-provided values for a remote database.\n"
        "5. Database: normally ndp; confirm it in GeoEast project management or data-source settings.\n"
        "6. Project: the open or target GeoEast project, shown in the main title, Open Project dialog, or project manager.\n"
        "7. User: the GeoEast login user, commonly the Windows user name.\n\n"
        "Recommended sequence: click [Auto Detect], then [Check Configuration]. After validation, click [Apply and Save], return to the main window, and click [Refresh Grid List].");
    add("配置尚未完整", "Configuration Incomplete");
    add("以下配置可能导致刷新网格或写库失败：\n\n- %1\n\n建议返回修改。是否仍然保存？",
        "The following settings may prevent grid refresh or database writes:\n\n- %1\n\nReturn to correct them. Save anyway?");
    add("配置未保存，请补齐后再应用。", "Configuration was not saved. Complete it before applying.");
    add("打开数据库配置", "Open Database Configuration");
    add("去填写", "Configure Now");
    add("GeoEast 配置不完整", "GeoEast Configuration Incomplete");
    add("执行【%1】前需要先补齐 GeoEast 配置。", "Complete the GeoEast configuration before %1.");
    add("缺失或无效项：\n\n- %1", "Missing or invalid item(s):\n\n- %1");
    add("缺失或无效项：\n\n- %1\n\n请点击【打开数据库配置】补齐后再执行。保存一次后，下次打开会自动记住。",
        "Missing or invalid item(s):\n\n- %1\n\nClick [Open Database Configuration] to complete the settings. Saved settings are restored next time.");
    add("无法执行【%1】：GeoEast 配置不完整。缺失或无效项：%2",
        "Cannot perform %1 because the GeoEast configuration is incomplete. Missing or invalid item(s): %2");

    // Runtime status, result paths, and user guidance.
    add("运行日志已复制到剪贴板", "Run log copied to the clipboard");
    add("运行日志已清空", "Run log cleared");
    add("日志已复制到剪贴板。", "Log copied to the clipboard.");
    add("日志已清空", "Log cleared");
    add("视图区已切换为平移模式", "View switched to pan mode");
    add("视图区已切换为选择模式", "View switched to selection mode");
    add("正在载入原始网格...", "Loading input grid...");
    add("正在载入结果多边形...", "Loading result polygons...");
    add("结果多边形已载入", "Result polygons loaded");
    add("结果多边形载入失败", "Failed to load result polygons");
    add("正在自动读取当前项目的数据列表...", "Reading the current project catalog...");
    add("请先完成数据库配置；选择输入网格时可打开配置引导",
        "Complete the database configuration first. Open the setup guide from the input grid selector.");
    add("已选择输入文件；勾选“显示原始网格层”后再载入图像",
        "Input file selected. Enable Show Input Grid Layer to load the image.");
    add("已选择输入文件，未自动载入图像以保持界面流畅",
        "Input file selected. The image was not loaded automatically to keep the interface responsive.");
    add("正在刷新 GeoEast 网格列表...", "Refreshing the GeoEast grid list...");
    add("正在刷新 GeoEast 结果列表...", "Refreshing the GeoEast result list...");
    add("任务运行中，暂不能刷新已有结果", "A task is running. Existing results cannot be refreshed yet.");
    add("已刷新输出目录结果，共 %1 个", "Output result list refreshed: %1 item(s)");
    add("暂无可载入结果，请先运行追踪或刷新已有结果列表",
        "No result is available. Run tracing or refresh the existing result list first.");
    add("没有可载入的结果", "No Result Available");
    add("请先运行追踪，或点击【刷新结果列表】读取输出目录/GeoEast 数据库中的已有结果。",
        "Run tracing or click [Refresh Result List] to read results from the output folder or GeoEast database.");
    add("任务运行中，暂不能载入 GeoEast 历史结果", "A task is running. GeoEast results cannot be loaded yet.");
    add("载入已有结果", "Load Existing Result");
    add("正在导出 GeoEast 历史结果...", "Exporting the selected GeoEast result...");
    add("结果文件不存在，请刷新结果列表", "The result file does not exist. Refresh the result list.");
    add("已载入结果多边形；如需叠加，请再选择对应原始网格",
        "Result polygons loaded. Select the corresponding input grid for an overlay.");
    add("已载入已有结果：%1", "Existing result loaded: %1");
    add("确认删除 GeoEast 结果", "Confirm GeoEast Result Deletion");
    add("将删除当前项目中的结果多边形对象：\n\n%1\n\n该操作不会删除输入网格，也不会删除本地输出文件。是否继续？",
        "The following result polygon object will be deleted from the current project:\n\n%1\n\nThe input grid and local output files will not be deleted. Continue?");
    add("将删除当前项目中的结果多边形对象：\n\n%1\n\n该操作不会删除输入网格，也不会删除其它解释对象。是否继续？",
        "The following result polygon object will be deleted from the current project:\n\n%1\n\nThe input grid and other interpretation objects will not be deleted. Continue?");
    add("删除", "Delete");
    add("正在删除 GeoEast 结果对象...", "Deleting the GeoEast result object...");
    add("正在连接 GeoEast 并追踪断层多边形...", "Connecting to GeoEast and tracing fault polygons...");
    add("正在追踪断层多边形...", "Tracing fault polygons...");
    add("算法程序定位错误", "Algorithm Executable Location Error");
    add("独立软件不能把自身作为核心算法程序启动。请确认发布包中存在 bin/faultpolytrace.exe。",
        "The standalone application cannot use itself as the algorithm executable. Confirm that bin/faultpolytrace.exe exists in the package.");
    add("GeoEast 结果刷新完成，共 %1 个", "GeoEast result refresh completed: %1 item(s)");
    add("GeoEast 结果刷新失败，请查看日志", "GeoEast result refresh failed. Check the log.");
    add("GeoEast 结果对象已删除或原本不存在", "The GeoEast result object was deleted or did not exist");
    add("GeoEast 结果删除失败，请查看日志", "GeoEast result deletion failed. Check the log.");
    add("运行结束，但未生成结果 CSV：%1", "Run finished without a result CSV: %1");
    add("请检查输入格式、输出目录写入权限和上方算法日志。",
        "Check the input format, output-folder write permission, and algorithm log above.");
    add("GeoEast 数据库写入对象：BOFaultPolygon/%1", "GeoEast database object: BOFaultPolygon/%1");
    add("原始网格视图文件：%1", "Input grid view file: %1");
    add("GeoEast 历史结果已导出：BOFaultPolygon/%1", "GeoEast result exported: BOFaultPolygon/%1");
    add("结果 CSV：%1", "Result CSV: %1");
    add("GeoJSON 文件：%1", "GeoJSON file: %1");
    add("诊断 JSON：%1", "Diagnostics JSON: %1");
    add("拓扑 CSV：%1", "Topology CSV: %1");
    add("枝干 CSV：%1", "Branch CSV: %1");
    add("GeoEast 历史结果已导出并载入", "GeoEast result exported and loaded");
    add("进程错误：%1", "Process error: %1");
    add("追踪中…", "Tracing...");
    add("自定义参数", "Custom Parameters");
    add("参数已手动调整；重新选择任一预设可整体恢复",
        "Parameters were adjusted manually. Select a preset to restore its full parameter set.");
    add("参数模板已保存：%1", "Parameter template saved: %1");
    add("参数模板已载入：%1", "Parameter template loaded: %1");
    add("无法读取 CSV：%1", "Cannot read CSV: %1");
    add("无法打开输入文件：%1", "Cannot open input file: %1");
    add("输入文件为空。", "The input file is empty.");
    add("矩阵网格头不足。", "The matrix-grid header is incomplete.");
    add("矩阵网格头无效。", "The matrix-grid header is invalid.");
    add("矩阵网格数值数量不足。", "The matrix grid does not contain enough values.");
    add("未识别到 GeoEast DAT 五列采样数据。", "No five-column GeoEast DAT samples were detected.");
    add("无法打开结果 CSV：%1", "Cannot open result CSV: %1");
    add("等待数据", "Waiting for Data");
    add("选择外部断层属性网格文件 (Ctrl+O)", "Select an external fault-attribute grid (Ctrl+O)");
    add("按当前参数运行断层多边形自动追踪 (F5)", "Run fault polygon tracing with current parameters (F5)");
    add("终止当前正在运行的追踪任务 (Shift+F5)", "Stop the running trace task (Shift+F5)");
    add("导出当前视图区图像 (Ctrl+Shift+P)", "Export the current view image (Ctrl+Shift+P)");
    add("保存当前算法参数模板 (Ctrl+S)", "Save the current parameter template (Ctrl+S)");
    add("读取已保存的参数模板 (Ctrl+Shift+O)", "Load a saved parameter template (Ctrl+Shift+O)");
    add("重新读取输出目录或数据库中的已有结果 (F6)", "Refresh results from the output folder or database (F6)");
    add("复制运行日志", "Copy Run Log");
    add("清空运行日志", "Clear Run Log");
    add("\n---- 详细日志（界面已折叠）----\n", "\n---- Verbose Log (collapsed in the interface) ----\n");
    add(" 像元", " cells");
    add(" 次", " iterations");
    add("删除结果", "Delete Result");
    add("列出可用于查看或管理的已有结果。外部文件模式读取输出目录，GeoEast 模式读取当前项目中的结果多边形对象。",
        "List results available for viewing or management. External-file mode reads the output folder; GeoEast mode reads result polygon objects in the current project.");
    add("载入所选结果", "Load Selected Result");
    add("形态学闭运算半径：调大可修复更宽的小断口，过大易把邻近断层连成一片",
        "Morphological closing radius: larger values repair wider small gaps but may connect nearby faults.");
    add("由骨架向两侧恢复断层带的半径：调大结果更宽，调小更贴中心线",
        "Fault-belt radius restored from the skeleton: increase for wider results or decrease for closer centerline fit.");
    add("断层带成形后的闭合半径：修补带间小缝隙，0 最保守",
        "Post-belt closing radius: repairs small gaps between belt cells; 0 is the most conservative.");
    add("原始属性包络半径：限制断层带向背景外扩，调大更宽松",
        "Attribute-envelope radius: limits expansion into the background; larger values are more permissive.");
    add("断点补连的最大端点距离：调大可跨越更长缺口，过大易误连邻近断层",
        "Maximum endpoint gap: larger values bridge longer gaps but may connect nearby faults incorrectly.");
    add("补连允许的最大方向差：调大能容纳更弯的断层，过大易跨脉络误连",
        "Maximum connection-angle difference: larger values allow greater curvature but may connect unrelated trends.");
    add("Chaikin 平滑次数：0 保留像元台阶，次数越多边界越圆滑",
        "Chaikin smoothing iterations: 0 preserves pixel steps; more iterations produce smoother boundaries.");
    add("Douglas-Peucker 简化容差：调大顶点更少，过大会损失形态细节",
        "Douglas-Peucker tolerance: larger values reduce vertices but may remove shape detail.");
    add("单击标题旁的勾选框展开或收起高级参数", "Use the title checkbox to expand or collapse advanced parameters");
    add("GeoEast 执行程序文件", "GeoEast Executable");
    add("GeoEast 执行程序文件不存在：%1", "GeoEast executable does not exist: %1");
    add("GeoEast 根目录", "GeoEast Root");
    add("GeoEast 数据根目录", "GeoEast Data Root");
    add("数据库名", "Database Name");
    add("项目名", "Project Name");
    add("用户名", "User Name");
    add("输出多边形名称", "Output Polygon Name");
    add("执行程序文件", "Executable");
    add("刷新已有结果", "Refresh Existing Results");
    add("GeoEast数据库：%1 / %2 / BOMapGrid/%3", "GeoEast Database: %1 / %2 / BOMapGrid/%3");
    add("外部文件：%1", "External File: %1");
    add("结果已写入 GeoEast 数据库，并已生成本地视图文件。",
        "Results were written to the GeoEast database and local view files were generated.");
    add("已选择 GeoEast 数据库结果；当前没有同名本地视图缓存。",
        "A GeoEast database result is selected, but no matching local view cache is available.");
    add("GeoEast对象：BOFaultPolygon/%1\n本地CSV：%2", "GeoEast Object: BOFaultPolygon/%1\nLocal CSV: %2");
    add("GeoEast对象：BOFaultPolygon/%1\n本地缓存：%2", "GeoEast Object: BOFaultPolygon/%1\nLocal Cache: %2");

    add("无法打开原始网格导出文件：%1", "Cannot open exported input-grid file: %1");
    add("原始网格导出文件格式不完整。", "The exported input-grid file is incomplete.");
    add("原始网格导出文件头无效。", "The exported input-grid header is invalid.");
    add("原始网格尺寸无效。", "The input-grid dimensions are invalid.");
    add("原始网格数值数量不足。", "The input grid does not contain enough values.");
    add("无法打开结果多边形导出文件：%1", "Cannot open exported result-polygon file: %1");
    add("自动识别失败时请手动选择 GeoEast 安装根目录", "Select the GeoEast installation root if automatic detection fails");
    add("自动识别失败时请手动选择 GeoEastData 根目录", "Select the GeoEastData root if automatic detection fails");
    add("GeoEast 数据库", "GeoEast Database");
    add("文件不存在：%1。请确认 docs 目录已随发布包或 GeoEast 便携启动器部署。",
        "File not found: %1. Confirm that the docs folder was deployed with the package or GeoEast portable launcher.");
    add("无法打开文件：%1", "Cannot open file: %1");
    add("无法打开目录：%1", "Cannot open folder: %1");
    add("本工具直接处理平面断层属性网格，不修改层位解释对象；正在打开 GeoEast 叠合查看指南。",
        "This tool processes planar fault-attribute grids without modifying horizon interpretation objects. Opening the GeoEast overlay guide.");
    add("正在打开 GeoEast 叠加查看步骤。官方图层显示和关闭请在 GeoEast 图层树中操作。",
        "Opening the GeoEast overlay instructions. Show or hide official layers in the GeoEast layer tree.");
    add("加载项目配置：%1 成功。", "Project configuration loaded: %1.");
    add("选择官方 DAT 文件", "Select Official DAT File");
    add("DAT 文件 (*.dat);;所有文件 (*.*)", "DAT Files (*.dat);;All Files (*.*)");
    add("帮助文档：docs/用户操作手册.md；异机交付说明：docs/评审交付与异机运行说明.md。",
        "Help: docs/User Manual; deployment details: docs/Deployment Guide.");
    add("选择 GeoEast 结果多边形颜色", "Choose GeoEast Result Polygon Color");
    add("已启动 %1：%2", "Started %1: %2");
    add("官方窗口中请在图层树加载 Grids/%1 与 Fault Polygons/%2；本插件无法从外部进程强制勾选 GeoEast 内部图层。",
        "In the GeoEast window, load Grids/%1 and Fault Polygons/%2 from the layer tree. This plugin cannot force layer selection in another process.");
    add("已打开 %1，按官方图层树查看结果", "%1 opened. View the result through the GeoEast layer tree.");
    add("正在刷新 GeoEast 网格列表", "Refreshing the GeoEast grid list");
    add("正在刷新 GeoEast 结果列表", "Refreshing the GeoEast result list");
    add("正在追踪断层多边形", "Tracing fault polygons");
    add("正在删除 GeoEast 结果对象", "Deleting the GeoEast result object");
    add("GeoBasemap 工区底图", "GeoBasemap");
    add("GeoMapping 平面成图", "GeoMapping");
    add("当前输出目录中没有可查看的结果 CSV。\n\n"
        "请先选择输入数据并完成一次追踪，或检查“输出目录”后点击【刷新结果列表】。",
        "There is no result CSV in the current output folder.\n\n"
        "Select input data and complete a trace, or check the Output Folder and click [Refresh Result List].");
    add("打开文件", "Open Files");
    add("结果 CSV", "Result CSV");
    add("诊断 JSON", "Diagnostics JSON");
    add("三步完成断层多边形追踪", "Trace Fault Polygons in Three Steps");
    add("打开 DAT 文件，或接入 GeoEast 数据库网格", "Open a DAT file or connect to a GeoEast database grid");
    add("检查追踪参数（推荐预设已就绪）", "Review trace parameters; a recommended preset is ready");
    add("单击左下角「开始追踪」，随后开启结果图层", "Click Start Trace, then enable the result layers");
    add("三步完成工区断层多边形追踪", "Trace Fault Polygons in Three Steps");
    add("确认数据库连接与当前工区（自动检测可一键填充）",
        "Confirm the database and project settings.");
    add("刷新并选择输入 BOMapGrid，填写输出多边形名称",
        "Select a BOMapGrid and name the output.");
    add("单击「开始追踪」，完成后开启结果图层查看",
        "Click Start Trace, then enable result layers.");
    add("当前 GeoEast 项目名，未识别时请填写", "Current GeoEast project name; enter it if detection fails");
    add("GeoEast 登录用户名", "GeoEast login user name");
    add("刷新已有结果列表", "Refresh the existing result list");
    add("载入当前选择的已有结果；GeoEast 数据库对象会同步为当前输出名称",
        "Load the selected result; a GeoEast database object is also used as the current output name");
    add("勾选后显示后台程序的原始英文输出，便于排查问题",
        "Show raw executable output for troubleshooting");
    add("恢复推荐算法参数、显示样式和默认输出名，不清空数据库连接信息",
        "Restore recommended parameters, display style, and output name while keeping database settings");
    add("当前 GeoEast 项目中已有的结果多边形对象。选择后可同步为当前输出名称，便于查看、覆盖或删除。",
        "Result polygon objects in the current GeoEast project. Select one to use its name for viewing, overwriting, or deletion.");
    add("导出并显示当前选择的结果多边形，同时叠加当前输入网格。",
        "Export and display the selected result polygons with the current input grid overlay.");

    // GeoEast launcher runtime text.
    add("GeoEast 连接配置已保存。", "GeoEast connection configuration saved.");
    add("当前参数和界面配置已保存。", "Current parameters and interface settings saved.");
    add("已恢复推荐默认配置：防黏连增强、青色结果线、默认输出名。数据库连接信息已保留。",
        "Recommended defaults restored: anti-merge preset, cyan result line, and default output name. Database connection settings were kept.");
    add("已恢复默认配置", "Default settings restored");
    add("任务运行中，暂不能载入输入网格", "A task is running. The input grid cannot be loaded yet.");
    add("显示输入网格", "Display Input Grid");
    add("正在载入输入网格：%1", "Loading input grid: %1");
    add("正在载入输入网格", "Loading input grid");
    add("暂无可显示结果，请先运行追踪或刷新结果列表",
        "No displayable result is available. Run tracing or refresh the result list first.");
    add("没有可选择的结果", "No Selectable Result");
    add("请先运行追踪，或点击【刷新结果列表】读取当前项目中的结果多边形对象。",
        "Run tracing or click [Refresh Result List] to read result polygon objects in the current project.");
    add("任务运行中，暂不能载入已有结果", "A task is running. Existing results cannot be loaded yet.");
    add("正在载入已有结果对象：%1", "Loading existing result object: %1");
    add("正在导出并载入已有结果对象", "Exporting and loading the existing result object");
    add("已载入原始网格视图：%1", "Input grid view loaded: %1");
    add("已载入结果多边形视图：%1", "Result polygon view loaded: %1");
    add("结果视图已载入", "Result view loaded");
    add("未找到结果视图文件，请先运行追踪", "Result view files were not found. Run tracing first.");
    add("未找到结果视图文件", "Result View Files Not Found");
    add("结果视图 PNG 已导出：%1", "Result view PNG exported: %1");
    add("结果视图 PNG 已导出", "Result view PNG exported");
    add("结果视图 PNG 导出失败：%1", "Failed to export result view PNG: %1");
    add("结果视图 PNG 导出失败", "Failed to export result view PNG");
    add("网格对象刷新完成", "Grid object refresh completed");
    add("结果对象刷新完成，共 %1 个", "Result object refresh completed: %1 item(s)");
    add("结果对象刷新完成", "Result object refresh completed");
    add("结果对象刷新失败，请查看日志", "Result object refresh failed. Check the log.");
    add("原始网格载入失败，请查看日志", "Input grid loading failed. Check the log.");
    add("原始网格导出失败，请查看日志", "Input grid export failed. Check the log.");
    add("已有结果已载入", "Existing result loaded");
    add("已有结果载入失败，请查看日志", "Existing result loading failed. Check the log.");
    add("已有结果载入失败", "Existing Result Loading Failed");
    add("结果对象已删除或原本不存在", "The result object was deleted or did not exist");
    add("删除失败，请查看日志", "Deletion failed. Check the log.");
    add("追踪完成，结果已写入 GeoEast 数据库", "Tracing completed. Results were written to the GeoEast database.");
    add("GeoEast 数据库配置", "GeoEast Database Configuration");
    add("FaultPolyTrace 帮助提示", "FaultPolyTrace Help");
    add("FaultPolyTrace 断层多边形自动追踪\n\n"
        "定位：平面断层属性到闭合断层多边形的自动追踪工具。\n"
        "入口：可独立运行，也可通过 GeoEast 菜单调用执行程序。",
        "FaultPolyTrace Fault Polygon Auto Tracing\n\n"
        "Purpose: automatically trace closed polygons from planar fault attributes.\n"
        "Access: run as a standalone application or launch the executable from GeoEast.");
    add("常用流程：\n"
        "1. 在【数据库配置】中确认 GeoEast 根目录、数据根目录、项目、用户和执行程序。\n"
        "2. 点击【刷新网格列表】读取 GeoEast 中的输入网格。\n"
        "3. 设置输入网格、输出多边形、结果颜色和必要参数。\n"
        "4. 点击【开始追踪】，完成后本窗口会显示原始网格和结果多边形。\n"
        "5. 如需用 GeoEast 官方窗口查看，点击【打开工区底图】或【打开平面成图】，再在图层树中勾选 Grids 和 Fault Polygons。\n"
        "6. 如需移除旧结果，在【结果查看】中点击【删除当前结果】。\n\n"
        "详细说明见 docs/用户操作手册.md 和 docs/评审交付与异机运行说明.md。",
        "Common workflow:\n"
        "1. In [Database Configuration], confirm the GeoEast root, data root, project, user, and executable.\n"
        "2. Click [Refresh Grid List] to read input grids from GeoEast.\n"
        "3. Select the input grid, output polygon name, result color, and required parameters.\n"
        "4. Click [Start Trace]. The input grid and result polygons are displayed when processing finishes.\n"
        "5. For viewing in GeoEast, open Basemap or GeoMapping and enable Grids and Fault Polygons in the layer tree.\n"
        "6. To remove an old result, use [Delete Current Result] under [Result View].\n\n"
        "See docs/User Manual and the deployment guide for complete instructions.");
    add("GeoEast 程序不存在", "GeoEast Application Not Found");
    add("未找到 %1：\n%2", "%1 was not found:\n%2");
    add("启动失败", "Launch Failed");
    add("无法启动 %1。请确认 GeoEast 环境和 license 可用。",
        "Cannot start %1. Confirm that the GeoEast environment and license are available.");
    add("项目: %1", "Project: %1");
    add("网格: %1", "Grid: %1");
    add("数据库: %1", "Database: %1");
    add("请稍候；完成后视图区会自动更新，无需重复点击。",
        "Please wait. The view updates automatically when loading finishes; no repeated click is needed.");
    add("数据源已切换，请选择当前数据源中的输入网格",
        "The data source changed. Select an input grid from the current source.");
    add("检测到旧版分位数设置 %1，已调整到当前安全范围 %2～%3。",
        "Legacy percentile setting %1 was adjusted to the current safe range %2-%3.");
    add("检测到旧版最小面积设置 %1，已调整为 %2，避免保留全部微小碎片。",
        "Legacy minimum-area setting %1 was adjusted to %2 to avoid retaining every tiny fragment.");
    add("根系防黏连、局部桥分裂和弱响应补追不能同时启用。三者处理目标不同，叠加会显著放大计算量和结果敏感性；请保留推荐的根系防黏连，并只选择一项按需功能进行对比。",
        "Root anti-merge, local bridge splitting, and weak-response retracing cannot be enabled together. Their goals differ, and stacking them greatly increases computation and result sensitivity. Keep the recommended root anti-merge and compare only one optional function at a time.");
    add("局部桥分裂会把低属性短桥两侧重新分组，分裂后的小对象仍受最小面积过滤，因此结果数量可能减少。仅在确认存在局部黏连时使用。",
        "Local bridge splitting regroups objects around low-attribute short bridges. Small split objects still pass through the minimum-area filter, so the result count may decrease. Use it only for confirmed local merging.");
    add("弱响应补追会额外检查主结果之外的细长弱响应，可能增加候选数量和处理时间。仅在主结果确有弱响应漏失时使用。",
        "Weak-response retracing examines slender weak responses outside the main result and may increase candidate count and processing time. Use it only when the main result misses weak responses.");

    return map;
}

const QMap<QString, QString>& englishTextMap() {
    static const QMap<QString, QString> map = buildEnglishTextMap();
    return map;
}

const QHash<QString, QString>& chineseTextByEnglish() {
    static const QHash<QString, QString> reverse = []() {
        QHash<QString, QString> result;
        const QMap<QString, QString>& source = englishTextMap();
        result.reserve(source.size());
        for (auto it = source.constBegin(); it != source.constEnd(); ++it) {
            // Keep the same first-match behavior as the former ordered linear scan.
            if (!result.contains(it.value())) {
                result.insert(it.value(), it.key());
            }
        }
        return result;
    }();
    return reverse;
}

QString translateTemplate(const QString& text, bool reverse) {
    const QMap<QString, QString>& map = englishTextMap();
    const QRegularExpression placeholderExpression(QStringLiteral("%([1-9])"));
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        const QString source = reverse ? it.value() : it.key();
        const QString target = reverse ? it.key() : it.value();
        if (!source.contains(placeholderExpression)) {
            continue;
        }

        QString pattern;
        QVector<int> placeholderIndexes;
        int previousEnd = 0;
        QRegularExpressionMatchIterator matches = placeholderExpression.globalMatch(source);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            pattern += QRegularExpression::escape(source.mid(previousEnd, match.capturedStart() - previousEnd));
            pattern += QStringLiteral("(.*?)");
            placeholderIndexes.push_back(match.captured(1).toInt());
            previousEnd = match.capturedEnd();
        }
        pattern += QRegularExpression::escape(source.mid(previousEnd));

        const QRegularExpression expression(
            QStringLiteral("^") + pattern + QStringLiteral("$"),
            QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch result = expression.match(text);
        if (!result.hasMatch()) {
            continue;
        }

        QString translated = target;
        for (int capture = 0; capture < placeholderIndexes.size(); ++capture) {
            translated.replace(QStringLiteral("%") + QString::number(placeholderIndexes.at(capture)),
                               result.captured(capture + 1));
        }
        return translated;
    }
    return text;
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
    return it == englishTextMap().constEnd() ? translateTemplate(key, false) : it.value();
}

QString canonical(const QString& text) {
    const QMap<QString, QString>& map = englishTextMap();
    // Most UI strings are created in Chinese. Avoid scanning every English value
    // for the overwhelmingly common case, then use a cached reverse index.
    if (map.contains(text)) {
        return text;
    }
    const auto reverseIt = chineseTextByEnglish().constFind(text);
    if (reverseIt != chineseTextByEnglish().constEnd()) {
        return reverseIt.value();
    }
    return translateTemplate(text, true);
}

void captureTranslatableText(QObject* root) {
    if (!root) {
        return;
    }
    QList<QObject*> objects = root->findChildren<QObject*>();
    objects.prepend(root);
    for (QObject* object : objects) {
        if (QAction* action = qobject_cast<QAction*>(object)) {
            rememberText(action, "fptTextZh", action->text());
            rememberText(action, "fptIconTextZh", action->iconText());
            rememberText(action, "fptToolTipZh", action->toolTip());
        }
        if (QMenu* menu = qobject_cast<QMenu*>(object)) {
            rememberText(menu, "fptTitleZh", menu->title());
        }
        if (QLabel* label = qobject_cast<QLabel*>(object)) {
            rememberText(label, "fptTextZh", label->text());
        } else if (QToolButton* button = qobject_cast<QToolButton*>(object)) {
            if (!button->defaultAction()) {
                rememberText(button, "fptTextZh", button->text());
            }
        } else if (QPushButton* button = qobject_cast<QPushButton*>(object)) {
            rememberText(button, "fptTextZh", button->text());
        } else if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(object)) {
            rememberText(checkBox, "fptTextZh", checkBox->text());
        } else if (QAbstractButton* button = qobject_cast<QAbstractButton*>(object)) {
            rememberText(button, "fptTextZh", button->text());
        } else if (QGroupBox* groupBox = qobject_cast<QGroupBox*>(object)) {
            rememberText(groupBox, "fptTitleZh", groupBox->title());
        }
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(object)) {
            rememberText(lineEdit, "fptPlaceholderZh", lineEdit->placeholderText());
        }
        if (QSpinBox* spinBox = qobject_cast<QSpinBox*>(object)) {
            rememberText(spinBox, "fptSuffixZh", spinBox->suffix());
        } else if (QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(object)) {
            rememberText(spinBox, "fptSuffixZh", spinBox->suffix());
        }
        if (QWidget* widget = qobject_cast<QWidget*>(object)) {
            rememberText(widget, "fptWindowTitleZh", widget->windowTitle());
            rememberText(widget, "fptToolTipZh", widget->toolTip());
            rememberText(widget, "fptAccessibleNameZh", widget->accessibleName());
            rememberText(widget, "fptAccessibleDescriptionZh", widget->accessibleDescription());
        }
    }
}

void applyTranslatableText(QObject* root, const QString& languageCode) {
    if (!root) {
        return;
    }
    QList<QObject*> objects = root->findChildren<QObject*>();
    objects.prepend(root);
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
            if (action->property("fptIconTextZh").isValid()) {
                const QVariant englishIconText = action->property("fptIconTextEn");
                action->setIconText(isEnglish(languageCode) && englishIconText.isValid()
                                        ? englishIconText.toString()
                                        : translated(action, "fptIconTextZh"));
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
        } else if (QToolButton* button = qobject_cast<QToolButton*>(object)) {
            if (!button->defaultAction() && button->property("fptTextZh").isValid()) {
                button->setText(translated(button, "fptTextZh"));
            }
        } else if (QPushButton* button = qobject_cast<QPushButton*>(object)) {
            if (button->property("fptTextZh").isValid()) {
                button->setText(translated(button, "fptTextZh"));
            }
        } else if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(object)) {
            if (checkBox->property("fptTextZh").isValid()) {
                checkBox->setText(translated(checkBox, "fptTextZh"));
            }
        } else if (QAbstractButton* button = qobject_cast<QAbstractButton*>(object)) {
            if (button->property("fptTextZh").isValid()) {
                button->setText(translated(button, "fptTextZh"));
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
        if (QSpinBox* spinBox = qobject_cast<QSpinBox*>(object)) {
            if (spinBox->property("fptSuffixZh").isValid()) {
                spinBox->setSuffix(translated(spinBox, "fptSuffixZh"));
            }
        } else if (QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(object)) {
            if (spinBox->property("fptSuffixZh").isValid()) {
                spinBox->setSuffix(translated(spinBox, "fptSuffixZh"));
            }
        }
        if (QWidget* widget = qobject_cast<QWidget*>(object)) {
            if (widget->property("fptWindowTitleZh").isValid()) {
                widget->setWindowTitle(translated(widget, "fptWindowTitleZh"));
            }
            if (widget->property("fptToolTipZh").isValid()) {
                widget->setToolTip(translated(widget, "fptToolTipZh"));
            }
            if (widget->property("fptAccessibleNameZh").isValid()) {
                widget->setAccessibleName(translated(widget, "fptAccessibleNameZh"));
            }
            if (widget->property("fptAccessibleDescriptionZh").isValid()) {
                widget->setAccessibleDescription(translated(widget, "fptAccessibleDescriptionZh"));
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
