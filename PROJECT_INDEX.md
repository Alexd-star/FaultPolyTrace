# FaultPolyTrace 项目文件索引

本文描述主分支应保留的文件结构。历史实验、渲染中间物、发布包和本机配置不属于主分支内容。

## 1. 根目录

| 文件 | 作用 |
|---|---|
| `README.md` | 项目入口、当前指标、构建和测试命令。 |
| `AI_PROJECT_CONTEXT.md` | 新 AI 接手时的唯一完整上下文。 |
| `PROJECT_INDEX.md` | 文件与目录索引。 |
| `CHANGELOG.md` | 当前版本变更记录。 |
| `PRODUCT.md` | 产品定位和界面原则。 |
| `.gitignore` | 排除构建、发布、测试输出、个人配置和本地归档。 |
| `pytest.ini` | Python 测试配置。 |

## 2. 核心源码

| 路径 | 主要职责 |
|---|---|
| `src/app/main.cpp` | CLI 参数解析、执行入口和错误处理。 |
| `src/core/FaultTracer.*` | 分割、骨架、分支连接、断层带重建、防黏连、多边形生成。 |
| `src/core/BranchAttribution.*` | 多尺度分支归属诊断和候选分组。 |
| `src/core/WeakPathSupport.*` | 弱响应路径级支撑评价。 |
| `src/core/LocalPolygonQuality.*` | 局部形态质量诊断与受控修形。 |
| `src/core/Topology.*` | 闭合、自交和多边形相交检查。 |
| `src/io/GridIO.*` | 外部网格和 GeoEast DAT 读取。 |
| `src/io/PolygonIO.*` | CSV/WKT、GeoJSON 等结果输出。 |
| `src/io/DiagnosticsIO.*` | 参数、耗时、覆盖和诊断 JSON 输出。 |
| `src/ui/TraceParameterModel.*` | 独立软件和插件共享的预设与 CLI 参数序列化。 |
| `src/ui/UiLocalization.*` | 中英文界面文本。 |

## 3. 独立软件

`standalone_app/qt_desktop/` 是独立产品源码，负责外部文件、GeoEast 数据源配置、参数模板、历史结果、真实图层查看、颜色/线宽/透明度、日志和耗时。它不直接链接 GeoEast SDK，通过 BO Runner 与 GeoEast 数据库交互。

入口：`standalone_app/qt_desktop/main.cpp`。主窗口：`FaultPolyTraceWindow.cpp/.h`。

## 4. GeoEast 集成

| 目录 | 作用 |
|---|---|
| `geoeast_plugin/qt_launcher/` | GeoEast 场景下的交互界面和真实图层查看。 |
| `geoeast_plugin/bo_runner/` | 读取 `BOMapGrid`，写入、导出和删除 `BOFaultPolygon`。 |
| `geoeast_plugin/mainframe_plugin/` | 在 GeoEast 主框架中注册菜单入口。 |

BO Adapter 对含洞多边形采用显式拒绝策略，因为当前 SDK 的 `BAFaultPolygonSegment` 没有可靠的内环归属字段；写入后会重读核对对象数量和范围。

## 5. 测试与脚本

- `tests/`：CLI 契约、算法回归、参数一致性、泛化、安装器和文档边界测试。
- `tests_cpp/`：弱路径、分支归属、局部质量模块的 C++ 测试。
- `scripts/build*.ps1`：各目标构建脚本。
- `scripts/一键总验收.ps1`：官方数据综合验收。
- `scripts/打包发布包.ps1`：生成完整运行目录与 ZIP。
- `scripts/构建Windows安装器.ps1`：生成 Inno Setup 安装器。
- `scripts/TestWindowsInstaller.ps1`：真实安装、官方数据运行、哈希比对和卸载。
- `scripts/GenerateBuildManifest.ps1`：统一源码、二进制、ZIP 和安装器构建身份。

## 6. 数据与输出

- `examples/synthetic/`：可提交的最小演示数据。
- `examples/synthetic_generalization/`：泛化回归的合成场景。
- `examples/outputs/`：本地验收证据，默认忽略，不提交。
- 官方 `test.dat`：外部赛题数据，不提交，运行时通过参数指定。
- `release/`、`build/`、`outputs/`：可重建产物，不提交。

## 7. 文档

主文档采用 `docs/01_项目简介.md` 至 `docs/17_项目交付说明.md`。特殊主题只保留当前仍由代码或交付流程引用的文档。完整清单见 `docs/文档目录.md`。

## 8. 项目入口

```powershell
# CLI
.\scripts\build.ps1
.\build\faultpolytrace.exe --help

# 完整测试
python -m pytest -q
.\scripts\test_cpp_core.ps1

# 官方数据验收
.\scripts\一键总验收.ps1 -OfficialDat <test.dat> -SkipGeoEastBuild -SkipPackage
```

## 9. 不应进入 Git 的内容

构建产物、发布包、安装器、测试输出、渲染页图、第三方参考仓库、本地许可证、本地数据库配置、密码、个人绝对路径以及旧版本视觉包均不应进入主分支。历史材料保留在本地 `archive/` 中，不与当前文档并列。
