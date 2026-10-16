#include "3dtiles/production/ProductionPipeline.h"
#include "3dtiles/production/Trace.h"
#include <osg/ComputeBoundsVisitor>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/CoordinateSystemNode>
#include <osgDB/WriteFile>
#include <osgDB/FileUtils>
#include <osgDB/ReadFile>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <map>
#include <iterator>
#include <functional>
#include <filesystem>
#include <set>
#include <sstream>
#include "utils/Utils.h"
#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
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
ProductionPipeline::writeContent(osg::Node* node, const std::string& relUri, bool isInstanceTile)
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
            // 由名字回查 feature：名字在 materialize 时设为源对象名；
            // 重名时退化为按出现顺序匹配（manifest 会记录 isProxy 供核对）
            FeatureId found = INVALID_FEATURE;
            for (const SourceObject& obj : _model.objects)
            {
                if (obj.name == n) { found = obj.globalFeatureId; break; }
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
    (void)isInstanceTile;
    out.ok = true;
    return out;
}

int ProductionPipeline::buildB3dmTree(int pn)
{
    const PartitionNode& P = _partitioner->nodes[pn];
    const int id = static_cast<int>(_tiles.size());
    _tiles.push_back(ProductionTile());
    _tiles[id].id = id;

    // 子节点优先（自底向上）
    for (int c : P.children)
    {
        const int cid = buildB3dmTree(c);
        _tiles[id].children.push_back(cid);
    }

    std::vector<std::uint32_t> items;
    TRACE("buildB3dm pn=" << pn << " children=" << P.children.size());
    _partitioner->subtreeItems(pn, items);
    const bool isLeaf = P.children.empty();

    osg::ref_ptr<osg::Node> content;
    bool isProxy = false;
    bool aggregate = false;
    std::map<FeatureId, std::vector<FeatureId>> aggSources;

    if (isLeaf)
    {
        // 叶保留原始高精度几何，只允许坐标规范化与无损重建（手册 7.4）
        TRACE("leaf materialize pn=" << pn << " items=" << items.size());
        content = _model.materialize(_prims, items, true);
        TRACE("leaf materialize done");
        _tiles[id].isLeaf = true;
    }
    else
    {
        ProxyMeshBuilder builder(_model, _profile, _prims);
        TRACE("proxy build pn=" << pn << " items=" << items.size());
        const ProxyResult pr = builder.build(items, _profile.budget.targetTrianglesPerTile,
                                             !_profile.strictIdentity);
        if (!pr.node.valid())
        {
            _failures.push_back("proxy generation failed for node " + std::to_string(pn)
                                + ": " + pr.failure);
        }
        else
        {
            content = pr.node;
            isProxy = true;
            aggregate = pr.aggregate;
            aggSources = pr.aggregateSources;
        }
    }

    if (content.valid())
    {
        const std::string relUri = std::string("tiles") + OSG_GIS_PLUGINS_PATH_SPLIT_STRING
                                 + "t" + std::to_string(id) + ".b3dm";
        TRACE("writeContent pn=" << pn);
        WriteOutcome wo = writeContent(content.get(), relUri, false);
        TRACE("writeContent done");
        if (!wo.ok)
        {
            _warnings.push_back("tile " + std::to_string(id) + ": " + wo.error);
        }
        else
        {
            const bool overBudget = wo.bytes > _profile.budget.maxTileContentBytes;
            if (overBudget)
            {
                // 手册 7.3：不允许输出超限瓦片
                _failures.push_back("tile " + std::to_string(id) + " content " +
                                    std::to_string(wo.bytes) + " bytes exceeds hard limit " +
                                    std::to_string(_profile.budget.maxTileContentBytes) +
                                    " (triangles=" + std::to_string(wo.triangles) + ")");
                _tiles[id].hasContent = false;
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

                _mapper.addTile(_tiles[id].uri, wo.featureOrder, isProxy);
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

    _tiles[id].isProxy = isProxy;
    _tiles[id].aggregate = aggregate;
    _tiles[id].aggregateSources = aggSources;
    if (aggregate) { ++_report.aggregateProxyCount; }

    // ---- 几何误差评估（手册 9）----
    if (_tiles[id].hasContent)
    {
        std::vector<SourceModel::Triangle> srcTris;
        TRACE("collectTriangles pn=" << pn);
        _model.collectTriangles(_prims, items, srcTris);
        TRACE("collectTriangles done src=" << srcTris.size());
        TriangleSoup src, prox;
        for (const SourceModel::Triangle& t : srcTris) { src.add(t.a, t.b, t.c); }
        collectTrianglesFromNode(content.get(), prox);
        if (isLeaf && !_opt.vertexCompression)
        {
            // 叶保留原始表面：只有确认没有任何有损变换时才可写 0
            _tiles[id].measuredProxyErrorM = 0.0;
            _tiles[id].errorUpperM = 0.0;
            _tiles[id].errorLowerM = 0.0;
            _tiles[id].errorStatus = "EXACT_LEAF";
        }
        else
        {
            GeometryErrorEvaluator ev(_profile.error);
            TRACE("evaluate pn=" << pn << " srcT=" << src.size() << " proxT=" << prox.size());
            const ErrorResult er = ev.evaluate(src, prox);
            TRACE("evaluate done");
            _tiles[id].measuredProxyErrorM = er.geometricErrorM();
            _tiles[id].errorLowerM = er.lowerBoundM;
            _tiles[id].errorUpperM = er.upperBoundM;
            _tiles[id].errorStatus = er.status;
            if (er.certified) { ++_report.nodesCertified; }
            if (er.boundLoose) { ++_report.nodesBoundLoose; }
            if (er.p95M > 0) { _report.errorP95 = std::max(_report.errorP95, er.p95M); }
        }

        // 接缝保留校验（手册 8.3）
        if (isProxy && _seams.seamVertexCount() > 0)
        {
            const std::pair<std::size_t, std::size_t> keep = _seams.verifyPreserved(content.get(), items);
            if (keep.second > 0 && keep.first < keep.second)
            {
                _warnings.push_back("tile " + std::to_string(id) + ": seam vertices preserved "
                                    + std::to_string(keep.first) + "/" + std::to_string(keep.second));
            }
        }
    }
    return id;
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
        const unsigned chunk = _profile.budget.maxInstancesPerTile;
        for (std::size_t start = 0; start < insts.size(); start += chunk)
        {
            const std::size_t end = std::min(insts.size(), start + chunk);
            // 原型几何（局部坐标）+ 每个实例一个 MatrixTransform
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
            if (wo.ok && wo.bytes <= _profile.budget.maxTileContentBytes)
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
            }
            else if (wo.ok)
            {
                _failures.push_back("instance tile " + std::to_string(childId) + " over hard budget: "
                                    + std::to_string(wo.bytes) + " bytes");
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
    _report.tileCount = static_cast<unsigned>(_tiles.size());
    _report.contentTileCount = 0;
    for (const ProductionTile& t : _tiles)
    {
        if (t.hasContent) { ++_report.contentTileCount; }
    }
    _report.maxDepth = 0;
    {
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
    _report.errorMax = _tiles[_root].geometricErrorM;
    double sum = 0.0;
    unsigned n = 0;
    for (const ProductionTile& t : _tiles)
    {
        if (t.hasContent) { sum += t.measuredProxyErrorM; ++n; }
    }
    _report.errorMean = (n > 0) ? sum / n : 0.0;
    _report.manifestOccurrences = static_cast<unsigned>(_mapper.occurrenceCount());

    for (const std::string& e : _mapper.validate()) { _failures.push_back("featureMapper: " + e); }

    _report.totalSeconds = total.elapsed();
    _report.peakRssBytes = currentPeakRss();
    _report.warnings = _warnings;
    _report.errors = _failures;
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
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) { return false; }
    f << _mapper.manifestJson(_model).dump(2);
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
    if (_root < 0)
    {
        _failures.push_back("finalize: no tile tree produced");
        discardStaging();
        return false;
    }
    if (!_failures.empty())
    {
        // run() 阶段已经有明确失败项：不产出任何数据集
        discardStaging();
        return false;
    }

    const std::string sep = OSG_GIS_PLUGINS_PATH_SPLIT_STRING;
    const std::string tsPath = _opt.stagingDir + sep + "tileset.json";
    if (!writeTilesetJson(tsPath)) { _failures.push_back("finalize: write tileset.json failed"); discardStaging(); return false; }
    if (!writeManifest(_opt.stagingDir + sep + "feature-manifest.json"))
    { _failures.push_back("finalize: write feature-manifest.json failed"); discardStaging(); return false; }
    if (!writeReport(_opt.stagingDir + sep + "conversion-report.json"))
    { _failures.push_back("finalize: write conversion-report.json failed"); discardStaging(); return false; }

    if (!selfValidate(tsPath)) { discardStaging(); return false; }

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
