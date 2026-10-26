#include "3dtiles/production/ProxyMeshBuilder.h"
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/PrimitiveSet>
#include <osg/BoundingBox>
#include <osg/ComputeBoundsVisitor>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include "utils/Utils.h"
#include "utils/Simplifier.h"

using namespace osgGISPlugins;
using namespace osgGISPlugins::production;

ProxyMeshBuilder::ProxyMeshBuilder(const SourceModel& model, const ProductionProfile& profile,
                                   const std::vector<PrimRef>& primTable)
    : _model(model), _prims(primTable), _profile(profile)
{
}

unsigned ProxyMeshBuilder::countTriangles(osg::Node* node)
{
    if (!node) { return 0; }
    Utils::TriangleCounterVisitor tcv;
    node->accept(tcv);
    return tcv.count;
}

namespace {
bool proxyDebug()
{
    static const bool on = std::getenv("WB_PROXY_DEBUG") != nullptr;
    return on;
}

// 内容字节估算，系数与 SourceModel 的 PrimRef::byteEstimate 一致
// （实测校准 ~150 B/三角形，见 SourceModel.cpp 注释）。
// 代理必须同时满足三角形预算与字节预算：实测发现部分瓦片 meshopt
// 简化受锁边限制收敛不到三角形目标，只看三角形数会漏掉字节超限。
struct ByteEstimateVisitor : public osg::NodeVisitor
{
    std::uint64_t tris = 0, verts = 0, texBytes = 0, texBytesFloor = 0;
    std::set<const osg::Geometry*> seen;
    std::set<const osg::Image*> seenImgs;
    ByteEstimateVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

    void collectTextureBytes(const osg::StateSet* ss)
    {
        if (!ss) { return; }
        const unsigned units = ss->getNumTextureAttributeLists();
        for (unsigned unit = 0; unit < units; ++unit)
        {
            const osg::Texture2D* tex =
                dynamic_cast<const osg::Texture2D*>(ss->getTextureAttribute(unit, osg::StateAttribute::TEXTURE));
            if (!tex) { continue; }
            const osg::Image* img = tex->getImage();
            if (!img) { continue; }
            if (!seenImgs.insert(img).second) { continue; }
            // 纹理字节按解码 RGBA8 的 1/8 折算（KTX2/ETC1S 经验压缩率），
            // atlas 合并收益不扣（保守）。纹理总是内嵌 glb bufferView
            //（Osg2Gltf 固定行为），大聚合瓦片纹理字节占大头，不能漏估。
            const std::uint64_t decoded = img->getTotalSizeInBytes() / 8ull;
            texBytes += decoded;
            // 降到下限分辨率后按面积折算：远景代理可以先降分辨率再考虑去纹理
            const unsigned edge = std::max<unsigned>(img->s(), img->t());
            const double ratio = edge <= proxyTextureFloorEdgePx
                ? 1.0
                : (static_cast<double>(proxyTextureFloorEdgePx) / edge)
                  * (static_cast<double>(proxyTextureFloorEdgePx) / edge);
            texBytesFloor += static_cast<std::uint64_t>(decoded * ratio);
        }
    }
    void apply(osg::Geode& geode) override
    {
        collectTextureBytes(geode.getStateSet());
        traverse(geode);
    }
    void apply(osg::Drawable& d) override
    {
        collectTextureBytes(d.getStateSet());
        if (osg::Geometry* g = d.asGeometry())
        {
            if (!seen.insert(g).second) { return; }
            const unsigned pc = g->getNumPrimitiveSets();
            for (unsigned i = 0; i < pc; ++i)
            {
                if (const osg::PrimitiveSet* ps = g->getPrimitiveSet(i))
                {
                    tris += ps->getNumPrimitives();
                }
            }
            if (const osg::Array* va = g->getVertexArray()) { verts += va->getNumElements(); }
        }
    }
};
} // namespace

ProxyBytes ProxyMeshBuilder::estimateBytes(osg::Node* node)
{
    ProxyBytes s;
    if (!node) { return s; }
    ByteEstimateVisitor v;
    node->accept(v);
    // 几何部分：200 B/三角形覆盖 position/normal/uv/索引/容器开销，
    // 实测 longxiang 超限瓦片 116~242 B/tri。
    s.geo = v.tris * 200ull + v.verts * 48ull;
    // 纹理部分：内嵌 glb 的 KTX2 字节按解码 /8 折算（lugouqiao 实测
    // 纹理密集瓦片字节为纯几何估算的 10 倍以上，漏估会直接超限）。
    s.texFull = v.texBytes;
    s.texFloor = v.texBytesFloor;
    return s;
}

double ProxyMeshBuilder::simplifyNode(osg::Node* node, unsigned targetTriangles,
                                      double targetErrorMeters, bool lockBorders) const
{
    const unsigned before = countTriangles(node);
    if (before == 0 || targetTriangles >= before) { return 1.0; }

    osg::ComputeBoundsVisitor cbv;
    node->accept(cbv);
    const osg::BoundingBox nbb = cbv.getBoundingBox();
    const double diag = nbb.valid() ? (nbb._max - nbb._min).length() : 1.0;

    // meshopt 的 target_error 与顶点数据同单位。关键约束冲突：面数目标与
    // 误差上限不一定同时可达 —— 一个 cell 内独立 feature 越多，压到配额所需
    // 的误差越大（实测 124m cell 内 144 个 8m 方块压到 15 面需要 ~30m 误差，
    // 而 6m 上限让 meshopt 停在 288→234 就再也减不动，聚合兜底因此永远
    // 收敛不了，正是 lugouqiao_v11 六轮聚合卡在 50 万面的根因）。
    // 这里按 4 倍递增误差重试直到达到面数目标（上限取节点对角线）；
    // 真实误差始终由 GeometryErrorEvaluator 独立复核，不在这里虚报（手册 9.1）。
    unsigned after = before;
    double err = std::max(1e-6, targetErrorMeters);
    for (int step = 0; step < 8 && after > targetTriangles; ++step)
    {
        const double ratio = static_cast<double>(targetTriangles)
                           / static_cast<double>(std::max<unsigned>(after, 1u));
        // 第 0 步保形（非 sloppy），后续用 sloppy 强行逼近目标
        Simplifier simplifier(ratio, step > 0, lockBorders,
                              static_cast<float>(err));
        node->accept(simplifier);
        const unsigned now = countTriangles(node);
        if (proxyDebug())
        {
            OSG_NOTICE << "[simplify] step=" << step << " " << after << " -> " << now
                       << " target=" << targetTriangles << " ratio=" << ratio
                       << " err=" << err << " diag=" << diag
                       << " lock=" << lockBorders << std::endl;
        }
        const bool stalled = (now >= after);
        after = now;
        if (stalled && err >= diag) { break; }
        err = std::min(err * 4.0, diag);
    }
    return (before > 0) ? static_cast<double>(after) / static_cast<double>(before) : 1.0;
}

namespace {

// 属性签名：只有连续属性布局一致的几何才能安全合并进同一个 primitive。
// 离散属性（feature ID 槽）按槽位与元素类型参与签名，避免错位（手册 5.3）。
struct AttrSig
{
    bool hasNormal = false;
    int normalType = 0;
    bool hasUv0 = false;
    int uv0Type = 0;
    bool hasColor = false;
    int colorType = 0;
    std::vector<int> attribTypes;

    bool operator<(const AttrSig& o) const
    {
        if (hasNormal != o.hasNormal) { return hasNormal < o.hasNormal; }
        if (normalType != o.normalType) { return normalType < o.normalType; }
        if (hasUv0 != o.hasUv0) { return hasUv0 < o.hasUv0; }
        if (uv0Type != o.uv0Type) { return uv0Type < o.uv0Type; }
        if (hasColor != o.hasColor) { return hasColor < o.hasColor; }
        if (colorType != o.colorType) { return colorType < o.colorType; }
        return attribTypes < o.attribTypes;
    }
};

int arrayType(const osg::Array* a) { return a ? static_cast<int>(a->getType()) : -1; }

unsigned countGeometryTriangles(const osg::Geometry* g)
{
    if (!g) { return 0; }
    unsigned t = 0;
    for (unsigned i = 0; i < g->getNumPrimitiveSets(); ++i)
    {
        if (const osg::PrimitiveSet* ps = g->getPrimitiveSet(i)) { t += ps->getNumPrimitives(); }
    }
    return t;
}

AttrSig signatureOf(osg::Geometry* g)
{
    AttrSig s;
    s.hasNormal = g->getNormalArray() != nullptr;
    s.normalType = arrayType(g->getNormalArray());
    s.hasUv0 = g->getTexCoordArray(0) != nullptr;
    s.uv0Type = arrayType(g->getTexCoordArray(0));
    s.hasColor = g->getColorArray() != nullptr;
    s.colorType = arrayType(g->getColorArray());
    for (unsigned a = 0; a < g->getNumVertexAttribArrays(); ++a)
    {
        s.attribTypes.push_back(arrayType(g->getVertexAttribArray(a)));
    }
    return s;
}

// 按类型把 src 的元素追加到 dst；dst 由 first 克隆而来，类型必然匹配。
bool appendElements(osg::Array* dst, const osg::Array* src)
{
    if (!dst || !src) { return false; }
    // 分支按 src 类型走，却把 dst 也按该类型 cast：dst 不同步长就错，写出去是
    // 堆越界。调用方用签名分组保证同类型，但这条保证是隐式的、跨函数的，
    // 这里显式拒绝，别让它变成一个"只在别的文件里成立"的前提（A-39）。
    if (dst->getType() != src->getType()) { return false; }
#define OSGGIS_APPEND_CASE(ARRTYPE, ENUMVAL)                              \
    if (src->getType() == osg::Array::ENUMVAL)                             \
    {                                                                     \
        ARRTYPE* d = static_cast<ARRTYPE*>(dst);                          \
        const ARRTYPE* s = static_cast<const ARRTYPE*>(src);              \
        d->insert(d->end(), s->begin(), s->end());                        \
        return true;                                                      \
    }
    OSGGIS_APPEND_CASE(osg::FloatArray, FloatArrayType)
    OSGGIS_APPEND_CASE(osg::DoubleArray, DoubleArrayType)
    OSGGIS_APPEND_CASE(osg::Vec2Array, Vec2ArrayType)
    OSGGIS_APPEND_CASE(osg::Vec2dArray, Vec2dArrayType)
    OSGGIS_APPEND_CASE(osg::Vec3Array, Vec3ArrayType)
    OSGGIS_APPEND_CASE(osg::Vec3dArray, Vec3dArrayType)
    OSGGIS_APPEND_CASE(osg::Vec4Array, Vec4ArrayType)
    OSGGIS_APPEND_CASE(osg::Vec4dArray, Vec4dArrayType)
    OSGGIS_APPEND_CASE(osg::Vec4bArray, Vec4bArrayType)
    OSGGIS_APPEND_CASE(osg::Vec2bArray, Vec2bArrayType)
    OSGGIS_APPEND_CASE(osg::Vec3bArray, Vec3bArrayType)
#undef OSGGIS_APPEND_CASE
    return false;
}

// 构造与 src 同类型的空数组（不复制数据），用于合并时的目标缓冲。
// 直接按 Type 分派，避免依赖 osg::Array 基类的 clone/resize 语义差异。
osg::ref_ptr<osg::Array> cloneEmpty(const osg::Array* src)
{
    if (!src) { return nullptr; }
    osg::ref_ptr<osg::Array> dst;
#define OSGGIS_NEW_CASE(ARRTYPE, ENUMVAL)                     \
    if (src->getType() == osg::Array::ENUMVAL)                 \
    {                                                          \
        dst = new ARRTYPE;                                     \
        static_cast<ARRTYPE*>(dst.get())->reserve(src->getNumElements()); \
        return dst;                                            \
    }
    OSGGIS_NEW_CASE(osg::FloatArray, FloatArrayType)
    OSGGIS_NEW_CASE(osg::DoubleArray, DoubleArrayType)
    OSGGIS_NEW_CASE(osg::Vec2Array, Vec2ArrayType)
    OSGGIS_NEW_CASE(osg::Vec2dArray, Vec2dArrayType)
    OSGGIS_NEW_CASE(osg::Vec3Array, Vec3ArrayType)
    OSGGIS_NEW_CASE(osg::Vec3dArray, Vec3dArrayType)
    OSGGIS_NEW_CASE(osg::Vec4Array, Vec4ArrayType)
    OSGGIS_NEW_CASE(osg::Vec4dArray, Vec4dArrayType)
    OSGGIS_NEW_CASE(osg::Vec4bArray, Vec4bArrayType)
    OSGGIS_NEW_CASE(osg::Vec2bArray, Vec2bArrayType)
    OSGGIS_NEW_CASE(osg::Vec3bArray, Vec3bArrayType)
#undef OSGGIS_NEW_CASE
    return nullptr;
}

bool collectIndexList(const osg::PrimitiveSet* ps, std::vector<unsigned>& out)
{
    if (ps->getMode() != osg::PrimitiveSet::TRIANGLES) { return false; }
    if (const auto* du = dynamic_cast<const osg::DrawElementsUInt*>(ps))
    {
        out.assign(du->begin(), du->end());
        return true;
    }
    if (const auto* dus = dynamic_cast<const osg::DrawElementsUShort*>(ps))
    {
        out.assign(dus->begin(), dus->end());
        return true;
    }
    if (const auto* da = dynamic_cast<const osg::DrawArrays*>(ps))
    {
        const unsigned first = static_cast<unsigned>(da->getFirst());
        const unsigned count = static_cast<unsigned>(da->getCount() > 0 ? da->getCount() : 0);
        out.reserve(count);
        for (unsigned i = 0; i < count; ++i) { out.push_back(first + i); }
        return true;
    }
    return false;
}

// 二维位交错：让等间隔抽样落在四叉树格点上，得到真正的空间均匀抽稀。
// 按行/按 ID 顺序抽样的话，抽样间隔恰好等于一行时会造成整行空洞
// （实测聚合代理最大点到代理距离因此超过 cell 尺度）。
unsigned morton2(unsigned x, unsigned y)
{
    unsigned r = 0;
    for (unsigned b = 0; b < 8; ++b)
    {
        r |= ((x >> b) & 1u) << (2 * b);
        r |= ((y >> b) & 1u) << (2 * b + 1);
    }
    return r;
}

// 把同一签名、同一 StateSet 的几何合并成一个 drawable（保持顶点顺序确定性）。
// 聚合代理是远景粗内容，合并大幅减少 drawcall 与"每几何最小面数"下限，
// 这是让海量小 feature 也能压进硬预算的关键（手册 7.3 第 5 点）。
osg::ref_ptr<osg::Geometry> mergeGeometries(const std::vector<osg::Geometry*>& group)
{
    if (group.empty()) { return nullptr; }
    osg::ref_ptr<osg::Geometry> out = new osg::Geometry;
    osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
    osg::ref_ptr<osg::Array> nrm = cloneEmpty(group.front()->getNormalArray());
    osg::ref_ptr<osg::Array> uv = cloneEmpty(group.front()->getTexCoordArray(0));
    osg::ref_ptr<osg::Array> col = cloneEmpty(group.front()->getColorArray());
    std::vector<osg::ref_ptr<osg::Array>> attribs;
    for (unsigned a = 0; a < group.front()->getNumVertexAttribArrays(); ++a)
    {
        attribs.push_back(cloneEmpty(group.front()->getVertexAttribArray(a)));
    }
    osg::ref_ptr<osg::DrawElementsUInt> idx =
        new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);

    unsigned vertexOffset = 0;
    for (osg::Geometry* g : group)
    {
        const osg::Vec3Array* gp = dynamic_cast<const osg::Vec3Array*>(g->getVertexArray());
        if (!gp) { continue; }
        pos->insert(pos->end(), gp->begin(), gp->end());
        if (nrm.valid() && !appendElements(nrm.get(), g->getNormalArray())) { nrm = nullptr; }
        if (uv.valid() && !appendElements(uv.get(), g->getTexCoordArray(0))) { uv = nullptr; }
        if (col.valid() && !appendElements(col.get(), g->getColorArray())) { col = nullptr; }
        for (std::size_t a = 0; a < attribs.size(); ++a)
        {
            if (!attribs[a].valid()) { continue; }
            const osg::Array* src = g->getVertexAttribArray(static_cast<unsigned>(a));
            if (!src || !appendElements(attribs[a].get(), src)) { attribs[a] = nullptr; }
        }
        for (unsigned p = 0; p < g->getNumPrimitiveSets(); ++p)
        {
            std::vector<unsigned> li;
            if (!collectIndexList(g->getPrimitiveSet(p), li)) { continue; }
            for (const unsigned v : li)
            {
                if (v < gp->size()) { idx->push_back(vertexOffset + v); }
            }
        }
        vertexOffset += static_cast<unsigned>(gp->size());
    }
    if (pos->empty() || idx->size() < 3) { return nullptr; }

    out->setVertexArray(pos.get());
    if (nrm.valid()) { out->setNormalArray(nrm.get(), osg::Array::BIND_PER_VERTEX); }
    if (uv.valid()) { out->setTexCoordArray(0, uv.get(), osg::Array::BIND_PER_VERTEX); }
    if (col.valid()) { out->setColorArray(col.get(), osg::Array::BIND_PER_VERTEX); }
    for (std::size_t a = 0; a < attribs.size(); ++a)
    {
        if (attribs[a].valid())
        {
            out->setVertexAttribArray(static_cast<unsigned>(a), attribs[a].get(),
                                      osg::Array::BIND_PER_VERTEX);
        }
    }
    out->addPrimitiveSet(idx.get());
    out->setStateSet(group.front()->getStateSet());
    out->dirtyBound();
    return out.get();
}

// 空间体素键：聚合代理的分组单位
struct Cell {
    long long x, y, z;
    bool operator<(const Cell& o) const {
        if (x != o.x) { return x < o.x; }
        if (y != o.y) { return y < o.y; }
        return z < o.z;
    }
};

} // namespace

osg::ref_ptr<osg::Node> ProxyMeshBuilder::buildAggregate(
    const std::vector<std::uint32_t>& items,
    double cellSize,
    std::map<FeatureId, std::vector<FeatureId>>& sources,
    bool mergeIntoSingleDrawable,
    unsigned triangleBudget) const
{
    // 按空间体素把对象聚合；每个被占用的 cell 生成一个低面代理 + 新 proxyFeatureId
    std::map<Cell, std::vector<std::uint32_t>> cells;
    std::map<Cell, std::set<FeatureId>> cellFeatures;
    for (std::uint32_t id : items)
    {
        const PrimRef& pr = _prims[id];
        const osg::Vec3d& c = pr.centroid;
        Cell key{ static_cast<long long>(std::floor(c.x() / cellSize)),
                  static_cast<long long>(std::floor(c.y() / cellSize)),
                  static_cast<long long>(std::floor(c.z() / cellSize)) };
        cells[key].push_back(id);
        cellFeatures[key].insert(pr.globalFeatureId);
    }

    osg::ref_ptr<osg::Group> root = new osg::Group;
    // proxyFeatureId 放在源 feature 空间之外，避免与真实源对象 ID 混淆（手册 8.2）
    FeatureId nextProxyId = 0xF0000000u;
    std::vector<Cell> keys;
    for (auto& kv : cells) { keys.push_back(kv.first); }
    std::sort(keys.begin(), keys.end());

    // 第一遍：每个 cell 物化成"按源 feature 分单元"的几何清单，并统计面数。
    // 配额必须在看到全部 cell 之后才能按比例分摊，否则先处理的 cell 吃掉整个
    // 预算，后面的 cell 被简化到 0 面（远景整块消失）。
    struct CellEntry {
        Cell cell;
        osg::ref_ptr<osg::Node> cluster;   // 保活：units 里的几何属于它
        std::vector<std::vector<osg::ref_ptr<osg::Geometry>>> units;  // 1 单元 = 1 源 feature
        std::vector<unsigned> unitTris;
        unsigned triangles = 0;
    };
    std::vector<CellEntry> entries;
    for (const Cell& k : keys)
    {
        CellEntry e;
        e.cell = k;
        e.cluster = _model.materialize(_prims, cells[k], true);
        if (!e.cluster.valid()) { continue; }

        auto pushUnit = [&](const std::vector<osg::ref_ptr<osg::Geometry>>& u) {
            unsigned t = 0;
            for (const osg::ref_ptr<osg::Geometry>& g : u) { t += countGeometryTriangles(g.get()); }
            if (t == 0) { return; }
            e.units.push_back(u);
            e.unitTris.push_back(t);
            e.triangles += t;
        };
        osg::Node* kids = e.cluster.get();
        if (osg::Group* grp = kids->asGroup())
        {
            for (unsigned c = 0; c < grp->getNumChildren(); ++c)
            {
                osg::Node* ch = grp->getChild(c);
                std::vector<osg::ref_ptr<osg::Geometry>> u;
                if (osg::Geode* gd = ch->asGeode())
                {
                    for (unsigned d = 0; d < gd->getNumDrawables(); ++d)
                    {
                        if (osg::Geometry* g = gd->getDrawable(d)->asGeometry()) { u.push_back(g); }
                    }
                }
                else if (osg::Geometry* g = ch->asGeometry()) { u.push_back(g); }
                pushUnit(u);
            }
        }
        else if (osg::Geometry* g = kids->asGeometry())
        {
            std::vector<osg::ref_ptr<osg::Geometry>> u;
            u.push_back(g);
            pushUnit(u);
        }
        if (e.triangles == 0 || e.units.empty()) { continue; }
        if (proxyDebug())
        {
            OSG_NOTICE << "[proxydbg] cell(" << k.x << "," << k.y << "," << k.z
                       << ") items=" << cells[k].size() << " units=" << e.units.size()
                       << " tris=" << e.triangles << std::endl;
        }
        entries.push_back(e);
    }

    unsigned allTris = 0;
    for (const CellEntry& e : entries) { allTris += e.triangles; }
    allTris = std::max<unsigned>(1u, allTris);

    // 第二遍：按 cell 面数占比分摊配额。
    // 远景聚合的核心手段是"抽稀 feature"，而不是把一堆同尺寸小 feature 交给
    // meshopt 做顶点聚类 —— 后者在稀疏点云上会整体坍缩成 0 三角形
    // （实测 864 索引的 cell 返回 0 且 result_error=1.0，整个 cell 直接消失）。
    for (CellEntry& e : entries)
    {
        const unsigned budget = std::min(_profile.budget.targetTrianglesPerTile,
                                         triangleBudget > 0 ? triangleBudget
                                                    : _profile.budget.targetTrianglesPerTile);
        const unsigned quota = std::max<unsigned>(
            1u, static_cast<unsigned>(std::ceil(
                    static_cast<double>(budget)
                  * static_cast<double>(e.triangles) / static_cast<double>(allTris))));

        const std::size_t n = e.units.size();
        std::vector<char> keep(n, 0);
        if (e.triangles <= quota)
        {
            std::fill(keep.begin(), keep.end(), static_cast<char>(1));
        }
        else
        {
            std::size_t k = static_cast<std::size_t>(
                (static_cast<double>(quota) * static_cast<double>(n))
                / static_cast<double>(e.triangles) + 0.5);
            k = std::max<std::size_t>(1, std::min<std::size_t>(k, n));
            // 按质心的粗粒度 ZXY 排序后再等间隔抽取：直接按 feature 顺序抽取会
            // 沿 ID 方向成条状聚集，被丢的 feature 离保留点更远（实测最大点到
            // 代理距离 130m > cell 124m，排序后降到 cell 的网格间距量级）
            std::vector<std::size_t> seq(n);
            std::iota(seq.begin(), seq.end(), static_cast<std::size_t>(0));
            const double sub = cellSize / 8.0;   // 每 cell 8×8×8 子格
            std::vector<unsigned> mk(n, 0);
            for (std::size_t i = 0; i < n; ++i)
            {
                osg::Vec3d acc(0.0, 0.0, 0.0);
                std::size_t cnt = 0;
                for (const osg::ref_ptr<osg::Geometry>& g : e.units[i])
                {
                    const osg::BoundingSphere bs = g->getBound();
                    if (!bs.valid()) { continue; }
                    acc += osg::Vec3d(bs.center());
                    ++cnt;
                }
                if (!cnt) { continue; }
                acc /= static_cast<double>(cnt);
                const long long qx = static_cast<long long>(std::floor(acc.x() / sub));
                const long long qy = static_cast<long long>(std::floor(acc.y() / sub));
                const long long qz = static_cast<long long>(std::floor(acc.z() / sub));
                mk[i] = (static_cast<unsigned>(qz & 7) << 16)
                      | morton2(static_cast<unsigned>(qx & 7), static_cast<unsigned>(qy & 7));
            }
            std::sort(seq.begin(), seq.end(), [&](std::size_t a, std::size_t b) {
                if (mk[a] != mk[b]) { return mk[a] < mk[b]; }
                return a < b;
            });
            for (std::size_t t = 0; t < k; ++t)
            {
                const std::size_t pos = (n == k) ? t
                    : std::min<std::size_t>(n - 1, t * n / k);
                keep[seq[pos]] = 1;
            }
        }

        std::vector<osg::Geometry*> kept;
        for (std::size_t i = 0; i < n; ++i)
        {
            if (!keep[i]) { continue; }
            for (const osg::ref_ptr<osg::Geometry>& g : e.units[i]) { kept.push_back(g.get()); }
        }

        osg::ref_ptr<osg::Geode> cellGeode = new osg::Geode;
        auto attach = [&](bool merge) {
            cellGeode->removeDrawables(0, cellGeode->getNumDrawables());
            if (merge)
            {
                std::map<std::pair<AttrSig, const osg::StateSet*>,
                         std::vector<osg::Geometry*>> byKey;
                for (osg::Geometry* g : kept) { byKey[{ signatureOf(g), g->getStateSet() }].push_back(g); }
                std::vector<std::pair<AttrSig, const osg::StateSet*>> order;
                for (auto& kv : byKey) { order.push_back(kv.first); }
                std::sort(order.begin(), order.end());
                for (const auto& key : order)
                {
                    osg::ref_ptr<osg::Geometry> m = mergeGeometries(byKey[key]);
                    if (m.valid()) { cellGeode->addDrawable(m.get()); }
                }
            }
            else
            {
                // 不合并时几何会被后续简化就地改写，必须深拷贝，不能污染源单元
                for (osg::Geometry* g : kept)
                {
                    osg::ref_ptr<osg::Geometry> c = osg::clone(g, osg::CopyOp::DEEP_COPY_ALL);
                    if (c.valid()) { cellGeode->addDrawable(c.get()); }
                }
            }
        };

        attach(mergeIntoSingleDrawable);
        if (cellGeode->getNumDrawables() == 0) { attach(false); }
        if (cellGeode->getNumDrawables() == 0) { continue; }

        // 抽稀后仍超配额（feature 本身很大）才动用简化；简化把 cell 削空时
        // 回退到未简化的保留单元，绝不允许 cell 整体消失
        if (countTriangles(cellGeode.get()) > quota)
        {
            simplifyNode(cellGeode.get(), quota, static_cast<double>(cellSize) * 0.05,
                         _profile.lockSeamBorder);
            if (countTriangles(cellGeode.get()) == 0) { attach(false); }
        }
        const unsigned trisAfter = countTriangles(cellGeode.get());
        if (proxyDebug())
        {
            OSG_NOTICE << "[proxydbg] cell(" << e.cell.x << "," << e.cell.y << "," << e.cell.z
                       << ") quota=" << quota << " units=" << n << " kept=" << kept.size()
                       << " " << e.triangles << " -> " << trisAfter << std::endl;
        }
        if (trisAfter == 0) { continue; }

        cellGeode->setName(std::string("proxy_cluster_") + std::to_string(nextProxyId));
        root->addChild(cellGeode.get());

        const FeatureId pid = nextProxyId++;
        std::vector<FeatureId> srcs(cellFeatures[e.cell].begin(), cellFeatures[e.cell].end());
        std::sort(srcs.begin(), srcs.end());
        sources[pid] = srcs;
    }
    return root;
}

ProxyResult ProxyMeshBuilder::build(const std::vector<std::uint32_t>& items,
                                    unsigned targetTriangles,
                                    bool allowAggregate) const
{
    ProxyResult res;
    if (items.empty())
    {
        res.failure = "empty item set";
        return res;
    }

    // 身份保持型代理：每个 globalFeatureId 一个 Geode
    osg::ref_ptr<osg::Node> node = _model.materialize(_prims, items, true);
    if (!node.valid())
    {
        res.failure = "materialize failed";
        return res;
    }
    const unsigned tris = countTriangles(node.get());
    res.triangles = tris;
    // 空代理必须显式失败：0 三角形的远景瓦片能通过所有下游预算闸门，
    // 但在 Cesium 里表现为整块模型消失（实测 lugouqiao 事故根因）
    if (tris == 0)
    {
        res.failure = "source set materializes to zero triangles";
        return res;
    }

    // 硬字节限是按编码后内容判的，而内节点代理的纹理随时可以降分辨率、
    // 甚至整体去掉（远景 LOD 常规做法，调用方按 proxyTextureMaxEdgePx 降采样，
    // 只有降到下限仍装不下才剥纹理并记 warning）。所以绑定约束是
    // **纯几何字节**：几何达标就接受，只是纹理留多少的差别；把纹理算成
    // 硬约束会让聚合 fallback 在纹理密集模型上永远收不敛
    // （longxiang 123 张纹理、node 20 实测 32.5 MB 估算是纯几何的 6 倍）。
    const std::uint64_t maxBytes = _profile.budget.maxTileContentBytes;

    if (tris <= targetTriangles)
    {
        const ProxyBytes bs = estimateBytes(node.get());
        if (bs.geo <= maxBytes)
        {
            res.node = node;
            res.identity = true;
            res.appliedRatio = 1.0;
            res.fitsWithFloorTextures = (bs.geo + bs.texFloor <= maxBytes);
            return res;
        }
    }

    // 身份保持型代理：一次简化到位。simplifyNode 内部已按误差递增逐级加压，
    // 旧的外层"比例阶梯"每档目标都是同一个 targetTriangles，等于把同一次
    // 简化重复 10 遍（手册 7.2 的分档在误差兜底后已无意义）
    osg::BoundingBox bb;
    for (std::uint32_t id : items) { if (id < _prims.size()) { bb.expandBy(_prims[id].aabb); } }
    const double diag = bb.valid() ? (bb._max - bb._min).length() : 1.0;
    const double targetErrorMeters = diag * 0.01;

    osg::ref_ptr<osg::Node> best = nullptr;
    unsigned bestTris = tris;
    double bestRatio = 1.0;
    {
        const auto tSimp = std::chrono::steady_clock::now();
        osg::ref_ptr<osg::Node> attempt = _model.materialize(_prims, items, true);
        if (attempt.valid())
        {
            bestRatio = simplifyNode(attempt.get(), targetTriangles, targetErrorMeters,
                                     _profile.lockSeamBorder);
            best = attempt;
            bestTris = countTriangles(attempt.get());
        }
        res.metrics.simplifySeconds +=
            std::chrono::duration<double>(std::chrono::steady_clock::now() - tSimp).count();
    }

    const ProxyBytes bestBytes = best.valid() ? estimateBytes(best.get()) : ProxyBytes{};
    const bool triOk = bestTris > 0 && bestTris <= targetTriangles;
    const bool bytesOk = bestBytes.geo <= maxBytes;
    if ((triOk && bytesOk) || !allowAggregate)
    {
        res.node = best;
        res.triangles = bestTris;
        res.appliedRatio = bestRatio;
        res.fitsWithFloorTextures = (bestBytes.geo + bestBytes.texFloor <= maxBytes);
        if (bestTris == 0)
        {
            res.failure = "proxy collapsed to zero triangles during simplification";
        }
        else if (bestTris > targetTriangles)
        {
            // 手册 8.3 第 7 点：锁边导致达不到目标时接受更多三角形，但必须如实上报
            res.failure = _profile.strictIdentity
                ? "strictIdentity: proxy exceeds triangle budget, failing"
                : std::string();
        }
        return res;
    }

    // ---- 聚合代理 fallback（手册 7.3 第 5 点）----
    if (_profile.strictIdentity)
    {
        res.failure = "strictIdentity forbids aggregate proxy; cannot meet budget";
        return res;
    }

    // 聚合代理 fallback（手册 7.3 第 5 点）。
    // cell 尺寸与是否合并几何都会影响最终面数下限（每个非空 cell 至少保留
    // 一个源 feature 的最小几何），所以按"更细 → 更粗"两种方向各试一轮。
    const double sizeFactors[] = { 1.0, 0.5, 0.25, 2.0, 4.0 };
    // 字节硬限按编码后字节判，估算器近似 200 B/tri，所以聚合目标还要再过一道
    // 字节折算上限，否则"达到三角形目标但仍然超字节"会整轮白跑。
    const unsigned byteTriCap =
        static_cast<unsigned>(_profile.budget.maxTileContentBytes / 200ull);
    const unsigned aggTarget = (byteTriCap > 0 && byteTriCap < targetTriangles)
                             ? byteTriCap : targetTriangles;
    unsigned lastAggTris = 0;
    std::string lastReason;
    const auto tAgg = std::chrono::steady_clock::now();
    for (double factor : sizeFactors)
    {
        const double cellSize = diag * 0.25 * factor;
        for (int mode = 0; mode < 2; ++mode)
        {
            const bool merge = (mode == 1);
            std::map<FeatureId, std::vector<FeatureId>> sources;
            osg::ref_ptr<osg::Node> agg = buildAggregate(items, cellSize, sources, merge, aggTarget);
            unsigned aggTris = countTriangles(agg.get());
            ++res.metrics.aggregateCandidates;
            lastAggTris = aggTris;
            if (!agg.valid() || aggTris == 0 || sources.empty())
            {
                lastReason = "aggregate produced no geometry";
                continue;
            }
            ProxyBytes bs = estimateBytes(agg.get());
            // 绑定约束是**几何字节**（纹理可以先降分辨率再剥）；但静态 200 B/tri 折算
            // 仍偏乐观（t03 根代理实测 41943 面 / geo 9.58 MB），所以按当前实测
            // geo bytes/tri 反推下一轮目标，最多收敛 3 轮。
            // 不锁边：聚合代理已经不代表任何单个源对象，cell 间缝合没有意义；
            // 锁边时"单个巨型 mesh 被切成多 cell"会让每格全是开边界，meshopt
            // 一点都压不动。
            for (int round = 0;
                 round < 3 && (aggTris > aggTarget || bs.geo > maxBytes);
                 ++round)
            {
                const double bytesPerTri = std::max(1.0,
                    static_cast<double>(bs.geo) / static_cast<double>(std::max<unsigned>(aggTris, 1u)));
                unsigned want = static_cast<unsigned>(
                    static_cast<double>(maxBytes) * 0.92 / bytesPerTri);
                want = std::min(want, aggTarget);
                if (want == 0 || want >= aggTris) { want = aggTris / 2; }
                if (want == 0) { break; }
                simplifyNode(agg.get(), want, cellSize * 0.1, false);
                aggTris = countTriangles(agg.get());
                bs = estimateBytes(agg.get());
                lastAggTris = aggTris;
                if (aggTris == 0) { break; }
            }
            if (proxyDebug())
            {
                OSG_NOTICE << "[proxydbg] aggregate factor=" << factor << " merge=" << merge
                           << " aggTris=" << aggTris << " geoBytes=" << bs.geo
                           << " texBytes=" << bs.texFull << " target=" << aggTarget << std::endl;
            }
            if (aggTris > targetTriangles)
            {
                lastReason = "triangles " + std::to_string(aggTris) + " > "
                           + std::to_string(targetTriangles);
                continue;
            }
            if (bs.geo > maxBytes)
            {
                lastReason = "geometry " + std::to_string(bs.geo) + " > "
                           + std::to_string(maxBytes) + " (textures excluded)";
                continue;
            }
            res.node = agg;
            res.triangles = aggTris;
            res.aggregate = true;
            res.aggregateSources = sources;
            res.fitsWithFloorTextures = (bs.geo + bs.texFloor <= maxBytes);
            res.appliedRatio = static_cast<double>(aggTris) / static_cast<double>(tris);
            res.metrics.aggregateSeconds +=
                std::chrono::duration<double>(std::chrono::steady_clock::now() - tAgg).count();
            return res;
        }
    }

    // 仍超预算：如实失败，绝不输出超限瓦片（手册 7.3 第 6 点）
    res.metrics.aggregateSeconds +=
        std::chrono::duration<double>(std::chrono::steady_clock::now() - tAgg).count();
    res.triangles = lastAggTris;
    res.node = nullptr;
    res.failure = "proxy still over budget after aggregate fallback: " + lastReason;
    return res;
}
