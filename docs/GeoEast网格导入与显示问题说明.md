# GeoEast 网格导入与显示问题说明

本文用于说明赛题 7 官方 `test.dat` 在 GeoEast 中手动导入后显示范围异常、对象列表刷新不及时，以及应如何核对和使用正确输入网格。

## 1. 结论

`test7_fault_attribute_bo` 是当前项目中用于赛题 7 处理和叠合显示的正确输入网格对象。

手动通过 GeoEast ASCII 网格输入模块导入 `test.dat` 后得到的 `test`、`testdata` 或其他同源对象，如果网格属性中出现 `X Minimum=800`、`Y Minimum=800`、`X GridNum=800`、`Y GridNum=800`、`Space=800` 等信息，说明导入格式解释错误，不应作为 FaultPolyTrace 的输入，也不应与 `FaultPolyTrace_Result_Check` 叠合评价。

正确输入网格应具有类似以下头信息：

| 属性 | 正确值 |
|---|---:|
| X Minimum | 622993.9 |
| Y Minimum | 4334420.1 |
| X GridNum | 1501 |
| Y GridNum | 701 |
| Space | 10 |

## 2. 为什么手动导入会错

GeoEast 帮助文档中 `Shared__GeoASCIIData.pdf.txt` 的网格输入模块说明指出，完整网格文件需要包含文件头和文件体两部分，并需要能确定以下网格属性：

| GeoEast 属性 | 含义 |
|---|---|
| `Minimum X` | 网格最小 X 坐标 |
| `Minimum Y` | 网格最小 Y 坐标 |
| `X Grid Spacing` | X 方向网格间距 |
| `Y Grid Spacing` | Y 方向网格间距 |
| `X Grid Number` | X 方向网格数 |
| `Y Grid Number` | Y 方向网格数 |
| `Grid Value` | 网格样值 |

赛题官方 `test.dat` 的实际开头如下：

```text
#            Line       CMP       X               Y                      Value
             800        300      622993.9000     4334420.1000           0.00
             800        301      622993.9000     4334430.1000           0.00
```

这个文件不是带完整 GeoEast 网格头段的标准网格文件，而是采样点列表。每一行的含义是：

| 列 | 含义 | 示例 |
|---|---|---:|
| 1 | 测线号 `Line` | 800 |
| 2 | 道号或 CMP 号 | 300 |
| 3 | 大地坐标 X | 622993.9000 |
| 4 | 大地坐标 Y | 4334420.1000 |
| 5 | 断层属性值 | 0.00 |

如果在 GeoEast 手动导入时没有为这种采样点列表配置正确的格式映射，GeoEast 可能会把第一列 `Line=800` 或第二列 `CMP=800/300` 当作网格坐标、网格数量或间距相关信息。这样生成的网格对象虽然能出现在数据管理器中，但它的地理范围已经错了。

## 3. 为什么错误对象会显示成巨大版面

断层多边形结果 `FaultPolyTrace_Result_Check` 使用真实大地坐标写入，范围大致位于：

```text
X: 约 623000 到 638000
Y: 约 4334420 到 4341420
```

如果原始网格被错误导入成 `X Minimum=800`、`Y Minimum=800`、`Space=800`，它的显示范围会与真实坐标系完全不匹配。此时在工区底图中叠加结果多边形，就会出现以下现象：

- 错误网格占据一个巨大画布。
- 正确多边形结果被压缩成很小的点或很小的局部线团。
- 原图与结果看起来无法对应。

这不是追踪算法输出坐标错了，而是手动导入的输入网格坐标头信息错了。

## 4. 为什么 `test7_fault_attribute_bo` 能正确显示

FaultPolyTrace 的 GeoEast BO 导入逻辑没有直接把 `test.dat` 当普通 ASCII 网格交给 GeoEast 解释，而是由程序按赛题 DAT 格式逐行解析：

1. 读取 `Line`、`CMP`、`X`、`Y`、`Value`。
2. 根据唯一 `Line` 数量得到 `nx=1501`。
3. 根据唯一 `CMP` 数量得到 `ny=701`。
4. 根据相邻 X/Y 坐标差得到 `dx=10`、`dy=10`。
5. 将采样点中心坐标转换为网格边界起点。
6. 写入 GeoEast `BOMapGrid` 的 `BAMapGridHead`。

相关代码位置：

- DAT 解析：[AsciiGridIO.cpp](C:/Users/26312/Documents/BGPCup/FaultPolyTrace/src/io/AsciiGridIO.cpp)
- GeoEast 网格写入：[GeoEastBoAdapter.cpp](C:/Users/26312/Documents/BGPCup/FaultPolyTrace/geoeast_plugin/bo_runner/GeoEastBoAdapter.cpp)

程序写入的关键头字段包括：

```text
head.nx = grid.nx()
head.ny = grid.ny()
head.sx = grid.sx()
head.sy = grid.sy()
head.dx = grid.dx()
head.dy = grid.dy()
```

所以 `test7_fault_attribute_bo` 与 `FaultPolyTrace_Result_Check` 使用同一套真实坐标体系，可以在工区底图中正确叠合。

## 5. 为什么手动导入后主界面列表没有立刻显示

GeoEast 的数据浏览、数据集管理和底图图层列表存在缓存和当前列表状态。手动导入后对象已写入数据库，但左侧数据树或主界面列表可能没有立即刷新。

可按以下顺序处理：

1. 在左侧数据树中右击 `网格`，执行刷新或重新展开。
2. 打开 `网格数据集管理` 查看是否已存在该对象。
3. 在平面成图中执行 `Refresh` 或 `Refresh All`。
4. 关闭并重新打开当前项目或平面成图窗口。
5. 如果对象仍不显示，检查是否导入到了其他数据集、其他项目或其他数据类型下。

该现象属于 GeoEast 界面缓存和数据管理视图刷新问题，不代表数据一定写入失败。

## 6. 正确操作建议

推荐流程：

1. 不再使用手动导入得到的错误对象作为输入。
2. 在 FaultPolyTrace Qt 启动器中使用 `从官方 DAT 导入/覆盖输入网格`。
3. 输入网格命名为 `test7_fault_attribute_bo`。
4. 勾选覆盖同名网格，保证对象为最新版本。
5. 点击 `刷新网格`，选择 `test7_fault_attribute_bo`。
6. 输出对象建议命名为 `FaultPolyTrace_Result_Check` 或带日期标记的新名称。
7. 运行后在 GeoEast 平面成图中叠加：
   - 原始网格：`test7_fault_attribute_bo`
   - 结果多边形：`FaultPolyTrace_Result_Check`

## 7. 如果必须使用 GeoEast 官方 Import Grid 手工导入

GeoEast 官方导入并不是不可靠，问题在于官方赛题给的 `test.dat` 是采样点列表，不是已经带完整网格头段的 GeoEast 标准网格 ASCII。只要先把 `test.dat` 转成带头信息的网格 ASCII，再按帮助文档配置 Header Item 和 Body Item，手工导入也可以得到正确坐标范围。

项目已提供转换脚本：

```powershell
python C:\Users\26312\Documents\BGPCup\FaultPolyTrace\scripts\转换官方DAT为GeoEast标准网格ASCII.py `
  --input D:\BaiduNetdiskDownload\BGPCup2025\赛题7-基于平面断层属性的断层多边形自动追踪\test.dat `
  --output C:\Users\26312\Documents\BGPCup\FaultPolyTrace\examples\outputs\geoeast_ascii_import_YYYYMMDD_HHMMSS\test7_fault_attribute_geoeast_grid_ascii.txt `
  --grid-name test7_fault_attribute_ascii
```

已生成的可复查样例：

```text
C:\Users\26312\Documents\BGPCup\FaultPolyTrace\examples\outputs\geoeast_ascii_import_20260505_233936\test7_fault_attribute_geoeast_grid_ascii.txt
C:\Users\26312\Documents\BGPCup\FaultPolyTrace\examples\outputs\geoeast_ascii_import_20260505_233936\test7_fault_attribute_geoeast_grid_ascii.导入说明.md
```

该文件头信息如下：

| 属性 | 值 |
|---|---:|
| Minimum X | 622993.900000 |
| Minimum Y | 4334420.100000 |
| Maximum X | 637993.900000 |
| Maximum Y | 4341420.100000 |
| X Grid Spacing | 10.000000 |
| Y Grid Spacing | 10.000000 |
| X Grid Number | 1501 |
| Y Grid Number | 701 |

在 GeoEast `Mappings -> Grids -> Import Grid` 中选择转换后的 TXT 文件，按旁边生成的 `*.导入说明.md` 配置：

- `Header Item`：把 `Grid Name`、`Minimum X`、`Minimum Y`、`X Grid Spacing`、`Y Grid Spacing`、`X Grid Number`、`Y Grid Number` 等项目映射到文件头对应行列。
- `Body Item`：把 `X`、`Y`、`Grid Value` 分别映射到数据体第 1、2、3 列。
- `DataBody Start`：设置为 `18`。

导入后应立即在网格属性中核对 `Minimum X/Y`、`Grid Spacing`、`Grid Number`。只要仍出现 `800`、`300` 这类 Line/CMP 数值，就说明格式映射没有正确生效。

## 8. 网格头信息核对方法

可使用 GeoEast runner 验证某个 BOMapGrid 的头信息：

```powershell
C:\Users\26312\GeoEast\GeoEast-RC\bin\faultpolytrace_geoeast_runner.exe `
  --db ndp `
  --project BGPCup2026_Test7 `
  --user 26312 `
  --grid test7_fault_attribute_bo `
  --output FaultPolyTrace_Result_Check `
  --validate-grid
```

输出中应重点检查：

```text
sx=622993.9
sy=4334420.1
nx=1501
ny=701
dx=10
dy=10
```

若某对象显示 `sx=800`、`sy=800`、`dx=800`、`dy=800` 或 `nx=800`、`ny=800`，应判定为导入格式错误。

## 9. 工区底图显示建议

在 GeoEast 平面成图中叠合查看时：

1. 先加载正确原始网格 `test7_fault_attribute_bo`。
2. 再加载结果多边形 `FaultPolyTrace_Result_Check`。
3. 如果画布范围异常，检查是否同时加载了错误导入的 `test`、`testdata` 等对象。
4. 在图层管理中隐藏或移除错误范围对象。
5. 使用 `Define Border` 或图层范围重置当前显示范围。
6. 执行 `Refresh All` 后重新查看。

如果错误对象仍参与显示范围计算，即使正确结果本身没问题，画布也可能被错误对象拉大。

## 10. 数据集、刷新与主网格的区别

GeoEast 中容易混淆三件事：

| 概念 | 作用 | 与本问题的关系 |
|---|---|---|
| BOMapGrid 网格对象头 | 决定网格真实坐标范围、网格数、网格间距 | 这是本问题的根因，头信息错了就会显示成巨大版面 |
| 数据集管理 | 决定对象在哪个集合或 `<All>` 中可见 | 影响“导入后列表是否立刻看到”，但不能修正错误坐标 |
| 工区底图/主网格/显示范围 | 决定当前窗口如何显示和缩放 | 可以改善查看体验，但不能把错误导入对象变正确 |

因此处理顺序应是：

1. 先确认网格对象头正确。
2. 再处理数据集可见性和刷新。
3. 最后调整工区底图显示范围。

如果顺序反过来，只在平面成图里反复缩放、刷新或切换主网格，可能暂时看见某些图层，但错误导入对象仍然是错误的。

## 11. 对当前项目的判断

当前现象不是算法错误，也不是 `FaultPolyTrace_Result_Check` 写库坐标错误，而是 GeoEast 手动 ASCII 导入 `test.dat` 时格式定义不匹配造成的输入对象坐标头错误。

后续验收、截图、答辩和算法测试应统一使用 `test7_fault_attribute_bo` 作为官方数据输入对象，并在材料中说明其由 FaultPolyTrace 的 DAT 转 BOMapGrid 导入器生成。
