# 泛化验证数据目录

`data/registry/` 保存可公开审查的数据来源、许可证、用途边界和下载入口，属于 Git 内容。真实数据、生成数据和处理中间物体积较大，分别放入 `data/external/`、`data/generated/`、`data/processed/`，这些目录已被 Git 忽略。

公开真实地震体通常不是赛题可直接读取的二维平面断层属性。必须保留原始坐标和切片位置，先从地震体计算相似性、相干或断层似然属性，再提取二维网格。若数据集包含专家标签，标签只用于评价，不得作为算法输入。

来源清单见 `data/registry/public_datasets.json`。自动下载会在数据目录生成 `source_manifest.json`，记录来源、许可证、文件大小和 SHA256，便于复现和审计。

