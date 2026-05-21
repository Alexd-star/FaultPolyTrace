# 测试说明

当前验证以脚本为主：

```powershell
.\scripts\run_smoke_test.ps1
.\scripts\性能回归检查.ps1
```

完整交付前运行：

```powershell
.\scripts\一键总验收.ps1
```

## 已覆盖内容

- 合成数据烟测：区域追踪、L 形闭合、多斑块百分位阈值、脊线带追踪、断裂补连。
- 官方数据性能回归：固定赛题 7 官方数据和推荐参数，检查耗时、多边形数量、面积过滤和拓扑质量。
- GeoEast 数据库读写：通过 `geoeast_plugin/bo_runner` 和对应构建/运行脚本验证。

## 建议门禁

每次修改核心算法、输入输出或 GeoEast 集成后，至少运行：

```powershell
.\scripts\run_smoke_test.ps1
.\scripts\性能回归检查.ps1
```

若修改 GeoEast BO runner 或主控插件，还应运行：

```powershell
.\scripts\build_geoeast_bo_runner.ps1
.\scripts\build_geoeast_mainframe_plugin.ps1
```
