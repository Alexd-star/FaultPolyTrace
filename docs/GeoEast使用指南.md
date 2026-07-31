# GeoEast 使用指南

本文记录赛题 7 在 GeoEast 中的验证流程：导入或生成输入网格、运行 FaultPolyTrace、查看原始断层属性图、查看追踪后的断层多边形。

## 1. “集成到 GeoEast”是什么意思

本赛题中的 GeoEast 集成可以分三层：

| 层级 | 含义 | 当前状态 |
|---|---|---|
| 数据集成 | 程序读取 GeoEast `BOMapGrid`，写入 GeoEast `BOFaultPolygon` | 工程已实现；最终提交需补有效 GeoEast 工区现场截图和叠加图证据 |
| 工作流集成 | 用户通过界面选择数据库、项目、输入、输出和参数 | 已通过 Qt 启动器完成 |
| 主控入口集成 | 在 GeoEast 主程序菜单或 Ribbon 中启动工具 | 已构建并部署，待有效 license 环境中界面点击截图 |

评分最核心的是读库、写库和结果可查看。主控入口已经具备工程交付形态，后续主要补 GeoEast 界面截图证据。

## 2. 不写死策略

程序不能依赖某一台电脑的固定用户名、磁盘路径或项目名。GeoEast runner 接受命令行参数：

- 数据库
- 项目
- 用户
- 密码
- 输入网格名
- 输出多边形名
- 所有算法参数

Qt 启动器和 PowerShell 脚本保留本机演示默认值，但关键值都可以通过界面、`.ini`、环境变量或脚本参数修改。

支持的环境变量包括：

```text
FPT_GEOEAST_ROOT
FPT_GEOEAST_DATA_ROOT
FPT_GEOEAST_DB
FPT_GEOEAST_PROJECT
FPT_GEOEAST_USER
FPT_GEOEAST_PASSWORD
FPT_GEOEAST_PG_HOST
FPT_GEOEAST_PG_PORT
FPT_GEOEAST_GRID
FPT_GEOEAST_OUTPUT
FPT_GEOEAST_RUNNER
FPT_OFFICIAL_DAT
```

这保证了本机可以一键演示，换到评审或队友电脑时也能改配置运行。

## 3. 正确输入网格

官方赛题 7 的 `test.dat` 是类似点表的断层属性文件，字段形态接近：

```text
Line CMP X Y Value
```

如果直接用 GeoEast 通用 ASCII 导入，格式定义稍有不对，就可能只导入少量有效点，导致后续追踪结果异常。当前验证中，建议使用程序转换并写入的网格对象：

```text
test7_fault_attribute_bo
```

不要把 `test` 或手工导入的 `test7_fault_attribute` 直接作为最终证据，除非先通过程序验证网格尺寸和非零像元数量正确。

## 4. 在 GeoEast 中查看原始属性图

推荐流程：

1. 启动 GeoEast。
2. 打开数据库 `ndp`。
3. 打开项目 `<目标项目名>`。
4. 打开 `Basemap`。
5. 使用 Basemap 的加载工具加载网格数据层。
6. 选择网格对象 `test7_fault_attribute_bo`。
7. 在显示管理树中激活该网格层。
8. 调整色标和显示范围，确认高值断层属性呈线状或带状分布。

GeoEast 帮助文档中的关键信息：

- `GeoBasemap` 支持网格数据层显示。
- 网格层会出现在 Basemap 显示管理树中。
- 色标范围跟随当前激活图层，如果色标不对，应先激活网格图层。

## 5. 在 GeoEast 中查看追踪结果

运行 FaultPolyTrace 后，结果通常写为：

```text
FaultPolyTrace_Result
```

查看方式一：主控数据树。

1. 在项目数据树中找到 `Combined Fault Polygons`。
2. 右键打开 `Properties...`。
3. 在 `GeoFaultPolygonBrowser` 中选择 `FaultPolyTrace_Result`。
4. 检查多边形段数量、顶点坐标和属性。

查看方式二：Basemap 叠合显示。

1. 打开 `Basemap`。
2. 加载 `test7_fault_attribute_bo` 网格层。
3. 再加载 fault polygon 数据。
4. 选择 `FaultPolyTrace_Result`。
5. 将多边形叠加到属性图上，检查追踪边界是否自然、闭合、贴合断层趋势。

GeoEast 帮助文档中的关键信息：

- `GeoFaultPolygonBrowser` 可浏览和编辑当前项目的组合断层多边形。
- 浏览器入口通常是 `Combined Fault Polygons` -> `Properties...`。
- 多边形顶点包含 X/Y 坐标和 wall type 信息。
- Basemap 支持断层多边形显示、图层顺序和样式调整。

### 5.1 官方数据语义核对

根据 `GeoBasemap.pdf`、`GeoFaultPolygonBrowser.pdf` 和 NGP SDK 的 `bofaultpolygon.h`：

- 一个 `BOFaultPolygon` 对象可包含多条 `BAFaultPolygonSegment`。
- 每条段具有独立段标识、组合标识、`closeFlag`、断层类型、级别和顶点列表。
- GeoEast 同时支持未闭合段与闭合段；本赛题输出为断层带外边界，因此 FaultPolyTrace 写出的正式段统一使用闭合标识。
- 两个未闭合段可通过端点连接；两个闭合段只有在相交时才由 GeoEast 的 `Link` 操作合并。自动追踪结果若需要补齐短缺口，应先依据属性和方向重建连续断层带，再重新生成闭合外边界，不能只在数据库层强行合并两个相离多边形。
- `Fill Closed Fault Polygon`、颜色、线型、线宽和平滑均为底图显示参数。调整它们不会补上真实缺失像元，也不会改变输出顶点；人工核验应同时查看原始属性与实际边界。
- GeoEast 数据结构没有表达“某外环的内环”的父子字段。当前适配器只写外边界，保留完整含洞几何到 CSV/WKT，避免把内环误写成独立断层对象。

当前 `GeoEastBoAdapter` 的正式写入设置 `closeFlag=1`，每个保留外边界写成一条独立段，并在事务式替换后回读外环数量、范围、外环面积、顶点数量和样式。外边界写入语义与官方说明书一致，但 `BOFaultPolygon` 不提供内环父子关系，含洞 WKT 不能完整往返；适配器会省略内环而不是把它误写成独立断层。后续弱连续性候选仍须先通过内环、局部包络和几何门控，不能绕过核心算法直接修改数据库段关系。

## 6. 从 PowerShell 运行

本机默认验证命令：

```powershell
.\scripts\run_geoeast_bgpcup2026_test7.ps1
```

带完整参数的可移植示例：

```powershell
.\scripts\run_geoeast_bgpcup2026_test7.ps1 `
  -GeoEastRoot "<GeoEast安装目录>" `
  -GeoEastDataRoot "<官方数据目录>\GeoEast-RC\GeoEastData.DataModel+SQLITE.20230728\GeoEastData" `
  -Database ndp `
  -Project <目标项目名> `
  -User <GeoEast用户名> `
  -Grid test7_fault_attribute_bo `
  -Output FaultPolyTrace_Result
```

如果只想从已有 `BOMapGrid` 运行，不想重新导入官方 DAT，可以加：

```powershell
-NoImportDat
```

## 7. 从 Qt 启动器运行

可执行文件：

```text
<项目根目录>\geoeast_plugin\qt_launcher\faultpolytrace_launcher.exe
```

建议流程：

1. 确认 `GeoEast 根目录`。
2. 确认 `GeoEast 数据根目录`。
3. 确认 PG 主机和端口。
4. 填写数据库 `ndp`、项目 `<目标项目名>`、用户 `<当前GeoEast用户>`。
5. 点击 `刷新网格`。
6. 选择 `test7_fault_attribute_bo`。
7. 设置输出名，例如 `FaultPolyTrace_Result`。
8. 默认使用 `ridge-belt`、`percentile=0.98`、`min area=120`，推荐预设为“防黏连增强”。
9. 点击 `开始追踪`。
10. 保存或截图日志证据：网格尺寸、多边形数量、拓扑诊断、耗时。
11. 在结果区点击【导出 GeoEast DAT】，保存为通知要求的 `lc_result.dat`。
12. 确认界面提示“已导出并通过结构校验”，再在 GeoEast 中使用【Import Fault Polygons】回导为临时对象并核对显示。

### 7.1 最终 DAT 的格式与验收

【导出 GeoEast DAT】不是把内部 CSV 修改扩展名。启动器会先调用 BO Runner 从当前项目的 `BOFaultPolygon` 回读最终外环，然后按本机 GeoEast 官方 `default_full.fmt` 输出八列，并在同一目录生成同名 `.fmt`。每条外环使用连续分段号，首点在末尾重复，`faultLevel/faultPointType/faultPolygonType/closeType` 固定为与 BO Writer 一致的 `0/0/1/1`。

GeoDataIO 会保留上一次导入任务的字段映射，因此仅选择 DAT 后直接导入可能出现“主控列表有对象和段数量，但内部没有有效点、平面成图为空”的假成功。正确流程是在格式定义页面点击【打开】，选择软件生成的同名 `.fmt`，确认数据体开始行是 `2`，且第 1—8 列依次映射为名称、X、Y、Connection Flag、Fault Level、Fault Point Type、Fault Polygon Type、Closing Type，再点击【导入】。

导出器会在原子替换目标文件前后分别检查表头、列数、有限坐标、不同顶点数、非零面积、闭合性、分段连续性和总记录数，并记录 SHA256。软件结构校验通过后，仍须在最终使用的 GeoEast 版本中回导一次。只有回导无字段解析错误、坐标和外环数量正常、临时对象与正式 `BOFaultPolygon` 显示一致，才算完成提交级验收。

## 8. 从 GeoEast 主控入口运行

先部署：

```powershell
.\scripts\部署到GeoEast.ps1
```

该脚本会构建并复制：

- `faultpolytrace_launcher.exe` 到 `GeoEast-RC\bin`
- `faultpolytrace_geoeast_runner.exe` 到 `GeoEast-RC\bin`
- `FaultPolyTraceMainframePlugin.dll` 和 `manifest.xml` 到 `GeoEast-RC\bin\plugins\GeoAttribute\FaultPolyTraceMainframePlugin`

然后重启 GeoEast，在主界面中查找：

```text
断层多边形追踪 -> 赛题7
```

点击后应打开 Qt 启动器。若入口不可见，先确认主控插件 DLL 和 `manifest.xml` 是否在上述插件目录，再检查 license 和 GeoEast 插件加载日志。

## 9. 最终证据清单

比赛材料建议准备以下截图、日志和录屏。为避免截图命名混乱，建议先生成现场证据包：

```powershell
.\scripts\检查GeoEast现场证据.ps1 -Initialize
```

脚本会在 `release\figure\geoeast_acceptance_<时间戳>` 下生成说明文件和占位清单。完成 GeoEast 现场操作后，将截图按固定文件名放入该目录，再运行：

```powershell
.\scripts\检查GeoEast现场证据.ps1 -EvidenceDir .\release\figure\geoeast_acceptance_<时间戳>
```

满分交付审计脚本会自动读取最新 `geoeast_acceptance_*` 目录。若以下截图齐全，GeoEast 现场项会从“待人工确认”转为“通过”。

必需截图：

| 文件名 | 内容 |
|---|---|
| `01_GeoEast菜单入口.png` | GeoEast 主界面中的“断层多边形追踪”菜单、按钮或 Ribbon 入口 |
| `02_启动器自动配置.png` | 从 GeoEast 入口打开后的启动器，数据库、项目、用户和 Runner 自动识别 |
| `03_输入网格列表.png` | 点击【刷新网格】后可选择 `test7_fault_attribute_bo` |
| `04_原始网格显示.png` | Basemap 或 GeoMapping 中的原始断层属性图 |
| `05_结果多边形图层.png` | `FaultPolyTrace_Result` 或用户命名结果图层单独显示 |
| `06_原始与结果叠加.png` | 原始网格与断层多边形叠加显示，颜色和线宽清晰 |
| `07_数据库对象管理.png` | `GeoFaultPolygonBrowser` 或对象管理器中可见结果对象 |
| `08_运行日志与耗时.png` | 启动器日志中可见多边形数量、拓扑诊断、耗时和写库结果 |

建议录屏：

| 文件名 | 内容 |
|---|---|
| `09_演示视频.mp4` | 1 到 2 分钟，覆盖 GeoEast 入口、选择网格、运行追踪、写库和叠加查看 |

截图和录屏的目的不是重复算法验收，而是证明“集成到 GeoEast 系统”的现场可用性。算法质量仍以官方 `test.dat` 一键验收、质量评估图、漏检质控和残余复核层为准。
