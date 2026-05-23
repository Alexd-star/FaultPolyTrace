# FaultPolyTrace

FaultPolyTrace 是面向第十三届“东方杯”开发赛道赛题 7 的断层多边形自动追踪项目：

**基于平面断层属性的断层多边形自动追踪。**

项目目标是做成一个可运行、可集成、可展示、可复现的 GeoEast 工具，而不是单纯的离线算法脚本。

## 1. 项目结构

```text
docs/                 中文开发文档、操作手册、算法方案、需求追踪
src/core/             算法核心
src/io/               文件和 GeoEast 数据适配层
src/app/              离线 CLI 程序
scripts/              构建、运行、测试脚本
examples/             合成测试数据和输出结果
tests/                回归测试和性能门禁说明
geoeast_plugin/       GeoEast runner、Qt 启动器、主控插件
```

## 2. 当前能力

当前项目已经完成以下核心能力：

- 读取外部二维网格和官方 DAT 转换数据。
- 支持 `region` 和 `ridge-belt` 两种追踪模式。
- 自动生成闭合断层多边形。
- 计算面积、周长和属性统计。
- 按用户设定的面积阈值过滤小多边形。
- 输出 CSV 和诊断 JSON。
- 读取 GeoEast `BOMapGrid`。
- 写入 GeoEast `BOFaultPolygon`。
- 提供 Qt 启动器进行输入选择、输出命名和参数设置。

已验证赛题 7 GeoEast 数据：

```text
数据库: ndp
项目: BGPCup2026_Test7
用户: 26312
输入网格: test7_fault_attribute_bo
输出多边形: FaultPolyTrace_Result
```

## 3. 最快看到界面的方式

直接运行 Qt 启动器：

```powershell
.\geoeast_plugin\qt_launcher\faultpolytrace_launcher.exe
```

或先部署到 GeoEast 主程序可发现的位置：

```powershell
.\scripts\部署到GeoEast.ps1
```

部署后可在 GeoEast 主界面查找 `断层多边形追踪 -> 赛题7` 入口。

启动器界面用于：

- 设置 GeoEast 根目录和数据根目录。
- 设置数据库、项目、用户。
- 刷新并选择输入网格。
- 设置输出多边形对象名。
- 设置追踪模式、阈值、面积阈值等算法参数。
- 启动追踪并查看运行日志。

## 4. 一键运行官方赛题 7 数据

从项目根目录运行：

```powershell
.\scripts\run_geoeast_bgpcup2026_test7.ps1
```

默认会：

1. 设置 GeoEast 环境变量。
2. 必要时从官方 `test.dat` 导入或覆盖输入网格。
3. 读取 `test7_fault_attribute_bo`。
4. 使用 `ridge-belt` 模式追踪断层多边形。
5. 写入 `FaultPolyTrace_Result`。

如果不想重新导入 DAT，只从已有 GeoEast 网格运行：

```powershell
.\scripts\run_geoeast_bgpcup2026_test7.ps1 -NoImportDat
```

## 5. 重要中文文档

优先阅读：

- `docs/用户操作手册.md`：从打开 Qt 到查看 GeoEast 原图和结果图的用户操作手册。
- `docs/GeoEast使用指南.md`：GeoEast 数据查看和验证说明。
- `docs/开发指南.md`：开发指南。
- `docs/软件设计说明.md`：正式软件设计说明。
- `docs/代码编制说明.md`：源码组织、模块职责和构建运行说明。
- `docs/总体蓝图.md`：总体蓝图。
- `docs/需求追踪矩阵.md`：赛题要求完成情况追踪。
- `docs/Ridge-Belt断层多边形算法方案.md`：核心算法方案。
- `docs/算法原理说明.md`：面向答辩和评审的算法原理完整说明。
- `docs/顶尖质量差距分析与推进计划.md`：当前成熟度、问题清单、诊断方法和冲顶路线。
- `docs/赛题7官方数据运行质量报告.md`：当前推荐的官方数据运行质量报告。
- `docs/官方数据最新验收报告.md`：当前根系防黏连推荐方案的最新验收指标和残余复核层说明。
- `docs/面积保持型平滑优化复盘.md`：本轮边界保真优化、对照实验和验收结论。
- `docs/发布说明与验收清单.md`：当前发布包内容和评分项验收清单。

质量复核脚本：

```powershell
.\scripts\运行顶尖质量诊断.ps1
```

该脚本会输出强响应漏检、弱线性残余、疑似误合并多边形和逐多边形质量诊断表，用于后续把结果质量继续推到顶尖。

## 6. 当前下一步

比赛交付的下一阶段重点是：

1. 在 GeoEast 中补原始属性图、根系防黏连结果叠合图和结果对象列表截图。
2. 用最新一键验收目录 `acceptance_20260523_062319_official_root_antimerge` 作为答辩证据锚点。
3. 展示正式结果层与残余复核层的区别，说明弱残余被质控记录而不是被忽略。
4. 把中文文档、发布包、截图和演示视频整理为最终提交材料。

