# GeoEast 使用指南

本文记录赛题 7 在 GeoEast 中的验证流程：导入或生成输入网格、运行 FaultPolyTrace、查看原始断层属性图、查看追踪后的断层多边形。

## 1. “集成到 GeoEast”是什么意思

本赛题中的 GeoEast 集成可以分三层：

| 层级 | 含义 | 当前状态 |
|---|---|---|
| 数据集成 | 程序读取 GeoEast `BOMapGrid`，写入 GeoEast `BOFaultPolygon` | 已完成并验证 |
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
3. 打开项目 `BGPCup2026_Test7`。
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

## 6. 从 PowerShell 运行

本机默认验证命令：

```powershell
.\scripts\run_geoeast_bgpcup2026_test7.ps1
```

带完整参数的可移植示例：

```powershell
.\scripts\run_geoeast_bgpcup2026_test7.ps1 `
  -GeoEastRoot "C:\Users\26312\GeoEast" `
  -GeoEastDataRoot "D:\BaiduNetdiskDownload\BGPCup2025\GeoEast-RC\GeoEastData.DataModel+SQLITE.20230728\GeoEastData" `
  -Database ndp `
  -Project BGPCup2026_Test7 `
  -User 26312 `
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
C:\Users\26312\Documents\BGPCup\FaultPolyTrace\geoeast_plugin\qt_launcher\faultpolytrace_launcher.exe
```

建议流程：

1. 确认 `GeoEast 根目录`。
2. 确认 `GeoEast 数据根目录`。
3. 确认 PG 主机和端口。
4. 填写数据库 `ndp`、项目 `BGPCup2026_Test7`、用户 `26312`。
5. 点击 `刷新网格`。
6. 选择 `test7_fault_attribute_bo`。
7. 设置输出名，例如 `FaultPolyTrace_Result`。
8. 默认使用 `ridge-belt`、`percentile=0.98`、`min area=200`。
9. 点击 `开始追踪`。
10. 保存或截图日志证据：网格尺寸、多边形数量、拓扑诊断、耗时。

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

比赛材料建议准备以下截图和日志：

- Basemap 中 `test7_fault_attribute_bo` 的原始属性图。
- Basemap 中 `FaultPolyTrace_Result` 叠加在属性图上的结果图。
- `GeoFaultPolygonBrowser` 中显示结果对象的截图。
- Qt 启动器运行日志截图。
- 命令行或 JSON 诊断结果：多边形数量、开放环数量、自相交数量、多边形相交数量、耗时。
- 说明官方 DAT 已转换为正确 `BOMapGrid`，避免 GeoEast ASCII 格式定义歧义。
