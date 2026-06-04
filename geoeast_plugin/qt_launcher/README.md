# FaultPolyTrace Qt 启动器

这个 Qt Widgets 启动器是面向比赛展示和实际使用的参数界面。

它不重复实现算法，而是根据界面中选择的 GeoEast 数据库、项目、输入网格、输出多边形名和追踪参数，启动：

```text
faultpolytrace_geoeast_runner.exe
```

然后把 runner 的日志实时显示在界面中。

## 默认配置原则

启动器面向不同客户机和不同 GeoEast 工区，不再内置某台开发机的项目、用户名、输入网格或数据盘路径。首次启动时会按以下顺序自动识别环境：

1. 读取 `FPT_GEOEAST_ROOT`、`FPT_GEOEAST_DATA_ROOT`、`FPT_GEOEAST_PROJECT`、`FPT_GEOEAST_USER`、`FPT_GEOEAST_GRID` 等环境变量。
2. 根据启动器所在目录判断是否位于 `<GeoEast安装根目录>\GeoEast-RC\bin` 或其子目录。
3. 根据 `NGP`、`IECO`、`GEOEAST`、`PG_HOME` 等 GeoEast 运行环境变量推断根目录和数据目录。
4. 读取启动器旁边已保存的 `faultpolytrace_launcher.ini`。
5. 仍无法识别时保留为空，并在界面中提示用户手动选择或填写。

启动器也可以在运行前把用户选择的 DAT 文件导入或覆盖为指定 `BOMapGrid`。

## 配置方式

- 启动器会在可执行文件旁边保存 `faultpolytrace_launcher.ini`。
- `faultpolytrace_launcher.example.ini` 是配置示例。
- 初次创建 ini 前，可以用环境变量提供默认值，例如：
  - `FPT_GEOEAST_ROOT`
  - `FPT_GEOEAST_DATA_ROOT`
  - `FPT_GEOEAST_PROJECT`
  - `FPT_GEOEAST_USER`
  - `FPT_GEOEAST_GRID`
  - `FPT_OFFICIAL_DAT`

保存后的 ini 只记录当前客户机自己的配置。示例配置文件不包含开发机路径。

## 构建

从项目根目录运行：

```powershell
.\scripts\build_qt_launcher.ps1
```

构建脚本会优先使用 GeoEast 自带 Qt 5.15.2，并在编译完成后自动调用同一套 Qt 的 `windeployqt` 部署运行库。不要把 Qt 5.14.2、Qt 5.15.2 或其它来源的 `Qt5*.dll` 手工混放到启动器目录，否则双击时可能出现“无法定位程序输入点”一类入口错误。

## 运行

从项目根目录运行：

```powershell
.\geoeast_plugin\qt_launcher\faultpolytrace_launcher.exe
```

如果把启动器复制到新机器，需要用构建时同一套 Qt 部署运行库。本项目在 GeoEast 环境下推荐使用：

```powershell
& "<GeoEast安装根目录>\NGP\Qt\Qt5.15.2\5.15.2\msvc2019_64\bin\windeployqt.exe" --force .\geoeast_plugin\qt_launcher\faultpolytrace_launcher.exe
```

如果运行的是复制到 `<GeoEast安装根目录>\GeoEast-RC\bin` 下的副本，也要对那个副本执行同样的 `windeployqt`，或直接重新运行部署脚本，确保 exe 旁边的 `Qt5Core.dll`、`Qt5Gui.dll`、`Qt5Widgets.dll` 和 `platforms\qwindows.dll` 来自同一个 Qt 版本。

## 比赛演示流程

1. 点击 `刷新网格`，列出当前 GeoEast 项目里的 `BOMapGrid`。
2. 选择需要处理的输入网格，或勾选 `从 DAT 导入/覆盖输入网格` 重新生成它。
3. 设置 `输出多边形`，例如 `FaultPolyTrace_Result`。
4. 检查 `最小面积`、阈值模式和 `ridge-belt` 参数。
5. 点击 `开始追踪`。
6. 在日志面板中查看网格统计、拓扑诊断、多边形数量和耗时。
