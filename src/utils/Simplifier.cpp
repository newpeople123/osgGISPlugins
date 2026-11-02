#include "utils/Simplifier.h"
#include <osg/Geometry>
#include <osg/PrimitiveSet>
#include <osg/Array>
#include <algorithm>
#include <cstdlib>
#include <cmath>

using namespace osgGISPlugins;

namespace {

const unsigned UNMAPPED = 0xFFFFFFFFu;

bool simpleDebug()
{
    static const bool on = std::getenv("WB_PROXY_DEBUG") != nullptr;
    return on;
}

bool geomProbeEnv()
{
    static const bool on = std::getenv("WB_PROXY_GEOM_DEBUG") != nullptr;
    return on;
}

// meshopt 的输入是紧排 float3；osg::Vec3 即 3×float，stride 直接取 sizeof。
void packPositions(const osg::Vec3Array& src, std::vector<float>& dst)
{
    dst.reserve(src.size() * 3);
    for (const osg::Vec3& v : src)
    {
        dst.push_back(v.x());
        dst.push_back(v.y());
        dst.push_back(v.z());
    }
}

bool readIndices(const osg::PrimitiveSet* ps, std::vector<unsigned>& out)
{
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
    if (const auto* dub = dynamic_cast<const osg::DrawElementsUByte*>(ps))
    {
        out.assign(dub->begin(), dub->end());
        return true;
    }
    // DrawArrays 展开成索引列表：顶点数组一旦被压缩，连续区间语义就失效，
    // 必须改写为索引图元才能保持引用一致
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

// 逐顶点数组按 remap 重建。返回 nullptr 表示该数组无法安全重映射 ——
// 调用方据此放弃本次简化，绝不产出属性错位的几何（手册 5.3 离散属性
// 不得错位；5.5 第 6 点：改动顶点后必须重新验证 ID 属性）。
osg::ref_ptr<osg::Array> remapArray(const osg::Array* src,
                                    const std::vector<unsigned>& remap,
                                    std::size_t newCount)
{
    if (!src) { return nullptr; }
    if (src->getBinding() == osg::Array::BIND_OVERALL) { return nullptr; }
    if (src->getNumElements() != remap.size()) { return nullptr; }

#define OSGGIS_REMAP_CASE(ARRTYPE, ENUMVAL)                                \
    if (src->getType() == osg::Array::ENUMVAL)                             \
    {                                                                      \
        const ARRTYPE* s = static_cast<const ARRTYPE*>(src);               \
        osg::ref_ptr<ARRTYPE> d = new ARRTYPE;                             \
        d->reserve(newCount);                                              \
        for (std::size_t i = 0; i < s->size(); ++i)                        \
        {                                                                  \
            if (remap[i] != UNMAPPED) { d->push_back((*s)[i]); }           \
        }                                                                  \
        if (d->size() != newCount) { return nullptr; }                     \
        return d.get();                                                    \
    }

    OSGGIS_REMAP_CASE(osg::FloatArray, FloatArrayType)
    OSGGIS_REMAP_CASE(osg::DoubleArray, DoubleArrayType)
    OSGGIS_REMAP_CASE(osg::Vec2Array, Vec2ArrayType)
    OSGGIS_REMAP_CASE(osg::Vec2dArray, Vec2dArrayType)
    OSGGIS_REMAP_CASE(osg::Vec3Array, Vec3ArrayType)
    OSGGIS_REMAP_CASE(osg::Vec3dArray, Vec3dArrayType)
    OSGGIS_REMAP_CASE(osg::Vec4Array, Vec4ArrayType)
    OSGGIS_REMAP_CASE(osg::Vec4dArray, Vec4dArrayType)
    OSGGIS_REMAP_CASE(osg::Vec4bArray, Vec4bArrayType)
    OSGGIS_REMAP_CASE(osg::Vec2bArray, Vec2bArrayType)
    OSGGIS_REMAP_CASE(osg::Vec3bArray, Vec3bArrayType)
#undef OSGGIS_REMAP_CASE
    return nullptr;
}

// BIND_OVERALL 的数组（材质常量）不参与顶点压缩
osg::Array* flatArray(osg::Array* arr)
{
    if (!arr || arr->getBinding() == osg::Array::BIND_OVERALL) { return nullptr; }
    return arr;
}

} // namespace

std::size_t Simplifier::removeExactDegenerateTriangles(const std::vector<float>& positions,
                                                       std::size_t vertexCount,
                                                       std::vector<unsigned int>& indices)
{
    if (indices.empty()) { return 0; }
    // sloppy 输出允许出现不完整尾部（文档容量与实测计数不一致），先裁掉尾数。
    const std::size_t tail = indices.size() % 3;
    if (tail) { indices.resize(indices.size() - tail); }
    if (indices.size() < 3) { indices.clear(); return 0; }

    const auto finiteVertex = [&](unsigned v) {
        if (v >= vertexCount || positions.size() < (static_cast<std::size_t>(v) + 1) * 3) { return false; }
        const float* p = &positions[static_cast<std::size_t>(v) * 3];
        return std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2]);
    };
    std::vector<unsigned int> kept;
    kept.reserve(indices.size());
    std::size_t removed = 0;
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        const unsigned a = indices[i], b = indices[i + 1], c = indices[i + 2];
        if (a == b || b == c || a == c) { ++removed; continue; }
        if (!finiteVertex(a) || !finiteVertex(b) || !finiteVertex(c)) { ++removed; continue; }
        const float* pa = &positions[static_cast<std::size_t>(a) * 3];
        const float* pb = &positions[static_cast<std::size_t>(b) * 3];
        const float* pc = &positions[static_cast<std::size_t>(c) * 3];
        const double ux = static_cast<double>(pb[0]) - pa[0];
        const double uy = static_cast<double>(pb[1]) - pa[1];
        const double uz = static_cast<double>(pb[2]) - pa[2];
        const double vx = static_cast<double>(pc[0]) - pa[0];
        const double vy = static_cast<double>(pc[1]) - pa[1];
        const double vz = static_cast<double>(pc[2]) - pa[2];
        const double cx = uy * vz - uz * vy;
        const double cy = uz * vx - ux * vz;
        const double cz = ux * vy - uy * vx;
        // 只删除“精确零面积”；面积很小但非零的细长三角形必须保留。
        if (cx == 0.0 && cy == 0.0 && cz == 0.0) { ++removed; continue; }
        kept.push_back(a); kept.push_back(b); kept.push_back(c);
    }
    indices.swap(kept);
    return removed;
}

std::vector<unsigned int> Simplifier::buildVertexRemap(
    const std::vector<std::vector<unsigned int>>& psets, std::size_t vertexCount)
{
    std::vector<unsigned> remap(vertexCount, UNMAPPED);
    for (const std::vector<unsigned>& idx : psets)
    {
        for (const unsigned v : idx)
        {
            if (v < vertexCount) { remap[v] = 0; }
        }
    }
    unsigned next = 0;
    for (std::size_t v = 0; v < vertexCount; ++v)
    {
        if (remap[v] != UNMAPPED) { remap[v] = next++; }
    }
    return remap;
}

bool Simplifier::simplifyIndices(const std::vector<unsigned int>& src,
                                 const std::vector<float>& positions,
                                 std::size_t vertexCount,
                                 std::vector<unsigned int>& out)
{
    out = src;
    if (src.size() < 3 || vertexCount == 0) { return false; }
    const std::size_t targetIndexCount =
        static_cast<std::size_t>(src.size() * getSampleRatio());
    // 目标不足一个三角形时不做简化。旧实现在这里把 vertex array 置空，
    // 远景代理被整体删空却仍算"符合预算"（AUDIT A-27 空瓦片根因）。
    if (targetIndexCount < 3 || targetIndexCount >= src.size()) { return false; }

    const size_t stride = sizeof(osg::Vec3);
    const unsigned options = _lockBorders ? meshopt_SimplifyLockBorder : 0;
    float resultError = 0.f;

    std::vector<unsigned int> dst(src.size());
    std::size_t n = meshopt_simplify(&dst[0], &src[0], src.size(), positions.data(),
                                     vertexCount, stride, targetIndexCount,
                                     _targetError, options, &resultError);
    lodError = osg::maximum(lodError, resultError);
    if (simpleDebug())
    {
        OSG_WARN << "[meshopt] src=" << src.size() << " verts=" << vertexCount
                 << " want=" << targetIndexCount << " got=" << n
                 << " err=" << resultError << " lock=" << _lockBorders
                 << " targetError=" << _targetError << std::endl;
    }

    if (_aggressive && n > targetIndexCount)
    {
        // 误差上限过宽时 meshopt_simplify 会提前停止（实测大节点 133k→133k），
        // 用 sloppy 强行逼近目标；真实误差由 GeometryErrorEvaluator 复核。
        // 文档说 destination 最坏需要 index_count 个元素，但实测 sloppy 在重复
        // 顶点/非流形输入下会返回大于 index_count 的计数（实测 704 进 1272 出），
        // 直接按文档分配会越界写堆 —— 这里扩容并按容量截断。
        const std::size_t capacity = src.size() * 2 + 16;
        std::vector<unsigned int> sloppyIdx(capacity);
        const std::size_t mRaw = meshopt_simplifySloppy(
            &sloppyIdx[0], &src[0], src.size(), positions.data(), vertexCount, stride,
            targetIndexCount, _targetErrorAggressive, &resultError);
        const std::size_t m = std::min(mRaw, capacity);
        if (simpleDebug())
        {
            OSG_WARN << "[meshopt/sloppy] want=" << targetIndexCount << " got=" << mRaw
                     << " cap=" << capacity << " err=" << resultError << std::endl;
        }
        lodError = osg::maximum(lodError, resultError);
        if (m >= 3 && m < n)
        {
            dst.assign(sloppyIdx.begin(), sloppyIdx.begin() + m);
            n = m;
        }
    }
    else
    {
        n = std::min(n, dst.size());
        dst.resize(n);
    }

    // dst 的有效长度是 n（sloppy 被拒绝时 dst 仍是初值尺寸，尾部为 0 索引，
    // 直接使用会凭空多出引用顶点 0 的退化三角形）
    if (dst.size() > n) { dst.resize(n); }
    const auto countDegen = [&](const std::vector<unsigned>& v) {
        std::size_t c = 0;
        for (std::size_t i = 0; i + 2 < v.size(); i += 3)
        {
            const unsigned a = v[i], b = v[i + 1], d = v[i + 2];
            if (a == b || b == d || a == d || a >= vertexCount || b >= vertexCount || d >= vertexCount)
            { ++c; continue; }
            const float* pa = &positions[static_cast<std::size_t>(a) * 3];
            const float* pb = &positions[static_cast<std::size_t>(b) * 3];
            const float* pc = &positions[static_cast<std::size_t>(d) * 3];
            const double ux = static_cast<double>(pb[0]) - pa[0];
            const double uy = static_cast<double>(pb[1]) - pa[1];
            const double uz = static_cast<double>(pb[2]) - pa[2];
            const double vx = static_cast<double>(pc[0]) - pa[0];
            const double vy = static_cast<double>(pc[1]) - pa[1];
            const double vz = static_cast<double>(pc[2]) - pa[2];
            if (uy * vz - uz * vy == 0.0 && uz * vx - ux * vz == 0.0
                && ux * vy - uy * vx == 0.0) { ++c; }
        }
        return c;
    };
    const std::size_t degBefore = countDegen(dst);
    const std::size_t removedDegenerate =
        removeExactDegenerateTriangles(positions, vertexCount, dst);
    _removedDegenerateTris += removedDegenerate;
    if ((simpleDebug() || geomProbeEnv()) && (degBefore || removedDegenerate))
    {
        OSG_WARN << "[simplifier] degen before=" << degBefore
                 << " removed=" << removedDegenerate
                 << " after=" << countDegen(dst)
                 << " n=" << dst.size() << std::endl;
    }
    if (dst.size() < 3)
    {
        if (geomProbeEnv())
        {
            OSG_WARN << "[simplifier] allDegenRestore srcN=" << src.size()
                     << " keptN=" << dst.size() << std::endl;
        }
        out = src; return false;  // 绝不删空有效几何
    }
    out.swap(dst);
    return true;
}

void Simplifier::simplifyMesh(osg::Geometry& geometry)
{
    const unsigned psetCount = geometry.getNumPrimitiveSets();
    if (psetCount == 0) { return; }

    osg::Vec3Array* positions = dynamic_cast<osg::Vec3Array*>(geometry.getVertexArray());
    if (!positions || positions->empty()) { return; }
    const std::size_t positionCount = positions->size();

    std::vector<float> packed;
    packPositions(*positions, packed);

    // 1) 逐 primitiveSet 简化，只收集新索引（仍指向原顶点编号）
    std::vector<std::vector<unsigned>> perPset(psetCount);
    bool anyChanged = false;
    for (unsigned i = 0; i < psetCount; ++i)
    {
        osg::PrimitiveSet* ps = geometry.getPrimitiveSet(i);
        if (!ps) { return; }
        std::vector<unsigned> src;
        if (!readIndices(ps, src))
        {
            // 未知图元类型无法参与统一顶点重映射：整条几何保持原样，
            // 绝不产出悬空索引
            OSG_WARN << "Simplifier: primitive set " << i << " (" << ps->className()
                     << ") has no readable index list; geometry kept unsimplified." << std::endl;
            return;
        }
        // 默认原样保留：非三角形图元不减面，但其顶点必须留在并集里
        perPset[i] = src;
        if (ps->getMode() != osg::PrimitiveSet::TRIANGLES) { continue; }
        std::vector<unsigned> dst;
        if (simplifyIndices(src, packed, positionCount, dst) && dst != src)
        {
            perPset[i].swap(dst);
            anyChanged = true;
        }
    }
    if (!anyChanged) { return; }

    // 2) 按所有 pset 的顶点并集做一次统一重映射 —— 多 pset 共享 positions 时
    //    必须如此，否则后一个 pset 的索引会被前一次的顶点压缩打断（A-26）。
    //    新下标必须按旧下标升序分配：remapArray 就是按旧下标升序打包顶点数组，
    //    按 pset 首次引用顺序赋号会让索引与顶点数组整体错位，输出三角面连接
    //    错误顶点（R8 实测：简化阶段凭空产生零面积面与混乱三角网）。
    const std::vector<unsigned> remap = buildVertexRemap(perPset, positionCount);
    const std::size_t newCount = static_cast<std::size_t>(
        std::count_if(remap.begin(), remap.end(),
                      [](unsigned v) { return v != UNMAPPED; }));
    if (newCount == 0) { return; }

    // 3) 重建逐顶点数组；任一数组无法安全重映射就整体放弃本次简化
    osg::ref_ptr<osg::Array> newPos = remapArray(positions, remap, newCount);
    osg::ref_ptr<osg::Array> newNrm = remapArray(flatArray(geometry.getNormalArray()),
                                                 remap, newCount);
    osg::ref_ptr<osg::Array> newCol = remapArray(flatArray(geometry.getColorArray()),
                                                 remap, newCount);
    std::vector<osg::ref_ptr<osg::Array>> newUv(geometry.getNumTexCoordArrays());
    for (unsigned u = 0; u < geometry.getNumTexCoordArrays(); ++u)
    {
        newUv[u] = remapArray(flatArray(geometry.getTexCoordArray(u)), remap, newCount);
        if (!newUv[u].valid() && flatArray(geometry.getTexCoordArray(u))) { return; }
    }
    std::vector<std::pair<unsigned, osg::ref_ptr<osg::Array>>> newAttribs;
    for (unsigned a = 0; a < geometry.getNumVertexAttribArrays(); ++a)
    {
        osg::Array* src = geometry.getVertexAttribArray(a);
        if (!flatArray(src)) { continue; }
        if (src->getNumElements() != positionCount)
        {
            OSG_WARN << "Simplifier: vertexAttribArray(" << a << ") size "
                     << src->getNumElements() << " != position count " << positionCount
                     << "; left untouched (may indicate pre-existing corruption)." << std::endl;
            continue;
        }
        osg::ref_ptr<osg::Array> dst = remapArray(src, remap, newCount);
        if (!dst.valid())
        {
            OSG_WARN << "Simplifier: unsupported per-vertex attribute type on slot " << a
                     << " (" << src->className() << "); geometry kept unsimplified." << std::endl;
            return;
        }
        newAttribs.push_back(std::make_pair(a, dst));
    }
    if (!newPos.valid()) { return; }
    if (flatArray(geometry.getNormalArray()) && !newNrm.valid()) { return; }
    if (flatArray(geometry.getColorArray()) && !newCol.valid()) { return; }

    // 4) 写回：顶点数组与索引缓冲同步替换
    geometry.setVertexArray(static_cast<osg::Vec3Array*>(newPos.get()));
    if (newNrm.valid()) { geometry.setNormalArray(newNrm.get(), osg::Array::BIND_PER_VERTEX); }
    if (newCol.valid()) { geometry.setColorArray(newCol.get(), osg::Array::BIND_PER_VERTEX); }
    for (unsigned u = 0; u < newUv.size(); ++u)
    {
        if (newUv[u].valid())
        {
            geometry.setTexCoordArray(u, newUv[u].get(), osg::Array::BIND_PER_VERTEX);
        }
    }
    for (auto& kv : newAttribs)
    {
        geometry.setVertexAttribArray(kv.first, kv.second.get(), osg::Array::BIND_PER_VERTEX);
    }

    for (unsigned i = 0; i < psetCount; ++i)
    {
        osg::PrimitiveSet* origin = geometry.getPrimitiveSet(i);
        if (!origin || perPset[i].empty()) { continue; }
        std::vector<unsigned> idx;
        idx.reserve(perPset[i].size());
        for (const unsigned v : perPset[i])
        {
            if (v < positionCount && remap[v] != UNMAPPED) { idx.push_back(remap[v]); }
        }
        if (idx.empty()) { continue; }
        // 顶点被压缩后 DrawArrays/UByte 的隐式或窄索引不再成立，统一升级为
        // 与 newCount 匹配的最紧凑索引类型，同时保留原图元模式
        const bool keepShort = newCount <= 65535u
            && dynamic_cast<const osg::DrawElementsUShort*>(origin) != nullptr;
        osg::ref_ptr<osg::DrawElements> de;
        if (keepShort) { de = new osg::DrawElementsUShort(origin->getMode()); }
        else           { de = new osg::DrawElementsUInt(origin->getMode()); }
        if (keepShort)
        {
            osg::DrawElementsUShort* ds = static_cast<osg::DrawElementsUShort*>(de.get());
            ds->reserve(idx.size());
            for (const unsigned v : idx) { ds->push_back(static_cast<unsigned short>(v)); }
        }
        else
        {
            osg::DrawElementsUInt* du = static_cast<osg::DrawElementsUInt*>(de.get());
            du->reserve(idx.size());
            for (const unsigned v : idx) { du->push_back(v); }
        }
        geometry.setPrimitiveSet(i, de.get());
    }

    // 诊断：写回后的几何若仍含精确退化三角形，说明过滤与写回之间存在缝隙
    // （曾出现逐 pset after=0 但代理仍有零面积面的现象）。只在探针开启时运行。
    if (geomProbeEnv())
    {
        const osg::Vec3Array* outPos = dynamic_cast<const osg::Vec3Array*>(geometry.getVertexArray());
        std::size_t bad = 0, total = 0, badPre = 0;
        std::size_t badRep = 0, badOob = 0, badZero = 0;
        for (unsigned i = 0; i < geometry.getNumPrimitiveSets() && outPos; ++i)
        {
            const osg::PrimitiveSet* ps = geometry.getPrimitiveSet(i);
            if (!ps || ps->getMode() != osg::PrimitiveSet::TRIANGLES) { continue; }
            std::vector<unsigned> idx;
            if (!readIndices(ps, idx)) { continue; }
            const std::size_t n = outPos->size();
            for (std::size_t k = 0; k + 2 < idx.size(); k += 3)
            {
                ++total;
                const unsigned a = idx[k], b = idx[k + 1], c = idx[k + 2];
                if (a >= n || b >= n || c >= n) { ++bad; ++badOob; continue; }
                if (a == b || b == c || a == c) { ++bad; ++badRep; continue; }
                const osg::Vec3& pa = (*outPos)[a];
                const osg::Vec3& pb = (*outPos)[b];
                const osg::Vec3& pc = (*outPos)[c];
                const double ux = static_cast<double>(pb.x()) - pa.x();
                const double uy = static_cast<double>(pb.y()) - pa.y();
                const double uz = static_cast<double>(pb.z()) - pa.z();
                const double vx = static_cast<double>(pc.x()) - pa.x();
                const double vy = static_cast<double>(pc.y()) - pa.y();
                const double vz = static_cast<double>(pc.z()) - pa.z();
                if (uy * vz - uz * vy == 0.0 && uz * vx - ux * vz == 0.0
                    && ux * vy - uy * vx == 0.0) { ++bad; ++badZero; }
            }
            // 同一 pset 在重映射**之前**（perPset 对旧 packed 坐标）就有的退化数：
            // badPre == bad 说明过滤/跳过路径把退化带进来了；否则是写回引入的
            const std::vector<unsigned>& pre = perPset[i];
            for (std::size_t k = 0; k + 2 < pre.size(); k += 3)
            {
                const unsigned a = pre[k], b = pre[k + 1], c = pre[k + 2];
                if (a >= positionCount || b >= positionCount || c >= positionCount) { ++badPre; continue; }
                const float* pa = &packed[static_cast<std::size_t>(a) * 3];
                const float* pb = &packed[static_cast<std::size_t>(b) * 3];
                const float* pc = &packed[static_cast<std::size_t>(c) * 3];
                const double ux = static_cast<double>(pb[0]) - pa[0];
                const double uy = static_cast<double>(pb[1]) - pa[1];
                const double uz = static_cast<double>(pb[2]) - pa[2];
                const double vx = static_cast<double>(pc[0]) - pa[0];
                const double vy = static_cast<double>(pc[1]) - pa[1];
                const double vz = static_cast<double>(pc[2]) - pa[2];
                if (a == b || b == c || a == c || (uy * vz - uz * vy == 0.0
                    && uz * vx - ux * vz == 0.0 && ux * vy - uy * vx == 0.0)) { ++badPre; }
            }
        }
        // remap 是按 pset 首次引用顺序赋新下标，而 remapArray 按旧下标升序打包；
        // 两者一致的前提是"首次引用顺序 == 升序"。这里统计不一致的顶点数。
        std::size_t rankMismatch = 0;
        unsigned expect = 0;
        for (std::size_t v = 0; v < remap.size(); ++v)
        {
            if (remap[v] == UNMAPPED) { continue; }
            if (remap[v] != expect) { ++rankMismatch; }
            ++expect;
        }
        if (bad || rankMismatch)
        {
            OSG_WARN << "[simplifier] writebackDegen=" << bad << " of " << total
                     << " preBad=" << badPre << " oob=" << badOob << " rep=" << badRep
                     << " zero=" << badZero << " psets=" << geometry.getNumPrimitiveSets()
                     << " rankMismatch=" << rankMismatch << "/" << expect << std::endl;
        }
    }
}
