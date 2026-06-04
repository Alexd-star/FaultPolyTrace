# 独立软件与 GeoEast 集成控件解耦说明

## 1. 解耦背景

FaultPolyTrace 同时面对两个不同使用场景。第一个场景是独立桌面软件：评审、演示人员或普通用户在没有 GeoEast 的电脑上打开程序，选择外部网格文件，运行断层多边形追踪，并查看原始网格、结果多边形和叠加结果。第二个场景是 GeoEast 集成控件：解释人员已经在 GeoEast 工区中工作，需要读取 BOMapGrid，写回 BOFaultPolygon，并在 GeoEast 底图或平面成图中查看结果。

这两个场景共用同一套算法核心，但不应该共用同一个界面外壳。若把“独立文件处理”和“GeoEast 数据库读写”强行平铺在一个窗口，用户会同时看到数据库、Runner、外部文件、历史记录、预览图、项目配置等大量控件，界面会变得拥挤，也容易误用。当前独立软件采用“数据源切换”的方式处理该矛盾：默认显示外部文件流程，只有用户主动选择【GeoEast数据库】时才显示数据库配置；GeoEast 菜单入口仍使用专门的集成控件。

因此当前版本采用“一个算法核心，两套清晰外壳”的架构。

## 2. 程序边界

| 程序 | 源码目录 | 发布包位置 | 面向用户 | 职责 |
|---|---|---|---|---|
| 独立桌面软件 `FaultPolyTrace.exe` | `standalone_app/qt_desktop` | 发布包根目录 | 评审、普通用户、演示人员；可选支持有 GeoEast 环境的用户 | 打开外部网格文件、运行算法、预览原始图/结果图/叠加图、导出 CSV/GeoJSON/PNG/诊断文件、保存历史和参数模板；可选连接 GeoEast 数据库读写 BOMapGrid/BOFaultPolygon |
| GeoEast 集成控件 `faultpolytrace_launcher.exe` | `geoeast_plugin/qt_launcher` | `qt_launcher/`，部署后也可位于 `GeoEast-RC/bin/FaultPolyTraceLauncher/` | GeoEast 平台用户 | 自动配置 GeoEast 环境、选择 BOMapGrid、设置输出 BOFaultPolygon、调用 runner 写回数据库 |
| 命令行算法核心 `faultpolytrace.exe` | `src/` | `bin/` | 自动化验收、脚本批处理、两套 GUI 外壳 | 读取外部网格，执行断层多边形追踪，输出 CSV、诊断、拓扑和分支文件 |
| GeoEast Runner `faultpolytrace_geoeast_runner.exe` | `geoeast_plugin/bo_runner` | `geoeast_plugin/bo_runner/` | GeoEast 集成控件 | 连接 GeoEast 数据库，读取 BOMapGrid，调用算法并写回 BOFaultPolygon |

## 3. 独立软件应保留的内容

独立软件默认保留外部文件全流程所需功能：

1. 选择输入文件，支持矩阵文本、GeoEast DAT 五列采样数据和自动识别。
2. 选择输出目录和输出名称。
3. 设置显示颜色、线宽、透明度。
4. 设置必要算法参数和预设模板。
5. 运行算法并在状态栏显示耗时，例如 `300 ms (0.300 s)`。
6. 查看原始网格、结果多边形和叠加结果。
7. 导出 PNG、CSV、GeoJSON、诊断 JSON、拓扑 CSV 和枝干 CSV。
8. 保存和打开参数模板。
9. 保存历史记录并允许回看历史任务。
10. 关闭后记住上次配置。

独立软件在默认外部文件流程中不显示 GeoEast 根目录、数据库、项目、用户、密码、BOMapGrid 列表、Runner 路径等字段。这些字段对没有 GeoEast 的用户没有意义，平铺显示只会增加理解成本。当用户选择【GeoEast数据库】数据源时，软件会显示这些配置项，并通过 GeoEast Runner 完成读库、写库和删除结果对象。

## 4. GeoEast 集成控件应保留的内容

GeoEast 集成控件只保留平台集成所需功能：

1. 自动识别或填写 GeoEast 根目录、数据根目录、数据库、项目和用户。
2. 刷新 GeoEast 数据库中的 BOMapGrid 列表。
3. 选择输入网格和输出多边形名称。
4. 设置结果样式和必要算法参数。
5. 调用 GeoEast Runner 完成数据库读取和写回。
6. 显示运行日志、耗时和 GeoEast 内查看结果的操作提示。

GeoEast 集成控件不承担独立文件图像浏览器职责。需要对外部文件进行完整预览和历史管理时，应使用发布包根目录的 `FaultPolyTrace.exe`。

## 5. 发布包结构

推荐发布包结构如下：

```text
FaultPolyTrace_<版本号>/
  FaultPolyTrace.exe
  01_打开图形界面.cmd
  02_运行离线演示.cmd
  03_查看命令行帮助.cmd
  bin/
    faultpolytrace.exe
  qt_launcher/
    faultpolytrace_launcher.exe
    qt.conf
    Qt*.dll
    platforms/
  standalone_app/
    qt_desktop/
      FaultPolyTraceDesktop.pro
      FaultPolyTraceWindow.cpp
      FaultPolyTraceWindow.h
  geoeast_plugin/
    bo_runner/
    mainframe_plugin/
  docs/
  examples/
  scripts/
  figure/
```

根目录 `FaultPolyTrace.exe` 是独立桌面软件。`qt_launcher/faultpolytrace_launcher.exe` 是 GeoEast 集成控件。两者名称、位置和职责必须保持清楚，不能互相覆盖。

## 6. 构建与验证要求

每次修改界面后，应至少执行以下验证：

```powershell
.\scripts\build.ps1
.\scripts\build_desktop_app.ps1
.\scripts\build_qt_launcher.ps1
.\scripts\run_smoke_test.ps1
python .\scripts\泛化鲁棒性验证.py
```

发布前还应执行：

```powershell
.\scripts\打包发布包.ps1 -Version <版本号>
```

验收重点不是“能不能编译”这么简单，而是确认：

1. 发布包根目录 `FaultPolyTrace.exe` 打开的是独立软件。
2. `01_打开图形界面.cmd` 打开的也是独立软件。
3. `qt_launcher/faultpolytrace_launcher.exe` 打开的是 GeoEast 集成控件。
4. 独立软件界面中没有 GeoEast 数据库字段。
5. GeoEast 集成控件中没有独立软件历史记录、外部图像浏览器等非必要内容。
6. 两套 GUI 都调用同一套算法核心，算法参数和质量指标不降级。

## 7. 后续维护原则

后续开发时应遵守以下原则：

1. 独立软件的新功能只改 `standalone_app/qt_desktop`。
2. GeoEast 集成控件的新功能只改 `geoeast_plugin/qt_launcher`。
3. 算法质量、参数、性能、拓扑输出等核心能力只改 `src/` 和相关脚本。
4. 发布包脚本负责把两个 GUI 外壳和一个算法核心组合到同一个交付包中。
5. 不再通过一个“运行模式”下拉框把两类界面强行塞在一个窗口里。
