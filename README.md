# FaultPolyTrace

FaultPolyTrace 是“基于平面断层属性的断层多边形自动追踪”赛题项目。系统从二维断层属性网格中提取断层响应带，输出闭合断层多边形、面积和拓扑诊断，并同时提供独立桌面软件、命令行程序和 GeoEast 集成入口。

## 当前状态

- 当前发布版本：`v1.1.3`
- 统一构建标识：以发布包根目录 `构建与依赖清单.json` 为最终核对依据
- 官方数据网格：`1501 x 701`
- 正式结果：`212` 个候选多边形，面积与形态过滤后保留 `200` 个
- 强响应覆盖率：`99.855208%`
- 弱响应覆盖率：`97.683691%`
- 拓扑：开放环 `0`、内环 `0`、自相交 `0`、多边形异常相交 `0`
- 10次连续运行：总耗时中位数 `1.762 s`、P95 `1.815 s`、最大 `1.815 s`，核心追踪P95 `0.936 s`，结果 CSV SHA256 为 `EE7473CD04A3EA988166DD4502EEE70D4FB4A0D674135541A9B38490C3B91369`
- 自动化验证：Python `237 passed`；独立软件界面合同 `216/216`；GeoEast 插件界面合同 `135/135`；参数一致性 `480/480`；C++ 核心检查全部通过
- 稳定性验证：当前默认线通过MSVC AddressSanitizer；10次运行工作集趋势`+0.002 MB`、句柄趋势`0`，16类异常输入全部被安全拒绝。Application Verifier需在管理员终端完成本版本复验
- 交付审计：`68` 项通过、`0` 项失败、`0` 项待人工确认

GeoEast 现场已经完成菜单入口、自动环境检测、`BOMapGrid` 列表、数据库写入、结果回读、原始网格/结果图层/叠加显示和删除二次确认的真实操作取证。正式结果未在取证过程中删除。

独立软件和 GeoEast 插件共用版本化 JSON 参数模板，支持运行日志保存、结果摘要复制、常用快捷键和显示样式即时调整。GeoEast 插件的透明度只作用于当前视图，不改变 `BOFaultPolygon` 几何。

## 仓库入口

| 路径 | 作用 |
|---|---|
| `src/` | 核心算法、文件读写、诊断和共享参数模型。 |
| `standalone_app/qt_desktop/` | 独立桌面软件源码。 |
| `geoeast_plugin/qt_launcher/` | GeoEast Qt 交互入口。 |
| `geoeast_plugin/bo_runner/` | GeoEast `BOMapGrid` / `BOFaultPolygon` 数据库适配程序。 |
| `geoeast_plugin/mainframe_plugin/` | GeoEast 主框架菜单插件。 |
| `scripts/` | 构建、测试、打包、质量评估和验收脚本。 |
| `tests/`、`tests_cpp/` | Python 契约/回归测试与 C++ 核心测试。 |
| `packaging/windows/` | Inno Setup 安装器配置。 |
| `docs/` | 精简后的当前中文文档体系。 |
| `docs/作品说明书图件/` | 作品说明书流程图、算法图、架构图的参考索引、分类绘图代码、矢量图、高清 PNG 和灰度审阅结果。 |
| `examples/synthetic/` | 可提交、可复现的合成示例数据。 |
| `data/registry/` | 公开泛化数据来源、许可证和用途边界。 |

完整文件说明见 [PROJECT_INDEX.md](PROJECT_INDEX.md)，文档阅读路径见 [docs/文档目录.md](docs/文档目录.md)。

## 构建

Windows PowerShell：

```powershell
.\scripts\build.ps1
.\scripts\build_desktop_app.ps1 -GeoEastRoot <GeoEast根目录>
.\scripts\build_qt_launcher.ps1 -GeoEastRoot <GeoEast根目录>
.\scripts\build_geoeast_bo_runner.ps1 -GeoEastRoot <GeoEast根目录> -UseVisualStudioProject
.\scripts\build_geoeast_mainframe_plugin.ps1 -GeoEastRoot <GeoEast根目录>
```

核心 CLI 构建结果位于 `build/faultpolytrace.exe`。Qt 软件的可执行文件分别位于各自源码目录；正式分发应使用发布脚本生成完整运行目录，不能只复制裸 EXE。

## 测试

```powershell
python -m pytest -q
.\scripts\test_cpp_core.ps1
.\scripts\检查参数一致性.ps1
.\scripts\检查独立软件界面功能.ps1
.\scripts\检查GeoEast插件界面功能.ps1
.\scripts\检查客户机硬编码.ps1
```

官方数据不提交到仓库。使用时显式指定：

```powershell
.\scripts\一键总验收.ps1 `
  -OfficialDat <官方test.dat> `
  -SkipGeoEastBuild `
  -SkipPackage
```

## 发布

```powershell
.\scripts\打包发布包.ps1 `
  -Version v1.1.3 `
  -GeoEastRoot <GeoEast根目录>

.\scripts\构建Windows安装器.ps1 `
  -PackageDir .\release\FaultPolyTrace_v1.1.3 `
  -Version 1.1.3
```

发布目录、ZIP、安装器和验收输出均为可重建产物，不进入 Git 主分支。构建脚本会生成源码快照哈希、二进制哈希和统一构建清单。

## 算法边界

正式默认线使用确定性的 Ridge-Belt 追踪、属性约束包络、骨架分支分析、根系防黏连、支撑包络分离、作用域弱桥、面积过滤、简单环兼容、边界简化/平滑和拓扑检查。支撑包络分离仅移除物理支撑半径之外的低值背景，且要求恰好得到两个具有线性形态的有效部分；扩展距离和邻域弱支撑只允许在已提交拆分的局部范围内使用。简单环兼容填补不会合并对象的小孔，并在无属性支撑的最窄位置打开较大背景环，使独立软件几何与GeoEast外环显示一致。多尺度分支归属、弱路径门控和局部多边形修形仍属于诊断或受控实验能力，默认关闭。

当前覆盖率是对属性阈值响应像元的覆盖评价，并不等同于相对专家真值的 Recall。没有专家标准答案时，应结合合成真值、对象拆分/合并、边界指标、参数扰动稳定性、全图分块复核和多解释者局部标注评价结果。

## 泛化验证

项目提供带精确对象真值的八类平面形态生成器、真实地震体到平面相似性属性的转换器、公开数据下载校验器，以及像元、边界、对象、中心线、拓扑和扰动稳定性指标。当前 80 个平面案例实测 IoU `0.849627`、Boundary F1 `0.996784`、零漏对象和零非法拓扑；独立交叉形态仍有 10 次对象误合并，已明确列为待提升项。

```powershell
python -m pip install -r requirements-validation.txt
.\scripts\运行完整泛化验证.ps1
```

公开真实剖面测试属于跨域压力测试，不等同于平面赛题精度。Thebe 四个真实平面条带也已完成实测，并暴露出简化相似性属性与赛题断层属性之间的明显输入域差异；该负向证据被保留，未用于针对性改写正式算法。数据来源、复现命令和完整证据边界见 [泛化验证与公开数据指南](docs/18_泛化验证与公开数据指南.md)。

新增8个与官方同尺寸的复杂同域案例，每例包含26个独立对象，并按官方整体稀疏度校准属性分布。正式基线在该组数据上取得 IoU `0.665353`、Precision `0.935923`、Recall `0.698300`、Boundary F1 `0.925000`，漏对象和无效多边形均为0；对象级评价同时发现高属性交切网络仍存在明显误合并。完整结果见[复杂同域泛化与算法可用性验证](docs/21_复杂同域泛化与算法可用性验证.md)。

## 重要文档

- [项目文件索引](PROJECT_INDEX.md)
- [用户手册](docs/02_用户手册.md)
- [开发者文档](docs/04_开发者文档.md)
- [算法说明](docs/08_算法说明.md)
- [测试说明](docs/13_测试说明.md)
- [当前验收报告](docs/官方数据最新验收报告.md)
- [需求追踪矩阵](docs/需求追踪矩阵.md)
- [交付说明](docs/17_项目交付说明.md)
- [泛化验证与公开数据指南](docs/18_泛化验证与公开数据指南.md)
- [复杂同域泛化与算法可用性验证](docs/21_复杂同域泛化与算法可用性验证.md)
- [物理拓扑与弱连续性升级实施结果](docs/22_物理拓扑与弱连续性升级开发方案.md)
- [作品说明书图件清单](docs/作品说明书图件/05_作品说明书图件清单.md)

## 安全与提交规则

- 不提交数据库密码、许可证、个人路径、本地 INI、构建目录、发布包或测试输出。
- 不针对固定坐标、固定数据集名称或固定 polygon ID 编写算法规则。
- 不把实验候选结果表述为正式 GeoEast 默认结果。
- 修改算法、参数、数据库适配或 UI 后，必须重新运行对应自动门禁。
