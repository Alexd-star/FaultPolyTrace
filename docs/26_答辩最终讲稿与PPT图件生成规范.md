# FaultPolyTrace 答辩最终讲稿与 PPT 图件生成规范

## 1. 这套 PPT 要让评委记住什么

评委听完 8 分钟后，应该能用三句话复述作品：

1. FaultPolyTrace 把平面断层属性网格转成闭合断层多边形。
2. 它重点处理弱响应延伸和交切处分支混连这两个难点。
3. 同一套核心既能在独立软件运行，也能在 GeoEast 工区中读取、写入和查看结果。

整套 PPT 按获奖队伍常用的顺序推进：先说明应用中遇到的麻烦，再讲作品做了哪几件事，然后给出官方网格结果、同域测试和真实软件操作。每一页只讲一个问题，不把参数清单、日志和历史候选塞进主讲稿。

## 2. 从获奖答辩中保留下来的做法

10 份字幕里，最稳定的表达节奏不是“先讲很多背景，再把所有模块都念一遍”，而是：

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

FaultPolyTrace 也按这个节奏讲。页面标题写成一句人会说的话，少用“研究背景、系统架构、结果分析”这样的空标题。数字只放当前正式 `v1.1.3` 结果，不再出现历史 `214`、`230` 或实验候选。

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
平面断层属性网格的自动追踪与多边形生成
```

**页面放置**

右侧放项目实际的官方结果纯边界图，底部小幅放独立软件叠加图和 GeoEast 叠加图。不要在封面放算法框图、参数数字或队伍成员长名单。队伍和指导教师信息放在标题下方的两行小字中。

**素材**

- `D:\Luo_docs\竞赛\东方杯\十三届东方杯\作品\作品说明书图片\官方结果图\01_fault_polygons_overview_no_boxes.png`
- `D:\Luo_docs\竞赛\东方杯\十三届东方杯\作品\作品说明书图片\操作步骤截图\01_独立软件\10_独立软件_官方数据叠加结果_最终.png`
- `D:\Luo_docs\竞赛\东方杯\十三届东方杯\作品\作品说明书图片\操作步骤截图\02_GeoEast插件\18_原始网格与结果叠加.png`

**旁白**

> 各位评委老师好，我们是东华理工大学团队。今天汇报的作品是 FaultPolyTrace，面向平面断层属性网格，自动生成闭合的断层多边形。我们重点解决两件事：弱属性延伸时尽量保持断层连续，断层相互交切时尽量不把不同对象混在一起。最后的结果既可以在独立软件中查看，也可以放回 GeoEast 工区使用。

### 第 2 页：问题

**页面标题**

```text
断层属性图最难的地方，是弱、近、交同时出现
```

**可见文字**

```text
弱响应容易断开
相邻断层容易粘连
结果还要形成闭合区域
```

**页面放置**

左侧占 60% 放官方原始属性图，标出一处弱缺口、一处相邻区域和一处交切位置。右侧三行短句，不做卡片墙。每个位置只画一条细引线。

**旁白**

> 这张图是输入属性。强响应区域比较容易识别，真正麻烦的是三种情况常常挨在一起：一条断层的弱延伸可能断开，附近的另一条断层又可能和它距离很近，交切处还会出现多个方向。全部放宽阈值，容易把不同断层连在一起；只保留强响应，又会把断层截短。最后输出还必须是闭合区域，能统计面积，也能在 GeoEast 中正常显示。我们的算法就是围绕这几个矛盾展开的。

### 第 3 页：总流程

**页面标题**

```text
我们先找断层带，再决定哪些区域可以连在一起
```

**可见文字**

```text
属性网格
强弱响应分开处理
方向骨架与断层带
分支关系判断
闭合多边形与质量检查
```

**页面放置**

使用 Image 2 生成的 `P03_属性到多边形_总流程.png` 作为横向主图。PPT 中用原生文字覆盖简短中文标签，不把生成图里的文字当作最终排版。

**旁白**

> 处理过程可以概括成五步。先读入属性网格，把强响应和弱响应分开看；然后根据属性方向形成骨架和断层带；到了分支和交切位置，再判断不同方向之间的关系；通过判断的区域才会生成闭合多边形；最后检查面积、闭合和对象之间的几何关系。这样做的好处是，每一步都能单独看，出了问题也知道是在属性提取、分支判断还是多边形生成阶段。

### 第 4 页：连续响应

**页面标题**

```text
弱属性不是单独看数值，而是看它能不能沿着断层方向延续
```

**可见文字**

```text
强响应确定主体
骨架提供方向
断层带恢复宽度
弱路径需要局部支撑
```

**页面放置**

使用 Image 2 生成的 `P04_强响应_骨架_断层带.png`，做成三列 A、B、C：强响应主体、中心骨架、带宽包络。底部放一条很短的结论句。

**旁白**

> 这里看的是一条断层从属性到多边形的变化。强响应先确定主体，骨架把方向提出来，断层带再把宽度恢复回来。弱区域只有在已有端部附近，并且沿着相同方向有连续属性支撑时，才会被当作延伸来考虑。一个孤立的低值像元，即使离多边形很近，也不会因为距离近就被直接包进去。

### 第 5 页：交切与分支

**页面标题**

```text
两块区域碰到一起，还要看它们是不是同一条断层
```

**可见文字**

```text
端点方向
属性支撑
旁侧分支
连接后检查
```

**页面放置**

使用 Image 2 生成的 `P05_交切处分支归属.png`。左侧画一个容易误合并的交切局部，右侧画经过方向和竞争分支判断后的两个对象。橙色只标出被拒绝的短桥，青绿色标出保留的方向延伸。

**旁白**

> 交切处不能只看连通性。系统会从多边形端部估计方向，查看两端之间的属性支撑，也会检查旁边是否还有另一条可能的分支。如果一条短桥只是把两个对象粘在一起，或者连接以后出现突然转弯、宽度突变，就不接受这条连接。即使连接已经完成，还要重新检查对象关系；检查不过，就回到连接前的结果。

### 第 6 页：多边形检查

**页面标题**

```text
生成多边形以后，还要确认它真的能用
```

**可见文字**

```text
闭合
面积
自相交
对象相交
包含关系
```

**页面放置**

使用 Image 2 生成的 `P06_多边形质量检查.png`。图中展示一个合格外环、一个自相交例子和一个对象包含例子，旁边用绿色、橙色、红色做结果分类。真实的 `0` 指标放在 PPT 原生文字中。

**旁白**

> 多边形画出来只是中间结果。我们还要检查环是否闭合，面积是否有效，单个对象有没有自相交，不同对象之间有没有异常相交或明显的包含关系。当前官方网格的正式结果中，这几类检查都是零。GeoEast 对复杂内环的显示方式受版本影响，所以写入 GeoEast 的结果采用已经验证过的外环形式，先保证老师打开以后能看到正确位置；独立软件里保留完整的检查信息。

### 第 7 页：软件结构

**页面标题**

```text
独立软件和 GeoEast 插件，用的是同一套追踪核心
```

**可见文字**

```text
外部网格文件       当前 GeoEast 工区
        \           /
         FaultPolyTrace C++ 核心
                 |
        结果、统计、图层查看
```

**页面放置**

使用 Image 2 生成的 `P07_双入口共享核心.png`，左侧加独立软件真实截图，右侧加 GeoEast 插件真实截图。生成图只画系统关系，不画假的软件界面。

**旁白**

> 这里说明软件是怎么组织的。独立软件从外部文件读取网格，GeoEast 插件从当前工区读取网格，但两边调用的是同一个 C++ 追踪核心。参数预设、结果统计和质量检查也保持一致。独立软件适合单独处理和复核，插件适合把结果放回正在使用的工区，两个入口各自承担不同的使用场景。

### 第 8 页：官方结果

**页面标题**

```text
官方 1501×701 网格最终保留 200 个闭合对象
```

**可见文字**

```text
212 个候选
200 个保留
```

**页面放置**

三联图固定为原始属性、纯边界、结果叠加，使用同一坐标范围和同一比例。数字放在三联图上方，不用大面积数字卡。

**素材**

- `官方结果图\01_original_fault_attribute.png`
- `官方结果图\01_fault_polygons_overview_no_boxes.png`
- `官方结果图\02_faultpolytrace_overlay.png`

**旁白**

> 这是官方网格上的正式结果。网格尺寸是 1501 乘 701。程序先得到 212 个候选对象，再按面积、闭合和几何关系筛选，最后保留 200 个。200 不是提前规定的数量，我们更关心的是对象是否闭合、位置是否对应、相互之间有没有明显冲突。左图看输入，中图看边界，右图看属性和结果是否对应。

### 第 9 页：质量和速度

**页面标题**

```text
这组结果同时看覆盖、几何关系和处理时间
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

> 这页放结果检查和耗时。强响应覆盖率是 99.855208%，弱响应覆盖率是 97.683691%。开放环、自相交、异常相交、包含和内部重叠都是零。核心追踪大约 1.09 秒，包含检查和诊断输出的完整处理大约 2.1 秒。这里的覆盖率是程序按照输入属性和输出多边形统计出来的数字，不能直接当成人工真值的 Recall。我们把它和几何检查、处理时间放在一起看。

### 第 10 页：实际操作

**页面标题**

```text
从选择网格到 GeoEast 底图，整套操作可以连续完成
```

**可见文字**

```text
选网格 → 运行 → 看结果
写入 GeoEast → 刷新 → 工区底图查看
```

**页面放置**

上半部分放独立软件三张连续截图，下半部分放 GeoEast 插件三张连续截图。每张图用小编号标记步骤，不使用大量说明框。

**素材**

- `操作步骤截图\01_独立软件\02_选择输入后显示原始网格.png`
- `操作步骤截图\01_独立软件\04_追踪完成并显示叠加结果.png`
- `操作步骤截图\02_GeoEast插件\18_原始网格与结果叠加.png`
- `操作步骤截图\02_GeoEast插件\03_GeoEast插件主界面.png`

**旁白**

> 独立软件里，选择输入网格以后先看原始属性，再点击开始追踪，完成后分别查看结果和叠加效果。GeoEast 插件的操作相同，只是网格来自当前工区。插件写入结果以后刷新列表，再从工区底图查看同一个对象。这里我会把两个图层分开开关，先看原始网格，再看多边形，最后叠加，这样每一步都能看清。

### 第 11 页：同域测试

**页面标题**

```text
我们没有只拿官方网格做一次演示
```

**可见文字**

```text
80 套独立同域真值案例
12 套官方尺寸复杂网络
覆盖、边界、中心线、对象关系和拓扑
```

**页面放置**

左侧放 `00_八类平面真值输入与结果总览.png`，右侧放 `01_平面真值质量与扰动稳定性.png`。下方用一行小字说明公开真实资料只用于观察输入变化带来的影响。

**旁白**

> 除了官方数据，我们还做了两类同域测试。第一类是 80 套独立案例，包含孤立曲线、近邻平行、分支、独立交叉、弱缺口、变宽、边界截断和噪声背景。第二类是 12 套官方尺寸的复杂网络，重点看交叉和分支位置的对象关系。测试时分别看像元覆盖、边界、中心线、对象关系和拓扑。公开真实资料只用来观察输入域发生变化以后哪里容易出问题，不把它和赛题数据混成一个分数。

### 第 12 页：结尾

**页面标题**

```text
我们把一次追踪，做成了一套可以反复使用的流程
```

**可见文字**

```text
看得见：从属性图找到断层带
分得开：交切处判断对象关系
用得上：结果能在软件和 GeoEast 中查看
```

**页面放置**

背景用官方叠加图的浅色裁切，中央放三个短句，右下角放作品名称和团队信息。不要再放新指标，也不要重复前面所有数字。

**旁白**

> 最后总结一下。FaultPolyTrace 先根据属性方向找到断层带，再在交切处判断哪些区域可以相连，最后生成闭合多边形并检查几何关系。它既能单独运行，也能在 GeoEast 工区中使用。我们希望它减少重复描绘和反复整理数据的工作，让解释人员把时间用在真正需要判断的局部区域。谢谢各位老师，欢迎提问。

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

### 5.7 P12：结尾主视觉

**文件名**：`P12_一次追踪到反复使用.png`

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

### 为什么不把所有弱点都连起来？

> 因为低值区域里既有断层的弱延伸，也有背景和相邻对象。我们先看它和已有端部的方向是否一致，再看附近有没有连续属性支撑和其他竞争分支。条件不够时保留为待复核区域，比直接把两个断层粘成一个更稳妥。

### 200 个对象是怎么来的？

> 程序先找到 212 个候选对象，再根据面积、闭合和对象之间的几何关系筛掉不合适的候选，最后保留 200 个。数量不是预先写死的，结果图中的每个对象都可以单独统计面积和检查几何关系。

### 你们的覆盖率能不能说明解释一定正确？

> 不能这样说。官方网格没有提供人工多边形真值，所以官方结果里的覆盖率是程序统计。我们另用独立真值案例检查边界、中心线和对象关系，同时把交切密集的位置保留给人工复核。这样报告出来的结论更准确。

### 为什么要做独立软件和插件两个版本？

> 独立软件方便读取外部文件，适合单独运行和复核；插件方便在已有 GeoEast 工区中读取网格、写入结果和查看图层。两边用同一个核心，所以不需要为两个入口分别调一套参数。

### 结果怎么放回 GeoEast？

> 插件会把多边形写入当前工区的结果对象，写入完成后刷新列表，再从工区底图查看。输出的 DAT 也按当前 GeoEast 能正常识别的列结构和外环形式生成。复杂内环在不同版本中的显示不完全一致，因此最终结果优先采用能够稳定显示的表达方式。

### 以后还会继续怎么改？

> 下一步是把交切处的分支归属做得更细。现在已经有方向、支撑和拓扑检查，后续会在独立交叉案例和官方数据上比较新的分支判断是否真的减少误合并。只有结果稳定、运行时间不明显增加，而且 GeoEast 读回没有变化，才会替换当前版本。

## 8. 排练要求

- 8 分钟主体稿按页面顺序完整讲一遍，不在正式演示时临时改高级参数；
- 第 2、5、8、9 页是评委最容易追问的地方，分别练习问题、交切、正式结果和统计方法；
- 每页讲完留半秒，看一眼图，再切页；
- 所有数字只从当前正式报告读取；
- 图片用实际结果或明确标注的示意图，不让生成图冒充测试结果；
- 现场如果 GeoEast 底图启动较慢，先展示插件内的图层结果和独立软件的叠加图，再回到工区底图，不连续点击按钮。
