# 浏览器视觉验收（手册第 15 节）

## 固定参数

| 项 | 值 | 说明 |
|---|---|---|
| 目标客户端 | **CesiumJS 1.119** | `index.html` 里 CDN URL 写死版本，不浮动 |
| 视口 | **1280 × 720** | 页面里固定 CSS 尺寸，`useBrowserRecommendedResolution=false` 口径 |
| FOV | **60°** | `camera.frustum.fov` |
| 屏幕空间误差 | **4 px** | `tileset.maximumScreenSpaceError`，精细审阅另测 2 px |
| 坐标系 | tileset 自身 ECEF | 相机路径全部相对 tileset `boundingSphere` 计算，换模型不用改代码 |
| 精度 | `deviceScaleFactor=1` | 截图不被 DPR 放大 |

## 9 条相机路径

| # | 名称 | 覆盖的手册要求 |
|---|---|---|
| 1 | `far-to-near` | 极远景到近景 |
| 2 | `near-to-far` | 近景返回远景 |
| 3 | `orbit` | 绕场景水平旋转（360°，每 30° 一帧） |
| 4 | `oblique-high` | 高角度斜视 |
| 5 | `oblique-low` | 低角度斜视 |
| 6 | `seam-pan` | 沿同级 tile seam 平移 |
| 7 | `feature-approach` | 在关键对象前后移动相机 |
| 8 | `lod-threshold` | 在每个 LOD 切换阈值附近来回移动 |
| 9 | `texture-zoom` | KTX2 纹理不同缩放 / mipmap 级别 |

LOD 切换阈值用 CesiumJS 的 SSE 定义反解：

```
sse(px) = geometricError × viewportHeightPx / (2 × distance × tan(fov/2))
=> distance = geometricError × viewportHeightPx / (2 × sse × tan(fov/2))
```

`lod-threshold` 路径对该距离取 ±6% 各放一帧，正好压在 refine 边界两侧。

## 缺陷分类

| 代码 | 含义 |
|---|---|
| `HOLE` | 几何洞 / 内容丢失 |
| `DUPLICATE` | 重复几何 / 重影 |
| `LOD_POP` | 轮廓或对象明显跳变 |
| `SEAM_GEO` | 几何裂缝 |
| `SEAM_NORMAL` | 法线 / 光照断层 |
| `SEAM_UV` | 纹理边界或 mipmap bleed |
| `FEATURE_MAP` | 选错对象 / 属性丢失 |
| `BOUND_CULL` | 包围体导致错误剔除 |
| `OVER_BUDGET` | 瓦片超限 |

每条缺陷记录都会带上：缺陷类型、tile URI、父子路径、sourceFeatureId、
相机位置（经纬高 + FOV + SSE）、环境信息（浏览器 / GPU / 驱动）、时间戳。
**缺陷未修复不得 PASS。**

## 手动跑

```bash
# 起一个静态服务器，根目录必须是仓库根（页面和 tileset 都要能访问到）
cd <仓库根>
python -m http.server 8080
```

浏览器打开：

```
http://127.0.0.1:8080/docs/visual-acceptance/index.html?tileset=out_test/longxiang/tileset.json
```

页面右侧面板：
- 上半部分显示环境信息（UA / GPU 渲染器 / WebGL 版本），**截图前先确认这里**
- 中间 9 个路径按钮，点哪个跑哪条
- "当前可见 tile" 实时列出可见 tile URI 与 geometricError
- 3D 视图里点击模型 → 显示 batch table 属性与 `sourceFeatureId`
- 底部缺陷表单：选类型 → 填 tile URI / 父子路径 / sourceFeatureId → 记录 → 导出 JSON
- "截图 PNG" 存当前帧

## 自动跑

```bash
python scripts/visual_capture.py \
    --tileset out_test/longxiang/tileset.json \
    --out docs/visual-acceptance/runs/longxiang \
    --paths far-to-near,near-to-far,orbit,oblique-high,oblique-low,seam-pan,feature-approach,lod-threshold,texture-zoom
```

产物：

```
docs/visual-acceptance/runs/longxiang/
├── capture.json            # 全部帧的相机、可见 tile、环境、console/page 错误
└── shots/
    ├── far-to-near/000_dist_30.00R.png ...
    ├── orbit/000_az_0.png ...
    └── lod-threshold/...
```

依赖：`pip install playwright && playwright install chromium`。

无独显的机器用 SwiftShader 软件渲染也能跑，但 `capture.json` 里
`env.gpuRenderer` 会是 `SwiftShader`/`ANGLE (Google, Vulkan ...)`。
**报告里必须如实写这一项**，不能拿软件渲染的结果冒充真机 GPU 验收。

## 环境说明（BLOCKED 项）

本仓库的自动化环境没有物理显示输出与 GPU：
- Playwright 的 Chromium 在 headless 下只能拿到 SwiftShader 软件 WebGL，
  **不能替代真实 GPU 驱动下的视觉验收**；
- 因此"截图里肉眼看接缝 / LOD_POP"这一项在本环境标 **BLOCKED**，
  页面与截图脚本已就绪，命令见上，需要有显示输出的机器上复跑。

自动化能验的部分（不依赖人眼）：
- 页面 console / page error 数量（内容丢失、shader 编译失败会直接报错）
- 每帧可见 tile 数与 URI（HOLE / BOUND_CULL 的间接信号）
- tile 是否超预算（`OVER_BUDGET`，配合 `tileset_tool.py` 的 BUDGET 检查）
- LOD 切换阈值前后的 tile 集合变化（LOD_POP 的量化信号）
