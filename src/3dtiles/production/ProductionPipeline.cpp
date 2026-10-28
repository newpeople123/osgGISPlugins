#include "3dtiles/production/ProductionPipeline.h"
#include "3dtiles/production/Trace.h"
#include <osg/ComputeBoundsVisitor>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/CoordinateSystemNode>
#include <osg/CopyOp>
#include <osg/Material>
#include <osgdb_gltf/material/GltfMaterial.h>
#include <osgdb_gltf/material/GltfPbrMRMaterial.h>
#include <osgDB/WriteFile>
#include <osgDB/FileUtils>
#include <osgDB/ReadFile>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <future>
#include <iterator>
#include <functional>
#include <filesystem>
#include <set>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>   // 只用于判断"暂存目录的属主进程是否还活着"
#endif
#include "utils/Utils.h"
#include "utils/GltfOptimizer.h"
#include "osgdb_gltf/Extensions.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"

namespace {

// ---------------------------------------------------------------------------
// 暂存目录的属主与清扫（本轮：失败 / 强杀场景）
//
// 旧实现在 `<output>/.wb-staging` 这个**固定名字**下建暂存区，也从不检查上一次
// 是不是被强杀过：进程被杀后残留的 tiles，会在下一次运行结束时被一并"搬进"
// 正式输出（tileset 不引用它们，但目录里多出一堆无主瓦片）。同一输出目录并发
// 跑两个任务时，还会互相清掉对方的暂存区。
//
// 现在：暂存目录名带 pid（`.wb-staging-<pid>`），里面写 `owner.pid`；每次开跑前
// 只清理**属主进程已经不在**的旧暂存目录，活着的（别人的任务）一律不碰。
// 限制：pid 可能被系统复用，所以这里只做"清理残留"这一件低风险的事，
// 不作为任何数据安全的唯一依据。
// ---------------------------------------------------------------------------
unsigned currentProcessId()
{
#ifdef _WIN32
    return static_cast<unsigned>(GetCurrentProcessId());
#else
    return 0u;
#endif
}

bool processAlive(unsigned pid)
{
#ifdef _WIN32
    if (pid == 0) { return false; }
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (h == nullptr) { return false; }   // 打不开 = 已经不在
    DWORD code = 0;
    const bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
#else
    (void)pid;
    return true;   // 非 Windows 不猜：宁可不清理
#endif
}

void writeOwnerPid(const std::filesystem::path& stagingDir)
{
    std::ofstream f(stagingDir / "owner.pid", std::ios::binary);
    if (f) { f << currentProcessId(); }
}

// 清掉 outputDir 下属于"已死进程"的旧暂存目录；`keep` 永不触碰。
void sweepStaleStagingDirs(const std::filesystem::path& outDir, const std::filesystem::path& keep)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(outDir, ec)) { return; }
    for (const auto& e : std::filesystem::directory_iterator(outDir, ec))
    {
        if (ec) { break; }
        if (!e.is_directory(ec)) { continue; }
        const std::string name = e.path().filename().string();
        if (name.rfind(".wb-staging", 0) != 0) { continue; }
        if (e.path() == keep) { continue; }
        unsigned pid = 0;
        {
            std::ifstream f(e.path() / "owner.pid");
            if (f) { f >> pid; }
        }
        if (pid != 0 && processAlive(pid)) { continue; }   // 活着的任务，别动
        std::error_code ecRemove;
        std::filesystem::remove_all(e.path(), ecRemove);
    }
}

// 清除纹理对象上的导出缓存名（exportOsgTextureIfNeeded 以此判断是否
// 已导出过），强制下一次 optimize 重新导出——配合降采样的 textureOptions
// 会生成新内容 hash 的新文件，不与共享缓存冲突。
// 用途：不可分节点（单对象带几十个材质/纹理，分区器无法再切）的瓦片
// 内容字节超限时，逐级降采样纹理重写（lugouqiao 实测 1.5k 三角形瓦片
// 因 30+ 张纹理达 10MB+）。
struct ClearTextureExportCacheVisitor : public osg::NodeVisitor
{
    ClearTextureExportCacheVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
    void clearTexture(osg::StateSet* ss)
    {
        if (!ss) { return; }
        const unsigned units = ss->getNumTextureAttributeLists();
        for (unsigned unit = 0; unit < units; ++unit)
        {
            if (osg::Texture2D* tex =
                    dynamic_cast<osg::Texture2D*>(ss->getTextureAttribute(unit, osg::StateAttribute::TEXTURE)))
            {
                tex->setUserValue(BASECOLOR_TEXTURE_FILENAME, std::string());
                tex->setUserValue(NORMAL_TEXTURE_FILENAME, std::string());
                tex->setUserValue(OCCLUSION_TEXTURE_FILENAME, std::string());
                tex->setUserValue(EMISSIVE_TEXTURE_FILENAME, std::string());
                tex->setUserValue(MR_TEXTURE_FILENAME, std::string());
                tex->setUserValue(DIFFUSE_TEXTURE_FILENAME, std::string());
                tex->setUserValue(SG_TEXTURE_FILENAME, std::string());
            }
        }
    }
    void apply(osg::Geode& geode) override
    {
        clearTexture(geode.getStateSet());
        traverse(geode);
    }
    void apply(osg::Drawable& d) override
    {
        clearTexture(d.getStateSet());
        if (osg::Geometry* g = d.asGeometry()) { clearTexture(g->getStateSet()); }
    }
};

// 远景 LOD 代理去纹理：为每个节点/Drawable 构造私有 StateSet（SHALLOW
// 拷贝容器，纹理属性清空，GltfMaterial 子类替换为只带 diffuse 的普通
// osg::Material）。
// 关键约束：绝不修改共享的原始 StateSet/Uniform/GltfMaterial —— 并行构建
// 时 worker 还在读源模型，任何共享写都会破坏引用计数（v8-v10 段错误根因）。
// 代理内容节点本身是 materialize 私有产物，替换其 stateSet 指针安全。
struct FarLodTextureStripper : public osg::NodeVisitor
{
    FarLodTextureStripper() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
    static osg::ref_ptr<osg::StateSet> makeTextureless(osg::StateSet* src)
    {
        // 全新空 StateSet，不从 src 拷贝构造：SHALLOW_COPY 的拷贝构造会给
        // 共享 Uniform addParent / 拷贝共享纹理属性的 ref_ptr，多 worker 并行
        // 时对同一批共享对象的引用计数做非原子写（历史段错误根因）。
        // 远景代理只需要一个材质副本：无纹理、无 Uniform、不共享任何属性。
        osg::ref_ptr<osg::StateSet> ss = new osg::StateSet();
        if (src)
        {
            if (osg::Material* mat =
                    dynamic_cast<osg::Material*>(src->getAttribute(osg::StateAttribute::MATERIAL)))
            {
                // GltfMaterial 子类（glTF 专用字段挂在 StateSet 上）替换为
                // 普通 osg::Material 副本，Osg2Gltf 走干净的 baseColorFactor 路径
                osg::ref_ptr<osg::Material> plain =
                    new osg::Material(*mat, osg::CopyOp::SHALLOW_COPY);
                ss->setAttributeAndModes(plain.get(), osg::StateAttribute::ON);
            }
        }
        return ss;
    }
    static void applyTo(osg::Node& n)
    {
        if (n.getStateSet()) { n.setStateSet(makeTextureless(n.getStateSet())); }
    }
    void apply(osg::Node& node) override
    {
        applyTo(node);
        traverse(node);
    }
    void apply(osg::Geode& geode) override
    {
        applyTo(geode);
        traverse(geode);
    }
    void apply(osg::Drawable& d) override { applyTo(d); }
};
} // namespace
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

#ifndef OSG_GIS_PLUGINS_PATH_SPLIT_STRING
#ifdef _WIN32
#define OSG_GIS_PLUGINS_PATH_SPLIT_STRING "\\"
#else
#define OSG_GIS_PLUGINS_PATH_SPLIT_STRING "/"
#endif
#endif

using namespace osgGISPlugins;
using namespace osgGISPlugins::production;

namespace {

class Timer {
public:
    Timer() : _t0(std::chrono::steady_clock::now()) {}
    double elapsed() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - _t0).count();
    }
private:
    std::chrono::steady_clock::time_point _t0;
};

// 线程 CPU 秒计时器。壁钟仪表和进程 CPU 秒不是一个量：8 个瓦片并行时
// Σ 壁钟只等于 1×工作量，CPU 秒是 8×；节点内再开线程又是一层。
// 想让"已归属 ≈ 进程 user 秒"这条算式成立，仪表必须也用 CPU 秒。
class CpuTimer {
public:
    CpuTimer() { _t0 = threadCpuSeconds(); }
    double elapsed() const { return threadCpuSeconds() - _t0; }
private:
    double _t0 = 0.0;
    static double threadCpuSeconds()
    {
#ifdef _WIN32
        FILETIME c, e, k, u;
        if (GetThreadTimes(GetCurrentThread(), &c, &e, &k, &u))
        {
            const auto toSec = [](const FILETIME& ft) {
                ULARGE_INTEGER v;
                v.LowPart = ft.dwLowDateTime;
                v.HighPart = ft.dwHighDateTime;
                return static_cast<double>(v.QuadPart) / 1.0e7;
            };
            return toSec(u) + toSec(k);
        }
#endif
        return 0.0;
    }
};

std::uint64_t currentPeakRss()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, pmc.cb)) { return pmc.PeakWorkingSetSize; }
#endif
    return 0;
}

// 进程自身的 CPU 秒 / 墙钟秒（自进程创建累计）。
// run() 的 totalSeconds 覆盖不到 main.cpp 里的 osgDB::readNodeFile，
// 335 MB 档那 32 % "无归属 CPU 秒"的嫌疑就在这段外面；把进程读数写进报告，
// 归属算术才不需要外部 PowerShell 监视器（它有 1 s 级采样滞后）。
struct ProcessTimes {
    double cpuSeconds = 0.0;
    double wallSeconds = 0.0;
    double userSeconds = 0.0;
    double kernelSeconds = 0.0;
};

ProcessTimes currentProcessTimes()
{
    ProcessTimes pt;
#ifdef _WIN32
    FILETIME creation, exitT, kernel, user;
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exitT, &kernel, &user))
    {
        const auto toSec = [](const FILETIME& ft) {
            ULARGE_INTEGER u;
            u.LowPart = ft.dwLowDateTime;
            u.HighPart = ft.dwHighDateTime;
            return static_cast<double>(u.QuadPart) / 1.0e7;   // 100 ns 单位
        };
        pt.userSeconds = toSec(user);
        pt.kernelSeconds = toSec(kernel);
        pt.cpuSeconds = pt.userSeconds + pt.kernelSeconds;
        FILETIME now;
        SYSTEMTIME st;
        GetSystemTime(&st);
        if (SystemTimeToFileTime(&st, &now))
        {
            pt.wallSeconds = toSec(now) - toSec(creation);
        }
    }
#endif
    return pt;
}

// OSG/glTF 是 Y-up，Cesium/3D Tiles 的 box 是 Z-up： (x, y, z) -> (x, -z, y)
// 这里对 AABB 的 8 个角点逐一变换后重新取 min/max，半轴必然非负。
// （修复 AUDIT A-10：原实现直接对 _max/_min 做旋转再相减，会得到负半轴）
void yUpToZUpBounds(const osg::BoundingBox& src, osg::BoundingBoxd& dst)
{
    dst.init();
    if (!src.valid()) { return; }
    const double xs[2] = {src.xMin(), src.xMax()};
    const double ys[2] = {src.yMin(), src.yMax()};
    const double zs[2] = {src.zMin(), src.zMax()};
    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            for (int k = 0; k < 2; ++k)
            {
                dst.expandBy(osg::Vec3d(xs[i], -zs[k], ys[j]));
            }
        }
    }
}

json boxJson(const osg::BoundingBoxd& zUp)
{
    if (!zUp.valid())
    {
        return json::array({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    }
    // 全程 double：center 也必须按 double 算。用 float 的 center() 会让
    // center-half 与 min 差出 ~1e-5，父子包围盒的"父覆盖子"校验会失败
    // （历史 bug：t04/t05/t06/t08/t10/t11 都报过 child box not covered）。
    const double cx = (zUp.xMin() + zUp.xMax()) * 0.5;
    const double cy = (zUp.yMin() + zUp.yMax()) * 0.5;
    const double cz = (zUp.zMin() + zUp.zMax()) * 0.5;
    const double hx = std::max(0.0, (zUp.xMax() - zUp.xMin()) * 0.5);
    const double hy = std::max(0.0, (zUp.yMax() - zUp.yMin()) * 0.5);
    const double hz = std::max(0.0, (zUp.zMax() - zUp.zMin()) * 0.5);
    return json::array({cx, cy, cz,
                        hx, 0.0, 0.0,
                        0.0, hy, 0.0,
                        0.0, 0.0, hz});
}

unsigned countGeodes(osg::Node* node)
{
    if (!node) { return 0; }
    struct Counter : public osg::NodeVisitor {
        Counter() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
        void apply(osg::Geode& g) override { if (g.getNumDrawables()) { ++n; } traverse(g); }
        unsigned n = 0;
    } c;
    node->accept(c);
    return c.n;
}

void collectTrianglesFromNode(osg::Node* node, TriangleSoup& soup)
{
    if (!node) { return; }
    struct Collector : public osg::NodeVisitor {
        Collector(TriangleSoup& s) : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), soup(s) {}
        void apply(osg::Transform& t) override
        {
            osg::Matrixd m;
            t.computeLocalToWorldMatrix(m, this);
            _stack.push_back(_current);
            _current = m * _current;
            traverse(t);
            _current = _stack.back();
            _stack.pop_back();
        }
        void apply(osg::Drawable& d) override
        {
            osg::Geometry* g = d.asGeometry();
            if (!g) { return; }
            const osg::Vec3Array* pos = dynamic_cast<const osg::Vec3Array*>(g->getVertexArray());
            if (!pos) { return; }
            for (unsigned p = 0; p < g->getNumPrimitiveSets(); ++p)
            {
                const osg::PrimitiveSet* ps = g->getPrimitiveSet(p);
                if (ps->getMode() != osg::PrimitiveSet::TRIANGLES) { continue; }
                for (unsigned i = 0; i + 2 < ps->getNumIndices(); i += 3)
                {
                    const osg::Vec3d a = osg::Vec3d((*pos)[ps->index(i)]) * _current;
                    const osg::Vec3d b = osg::Vec3d((*pos)[ps->index(i + 1)]) * _current;
                    const osg::Vec3d c = osg::Vec3d((*pos)[ps->index(i + 2)]) * _current;
                    soup.add(a, b, c);
                }
            }
        }
        TriangleSoup& soup;
        osg::Matrixd _current;
        std::vector<osg::Matrixd> _stack;
    } c(soup);
    c._current.makeIdentity();
    node->accept(c);
}

} // namespace

// ---------------------------------------------------------------------------
// SeamRegistry
// ---------------------------------------------------------------------------
std::int64_t SeamRegistry::key(const osg::Vec3d& p) const
{
    const auto q = [this](double v) { return static_cast<std::int64_t>(std::floor(v / _tol)); };
    const std::int64_t a = q(p.x()), b = q(p.y()), c = q(p.z());
    return (a * 73856093) ^ (b * 19349663) ^ (c * 83492791);
}

void SeamRegistry::build(const SourceModel& model)
{
    _seamPositions.clear();
    _itemSeams.clear();
    // 建立 位置key -> 顶点出现位置(primRef, 三角形, 角) 的索引
    std::map<std::int64_t, std::vector<std::pair<std::uint32_t, int>>> index;
    std::map<std::uint32_t, std::set<std::int64_t>> perItem;
    for (std::uint32_t id = 0; id < model.prims.size(); ++id)
    {
        const PrimRef& pr = model.prims[id];
        std::vector<unsigned> local;
        model.collectTriangleIndices(pr, local);
        const SourceMesh& mesh = model.meshes[pr.meshId];
        for (unsigned v : local)
        {
            const osg::Vec3& p = (*mesh.positions)[v];
            const std::int64_t k = key(osg::Vec3d(p));
            index[k].push_back({id, static_cast<int>(v)});
            perItem[id].insert(k);
        }
    }
    // 出现在 >=2 个不同 item 的位置就是候选 seam（跨瓦片边界的可能位置）
    for (auto& kv : index)
    {
        std::set<std::uint32_t> items;
        for (auto& occ : kv.second) { items.insert(occ.first); }
        if (items.size() < 2) { continue; }
        // 记录代表性坐标（取第一个出现）
        const std::uint32_t firstItem = *items.begin();
        const SourceMesh& mesh = model.meshes[model.prims[firstItem].meshId];
        const int v = kv.second.front().second;
        const osg::Vec3& p = (*mesh.positions)[v];
        const std::size_t sid = _seamPositions.size();
        _seamPositions.push_back(osg::Vec3d(p));
        for (std::uint32_t it : items) { _itemSeams[it].push_back(sid); }
    }
}

std::size_t SeamRegistry::detectCrossTileSeams(const SourceModel& model,
                                              const std::map<std::uint32_t, int>& itemToLeaf)
{
    std::size_t n = 0;
    std::vector<char> isCross(_seamPositions.size(), 0);
    for (auto& kv : _itemSeams)
    {
        auto itLeaf = itemToLeaf.find(kv.first);
        if (itLeaf == itemToLeaf.end()) { continue; }
        for (std::size_t sid : kv.second)
        {
            // 该 seam 位置是否出现在 >=2 个不同叶
            std::set<int> leaves;
            for (auto& kv2 : _itemSeams)
            {
                auto it2 = itemToLeaf.find(kv2.first);
                if (it2 == itemToLeaf.end()) { continue; }
                if (std::find(kv2.second.begin(), kv2.second.end(), sid) != kv2.second.end())
                {
                    leaves.insert(it2->second);
                }
            }
            if (leaves.size() >= 2 && !isCross[sid]) { isCross[sid] = 1; ++n; }
        }
    }
    // 只保留跨瓦片的那些
    std::vector<osg::Vec3d> kept;
    std::map<std::uint32_t, std::vector<std::size_t>> keptItems;
    std::map<std::size_t, std::size_t> remap;
    for (std::size_t i = 0; i < _seamPositions.size(); ++i)
    {
        if (!isCross[i]) { continue; }
        remap[i] = kept.size();
        kept.push_back(_seamPositions[i]);
    }
    for (auto& kv : _itemSeams)
    {
        for (std::size_t sid : kv.second)
        {
            auto it = remap.find(sid);
            if (it != remap.end()) { keptItems[kv.first].push_back(it->second); }
        }
    }
    _seamPositions.swap(kept);
    _itemSeams.swap(keptItems);
    (void)model;
    return _seamPositions.size();
}

std::pair<std::size_t, std::size_t> SeamRegistry::verifyPreserved(
    osg::Node* proxy, const std::vector<std::uint32_t>& items) const
{
    std::vector<std::size_t> wanted;
    std::set<std::size_t> uniq;
    for (std::uint32_t id : items)
    {
        auto it = _itemSeams.find(id);
        if (it == _itemSeams.end()) { continue; }
        for (std::size_t sid : it->second)
        {
            if (uniq.insert(sid).second) { wanted.push_back(sid); }
        }
    }
    if (wanted.empty() || !proxy) { return {0, 0}; }

    // 收集 proxy 的所有顶点位置 -> key 集合
    std::set<std::int64_t> proxyKeys;
    struct Collector : public osg::NodeVisitor {
        Collector(std::set<std::int64_t>& s, const SeamRegistry* r)
            : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), keys(s), reg(r) {}
        void apply(osg::Drawable& d) override
        {
            osg::Geometry* g = d.asGeometry();
            if (!g) { return; }
            const osg::Vec3Array* pos = dynamic_cast<const osg::Vec3Array*>(g->getVertexArray());
            if (!pos) { return; }
            for (const osg::Vec3& p : *pos) { keys.insert(reg->key(osg::Vec3d(p))); }
        }
        std::set<std::int64_t>& keys;
        const SeamRegistry* reg;
    } c(proxyKeys, this);
    proxy->accept(c);

    std::size_t kept = 0;
    for (std::size_t sid : wanted)
    {
        // 容差内匹配：检查 27 个邻域 cell
        const osg::Vec3d& p = _seamPositions[sid];
        bool found = false;
        for (int dx = -1; dx <= 1 && !found; ++dx)
        {
            for (int dy = -1; dy <= 1 && !found; ++dy)
            {
                for (int dz = -1; dz <= 1 && !found; ++dz)
                {
                    const osg::Vec3d q(p.x() + dx * _tol, p.y() + dy * _tol, p.z() + dz * _tol);
                    if (proxyKeys.count(key(q))) { found = true; }
                }
            }
        }
        if (found) { ++kept; }
    }
    return {kept, wanted.size()};
}

// ---------------------------------------------------------------------------
// ProductionPipeline
// ---------------------------------------------------------------------------
ProductionPipeline::ProductionPipeline(const ProductionProfile& profile)
    : _profile(profile), _seams(1.0e-4)
{
}

std::vector<unsigned> ProductionPipeline::textureDownscaleLadder(unsigned textureMaxEdgePx)
{
    std::vector<unsigned> rungs;
    static const unsigned kCaps[] = { 256u, 128u, 64u };
    for (unsigned cap : kCaps)
    {
        if (cap < proxyTextureFloorEdgePx) { break; }
        // 代理首轮已按 textureMaxEdgePx 压过，比它更宽（或相等）的档位是重写浪费。
        // 判据只能用显式传入的本瓦片上限，不能用钳制后的 topts：叶瓦片不传上限，
        // topts 仍是全局 -tw/-th（2048），拿它比较会让阶梯对叶瓦片永远空转，
        // 等于悄悄废掉手册 7.3 的超限兜底。
        if (textureMaxEdgePx > 0 && cap >= textureMaxEdgePx) { continue; }
        rungs.push_back(cap);
    }
    return rungs;
}

ProductionPipeline::WriteOutcome
ProductionPipeline::writeContent(osg::Node* node, const std::string& relUri,
                                 bool isInstanceTile, unsigned textureMaxEdgePx,
                                 bool foldTextureTransform)
{
    WriteOutcome out;
    if (!node) { out.error = "null node"; return out; }

    osg::ref_ptr<osg::Node> work = node;
    // 实例瓦片的位姿只能待在 i3dm 特征表里：把变换折进几何会 (a) **就地**改写 N 个
    // 实例共享的同一份顶点数组（N 个矩阵连乘，A-41），(b) 让
    // `createFeatureI3DMTableBinary` 只能从被复位成单位阵的 MatrixTransform 里
    // decompose 出全 0 位姿。老路径 `I3DMTile::optimizeNode()` 正是为此把这几位
    // 优化选项剔掉的（src/3dtiles/I3DMTile.cpp:5-12），生产路径漏了这一步。
    unsigned optBits = _opt.gltfOptimizerOptions;
    if (isInstanceTile)
    {
        optBits &= ~GltfOptimizer::FLATTEN_TRANSFORMS;
        optBits &= ~GltfOptimizer::MERGE_TRANSFORMS;
        optBits &= ~GltfOptimizer::FLATTEN_STATIC_TRANSFORMS;
        optBits &= ~GltfOptimizer::FLATTEN_STATIC_TRANSFORMS_DUPLICATING_SHARED_SUBGRAPHS;
    }
    // 纹理 / 材质优化（图集 + KTX2）
    GltfOptimizer optimizer;
    GltfOptimizer::GltfTextureOptimizationOptions topts = _opt.textureOptions;
    if (textureMaxEdgePx > 0)
    {
        // 内节点代理（远景 LOD）：单张纹理边长压到 proxyTextureMaxEdgePx。
        // 本瓦片之外的瓦片绝不能被这次降分辨率带小：源模型里同一张贴图会被多个
        // 瓦片共用，而 GltfOptimizer 克隆 StateSet 只到 osg::Texture 层，
        // osg::Image 依旧是同一份（实测），所以隔离靠的是
        // GltfOptimizer::resizeImageToPowerOfTwo 的"改像素先私有化"（A-48）。
        topts.maxTextureWidth = std::min(topts.maxTextureWidth, static_cast<int>(textureMaxEdgePx));
        topts.maxTextureHeight = std::min(topts.maxTextureHeight, static_cast<int>(textureMaxEdgePx));
    }
    optimizer.setGltfTextureOptimizationOptions(topts);
    optimizer.optimize(work.get(), optBits);

    // A-51 方案 A：写出前把图集子矩形折进 TEXCOORD_0（只对代理开）。
    // uvBackup 跨整个 writeContent 存活：纹理超限阶梯会在同一个节点上再跑一次
    // 图集，那一轮必须看到原始 UV，所以每次 optimize 之前先还原、每次写盘之前重折。
    GltfOptimizer::TextureUvBackup uvBackup;
    // 折叠态不能活过本瓦片。图集克隆只到 osg::Texture 层，同一份几何/贴图和后续
    // 瓦片共用（见上面 A-48 注释），而图集装配判据正包含变换 enable 键与 |uv|>1
    // 平铺判断。折叠把 UV 压进 [0,1]、把键清零，若让它逃进下一个瓦片的 optimize()，
    // 那张本该独立导出的 REPEAT 贴图就会被错收进图集——产物级实测：60 个代理里有
    // 4 页图集换了内容、2 个根本没走折叠的叶瓦片跟着变了字节。
    struct FoldScopeGuard
    {
        GltfOptimizer::TextureUvBackup& backup;
        ~FoldScopeGuard() { GltfOptimizer::restoreFoldedTextureUvs(backup); }
    } foldScopeGuard{ uvBackup };
    const auto foldBeforeWrite = [&]() {
        if (!foldTextureTransform) { return; }
        const unsigned folded =
            GltfOptimizer::foldTextureTransformsIntoUv(work.get(), uvBackup);
        TRACE("foldTextureTransformsIntoUv folded=" << folded);
    };

    // 每个 tile 写出前重建 tile-local batch index（手册 5.5 第 4 点）
    BatchIdVisitor biv;
    work->accept(biv);
    out.batchLength = biv.getBatchLength();

    // 记录 Geode 顺序（与 BatchIdVisitor 编号顺序一致），用于 manifest
    {
        struct Order : public osg::NodeVisitor {
            Order(std::vector<std::string>& n) : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), names(n) {}
            void apply(osg::Geode& g) override { if (g.getNumDrawables()) { names.push_back(g.getName()); } traverse(g); }
            std::vector<std::string>& names;
        };
        std::vector<std::string> names;
        Order o(names);
        work->accept(o);
        for (const std::string& n : names)
        {
            // 聚合代理的 Geode 名是 proxy_cluster_<proxyFeatureId>，直接解出身份。
            // 落到下面的位置回退会把代理冒充成第 i 个真实源 feature（手册 8.1 禁止）。
            FeatureId found = INVALID_FEATURE;
            const std::string prefix = "proxy_cluster_";
            if (n.compare(0, prefix.size(), prefix) == 0)
            {
                found = static_cast<FeatureId>(std::strtoul(n.c_str() + prefix.size(), nullptr, 10));
            }
            else
            {
                for (const SourceObject& obj : _model.objects)
                {
                    if (obj.name == n) { found = obj.globalFeatureId; break; }
                }
            }
            out.featureOrder.push_back(found);
        }
        for (std::size_t i = 0; i < out.featureOrder.size(); ++i)
        {
            if (out.featureOrder[i] == INVALID_FEATURE)
            {
                out.featureOrder[i] = static_cast<FeatureId>(i);
            }
        }
    }

    Utils::TriangleCounterVisitor tcv;
    work->accept(tcv);
    out.triangles = tcv.count;
    if (out.triangles == 0) { out.error = "no triangles"; return out; }

    const std::string abs = _opt.stagingDir + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + relUri;
    const std::string dir = osgDB::getFilePath(abs);
    if (!dir.empty()) { osgDB::makeDirectory(dir); }
    foldBeforeWrite();
    if (!osgDB::writeNodeFile(*work.get(), abs, _opt.writeOptions))
    {
        out.error = "writeNodeFile failed: " + abs;
        return out;
    }
    out.bytes = 0;
    {
        std::ifstream f(Utils::utf8ToPath(abs), std::ios::binary | std::ios::ate);
        if (f.is_open()) { out.bytes = static_cast<std::uint64_t>(f.tellg()); }
    }

    // 纹理降采样兜底（手册 7.3 不允许超限瓦片）：分区估算含纹理字节后，
    // 仍有一类超限无法靠切分解决——不可分节点（单对象带几十个材质）。
    // 此时唯一可压的维度是纹理分辨率，逐级降采样重写直到达标或到下限。
    if (out.bytes > _profile.budget.maxTileContentBytes)
    {
        for (unsigned cap : textureDownscaleLadder(textureMaxEdgePx))
        {
            TRACE("rewriteWithSmallerTextures cap=" << cap << " curBytes=" << out.bytes);
            // 折叠态的 UV 绝不能进入下一轮图集：先装回原始 TEXCOORD_0，
            // 否则子矩形变换叠两次，而且 |uv|>1 闸门会把已折叠的 [0,1] UV 当成
            // 不平铺，把本该独立导出的 REPEAT 贴图错收进图集。
            GltfOptimizer::restoreFoldedTextureUvs(uvBackup);
            ClearTextureExportCacheVisitor clr;
            work->accept(clr);

            GltfOptimizer optimizer2;
            // 从首轮的实际档位继续折半（代理首轮已经被 proxyTextureMaxEdgePx 压过），
            // 图集尺寸一并跟上：图小了就无需大 atlas，否则 atlas 页数不降反升。
            GltfOptimizer::GltfTextureOptimizationOptions ladder = topts;
            ladder.maxTextureWidth = std::min(ladder.maxTextureWidth, static_cast<int>(cap));
            ladder.maxTextureHeight = std::min(ladder.maxTextureHeight, static_cast<int>(cap));
            ladder.maxTextureAtlasWidth = std::min(ladder.maxTextureAtlasWidth, static_cast<int>(cap));
            ladder.maxTextureAtlasHeight = std::min(ladder.maxTextureAtlasHeight, static_cast<int>(cap));
            optimizer2.setGltfTextureOptimizationOptions(ladder);
            optimizer2.optimize(work.get(), optBits);

            foldBeforeWrite();
            if (!osgDB::writeNodeFile(*work.get(), abs, _opt.writeOptions))
            {
                out.error = "writeNodeFile failed (texture rewrite): " + abs;
                return out;
            }
            out.bytes = 0;
            {
                std::ifstream f(Utils::utf8ToPath(abs), std::ios::binary | std::ios::ate);
                if (f.is_open()) { out.bytes = static_cast<std::uint64_t>(f.tellg()); }
            }
            if (out.bytes <= _profile.budget.maxTileContentBytes) { break; }
        }
    }

    // 说明：内节点代理的纹理在这里按 proxyTextureMaxEdgePx 降分辨率（见上面
    // textureMaxEdgePx），仍然超限则由上面的阶梯继续折半；剥掉整张纹理只在
    // processTileHeavy 里、构建阶段用私有 StateSet 做（写时剥离要改动与源模型
    // 共享的 StateSet，并行构建下会破坏引用计数，v8-v10 段错误根因），
    // 见 FarLodTextureStripper。
    out.ok = true;
    return out;
}

namespace {
// 定义在写产出校验之后（selfValidate 段），这里先声明：
// 写阶段也要按同样的口径读回 glTF JSON 判 primitive / 材质预算。
bool readU32LE(const std::string& buf, std::size_t off, std::uint32_t& out);
bool glbJsonRange(const std::string& bytes, std::size_t tileStart,
                  std::size_t& jsonOff, std::uint32_t& jsonLen);
void forEachGltfJsonChunk(const std::string& bytes,
                          const std::function<void(const std::string&)>& onJson);
}

// ---------------------------------------------------------------------------
// tile 间并行构建（性能优化）
//
// 原实现自底向上递归，每个节点串行做三件重活：materialize/代理简化、
// 双向 Hausdorff 误差评估、写出 b3dm。实测（lugouqiao 335MB）简化与
// 误差评估占绝大部分墙钟时间，且二者只读共享源数据、输出独立节点，
// 天然可并行；写出涉及纹理导出缓存与聚合统计，保持主线程串行。
// 三段式：预分配 id -> 线程池并行重活 -> 主线程按 id 顺序串行收尾。
// ---------------------------------------------------------------------------

// 递归预分配 tile id 与父子骨架。id 语义与旧串行版完全一致
// （先子后父，父 id = 最大子 id + 1），保证 tileset.json 的
// children 顺序与 t{id}.b3dm 命名和旧版产物一一对应。
int ProductionPipeline::assignTileIdsB3dm(int pn)
{
    const PartitionNode& P = _partitioner->nodes[pn];
    const int id = static_cast<int>(_tiles.size());
    _tiles.push_back(ProductionTile());
    _tiles[id].id = id;
    _tiles[id].isLeaf = P.children.empty();

    for (int c : P.children)
    {
        const int cid = assignTileIdsB3dm(c);
        _tiles[id].children.push_back(cid);
    }
    _pnToTileId[pn] = id;
    return id;
}

// 并行重活：几何构建（叶物化 / 代理简化）+ 误差评估。
// 只读 _model/_prims/_profile/_opt，输出独立节点与评估结果；
// 共享状态（_tiles/_mapper/_report/_failures/_warnings/_seams 写）一概不碰。
ProductionPipeline::TileHeavyWork
ProductionPipeline::processTileHeavy(int pn, int id)
{
    TileHeavyWork w;
    const Timer tHeavy;
    const CpuTimer cHeavy;
    const TileTaskScope taskScope;   // 让误差评估器知道现在有几个瓦片在并行抢核
    const PartitionNode& P = _partitioner->nodes[pn];
    std::vector<std::uint32_t> items;
    TRACE("buildB3dm pn=" << pn << " children=" << P.children.size());
    _partitioner->subtreeItems(pn, items);
    const bool isLeaf = P.children.empty();
    w.leaf = isLeaf;

    // 权威身份范围：只认源单元表，不认 Geode 名字（重名/聚合都会让名字回查失真）
    {
        std::set<FeatureId> fids;
        for (std::uint32_t itemId : items)
        {
            if (itemId >= _prims.size()) { continue; }
            const PrimRef& p = _prims[itemId];
            if (p.tris.empty()) { continue; }
            fids.insert(p.globalFeatureId);
        }
        w.sourceScope.assign(fids.begin(), fids.end());
    }

    if (isLeaf)
    {
        // 叶保留原始高精度几何，只允许坐标规范化与无损重建（手册 7.4）
        TRACE("leaf materialize pn=" << pn << " items=" << items.size());
        const Timer tMaterialize;
        const CpuTimer cMaterialize;
        w.content = _model.materialize(_prims, items, true);
        w.microMaterializeSec = tMaterialize.elapsed();
        w.microMaterializeCpuSec = cMaterialize.elapsed();
        TRACE("leaf materialize done");
    }
    else
    {
        ProxyMeshBuilder builder(_model, _profile, _prims);
        TRACE("proxy build pn=" << pn << " items=" << items.size());
        const Timer tProxyBuild;
        const CpuTimer cProxyBuild;
        const ProxyResult pr = builder.build(items, _profile.budget.targetTrianglesPerTile,
                                             !_profile.strictIdentity);
        w.microProxyBuildSec = tProxyBuild.elapsed();
        w.microProxyBuildCpuSec = cProxyBuild.elapsed();
        w.microProxySimplifySec = pr.metrics.simplifySeconds;
        w.microProxyAggregateSec = pr.metrics.aggregateSeconds;
        w.microAggregateCandidates = pr.metrics.aggregateCandidates;
        // 注意：聚合 fallback 耗尽后 pr.node 仍是最后一次聚合结果（有效），
        // failure 已置位。只检查 node.valid() 会让超限代理漏网到写出阶段
        // （lugouqiao_v11 实测 7 个瓦片 14~50 万三角形漏过 selfValidate 之前的
        // 全部检查，就是这个洞）。
        if (!pr.node.valid() || !pr.failure.empty())
        {
            w.failure = "proxy generation failed for node " + std::to_string(pn)
                      + ": " + (pr.failure.empty() ? "invalid proxy node" : pr.failure);
            w.microHeavySec = tHeavy.elapsed();
            w.microHeavyCpuSec = cHeavy.elapsed();
            return w;
        }
        w.content = pr.node;
        w.isProxy = true;
        w.aggregate = pr.aggregate;
        w.aggregateSources = pr.aggregateSources;

        // 内节点代理（远景 LOD）纹理策略：先降分辨率，装不下才去纹理。
        // 判据用 ProxyMeshBuilder 在**建好的代理节点**上量的去重估算：同一张
        // 纹理被 N 个 primitive 引用只算一次。旧实现在这里用
        // Σ PrimRef.byteEstimate 逐 primitive 累加纹理字节，共享纹理被重复计数，
        // longxiang 实测 42/42 代理被判超限 → 整片退化成无纹理灰模（顶层瓦片
        // 无贴图，视觉验收不接受）。
        // 降分辨率本身不在构建期做：代理的 StateSet 与源模型共享，构建期改图
        // 会波及其他 worker 正在读的原始模型。改为给写出阶段传
        // proxyTextureMaxEdgePx，由 GltfOptimizer 缩放——它克隆 StateSet 只到
        // osg::Texture 层，osg::Image 仍是共享的，所以隔离依赖
        // resizeImageToPowerOfTwo 的"改像素先私有化"（A-48），不是靠克隆本身。
        // 上限只有比全局 -tw/-th 更紧时才真的起作用：-ptw >= -tw 时它是空操作，
        // 把 60 个代理全记成"降过分辨率"会让报告说假话。
        const int globalTextureCap = std::min(_opt.textureOptions.maxTextureWidth,
                                              _opt.textureOptions.maxTextureHeight);
        w.texturesCappedPx = static_cast<int>(_profile.budget.proxyTextureMaxEdgePx) >= globalTextureCap
                                 ? 0u : _profile.budget.proxyTextureMaxEdgePx;
        if (!pr.fitsWithFloorTextures)
        {
            // 连下限分辨率都装不下（几何本身吃掉大半预算）：最后手段，
            // 构建时直接建成无纹理版，通过 w.texturesDropped 上报记 warning。
            TRACE("proxy far-lod detexture pn=" << pn
                  << " floor textures do not fit, textures at floor are the last resort");
            FarLodTextureStripper stripper;
            w.content->accept(stripper);
            w.texturesCappedPx = 0;
            w.texturesDropped = true;
        }
        else
        {
            TRACE("proxy far-lod texture cap pn=" << pn << " capPx=" << w.texturesCappedPx);
        }
    }

    if (!w.content.valid())
    {
        w.microHeavySec = tHeavy.elapsed();
        w.microHeavyCpuSec = cHeavy.elapsed();
        return w;
    }

    // ---- 几何误差评估（手册 9）----
    const Timer tCollect;
    const CpuTimer cCollect;
    std::vector<SourceModel::Triangle> srcTris;
    TRACE("collectTriangles pn=" << pn);
    _model.collectTriangles(_prims, items, srcTris);
    TRACE("collectTriangles done src=" << srcTris.size());
    TriangleSoup src, prox;
    for (const SourceModel::Triangle& t : srcTris) { src.add(t.a, t.b, t.c); }
    collectTrianglesFromNode(w.content.get(), prox);
    w.microCollectSec = tCollect.elapsed();
    w.microCollectCpuSec = cCollect.elapsed();
    if (isLeaf && !_opt.vertexCompression)
    {
        // 叶保留原始表面：只有确认没有任何有损变换时才可写 0
        w.measuredProxyErrorM = 0.0;
        w.errorUpperM = 0.0;
        w.errorLowerM = 0.0;
        w.errorStatus = "EXACT_LEAF";
    }
    else
    {
        GeometryErrorEvaluator ev(_profile.error);
        TRACE("evaluate pn=" << pn << " srcT=" << src.size() << " proxT=" << prox.size());
        const Timer tEval;
        const CpuTimer cEval;
        const ErrorResult er = ev.evaluate(src, prox);
        w.microEvaluateSec = tEval.elapsed();
        w.microEvaluateCpuSec = cEval.elapsed() + er.workerCpuSeconds;
        w.microEvaluateWorkerCpuSec = er.workerCpuSeconds;
        w.microEvaluateSamples = er.samplesUsed;
        w.microBvhBuildSec = er.bvhBuildSeconds;
        w.microEvalInitSec = er.initQuerySeconds;
        w.microEvalPushSec = er.pushSeconds;
        w.microEvalRefineSec = er.refineSeconds;
        w.microEvalQuantileSec = er.quantileSeconds;
        w.microEvalRefineIterations = er.refineIterations;
        w.microEvalSourceTriangles = er.sourceTriangles;
        w.microEvalProxyTriangles = er.proxyTriangles;
        w.microEvalDistanceCalls = er.distanceCalls;
        w.microEvalInitDistanceCalls = er.initDistanceCalls;
        w.microEvalRefineDistanceCalls = er.refineDistanceCalls;
        w.microEvalBvhNodeVisits = er.bvhNodeVisits;
        w.microEvalBvhTriangleTests = er.bvhTriangleTests;
        w.microEvalInitParallelSec = er.initParallelSeconds;
        w.microEvalInitThreadCreateSec = er.initThreadCreateSeconds;
        TRACE("evaluate done");
        w.measuredProxyErrorM = er.geometricErrorM();
        w.errorLowerM = er.lowerBoundM;
        w.errorUpperM = er.upperBoundM;
        w.errorStatus = er.status;
        w.certified = er.certified;
        w.boundLoose = er.boundLoose;
        w.p95M = er.p95M;
    }
    w.microHeavySec = tHeavy.elapsed();
    w.microHeavyCpuSec = cHeavy.elapsed();
    return w;
}

// 主线程串行收尾：写出 + 状态聚合 + seam 校验。
void ProductionPipeline::finalizeTileFromWork(int pn, int id, TileHeavyWork&& w)
{
    TRACE("finalize id=" << id << " begin pn=" << pn);
    const Timer tFinalize;
    const CpuTimer cFinalize;
    // 细分仪表：先落账，即使这个瓦片随后降级/失败
    _microHeavySec += w.microHeavySec;
    _microHeavyCpuSec += w.microHeavyCpuSec;
    _microMaterializeSec += w.microMaterializeSec;
    _microMaterializeCpuSec += w.microMaterializeCpuSec;
    _microProxyBuildSec += w.microProxyBuildSec;
    _microProxyBuildCpuSec += w.microProxyBuildCpuSec;
    _microProxySimplifySec += w.microProxySimplifySec;
    _microProxyAggregateSec += w.microProxyAggregateSec;
    _microAggregateCandidates += w.microAggregateCandidates;
    _microCollectSec += w.microCollectSec;
    _microCollectCpuSec += w.microCollectCpuSec;
    _microEvaluateSec += w.microEvaluateSec;
    _microEvaluateCpuSec += w.microEvaluateCpuSec;
    _microEvaluateWorkerCpuSec += w.microEvaluateWorkerCpuSec;
    _microEvaluateSamples += w.microEvaluateSamples;
    _microBvhBuildSec += w.microBvhBuildSec;
    _microEvalInitSec += w.microEvalInitSec;
    _microEvalPushSec += w.microEvalPushSec;
    _microEvalRefineSec += w.microEvalRefineSec;
    _microEvalQuantileSec += w.microEvalQuantileSec;
    _microEvalRefineIterations += w.microEvalRefineIterations;
    _microEvalSourceTriangles += w.microEvalSourceTriangles;
    _microEvalProxyTriangles += w.microEvalProxyTriangles;
    _microEvalDistanceCalls += w.microEvalDistanceCalls;
    _microEvalInitDistanceCalls += w.microEvalInitDistanceCalls;
    _microEvalRefineDistanceCalls += w.microEvalRefineDistanceCalls;
    _microEvalBvhNodeVisits += w.microEvalBvhNodeVisits;
    _microEvalBvhTriangleTests += w.microEvalBvhTriangleTests;
    _microEvalInitParallelSec += w.microEvalInitParallelSec;
    _microEvalInitThreadCreateSec += w.microEvalInitThreadCreateSec;
    // 逐瓦片一份（A-47）：阶段级 micro_* 只有总和，看不出关键路径在哪个瓦片上
    _tiles[id].timing.heavySec = w.microHeavySec;
    _tiles[id].timing.heavyCpuSec = w.microHeavyCpuSec;
    _tiles[id].timing.materializeSec = w.microMaterializeSec;
    _tiles[id].timing.materializeCpuSec = w.microMaterializeCpuSec;
    _tiles[id].timing.proxyBuildSec = w.microProxyBuildSec;
    _tiles[id].timing.proxyBuildCpuSec = w.microProxyBuildCpuSec;
    _tiles[id].timing.collectSec = w.microCollectSec;
    _tiles[id].timing.collectCpuSec = w.microCollectCpuSec;
    _tiles[id].timing.evaluateSec = w.microEvaluateSec;
    _tiles[id].timing.evaluateCpuSec = w.microEvaluateCpuSec;
    _tiles[id].timing.evaluateWorkerCpuSec = w.microEvaluateWorkerCpuSec;
    _tiles[id].timing.bvhBuildSec = w.microBvhBuildSec;
    _tiles[id].timing.evaluateSamples = w.microEvaluateSamples;
    _tiles[id].timing.evalInitSec = w.microEvalInitSec;
    _tiles[id].timing.evalPushSec = w.microEvalPushSec;
    _tiles[id].timing.evalRefineSec = w.microEvalRefineSec;
    _tiles[id].timing.evalQuantileSec = w.microEvalQuantileSec;
    _tiles[id].timing.evalRefineIterations = w.microEvalRefineIterations;
    _tiles[id].timing.evalSourceTriangles = w.microEvalSourceTriangles;
    _tiles[id].timing.evalProxyTriangles = w.microEvalProxyTriangles;
    // 身份与覆盖范围先落账：即使这个瓦片随后降级成"无内容"，它覆盖的源
    // feature 范围仍然要能被报告和自检看到（A-31 的"子树仍然覆盖"不能只
    // 活在 warning 文案里）。
    const std::size_t scopeCount = w.sourceScope.size();
    _tiles[id].isProxy = w.isProxy;
    _tiles[id].aggregateSources = std::move(w.aggregateSources);
    _tiles[id].sourceScope = std::move(w.sourceScope);

    if (!w.failure.empty())
    {
        // 叶失败 = 源 feature 真丢，判整个数据集失败（手册 14.17）。
        // 内节点代理失败不是丢数据：子树仍然覆盖同一 source scope，REPLACE 下
        // "无内容、直接细化"是合法瓦片。硬要发布一个超限远景代理违反手册 7.3，
        // 但因远景代理放不下就废掉整批数据（lugouqiao 335 MB 实测）也不是取舍。
        // 所以这里降级为 warning + 计数，让调用方看得见 LOD 空洞。
        if (w.leaf)
        {
            _failures.push_back(w.failure);
            _microFinalizeSec += tFinalize.elapsed();
            _microFinalizeCpuSec += cFinalize.elapsed();
            _tiles[id].timing.finalizeSec = tFinalize.elapsed();
            _tiles[id].timing.finalizeCpuSec = cFinalize.elapsed();
            return;
        }
        _warnings.push_back(w.failure + " -> tile carries no far-LOD content, children "
                            "still cover " + std::to_string(scopeCount)
                            + " source features");
        ++_report.proxySkippedCount;
        _microFinalizeSec += tFinalize.elapsed();
        _microFinalizeCpuSec += cFinalize.elapsed();
        _tiles[id].timing.finalizeSec = tFinalize.elapsed();
        _tiles[id].timing.finalizeCpuSec = cFinalize.elapsed();
        return;
    }

    const bool isLeaf = w.leaf;
    osg::ref_ptr<osg::Node> content = w.content;

    if (content.valid())
    {
        const std::string relUri = std::string("tiles") + OSG_GIS_PLUGINS_PATH_SPLIT_STRING
                                 + "t" + std::to_string(id) + ".b3dm";
        TRACE("writeContent pn=" << pn);
        const Timer tWrite;
        const CpuTimer cWrite;
        WriteOutcome wo = writeContent(content.get(), relUri, false, w.texturesCappedPx,
                                       !isLeaf);
        const double writeSec = tWrite.elapsed();
        _microWriteSec += writeSec;
        _microWriteCpuSec += cWrite.elapsed();
        _tiles[id].timing.writeSec = writeSec;
        _tiles[id].timing.writeCpuSec = cWrite.elapsed();
        TRACE("writeContent done");
        if (!wo.ok)
        {
            // 叶瓦片写出失败 = 这批源 feature 在整个数据集里彻底消失，
            // 只记 warning 会产出一个"看起来成功其实缺件"的数据集（手册 1.1）。
            // 代理写出失败不丢数据（子树仍然覆盖同一 source scope）。
            if (isLeaf && scopeCount > 0)
            {
                _failures.push_back("leaf tile " + std::to_string(id) + " write failed: "
                                    + wo.error + " (" + std::to_string(scopeCount)
                                    + " source features would be lost)");
            }
            else
            {
                _warnings.push_back("tile " + std::to_string(id) + ": " + wo.error);
            }
        }
        else
        {
            if (w.texturesDropped)
            {
                _warnings.push_back("tile " + std::to_string(id) +
                                    ": far-LOD proxy built without textures, even the "
                                    + std::to_string(proxyTextureFloorEdgePx)
                                    + "px texture floor does not fit budget (bytes=" +
                                    std::to_string(wo.bytes) + ")");
            }
            else if (w.texturesCappedPx > 0)
            {
                ++_report.proxyTextureCappedCount;
            }
            const bool overBytes = wo.bytes > _profile.budget.maxTileContentBytes;
            const bool overTris = wo.triangles > _profile.budget.maxTrianglesPerTile;
            // primitive / 材质预算必须按**实际导出的 glTF** 判，不能拿源 prim 数
            // 代替：内节点代理按 (feature, material) 物化，聚合代理再按 cell 复制
            // 宿主，保纹理后每个贴图又是独立材质（GltfMerger 按 世界矩阵×材质×
            // 属性集 分桶）。父节点的源负载**本来就会超预算**（它覆盖整个子树），
            // 真正受 24/24 约束的是导出宿主数：龙翔桥站厅 pn=9 源 prim=358/mat=332，
            // 导出 49/49。内节点超限沿用既有降级策略（删文件 → 无内容内节点，
            // 子树仍覆盖同一 source scope）；叶瓦片不做这里降级，交给 selfValidate
            // 失败关闭——叶缺件不可接受。
            unsigned contentPrims = 0, contentMats = 0;
            bool parsedContent = false;
            if (!isLeaf)
            {
                const std::string absContent = _opt.stagingDir
                    + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + relUri;
                std::ifstream cf(Utils::utf8ToPath(absContent), std::ios::binary);
                if (cf.is_open())
                {
                    std::string body((std::istreambuf_iterator<char>(cf)),
                                     std::istreambuf_iterator<char>());
                    forEachGltfJsonChunk(body, [&](const std::string& gltfJson) {
                        try
                        {
                            const json gltf = json::parse(gltfJson);
                            if (gltf.contains("meshes") && gltf["meshes"].is_array())
                            {
                                for (const json& m : gltf["meshes"])
                                {
                                    if (m.contains("primitives") && m["primitives"].is_array())
                                    {
                                        contentPrims += static_cast<unsigned>(m["primitives"].size());
                                    }
                                }
                            }
                            if (gltf.contains("materials") && gltf["materials"].is_array())
                            {
                                contentMats += static_cast<unsigned>(gltf["materials"].size());
                            }
                            parsedContent = true;
                        }
                        catch (const std::exception&)
                        {
                            parsedContent = false;
                        }
                    });
                }
            }
            const bool overPrims = parsedContent
                && contentPrims > _profile.budget.maxPrimitivesPerTile;
            const bool overMats = parsedContent
                && contentMats > _profile.budget.maxMaterialsPerTile;
            if (overBytes || overTris || overPrims || overMats)
            {
                std::vector<std::string> over;
                if (overBytes)
                {
                    over.push_back("bytes=" + std::to_string(wo.bytes) + " > "
                        + std::to_string(_profile.budget.maxTileContentBytes));
                }
                if (overTris)
                {
                    over.push_back("triangles=" + std::to_string(wo.triangles) + " > "
                        + std::to_string(_profile.budget.maxTrianglesPerTile));
                }
                if (overPrims)
                {
                    over.push_back("primitives=" + std::to_string(contentPrims) + " > "
                        + std::to_string(_profile.budget.maxPrimitivesPerTile));
                }
                if (overMats)
                {
                    over.push_back("materials=" + std::to_string(contentMats) + " > "
                        + std::to_string(_profile.budget.maxMaterialsPerTile));
                }
                std::string why;
                for (const std::string& o : over)
                {
                    if (!why.empty()) { why += ", "; }
                    why += o;
                }
                if (isLeaf)
                {
                    // 手册 7.3：不允许输出超限瓦片；叶超限 = 分区器没能把它切开，
                    // 删掉又等于丢源 feature，只能判失败（14.17 失败关闭）。
                    _failures.push_back("tile " + std::to_string(id) + " content " + why
                                        + " (bytes=" + std::to_string(wo.bytes)
                                        + ", triangles=" + std::to_string(wo.triangles) + ")");
                    _tiles[id].hasContent = false;
                }
                else
                {
                    // 内节点超限只是"这一层远景代理没做好"，子树仍覆盖同一
                    // source scope：删掉刚写进暂存区的超限文件，退化为无内容
                    // 内节点（A-31），既不违反 7.3 也不报废整批数据。
                    const std::string abs = _opt.stagingDir + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + relUri;
                    std::error_code ec;
                    std::filesystem::remove(Utils::utf8ToPath(abs), ec);
                    _warnings.push_back("tile " + std::to_string(id) + ": proxy content " + why
                                        + " -> over-budget far-LOD file removed, children still cover "
                                        + std::to_string(scopeCount) + " source features"
                                        + (ec ? " (file removal failed: " + ec.message() + ")" : ""));
                    ++_report.proxySkippedCount;
                    _tiles[id].hasContent = false;
                }
            }
            else
            {
                _tiles[id].hasContent = true;
                _tiles[id].uri = std::string("tiles/") + "t" + std::to_string(id) + ".b3dm";
                _tiles[id].contentBytes = wo.bytes;
                _tiles[id].triangles = wo.triangles;
                _tiles[id].featureOrder = wo.featureOrder;

                osg::ComputeBoundsVisitor cbv;
                content->accept(cbv);
                _tiles[id].contentBounds = cbv.getBoundingBox();

                _mapper.addTile(_tiles[id].uri, wo.featureOrder, w.isProxy);
                _report.maxContentBytes = std::max(_report.maxContentBytes, wo.bytes);
                _report.maxTrianglesInTile = std::max(_report.maxTrianglesInTile, wo.triangles);
            }
        }
    }
    else if (!isLeaf)
    {
        _warnings.push_back("internal node " + std::to_string(pn) + " has no proxy content; "
                            "children still cover the source scope");
    }

    _tiles[id].aggregate = w.aggregate;
    if (w.aggregate) { ++_report.aggregateProxyCount; }
    TRACE("finalize id=" << id << " state done");

    // 误差/统计聚合并入 tile 状态（与旧串行版一致：只有 hasContent 的
    // 瓦片才携带评估结果；超限/写出失败的瓦片误差字段保持默认值）
    if (_tiles[id].hasContent)
    {
        _tiles[id].measuredProxyErrorM = w.measuredProxyErrorM;
        _tiles[id].errorLowerM = w.errorLowerM;
        _tiles[id].errorUpperM = w.errorUpperM;
        _tiles[id].errorStatus = w.errorStatus;
        if (w.certified) { ++_report.nodesCertified; }
        if (w.boundLoose) { ++_report.nodesBoundLoose; }
        if (w.p95M > 0) { _report.errorP95 = std::max(_report.errorP95, w.p95M); }

        // 接缝保留校验（手册 8.3）
        if (w.isProxy && _seams.seamVertexCount() > 0)
        {
            std::vector<std::uint32_t> items;
            _partitioner->subtreeItems(pn, items);
            const std::pair<std::size_t, std::size_t> keep = _seams.verifyPreserved(content.get(), items);
            if (keep.second > 0 && keep.first < keep.second)
            {
                _warnings.push_back("tile " + std::to_string(id) + ": seam vertices preserved "
                                    + std::to_string(keep.first) + "/" + std::to_string(keep.second));
            }
        }
    }
    const double finalizeSec = tFinalize.elapsed();
    _microFinalizeSec += finalizeSec;
    _microFinalizeCpuSec += cFinalize.elapsed();
    _tiles[id].timing.finalizeSec = finalizeSec;
    _tiles[id].timing.finalizeCpuSec = cFinalize.elapsed();
    TRACE("finalize id=" << id << " end");
}

int ProductionPipeline::buildB3dmTree(int rootPn)
{
    // 阶段 1：主线程预分配 id/骨架（快，纯结构遍历）
    const int rootId = assignTileIdsB3dm(rootPn);

    // pn -> id 反查
    std::vector<int> idToPn(_tiles.size(), -1);
    for (const auto& kv : _pnToTileId) { idToPn[kv.second] = kv.first; }

    // 阶段 2/3：滑动窗口流水线。
    // 并行度：WB_JOBS 环境变量可覆盖；默认 8（过大只会推高内存峰值，
    // 24 核机器实测 8 已足够喂饱 IO 与单线程 meshopt）。
    unsigned jobs = 8;
    if (const char* j = std::getenv("WB_JOBS")) { jobs = static_cast<unsigned>(std::atoi(j)); }
    if (jobs == 0) { jobs = 1; }
    const std::size_t window = static_cast<std::size_t>(jobs) * 2;

    // 按工作量（子树三角形数）降序提交：最大的任务最先开跑，
    // 避免收尾阶段只剩一个巨型任务在跑（straggler）。
    std::vector<int> pns;
    pns.reserve(_pnToTileId.size());
    for (const auto& kv : _pnToTileId) { pns.push_back(kv.first); }
    std::sort(pns.begin(), pns.end(), [this](int a, int b) {
        return _partitioner->nodes[a].triangles > _partitioner->nodes[b].triangles;
    });

    TRACE("parallel build jobs=" << jobs << " tiles=" << pns.size());

    std::size_t next = 0;
    std::map<int, std::future<TileHeavyWork>> inflight;  // key = pn
    std::map<int, TileHeavyWork> ready;                  // key = tile id
    auto submit = [this, &inflight, &idToPn](int pn) {
        const int id = idToPn[pn];
        inflight.emplace(pn, std::async(std::launch::async, [this, pn, id]() {
            return processTileHeavy(pn, id);
        }));
    };
    // 在制品总量 = 正在算的 + 算完等写出的，恒不超过窗口大小。
    // 只按 inflight 计窗口的话，乱序收割会把已完成瓦片的中间产物（物化节点、
    // 三角汤）无限堆在 ready 里，等一个大瓦片的同时内存能涨到整个模型量级。
    auto refill = [&]() {
        while (inflight.size() + ready.size() < window && next < pns.size())
        {
            submit(pns[next++]);
        }
    };
    // 乱序收割：把已经算完的任务挪出 inflight，腾出窗口槽位。
    // 必须及时收割，否则已完成的 future 仍占着槽位，worker 空转、
    // 并发度随收割进度一路掉下去（v18 实测 8 job 平均只吃到 3.4 核）。
    auto harvestReady = [&]() {
        std::size_t moved = 0;
        for (auto it = inflight.begin(); it != inflight.end(); )
        {
            if (it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                ready.emplace(idToPn[it->first], it->second.get());
                it = inflight.erase(it);
                ++moved;
            }
            else { ++it; }
        }
        return moved;
    };
    refill();

    // 主线程按 tile id 升序写出，保证与串行版完全一致的
    // _mapper.addTile 顺序、report 聚合顺序与 staging 文件名。
    // 注意：提交顺序按工作量降序，而收集按 id 升序——id 靠前的小任务
    // 可能还没进窗口，此时主线程直接同步执行它（窗口任务照常并行），
    // 绝不能解引用 end() 迭代器。
    for (int id = 0; id < static_cast<int>(_tiles.size()); ++id)
    {
        const int pn = idToPn[id];
        while (ready.find(id) == ready.end())
        {
            if (inflight.find(pn) == inflight.end())
            {
                ready.emplace(id, processTileHeavy(pn, id));
                break;
            }
            if (harvestReady() > 0) { refill(); continue; }
            auto it = inflight.find(pn);
            if (it != inflight.end()) { it->second.wait_for(std::chrono::milliseconds(20)); }
        }
        auto rit = ready.find(id);
        TileHeavyWork w = std::move(rit->second);
        ready.erase(rit);
        finalizeTileFromWork(pn, id, std::move(w));
        refill();
    }
    return rootId;
}

int ProductionPipeline::buildInstanceTree()
{
    if (_model.instances.empty()) { return -1; }

    // 按原型分组，再按实例世界位置做空间分块
    std::map<FeatureId, std::vector<const SourceInstance*>> byProto;
    for (const SourceInstance& inst : _model.instances)
    {
        byProto[inst.prototypeFeatureId].push_back(&inst);
    }

    const int rootId = static_cast<int>(_tiles.size());
    _tiles.push_back(ProductionTile());
    _tiles[rootId].id = rootId;

    for (auto& kv : byProto)
    {
        const FeatureId proto = kv.first;
        const std::vector<const SourceInstance*>& insts = kv.second;
        // 原型几何（局部坐标）：同一原型的实例在 glTF 里共用一份 accessor
        const SourceObject* protoObj = _model.objectOf(proto);
        if (!protoObj) { continue; }
        std::vector<std::uint32_t> protoItems;
        for (std::uint32_t i = 0; i < _model.prims.size(); ++i)
        {
            if (_model.prims[i].globalFeatureId == proto) { protoItems.push_back(i); }
        }
        if (protoItems.empty()) { continue; }
        osg::ref_ptr<osg::Node> protoGeom = _model.materialize(protoItems, true);
        if (!protoGeom.valid()) { continue; }

        // i3dm 的 accessor 只存一份，但运行时按实例数展开绘制：
        // 真实三角形数 = 实例数 x 原型三角形数。分块只看实例个数会让
        // "低模原型 x 512 个实例"直接冲过 12 万硬上限（实测 lugouqiao：
        // 字节没超、三角面 507822），写盘前必须按原型体积收敛块大小。
        const unsigned protoTris = ProxyMeshBuilder::countTriangles(protoGeom.get());
        if (protoTris > _profile.budget.maxTrianglesPerTile)
        {
            _failures.push_back("prototype " + std::to_string(proto) + " has "
                                + std::to_string(protoTris) + " triangles, over hard limit "
                                + std::to_string(_profile.budget.maxTrianglesPerTile)
                                + " even as a single instance");
            continue;
        }
        unsigned chunk = _profile.budget.maxInstancesPerTile;
        if (protoTris > 0)
        {
            chunk = std::min(chunk, _profile.budget.maxTrianglesPerTile / protoTris);
        }

        // 原型顶点在 `SourceModel` 里已按世界矩阵烘好（SourceModel.cpp:356
        // `w = p * f.worldMatrix`），而 `SourceInstance::matrix` 存的是实例**自己**的
        // 世界矩阵（SourceModel.cpp:557）。i3dm 特征表里的矩阵是直接乘在 GLB 几何上的，
        // 所以必须是"相对原型"的那一个：world_baked · (inverse(protoWorld) · instWorld)
        // = local · instWorld。直接用 instWorld 会把世界变换乘两遍。
        osg::Matrixd pw = protoObj->worldMatrix;
        osg::Matrixd invProto;
        // osg::Matrixd::invert 是 `this = inverse(rhs)`，不是 `rhs -> 写入 this`
        // 的 const 版本：方向写反时 invProto 会停在单位阵上，实例位姿静默变成
        // 绝对世界平移（= 双变换），且不会报错。
        if (!invProto.invert(pw))
        {
            _failures.push_back("prototype " + std::to_string(proto)
                                + " has a non-invertible world matrix; instance placement"
                                  " cannot be expressed relative to the baked prototype");
            continue;
        }

        for (std::size_t start = 0; start < insts.size(); start += chunk)
        {
            const std::size_t end = std::min(insts.size(), start + chunk);
            osg::ref_ptr<osg::Group> group = new osg::Group;
            for (std::size_t k = start; k < end; ++k)
            {
                osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform;
                mt->setMatrix(invProto * insts[k]->matrix);
                // 每个实例一份 Geode（保留实例身份与属性）
                osg::ref_ptr<osg::Geode> g = new osg::Geode;
                for (unsigned c = 0; c < protoGeom->asGroup()->getNumChildren(); ++c)
                {
                    osg::Node* child = protoGeom->asGroup()->getChild(c);
                    if (child->asGeode())
                    {
                        for (unsigned d = 0; d < child->asGeode()->getNumDrawables(); ++d)
                        {
                            g->addDrawable(child->asGeode()->getDrawable(d));
                        }
                    }
                }
                g->setName(insts[k]->name);
                mt->addChild(g.get());
                group->addChild(mt.get());
            }

            const int childId = static_cast<int>(_tiles.size());
            _tiles.push_back(ProductionTile());
            _tiles[childId].id = childId;
            const std::string relUri = std::string("tiles") + OSG_GIS_PLUGINS_PATH_SPLIT_STRING
                                     + "i" + std::to_string(childId) + ".i3dm";
            const Timer tInstWrite;
            const CpuTimer cInstWrite;
            WriteOutcome wo = writeContent(group.get(), relUri, true);
            const double instWriteSec = tInstWrite.elapsed();
            _microWriteSec += instWriteSec;
            _microWriteCpuSec += cInstWrite.elapsed();
            _tiles[childId].timing.writeSec = instWriteSec;
            _tiles[childId].timing.writeCpuSec = cInstWrite.elapsed();
            const bool overBytes = wo.bytes > _profile.budget.maxTileContentBytes;
            const bool overTris = wo.triangles > _profile.budget.maxTrianglesPerTile;
            if (wo.ok && !overBytes && !overTris)
            {
                _tiles[childId].hasContent = true;
                _tiles[childId].uri = std::string("tiles/") + "i" + std::to_string(childId) + ".i3dm";
                _tiles[childId].contentBytes = wo.bytes;
                _tiles[childId].triangles = wo.triangles;
                _tiles[childId].isLeaf = true;
                osg::ComputeBoundsVisitor cbv;
                group->accept(cbv);
                _tiles[childId].contentBounds = cbv.getBoundingBox();
                std::vector<std::uint32_t> ids;
                for (std::size_t k = start; k < end; ++k) { ids.push_back(insts[k]->instanceId); }
                _mapper.addInstanceTile(_tiles[childId].uri, {proto}, ids);
                ++_report.instanceTileCount;
                _report.instanceCount += static_cast<unsigned>(end - start);
                _tiles[rootId].children.push_back(childId);
                _report.maxContentBytes = std::max(_report.maxContentBytes, wo.bytes);
                _report.maxTrianglesInTile = std::max(_report.maxTrianglesInTile, wo.triangles);
            }
            else if (wo.ok)
            {
                // 实例瓦片是叶子：删掉等于丢实例，只能显式失败关闭发布（手册 7.3 / 14.17）
                const std::string abs = _opt.stagingDir + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + relUri;
                std::error_code ec;
                std::filesystem::remove(Utils::utf8ToPath(abs), ec);
                _failures.push_back("instance tile " + std::to_string(childId)
                                    + " over hard budget: "
                                    + std::to_string(wo.bytes) + " bytes / "
                                    + std::to_string(wo.triangles) + " triangles ("
                                    + std::to_string(end - start) + " x prototype "
                                    + std::to_string(protoTris) + " triangles), limit "
                                    + std::to_string(_profile.budget.maxTileContentBytes)
                                    + " bytes / "
                                    + std::to_string(_profile.budget.maxTrianglesPerTile)
                                    + " triangles");
            }
            else
            {
                _failures.push_back("instance tile " + std::to_string(childId) + ": " + wo.error);
            }
        }
    }
    return rootId;
}

void ProductionPipeline::aggregateBounds(int tile)
{
    if (tile < 0 || tile >= static_cast<int>(_tiles.size())) { return; }
    for (int c : _tiles[tile].children) { aggregateBounds(c); }
    osg::BoundingBox bb = _tiles[tile].contentBounds;
    for (int c : _tiles[tile].children)
    {
        bb.expandBy(_tiles[c].declaredBounds.valid() ? _tiles[c].declaredBounds : _tiles[c].contentBounds);
    }
    _tiles[tile].subtreeBounds = bb;
    // REPLACE 树：父包围体必须同时包住自己的代理内容和所有后代范围（手册 12.1）
    _tiles[tile].declaredBounds = bb;
}

void ProductionPipeline::assignGeometricError(int tile)
{
    if (tile < 0 || tile >= static_cast<int>(_tiles.size())) { return; }
    double maxChild = 0.0;
    for (int c : _tiles[tile].children)
    {
        assignGeometricError(c);
        maxChild = std::max(maxChild, _tiles[c].geometricErrorM);
    }
    // G(n) = max(measuredProxyErrorUpper(n), max(G(child)))  —— 手册 9.6
    // 不加任何 epsilon；父级自然 >= 子级
    _tiles[tile].geometricErrorM = std::max(_tiles[tile].measuredProxyErrorM, maxChild);
}

bool ProductionPipeline::run(osg::Node* input, const Options& opt)
{
    Timer total;
    // run() 之前发生了什么（readNodeFile / DLL 初始化 / 参数解析）只有进程读数知道
    const ProcessTimes ptBeforeRun = currentProcessTimes();
    _opt = opt;
    // 暂存目录：<outputDir>/.wb-staging（可在 Options 里覆盖）
    if (_opt.stagingDir.empty())
    {
        // 名字带 pid：两个进程跑同一个输出目录时不会共用一个暂存区
        _opt.stagingDir = _opt.outputDir + OSG_GIS_PLUGINS_PATH_SPLIT_STRING
                        + ".wb-staging-" + std::to_string(currentProcessId());
    }
    {
        // 先清扫上一次被强杀留下的暂存目录（只清"属主已死"的），再建自己的
        const std::filesystem::path outDir = Utils::utf8ToPath(_opt.outputDir);
        const std::filesystem::path mine = Utils::utf8ToPath(_opt.stagingDir);
        sweepStaleStagingDirs(outDir, mine);
        std::error_code ecMake;
        std::filesystem::create_directories(mine, ecMake);
        writeOwnerPid(mine);
    }
    osgDB::makeDirectory(_opt.stagingDir);
    try
    {
        _profile.validate();
    }
    catch (const std::exception& e)
    {
        _failures.push_back(std::string("profile: ") + e.what());
        return false;
    }

    // 报告里的 profile 必须是**生效**的那份：ConversionReport::profile 默认是
    // 硬编码缺省值，不赋值的话 CLI 覆盖（-tol/-ptw/预算）在产物里一律显示成缺省，
    // 验收时无法从 conversion-report.json 判断这批产物到底用了什么参数。
    _report.profile = _profile;

    // 报告身份字段（AUDIT A-58）：这四格以前从不赋值，产物里的 conversion-report.json
    // 恒为 inputFile="" / outputDir="" / inputBytes=0 / outputBytes=0，"0"看起来像
    // "测过并且真的是 0"。取不到就在 warnings 里点名，不静默留 0。
    _report.inputFile = _opt.sourceFile;
    _report.outputDir = _opt.outputDir;
    if (_opt.sourceFile.empty())
    {
        _warnings.push_back("report identity: caller did not pass the source path; "
                            "inputFile/inputBytes are unavailable (not measured zero)");
    }
    else
    {
        std::error_code ec_input;
        const auto sz = std::filesystem::file_size(Utils::utf8ToPath(_opt.sourceFile), ec_input);
        if (ec_input)
        {
            _warnings.push_back("report identity: cannot stat source file " + _opt.sourceFile
                                + " (" + ec_input.message() + "); inputBytes is unavailable");
        }
        else
        {
            _report.inputBytes = static_cast<std::uint64_t>(sz);
        }
    }

    // ---- import / normalize ----
    Timer tImport;
    CpuTimer cImport;
    TRACE("model.build");
    // 先不急着因"没几何"退出：build() 失败最常见的原因就是所有 drawable 都被
    // 坐标有效性检查丢掉了。若在这里直接 return，用户只会看到一句
    // "no geometry"，而"哪个节点、哪条 drawable、丢了几个三角形"全在
    // _model.nonFiniteGeometry 里没被落账。所以先记账，再判失败。
    const bool modelBuilt = _model.build(input);
    _report.addStage("import_normalize", tImport.elapsed());
    _report.addStage("import_normalize_cpu", cImport.elapsed());
    _report.sourceFeatureCount = _model.featureCount();

    // 读模阶段丢弃的越界三角形必须出现在报告里（约束 §2.4：不得静默丢弃源对象；
    // 钳位或伪造几何同样禁止）。逐条明细只列前若干条，避免畸形输入把报告撑爆。
    if (!_model.indexViolations.empty())
    {
        std::uint64_t droppedTris = 0;
        std::set<FeatureId> affected;
        for (const IndexViolation& v : _model.indexViolations)
        {
            droppedTris += v.droppedTriangles;
            affected.insert(v.globalFeatureId);
        }
        _report.addCounter("outOfRangeIndexTriangles", droppedTris);
        _report.addCounter("outOfRangeIndexPrimitiveSets", _model.indexViolations.size());
        _report.addCounter("outOfRangeIndexFeatures", affected.size());
        std::ostringstream ids;
        std::size_t listed = 0;
        for (const FeatureId fid : affected)
        {
            if (listed == 16) { ids << ",+" << (affected.size() - listed) << " more"; break; }
            ids << (listed ? "," : "") << fid;
            ++listed;
        }
        _warnings.push_back("index validation: dropped " + std::to_string(droppedTris)
            + " triangle(s) referencing a vertex index beyond the array, in "
            + std::to_string(_model.indexViolations.size()) + " primitive set(s) of "
            + std::to_string(affected.size()) + " feature(s) [" + ids.str()
            + "]; input geometry is malformed and was neither clamped nor rebuilt");
        for (std::size_t i = 0; i < std::min<std::size_t>(10, _model.indexViolations.size()); ++i)
        {
            const IndexViolation& v = _model.indexViolations[i];
            _warnings.push_back("index validation: node '" + v.nodeName + "' path '" + v.nodePath
                + "' drawable " + std::to_string(v.drawableIndex) + " primitiveSet "
                + std::to_string(v.primitiveSetIndex) + ": dropped "
                + std::to_string(v.droppedTriangles) + " triangle(s), vertexCount="
                + std::to_string(v.vertexCount) + " maxOffendingIndex="
                + std::to_string(v.maxOffendingIndex));
        }
        if (_model.indexViolations.size() > 10)
        {
            _warnings.push_back("index validation: ... and "
                + std::to_string(_model.indexViolations.size() - 10) + " more primitive set(s)");
        }
    }

    // 坐标有效性的账。语义（本轮从"只发 warning"改成**失败关闭**）：
    // 一条 drawable 里只要有一个不可表示的顶点，整条就被丢掉——这是无法局部修补的
    // 数据缺陷。丢掉之后**没有人能判断"剩下的还够不够"**：它可能是 1/3000 的装饰件，
    // 也可能就是这个模型的主体。把这种不确定性包装成 warning 后照常发布，等于交付一个
    // "看起来完整、实际少了一块"的数据集（手册 14.17 失败关闭、7.3 不出超限/残缺产物）。
    // 所以这里同时记 counters + warnings（诊断用）**并置位 failure**：管线返回 false，
    // CLI 走 rc=3、丢弃暂存目录、不提交任何正式输出。用户要么修源数据，要么先把
    // 问题 drawable 单独摘出去再来一遍——而不是拿到一个悄悄缺件的产物。
    // 法线非有限不算失败：那是置零后交给 A-43 重建的，几何与拓扑没变。
    if (!_model.nonFiniteGeometry.empty())
    {
        std::uint64_t droppedTris = 0, nonFiniteDoubles = 0, floatOverflows = 0;
        for (const NonFiniteGeometry& g : _model.nonFiniteGeometry)
        {
            droppedTris += g.droppedTriangles;
            nonFiniteDoubles += g.nonFiniteDoubles;
            floatOverflows += g.floatOverflows;
        }
        _report.addCounter("nonFiniteDrawables", _model.nonFiniteGeometry.size());
        _report.addCounter("nonFiniteDroppedTriangles", droppedTris);
        _report.addCounter("nonFiniteVertexCount", nonFiniteDoubles);
        _report.addCounter("floatOverflowVertexCount", floatOverflows);
        _warnings.push_back("coordinate validity: dropped " + std::to_string(_model.nonFiniteGeometry.size())
            + " drawable(s) / " + std::to_string(droppedTris) + " triangle(s) whose vertex positions are "
            + "not representable: " + std::to_string(nonFiniteDoubles) + " non-finite after the world "
            + "transform, " + std::to_string(floatOverflows) + " finite in double but overflowing float; "
            + "nothing was clamped, moved or rebuilt");
        // 失败项必须点名到节点/drawable/丢失量，否则用户无法定位要修哪一块
        _failures.push_back("coordinate validity: " + std::to_string(_model.nonFiniteGeometry.size())
            + " drawable(s) dropped (" + std::to_string(droppedTris) + " triangle(s), "
            + std::to_string(nonFiniteDoubles) + " non-finite + " + std::to_string(floatOverflows)
            + " float-overflow vertex position(s)); refusing to publish a dataset that is silently "
            + "missing geometry (first: node '" + _model.nonFiniteGeometry.front().nodeName
            + "', drawable " + std::to_string(_model.nonFiniteGeometry.front().drawableIndex) + ")");
        for (std::size_t i = 0; i < std::min<std::size_t>(10, _model.nonFiniteGeometry.size()); ++i)
        {
            const NonFiniteGeometry& g = _model.nonFiniteGeometry[i];
            _warnings.push_back("coordinate validity: node '" + g.nodeName + "' path '" + g.nodePath
                + "' drawable " + std::to_string(g.drawableIndex) + ": vertices="
                + std::to_string(g.vertexCount) + " nonFinite=" + std::to_string(g.nonFiniteDoubles)
                + " floatOverflow=" + std::to_string(g.floatOverflows) + " trianglesLost="
                + std::to_string(g.droppedTriangles));
        }
        if (_model.nonFiniteGeometry.size() > 10)
        {
            _warnings.push_back("coordinate validity: ... and "
                + std::to_string(_model.nonFiniteGeometry.size() - 10) + " more drawable(s)");
        }
        // 立即收工，不再往下切分/建代理/写盘：这一批数据注定发布不了，
        // 让 335 MB 的模型先跑两小时再被 finalize 拒掉是纯粹的浪费。
        return false;
    }
    if (_model.nonFiniteNormalsZeroed > 0)
    {
        _report.addCounter("nonFiniteNormalsZeroed", _model.nonFiniteNormalsZeroed);
        _warnings.push_back("coordinate validity: " + std::to_string(_model.nonFiniteNormalsZeroed)
            + " normal(s) were non-finite after transform/normalize/float cast; they were zeroed and "
            + "handed to the zero-normal rebuild (A-43) instead of being written as NaN");
    }

    if (!modelBuilt)
    {
        // 坐标记账已经在上面落过（如果就是这个原因）；这里补一条总口径
        _failures.push_back("SourceModel build failed (no geometry)"
            + std::string(_model.nonFiniteGeometry.empty() ? "" :
                          " - all drawables were dropped by the coordinate-validity check"));
        return false;
    }

    // 原始 OBJ 索引预检的结果必须进报告（AUDIT A-57）。**干净文件也要落计数**：
    // 否则"没扫过"和"扫过且没问题"在 conversion-report.json 里长得一模一样，
    // 与 A-58 那条"0 得像测出来的"是同一个坑。
    if (_opt.objPrecheck.ran)
    {
        const ObjPrecheckResult& pre = _opt.objPrecheck;
        _report.addStage("obj_precheck", pre.seconds);
        _report.addCounter("objPrecheckFaces", pre.faces);
        _report.addCounter("objPrecheckCheckedRefs", pre.checkedRefs);
        _report.addCounter("objPrecheckBadFaces", pre.badFaces);
        _report.addCounter("objPrecheckViolations", pre.hardViolations);
        _report.addCounter("objPrecheckForwardRefs", pre.forwardRefs);
        if (!pre.readable)
        {
            _warnings.push_back("obj precheck: the source file could not be opened for "
                                "scanning; face index coverage is unknown (not \"clean\")");
        }
        else if (pre.hardViolations != 0 || pre.pendingOverflow)
        {
            // warn 模式走到这里：几何照读者的样子转换，但报告必须点名——
            // 读模后那道守卫看不见这类越界（reader 吸收了），这是唯一的记账点。
            _warnings.push_back("obj precheck: " + std::to_string(pre.hardViolations)
                + " invalid face reference(s) across " + std::to_string(pre.badFaces)
                + " face(s) of " + std::to_string(pre.faces) + " scanned in "
                + std::to_string(pre.lines) + " line(s); the OBJ reader absorbs them into legal "
                "local indices, so the post-read index guard cannot see them and geometry may "
                "carry garbage vertices"
                + (pre.pendingOverflow ? "; scan stopped early (pending-overflow)" : ""));
            const std::vector<std::string> details = pre.messages();
            for (std::size_t i = 0; i < std::min<std::size_t>(10, details.size()); ++i)
            {
                _warnings.push_back("obj precheck: " + details[i]);
            }
            if (details.size() > 10)
            {
                _warnings.push_back("obj precheck: ... and "
                    + std::to_string(details.size() - 10) + " more reference(s)");
            }
        }
    }

    Timer tSeam;
    CpuTimer cSeam;
    TRACE("seams");
    _seams.build(_model);
    _report.addStage("seam_registry", tSeam.elapsed());
    _report.addStage("seam_registry_cpu", cSeam.elapsed());

    // ---- partition ----
    Timer tPart;
    CpuTimer cPart;
    TRACE("partition");
    _prims = _model.prims;
    _partitioner = new SpatialPartitioner(_model, _profile, _prims);
    const int rootPn = _partitioner->build();
    if (rootPn < 0)
    {
        _failures.push_back("spatial partition failed");
        return false;
    }
    for (const std::string& f : _partitioner->failures) { _failures.push_back("partition: " + f); }
    for (const std::string& w : _partitioner->warnings) { _warnings.push_back("partition: " + w); }
    if (!_partitioner->verifyCoverage())
    {
        _failures.push_back("spatial partition coverage check failed");
    }
    _report.addStage("partition", tPart.elapsed());
    _report.addStage("partition_cpu", cPart.elapsed());

    // ---- proxy / encode / error ----
    Timer tBuild;
    _microHeavySec = 0.0;
    _microHeavyCpuSec = 0.0;
    _microMaterializeCpuSec = 0.0;
    _microProxyBuildCpuSec = 0.0;
    _microCollectCpuSec = 0.0;
    _microEvaluateCpuSec = 0.0;
    _microEvaluateWorkerCpuSec = 0.0;
    _microWriteCpuSec = 0.0;
    _microFinalizeCpuSec = 0.0;
    _microMaterializeSec = 0.0;
    _microProxyBuildSec = 0.0;
    _microWriteSec = 0.0;
    _microProxySimplifySec = 0.0;
    _microProxyAggregateSec = 0.0;
    _microEvaluateSec = 0.0;
    _microCollectSec = 0.0;
    _microAggregateCandidates = 0;
    _microEvaluateSamples = 0;
    _microBvhBuildSec = 0.0;
    _microEvalInitSec = 0.0;
    _microEvalPushSec = 0.0;
    _microEvalRefineSec = 0.0;
    _microEvalQuantileSec = 0.0;
    _microEvalRefineIterations = 0;
    _microEvalSourceTriangles = 0;
    _microEvalProxyTriangles = 0;
    _microEvalDistanceCalls = 0;
    _microEvalInitDistanceCalls = 0;
    _microEvalRefineDistanceCalls = 0;
    _microEvalBvhNodeVisits = 0;
    _microEvalBvhTriangleTests = 0;
    _microEvalInitParallelSec = 0.0;
    _microEvalInitThreadCreateSec = 0.0;
    _microFinalizeSec = 0.0;
    GltfOptimizer::resetTextureExportStats();
    TRACE("buildTrees");
    _tiles.clear();
    const int b3dmRoot = _prims.empty() ? -1 : buildB3dmTree(rootPn);
    TRACE("b3dmRoot=" << b3dmRoot);
    const int i3dmRoot = buildInstanceTree();
    TRACE("i3dmRoot=" << i3dmRoot);
    _report.addStage("proxy_encode_error", tBuild.elapsed());
    // 细分仪表（micro_* 前缀）：stages 只放秒数，层级关系求和时只能取一层——
    //   micro_total           = 各瓦片 processTileHeavy 全程的**壁钟**累加（并行侧
    //                           Σ 工作量，不是进程 CPU 秒；CPU 秒口径见下面同层的
    //                           micro_total_cpu）。8 路并行时 Σ 壁钟 ≈ 1 份工作量。
    //     ├─ micro_materialize + micro_proxy_build + micro_collect_triangles + micro_evaluate
    //     │    （micro_proxy_simplify / micro_proxy_aggregate 在 proxy_build 之内，
    //     │      micro_bvh_build / micro_eval_* 在 micro_evaluate 之内）
    //     └─ 残差 = 身份范围收集 / 代理字节预估 / 去纹理等未单列的小段
    //   micro_finalize_encode = 主线程收尾全程，micro_write 是其中的编码写出段
    _report.addStage("micro_total", _microHeavySec);
    _report.addStage("micro_materialize", _microMaterializeSec);
    _report.addStage("micro_proxy_build", _microProxyBuildSec);
    _report.addStage("micro_proxy_simplify", _microProxySimplifySec);
    _report.addStage("micro_proxy_aggregate", _microProxyAggregateSec);
    _report.addStage("micro_evaluate", _microEvaluateSec);
    _report.addStage("micro_collect_triangles", _microCollectSec);
    _report.addStage("micro_bvh_build", _microBvhBuildSec);
    // 评估内部四段（A-55）：micro_evaluate 之内按"查询 / 入堆剪枝 / 细分 / 分位数"拆开
    _report.addStage("micro_eval_init", _microEvalInitSec);
    _report.addStage("micro_eval_push", _microEvalPushSec);
    _report.addStage("micro_eval_refine", _microEvalRefineSec);
    _report.addStage("micro_eval_quantile", _microEvalQuantileSec);
    _report.addStage("micro_finalize_encode", _microFinalizeSec);
    _report.addStage("micro_write", _microWriteSec);
    // 同一层的 CPU 秒口径（AUDIT A-47 闭合判据）。壁钟那套在并行下必然小于
    // 进程 CPU 秒：8 个瓦片同跑，Σ 壁钟只算 1 份工作量。
    // micro_total_cpu = Σ(瓦片调用线程 CPU 秒) + Σ(节点内 worker 线程 CPU 秒)，
    // 和壁钟层一样是"外层包住内层"的关系：micro_evaluate_cpu 是它的一个子集
    // （evaluate 段的调用线程 + worker），micro_finalize_encode_cpu 与它同层。
    _report.addStage("micro_total_cpu", _microHeavyCpuSec + _microEvaluateWorkerCpuSec);
    _report.addStage("micro_materialize_cpu", _microMaterializeCpuSec);
    _report.addStage("micro_proxy_build_cpu", _microProxyBuildCpuSec);
    _report.addStage("micro_evaluate_cpu", _microEvaluateCpuSec);
    _report.addStage("micro_evaluate_worker_cpu", _microEvaluateWorkerCpuSec);
    _report.addStage("micro_collect_triangles_cpu", _microCollectCpuSec);
    _report.addStage("micro_finalize_encode_cpu", _microFinalizeCpuSec);
    _report.addStage("micro_write_cpu", _microWriteCpuSec);
    // 第三方线程的 CPU 秒（AUDIT A-47 判据①剩下的那块）：写 .ktx2 时 libktx 会
    // 自起 threadCount = hardware_concurrency 个 basis 压缩线程
    // （src/osgdb_ktx/LoadTextureKTX.cpp:630），上面所有 micro_* 都是**调用线程**
    // 的 CPU 秒，一秒也记不到它们。process_cpu 是进程口径的差值，
    // 与 thread_cpu 相减就是这批第三方线程的量（含写出期间并行瓦片的 worker 秒，
    // 因此是上界）。
    const GltfOptimizer::TextureExportStats texExport = GltfOptimizer::textureExportStats();
    _report.addStage("micro_texture_export", texExport.wallSeconds);
    _report.addStage("micro_texture_export_thread_cpu", texExport.threadCpuSeconds);
    _report.addStage("micro_texture_export_process_cpu", texExport.processCpuSeconds);
    _report.addCounter("micro_texture_export_files", texExport.files);
    // 计数走 counters：以前塞在 stages 里和秒数共用 "seconds" 字段，
    // 按秒求和会把 4.4 亿候选当成 4.4 亿秒加进已归属时间（AUDIT A-47）。
    _report.addCounter("micro_aggregate_candidates", _microAggregateCandidates);
    _report.addCounter("micro_evaluate_samples", _microEvaluateSamples);
    // 重复覆盖口径（A-55）：Σ evalSourceTriangles 是"所有祖先各自重评一遍子树源面"
    // 的总量，与源模型真实三角形数之比就是重复倍数
    _report.addCounter("micro_eval_refine_iterations", _microEvalRefineIterations);
    _report.addCounter("micro_eval_source_triangles", _microEvalSourceTriangles);
    _report.addCounter("micro_eval_proxy_triangles", _microEvalProxyTriangles);
    // 真实查询口径（本轮 §6.1）。micro_evaluate_samples 是逻辑采样记录：
    // 细分一次记 6 条样本但只发 1 次查询，所以 samples ≥ distanceCalls 是常态，
    // 拿样本数当"查询次数"会把成本高估。两项必须成对读。
    _report.addCounter("micro_eval_distance_calls", _microEvalDistanceCalls);
    _report.addCounter("micro_eval_init_distance_calls", _microEvalInitDistanceCalls);
    _report.addCounter("micro_eval_refine_distance_calls", _microEvalRefineDistanceCalls);
    _report.addCounter("micro_eval_bvh_node_visits", _microEvalBvhNodeVisits);
    _report.addCounter("micro_eval_bvh_triangle_tests", _microEvalBvhTriangleTests);
    // initQuerySeconds 里可分离出来的线程管理墙钟（见报告口径说明）
    _report.addStage("micro_eval_init_parallel", _microEvalInitParallelSec);
    _report.addStage("micro_eval_init_thread_create", _microEvalInitThreadCreateSec);

    // 远景代理纹理降分辨率是**成片**发生的（每个内节点代理一次），逐瓦片
    // warning 会把真正需要看的告警淹掉：这里只出一条汇总，逐瓦片明细在
    // report.proxyTextureCappedCount + 瓦片 timing 里。
    if (_report.proxyTextureCappedCount > 0)
    {
        _warnings.push_back("far-LOD proxies: " + std::to_string(_report.proxyTextureCappedCount)
                            + " inner-node tiles written with textures capped at <= "
                            + std::to_string(_profile.budget.proxyTextureMaxEdgePx)
                            + " px (leaf tiles keep the -tw/-th resolution)");
    }

    // ---- 组装根 ----
    if (b3dmRoot >= 0 && i3dmRoot >= 0)
    {
        // 有实例：根不承载内容，两个子树各自分区（手册 10.3：语义清晰的分支）
        const int rid = static_cast<int>(_tiles.size());
        _tiles.push_back(ProductionTile());
        _tiles[rid].id = rid;
        _tiles[rid].children.push_back(b3dmRoot);
        _tiles[rid].children.push_back(i3dmRoot);
        _tiles[rid].refine = "REPLACE";
        _root = rid;
    }
    else
    {
        _root = (b3dmRoot >= 0) ? b3dmRoot : i3dmRoot;
    }
    if (_root < 0)
    {
        _failures.push_back("no tile produced");
        return false;
    }

    TRACE("aggregateBounds");
    aggregateBounds(_root);
    TRACE("assignGeometricError");
    assignGeometricError(_root);
    TRACE("stats");

    // ---- 统计 ----
    TRACE("stats: tileCount=" << _tiles.size());
    _report.tileCount = static_cast<unsigned>(_tiles.size());
    _report.contentTileCount = 0;
    for (const ProductionTile& t : _tiles)
    {
        if (t.hasContent) { ++_report.contentTileCount; }
    }
    _report.maxDepth = 0;
    {
        TRACE("stats: maxDepth walk");
        std::vector<int> stack{_root};
        std::map<int, unsigned> depth;
        depth[_root] = 0;
        while (!stack.empty())
        {
            const int n = stack.back();
            stack.pop_back();
            _report.maxDepth = std::max(_report.maxDepth, depth[n]);
            for (int c : _tiles[n].children)
            {
                depth[c] = depth[n] + 1;
                stack.push_back(c);
            }
        }
    }
    TRACE("stats: error agg");
    _report.errorMax = _tiles[_root].geometricErrorM;
    double sum = 0.0;
    unsigned n = 0;
    for (const ProductionTile& t : _tiles)
    {
        if (t.hasContent) { sum += t.measuredProxyErrorM; ++n; }
    }
    _report.errorMean = (n > 0) ? sum / n : 0.0;
    TRACE("stats: occurrenceCount");
    _report.manifestOccurrences = static_cast<unsigned>(_mapper.sourceOccurrenceCount(_model));

    TRACE("stats: mapper.validate");
    for (const std::string& e : _mapper.validate()) { _failures.push_back("featureMapper: " + e); }

    TRACE("stats: finalize report (rss/seconds)");
    _report.totalSeconds = total.elapsed();
    const ProcessTimes ptAtReport = currentProcessTimes();
    _report.processWallSeconds = ptAtReport.wallSeconds;
    _report.processCpuSeconds = ptAtReport.cpuSeconds;
    _report.processUserSeconds = ptAtReport.userSeconds;
    _report.processKernelSeconds = ptAtReport.kernelSeconds;
    // run() 之前那一段（main.cpp 的 osgDB::readNodeFile 是主体）单独成一项。
    // 那时只有主线程在跑，所以 CPU 秒就是它的真实用量；wall 与 cpu 都记，
    // 归属求和用 cpu 那一项（wall 里含 IO 等待，加进 CPU 秒账里会算重）。
    _report.addStage("input_read", ptBeforeRun.wallSeconds);
    _report.addStage("input_read_cpu", ptBeforeRun.cpuSeconds);
    _report.peakRssBytes = currentPeakRss();
    _report.warnings = _warnings;
    _report.errors = _failures;
    TRACE("stats: run() returning " << (_failures.empty() ? "true" : "false"));
    return _failures.empty();
}

static json tileToJson(const std::vector<ProductionTile>& tiles, int id)
{
    const ProductionTile& t = tiles[id];
    json j;
    osg::BoundingBoxd zUp;
    yUpToZUpBounds(t.declaredBounds, zUp);
    j["boundingVolume"]["box"] = boxJson(zUp);
    j["geometricError"] = t.geometricErrorM;
    if (t.children.empty() && !t.hasContent)
    {
        // 空节点：不写 content，也不写 refine
    }
    else
    {
        // root 必须写 refine；非根也显式写出，便于排查（手册 10.3 第 7 点）
        j["refine"] = t.refine;
    }
    if (t.hasContent) { j["content"]["uri"] = t.uri; }
    if (!t.children.empty())
    {
        json ch = json::array();
        for (int c : t.children) { ch.push_back(tileToJson(tiles, c)); }
        j["children"] = ch;
    }
    return j;
}

bool ProductionPipeline::writeTilesetJson(const std::string& path) const
{
    if (_root < 0) { return false; }
    const ProductionTile& root = _tiles[_root];
    osg::BoundingBoxd zUpRoot;
    yUpToZUpBounds(root.declaredBounds, zUpRoot);

    // tileset.geometricError 的语义：整个 tileset 完全不渲染时的误差。
    // 这里用根包围球半径作为保守值（不渲染任何东西时，模型表面上任意点到
    // "空"的最大偏差不超过包围球直径，取半径是常用且可解释的保守下界）。
    // 不使用任何 *1.5 之类的魔法系数（手册 9.6）。
    double rootRadius = 0.0;
    if (zUpRoot.valid())
    {
        rootRadius = (zUpRoot._max - zUpRoot._min).length() * 0.5;
    }
    const double tilesetError = std::max(rootRadius, root.geometricErrorM);

    json j;
    j["asset"]["version"] = "1.0";
    j["asset"]["tilesetVersion"] = _profile.version;
    j["asset"]["gltfUpAxis"] = "Y";
    j["asset"]["generator"] = _profile.generator;
    j["geometricError"] = tilesetError;

    json rootJson = tileToJson(_tiles, _root);
    // 地理定位：root transform（ENU -> WGS84）
    {
        const osg::EllipsoidModel ellipsoidModel;
        osg::Matrixd localToWorld;
        ellipsoidModel.computeLocalToWorldTransformFromLatLongHeight(
            osg::DegreesToRadians(_opt.latitude), osg::DegreesToRadians(_opt.longitude),
            _opt.altitude, localToWorld);
        const double* ptr = localToWorld.ptr();
        std::vector<double> tf;
        for (unsigned i = 0; i < 16; ++i) { tf.push_back(ptr[i]); }
        rootJson["transform"] = tf;
    }
    j["root"] = rootJson;

    std::ofstream f(Utils::utf8ToPath(path), std::ios::binary);
    if (!f.is_open()) { return false; }
    f << j.dump(2);
    f.close();
    return true;
}

bool ProductionPipeline::writeManifest(const std::string& path) const
{
    json m = _mapper.manifestJson(_model);

    // 聚合代理的"代理 ID → 源 feature 集合"只存在于 ProductionTile 上
    // （ProxyMeshBuilder 直接回传），mapper 侧的 addAggregateTile 通路没人调用，
    // 所以修复前 manifest.aggregateProxies 恒为 {}（AUDIT A-34：longxiang 报
    // aggregateProxyCount=10 而 manifest 一个条目都没有）。这里按 tileUri 补齐。
    json agg = json::object();
    for (const ProductionTile& t : _tiles)
    {
        // hasContent 这一条与 selfValidate 同口径：A-31 降级节点没有 tileUri，
        // 它的 scope 由后代瓦片覆盖，不该往 manifest 里塞一个空字符串 key。
        // 构建期降级路径会在 aggregateSources 赋值之前就 return，所以现有夹具
        // 走不到这一支（去掉它做变异测试，118 条断言仍全过）；能走到的是
        // "估算达标但实测超限"那条防御分支，留着是防 manifest 结构被污染。
        if (!t.hasContent || !t.aggregate || t.aggregateSources.empty()) { continue; }
        json perTile = json::object();
        for (const auto& kv : t.aggregateSources)
        {
            perTile[std::to_string(kv.first)] = kv.second;
        }
        agg[t.uri] = perTile;
    }
    m["aggregateProxies"] = agg;

    std::ofstream f(Utils::utf8ToPath(path), std::ios::binary);
    if (!f.is_open()) { return false; }
    f << m.dump(2);
    f.close();
    return true;
}

json TileTimings::toJson() const
{
    // 只到 0.1 ms：仪表是给人读占比的，多余位数只会让 report 变大且制造虚假精度
    const auto r = [](double s) { return std::round(s * 1.0e4) / 1.0e4; };
    return json{
        {"heavySec", r(heavySec)},
        {"materializeSec", r(materializeSec)},
        {"proxyBuildSec", r(proxyBuildSec)},
        {"collectSec", r(collectSec)},
        {"evaluateSec", r(evaluateSec)},
        {"bvhBuildSec", r(bvhBuildSec)},
        {"writeSec", r(writeSec)},
        {"finalizeSec", r(finalizeSec)},
        {"heavyCpuSec", r(heavyCpuSec)},
        {"materializeCpuSec", r(materializeCpuSec)},
        {"proxyBuildCpuSec", r(proxyBuildCpuSec)},
        {"collectCpuSec", r(collectCpuSec)},
        {"evaluateCpuSec", r(evaluateCpuSec)},
        {"evaluateWorkerCpuSec", r(evaluateWorkerCpuSec)},
        {"writeCpuSec", r(writeCpuSec)},
        {"finalizeCpuSec", r(finalizeCpuSec)},
        {"evaluateSamples", evaluateSamples},
        {"evalInitSec", r(evalInitSec)},
        {"evalPushSec", r(evalPushSec)},
        {"evalRefineSec", r(evalRefineSec)},
        {"evalQuantileSec", r(evalQuantileSec)},
        {"evalRefineIterations", evalRefineIterations},
        {"evalSourceTriangles", evalSourceTriangles},
        {"evalProxyTriangles", evalProxyTriangles}
    };
}

bool ProductionPipeline::writeReport(const std::string& path) const
{
    std::ofstream f(Utils::utf8ToPath(path), std::ios::binary);
    if (!f.is_open()) { return false; }
    json j = _report.toJson();
    j["tilesetGeometricError"] = _root >= 0 ? _tiles[_root].geometricErrorM : 0.0;
    json tiles = json::array();
    for (const ProductionTile& t : _tiles)
    {
        json tj;
        tj["id"] = t.id;
        tj["uri"] = t.uri;
        tj["hasContent"] = t.hasContent;
        tj["isLeaf"] = t.isLeaf;
        tj["isProxy"] = t.isProxy;
        tj["aggregate"] = t.aggregate;
        tj["sourceFeatures"] = static_cast<unsigned>(t.sourceScope.size());
        tj["triangles"] = t.triangles;
        tj["contentBytes"] = t.contentBytes;
        tj["measuredProxyErrorM"] = t.measuredProxyErrorM;
        tj["errorLowerM"] = t.errorLowerM;
        tj["errorUpperM"] = t.errorUpperM;
        tj["geometricErrorM"] = t.geometricErrorM;
        tj["errorStatus"] = t.errorStatus;
        tj["refine"] = t.refine;
        tj["timing"] = t.timing.toJson();
        if (t.aggregate)
        {
            json agg = json::object();
            for (auto& kv : t.aggregateSources) { agg[std::to_string(kv.first)] = kv.second; }
            tj["aggregateSources"] = agg;
        }
        tiles.push_back(tj);
    }
    j["tiles"] = tiles;
    f << j.dump(2);
    f.close();
    return true;
}

// ---------------------------------------------------------------------------
// 产物级 glTF 读取（selfValidate 的 primitive / 材质预算闸用）
//
// 为什么必须读产物而不是数 OSG 节点上的 drawable：最终 primitive 数由写出侧
// GltfMerger::mergeMeshes() 决定，它按 世界矩阵 × 材质索引 × 属性名集合
// 分桶融图元，一个瓦片里 N 个 drawable 最后可能只剩 1 个 primitive。
// ---------------------------------------------------------------------------
namespace {
bool readU32LE(const std::string& buf, std::size_t off, std::uint32_t& out)
{
    if (off + 4 > buf.size()) { return false; }
    out = static_cast<std::uint32_t>(static_cast<unsigned char>(buf[off]))
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(buf[off + 1])) << 8)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(buf[off + 2])) << 16)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(buf[off + 3])) << 24);
    return true;
}

// 单个内层 tile（b3dm/i3dm）的 glTF JSON 起点；容器不合法时返回 false。
bool glbJsonRange(const std::string& bytes, std::size_t tileStart,
                  std::size_t& jsonOff, std::uint32_t& jsonLen)
{
    std::size_t header = 0;
    if (bytes.compare(tileStart, 4, "b3dm") == 0) { header = 28; }
    else if (bytes.compare(tileStart, 4, "i3dm") == 0) { header = 32; }
    else { return false; }

    // 两种容器的表长字段布局相同：byteLength@8、featureTableJson@12、
    // featureTableBinary@16、batchTableJson@20、batchTableBinary@24；
    // i3dm 只是头多 4 字节（glTFFormat）。
    std::uint32_t ftj = 0, ftb = 0, btj = 0, btb = 0;
    if (!readU32LE(bytes, tileStart + 12, ftj)) { return false; }
    if (!readU32LE(bytes, tileStart + 16, ftb)) { return false; }
    if (!readU32LE(bytes, tileStart + 20, btj)) { return false; }
    if (!readU32LE(bytes, tileStart + 24, btb)) { return false; }

    const std::size_t glb = tileStart + header + ftj + ftb + btj + btb;
    if (glb + 20 > bytes.size()) { return false; }
    if (bytes.compare(glb, 4, "glTF") != 0) { return false; }
    if (!readU32LE(bytes, glb + 12, jsonLen)) { return false; }
    if (bytes.compare(glb + 16, 4, "JSON") != 0) { return false; }
    jsonOff = glb + 20;
    return jsonOff + jsonLen <= bytes.size();
}

// 遍历一个瓦片内容文件里内嵌的每一个 glTF JSON chunk（cmpt 会有多个内层 tile）。
void forEachGltfJsonChunk(const std::string& bytes,
                          const std::function<void(const std::string&)>& onJson)
{
    std::vector<std::size_t> tileStarts;
    if (bytes.size() >= 16 && bytes.compare(0, 4, "cmpt") == 0)
    {
        std::uint32_t outerLength = 0, innerCount = 0;
        if (!readU32LE(bytes, 8, outerLength)) { return; }
        if (!readU32LE(bytes, 12, innerCount)) { return; }
        if (outerLength > bytes.size()) { return; }
        std::size_t p = 16;
        for (std::uint32_t i = 0; i < innerCount; ++i)
        {
            std::uint32_t innerLength = 0;
            if (!readU32LE(bytes, p + 8, innerLength)) { return; }
            if (innerLength < 12 || p + innerLength > bytes.size()) { return; }
            tileStarts.push_back(p);
            p += innerLength;
        }
    }
    else
    {
        tileStarts.push_back(0);
    }

    for (std::size_t start : tileStarts)
    {
        std::size_t jsonOff = 0;
        std::uint32_t jsonLen = 0;
        if (!glbJsonRange(bytes, start, jsonOff, jsonLen)) { continue; }
        onJson(bytes.substr(jsonOff, jsonLen));
    }
}
} // namespace

// ---------------------------------------------------------------------------
// 手册 14.17：异常终止与重试
//
// 之前是边跑边往 outputDir 里写瓦片，run() 失败后 main 仍然把 tileset.json
// 写进同一个目录，磁盘上就留下一个"看起来成功其实内容不全"的数据集。
// 现在改成：全部产物先落暂存目录，自校验通过后才整体搬到 outputDir。
// ---------------------------------------------------------------------------
bool ProductionPipeline::selfValidate(const std::string& tilesetJsonPath)
{
    std::ifstream f(Utils::utf8ToPath(tilesetJsonPath), std::ios::binary);
    if (!f.is_open()) { return false; }
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    json parsed;
    try
    {
        parsed = json::parse(text);
    }
    catch (const std::exception& e)
    {
        _failures.push_back(std::string("selfValidate: tileset.json is not valid JSON: ") + e.what());
        return false;
    }

    bool ok = true;
    // 诊断开关：WB_KEEP_FAILED_CONTENT=1 时把越限瓦片内容拷到
    // <output>/failed-content/。暂存目录在失败后会被整体丢弃，不拷出来就无法
    // 解析"闸门看到的 primitive 数"到底由什么构成。不设变量时行为完全不变。
    const bool keepFailedContent = std::getenv("WB_KEEP_FAILED_CONTENT") != nullptr;
    const auto dumpFailedContent = [&](const std::string& uri, const std::string& abs) {
        if (!keepFailedContent) { return; }
        std::error_code ec;
        const std::filesystem::path dst = Utils::utf8ToPath(_opt.outputDir)
            / "failed-content" / Utils::utf8ToPath(uri);
        std::filesystem::create_directories(dst.parent_path(), ec);
        std::filesystem::copy_file(Utils::utf8ToPath(abs), dst,
                                   std::filesystem::copy_options::overwrite_existing, ec);
    };
    // 预算失败时把"分区器看到的负载"和"导出后的宿主数"放在同一条 warning 里。
    // 两者的差就是 proxy/材质保真环节放大的部分，没有这条读数就只能靠猜。
    std::map<int, int> idToPn;
    for (const auto& kv : _pnToTileId) { idToPn[kv.second] = kv.first; }
    std::vector<std::string> budgetDescribed;
    const auto describeBudget = [&](const std::string& uri, unsigned expPrims, unsigned expMats) {
        if (std::find(budgetDescribed.begin(), budgetDescribed.end(), uri) != budgetDescribed.end())
        {
            return;
        }
        budgetDescribed.push_back(uri);
        int tileId = -1;
        for (const ProductionTile& t : _tiles) { if (t.uri == uri) { tileId = t.id; break; } }
        if (tileId < 0) { return; }
        const auto it = idToPn.find(tileId);
        if (it == idToPn.end() || !_partitioner) { return; }
        const PartitionNode& P = _partitioner->nodes[it->second];
        const ProductionTile& T = _tiles[tileId];
        _warnings.push_back("tile " + std::to_string(tileId) + " budget breakdown (pn="
            + std::to_string(it->second) + ", leaf=" + (T.isLeaf ? "1" : "0")
            + ", aggregate=" + (T.aggregate ? "1" : "0")
            + "): partition tri=" + std::to_string(P.triangles)
            + " vert=" + std::to_string(P.vertices)
            + " prim=" + std::to_string(P.primitiveCount)
            + " mat=" + std::to_string(P.materialCount)
            + " estBytes=" + std::to_string(P.byteEstimate)
            + "; exported prims=" + std::to_string(expPrims)
            + " materials=" + std::to_string(expMats));
    };
    // 1) 所有 content URI 必须在暂存目录里真实存在
    std::vector<std::string> uris;
    std::function<void(const json&)> walk = [&](const json& t) {
        if (t.contains("content") && t["content"].is_object()
            && t["content"].contains("uri") && t["content"]["uri"].is_string())
        {
            uris.push_back(t["content"]["uri"].get<std::string>());
        }
        if (t.contains("children") && t["children"].is_array())
        {
            for (const json& c : t["children"]) { walk(c); }
        }
    };
    if (parsed.contains("root") && parsed["root"].is_object()) { walk(parsed["root"]); }
    for (const std::string& u : uris)
    {
        const std::string abs = _opt.stagingDir + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + u;
        std::ifstream cf(Utils::utf8ToPath(abs), std::ios::binary);
        if (!cf.is_open())
        {
            _failures.push_back("selfValidate: content uri missing on disk: " + u);
            ok = false;
            continue;
        }
        cf.seekg(0, std::ios::end);
        const std::uint64_t bytes = static_cast<std::uint64_t>(cf.tellg());
        cf.seekg(0, std::ios::beg);
        std::string contentBytes;
        contentBytes.reserve(static_cast<std::size_t>(bytes));
        contentBytes.assign(std::istreambuf_iterator<char>(cf), std::istreambuf_iterator<char>());
        cf.close();
        if (bytes > _profile.budget.maxTileContentBytes)
        {
            _failures.push_back("selfValidate: content over hard byte limit: " + u
                                + " (" + std::to_string(bytes) + " > "
                                + std::to_string(_profile.budget.maxTileContentBytes) + ")");
            dumpFailedContent(u, abs);
            ok = false;
        }

        // 1a) glTF primitive / 材质预算（手册 7.3 第 2 步"材质/图元合并"的
        //     产物级复核）。读不到可解析的 glTF 也算失败：预算无法证明时
        // 不能当成"没超"（A-32/A-33 同族：声明了预算却没有闸）。
        unsigned prims = 0;
        unsigned mats = 0;
        bool sawGltf = false;
        bool badGltf = false;
        forEachGltfJsonChunk(contentBytes, [&](const std::string& gltfJson) {
            sawGltf = true;
            try
            {
                const json gltf = json::parse(gltfJson);
                if (gltf.contains("meshes") && gltf["meshes"].is_array())
                {
                    for (const json& m : gltf["meshes"])
                    {
                        if (m.contains("primitives") && m["primitives"].is_array())
                        {
                            prims += static_cast<unsigned>(m["primitives"].size());
                        }
                    }
                }
                if (gltf.contains("materials") && gltf["materials"].is_array())
                {
                    mats += static_cast<unsigned>(gltf["materials"].size());
                }
            }
            catch (const std::exception&)
            {
                badGltf = true;
            }
        });
        if (!sawGltf || badGltf)
        {
            _failures.push_back(std::string("selfValidate: content has no parsable glTF body (")
                                + (badGltf ? "json error" : "no glTF chunk") + "): " + u);
            ok = false;
        }
        else if (prims > _profile.budget.maxPrimitivesPerTile)
        {
            _failures.push_back("selfValidate: content over primitive budget: " + u + " ("
                                + std::to_string(prims) + " primitives > "
                                + std::to_string(_profile.budget.maxPrimitivesPerTile) + ")");
            dumpFailedContent(u, abs);
            describeBudget(u, prims, mats);
            ok = false;
        }
        if (mats > _profile.budget.maxMaterialsPerTile)
        {
            _failures.push_back("selfValidate: content over material budget: " + u + " ("
                                + std::to_string(mats) + " materials > "
                                + std::to_string(_profile.budget.maxMaterialsPerTile) + ")");
            dumpFailedContent(u, abs);
            describeBudget(u, prims, mats);
            ok = false;
        }
    }

    // 1b) 序列化后的包围体与 geometricError 必须是有限值，且父盒覆盖子盒。
    //     boxJson 会把 NaN 半轴 std::max(0.0, nan) 折成 0、把未初始化的包围盒
    //     折成全零盒——NaN 就这样被静默"洗白"成一份 schema 合法的 tileset，
    //     而 Cesium 用一个塌成原点的盒做视锥剔除会把整个子树剔掉（手册 17：
    //     NaN bounds 为零 / 错误 bounds 剔除）。父覆盖子历史上因 float 中心
    //     舍入在 t04/t05/t06/t08/t10/t11 报过 child box not covered，改成
    //     double 之后不再触发，这里是那条回归的守门人。
    {
        auto readBox = [](const json& t, double* out) -> bool {
            if (!t.contains("boundingVolume") || !t["boundingVolume"].is_object()) { return false; }
            const json& bv = t["boundingVolume"];
            if (!bv.contains("box") || !bv["box"].is_array() || bv["box"].size() != 12) { return false; }
            for (int i = 0; i < 12; ++i)
            {
                if (!bv["box"][i].is_number()) { return false; }
                out[i] = bv["box"][i].get<double>();
            }
            return true;
        };
        std::function<void(const json&, const double*, int)> checkGeom =
            [&](const json& t, const double* parent, int depth)
        {
            double b[12];
            if (!readBox(t, b))
            {
                _failures.push_back("selfValidate: tile at depth " + std::to_string(depth)
                                    + " has no 12-number boundingVolume.box");
                ok = false;
            }
            else
            {
                for (int i = 0; i < 12; ++i)
                {
                    if (!std::isfinite(b[i]))
                    {
                        _failures.push_back("selfValidate: boundingVolume.box["
                                            + std::to_string(i) + "] is not finite at depth "
                                            + std::to_string(depth));
                        ok = false;
                        break;
                    }
                }
                if (b[3] < 0.0 || b[7] < 0.0 || b[11] < 0.0)
                {
                    _failures.push_back("selfValidate: negative half-axis in boundingVolume.box "
                                        "at depth " + std::to_string(depth));
                    ok = false;
                }
                if (parent)
                {
                    // 轴向对齐盒：|中心差| + 子半轴 <= 父半轴（留 1e-6 相对容差）
                    const double eps = 1e-6 * std::max(1.0, std::max(parent[3],
                                                     std::max(parent[7], parent[11])));
                    // 盒布局 [cx,cy,cz, hx,0,0, 0,hy,0, 0,0,hz]：第 i 轴的
                    // 中心在 i、半轴在 3+4i（不是 3+i，也不是 i*4）。
                    const int ci[3] = { 0, 1, 2 };
                    const int hi[3] = { 3, 7, 11 };
                    for (int a = 0; a < 3; ++a)
                    {
                        const double cChild = b[ci[a]];
                        const double cPar = parent[ci[a]];
                        const double hChild = b[hi[a]];
                        const double hPar = parent[hi[a]];
                        if (std::abs(cChild - cPar) + hChild > hPar + eps)
                        {
                            _failures.push_back(std::string("selfValidate: child box not covered by parent "
                                                "on axis ") + std::to_string(a)
                                                + " at depth " + std::to_string(depth)
                                                + " (|dc|=" + std::to_string(std::abs(cChild - cPar))
                                                + " hChild=" + std::to_string(hChild)
                                                + " hPar=" + std::to_string(hPar) + ")");
                            ok = false;
                            break;
                        }
                    }
                }
            }
            if (t.contains("geometricError"))
            {
                const double ge = t["geometricError"].is_number() ? t["geometricError"].get<double>()
                                                                  : std::numeric_limits<double>::quiet_NaN();
                if (!std::isfinite(ge) || ge < 0.0)
                {
                    _failures.push_back("selfValidate: geometricError is not a finite non-negative "
                                        "number at depth " + std::to_string(depth));
                    ok = false;
                }
            }
            if (t.contains("children") && t["children"].is_array())
            {
                for (const json& c : t["children"]) { checkGeom(c, b, depth + 1); }
            }
        };
        if (parsed.contains("root") && parsed["root"].is_object())
        {
            checkGeom(parsed["root"], nullptr, 0);
        }
    }

    // 2) 每个瓦片的字节预算（写盘后实测）
    for (const ProductionTile& t : _tiles)
    {
        if (t.hasContent && t.contentBytes > _profile.budget.maxTileContentBytes)
        {
            _failures.push_back("selfValidate: tile " + std::to_string(t.id) + " content "
                                + std::to_string(t.contentBytes) + " bytes exceeds hard limit "
                                + std::to_string(_profile.budget.maxTileContentBytes));
            ok = false;
        }
        if (t.hasContent && t.triangles > _profile.budget.maxTrianglesPerTile)
        {
            _failures.push_back("selfValidate: tile " + std::to_string(t.id) + " has "
                                + std::to_string(t.triangles) + " triangles, over hard limit "
                                + std::to_string(_profile.budget.maxTrianglesPerTile));
            ok = false;
        }
    }

    // 2b) 内容顶点必须留在源模型范围内（A-38）。有限性/覆盖关系都拦不住
    //     "有限但荒谬"的包围盒：matrix_a34/t02 实测一个被索引引用的顶点是
    //     (6.55e10, -1.2e-8, 4.47e27)，JSON 合法、父盒照样盖住子盒，但
    //     geometricError 被顶到 5.8e24 m、整棵树的可渲染性没了意义。
    //     放宽量取源对角线的一半：远景代理的顶点会被简化器移动，但任何
    //     真实输出都不可能被移动出源模型尺度这个量级。
    osg::BoundingBox srcFrame;
    double srcSlack = 0.0;
    if (_model.bounds.valid())
    {
        // 参考系必须是"全部源对象"的范围，而不是 _model.bounds 本身：
        // SourceModel::build 在识别出重复实例后会把 bounds 重算成"剔除实例几何
        // 之后"的范围（SourceModel.cpp:578-584），而 i3dm 瓦片的内容恰恰住在被
        // 剔除的那一半。直接拿收窄后的 bounds 去比，每个实例数据集里正常的 i3dm
        // 瓦片都会被判成越界（matrix_a38gate/t05：i2.i3dm 的 x=[308.96,310.96]
        // 对源 [35.87,37.87]±1.73 拒发，而同一二进制之前它出盘是正常的 4 瓦片）。
        srcFrame = _model.bounds;
        for (const SourceObject& o : _model.objects)
        {
            if (o.aabb.valid()) { srcFrame.expandBy(o.aabb); }
        }
        srcSlack = 0.5 * (osg::Vec3d(srcFrame._max) - osg::Vec3d(srcFrame._min)).length();
    }
    for (const ProductionTile& t : _tiles)
    {
        if (!t.hasContent || !srcFrame.valid() || !t.contentBounds.valid()) { continue; }
        const osg::BoundingBox& cb = t.contentBounds;
        const double lo[3] = { cb.xMin(), cb.yMin(), cb.zMin() };
        const double hi[3] = { cb.xMax(), cb.yMax(), cb.zMax() };
        const double slo[3] = { srcFrame.xMin(), srcFrame.yMin(), srcFrame.zMin() };
        const double shi[3] = { srcFrame.xMax(), srcFrame.yMax(), srcFrame.zMax() };
        for (int a = 0; a < 3; ++a)
        {
            if (!std::isfinite(lo[a]) || !std::isfinite(hi[a]))
            {
                _failures.push_back("selfValidate: tile " + std::to_string(t.id)
                                    + " content bounds are not finite on axis "
                                    + std::to_string(a));
                ok = false;
                break;
            }
            if (lo[a] < slo[a] - srcSlack || hi[a] > shi[a] + srcSlack)
            {
                _failures.push_back("selfValidate: tile " + std::to_string(t.id)
                                    + " content bounds leave the source model extent on axis "
                                    + std::to_string(a) + " ([" + std::to_string(lo[a]) + ", "
                                    + std::to_string(hi[a]) + "] vs source ["
                                    + std::to_string(slo[a]) + ", " + std::to_string(shi[a])
                                    + "] +-" + std::to_string(srcSlack)
                                    + "): garbage vertex or wrong coordinate frame");
                ok = false;
                break;
            }
        }
    }

    // 3) geometricError 根到叶单调不增
    std::function<void(int)> checkGe = [&](int idx) {
        if (idx < 0 || static_cast<std::size_t>(idx) >= _tiles.size()) { return; }
        for (int c : _tiles[idx].children)
        {
            if (c < 0 || static_cast<std::size_t>(c) >= _tiles.size()) { continue; }
            if (_tiles[c].geometricErrorM > _tiles[idx].geometricErrorM + 1e-9)
            {
                _failures.push_back("selfValidate: geometricError increases from tile "
                                    + std::to_string(idx) + " to child " + std::to_string(c));
                ok = false;
            }
            checkGe(c);
        }
    };
    checkGe(_root);

    // 3b) 树深硬上限（手册 §3：深度 ≤ 48，且这条限制不允许被自动放宽）。
    //     maxDepth 只有报告值、发布闸不检查的话，一个失控的分区递归照样能出盘。
    {
        std::vector<std::pair<int, unsigned>> stack;
        stack.push_back(std::make_pair(_root, 0u));
        unsigned deepest = 0;
        int deepestTile = _root;
        while (!stack.empty())
        {
            const int idx = stack.back().first;
            const unsigned d = stack.back().second;
            stack.pop_back();
            if (idx < 0 || static_cast<std::size_t>(idx) >= _tiles.size()) { continue; }
            if (d > deepest) { deepest = d; deepestTile = idx; }
            for (int c : _tiles[idx].children)
            {
                stack.push_back(std::make_pair(c, d + 1));
            }
        }
        if (deepest > _profile.budget.maxTreeDepth)
        {
            _failures.push_back("selfValidate: tree depth " + std::to_string(deepest)
                                + " exceeds hard limit "
                                + std::to_string(_profile.budget.maxTreeDepth)
                                + " (deepest tile " + std::to_string(deepestTile) + ")");
            ok = false;
        }
    }

    // 4) 源身份覆盖（手册 9.5 / 17："REPLACE 子树完整覆盖其 source scope"）
    //    这类缺陷在 Hausdorff 数值上是隐形的：被丢掉的小簇紧贴邻近表面时
    //    双向距离仍然很小，平均误差一切正常但业务部件已经消失。只能按
    //    globalFeatureId 逐叶核对。用的是分区单元表推出的 sourceScope，
    //    不是 Geode 名字回查出来的 featureOrder（重名/聚合都会让它失真）。
    std::set<FeatureId> sourceUniverse;
    for (const PrimRef& p : _prims)
    {
        if (!p.tris.empty()) { sourceUniverse.insert(p.globalFeatureId); }
    }
    std::set<FeatureId> leafCovered;
    for (const ProductionTile& t : _tiles)
    {
        if (t.hasContent && t.triangles == 0)
        {
            _failures.push_back("selfValidate: tile " + std::to_string(t.id) +
                                " has content but zero triangles");
            ok = false;
        }
        if (!t.isLeaf || t.sourceScope.empty()) { continue; }   // 实例叶无源单元
        if (!t.hasContent)
        {
            _failures.push_back("selfValidate: leaf tile " + std::to_string(t.id) + " carries no content"
                                + " but owns " + std::to_string(t.sourceScope.size())
                                + " source features (geometry lost)");
            ok = false;
            continue;
        }
        leafCovered.insert(t.sourceScope.begin(), t.sourceScope.end());
    }
    std::vector<FeatureId> uncovered;
    for (FeatureId fid : sourceUniverse)
    {
        if (!leafCovered.count(fid)) { uncovered.push_back(fid); }
    }
    if (!uncovered.empty())
    {
        std::string head;
        for (std::size_t i = 0; i < uncovered.size() && i < 8; ++i)
        {
            head += (i ? "," : "") + std::to_string(uncovered[i]);
        }
        _failures.push_back("selfValidate: " + std::to_string(uncovered.size())
                            + " source features are absent from every leaf tile (first ids: " + head + ")");
        _report.featuresMissingFromLeaves = static_cast<unsigned>(uncovered.size());
        ok = false;
    }

    // 5) 聚合代理的身份映射（手册 7.5 / 8.1）：proxyFeatureId 必须登记它
    //    代表的源集合，且只能是父级粗层——叶必须保留对象身份。
    //    抽稀掉的源 feature 单独计数进 report，不藏进平均误差（9.5）。
    for (const ProductionTile& t : _tiles)
    {
        if (!t.aggregate) { continue; }
        if (t.isLeaf)
        {
            _failures.push_back("selfValidate: leaf tile " + std::to_string(t.id)
                                + " uses an aggregate proxy; leaves must keep object identity");
            ok = false;
        }
        if (!t.hasContent)
        {
            // 被降级的远景代理（A-31：超预算文件已删除、该层无内容）不写任何
            // batch id，既没有身份冒充风险，也不该把它的 scope 记成"抽稀丢失"
            // ——真实细节仍在后代叶瓦片里。
            continue;
        }
        if (t.aggregateSources.empty())
        {
            _failures.push_back("selfValidate: aggregate proxy tile " + std::to_string(t.id)
                                + " records no source-feature mapping");
            ok = false;
            continue;
        }
        const std::set<FeatureId> scope(t.sourceScope.begin(), t.sourceScope.end());
        std::set<FeatureId> mapped;
        for (const auto& kv : t.aggregateSources)
        {
            for (FeatureId src : kv.second)
            {
                if (!scope.count(src))
                {
                    _failures.push_back("selfValidate: aggregate proxy tile " + std::to_string(t.id)
                                        + " maps source feature " + std::to_string(src)
                                        + " that is outside its own scope");
                    ok = false;
                }
                mapped.insert(src);
            }
        }
        const unsigned dropped = static_cast<unsigned>(scope.size())
                               - static_cast<unsigned>(std::min(mapped.size(), scope.size()));
        _report.proxyDroppedFeatureCount += dropped;
        if (dropped > 0)
        {
            _warnings.push_back("tile " + std::to_string(t.id) + ": aggregate proxy thins out "
                                + std::to_string(dropped) + " of " + std::to_string(scope.size())
                                + " source features (detail is only available in descendant leaves)");
        }
    }

    return ok;
}

void ProductionPipeline::discardStaging()
{
    if (_opt.stagingDir.empty()) { return; }
    std::error_code ec;
    const std::filesystem::path p = Utils::utf8ToPath(_opt.stagingDir);
    if (std::filesystem::exists(p, ec))
    {
        std::filesystem::remove_all(p, ec);
        if (ec) { _warnings.push_back("discardStaging: " + ec.message()); }
    }
}

bool ProductionPipeline::finalize()
{
    TRACE("finalize enter");
    if (_root < 0)
    {
        _failures.push_back("finalize: no tile tree produced");
        discardStaging();
        return false;
    }
    if (!_failures.empty())
    {
        // run() 阶段已经有明确失败项：不产出任何数据集
        TRACE("finalize: pre-existing failures=" << _failures.size());
        discardStaging();
        return false;
    }

    const std::string sep = OSG_GIS_PLUGINS_PATH_SPLIT_STRING;
    const std::string tsPath = _opt.stagingDir + sep + "tileset.json";
    TRACE("finalize: writeTilesetJson");
    if (!writeTilesetJson(tsPath)) { _failures.push_back("finalize: write tileset.json failed"); discardStaging(); return false; }
    TRACE("finalize: writeManifest");
    if (!writeManifest(_opt.stagingDir + sep + "feature-manifest.json"))
    { _failures.push_back("finalize: write feature-manifest.json failed"); discardStaging(); return false; }
    TRACE("finalize: selfValidate");
    if (!selfValidate(tsPath)) { TRACE("finalize: selfValidate FAILED (" << _failures.size() << " failures)"); discardStaging(); return false; }
    TRACE("finalize: selfValidate ok");

    // 属主标记不进产物、也不进 outputBytes 统计
    {
        std::error_code ecOwner;
        std::filesystem::remove(std::filesystem::path(Utils::utf8ToPath(_opt.stagingDir)) / "owner.pid",
                                ecOwner);
    }

    // report 必须在自校验之后落盘：覆盖计数与聚合抽稀 warning 都是校验阶段才产生的
    // outputBytes 的口径（AUDIT A-58，固定下来不再随版本变）：自校验通过、写报告之前，
    // 暂存目录里**全部已存在文件**的字节和——tileset.json、feature-manifest.json、
    // tiles/*、textures/* 都算（textures/ 那批是与 GLB 内嵌字节逐字节相同的名字副本，
    // 它们确实占盘，所以确实计数），**不含 conversion-report.json 本身**（此刻还没写）。
    // 遍历失败不静默：记 0 同时在 warnings 里点名不可用——所以这段必须在
    // _report.warnings 快照之前跑，否则这条 warning 自己进不了报告。
    {
        std::uint64_t bytes = 0;
        std::error_code ec;
        bool walked = true;
        for (std::filesystem::recursive_directory_iterator it(Utils::utf8ToPath(_opt.stagingDir), ec), end;
             !ec && it != end; it.increment(ec))
        {
            if (!it->is_regular_file(ec) || ec) { continue; }
            if (it->path().filename() == "conversion-report.json") { continue; }
            const auto sz = it->file_size(ec);
            if (ec) { walked = false; break; }
            bytes += static_cast<std::uint64_t>(sz);
        }
        _report.outputBytes = bytes;
        if (!walked || ec)
        {
            _warnings.push_back("report identity: outputBytes walk over " + _opt.stagingDir
                                + " incomplete (" + ec.message() + "); the number below is partial");
        }
    }

    _report.warnings = _warnings;
    _report.errors = _failures;

    TRACE("finalize: writeReport");
    if (!writeReport(_opt.stagingDir + sep + "conversion-report.json"))
    { _failures.push_back("finalize: write conversion-report.json failed"); discardStaging(); return false; }

    // 自校验通过：把暂存内容搬到最终目录
    std::error_code ec;
    const std::filesystem::path outDir = Utils::utf8ToPath(_opt.outputDir);
    std::filesystem::create_directories(outDir, ec);
    if (ec && !std::filesystem::exists(outDir))
    {
        _failures.push_back("finalize: cannot create output dir " + _opt.outputDir + ": " + ec.message());
        discardStaging();
        return false;
    }

    std::vector<std::filesystem::path> entries;
    for (const auto& e : std::filesystem::directory_iterator(Utils::utf8ToPath(_opt.stagingDir), ec))
    {
        entries.push_back(e.path());
    }

    // ---- 提交保护（本轮）：旧产物不得在"新版本就位前"被破坏 ----
    // 原来的顺序是"逐条 remove_all(dst) → rename"，进程在两条之间死掉，旧数据集就
    // 少了 `tiles/` 又没拿到新的 —— 留下一个"看起来是数据集、实际半残"的目录，
    // 正是手册 14.17 禁止的形态。现在先把上一版**整体**挪到 <output>/.wb-previous
    // （同卷 rename，单条目录操作），新内容全部就位后才删备份；中途失败就回滚。
    // 限制（如实写进文档）：多条目提交做不到严格原子，进程被强杀仍可能停在
    // "旧的在备份里、新的只搬了一半"的中间态；但此时旧数据**还在** `.wb-previous`，
    // 可以人工恢复，且下一轮跑之前会先清掉残留备份。
    const std::filesystem::path backupDir = outDir / ".wb-previous";
    auto isDatasetEntry = [](const std::filesystem::path& p) {
        const std::string n = p.filename().string();
        // 注意**不含 textures/**：那是跑管线时就写在 output 里的纹理缓存
        // （内容寻址、可能被别的任务共用），不是"上一版数据集"的组成部分；
        // 把它挪进备份再靠暂存区搬回来，等于把刚压好的纹理连同备份一起删掉。
        return n == "tileset.json" || n == "feature-manifest.json"
            || n == "conversion-report.json" || n == "conversion-failure.json"
            || n == "tiles";
    };
    std::vector<std::string> backedUp;
    {
        std::filesystem::remove_all(backupDir, ec);   // 上一轮崩在这里留下的残留
        ec.clear();
        for (const auto& e : std::filesystem::directory_iterator(outDir, ec))
        {
            if (ec) { break; }
            if (!isDatasetEntry(e.path())) { continue; }
            std::filesystem::create_directories(backupDir, ec);
            if (ec) { break; }
            std::error_code ecMove;
            std::filesystem::rename(e.path(), backupDir / e.path().filename(), ecMove);
            if (ecMove)
            {
                // 挪不动就退回复制+删除，仍然失败则放弃备份（宁可继续，也不要留半个备份）
                std::filesystem::copy(e.path(), backupDir / e.path().filename(),
                                      std::filesystem::copy_options::recursive
                                      | std::filesystem::copy_options::overwrite_existing, ecMove);
                if (!ecMove) { std::filesystem::remove_all(e.path(), ecMove); }
            }
            if (!ecMove) { backedUp.push_back(e.path().filename().string()); }
        }
        if (!backedUp.empty())
        {
            _warnings.push_back("output commit: moved the previous dataset ("
                + std::to_string(backedUp.size()) + " entr(y/ies)) to "
                + backupDir.u8string() + " before publishing the new one; it is removed only "
                + "after a successful commit, so a crash mid-commit leaves the old dataset "
                + "recoverable there");
        }
    }
    auto rollback = [&]() {
        // 把已经搬进来的新内容挪回暂存目录，再把备份挪回来
        for (const std::filesystem::path& src : entries)
        {
            const std::filesystem::path dst = outDir / src.filename();
            std::error_code e2;
            if (std::filesystem::exists(dst, e2)) { std::filesystem::remove_all(dst, e2); }
        }
        for (const std::string& name : backedUp)
        {
            std::error_code e3;
            std::filesystem::rename(backupDir / name, outDir / name, e3);
            if (e3)
            {
                _warnings.push_back("output commit rollback: could not restore " + name
                                    + " from " + backupDir.u8string() + " (" + e3.message() + ")");
            }
        }
        std::filesystem::remove_all(backupDir, ec);
    };

    for (const std::filesystem::path& src : entries)
    {
        const std::filesystem::path dst = outDir / src.filename();
        TRACE("finalize: move " << src.filename().string());
        // 目标已存在时先删掉，rename 不会覆盖（数据集条目已被挪进 .wb-previous，
        // 这里剩下的只可能是 _i/_o 里非数据集的文件）
        std::filesystem::remove_all(dst, ec);
        ec.clear();
        std::filesystem::rename(src, dst, ec);
        if (ec)
        {
            // 跨卷/异常情况下退回复制
            std::filesystem::copy(src, dst,
                                  std::filesystem::copy_options::recursive
                                  | std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
            {
                _failures.push_back("finalize: cannot move " + src.string() + " -> "
                                    + dst.string() + ": " + ec.message());
                rollback();
                discardStaging();
                return false;
            }
        }
    }
    // 全部就位，备份可以删了
    std::filesystem::remove_all(backupDir, ec);
    discardStaging();
    return true;
}
