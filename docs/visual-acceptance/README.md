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

## 10 条相机路径

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
| 10 | `axis-sweep` | 沿 ±X/±Y/±Z 六轴正视（3R 与 1.3R 各一帧） |

前 9 条都绕"上"轴转，对法线接近水平的扁平/零厚度资产（t01 曲面平板、
t07 零轴、t12 大纹理平板）永远是侧视，退化到零面积三角形 → 一帧像素都不剩，
会被误判成空洞。`axis-sweep` 专门补这个覆盖缺口。

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
- 中间 10 个路径按钮，点哪个跑哪条
- "当前可见 tile" 实时列出可见 tile URI 与 geometricError
- 3D 视图里点击模型 → 显示 batch table 属性与 `sourceFeatureId`
- 底部缺陷表单：选类型 → 填 tile URI / 父子路径 / sourceFeatureId → 记录 → 导出 JSON
- "截图 PNG" 存当前帧

## 自动跑（本机可用：node + headless Edge，无需 python/playwright）

前置：仓库根下要有本地 CesiumJS 1.119 副本 `vendor/Cesium/`（页面默认
`cesiumBase=/vendor/Cesium`，不依赖 CDN；`vendor/` 已在 `.gitignore` 里）。取一次：

```bash
cd <仓库根> && npm pack cesium@1.119.0 && tar -xf cesium-1.119.0.tgz
mkdir -p vendor && cp -r package/Build/Cesium vendor/Cesium && rm -rf package cesium-1.119.0.tgz
```

`vendor/Cesium/Workers/transcodeKTX2.js` + `ThirdParty/basis_transcoder.wasm`
必须存在，否则 KTX2 瓦片解不出纹理（页面会退成灰模，`colorSpread` 判据能抓到）。

```bash
# 一条命令：按需自起 node 静态服务器（scripts/serve_static.js，端口 8080），
# 用 msedge --headless=new --disable-gpu --enable-unsafe-swiftshader 驱动本页，
# 页面把每帧 PNG + 度量 POST 回 /va-report，落盘到 runs/<session>/。
bash scripts/visual_capture.sh <session> <tileset-url> [paths] [maxFrames] [port]

# 例：真实模型 10 条路径全帧
VA_BUDGET=900000 bash scripts/visual_capture.sh full_longxiang \
    http://127.0.0.1:8081/longxiang_covfix/tileset.json "" 0 8080

# 汇总所有 session 的 summary.json 成一张表
node scripts/va_summary.js
```

`<tileset-url>` 可以是仓库内相对路径（同服务器），也可以是另一个端口的绝对 URL
（`serve_static.js` 已带 `Access-Control-Allow-Origin: *`）——F 盘产物用
`node scripts/serve_static.js 8081 /f/wb_3dtiles_out` 单独起一个只读服务即可。

产物：

```
docs/visual-acceptance/runs/<session>/
├── <path>_NN_<frame>.png     # 固定视口 1280×720 截图，页面无面板/HUD
├── manifest.jsonl            # 每帧一条：相机、可见 tile、tile 树状态、度量、env
└── summary.json              # 汇总：holes / tooSmallToJudge / flatFrames /
                              #       metricMismatch / darkFrames / console
```

`python scripts/visual_capture.py` 那套 Playwright 脚本保留不动，但在本机
**BLOCKED**（`python` 是 Windows Store 占位程序，rc=49，见 STATUS.md B-5）。

## 每帧量化度量（不依赖人眼的判据）

| 字段 | 算法 | 用途 |
|---|---|---|
| `silhouette.px` | 中性灰底（`rgb(128,128,128)`）下"与背景色差 > 6"的像素数 | **主判据**：该帧到底画出了多少几何 |
| `silhouette.lumAvg` | 剪影内平均亮度 | 区分"没进帧"和"进了帧但纯黑" |
| `coverage.px` | 同相机"显示 / 隐藏 tileset"两帧差值 | 交叉校验：应与 `silhouette.px` 一致，偏差 >2 % 记 `metricMismatch` |
| `projectedRadiusPx` / `contentProjectedRadiusPx` | 外接球投影半径 = `radius/dist/tan(fov/2)·h/2`；后者取"有内容的那些 tile"的最大值 | 只有投影 `>20 px` 的帧才有资格判 `HOLE`，否则记 `tooSmallToJudge`。根球会被离群部件/大坐标撑大（t05 内容 1 m、根球 1.7 km），判据用后者 |
| `colorSpread.sd / chromaFraction` | 剪影内 RGB 标准差 / 彩度占比 | 纹理是否真的贴上（KTX2 解码失败会退成灰模） |
| `tileStates[]` | 整棵树的 `contentReady` / `hasContent` / `geometricError` | `sil=0` 时分清"内容没加载"还是"加载了但没画" |
| `camOk` | 相机 `position/direction/up/right` 是否全为有限数 | NaN 相机会污染其后**所有**帧，必须先判它再判空洞 |
| `settleIters` | 本帧为等内容就绪实际渲染的次数（上限 12） | 贴上限 = 内容没落地就出图，该帧 `sil` 偏低，不能判空洞 |

### 已知不可信项（不要拿它们当证据）

- `tile.isVisible`：1.119 下整棵树的 tile 全都报 `true`，不是逐帧选中信号。
- `scene.pick()`：几何明明画出来了也常返回 `null`（软件渲染下拾取缓冲不可靠）。
- `debugColorizeTiles` 色簇数：只作用于新建的绘制命令，缓存内容不重着色，
  实测出现"剪影 24 万像素但色簇 0"的帧；瓦片是否渲染以 `silhouette` + `tileStates`
  为准，本页已不再采这一帧。

### 帧参数与相机

- 帧的 `dir` 必须是三元数组（`applyFrame` 按下标取值）；传 `Cartesian3` 对象
  会得到 `new Cartesian3(undefined, undefined, undefined)` → NaN 相机。
- `center` 分支里的 `dist` 是**绝对米数**，`dir`+`dist` 分支里的 `dist` 是
  **外接球半径倍数**，两者不能混（`lod-threshold` 曾因此把相机送到几百 km 外）。

### `--virtual-time-budget` 是"跑完还要烧完"，不是超时上限

页面自报 6 帧只用了 0.6 s 虚拟时间，但 `--virtual-time-budget=300000` 时
Edge 会把剩下的虚拟时间补完才退出，实测一条 3 帧的命令卡了 3 分钟。
现在默认 **15000**（约 1500 帧的余量），每帧虚拟开销 ~0.1 s。
批量截图真正的时间成本是软件渲染本身：1280×720 下每帧 ~6 s（近景大轮廓更贵）。

### 每帧的像素度量合并成一次解码

`silhouette` / `coverage` / `colorSpread` / `bright` 原本各写一个函数、
各自 `imgOf(PNG)` + `getImageData` 扫一遍 92 万像素（每帧解码 5 次、扫 5 遍）。
现在统一在 `analyzeFrame()` 里一次解码、一次循环算完，**判据定义逐字未变**，
历史数字仍可比；每帧渲染次数也从 10 次（`shotNow` 成对双渲染）降到 6 次。

### 出帧链路的四个坑（改页面时别退回旧写法）

1. **`requestRenderMode: true` 下 `scene.render()` 会复用上一帧缓存的命令列表。**
   切换 `tileset.show` / `debugColorizeTiles` 后必须 `scene.requestRender()` 再
   `scene.render()`，否则"开/关差值"恒为 0——这个假信号曾把整批截图误判成
   "几何一个都没画"，而实际 PNG 里模型是好的。
2. **无头环境没有真实 vsync，`requestAnimationFrame` 可能根本不触发。**
   等待瓦片加载的循环（`settleShot()`）必须用 `setTimeout` 驱动，
   且**退出条件按渲染次数而不是按秒**：`Date.now()` 在无头下是虚拟时钟，
   "等 8 秒"实测等于每帧白画 500 遍全画布。现在等"未就绪内容数归零"，
   上限 12 次渲染，实际每帧 2 次就出图（`settleIters` 随记录上报，
   贴上限的帧说明仍在等内容，不能当成产品缺陷）。
3. 天空 / 大气随时间渐变，会把背景变化混进差值和颜色直方图里
   （实测把大气梯度数成 105 个"瓦片"）。批量截图统一关天空、用常量灰底。
4. **相机只能用 `camera.lookAtTransform(frame, HeadingPitchRange)`。**
   手写 `camera.position/direction/up/right` 在 `up ∥ direction`（沿 ±Z 正视）
   时退化，Cesium 用旧基重算 `right` 得到 NaN，而 NaN 一旦产生就会**污染后面
   所有帧**——实测整条 `axis-sweep` 60 帧全空、且症状出现在别的路径之后，
   极难归因。`viewFrame()` 自己构造正交基（+Z 指向相机方向）交给 Cesium。

   修复后复跑实测（会话 `fa_t01_plane_curved`，2026-10-19 16:08 完成，
   `node scripts/va_paths.js fa_`）：69 帧 / 58 个帧名全部出图，
   `cameraFinite=false` **0 帧**、`silhouette=0` **0 帧**，其中 12 个
   `ax_*`（六轴正视，含 `up ∥ direction` 的 ±Z）帧中位剪影 65 674–819 868 px。
   对照修复前的 `ax_t05_instances` / `ax_t08_big_coords`：同样 12 帧，
   `cameraFinite=false` 12、空洞 12。10 条注册路径里 `lod-threshold` 在 t01
   上产生 0 帧不是漏跑——该数据集只有 1 个根瓦片且 `geometricError=0`，
   没有 refine 距离可采样（其它 9 条路径均有帧）。

## 环境说明（BLOCKED 项）

本机没有物理显示输出与独显，headless Edge 走 SwiftShader 软件 WebGL
（`env.gpuRenderer = ANGLE (Google, Vulkan 1.3.0 (Swift Shader Device ...), SwiftShader driver-1.0.0)`，
每条记录里都如实带着）。因此：

- **截图本身是真实渲染结果**，`silhouette` / `coverage` / `colorSpread` / `tileStates`
  这些量化判据全部可用，`HOLE`、`OVER_BUDGET`、内容加载失败、纹理未生效都能自动判；
- 但"肉眼看 `SEAM_NORMAL` / `LOD_POP` 主观突兀感、材质观感"这一类需要真机 GPU
  + 显示器复核，本环境标 **BLOCKED**，页面直接开 `index.html?tileset=...` 即可人工审阅。

软件渲染的帧率不代表真机性能，**不得**把本环境的截图耗时当性能数据上报
（性能数字见 `docs/model23dtiles-production/PERFORMANCE_REPORT.md`，口径单独说明）。
