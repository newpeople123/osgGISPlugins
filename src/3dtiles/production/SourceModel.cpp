#include "3dtiles/production/SourceModel.h"
#include "3dtiles/production/Trace.h"
#include <osg/Transform>
#include <osg/MatrixTransform>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/PrimitiveSet>
#include <osg/UserDataContainer>
#include <osg/ValueObject>
#include <osgDB/ReadFile>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include "utils/Utils.h"

using namespace osgGISPlugins;
using namespace osgGISPlugins::production;

namespace {

// 把 DrawElements / DrawArrays 展开成三角形列表
void expandPrimitiveSet(const osg::PrimitiveSet* ps, std::vector<unsigned>& out)
{
    const GLenum mode = ps->getMode();
    switch (mode)
    {
    case osg::PrimitiveSet::TRIANGLES:
    {
        for (unsigned i = 0; i + 2 < ps->getNumIndices(); i += 3)
        {
            out.push_back(ps->index(i));
            out.push_back(ps->index(i + 1));
            out.push_back(ps->index(i + 2));
        }
        break;
    }
    case osg::PrimitiveSet::TRIANGLE_STRIP:
    {
        for (unsigned i = 0; i + 2 < ps->getNumIndices(); ++i)
        {
            if ((i & 1) == 0)
            {
                out.push_back(ps->index(i));
                out.push_back(ps->index(i + 1));
                out.push_back(ps->index(i + 2));
            }
            else
            {
                out.push_back(ps->index(i + 1));
                out.push_back(ps->index(i));
                out.push_back(ps->index(i + 2));
            }
        }
        break;
    }
    case osg::PrimitiveSet::TRIANGLE_FAN:
    {
        for (unsigned i = 1; i + 1 < ps->getNumIndices(); ++i)
        {
            out.push_back(ps->index(0));
            out.push_back(ps->index(i));
            out.push_back(ps->index(i + 1));
        }
        break;
    }
    case osg::PrimitiveSet::QUADS:
    {
        for (unsigned i = 0; i + 3 < ps->getNumIndices(); i += 4)
        {
            out.push_back(ps->index(i));
            out.push_back(ps->index(i + 1));
            out.push_back(ps->index(i + 2));
            out.push_back(ps->index(i));
            out.push_back(ps->index(i + 2));
            out.push_back(ps->index(i + 3));
        }
        break;
    }
    default:
        break;  // LINES/POINTS/POLYGON 不产生三角形
    }
}

// osg::ValueObject 的类型分派用 GetValueVisitor，不能直接 get(T&)。
class PropertyValueVisitor : public osg::ValueObject::GetValueVisitor
{
public:
    void apply(bool value) override { _v = value ? "true" : "false"; _t = "bool"; }
    void apply(char value) override { _v = std::to_string(value); _t = "char"; }
    void apply(unsigned char value) override { _v = std::to_string(value); _t = "uchar"; }
    void apply(short value) override { _v = std::to_string(value); _t = "short"; }
    void apply(unsigned short value) override { _v = std::to_string(value); _t = "ushort"; }
    void apply(int value) override { _v = std::to_string(value); _t = "int"; }
    void apply(unsigned int value) override { _v = std::to_string(value); _t = "uint"; }
    void apply(float value) override { _v = std::to_string(value); _t = "float"; }
    void apply(double value) override { _v = std::to_string(value); _t = "double"; }
    void apply(const std::string& value) override { _v = value; _t = "string"; }
    void apply(const osg::Vec2f& value) override { _v = "vec2f"; _t = "vec2f"; }
    void apply(const osg::Vec3f& value) override { _v = "vec3f"; _t = "vec3f"; }
    void apply(const osg::Vec4f& value) override { _v = "vec4f"; _t = "vec4f"; }
    void apply(const osg::Vec2d& value) override { _v = "vec2d"; _t = "vec2d"; }
    void apply(const osg::Vec3d& value) override { _v = "vec3d"; _t = "vec3d"; }
    void apply(const osg::Vec4d& value) override { _v = "vec4d"; _t = "vec4d"; }
    void apply(const osg::Quat& value) override { _v = "quat"; _t = "quat"; }
    void apply(const osg::Plane& value) override { _v = "plane"; _t = "plane"; }
    void apply(const osg::Matrixf& value) override { _v = "matrixf"; _t = "matrixf"; }
    void apply(const osg::Matrixd& value) override { _v = "matrixd"; _t = "matrixd"; }
    const std::string& value() const { return _v; }
    const std::string& type() const { return _t; }
private:
    std::string _v;
    std::string _t = "unserializable";
};

std::string valueObjectToString(const osg::ValueObject* vo, std::string& typeName)
{
    if (!vo) { typeName = "unknown"; return ""; }
    PropertyValueVisitor v;
    vo->get(v);
    typeName = v.type();
    return v.value();
}

// 几何内容签名：用于重复实例识别（不依赖指针地址）
struct MeshSignature {
    std::size_t vertCount = 0;
    std::size_t triCount = 0;
    std::size_t hash = 0;
    bool operator==(const MeshSignature& o) const {
        return vertCount == o.vertCount && triCount == o.triCount && hash == o.hash;
    }
};

MeshSignature computeSignature(const osg::Geode& geode)
{
    MeshSignature sig;
    std::size_t seed = 0;
    for (unsigned i = 0; i < geode.getNumDrawables(); ++i)
    {
        const osg::Geometry* g = geode.getDrawable(i)->asGeometry();
        if (!g) { continue; }
        const osg::Vec3Array* pos = dynamic_cast<const osg::Vec3Array*>(g->getVertexArray());
        if (!pos) { continue; }
        sig.vertCount += pos->size();
        for (unsigned p = 0; p < g->getNumPrimitiveSets(); ++p)
        {
            sig.triCount += g->getPrimitiveSet(p)->getNumIndices() / 3;
        }
        const std::size_t step = pos->size() / 32 + 1;
        for (std::size_t k = 0; k < pos->size(); k += step)
        {
            const osg::Vec3& v = (*pos)[k];
            seed ^= std::hash<double>()(v.x()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<double>()(v.y()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<double>()(v.z()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
    }
    sig.hash = seed;
    return sig;
}

} // namespace

namespace osgGISPlugins { namespace production {

// ---------------------------------------------------------------------------
// 遍历器：收集 Geode / 世界矩阵 / 节点路径 / 属性
// ---------------------------------------------------------------------------
class SourceModelVisitor : public osg::NodeVisitor {
public:
    struct FoundGeode {
        osg::ref_ptr<osg::Geode> geode;
        osg::Matrixd worldMatrix;
        std::string nodePath;
        FeatureId parentFeatureId;
    };

    std::vector<FoundGeode> found;

    SourceModelVisitor()
        : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
    {
        _current.makeIdentity();
    }

    void apply(osg::Transform& t) override
    {
        osg::Matrixd local;
        t.computeLocalToWorldMatrix(local, this);
        pushMatrix(local);
        traverse(t);
        popMatrix();
    }

    void apply(osg::Geode& geode) override
    {
        if (geode.getNumDrawables() == 0) { return; }
        FoundGeode f;
        f.geode = &geode;
        f.worldMatrix = _current;
        f.nodePath = _path;
        f.parentFeatureId = _currentParent;
        found.push_back(f);
        traverse(geode);
    }

    void apply(osg::Node& node) override
    {
        const std::string saved = _path;
        if (!_path.empty()) { _path += "/"; }
        _path += node.getName().empty() ? std::string("<unnamed>") : node.getName();
        _stack.push_back(_currentParent);
        traverse(node);
        _currentParent = _stack.back();
        _stack.pop_back();
        _path = saved;
    }

private:
    void pushMatrix(const osg::Matrixd& m) { _stackM.push_back(_current); _current = m * _current; }
    void popMatrix() { if (!_stackM.empty()) { _current = _stackM.back(); _stackM.pop_back(); } }

    osg::Matrixd _current;
    std::vector<osg::Matrixd> _stackM;
    std::string _path;
    FeatureId _currentParent = INVALID_FEATURE;
    std::vector<FeatureId> _stack;
};

}} // namespace

// ---------------------------------------------------------------------------
// build
// ---------------------------------------------------------------------------
bool SourceModel::build(osg::Node* root)
{
    if (!root) { return false; }
    objects.clear();
    meshes.clear();
    instances.clear();
    prims.clear();
    featureToObject.clear();
    bounds.init();

    SourceModelVisitor v;
    root->accept(v);

    // 第一遍：按内容签名分组，识别重复实例
    std::unordered_map<std::size_t, std::vector<std::size_t>> sigBuckets;
    std::vector<MeshSignature> sigs(v.found.size());
    for (std::size_t i = 0; i < v.found.size(); ++i)
    {
        sigs[i] = computeSignature(*v.found[i].geode);
        std::size_t key = sigs[i].hash ^ (sigs[i].vertCount * 1315423911u) ^ (sigs[i].triCount * 2654435761u);
        sigBuckets[key].push_back(i);
    }

    // 记录每个 geode 归属：原型 or 实例
    std::vector<FeatureId> geodeFeature(v.found.size(), INVALID_FEATURE);
    std::vector<char> isInstance(v.found.size(), 0);
    std::vector<FeatureId> prototypeOf(v.found.size(), INVALID_FEATURE);

    std::vector<osg::ref_ptr<osg::StateSet>> uniqueStateSets;
    std::uint32_t nextInstanceId = 0;
    for (auto& kv : sigBuckets)
    {
        std::vector<std::size_t>& bucket = kv.second;
        if (bucket.size() < 2) { continue; }
        // 桶内按签名 + compareGeode 严格确认
        std::vector<std::vector<std::size_t>> groups;
        for (std::size_t idx : bucket)
        {
            bool placed = false;
            for (auto& g : groups)
            {
                if (Utils::compareGeode(v.found[g.front()].geode, v.found[idx].geode))
                {
                    g.push_back(idx);
                    placed = true;
                    break;
                }
            }
            if (!placed) { groups.push_back({idx}); }
        }
        for (auto& g : groups)
        {
            if (g.size() < 2) { continue; }
            // 矩阵是否真的不同
            bool differ = false;
            osg::Matrixd ref = v.found[g.front()].worldMatrix;
            for (std::size_t k = 1; k < g.size(); ++k)
            {
                if (!Utils::compareMatrix(ref, v.found[g[k]].worldMatrix)) { differ = true; break; }
            }
            if (!differ) { continue; }
            for (std::size_t k = 0; k < g.size(); ++k)
            {
                isInstance[g[k]] = 1;
                prototypeOf[g[k]] = static_cast<FeatureId>(g.front());   // 占位，后面换成 featureId
            }
        }
    }

    // 第二遍：创建对象 / mesh / prims
    std::vector<FeatureId> geodeToObjectIndex(v.found.size(), INVALID_FEATURE);
    for (std::size_t i = 0; i < v.found.size(); ++i)
    {
        const SourceModelVisitor::FoundGeode& f = v.found[i];
        osg::Geode* geode = f.geode.get();

        SourceObject obj;
        obj.globalFeatureId = static_cast<FeatureId>(objects.size());
        obj.parentFeatureId = f.parentFeatureId;
        obj.name = geode->getName();
        obj.nodePath = f.nodePath + "/" + (geode->getName().empty() ? std::string("<geode>") : geode->getName());
        obj.worldMatrix = f.worldMatrix;

        // 属性
        const osg::UserDataContainer* udc = geode->getUserDataContainer();
        if (udc)
        {
            for (unsigned j = 0; j < udc->getNumUserObjects(); ++j)
            {
                const osg::Object* o = udc->getUserObject(j);
                const osg::ValueObject* vo = o ? o->asValueObject() : nullptr;
                if (!vo) { continue; }
                SourceProperty p;
                p.key = vo->getName();
                p.value = valueObjectToString(vo, p.typeName);
                if (p.typeName == "unserializable")
                {
                    // 手册 5.1：不可序列化属性不得静默忽略
                    OSG_WARN << "[SourceModel] property '" << p.key << "' of node '"
                             << obj.name << "' is not serializable; recorded as error."
                             << std::endl;
                }
                obj.properties.push_back(p);
            }
        }

        // mesh（烘焙世界变换）
        for (unsigned d = 0; d < geode->getNumDrawables(); ++d)
        {
            osg::Geometry* geom = geode->getDrawable(d)->asGeometry();
            if (!geom) { continue; }
            const osg::Array* srcPos = geom->getVertexArray();
            const osg::Vec3Array* srcPos3 = dynamic_cast<const osg::Vec3Array*>(srcPos);
            if (!srcPos3 || srcPos3->empty()) { continue; }

            SourceMesh mesh;
            mesh.positions = new osg::Vec3Array;
            mesh.positions->reserve(srcPos3->size());
            bool finite = true;
            for (const osg::Vec3& p : *srcPos3)
            {
                const osg::Vec3d w = osg::Vec3d(p) * f.worldMatrix;
                if (!std::isfinite(w.x()) || !std::isfinite(w.y()) || !std::isfinite(w.z()))
                {
                    finite = false;
                    break;
                }
                mesh.positions->push_back(osg::Vec3(static_cast<float>(w.x()),
                                                    static_cast<float>(w.y()),
                                                    static_cast<float>(w.z())));
                mesh.aabb.expandBy((*mesh.positions)[mesh.positions->size() - 1]);
            }
            if (!finite)
            {
                OSG_WARN << "[SourceModel] non-finite vertex position in node '" << obj.name
                         << "'; drawable skipped and reported." << std::endl;
                continue;
            }

            // 连续属性
            const osg::Vec3Array* srcNrm = dynamic_cast<const osg::Vec3Array*>(geom->getNormalArray());
            if (srcNrm && srcNrm->size() == srcPos3->size())
            {
                mesh.normals = new osg::Vec3Array(*srcNrm);
                // 法线用旋转部分变换（忽略平移）
                for (osg::Vec3& n : *mesh.normals)
                {
                    osg::Vec3d r = osg::Matrixd::transform3x3(osg::Vec3d(n), f.worldMatrix);
                    r.normalize();
                    n.set(static_cast<float>(r.x()), static_cast<float>(r.y()), static_cast<float>(r.z()));
                }
            }
            const osg::Vec2Array* srcUV = dynamic_cast<const osg::Vec2Array*>(geom->getTexCoordArray(0));
            if (srcUV && srcUV->size() == srcPos3->size()) { mesh.uv0 = new osg::Vec2Array(*srcUV); }
            const osg::Vec4Array* srcCol = dynamic_cast<const osg::Vec4Array*>(geom->getColorArray());
            if (srcCol && srcCol->size() == srcPos3->size()) { mesh.colors = new osg::Vec4Array(*srcCol); }

            // 通用顶点属性：全量保存（手册 5.5：保留源 vertex attribute 0 原始数据）
            for (unsigned a = 0; a < geom->getNumVertexAttribArrays(); ++a)
            {
                while (mesh.attribs.size() <= a) { mesh.attribs.push_back(nullptr); }
                mesh.attribs[a] = geom->getVertexAttribArray(a);
            }

            // 材质：按 StateSet 内容去重，得到稳定的 materialId（不使用指针地址）
            osg::ref_ptr<osg::StateSet> ss = geom->getStateSet();
            if (!ss.valid()) { ss = geode->getStateSet(); }
            unsigned materialId = 0;
            {
                bool found = false;
                for (std::size_t k = 0; k < uniqueStateSets.size(); ++k)
                {
                    if (Utils::compareStateSet(uniqueStateSets[k], ss))
                    {
                        materialId = static_cast<unsigned>(k);
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    materialId = static_cast<unsigned>(uniqueStateSets.size());
                    uniqueStateSets.push_back(ss);
                }
            }
            mesh.stateSet = ss;

            for (unsigned p = 0; p < geom->getNumPrimitiveSets(); ++p)
            {
                SourcePrimSet sps;
                sps.materialId = materialId;
                expandPrimitiveSet(geom->getPrimitiveSet(p), sps.tris);
                if (sps.tris.empty()) { continue; }
                mesh.primSets.push_back(sps);
            }
            if (mesh.primSets.empty()) { continue; }

            const unsigned meshId = static_cast<unsigned>(meshes.size());
            meshes.push_back(mesh);
            obj.meshIds.push_back(meshId);
        }

        if (obj.meshIds.empty()) { continue; }

        // prims
        unsigned objTris = 0;
        // 纹理字节估算（真实 FBX 缺陷 #4，lugouqiao 335MB 复现）：纹理无论
        // KTX2 与否都会内嵌进 b3dm 的 glb bufferView（Osg2Gltf 固定内嵌），
        // 大聚合瓦片纹理字节远超几何字节——lugouqiao 实测单瓦片 51MB、
        // 最高 ~2100 B/tri，旧估算只算几何导致分区器漏切。
        // KTX2/ETC1S 约 RGBA8 的 1/8，按 /8 折算；atlas 合并收益不扣（保守切分）。
        std::map<unsigned, std::uint64_t> texBytesByMesh;
        auto textureBytesForMesh = [this, &texBytesByMesh](unsigned meshId) -> std::uint64_t {
            auto it = texBytesByMesh.find(meshId);
            if (it != texBytesByMesh.end()) { return it->second; }
            std::uint64_t b = 0;
            const SourceMesh& m = meshes[meshId];
            if (m.stateSet.valid())
            {
                const unsigned units = m.stateSet->getNumTextureAttributeLists();
                for (unsigned unit = 0; unit < units; ++unit)
                {
                    if (const osg::Texture2D* tex =
                            dynamic_cast<const osg::Texture2D*>(m.stateSet->getTextureAttribute(unit, osg::StateAttribute::TEXTURE)))
                    {
                        if (const osg::Image* img = tex->getImage())
                        {
                            b += img->getTotalSizeInBytes() / 8ull;
                        }
                    }
                }
            }
            texBytesByMesh[meshId] = b;
            return b;
        };
        for (unsigned meshId : obj.meshIds)
        {
            const SourceMesh& mesh = meshes[meshId];
            for (unsigned pi = 0; pi < mesh.primSets.size(); ++pi)
            {
                PrimRef pr;
                pr.objectId = static_cast<unsigned>(objects.size());
                pr.meshId = meshId;
                pr.primSetIndex = pi;
                pr.materialId = mesh.primSets[pi].materialId;
                pr.globalFeatureId = obj.globalFeatureId;
                const unsigned n = static_cast<unsigned>(mesh.primSets[pi].tris.size() / 3);
                pr.tris.resize(n);
                for (unsigned t = 0; t < n; ++t) { pr.tris[t] = t; }
                pr.triCount = n;

                // 指标
                pr.aabb.init();
                osg::Vec3d cen(0.0, 0.0, 0.0);
                std::vector<unsigned> verts;
                verts.reserve(n * 3);
                for (unsigned t = 0; t < n; ++t)
                {
                    unsigned idx[3];
                    triangleIndices(pr, t, idx);
                    for (int k = 0; k < 3; ++k)
                    {
                        const osg::Vec3& v = (*mesh.positions)[idx[k]];
                        pr.aabb.expandBy(v);
                        cen += osg::Vec3d(v);
                        verts.push_back(idx[k]);
                    }
                }
                std::sort(verts.begin(), verts.end());
                verts.erase(std::unique(verts.begin(), verts.end()), verts.end());
                pr.vertexCount = static_cast<unsigned>(verts.size());
                pr.centroid = (n > 0) ? cen / static_cast<double>(n * 3) : osg::Vec3d(0, 0, 0);
                // 内容字节估算。实测校准（longxiang.fbx，6 个超限瓦片）：
                // 真实 b3dm ≈ 116–177 字节/三角形。旧公式 vert*48+tri*12
                // 只有 ~36 B/tri，导致 17 MB 的瓦片估算成 4.3 MB，
                // 分区器以为没超预算就不再切分，最后 selfValidate 才爆。
                // 现按 2 顶点/三角形折算：160*tri + 32*vert ≈ 176 B/tri（偏保守）。
                // 纹理字节按解码 RGBA8 的 1/8（KTX2/ETC1S 经验压缩率）计入，
                // 同一 mesh 多 primSet 共享纹理会重复计，偏保守可接受。
                // 材质/纹理的开销在 SpatialPartitioner::computeLoad 按瓦片去重另加。
                pr.byteEstimate = static_cast<std::uint64_t>(pr.vertexCount) * 32ull
                                + static_cast<std::uint64_t>(n) * 160ull
                                + textureBytesForMesh(meshId);
                pr.splitLevel = 0;
                prims.push_back(pr);
                objTris += n;
            }
        }

        obj.totalTris = objTris;
        obj.aabb.init();
        for (unsigned meshId : obj.meshIds) { obj.aabb.expandBy(meshes[meshId].aabb); }
        bounds.expandBy(obj.aabb);

        geodeToObjectIndex[i] = obj.globalFeatureId;
        geodeFeature[i] = obj.globalFeatureId;
        if (isInstance[i])
        {
            obj.instanced = true;
        }
        objects.push_back(obj);
        featureToObject[obj.globalFeatureId] = static_cast<unsigned>(objects.size() - 1);
    }

    // 第三遍：把重复实例从普通几何里摘出来，登记为 SourceInstance
    for (std::size_t i = 0; i < v.found.size(); ++i)
    {
        if (!isInstance[i]) { continue; }
        const FeatureId fid = geodeFeature[i];
        if (fid == INVALID_FEATURE) { continue; }
        const std::size_t protoSlot = prototypeOf[i];
        const FeatureId protoFid = geodeFeature[protoSlot];
        if (protoFid == INVALID_FEATURE) { continue; }
        if (fid == protoFid)
        {
            // 原型本体：保留几何，标记 instanced
            continue;
        }
        SourceInstance inst;
        inst.instanceId = nextInstanceId++;
        inst.globalFeatureId = fid;
        inst.prototypeFeatureId = protoFid;
        inst.matrix = v.found[i].worldMatrix;
        inst.name = objects[fid].name;
        inst.properties = objects[fid].properties;
        instances.push_back(inst);
    }

    // 实例本体的几何不参与 b3dm 空间划分（交给 i3dm 路径）
    if (!instances.empty())
    {
        std::vector<PrimRef> kept;
        kept.reserve(prims.size());
        std::vector<char> isPrototype(objects.size(), 0);
        for (const SourceInstance& inst : instances) { isPrototype[inst.prototypeFeatureId] = 1; }
        std::vector<char> isInstanceObj(objects.size(), 0);
        for (const SourceInstance& inst : instances) { isInstanceObj[inst.globalFeatureId] = 1; }
        for (const PrimRef& pr : prims)
        {
            if (isInstanceObj[pr.globalFeatureId]) { continue; }
            kept.push_back(pr);
        }
        prims.swap(kept);

        // 重新计算 bounds（剔除实例几何后）
        bounds.init();
        for (const SourceObject& o : objects)
        {
            if (isInstanceObj[o.globalFeatureId]) { continue; }
            bounds.expandBy(o.aabb);
        }
    }

    return !prims.empty() || !instances.empty();
}

unsigned SourceModel::totalTriangles() const
{
    unsigned n = 0;
    for (const PrimRef& p : prims) { n += p.triCount; }
    return n;
}

unsigned SourceModel::totalVertices() const
{
    unsigned n = 0;
    for (const SourceMesh& m : meshes)
    {
        if (m.positions.valid()) { n += static_cast<unsigned>(m.positions->size()); }
    }
    return n;
}

unsigned SourceModel::materialCount() const
{
    std::vector<unsigned> ids;
    for (const PrimRef& p : prims) { ids.push_back(p.materialId); }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return static_cast<unsigned>(ids.size());
}

void SourceModel::triangleIndices(const PrimRef& pr, std::uint32_t t, unsigned out[3]) const
{
    const SourceMesh& mesh = meshes[pr.meshId];
    const SourcePrimSet& ps = mesh.primSets[pr.primSetIndex];
    const std::uint32_t local = (t < pr.tris.size()) ? pr.tris[t] : 0u;
    const unsigned base = local * 3u;
    out[0] = ps.tris[base];
    out[1] = ps.tris[base + 1];
    out[2] = ps.tris[base + 2];
}

void SourceModel::collectTriangleIndices(const PrimRef& pr, std::vector<unsigned>& out) const
{
    const SourceMesh& mesh = meshes[pr.meshId];
    const SourcePrimSet& ps = mesh.primSets[pr.primSetIndex];
    out.reserve(out.size() + pr.tris.size() * 3);
    for (std::uint32_t t : pr.tris)
    {
        const unsigned base = t * 3u;
        out.push_back(ps.tris[base]);
        out.push_back(ps.tris[base + 1]);
        out.push_back(ps.tris[base + 2]);
    }
}

void SourceModel::collectTriangles(const std::vector<PrimRef>& primTable,
                                   const std::vector<std::uint32_t>& itemIds,
                                   std::vector<Triangle>& out) const
{
    for (std::uint32_t id : itemIds)
    {
        if (id >= primTable.size()) { continue; }
        const PrimRef& pr = primTable[id];
        const SourceMesh& mesh = meshes[pr.meshId];
        const SourcePrimSet& ps = mesh.primSets[pr.primSetIndex];
        for (std::uint32_t t : pr.tris)
        {
            const unsigned base = t * 3u;
            Triangle tri;
            tri.a = osg::Vec3d((*mesh.positions)[ps.tris[base]]);
            tri.b = osg::Vec3d((*mesh.positions)[ps.tris[base + 1]]);
            tri.c = osg::Vec3d((*mesh.positions)[ps.tris[base + 2]]);
            out.push_back(tri);
        }
    }
}

std::uint64_t SourceModel::estimateBytes(const std::vector<PrimRef>& primTable,
                                         const std::vector<std::uint32_t>& itemIds) const
{
    std::uint64_t b = 0;
    std::vector<unsigned> mats;
    for (std::uint32_t id : itemIds)
    {
        if (id >= primTable.size()) { continue; }
        const PrimRef& pr = primTable[id];
        b += pr.byteEstimate;
        mats.push_back(pr.materialId);
    }
    std::sort(mats.begin(), mats.end());
    mats.erase(std::unique(mats.begin(), mats.end()), mats.end());
    b += static_cast<std::uint64_t>(mats.size()) * 4096ull;   // 材质/纹理粗估
    return b;
}

osg::ref_ptr<osg::Node> SourceModel::materialize(const std::vector<PrimRef>& primTable,
                                                 const std::vector<std::uint32_t>& itemIds,
                                                 bool perFeatureGeodes) const
{
    osg::ref_ptr<osg::Group> root = new osg::Group;
    if (itemIds.empty()) { return root; }

    // 按 (featureId, materialId) 分组，组内共享一份顶点缓冲
    struct Group {
        FeatureId fid;
        unsigned materialId;
        std::vector<std::uint32_t> items;
    };
    std::vector<Group> groups;
    std::map<std::pair<FeatureId, unsigned>, std::size_t> index;
    for (std::uint32_t id : itemIds)
    {
        if (id >= primTable.size()) { continue; }
        const PrimRef& pr = primTable[id];
        if (pr.tris.empty()) { continue; }
        auto key = std::make_pair(pr.globalFeatureId, pr.materialId);
        auto it = index.find(key);
        if (it == index.end())
        {
            index[key] = groups.size();
            groups.push_back({pr.globalFeatureId, pr.materialId, {id}});
        }
        else
        {
            groups[it->second].items.push_back(id);
        }
    }
    // 确定性排序
    std::sort(groups.begin(), groups.end(), [](const Group& a, const Group& b) {
        if (a.fid != b.fid) { return a.fid < b.fid; }
        return a.materialId < b.materialId;
    });

    // 物化：每个 feature 一个 Geode
    TRACE("materialize groups=" << groups.size());
    std::map<FeatureId, osg::ref_ptr<osg::Geode>> geodes;
    for (const Group& g : groups)
    {
        TRACE("  group fid=" << g.fid << " mat=" << g.materialId << " items=" << g.items.size());
        // 收集本组三角形与用到的顶点
        std::vector<unsigned> triIdx;
        for (std::uint32_t id : g.items)
        {
            collectTriangleIndices(primTable[id], triIdx);
        }
        if (triIdx.empty()) { continue; }

        // 顶点重映射（按 meshId 分组，一个 Group 可能跨多个 mesh）
        std::map<std::pair<unsigned, unsigned>, unsigned> remap;
        osg::ref_ptr<osg::Vec3Array> pos = new osg::Vec3Array;
        osg::ref_ptr<osg::Vec3Array> nrm = nullptr;
        osg::ref_ptr<osg::Vec2Array> uv = nullptr;
        osg::ref_ptr<osg::Vec4Array> col = nullptr;
        std::vector<osg::ref_ptr<osg::Array>> outAttribs;

        bool hasNormal = false, hasUV = false, hasColor = false;
        unsigned firstMesh = primTable[g.items.front()].meshId;
        const SourceMesh& fm = meshes[firstMesh];
        hasNormal = fm.normals.valid();
        hasUV = fm.uv0.valid();
        hasColor = fm.colors.valid();
        if (hasNormal) { nrm = new osg::Vec3Array; }
        if (hasUV) { uv = new osg::Vec2Array; }
        if (hasColor) { col = new osg::Vec4Array; }
        outAttribs.assign(fm.attribs.size(), nullptr);
        for (std::size_t a = 0; a < fm.attribs.size(); ++a)
        {
            const osg::Array* src = fm.attribs[a].get();
            if (!src) { continue; }
            if (dynamic_cast<const osg::FloatArray*>(src)) { outAttribs[a] = new osg::FloatArray; }
            else if (dynamic_cast<const osg::Vec2Array*>(src)) { outAttribs[a] = new osg::Vec2Array; }
            else if (dynamic_cast<const osg::Vec3Array*>(src)) { outAttribs[a] = new osg::Vec3Array; }
            else if (dynamic_cast<const osg::Vec4Array*>(src)) { outAttribs[a] = new osg::Vec4Array; }
        }

        std::vector<unsigned> outTris;
        outTris.reserve(triIdx.size());

        // 需要按 mesh 回溯顶点来源，所以逐 PrimRef 处理
        for (std::uint32_t id : g.items)
        {
            const PrimRef& pr = primTable[id];
            const SourceMesh& mesh = meshes[pr.meshId];
            std::vector<unsigned> localTri;
            collectTriangleIndices(pr, localTri);
            for (unsigned v : localTri)
            {
                if (!mesh.positions.valid())
                {
                    TRACE("  !! mesh " << pr.meshId << " has no positions");
                    continue;
                }
                if (v >= mesh.positions->size())
                {
                    TRACE("  !! vertex index " << v << " >= positions "
                          << mesh.positions->size() << " (mesh " << pr.meshId << ")");
                    continue;
                }
                auto key = std::make_pair(pr.meshId, v);
                auto it = remap.find(key);
                if (it == remap.end())
                {
                    const unsigned nv = static_cast<unsigned>(pos->size());
                    pos->push_back((*mesh.positions)[v]);
                    if (nrm.valid() && mesh.normals.valid() && v < mesh.normals->size())
                    {
                        nrm->push_back((*mesh.normals)[v]);
                    }
                    if (uv.valid() && mesh.uv0.valid() && v < mesh.uv0->size())
                    {
                        uv->push_back((*mesh.uv0)[v]);
                    }
                    if (col.valid() && mesh.colors.valid() && v < mesh.colors->size())
                    {
                        col->push_back((*mesh.colors)[v]);
                    }
                    for (std::size_t a = 0; a < outAttribs.size(); ++a)
                    {
                        osg::Array* dst = outAttribs[a].get();
                        const osg::Array* src = (a < mesh.attribs.size()) ? mesh.attribs[a].get() : nullptr;
                        if (!dst || !src || v >= src->getNumElements()) { continue; }
                        if (osg::FloatArray* df = dynamic_cast<osg::FloatArray*>(dst))
                        {
                            const osg::FloatArray* sf = dynamic_cast<const osg::FloatArray*>(src);
                            if (sf) { df->push_back((*sf)[v]); }
                        }
                        else if (osg::Vec2Array* dv = dynamic_cast<osg::Vec2Array*>(dst))
                        {
                            const osg::Vec2Array* sv = dynamic_cast<const osg::Vec2Array*>(src);
                            if (sv) { dv->push_back((*sv)[v]); }
                        }
                        else if (osg::Vec3Array* dv = dynamic_cast<osg::Vec3Array*>(dst))
                        {
                            const osg::Vec3Array* sv = dynamic_cast<const osg::Vec3Array*>(src);
                            if (sv) { dv->push_back((*sv)[v]); }
                        }
                        else if (osg::Vec4Array* dv = dynamic_cast<osg::Vec4Array*>(dst))
                        {
                            const osg::Vec4Array* sv = dynamic_cast<const osg::Vec4Array*>(src);
                            if (sv) { dv->push_back((*sv)[v]); }
                        }
                    }
                    remap[key] = nv;
                    outTris.push_back(nv);
                }
                else
                {
                    outTris.push_back(it->second);
                }
            }
        }
        if (outTris.empty()) { continue; }

        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
        geom->setVertexArray(pos.get());
        if (nrm.valid() && nrm->size() == pos->size()) { geom->setNormalArray(nrm.get(), osg::Array::BIND_PER_VERTEX); }
        if (uv.valid() && uv->size() == pos->size()) { geom->setTexCoordArray(0, uv.get()); }
        if (col.valid() && col->size() == pos->size()) { geom->setColorArray(col.get(), osg::Array::BIND_PER_VERTEX); }
        for (std::size_t a = 0; a < outAttribs.size(); ++a)
        {
            osg::Array* dst = outAttribs[a].get();
            if (dst && dst->getNumElements() == pos->size())
            {
                geom->setVertexAttribArray(static_cast<unsigned>(a), dst, osg::Array::BIND_PER_VERTEX);
            }
        }
        const bool useInt = pos->size() > 65535;
        if (useInt)
        {
            osg::ref_ptr<osg::DrawElementsUInt> de = new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES);
            de->reserve(outTris.size());
            for (unsigned v : outTris) { de->push_back(v); }
            geom->addPrimitiveSet(de.get());
        }
        else
        {
            osg::ref_ptr<osg::DrawElementsUShort> de = new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLES);
            de->reserve(outTris.size());
            for (unsigned v : outTris) { de->push_back(static_cast<unsigned short>(v)); }
            geom->addPrimitiveSet(de.get());
        }

        // 材质/纹理引用必须带回，否则 atlas / KTX2 全部丢失
        {
            const unsigned refMesh = primTable[g.items.front()].meshId;
            if (meshes[refMesh].stateSet.valid())
            {
                geom->setStateSet(meshes[refMesh].stateSet.get());
            }
        }

        FeatureId targetFid = perFeatureGeodes ? g.fid : groups.front().fid;
        auto it = geodes.find(targetFid);
        if (it == geodes.end())
        {
            osg::ref_ptr<osg::Geode> geode = new osg::Geode;
            const SourceObject* so = objectOf(targetFid);
            geode->setName(so ? so->name : std::string("feature_") + std::to_string(targetFid));
            if (so)
            {
                osg::ref_ptr<osg::UserDataContainer> udc = new osg::DefaultUserDataContainer;
                for (const SourceProperty& p : so->properties)
                {
                    udc->addUserObject(new osg::StringValueObject(p.key, p.value));
                }
                geode->setUserDataContainer(udc.get());
            }
            geodes[targetFid] = geode;
            it = geodes.find(targetFid);
        }
        it->second->addDrawable(geom.get());
    }

    // 确定性顺序加入 root
    std::vector<FeatureId> keys;
    for (auto& kv : geodes) { keys.push_back(kv.first); }
    std::sort(keys.begin(), keys.end());
    for (FeatureId k : keys) { root->addChild(geodes[k].get()); }
    return root;
}
