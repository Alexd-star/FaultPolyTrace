# FaultPolyTrace 发布包快速入口

本发布包面向评审、演示和异机复核。解压后请优先使用根目录中的 `FaultPolyTrace.exe` 或三个入口脚本，不建议直接双击 `bin/faultpolytrace.exe`。

## 1. 推荐打开顺序

1. 双击 `FaultPolyTrace.exe` 或 `01_打开图形界面.cmd`，打开 FaultPolyTrace 独立桌面软件。
2. 如果当前电脑没有 GeoEast 环境，双击 `02_运行离线演示.cmd`，验证算法模块可以独立处理外部网格文件。
3. 如需接入 GeoEast，请在 GeoEast 菜单入口中使用 `qt_launcher\faultpolytrace_launcher.exe`，不要把独立软件当作数据库控件使用。
4. 如需查看命令行参数，双击 `03_查看命令行帮助.cmd`。

## 2. 各入口的定位

| 入口 | 用途 | 是否需要 GeoEast |
|---|---|---|
| `FaultPolyTrace.exe` | 独立桌面软件，读取外部网格文件、显示原始图/结果图/叠加图、导出文件结果 | 不需要 |
| `01_打开图形界面.cmd` | 打开独立桌面软件，适合评审和普通用户双击 | 不需要 |
| `02_运行离线演示.cmd` | 使用发布包自带合成网格跑一遍算法，证明独立运行能力 | 不需要 |
| `03_查看命令行帮助.cmd` | 查看 `faultpolytrace.exe` 的完整参数 | 不需要 |
| `bin/faultpolytrace.exe` | 命令行算法核心，适合批处理和自动验收 | 不需要 |
| `qt_launcher/faultpolytrace_launcher.exe` | GeoEast 集成控件，选择 BOMapGrid 并写回 BOFaultPolygon | 需要 |
| `geoeast_plugin/bo_runner/faultpolytrace_geoeast_runner.exe` | GeoEast 数据库读写 Runner | 需要 |

## 3. 常见问题

### 3.1 双击 `faultpolytrace.exe` 为什么不直接计算？

`faultpolytrace.exe` 是命令行算法核心，需要输入网格路径和输出路径。没有参数时它只显示说明，不会开始计算。评审或演示时请使用 `FaultPolyTrace.exe`、`01_打开图形界面.cmd` 或 `02_运行离线演示.cmd`。

### 3.2 提示缺少 Qt 或“无法定位程序输入点”怎么办？

发布包根目录已经包含与 `FaultPolyTrace.exe` 匹配的 Qt 运行库和 `qt.conf`。请从完整发布包根目录启动，不要只复制单个 exe。GeoEast 集成控件的运行库位于 `qt_launcher` 目录，也需要保持目录完整。如果仍然报错，通常是目录被混入了其它 Qt 版本的 `Qt5*.dll`，请重新解压发布包。

### 3.3 没有 GeoEast 能不能验证算法？

可以。运行 `02_运行离线演示.cmd` 会读取 `examples/synthetic/broken_ridge_fault_grid.txt`，并在当前用户“文档/FaultPolyTraceOutputs/offline_demo_时间戳”目录下生成断层多边形 CSV、拓扑明细、分支结果和诊断 JSON；文档目录不可写时自动改用当前用户临时目录。独立桌面软件也可以直接打开外部网格文件并导出 CSV、GeoJSON、诊断 JSON、拓扑 CSV 和 PNG 视图。

### 3.4 有 GeoEast 时怎么验证集成？

先确认 GeoEast 许可证、数据库、项目和输入网格可用，再通过 GeoEast 菜单入口打开集成控件。集成控件程序是 `qt_launcher\faultpolytrace_launcher.exe`，它只负责 GeoEast 数据库读写，不承担独立文件浏览软件的职责。详细步骤见 `docs/用户操作手册.md` 和 `docs/GeoEast使用指南.md`。
