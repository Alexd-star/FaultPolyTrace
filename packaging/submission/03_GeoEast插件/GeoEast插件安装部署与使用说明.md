# FaultPolyTrace GeoEast 插件安装部署与使用说明

## 1. 两种 GeoEast 启动方式

本作品支持两种互不冲突的 GeoEast 启动方式。完整插件方式由主框架插件、集成界面和 BO 数据库执行程序组成：主框架插件负责在 GeoEast Ribbon 中注册“断层多边形追踪”入口，集成界面负责工区、数据库、输入网格、输出名称和追踪参数配置，BO Runner 负责读取 `BOMapGrid`、调用与独立软件相同的 C++ 核心算法，并将闭合结果写入 `BOFaultPolygon`。

比赛最终效果评测采用外部应用方式。对应运行包位于 `06_评测运行程序`，其根目录严格只有 `FaultPolyTrace.exe` 和 `pyalgo_faultpolytrace`。顶层 EXE 只用自身目录定位子目录中的 Launcher，不依赖当前工作目录，也不需要把 Qt DLL 散放在 GeoEast `bin` 根部。评测时不要求安装主框架 DLL。

## 2. 最终评测外部应用部署

评测包结构如下，目录名称不得改变：

```text
FaultPolyTrace.exe
pyalgo_faultpolytrace/
  faultpolytrace_launcher.exe
  faultpolytrace_geoeast_runner.exe
  Qt5*.dll
  platforms/qwindows.dll
  其他运行依赖
```

部署和测试步骤如下：

1. 先在任意新建测试目录中完整复制上述两项，双击 `FaultPolyTrace.exe`，确认界面能够独立打开。
2. 完全关闭 GeoEast，把上述两项完整复制到 `%GEOEAST%/bin`。不要复制整个代码提交包，也不要只复制顶层 EXE。
3. 在 GeoEast 的菜单定制或工具箱中登记应用程序 basename `FaultPolyTrace`，不填写开发机绝对路径。
4. 点击工具箱入口，确认打开的是同一 FaultPolyTrace 界面。
5. 进入数据库配置检查，刷新 `BOMapGrid`，选择“防黏连增强”预设和“正式处理”质量模式，运行后确认结果写入 `BOFaultPolygon`。

开发机做干净配置测试时，应临时设置一个新的 `FPT_CONFIG_ROOT`，避免已有用户配置中仍然存在的开发目录路径造成假通过。正式评测机没有旧配置时，程序会自动使用当前用户可写配置目录。

## 3. 完整 Ribbon 插件自动部署

部署前保存工区并完全关闭 GeoEast。普通用户优先双击 `安装GeoEast插件.cmd`；该入口会以适合中文路径的方式调用部署脚本，并在失败时保留错误提示。也可以在 PowerShell 中进入本目录后执行：

```powershell
.\安装GeoEast插件.ps1
```

脚本优先读取环境变量 `FPT_GEOEAST_ROOT`，随后检测当前用户目录、`C:\GeoEast`、`D:\GeoEast` 和 Program Files 中同时包含 `GeoEast-RC`、`NGP` 的目录。如果 GeoEast 位于其他位置，显式传入根目录：

```powershell
.\安装GeoEast插件.ps1 -GeoEastRoot "D:\GeoEast"
```

脚本只向指定 GeoEast 安装目录复制本插件文件，不修改数据库内容，不包含参赛者电脑用户名或固定盘符。检测到 GeoEast 正在运行时会停止部署，避免 DLL 被占用；覆盖已有插件前会把关键文件备份到当前用户本地数据目录，复制完成后再逐项核对 SHA256。若 GeoEast 安装目录受系统保护，脚本会提示以管理员身份运行，而不会留下含义不明的半成品。

## 4. 启动与自动配置

重新启动 GeoEast，在 Ribbon 中打开“断层多边形追踪—赛题7”。插件根据宿主路径、当前用户和工区环境自动检测 GeoEast 根目录、数据目录、执行程序和可用连接信息。若数据库连接仍缺少必要字段，点击数据库配置窗口中的帮助按钮，可查看各字段含义和检查位置；配置完成后点击测试或刷新，输入网格下拉列表应显示当前数据库中的可用 `BOMapGrid`。

## 5. GeoEast 标准流程

1. 在 GeoEast 中打开目标工区，再启动“断层多边形追踪”。
2. 检查顶部集成状态，必要时进入数据库配置并完成连接测试。
3. 点击【刷新网格列表】，从下拉框选择输入 `BOMapGrid`。
4. 填写输出断层多边形名称。名称应在当前工区中可识别，并避免覆盖仍需保留的正式结果。
5. 复现正式结果时选择“防黏连增强”预设和“正式处理”质量模式；其他数据可选择推荐参数或已保存模板，然后点击【开始追踪】。
6. 完成后检查状态栏中的核心处理耗时和总耗时，并确认“已写入 BOFaultPolygon”提示。
7. 点击【刷新结果列表】，选择目标结果；开启“原始网格”和“结果多边形”图层即可查看单层或叠加效果。
8. 在显示属性中修改线条颜色、线宽和透明度。显示修改只作用于当前查看效果，不改变数据库中的多边形坐标和属性值。
9. 不再需要的结果可在结果管理区选中后执行删除。删除前软件会要求确认，避免误删其他解释成果。

插件支持保存和打开版本化参数模板、保存运行日志、复制当前结果摘要以及恢复推荐参数。参数模板可与独立软件互相读取。`Ctrl+S` 保存参数模板，`Ctrl+Shift+O` 打开参数模板，`Ctrl+Shift+S` 保存运行日志，`Ctrl+Shift+P` 导出当前视图 PNG，`F5` 开始追踪，`Shift+F5` 停止追踪，`Home` 恢复适合窗口；当数据库配置、输入网格、输出名称或执行程序路径不完整时，界面会阻止不成立的操作并给出下一步提示。

## 6. 查看与显示逻辑

网格选择、结果选择和图层开关是相互独立的。选定网格并打开“原始网格”后，右侧显示所选网格；选定已有结果并打开“结果多边形”后，右侧显示所选结果；两个图层同时开启时形成叠加图。图层默认关闭，避免在数据尚未选定时造成结果已显示的误解。颜色、线宽和透明度在显示时即时生效，用户可以按照解释和制图需要调整线条属性。若列表为空或导出缓存失败，界面会提示缺少数据库配置、工区数据或读取权限，不会保持一个看似成功但内容未更新的旧画面。

## 7. 版本与环境要求

本包主框架插件使用比赛开发机 GeoEast SDK 与 Qt 5.15.2、MSVC 2019 x64 构建。GeoEast 插件属于宿主相关二进制，评审环境应与比赛提供版本一致。若 GeoEast 版本、编译器 ABI 或数据库 SDK 明显不同，应使用 `../01_源码` 中的插件源码在目标环境重新构建。独立软件不依赖 GeoEast，可在没有 GeoEast 的 Windows 10/11 x64 电脑上单独安装运行。

## 8. 完整插件方式的部署后文件位置

| 文件 | 目标位置 |
|---|---|
| `FaultPolyTraceMainframePlugin.dll`、`manifest.xml` | `GeoEast-RC/bin/plugins/GeoAttribute/FaultPolyTraceMainframePlugin/` |
| `faultpolytrace_launcher.exe` | `GeoEast-RC/bin/` |
| Launcher 完整运行目录 | `GeoEast-RC/bin/FaultPolyTraceLauncher/` |
| `faultpolytrace_geoeast_runner.exe` | `GeoEast-RC/bin/` 和 Launcher 目录 |


