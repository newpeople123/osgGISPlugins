# BASELINE.md — model23dtiles 改造前基线

生成时间：2026-10-15
执行者：WorkBuddy（按《osgGISPlugins_model23dtiles_WorkBuddy生产级改造执行手册》第 4 节 Phase 0）

---

## 1. Git 与工作区状态

```
$ git status --short --branch
## dev...origin/dev [ahead 4]
?? osgGISPlugins_model23dtiles_WorkBuddy生产级改造执行手册.md
?? out_test/
?? scripts/
?? simplifier_test.obj
?? tg_build/
?? tile_writer_test.obj

$ git branch --show-current
dev

$ git log -8 --oneline --decorate
b229500 (HEAD -> dev) 修改: 1.将using namespce 从头文件移动至cpp文件; 2.尽量调整为前置声明;
5d4ec31 修改: 1.树构建器的的相关方法重构：形参改为引用； 2.瓦片的最大三角面数量动态设置...
a750d3f 修改: 1.重构CMakeLists文件
01e6823 修改: 1.CMakeLists文件; 2.使用CppDepend分析代码并修正有关问题;
aec8cf3 (tag: v2.3.1, origin/master, origin/dev, origin/HEAD) 修改: 1.显著减小docker镜像体积
7d41eeb 修改: 1.gui工具版本号
d92080c 修改: 1.Dockerfile文件 新增: 1.dockerignore文件
22d8164 修改: gui 1.CMakeLists.txt删除冗余内容

$ git diff --stat
(空 —— 所有被跟踪文件无未提交修改)
```

**结论**：被跟踪文件干净，未被跟踪文件为历史会话遗留与本任务新增。按手册 0.3 条，
从 `dev` 切出独立 feature 分支 `prod/model23dtiles-production`，不覆盖任何用户改动，不推送。

远端：`origin https://github.com/newpeople123/osgGISPlugins.git`（只读用途，本次不推送）

---

## 2. 构建环境

| 项 | 值 |
|---|---|
| OS | Windows（MSYS/Git Bash 终端） |
| CMake | 4.4.3 |
| vcpkg | 2025-06-02-145689e84b7637525510e2c9b4ee603fda046b56（`E:/SDK/vcpkg`） |
| 编译器 | MSVC 19.44.35228（VS2022 Community，工具集 14.44.35207） |
| 生成器 | Visual Studio 17 2022 / x64 |
| OSG | 3.6.5（vcpkg，仓库内 `vcpkg_installed/x64-windows`） |
| TBB | 有（`model23dtiles` 定义 `USE_TBB_PARALLEL=1`） |
| meshoptimizer | 0.23（vcpkg） |
| PROJ | 9.x（vcpkg） |
| KTX | 4.x（vcpkg） |
| FBX SDK | 有（`osgdb_fbx.dll` 已构建） |
| 已安装 OSG 插件 | `osgdb_obj / osgdb_osg / osgdb_gltf / osgdb_fbx / osgdb_ktx / osgdb_webp` …  **缺 `osgdb_osgt.dll`** |

### 2.1 构建阻塞点与解决办法（已记录，可复现）

1. 仓库自带 `build/` 目录的 `CMakeCache.txt` 是另一台机器（`E:/dev/C++/osgGISPlugins`）生成的，
   CMake 直接报 “current CMakeCache.txt directory … is different”。→ 新建 `build_wb/`。
2. 直接 `vcpkg install` 失败：`7z.exe` 在本沙箱内退出码 3221226505。
   → 复用仓库内已装好的 `vcpkg_installed`，用 `-DVCPKG_MANIFEST_INSTALL=OFF -DVCPKG_INSTALLED_DIR=...`。
3. **MSBuild MSB6001 / “No CMAKE_CXX_COMPILER could be found”**：
   环境中同时存在 `https_proxy` 与 `HTTPS_PROXY`（大小写重复）。MSBuild 的 `ToolTask`
   用 `Hashtable.Insert` 收集环境变量时按大小写区分键，同名不同大小写直接抛
   `System.ArgumentException`。→ 构建前 `unset https_proxy http_proxy HTTPS_PROXY HTTP_PROXY`。
4. Git Bash 会把 `-- /m:8` 中的 `/m:8` 当路径做 MSYS 转换 → MSB1008。→ 改用 `--parallel 8`。

### 2.2 可复现构建命令

```bash
bash scripts/wb_build.sh              # 配置 + 编译 model23dtiles (Release)
bash scripts/wb_build.sh reconfigure  # 删掉 build_wb 重新配置再编译
bash scripts/wb_build.sh all          # 编译全部目标
```

脚本内部实际执行的命令（原样记录）：

```bash
export INCLUDE="C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/ucrt;\
C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um;\
C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared;\
C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/include"
export LIB="C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/ucrt/x64;\
C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64;\
C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/lib/x64"
unset https_proxy http_proxy all_proxy ftp_proxy HTTPS_PROXY HTTP_PROXY ALL_PROXY FTP_PROXY

cmake -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=E:/SDK/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_MANIFEST_INSTALL=OFF \
  -DVCPKG_INSTALLED_DIR=<repo>/vcpkg_installed \
  -DVCPKG_TARGET_TRIPLET=x64-windows \
  -S <repo> -B <repo>/build_wb

cmake --build <repo>/build_wb --config Release --target model23dtiles --parallel 8
```

产物：`Release/model23dtiles.exe`（544256 字节，2026-10-15 23:47 链接成功）

---

## 3. 测试数据

无真实 FBX 样本可用（全盘搜索无 `.fbx` 文件）。按手册「缺 FBX SDK/样本」处理：
用**程序化生成的确定性资产**做端到端基线，FBX 端到端标记为 `BLOCKED`（见第 7 节）。

生成器：`scripts/gen_test_assets.py`（固定随机种子，输出逐字节可复现）

```bash
python scripts/gen_test_assets.py      # 输出到 <repo>/test_assets
```

| 资产 | 说明 | 三角形 | 字节 |
|---|---|---:|---:|
| t01_plane_curved.obj | 简单曲面 | 3042 | 188180 |
| t02_thin_features.obj | 薄杆/薄壁/尖角/小突起 | 572 | 80691 |
| t03_giant_single_mesh.obj | 单个不可按对象切分的巨型 mesh | 134162 | 9429501 |
| t04_many_objects.obj | 1200 个独立小对象 | 14400 | 2521062 |
| t05_instances.osg | 300 个重复实例（i3dm） | 3600 | 307230 |
| t06_nonuniform.obj | 细长走廊 2000×30×30 | 2412 | 403061 |
| t07_zero_axis.obj | 零轴（严格平面 + 竖直墙） | 2404 | 128196 |
| t08_big_coords.obj | 大坐标 ~1e7 | 720 | 139908 |
| t09_cross_tile_object.obj | 单个对象横跨 1000 m | 492 | 79721 |
| t10_material_heavy.obj | 40 种材质 | 480 | 78016 |
| t11_sparse_outlier.obj | 密集簇 + 4000 m 外离群对象 | 1824 | 297348 |
| t12_big_texture.obj | 2048² 纹理（硬预算用例） | 6962 | 441961 |

---

## 4. 基线运行

```bash
python scripts/run_matrix.py --out <repo>/docs/model23dtiles-production/baseline
```

（每个资产执行：`<tool> -sp -i <abs input> -o <abs output>`，采集墙钟时间、
Windows PSAPI 峰值工作集、退出码、输入/输出字节，并对产物跑 `scripts/tileset_tool.py validate`）

> **口径澄清（2026-10-20）**：这里的"输入/输出字节"是 **harness 自己量的**
> （`scripts/run_matrix.py:132` 用 `os.path.getsize`、`:140` 用 `dir_bytes(outdir)`），
> 汇总落在 `matrix.json`；**不是** `conversion-report.json` 的字段。后者的
> `inputBytes`/`outputBytes`/`inputFile`/`outputDir` 在盘上 220 份产物里**全部是 `""`/`0`**
> （声明并序列化但从未赋值），记 STATUS **B-10**，别把这两个来源混读。
>
> **上面那条命令今天跑不了**：`run_matrix.py` 是 Python，受 B-5 阻塞（2026-10-20 当场
> 复测 `python -c "print('ok')"` → **rc=49**），所以 §4 的 `python scripts/run_matrix.py …`
> 只能作为 2026-10-16 那批数字的**出处记录**，不是可重跑步骤。现役矩阵链路是
> `bash scripts/run_matrix.sh` + `node scripts/collect_matrix.js`，它**不采集目录字节**，
> 因此"输出字节"这一列在当前链路上**无法复现**，需要按 B-10 二选一补齐。

| 资产 | 退出码 | 秒 | 峰值 MiB | tile 数 | 深度 | 内容文件 | 输出字节 | 校验 ERROR |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| t01_plane_curved.obj | 0xC0000409 | 1.49 | 20.5 | 5 | 4 | 3 | 137496 | 5 |
| t02_thin_features.obj | 0xC0000409 | 1.33 | 21.4 | 5 | 4 | 3 | 122762 | 5 |
| t03_giant_single_mesh.obj | 0xC0000409 | 2.29 | 81.6 | 5 | 4 | 3 | 8831554 | 5 |
| t04_many_objects.obj | 0xC0000409 | 2.16 | 79.2 | 5 | 4 | 3 | 3390784 | 5 |
| t05_instances.osg | — | — | — | i3dm 单独跑通，产出 1 个 i3dm | | | | |
| t06_nonuniform.obj | 0xC0000409 | 1.52 | 29.7 | 4 | 3 | 2 | 388083 | 4 |
| t07_zero_axis.obj | 0xC0000409 | 1.42 | 21.0 | 3 | 2 | 1 | 60962 | 3 |
| t08_big_coords.obj | 0xC0000409 | 1.51 | 21.6 | 3 | 2 | 1 | 60149 | 3 |
| t09_cross_tile_object.obj | 0xC0000409 | 1.40 | 21.2 | 5 | 4 | 3 | 124224 | 5 |
| t10_material_heavy.obj | 0xC0000409 | 1.45 | 21.4 | 5 | 4 | 3 | 121717 | 5 |
| t11_sparse_outlier.obj | 0xC0000409 | 1.50 | 26.3 | 4 | 3 | 2 | 295018 | 4 |
| t12_big_texture.obj | 0xC0000409 | 3.30 | 238.7 | 3 | 2 | 1 | 848171 | 3 |

**退出码 0xC0000409（STATUS_STACK_BUFFER_OVERRUN）在所有资产上都出现，且发生在
tileset.json 写完之后**：输出完整但进程退出码不可用。这是一个独立的现网缺陷
（无法用退出码判定成功/失败），见 AUDIT A-21。

---

## 5. 基线缺陷实测证据

### 5.1 几何误差是「体积加权平均 + 固定 epsilon」

`t01_plane_curved` 的 tileset.json（实测）：

```
tileset.geometricError = 195.55894666318426
root      ge = 130.3726311087895   refine = ADD
 └ child  ge = 130.2726311087895   refine = ADD     ← 差值恰好 0.100000（MIN_GEOMETRIC_ERROR_DIFFERENCE）
    └ leaf ge = 3.045356           refine = REPLACE
```

`t03_giant_single_mesh`：

```
tileset.geometricError = 781.2612094123297 = 520.840806 × 1.5   ← 恰好 ×1.5（Tileset.cpp:158）
root ge = 520.840806 / child ge = 520.740806   ← 差值恰好 0.1
```

### 5.2 boundingVolume 半轴为负（违反 3D Tiles 1.0）

全部 12 个资产的每一级 tile 都报同一错误：

```
[ERROR][BOUNDS] root : box half-extent[7] = -2.99518 is negative
```

`box` 数组第 7 项（Y 半轴）为负。原因见 AUDIT A-10。

### 5.3 空间划分基本没有发生

每个资产都只产出 **1 个叶瓦片**（`0/Tile_L0_0_0_0.b3dm`），即 134162 三角形的
巨型单 mesh（t03）没有被切分，1200 个对象（t04）也被塞进 1 个瓦片。
`t03` 单个 b3dm = 4044904 字节（L0），L0+L1+L2 = 8831554 字节，
即整份几何被**存了 3 份**。

### 5.4 ADD refine 导致父/子同表面同时绘制

所有内部节点 `refine = ADD`。且 `Tile::writeNode()`（Tile.cpp:357-366）把
**第一个子节点**的几何深拷贝后当成本级内容写出。父内容与子内容描述的是同一批
源几何，叠加渲染 → 重影 / Z-fighting / 双倍显存。

### 5.5 相对输出路径失效

`main.cpp` 在读取前调用 `osgDB::setCurrentWorkingDirectory(inputPath)`，
导致 `-o` 给出的相对路径被解析到输入文件所在目录，产物“消失”。
基线运行器已改为全部使用绝对路径。

---

## 6. 基线资源统计（对比手册第 3 节 profile）

`desktop-web-high-quality` 软目标 4 MiB / 硬上限 8 MiB。

| 资产 | 最大单文件字节 | 是否超硬上限 |
|---|---:|---|
| t03_giant_single_mesh | 4044904 | 否（单文件）但整树 8831554 字节且无空间划分 |
| t04_many_objects | 1145848 | 否 |
| t12_big_texture | ~846000 | 否 |

基线没有触发硬上限，是因为**根本没有做空间划分**，而不是因为预算控制有效。

---

## 7. BLOCKED 项

| 编号 | 缺失项 | 影响 | 复现命令 |
|---|---|---|---|
| B-1 | ~~真实 FBX 样本（全盘无 `.fbx`）~~ **已解除（2026-10-16）** | FBX 端到端已在 `D:\BaiduNetdiskDownload\longxiang.fbx` 实测，见 §9 | `Release/model23dtiles.exe -sp -i D:/BaiduNetdiskDownload/longxiang.fbx -o <abs path>/out` |
| B-2 | `osgdb_osgt.dll` 未部署到 `Release/osgPlugins-3.6.5` | `.osgt` 输入 “file not handled”；已改用 `.osg` | 从 `vcpkg_installed/x64-windows/plugins/osgPlugins-3.6.5` 拷贝 |
| B-3 | OGC 3D Tiles Validator（`3d-tiles-validator`，需 npm 安装） | 无法跑官方 validator；本任务用 `scripts/tileset_tool.py` 做等价结构/内容校验 | `npm i -g 3d-tiles-validator && 3d-tiles-validator --tilesetFile <tileset.json>` |
| B-4 | ~~CesiumJS 视觉验收页面 + 可运行浏览器~~ **部分解除（2026-10-16）** | 页面与自动截图脚本已就绪（`docs/visual-acceptance/`），但无物理显示输出与 GPU，人眼验收仍需在有显示的机器上复跑 | 见 `docs/visual-acceptance/README.md` |

---

## 9. 真实 FBX 端到端（B-1 解除记录）

输入：`D:\BaiduNetdiskDownload\longxiang.fbx`（65.8 MB，配套 `longxiang.fbm` 65 个纹理文件）。

命令：

```
Release/model23dtiles.exe -sp -i D:/BaiduNetdiskDownload/longxiang.fbx -o out_test/longxiang
```

实测结果与统计见 `STATUS.md` 的「真实 FBX 实测」小节与
`docs/model23dtiles-production/runs/real_fbx/longxiang_run.log`。

---

## 8. 失败归类（手册第 4 节第 8 步）

| 类别 | 是否出现 | 证据 |
|---|---|---|
| IMPORT | 否 | 所有 OBJ/OSG 资产读入成功 |
| SPATIAL | **是** | 每个资产仅 1 个叶瓦片；巨型单 mesh 未切分 |
| PROXY | **是** | 父内容是子节点的深拷贝，不是子树并集代理 |
| ERROR | **是** | 体积加权平均 + 0.1 epsilon + ×1.5 |
| FEATURE_MAP | **是** | 无 globalFeatureId / 无 manifest；`_BATCHID` 写出时临时编号 |
| SEAM | **是** | 无 seam registry；无跨瓦片边界一致性保障 |
| ENCODING | 否 | b3dm/i3dm 头与内嵌 glb 结构校验通过 |
| SPEC | **是** | box 半轴为负；root→leaf refine=ADD 语义错误 |
| VISUAL | BLOCKED | 见 B-4 |
| PERF | **是** | 整份几何重复 3 份；退出码崩溃 |

---

## 10. 复核基线（Phase 0 re-check，2026-10-19，HEAD `ee6499e`）

§1～§6 是 2026-10-15 在**改造前代码**上测出的历史基线，仍然有效、不改写；
本节记录"当前分支状态 + 当前构建/测试基线"，后续任何回归都以此处为准。

```
$ git status --short --branch
## prod/model23dtiles-production          （仍不推送远端；未被跟踪文件为
                                            历史会话遗留脚本/日志，本任务只
                                            精确 add 自己的文件）
$ git log --oneline -6
ee6499e 代理字节预算拆成 geo/tex 两本账：纹理不再是远景代理的硬约束（A-30）
1fd1fbe 聚合代理按字节硬限反推面数配额并整体不锁边挤压，修 t03 巨型单网格回归
245b717 补上源身份覆盖不变量：A-27 那类"误差不变但部件消失"从此无处藏身
6ab22c4 重写预算驱动简化器与聚合代理：修掉 A-26 多 primitiveSet 不简化、A-27 空代理
37d832c 修复超限代理漏网：检查proxy failure + 聚合fallback不锁边强简化兜底
cf5d010 修复远景LOD代理确定性段错误：构建时去纹理+atlas跳过无纹理StateSet
```

主分支基线提交链：`a53d419`（Phase 0）→ `d18175e` → `2c3d597` → `dcda2a9`
→ `0d8bf02` → `bfa43a0` → 本轮 4 个 commit。

### 10.1 构建与测试基线

| 项 | 命令 | 2026-10-19 实测 |
|---|---|---|
| Release 工具 | `bash scripts/wb_build.sh model23dtiles` | 成功（CMake 4.4.3 + VS 17 2022 / MSVC 14.44；`cl` 不在 PATH，由 wb_build.sh 注入 vcvars） |
| 单元测试 | `bash scripts/wb_build.sh test_production && ./Release/test_production.exe` | **passed=71 failed=0**（历史基线 44 断言；A-31/A-32 并入后 82，A-33/34/35 并入后 **118/118**，见 VALIDATION_REPORT §3） |
| 12 资产矩阵 | bash+node 采集器（见 VALIDATION_REPORT §1 命令） | **12/12 发布，§3 硬限违例 0**，`featuresMissingFromLeaves=0`、`proxyDroppedFeatureCount=0` |
| 真实 FBX | `Release/model23dtiles.exe -sp -i D:/BaiduNetdiskDownload/longxiang.fbx -o F:/wb_3dtiles_out/longxiang_covfix` | 0 error、129 tiles、93.4 s / 2 401 MiB（见 VALIDATION_REPORT §6.1） |

### 10.2 与 2026-10-15 基线的环境差异（影响可复现性）

- **Python 不可用（新增 B-5）**：`python`/`python3` 解析到
  `C:\Users\hdec\AppData\Local\Microsoft\WindowsApps\python.exe`（Store 占位程序），
  `python -c "print(1)"` 退出码 49、零输出。因此 `run_matrix.py`、
  `run_extra_scenarios.py`、`tileset_tool.py`、`visual_capture.py` 本轮全部
  无法执行；矩阵改用等价的 bash+node 采集（同一命令行、同一指标来源
  `conversion-report.json`），S1–S4 四场景与截图仍是 2026-10-16 的历史产物。
- 宿主改写退出码：`model23dtiles.exe` 正常发布产物时退出码仍可能被改写
  （历史实测 `0xC0000409`），因此**判据只用 `errors==0` + `tileset.json` 是否落盘**，
  与 §4 当时的记法一致。
- 内存：本机 127 GiB，`longxiang` 峰值 2.4 GiB 不构成风险，但换小内存机器
  需按 §10.1 的 RSS 值预留。

### 10.3 仍未闭环的项（详见 STATUS.md BLOCKED）

B-4 真机 GPU 人眼验收、B-5 Python、B-6 t08/t11 远景部件瞬现、
B-7 longxiang 内存回退（335 MB 的 lugouqiao 实测工作集 5.5 GiB 以上，见
PERFORMANCE_REPORT §6.4）、B-8 cmpt 混合实例场景（`mixed.cmpt` 探针 0/0/0 已过，
生产写出器仍输出兄弟 b3dm/i3dm 子树，仓库内无 cmpt 写出器）。
~~B-3 外部官方 validator~~ **工具侧**已闭环：`bash scripts/run_validator.sh <dir>` 实测
14/14 个 tileset `numErrors=0`（VALIDATION_REPORT §7）。**2026-10-20 更新**：工具仍可跑，
但 `lugouqiao_v17` 判 **rc=1 / 26 个内容文件 ERROR**（A-43 零长度 NORMAL），
同批 `matrix_a42fix` 12/12 与 `longxiang_a42` 判 0 error — 所以 B-3 的"能力缺口"没了，
取而代之的是一条**未过的发布判据**（见 VALIDATION_REPORT §7 的 2026-10-20 复跑）。
