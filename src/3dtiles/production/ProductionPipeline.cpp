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
#include <cstdlib>
#include <map>
#include <future>
#include <iterator>
#include <functional>
#include <filesystem>
#include <set>
#include <sstream>
#include "utils/Utils.h"
#include "utils/GltfOptimizer.h"
#include "osgdb_gltf/Extensions.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"

namespace {
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

std::uint64_t currentPeakRss()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, pmc.cb)) { return pmc.PeakWorkingSetSize; }
#endif
    return 0;
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

ProductionPipeline::WriteOutcome
ProductionPipeline::writeContent(osg::Node* node, const std::string& relUri,
                                 bool isInstanceTile)
{
    WriteOutcome out;
    if (!node) { out.error = "null node"; return out; }

    osg::ref_ptr<osg::Node> work = node;
    // 纹理 / 材质优化（图集 + KTX2）
    GltfOptimizer optimizer;
    optimizer.setGltfTextureOptimizationOptions(_opt.textureOptions);
    optimizer.optimize(work.get(), _opt.gltfOptimizerOptions);

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
    if (!osgDB::writeNodeFile(*work.get(), abs, _opt.writeOptions))
    {
        out.error = "writeNodeFile failed: " + abs;
        return out;
    }
    out.bytes = 0;
    {
        std::ifstream f(abs, std::ios::binary | std::ios::ate);
        if (f.is_open()) { out.bytes = static_cast<std::uint64_t>(f.tellg()); }
    }

    // 纹理降采样兜底（手册 7.3 不允许超限瓦片）：分区估算含纹理字节后，
    // 仍有一类超限无法靠切分解决——不可分节点（单对象带几十个材质）。
    // 此时唯一可压的维度是纹理分辨率，逐级降采样重写直到达标或到下限。
    if (out.bytes > _profile.budget.maxTileContentBytes)
    {
        static const unsigned texCaps[] = { 256u, 128u, 64u };
        for (unsigned cap : texCaps)
        {
            TRACE("rewriteWithSmallerTextures cap=" << cap << " curBytes=" << out.bytes);
            ClearTextureExportCacheVisitor clr;
            work->accept(clr);

            GltfOptimizer optimizer2;
            GltfOptimizer::GltfTextureOptimizationOptions topts = _opt.textureOptions;
            topts.maxTextureWidth = std::min(topts.maxTextureWidth, static_cast<int>(cap));
            topts.maxTextureHeight = std::min(topts.maxTextureHeight, static_cast<int>(cap));
            topts.maxTextureAtlasWidth = std::min(topts.maxTextureAtlasWidth, static_cast<int>(cap));
            topts.maxTextureAtlasHeight = std::min(topts.maxTextureAtlasHeight, static_cast<int>(cap));
            optimizer2.setGltfTextureOptimizationOptions(topts);
            optimizer2.optimize(work.get(), _opt.gltfOptimizerOptions);

            if (!osgDB::writeNodeFile(*work.get(), abs, _opt.writeOptions))
            {
                out.error = "writeNodeFile failed (texture rewrite): " + abs;
                return out;
            }
            out.bytes = 0;
            {
                std::ifstream f(abs, std::ios::binary | std::ios::ate);
                if (f.is_open()) { out.bytes = static_cast<std::uint64_t>(f.tellg()); }
            }
            if (out.bytes <= _profile.budget.maxTileContentBytes) { break; }
        }
    }

    // 说明：超限且不可再压的内节点代理不在这里剥纹理——写时剥离需要改动
    // 与源模型共享的 StateSet（materialize 复用），并行构建下会破坏引用计数
    // （v8-v10 段错误根因）。内节点代理的超预算问题在 processTileHeavy 里
    // 以"构建时就建成无纹理版"解决，见 FarLodTextureStripper。
    (void)isInstanceTile;
    out.ok = true;
    return out;
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
        w.content = _model.materialize(_prims, items, true);
        TRACE("leaf materialize done");
    }
    else
    {
        ProxyMeshBuilder builder(_model, _profile, _prims);
        TRACE("proxy build pn=" << pn << " items=" << items.size());
        const ProxyResult pr = builder.build(items, _profile.budget.targetTrianglesPerTile,
                                             !_profile.strictIdentity);
        // 注意：聚合 fallback 耗尽后 pr.node 仍是最后一次聚合结果（有效），
        // failure 已置位。只检查 node.valid() 会让超限代理漏网到写出阶段
        // （lugouqiao_v11 实测 7 个瓦片 14~50 万三角形漏过 selfValidate 之前的
        // 全部检查，就是这个洞）。
        if (!pr.node.valid() || !pr.failure.empty())
        {
            w.failure = "proxy generation failed for node " + std::to_string(pn)
                      + ": " + (pr.failure.empty() ? "invalid proxy node" : pr.failure);
            return w;
        }
        w.content = pr.node;
        w.isProxy = true;
        w.aggregate = pr.aggregate;
        w.aggregateSources = pr.aggregateSources;

        // 内节点代理大小预估：几何按目标三角形数近似，纹理取子树全部纹理字节
        // （代理会携带子树的所有材质纹理，这是根/中层瓦片超限的主因）。
        // 预估超限 → 构建时直接建成无纹理版（远景 LOD 无纹理是常规做法），
        // 通过 w.texturesDropped 上报记 warning，不静默缺内容。
        // 必须在构建后立刻去纹理且只动私有 StateSet——若留到写出阶段剥离，
        // 会改动与源模型共享的 StateSet（v8-v10 段错误根因），见
        // FarLodTextureStripper 注释。
        std::uint64_t texBytes = 0;
        std::uint64_t allBytes = 0;
        for (std::uint32_t itemId : items)
        {
            if (itemId >= _prims.size()) { continue; }
            const PrimRef& p = _prims[itemId];
            allBytes += p.byteEstimate;
            const std::uint64_t geoBytes =
                p.vertexCount * 32ull + static_cast<std::uint64_t>(p.tris.size()) * 160ull;
            texBytes += (p.byteEstimate > geoBytes) ? (p.byteEstimate - geoBytes) : 0ull;
        }
        const std::uint64_t proxyEstimate =
            std::min<std::uint64_t>(allBytes,
                static_cast<std::uint64_t>(_profile.budget.targetTrianglesPerTile) * 200ull
                    + texBytes);
        if (pr.needsTextureless || proxyEstimate > _profile.budget.maxTileContentBytes)
        {
            TRACE("proxy far-lod detexture pn=" << pn << " estimate=" << proxyEstimate);
            FarLodTextureStripper stripper;
            w.content->accept(stripper);
            w.texturesDropped = true;
        }
    }

    if (!w.content.valid()) { return w; }

    // ---- 几何误差评估（手册 9）----
    std::vector<SourceModel::Triangle> srcTris;
    TRACE("collectTriangles pn=" << pn);
    _model.collectTriangles(_prims, items, srcTris);
    TRACE("collectTriangles done src=" << srcTris.size());
    TriangleSoup src, prox;
    for (const SourceModel::Triangle& t : srcTris) { src.add(t.a, t.b, t.c); }
    collectTrianglesFromNode(w.content.get(), prox);
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
        const ErrorResult er = ev.evaluate(src, prox);
        TRACE("evaluate done");
        w.measuredProxyErrorM = er.geometricErrorM();
        w.errorLowerM = er.lowerBoundM;
        w.errorUpperM = er.upperBoundM;
        w.errorStatus = er.status;
        w.certified = er.certified;
        w.boundLoose = er.boundLoose;
        w.p95M = er.p95M;
    }
    return w;
}

// 主线程串行收尾：写出 + 状态聚合 + seam 校验。
void ProductionPipeline::finalizeTileFromWork(int pn, int id, TileHeavyWork&& w)
{
    TRACE("finalize id=" << id << " begin pn=" << pn);
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
            return;
        }
        _warnings.push_back(w.failure + " -> tile carries no far-LOD content, children "
                            "still cover " + std::to_string(scopeCount)
                            + " source features");
        ++_report.proxySkippedCount;
        return;
    }

    const bool isLeaf = w.leaf;
    osg::ref_ptr<osg::Node> content = w.content;

    if (content.valid())
    {
        const std::string relUri = std::string("tiles") + OSG_GIS_PLUGINS_PATH_SPLIT_STRING
                                 + "t" + std::to_string(id) + ".b3dm";
        TRACE("writeContent pn=" << pn);
        WriteOutcome wo = writeContent(content.get(), relUri, false);
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
                                    ": far-LOD proxy built without textures to fit budget (bytes=" +
                                    std::to_string(wo.bytes) + ")");
            }
            const bool overBytes = wo.bytes > _profile.budget.maxTileContentBytes;
            const bool overTris = wo.triangles > _profile.budget.maxTrianglesPerTile;
            if (overBytes || overTris)
            {
                const std::string why = std::string(overBytes ? "bytes=" : "triangles=")
                    + std::to_string(overBytes ? wo.bytes : static_cast<std::uint64_t>(wo.triangles))
                    + " > " + std::to_string(overBytes ? _profile.budget.maxTileContentBytes
                                                       : _profile.budget.maxTrianglesPerTile);
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
                    std::filesystem::remove(abs, ec);
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
    auto submit = [this, &inflight, &idToPn](int pn) {
        const int id = idToPn[pn];
        inflight.emplace(pn, std::async(std::launch::async, [this, pn, id]() {
            return processTileHeavy(pn, id);
        }));
    };
    while (inflight.size() < window && next < pns.size()) { submit(pns[next++]); }

    // 主线程按 tile id 升序收结果并串行写出，保证与串行版完全一致的
    // _mapper.addTile 顺序、report 聚合顺序与 staging 文件名。
    // 注意：提交顺序按工作量降序，而收集按 id 升序——id 靠前的小任务
    // 可能还没进窗口，此时主线程直接同步执行它（窗口任务照常并行），
    // 绝不能解引用 end() 迭代器。
    for (int id = 0; id < static_cast<int>(_tiles.size()); ++id)
    {
        const int pn = idToPn[id];
        auto it = inflight.find(pn);
        if (it == inflight.end())
        {
            TileHeavyWork w = processTileHeavy(pn, id);
            finalizeTileFromWork(pn, id, std::move(w));
            continue;
        }
        TileHeavyWork w = it->second.get();
        inflight.erase(it);
        finalizeTileFromWork(pn, id, std::move(w));
        if (next < pns.size()) { submit(pns[next++]); }
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

        for (std::size_t start = 0; start < insts.size(); start += chunk)
        {
            const std::size_t end = std::min(insts.size(), start + chunk);
            osg::ref_ptr<osg::Group> group = new osg::Group;
            for (std::size_t k = start; k < end; ++k)
            {
                osg::ref_ptr<osg::MatrixTransform> mt = new osg::MatrixTransform;
                mt->setMatrix(insts[k]->matrix);
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
            WriteOutcome wo = writeContent(group.get(), relUri, true);
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
                std::filesystem::remove(abs, ec);
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
    _opt = opt;
    // 暂存目录：<outputDir>/.wb-staging（可在 Options 里覆盖）
    if (_opt.stagingDir.empty())
    {
        _opt.stagingDir = _opt.outputDir + OSG_GIS_PLUGINS_PATH_SPLIT_STRING + ".wb-staging";
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

    // ---- import / normalize ----
    Timer tImport;
    TRACE("model.build");
    if (!_model.build(input))
    {
        _failures.push_back("SourceModel build failed (no geometry)");
        return false;
    }
    _report.addStage("import_normalize", tImport.elapsed());
    _report.sourceFeatureCount = _model.featureCount();

    Timer tSeam;
    TRACE("seams");
    _seams.build(_model);
    _report.addStage("seam_registry", tSeam.elapsed());

    // ---- partition ----
    Timer tPart;
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

    // ---- proxy / encode / error ----
    Timer tBuild;
    TRACE("buildTrees");
    _tiles.clear();
    const int b3dmRoot = _prims.empty() ? -1 : buildB3dmTree(rootPn);
    TRACE("b3dmRoot=" << b3dmRoot);
    const int i3dmRoot = buildInstanceTree();
    TRACE("i3dmRoot=" << i3dmRoot);
    _report.addStage("proxy_encode_error", tBuild.elapsed());

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

    std::ofstream f(path, std::ios::binary);
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

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) { return false; }
    f << m.dump(2);
    f.close();
    return true;
}

bool ProductionPipeline::writeReport(const std::string& path) const
{
    std::ofstream f(path, std::ios::binary);
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
// 手册 14.17：异常终止与重试
//
// 之前是边跑边往 outputDir 里写瓦片，run() 失败后 main 仍然把 tileset.json
// 写进同一个目录，磁盘上就留下一个"看起来成功其实内容不全"的数据集。
// 现在改成：全部产物先落暂存目录，自校验通过后才整体搬到 outputDir。
// ---------------------------------------------------------------------------
bool ProductionPipeline::selfValidate(const std::string& tilesetJsonPath)
{
    std::ifstream f(tilesetJsonPath, std::ios::binary);
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
        std::ifstream cf(abs, std::ios::binary);
        if (!cf.is_open())
        {
            _failures.push_back("selfValidate: content uri missing on disk: " + u);
            ok = false;
        }
        else
        {
            cf.seekg(0, std::ios::end);
            const std::uint64_t bytes = static_cast<std::uint64_t>(cf.tellg());
            cf.close();
            if (bytes > _profile.budget.maxTileContentBytes)
            {
                _failures.push_back("selfValidate: content over hard byte limit: " + u
                                    + " (" + std::to_string(bytes) + " > "
                                    + std::to_string(_profile.budget.maxTileContentBytes) + ")");
                ok = false;
            }
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
    const std::filesystem::path p(_opt.stagingDir);
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

    // report 必须在自校验之后落盘：覆盖计数与聚合抽稀 warning 都是校验阶段才产生的
    _report.warnings = _warnings;
    _report.errors = _failures;
    TRACE("finalize: writeReport");
    if (!writeReport(_opt.stagingDir + sep + "conversion-report.json"))
    { _failures.push_back("finalize: write conversion-report.json failed"); discardStaging(); return false; }

    // 自校验通过：把暂存内容搬到最终目录
    std::error_code ec;
    const std::filesystem::path outDir(_opt.outputDir);
    std::filesystem::create_directories(outDir, ec);
    if (ec && !std::filesystem::exists(outDir))
    {
        _failures.push_back("finalize: cannot create output dir " + _opt.outputDir + ": " + ec.message());
        discardStaging();
        return false;
    }

    std::vector<std::filesystem::path> entries;
    for (const auto& e : std::filesystem::directory_iterator(_opt.stagingDir, ec))
    {
        entries.push_back(e.path());
    }
    for (const std::filesystem::path& src : entries)
    {
        const std::filesystem::path dst = outDir / src.filename();
        TRACE("finalize: move " << src.filename().string());
        // 目标已存在时先删掉，rename 不会覆盖
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
                discardStaging();
                return false;
            }
        }
    }
    discardStaging();
    return true;
}
