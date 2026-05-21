# FaultPolyTrace Qt 启动器

这个 Qt Widgets 启动器是面向比赛展示和实际使用的参数界面。

它不重复实现算法，而是根据界面中选择的 GeoEast 数据库、项目、输入网格、输出多边形名和追踪参数，启动：

```text
faultpolytrace_geoeast_runner.exe
```

然后把 runner 的日志实时显示在界面中。

## 默认演示设置

```text
数据库: ndp
项目: BGPCup2026_Test7
用户: 26312
输入网格: test7_fault_attribute_bo
输出多边形: FaultPolyTrace_Result
```

启动器也可以在运行前把官方 `test.dat` 导入或覆盖为正确的 `BOMapGrid`。

## 配置方式

- 启动器会在可执行文件旁边保存 `faultpolytrace_launcher.ini`。
- `faultpolytrace_launcher.example.ini` 是配置示例。
- 初次创建 ini 前，可以用环境变量覆盖默认值，例如：
  - `FPT_GEOEAST_ROOT`
  - `FPT_GEOEAST_DATA_ROOT`
  - `FPT_GEOEAST_PROJECT`
  - `FPT_GEOEAST_USER`
  - `FPT_GEOEAST_GRID`
  - `FPT_OFFICIAL_DAT`

本机默认值只是为了方便演示；runner 本身仍然支持命令行和环境变量驱动。

## 构建

从项目根目录运行：

```powershell
.\scripts\build_qt_launcher.ps1
```

## 运行

从项目根目录运行：

```powershell
.\geoeast_plugin\qt_launcher\faultpolytrace_launcher.exe
```

如果把启动器复制到新机器，需要部署一次 Qt 运行库：

```powershell
& "D:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin\windeployqt.exe" .\geoeast_plugin\qt_launcher\faultpolytrace_launcher.exe
```

## 比赛演示流程

1. 点击 `刷新网格`，列出当前 GeoEast 项目里的 `BOMapGrid`。
2. 选择 `test7_fault_attribute_bo`，或勾选 `从官方 DAT 导入/覆盖输入网格` 重新生成它。
3. 设置 `输出多边形`，例如 `FaultPolyTrace_Result`。
4. 检查 `最小面积`、阈值模式和 `ridge-belt` 参数。
5. 点击 `开始追踪`。
6. 在日志面板中查看网格统计、拓扑诊断、多边形数量和耗时。
