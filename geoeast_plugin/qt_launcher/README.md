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

启动器还可以把当前项目中的最终 `BOFaultPolygon` 直接导出为 GeoEast 断层多边形 DAT。它会先回读数据库对象，再按 GeoEast 官方 `default_full.fmt` 八列定义生成 DAT，并在同一目录写出匹配的同名 `.fmt`。该功能与“输入网格 DAT”属于两个方向不同的入口：前者导出结果多边形，后者导入属性网格。

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
7. 点击【导出 GeoEast DAT】，按通知保存为 `lc_result.dat`；确认自动结构校验通过后，再在 GeoEast 中回导一次完成最终验收。

## GeoEast DAT 导出规则

- 数据来源：当前项目中所选的 `BOFaultPolygon`，每次导出都重新回读，避免使用陈旧预览缓存。
- DAT 字段：`faultPolygonName x y connectionFlag faultLevel faultPointType faultPolygonType closeType`。
- 分段方式：每个外环一个连续 `connectionFlag`，环末重复首点。
- 配套格式：软件同时生成同名 `.fmt`，内容与本机 GeoEast `default_full.fmt` 一致。回导时在格式定义页面点【打开】选择该文件。
- 当前语义：`faultLevel=0`、`faultPointType=0`（上盘）、`faultPolygonType=1`（正断层）、`closeType=1`（闭合）。
- 自动检查：表头、8 列、有限坐标、至少 3 个不同顶点、非零面积、闭合、分段连续、行数及 SHA256。
- 防假成功：导入后除查看主控列表数量外，还应回读 `BOFaultPolygon` 并确认有效多边形数量一致；只有列表数量而回读为 0 说明字段映射错误。
- 最终边界：自动检查证明文件符合本机官方格式定义；正式提交前仍应使用 GeoEast【Import Fault Polygons】回导并检查显示一致。
