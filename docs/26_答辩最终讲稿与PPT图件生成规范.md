# FaultPolyTrace 答辩最终讲稿与 PPT 图件设计规范

## 1. 答辩内容定位

答辩结束后，应使评委清楚理解以下三点：

1. FaultPolyTrace 将平面断层属性网格自动转换为闭合断层多边形。
2. 算法重点处理弱响应连续追踪和交切区域分支归属。
3. 统一算法核心同时支持独立软件运行和 GeoEast 平台集成。

整套 PPT 采用获奖队伍常用的叙述顺序：应用背景与设计挑战、关键技术与系统实现、测试结果、软件演示和工作总结。每页围绕一个明确主题展开，参数明细、运行日志和历史实验结果不放入主体汇报。

## 2. 从获奖答辩中保留下来的做法

10 份字幕中反复出现的讲解结构如下：

```text
场景中的问题
→ 造成问题的具体原因
→ 一个清楚的技术动作
→ 一张图或一个数字说明结果
```

例如：

- 77 GHz 雷达队先说不同驾驶场景需要不同的测距、测角能力，再讲多模式架构，最后放芯片和测试结果；
- 动态光学脑机接口队把“能效低”和“幅度、相位相互影响”分成两个问题，再分别讲动态传感和交叉稳定转换；
- 随机行走求解器队先解释电容提取到底做什么，再讲为什么用随机行走，最后用同一批 case 和参考工具比较；
- 温度传感器队没有把页面塞满，主要依靠架构图、芯片照片、测试曲线和几个数字完成讲解；
- 被问到比较范围、芯片数量或结果来源时，获奖队伍通常先给一句明确回答，再补充条件和限制。

FaultPolyTrace 按照同样的节奏组织汇报。页面标题采用简洁、准确的技术表述，主要使用“研究对象 + 技术内容”或“测试对象 + 结果类型”的形式；具体结论在图示、数据和讲解中展开。讲稿使用规范的答辩语言，保留必要的自然衔接，但不采用聊天式标题和过度口语化表达。所有数字均取自当前正式 `v1.1.3` 结果，不再出现历史 `214`、`230` 或实验候选。

语言表达统一遵循以下规则：

- 页面标题用于准确说明本页技术内容，不写成旁白句或宣传语；
- 讲稿采用“首先介绍”“针对上述问题”“下面展示”“测试结果表明”“最后进行总结”等正式衔接；
- 技术动作说明到具体对象和判断条件，避免只写“更智能”“更先进”“真正可用”等泛化表述；
- 结果页先说明测试对象和条件，再给出指标及其含义，不扩大指标的适用范围；
- 软件演示使用操作语言，算法说明使用技术语言，两部分不混写。

## 3. 统一视觉方案

### 3.1 真实图和示意图的分工

真实结果必须使用项目实际生成的图：原始属性、纯边界、叠加结果、独立软件截图、GeoEast 截图、泛化测试图。它们负责说明“实际跑出了什么”。

Image 2 只制作算法结构图和系统关系图：连续响应怎么形成、分支为什么不直接合并、质量检查看哪些关系、两个程序如何共用核心。它们负责说明“程序怎样工作”，不承担真实测量数字。

### 3.2 获奖队伍的制图特点

- 白色或浅灰背景，页面留白充足，结构线细而清楚；
- 颜色不多，通常用深蓝表示主结构，用青绿色表示正常数据流，用橙色表示问题、边界或需要注意的位置；
- 先画总架构，再放大一个局部，不把所有模块压在一张图里；
- 芯片类作品常把结构图、实物照片和曲线放在同一页，软件作品对应为算法图、真实界面和输出结果；
- 图中标签很短，解释放在旁白里；
- 不使用霓虹、发光球、复杂渐变、装饰性 3D 或科技感 HUD；
- 真实结果图统一坐标范围，图片保持比例，避免为了填满版面而拉伸。

### 3.3 PPT 基础参数

- 画布：16:9；
- 背景：`#F7F9FB`；
- 深蓝：`#102A43`；
- 青绿：`#1C9AA5`；
- 橙色：`#E78A3B`；
- 正文：微软雅黑或思源黑体，至少 16 pt；
- 页标题：35 至 38 pt，尽量保持一行；
- 图中文字：由 PPT 原生文字添加，Image 2 生成图尽量不承担长中文标签；
- 所有箭头先确定方向，再放节点，箭头不穿过文字和对象；
- 一页只留一张主图或一组有明确关系的图片。

## 4. 12 页最终页面方案

### 第 1 页：封面

**页面标题**

```text
FaultPolyTrace
```

**副标题**

```text
基于平面断层属性的断层多边形自动追踪系统
```

**页面放置**

右侧放项目实际的官方结果纯边界图，底部小幅放独立软件叠加图和 GeoEast 叠加图。不要在封面放算法框图、参数数字或队伍成员长名单。队伍和指导教师信息放在标题下方的两行小字中。

**素材**

- `D:\Luo_docs\竞赛\东方杯\十三届东方杯\作品\作品说明书图片\官方结果图\01_fault_polygons_overview_no_boxes.png`
- `D:\Luo_docs\竞赛\东方杯\十三届东方杯\作品\作品说明书图片\操作步骤截图\01_独立软件\10_独立软件_官方数据叠加结果_最终.png`
- `D:\Luo_docs\竞赛\东方杯\十三届东方杯\作品\作品说明书图片\操作步骤截图\02_GeoEast插件\18_原始网格与结果叠加.png`

**旁白**

> 各位评委老师，大家好。我们是来自东华理工大学的参赛团队。我们的作品是 FaultPolyTrace，一套面向平面断层属性的断层多边形自动追踪系统。作品针对弱响应延伸不连续和交切区域容易误合并的问题，完成断层响应提取、闭合多边形生成、面积筛选及几何质量检查，并提供独立软件和 GeoEast 插件两种运行方式。下面从项目背景、算法设计、测试结果和作品演示四个方面进行汇报。

### 第 2 页：问题

**页面标题**

```text
平面断层属性追踪面临的关键问题
```

**可见文字**

```text
弱响应延伸不连续
交切区域对象归属不明确
追踪结果需要形成闭合区域
```

**页面放置**

左侧占 60% 放官方原始属性图，标出一处弱缺口、一处相邻区域和一处交切位置。右侧三行短句，不做卡片墙。每个位置只画一条细引线。

**旁白**

> 首先介绍项目背景与设计难点。平面断层属性中的强响应主体通常较为清楚，而端部及局部间断区域往往表现为弱响应；相邻断层在交切区域距离较近，仅依据阈值和像元连通关系，容易造成断层截断或对象误合并。同时，追踪结果还需要满足闭合、面积可统计以及 GeoEast 可读写等要求。因此，本项目需要同时解决弱响应连续追踪、交切区域对象归属和闭合多边形生成三个问题。

### 第 3 页：总流程

**页面标题**

```text
断层多边形自动追踪总体流程
```

**可见文字**

```text
属性网格解析与响应分层
方向骨架提取与断层带重建
分支归属与局部连接判断
闭合多边形生成
面积筛选与几何质量检查
```

**页面放置**

使用 Image 2 生成的 `P03_属性到多边形_总流程.png` 作为横向主图。PPT 中用原生文字覆盖简短中文标签，不把生成图里的文字当作最终排版。

**旁白**

> 针对上述问题，我们设计了分层处理流程。首先解析属性网格并提取强、弱响应；随后构建方向骨架和断层带，获得断层主体、局部方向及宽度信息；在交切区域进行分支归属和局部连接判断；最后生成闭合多边形，并完成面积筛选与几何质量检查。各处理阶段均保留独立诊断信息，便于分析属性提取、分支判断和边界生成过程中的具体问题。

### 第 4 页：连续响应

**页面标题**

```text
基于方向约束的强弱响应协同提取
```

**可见文字**

```text
强响应主体提取
方向骨架构建
断层带宽度恢复
弱响应延伸门控
```

**页面放置**

使用 Image 2 生成的 `P04_强响应_骨架_断层带.png`，做成三列 A、B、C：强响应主体、中心骨架、带宽包络。底部放一条很短的结论句。

**旁白**

> 本页介绍强弱响应协同提取方法。算法首先利用强响应确定断层主体，再由骨架提取局部方向和分支结构，并依据属性分布恢复断层带宽度。对于主体端部附近的弱响应区域，系统进一步检查延伸方向、局部属性支撑和背景隔离情况。只有满足方向连续性和支撑条件的弱路径才参与断层延伸；孤立低值和缺少方向依据的区域不会直接纳入追踪结果。

### 第 5 页：交切与分支

**页面标题**

```text
交切区域的分支归属与防误合并
```

**可见文字**

```text
端点方向一致性
局部属性支撑
竞争分支判断
连接后几何复查
```

**页面放置**

使用 Image 2 生成的 `P05_交切处分支归属.png`。左侧画一个容易误合并的交切局部，右侧画经过方向和竞争分支判断后的两个对象。橙色只标出被拒绝的短桥，青绿色标出保留的方向延伸。

**旁白**

> 下面介绍交切区域的处理方法。像元相邻只能说明空间接触，不能直接确定断层归属。算法根据端点方向、两端距离、局部宽度和路径支撑构造连接候选，同时检查旁侧竞争分支、转向变化和宽度突变。候选连接完成后，系统再次执行对象关系和几何质量检查；若出现异常相交、局部鼓包或对象粘连，则撤销本次连接，从而降低交切区域的误合并风险。

### 第 6 页：多边形检查

**页面标题**

```text
闭合多边形生成与几何质量检查
```

**可见文字**

```text
环闭合与面积有效性
单对象自相交检查
对象间异常相交检查
包含与内部重叠检查
```

**页面放置**

使用 Image 2 生成的 `P06_多边形质量检查.png`。图中展示一个合格外环、一个自相交例子和一个对象包含例子，旁边用绿色、橙色、红色做结果分类。真实的 `0` 指标放在 PPT 原生文字中。

**旁白**

> 完成边界生成后，系统依次检查环闭合、面积有效性、自相交、对象间异常相交、包含和内部重叠。官方数据正式结果中，上述异常项均为零。考虑到 GeoEast 不同版本对复杂内环的显示方式存在差异，正式 DAT 采用经过实际导入验证的简单外环表达，使多边形能够按照正确坐标和范围显示；完整的几何检查信息可在独立软件和诊断文件中查看。

### 第 7 页：软件结构

**页面标题**

```text
独立软件与 GeoEast 插件系统架构
```

**可见文字**

```text
外部网格文件       GeoEast 当前工区
        \           /
         FaultPolyTrace C++ 核心
                 |
        结果、统计、图层查看
```

**页面放置**

使用 Image 2 生成的 `P07_双入口共享核心.png`，左侧加独立软件真实截图，右侧加 GeoEast 插件真实截图。生成图只画系统关系，不画假的软件界面。

**旁白**

> 作品采用统一核心、双端应用的软件架构。独立软件读取外部网格文件，适用于脱离 GeoEast 的计算和结果复核；GeoEast 插件读取当前工区中的网格对象，并将追踪结果写回断层组合多边形对象。两个程序共同调用 FaultPolyTrace C++ 核心，使用相同的正式参数、结果统计和质量检查方法，因此能够保持处理逻辑和输出结果的一致性。

### 第 8 页：官方结果

**页面标题**

```text
官方数据追踪结果与面积筛选
```

**可见文字**

```text
输入网格：1501 × 701
候选多边形：212 个
面积筛选后保留：200 个
```

**页面放置**

三联图固定为原始属性、纯边界、结果叠加，使用同一坐标范围和同一比例。数字放在三联图上方，不用大面积数字卡。

**素材**

- `官方结果图\01_original_fault_attribute.png`
- `官方结果图\01_fault_polygons_overview_no_boxes.png`
- `官方结果图\02_faultpolytrace_overlay.png`

**旁白**

> 下面展示官方数据的正式追踪结果。输入网格尺寸为 1501 乘 701，算法共生成 212 个候选多边形，经过面积筛选和几何检查后保留 200 个闭合对象。多边形数量由输入属性和正式参数计算得到，并非预先设定。三幅图依次为原始属性、纯边界和叠加结果，可用于观察断层响应的位置对应关系、边界完整性以及交切区域的对象分布。

### 第 9 页：质量和速度

**页面标题**

```text
官方数据质量指标与处理性能
```

**可见文字**

```text
强响应覆盖率 99.855208%
弱响应覆盖率 97.683691%
开放环、自相交、异常相交 0
包含、内部重叠 0
完整处理约 2.1 秒
```

**页面放置**

左侧放 `F16_官方数据质量指标总览.png`，右侧放 `F17_官方数据耗时分解.png`。数字不做仪表盘，不加发光效果。图注写“覆盖率为程序统计，不等同于专家真值 Recall”。

**旁白**

> 本页给出官方数据的质量指标和处理性能。强响应覆盖率为 99.855208%，弱响应覆盖率为 97.683691%；开放环、自相交、异常相交、包含和内部重叠均为零。开发机上核心追踪耗时约 1093 毫秒，包含质量检查和诊断输出的完整处理耗时约 2107 毫秒。这里的覆盖率用于描述输入属性被结果多边形覆盖的程度，不等同于基于专家解释真值计算的召回率，因此还需要结合独立真值测试评价算法表现。

### 第 10 页：实际操作

**页面标题**

```text
独立软件与 GeoEast 插件运行流程
```

**可见文字**

```text
独立软件：网格读取 → 参数设置 → 追踪与叠加查看
GeoEast 插件：工区读取 → 结果写入 → 图层显示
```

**页面放置**

上半部分放独立软件三张连续截图，下半部分放 GeoEast 插件三张连续截图。每张图用小编号标记步骤，不使用大量说明框。

**素材**

- `操作步骤截图\01_独立软件\02_选择输入后显示原始网格.png`
- `操作步骤截图\01_独立软件\04_追踪完成并显示叠加结果.png`
- `操作步骤截图\02_GeoEast插件\18_原始网格与结果叠加.png`
- `操作步骤截图\02_GeoEast插件\03_GeoEast插件主界面.png`

**旁白**

> 作品提供独立软件和 GeoEast 插件两种操作方式。独立软件完成外部网格读取、参数设置、自动追踪和叠加查看；GeoEast 插件从当前工区选择输入网格，运行同一追踪核心，并将结果写回数据库。写入完成后刷新结果列表，即可在插件视图和 GeoEast 工区底图中分别查看原始网格、结果多边形及其叠加关系。下面通过实际操作进行演示。

### 第 11 页：同域测试

**页面标题**

```text
同域真值与复杂形态验证
```

**可见文字**

```text
80 套独立同域真值案例
12 套 1501 × 701 复杂网络
像元、边界、中心线、对象关系与拓扑评价
```

**页面放置**

左侧放 `00_八类平面真值输入与结果总览.png`，右侧放 `01_平面真值质量与扰动稳定性.png`。下方用一行小字说明公开真实资料只用于观察输入变化带来的影响。

**旁白**

> 为检验算法是否仅适用于官方数据，我们构建了两类独立测试。第一类包含 80 套同域真值案例，覆盖孤立曲线、近邻平行、分支、独立交叉、弱缺口、变宽断层带、边界截断和噪声背景；第二类包含 12 套 1501 乘 701 的复杂断层网络，重点检验交切和分支场景。测试分别评价像元、边界、中心线、对象关系、拓扑以及参数扰动稳定性。公开真实资料仅用于分析输入分布变化，不与官方数据合并统计。

### 第 12 页：结尾

**页面标题**

```text
项目总结
```

**可见文字**

```text
平面断层属性自动追踪
交切关系与闭合几何处理
独立软件与 GeoEast 平台集成
```

**页面放置**

背景用官方叠加图的浅色裁切，中央放三个短句，右下角放作品名称和团队信息。不要再放新指标，也不要重复前面所有数字。

**旁白**

> 最后对本项工作进行总结。FaultPolyTrace 完成了平面断层属性读取、连续断层带追踪、交切关系处理、闭合多边形生成、面积筛选和几何质量检查，并形成独立软件与 GeoEast 插件两种应用形态。官方数据结果、同域真值测试和平台实际运行表明，系统能够稳定完成赛题规定的处理流程。我们的汇报到此结束，感谢各位评委老师的聆听，请各位老师批评指正。

## 5. Image 2 图件清单和提示词

### 5.1 生成规则

Image 2 生成图件时统一采用以下自我提示：

```text
这是科研论文中的方法图，不是宣传海报，不是软件 UI，不是 3D 科幻图。
画布优先使用 landscape 16:9 或 3:2，白色或浅灰背景，深蓝、青绿色、橙色三色以内。
先安排图的分区、箭头和主体关系，再添加少量短标签。
线条细而清楚，节点间距均匀，所有箭头方向明确，不穿过节点和文字。
不使用霓虹、发光、玻璃拟态、巨大标题、装饰性圆球、假 Logo、密集小字或随机中文。
图中不生成真实测试数字，真实数字由 PPT 原生文字补上。
```

### 5.2 P03：属性到多边形总流程

**文件名**：`P03_属性到多边形_总流程.png`

**对应页面**：第 3 页。

**构图**：四个横向阶段，左侧是稀疏平面属性网格，中间是方向骨架和断层带，右侧是多个闭合多边形，底部加一个轻量质量检查分支。用青绿色实线表示正常流程，用橙色细线表示质量不通过后的回退。

**Image 2 提示词**

```text
Landscape 16:9 academic method figure, 3:2 or 16:9 composition, white background, restrained Nature and Science paper palette. Show a clean left-to-right workflow for converting a sparse planar geological fault-attribute grid into closed fault polygons. Four spacious zones: (A) sparse grayscale attribute grid with several elongated curved responses, (B) strong and weak response separation with a thin directional ridge skeleton, (C) variable-width fault belts and branch-aware local paths, (D) several clean closed polygon regions with different muted blue and teal outlines. Add a small lower quality-control branch with simple geometry symbols for closed ring, self-intersection check, overlap check, and area filter. Use thin slate arrows for the normal path and one short muted orange return arrow for rejected candidates. Vector-like scientific diagram, precise alignment, generous whitespace, no 3D, no futuristic HUD, no fake UI, no random long text, no watermark. Leave clean empty label zones for PowerPoint annotations. Technical content only, not a marketing poster.
```

### 5.3 P04：强响应、骨架和断层带

**文件名**：`P04_强响应_骨架_断层带.png`

**对应页面**：第 4 页。

**构图**：三联图。左图显示稀疏强响应，中心用深蓝色高亮主体；中图将响应收缩成平滑中心骨架，标出方向箭头；右图沿骨架生成可变宽度断层带，弱响应延伸用淡青色表示。三幅图坐标范围一致。

**Image 2 提示词**

```text
Landscape 16:9 three-panel scientific figure on white background, publication-grade geological image-processing schematic. Panel A: a sparse planar fault-attribute response, dark navy elongated ridge pixels on a very light gray field, with a few weak low-contrast extensions. Panel B: the same geometry reduced to a smooth centerline skeleton with small directional arrows and a clear branch point. Panel C: the centerline expanded into a variable-width fault belt, dark navy core and pale teal envelope, with a short weak extension shown as a controlled translucent teal path. Keep identical spatial framing across all three panels, thin gray coordinate ticks without numeric labels, clean panel letters A B C, restrained navy teal amber palette, precise vector-like geometry, no random text, no fake map labels, no 3D, no glow, no watermark. Leave blank space above each panel for Chinese captions added later.
```

### 5.4 P05：交切处分支归属

**文件名**：`P05_交切处分支归属.png`

**对应页面**：第 5 页。

**构图**：左右对照。左边用橙色虚线画出仅按邻近关系产生的错误短桥，两个对象被粘成一块；右边保留两个对象，显示端点切线、竞争分支和一条青绿色的方向连续路径。底部用一个小圆环表示“连接后重新检查”。

**Image 2 提示词**

```text
Landscape 16:9 academic topology schematic, white background, two-panel comparison of branch attribution at a fault intersection in a planar geological attribute grid. Left panel: naive proximity-based merging, two elongated fault-band polygons connected by an obviously wrong short bridge near a crossing, show the wrong bridge in muted terracotta dashed stroke and a subtle red cross, but keep the geometry clean and not dramatic. Right panel: constrained attribution, two nearby fault objects remain separate near the crossing; show endpoint tangent arrows, one pale teal accepted path following a smooth direction, and one muted orange rejected competing branch. Add a small lower inset with a clean closed-ring symbol and an overlap-check symbol. Use slate gray, deep navy, muted teal, terracotta; thin publication-quality lines; exact geometric alignment; no long text, no UI panels, no 3D, no neon, no watermark. Leave blank annotation space for PowerPoint labels.
```

### 5.5 P06：多边形质量检查

**文件名**：`P06_多边形质量检查.png`

**对应页面**：第 6 页。

**构图**：左边是合格闭合外环，右边上下两个小例子分别是自相交和对象包含。用绿色、橙色、红色区分合格、需要复核、拒绝。图形尽量接近真实二维几何，而不是装饰性图标。

**Image 2 提示词**

```text
Landscape 16:9 scientific geometry quality-control figure, clean white paper background, three aligned examples of planar polygon validation. Left large example: one smooth closed irregular geological fault polygon, deep navy outline, pale blue fill, green check mark area kept subtle. Top-right small example: a self-intersecting bow-tie ring in terracotta with a clear red warning mark. Bottom-right small example: one polygon completely containing another, shown with nested outlines and an orange review mark. Add simple abstract symbols for area measurement, closed ring, self-intersection, and overlap relation, but no long text. Thin geometry lines, accurate topology, balanced margins, muted navy teal orange red palette, journal figure style, no 3D, no glossy UI, no fake data, no watermark. Leave clean space for native Chinese labels.
```

### 5.6 P07：双入口共享核心

**文件名**：`P07_双入口共享核心.png`

**对应页面**：第 7 页。

**构图**：中央一个深蓝核心模块，左侧是外部网格文件和独立软件窗口的抽象轮廓，右侧是 GeoEast 工区和插件窗口的抽象轮廓，底部是 CSV、DAT 和统计结果三个输出符号。左右箭头汇聚到同一个核心，不能画成两套算法。

**Image 2 提示词**

```text
Landscape 16:9 academic software architecture figure, white background, clean systems-paper style. Put one large central deep-navy rounded rectangle representing a shared deterministic C++ fault-tracing core, with subtle internal abstract modules for attribute tracing, branch reasoning, polygon construction, and geometry checks. On the left, a simple external-file input lane with a planar grid sheet and a restrained standalone desktop window silhouette. On the right, a geological work-area lane with a grid layer, a database cylinder, and a restrained GeoEast plugin window silhouette. Both lanes use separate slate arrows converging into the single central core. At the bottom, three clean output symbols: closed polygon, CSV report, and DAT database object. Use muted navy, teal, slate, warm orange accents, thin precise connectors, generous spacing, no brand logos, no realistic fake UI text, no 3D, no neon, no watermark. Leave blank label zones for PowerPoint Chinese text.
```

### 5.7 P12：项目总结主视觉

**文件名**：`P12_项目总结主视觉.png`

**对应页面**：第 12 页。

**构图**：不再画复杂流程。用一条简洁的弧线把属性网格、断层带、多边形和 GeoEast 图层四个小场景连起来，作为结尾背景。三句结论由 PPT 原生文字添加。

**Image 2 提示词**

```text
Landscape 16:9 restrained closing scientific illustration for a geological software presentation. Very light gray-white background with a subtle pale planar fault-attribute texture on the left, a clean navy directional fault belt in the center, several elegant closed polygons toward the right, and a faint geological work-area layer at the far right. Connect the four stages with one thin smooth teal curve, minimal and calm. Use deep navy, muted teal, pale blue, and one warm orange accent only. The composition should leave a large clean central area for PowerPoint conclusion text. Academic journal figure mood, precise flat geometry, no title, no random labels, no UI, no 3D, no neon, no watermark.
```

## 6. 图片生成和选用顺序

1. 先生成 P03、P04、P05 三张算法图，确认线条、颜色和空间关系统一。
2. 再生成 P06、P07、P12 三张工程图，保持同一白底、同一色彩和同一线宽。
3. 每张图生成后先看全图，再看 100% 放大细节，检查是否有断线、错误连接、文字乱码、主体偏到边缘或图形被裁掉。
4. PPT 中的中文标题、图注、数字和坐标由原生文字添加。Image 2 图中只保留必要的短英文或无文字结构，避免图片文字和 PPT 文字重复。
5. 真实结果图不做 AI 重绘。只做等比裁切、亮度和版面处理，保留实际结果的形状和颜色。
6. 生成图只放在算法解释页，不能把示意图当成官方网格结果，也不能给示意图标注真实指标。

## 7. 现场问答的最终说法

### 为什么不对全部弱响应执行连接？

> 低值区域中既包含断层的弱延伸，也可能包含背景响应和相邻对象。算法首先判断其与已有端部的方向一致性，再检查局部属性支撑和竞争分支。未满足连接条件的区域保留为复核候选，以避免将相邻断层错误合并。

### 200 个多边形是如何确定的？

> 程序首先生成 212 个候选多边形，再依据面积阈值、环闭合和对象间几何关系进行筛选，最终保留 200 个。该数量由输入数据和正式参数计算得到，并非预设值；每个保留对象均可独立进行面积统计和几何检查。

### 覆盖率是否能够代表解释精度？

> 不能直接等同。官方网格没有提供人工多边形真值，因此该覆盖率用于描述输入属性被输出多边形覆盖的程度。项目另行使用独立真值案例评价边界、中心线和对象关系，并对交切密集区域保留人工复核，以避免扩大覆盖率指标的含义。

### 为什么同时提供独立软件和 GeoEast 插件？

> 独立软件面向外部文件读取、独立运行和结果复核；GeoEast 插件面向现有工区中的网格读取、结果写入和图层显示。两种程序调用同一 C++ 追踪核心，并使用相同的正式参数，因此无需分别维护两套算法。

### 追踪结果如何写入 GeoEast？

> 插件将多边形写入当前工区的断层组合多边形对象，写入完成后刷新结果列表，即可在工区底图中查看。DAT 文件按照 GeoEast 可识别的列结构和外环形式生成。由于不同版本对复杂内环的显示方式存在差异，正式结果采用经过实际导入验证的表达形式。

### 后续改进方向是什么？

> 后续工作将进一步细化交切区域的分支归属判断。当前方法已经使用方向、属性支撑和几何检查；新的分支判断需要在独立交叉案例和官方数据上进行对比，确认误合并减少、运行时间保持稳定且 GeoEast 回读结果一致后，再纳入正式版本。

## 8. 排练要求

- 8 分钟主体稿按页面顺序完整讲一遍，不在正式演示时临时改高级参数；
- 第 2、5、8、9 页是评委最容易追问的地方，分别练习问题、交切、正式结果和统计方法；
- 每页讲完留半秒，看一眼图，再切页；
- 所有数字只从当前正式报告读取；
- 图片用实际结果或明确标注的示意图，不让生成图冒充测试结果；
- 现场如果 GeoEast 底图启动较慢，先展示插件内的图层结果和独立软件的叠加图，再回到工区底图，不连续点击按钮。
