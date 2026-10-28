// modelstats - read-only forensic probe for real models (FBX/OBJ) driving the
// 3D Tiles production pipeline.
//
// Why this exists: model23dtiles has no dry-run mode and tests/osgdb_fbx/main.cpp is a
// hard-coded scratch program, so nothing in the repo could report what a real FBX actually
// loads as. The task book requires per-model node/mesh/vertex/triangle counts plus a
// stage-by-stage coordinate-integrity check (§3.2, §5.1) before any optimization claim.
//
// Read options, code-page handling and the chdir-to-input-dir behaviour mirror
// tools/model23dtiles/main.cpp so the numbers describe what the pipeline really sees.
// Nothing is written back into the input directory.
//
// Stages covered here:
//   S1   after the reader returns the OSG graph, local coordinates
//   S1w  the same graph in world coordinates (matrix stack applied by this visitor)
//   S3   after SourceModel::build() bakes world matrices into the internal geometry IR
//   S4   float-space hazards - osg::Vec3 is float, so this is where a huge double becomes
//        inf and where (1e24)^2 overflows an area computation
// S2 (up-axis/projection) and S5/S6 (proxy+error, output) are NOT covered: S2 is
// analytically identity for FBX with the CLI defaults, S5/S6 need the real pipeline.
#include <osg/ArgumentParser>
#include <osg/Array>
#include <osg/BoundingBox>
#include <osg/ComputeBoundsVisitor>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/Image>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/NodeVisitor>
#include <osg/PrimitiveSet>
#include <osg/StateSet>
#include <osg/Texture>
#include <osg/Notify>
#include <osgDB/ConvertUTF>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include <osgDB/Options>
#include <osgDB/ReadFile>
#include <osgDB/Registry>
#include <osgUtil/Optimizer>

#include <algorithm>
#include <chrono>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "utils/Utils.h"
#include "3dtiles/production/SourceModel.h"

namespace {

template <typename T>
T parseArgument(osg::ArgumentParser& arguments, const std::string& option, T&& defaultValue)
{
    T value = std::forward<T>(defaultValue);
    arguments.read(option, value);
    return value;
}

std::string jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (const char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else { out += c; }
        }
    }
    return out;
}

struct Magnitude
{
    unsigned long long count = 0;
    double maxAbs = 0.0;
    double min = 0.0;
    double max = 0.0;
    bool hasRange = false;

    void add(double v)
    {
        ++count;
        const double a = std::fabs(v);
        if (a > maxAbs) { maxAbs = a; }
        if (!hasRange) { min = max = v; hasRange = true; }
        else { if (v < min) { min = v; } if (v > max) { max = v; } }
    }
};

// What a coordinate-magnitude audit needs is count + extreme + how many values stop being
// representable downstream - not just a boolean "is valid".
struct Integrity
{
    unsigned long long vertices = 0;
    unsigned long long nonFiniteVertices = 0;      // NaN/Inf already present in the array
    unsigned long long floatOverflowVertices = 0;  // finite double that would become inf as float
    unsigned long long triangles = 0;
    unsigned long long repeatedIndexTris = 0;      // a==b || b==c || a==c
    unsigned long long zeroAreaTris = 0;           // area == 0 computed in double
    unsigned long long areaNotFiniteTris = 0;      // float-space area became inf/NaN
    unsigned long long indexOutOfRangeTris = 0;    // index >= vertexArray size (A-56 class)
    unsigned long long maxOffendingIndex = 0;
    unsigned long long nonFiniteSkippedTris = 0;
    unsigned long long normals = 0;
    unsigned long long zeroLengthNormals = 0;
    unsigned long long nonFiniteNormals = 0;
    unsigned long long normalsOffUnit = 0;         // |len-1| > 1e-3
    Magnitude mag[3];                              // x / y / z
    std::vector<std::string> examples;

    void note(const char* what, double a, double b, double c)
    {
        if (examples.size() >= 8) { return; }
        std::ostringstream os;
        os << what << " (" << a << ", " << b << ", " << c << ')';
        examples.push_back(os.str());
    }
};

// A linear map cannot turn a degenerate triangle into a non-degenerate one, so
// "local zero-area > world zero-area" is only possible if the exact test
// `cr.length() == 0.0` is what breaks - not the geometry. This block measures both arms for
// the *same* triangle in the *same* walk, so the two counts are attributable per triangle
// instead of coming from two independent passes over two independent accumulators.
struct JointDegenerate
{
    unsigned long long tris = 0;                  // in-range, finite in both arms
    unsigned long long skippedEitherNonFinite = 0;
    unsigned long long areaNotFiniteEither = 0;
    unsigned long long zeroAreaLocalExact = 0;
    unsigned long long zeroAreaWorldExact = 0;
    unsigned long long zeroAreaBothExact = 0;
    unsigned long long zeroAreaLocalOnlyExact = 0;   // exactly degenerate before the map only
    unsigned long long zeroAreaWorldOnlyExact = 0;   // must stay 0 or the ruler is wrong
    unsigned long long relDegenerateLocal = 0;       // |cr| <= 1e-12 * |ab|*|ac|
    unsigned long long relDegenerateWorld = 0;
    unsigned long long relDegenerateLocalOnly = 0;
    unsigned long long relDegenerateWorldOnly = 0;
    unsigned long long underNonIdentityMatrix = 0;
    double maxWorldAreaOnLocalExactZero = 0.0;       // largest surviving area after the map
    double maxWorldRelativeOnLocalExactZero = 0.0;   // that area / (|ab|*|ac|) in world space
    double minAbsDet = 0.0;                          // smallest |det| over counted triangles
    double maxAbsDet = 0.0;
    bool hasDet = false;
    std::vector<std::string> examples;

    void note(const std::string& s)
    {
        if (examples.size() >= 6) { return; }
        examples.push_back(s);
    }
};

bool floatSafe(double v) { return std::isfinite(v) && std::fabs(v) <= static_cast<double>(FLT_MAX); }

double matrixDet3(const osg::Matrixd& m)
{
    return m(0, 0) * (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1))
         - m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0))
         + m(0, 2) * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));
}

bool matrixIsIdentity(const osg::Matrixd& m)
{
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            const double want = (r == c) ? 1.0 : 0.0;
            if (std::fabs(m(r, c) - want) > 1e-9) { return false; }
        }
    }
    return true;
}

// Per-Transform evidence for the unit/scale question (§5.2 "疑似异常" vs "合法的大尺度坐标"):
// which node in the chain carries a non-unit scale, how much geometry sits under it, and what
// the local vs world coordinate range is on each axis. Without this, a huge world extent cannot
// be attributed either to the author's coordinates or to a scale node in the file.
struct TransformAudit
{
    const osg::Transform* node = nullptr;   // null for the no-transform root bucket
    unsigned long long visits = 0;          // >1 when the node is shared by several paths
    unsigned long long geometries = 0, vertices = 0, triangles = 0;
    double localScale[3] = { 1.0, 1.0, 1.0 };   // column norms of this node's own matrix
    double localDet = 1.0;
    double accumScale[3] = { 1.0, 1.0, 1.0 };   // column norms of the accumulated world matrix
    double accumDetMin = 1.0, accumDetMax = 1.0;
    double accumTransLen = 0.0;
    bool accumIdentity = true;
    double localAxisMax[3] = { 0.0, 0.0, 0.0 };  // max |coordinate| before the chain
    double worldAxisMin[3] = { 0.0, 0.0, 0.0 };
    double worldAxisMax[3] = { 0.0, 0.0, 0.0 };
    bool hasWorld = false;
};

void matrixScales(const osg::Matrixd& m, double out[3])
{
    // OSG multiplies row vectors by the matrix, so the image of basis vector j is column j.
    for (int j = 0; j < 3; ++j) {
        const double c0 = m(0, j), c1 = m(1, j), c2 = m(2, j);
        out[j] = std::sqrt(c0 * c0 + c1 * c1 + c2 * c2);
    }
}

// ---------------------------------------------------------------------------
// Graph walker: the raw OSG scene exactly as the reader produced it.
// ---------------------------------------------------------------------------
class GraphWalker : public osg::NodeVisitor
{
public:
    // Matrix accumulation order is measurable, not arguable. OSG multiplies row vectors from the
    // node outward, so the world matrix of a geometry is local * parent * grandparent...; the
    // opposite order (parent * local) silently produces a plausible but different scene. Measured
    // against osg::ComputeBoundsVisitor and SourceModel::build on the real FBX set, child-first
    // reproduces both exactly (m4 center and half-diagonal identical) while parent-first is off by
    // tens of kilometres - so child-first is the only order this probe may report as "world".
    GraphWalker(bool worldSpace, std::string resourceDir, bool childFirst = true)
        : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
        , _world(worldSpace)
        , _childFirst(childFirst)
        , _resourceDir(std::move(resourceDir))
    {
        _stack.push_back(osg::Matrixd::identity());
        // Index 0 is the "no transform above it" bucket, so geometry reached without any
        // Transform ancestor stays attributable instead of silently missing the audit.
        stats.audits.push_back(TransformAudit{});
        _auditIdx.push_back(0);
    }

    void apply(osg::Node& node) override
    {
        ++stats.nodes;
        if (node.asGeode()) { ++stats.geodes; }
        else if (node.asGroup()) { ++stats.groups; }
        traverse(node);
    }

    void apply(osg::Group& group) override
    {
        ++stats.nodes;
        ++stats.groups;
        traverse(group);
    }

    void apply(osg::Geode& geode) override
    {
        ++stats.nodes;
        ++stats.geodes;
        collectState(geode.getStateSet());
        // A Geode's "children" are drawables, not nodes: iterate them explicitly instead of
        // relying on traverse() so this does not depend on visitor dispatch details.
        for (unsigned i = 0; i < geode.getNumDrawables(); ++i) {
            visitDrawable(geode.getDrawable(i));
        }
    }

    void apply(osg::Transform& transform) override
    {
        ++stats.nodes;
        ++stats.transforms;
        osg::Matrixd local;
        const bool ok = transform.computeLocalToWorldMatrix(local, this);
        const osg::Matrixd parent = _stack.back();
        const osg::Matrixd accumulated = !ok ? parent
                                   : (_childFirst ? local * parent : parent * local);
        _stack.push_back(accumulated);
        _auditIdx.push_back(recordTransform(&transform, local, accumulated));
        if (!matrixIsIdentity(accumulated)) {
            ++stats.nonIdentityWorldMatrices;
            stats.worldEqualsLocal = false;
        }
        traverse(transform);
        _stack.pop_back();
        _auditIdx.pop_back();
    }

    struct Stats
    {
        unsigned long long nodes = 0, groups = 0, geodes = 0, transforms = 0;
        unsigned long long drawables = 0, geometries = 0, nonGeometryDrawables = 0;
        unsigned long long geometriesWithoutVec3Position = 0;
        unsigned long long primitiveSets = 0, nonIndexedPrimitiveSets = 0;
        unsigned long long primitiveSetsWithOutOfRangeIndex = 0;
        unsigned long long maxOffendingIndex = 0;
        unsigned long long vertexArrayValues = 0;
        unsigned long long uvArrays = 0, uvNonFinite = 0;
        unsigned long long stateSets = 0, blendStateSets = 0, stateSetsUnique = 0;
        // Matrix-stack evidence: how many Transform nodes actually move or turn geometry.
        // worldEqualsLocal staying true is what tells a matching local/world magnitude
        // column apart from a dead stack.
        unsigned long long nonIdentityWorldMatrices = 0;
        bool worldEqualsLocal = true;
        unsigned long long textures = 0, images = 0, imagesUnnamed = 0;
        unsigned long long texturesUnique = 0, imagesUnique = 0;
        unsigned long long imageBytes = 0;
        unsigned long long imagesResident = 0, imagesDeclaredOnly = 0;
        unsigned long long imageBytesResident = 0;
        std::set<std::string> imageFiles;
        std::vector<std::string> missingImages;
        std::map<std::string, unsigned long long> primModeCounts;
        Integrity pos;        // local for the local walker, world for the world walker
        Integrity normal;
        JointDegenerate joint;   // only the world walker fills this (one walk, both arms)
        std::vector<TransformAudit> audits;
        std::map<std::string, unsigned long long> localScaleHistogram;
        // Attribution for the worldArmVsComputeBounds check. osg::ComputeBoundsVisitor does not
        // read POSITION: it takes each Geometry's own getBoundingBox(), which OSG computes over
        // every array in Geometry::getArrayList() (normals, colours, texcoords, vertex attribs),
        // and maps its eight corners. This union reproduces that input from the geometries the
        // walker actually reached, so "CBV is bigger than the position box" can be split into
        // "non-position arrays" (union ≈ CBV) versus "the walker missed geometry" (union << CBV).
        double boundUnionMin[3] = { 0.0, 0.0, 0.0 };
        double boundUnionMax[3] = { 0.0, 0.0, 0.0 };
        bool boundUnionValid = false;
        unsigned long long boundUnionGeoms = 0, boundUnionInvalid = 0;
    } stats;

private:
    size_t recordTransform(const osg::Transform* node, const osg::Matrixd& localM,
                           const osg::Matrixd& accumM)
    {
        const auto found = _auditByNode.find(node);
        if (found != _auditByNode.end()) {
            TransformAudit& rec = stats.audits[found->second];
            ++rec.visits;
            const double d = std::fabs(matrixDet3(accumM));
            rec.accumDetMin = std::min(rec.accumDetMin, d);
            rec.accumDetMax = std::max(rec.accumDetMax, d);
            return found->second;
        }
        TransformAudit rec;
        rec.node = node;
        rec.visits = 1;
        matrixScales(localM, rec.localScale);
        rec.localDet = std::fabs(matrixDet3(localM));
        matrixScales(accumM, rec.accumScale);
        rec.accumDetMin = rec.accumDetMax = std::fabs(matrixDet3(accumM));
        rec.accumTransLen = std::sqrt(accumM(3, 0) * accumM(3, 0) + accumM(3, 1) * accumM(3, 1)
                                     + accumM(3, 2) * accumM(3, 2));
        rec.accumIdentity = matrixIsIdentity(accumM);
        const size_t idx = stats.audits.size();
        stats.audits.push_back(rec);
        _auditByNode.emplace(node, idx);
        char key[96];
        std::snprintf(key, sizeof(key), "%.6g,%.6g,%.6g",
                      rec.localScale[0], rec.localScale[1], rec.localScale[2]);
        ++stats.localScaleHistogram[key];
        return idx;
    }

    TransformAudit& curAudit() { return stats.audits[_auditIdx.back()]; }

    void visitDrawable(osg::Drawable* d)
    {
        if (!d) { return; }
        ++stats.drawables;
        osg::Geometry* geom = d->asGeometry();
        if (!geom) { ++stats.nonGeometryDrawables; return; }
        ++stats.geometries;
        if (_world) { ++curAudit().geometries; }
        collectState(geom->getStateSet());
        analyzeGeometry(*geom);
    }

    void collectState(osg::StateSet* ss)
    {
        if (!ss) { return; }
        ++stats.stateSets;
        const bool firstStateSet = _seenStateSets.insert(ss).second;
        if (firstStateSet) { ++stats.stateSetsUnique; }
        for (unsigned unit = 0; unit < ss->getNumTextureAttributeLists(); ++unit) {
            osg::Texture* tex = dynamic_cast<osg::Texture*>(
                ss->getTextureAttribute(unit, osg::StateAttribute::TEXTURE));
            if (!tex) { continue; }
            ++stats.textures;
            if (_seenTextures.insert(tex).second) { ++stats.texturesUnique; }
            for (unsigned i = 0; i < tex->getNumImages(); ++i) {
                osg::Image* img = tex->getImage(i);
                if (!img) { continue; }
                ++stats.images;
                // Attachment counters multiply because the reader shares one Image across many
                // textures (longxiang: 216 attachments over 63 distinct files). Bytes, residency
                // and file resolution are only meaningful over unique pointers.
                if (!_seenImages.insert(img).second) { continue; }
                ++stats.imagesUnique;
                stats.imageBytes += img->getTotalDataSize();
                // getTotalDataSize() is the declared s*t*r*sizes figure: it also counts an
                // Image whose pixels were never read. Only data() != nullptr is resident.
                if (img->data() != nullptr) {
                    ++stats.imagesResident;
                    stats.imageBytesResident += img->getTotalDataSize();
                } else {
                    ++stats.imagesDeclaredOnly;
                }
                const std::string fn = img->getFileName();
                if (fn.empty()) { ++stats.imagesUnnamed; continue; }
                if (!stats.imageFiles.insert(fn).second) { continue; }
                if (!osgDB::fileExists(fn) && !osgDB::fileExists(osgDB::concatPaths(_resourceDir, fn))) {
                    stats.missingImages.push_back(fn);
                }
            }
        }
        if (ss->getMode(GL_BLEND) & osg::StateAttribute::ON) { ++stats.blendStateSets; }
    }

    void analyzeGeometry(osg::Geometry& geom)
    {
        osg::Vec3Array* pos = dynamic_cast<osg::Vec3Array*>(geom.getVertexArray());
        const unsigned nv = pos ? static_cast<unsigned>(pos->size()) : 0u;
        if (_world) { unionOfGeometryBound(geom); }
        if (pos) {
            stats.vertexArrayValues += nv;
            scanVec3(*pos);
        } else {
            ++stats.geometriesWithoutVec3Position;
        }

        osg::Vec3Array* nrm = dynamic_cast<osg::Vec3Array*>(geom.getNormalArray());
        if (nrm) { scanNormals(*nrm); }

        auto& uvs = geom.getTexCoordArrayList();
        for (auto& arr : uvs) {
            osg::Vec2Array* uv = dynamic_cast<osg::Vec2Array*>(arr.get());
            if (!uv) { continue; }
            ++stats.uvArrays;
            for (const osg::Vec2& t : *uv) {
                if (!std::isfinite(t.x()) || !std::isfinite(t.y())) { ++stats.uvNonFinite; }
            }
        }

        for (unsigned i = 0; i < geom.getNumPrimitiveSets(); ++i) {
            osg::PrimitiveSet* ps = geom.getPrimitiveSet(i);
            if (!ps) { continue; }
            ++stats.primitiveSets;
            stats.primModeCounts[modeName(ps->getMode())]++;
            const osg::DrawElements* de = dynamic_cast<const osg::DrawElements*>(ps);
            if (!de) {
                ++stats.nonIndexedPrimitiveSets;
                const unsigned long long nTri =
                    triangleCountForMode(ps->getMode(), ps->getNumIndices());
                stats.pos.triangles += nTri;
                if (_world) { curAudit().triangles += nTri; }
                continue;
            }
            if (pos) {
                unsigned maxIdx = 0;
                for (unsigned k = 0; k < de->getNumIndices(); ++k) {
                    const unsigned idx = de->index(k);
                    if (idx > maxIdx) { maxIdx = idx; }
                }
                if (maxIdx >= nv) {
                    ++stats.primitiveSetsWithOutOfRangeIndex;
                    stats.maxOffendingIndex = std::max(stats.maxOffendingIndex,
                        static_cast<unsigned long long>(maxIdx));
                }
            }
            if (pos) { analyzeIndices(*de, nv, *pos); }
        }
    }

    unsigned long long triangleCountForMode(GLenum mode, unsigned n) const
    {
        switch (mode) {
        case osg::PrimitiveSet::TRIANGLES:      return n / 3;
        case osg::PrimitiveSet::TRIANGLE_STRIP: return n >= 3 ? n - 2 : 0;
        case osg::PrimitiveSet::TRIANGLE_FAN:   return n >= 3 ? n - 2 : 0;
        case osg::PrimitiveSet::POLYGON:        return n >= 3 ? n - 2 : 0;
        case osg::PrimitiveSet::QUADS:          return n / 4;
        case osg::PrimitiveSet::QUAD_STRIP:     return n >= 4 ? (n / 2) - 1 : 0;
        default:                                return 0;
        }
    }

    // Measure each triangle both in double and in float: the float pass is what catches the
    // magnitude classes that break area/distance/normal math downstream.
    void analyzeIndices(const osg::DrawElements& de, unsigned nv, osg::Vec3Array& pos)
    {
        const GLenum mode = de.getMode();
        const unsigned n = de.getNumIndices();
        std::vector<unsigned> tris;
        tris.reserve(n);
        switch (mode) {
        case osg::PrimitiveSet::TRIANGLES:
            for (unsigned k = 0; k + 2 < n; k += 3) {
                tris.push_back(de.index(k)); tris.push_back(de.index(k + 1)); tris.push_back(de.index(k + 2));
            }
            break;
        case osg::PrimitiveSet::TRIANGLE_STRIP:
        case osg::PrimitiveSet::TRIANGLE_FAN:
        case osg::PrimitiveSet::POLYGON:
            for (unsigned k = 2; k < n; ++k) {
                tris.push_back(de.index(0)); tris.push_back(de.index(k - 1)); tris.push_back(de.index(k));
            }
            break;
        default:
            return;   // lines/points/etc: nothing to measure as a triangle
        }
        if (tris.empty()) { return; }
        if (_world) { curAudit().triangles += tris.size() / 3; }

        Integrity& it = stats.pos;
        for (size_t k = 0; k + 2 < tris.size(); k += 3) {
            const unsigned i0 = tris[k], i1 = tris[k + 1], i2 = tris[k + 2];
            ++it.triangles;
            if (i0 >= nv || i1 >= nv || i2 >= nv) {
                ++it.indexOutOfRangeTris;
                it.maxOffendingIndex = std::max(it.maxOffendingIndex,
                    static_cast<unsigned long long>(std::max(i0, std::max(i1, i2))));
                continue;
            }
            if (i0 == i1 || i1 == i2 || i0 == i2) { ++it.repeatedIndexTris; }
            const osg::Vec3d aL(pos[i0].x(), pos[i0].y(), pos[i0].z());
            const osg::Vec3d bL(pos[i1].x(), pos[i1].y(), pos[i1].z());
            const osg::Vec3d cL(pos[i2].x(), pos[i2].y(), pos[i2].z());
            const osg::Vec3d a = world(pos[i0]);
            const osg::Vec3d b = world(pos[i1]);
            const osg::Vec3d c = world(pos[i2]);
            if (_world) { jointScan(aL, bL, cL, a, b, c); }
            if (!std::isfinite(a.x()) || !std::isfinite(a.y()) || !std::isfinite(a.z())
                || !std::isfinite(b.x()) || !std::isfinite(b.y()) || !std::isfinite(b.z())
                || !std::isfinite(c.x()) || !std::isfinite(c.y()) || !std::isfinite(c.z())) {
                ++it.nonFiniteSkippedTris;
                it.note("non-finite triangle vertex", a.x(), a.y(), a.z());
                continue;
            }
            const osg::Vec3d cr = (b - a) ^ (c - a);
            if (cr.length() == 0.0) {
                ++it.zeroAreaTris;
                it.note("zero-area triangle", a.x(), a.y(), a.z());
            }
            const osg::Vec3 fa(static_cast<float>(a.x()), static_cast<float>(a.y()), static_cast<float>(a.z()));
            const osg::Vec3 fb(static_cast<float>(b.x()), static_cast<float>(b.y()), static_cast<float>(b.z()));
            const osg::Vec3 fc(static_cast<float>(c.x()), static_cast<float>(c.y()), static_cast<float>(c.z()));
            const bool faSafe = floatSafe(a.x()) && floatSafe(a.y()) && floatSafe(a.z());
            const bool fbSafe = floatSafe(b.x()) && floatSafe(b.y()) && floatSafe(b.z());
            const bool fcSafe = floatSafe(c.x()) && floatSafe(c.y()) && floatSafe(c.z());
            it.floatOverflowVertices += (faSafe ? 0 : 1) + (fbSafe ? 0 : 1) + (fcSafe ? 0 : 1);
            const float farea = ((fb - fa) ^ (fc - fa)).length();
            if (!std::isfinite(farea)) {
                ++it.areaNotFiniteTris;
                it.note("float-space area became non-finite", a.x(), a.y(), a.z());
            }
        }
    }

    // One walk, both arms, same triangle. The exact-zero test is what a linear map cannot
    // preserve: three locally-collinear float32 vertices give cr == 0 exactly, and after a
    // rotation the same triangle's cross product rounds to something tiny but nonzero.
    void jointScan(const osg::Vec3d& aL, const osg::Vec3d& bL, const osg::Vec3d& cL,
                   const osg::Vec3d& a, const osg::Vec3d& b, const osg::Vec3d& c)
    {
        JointDegenerate& jn = stats.joint;
        const osg::Vec3d abL = bL - aL, acL = cL - aL;
        const osg::Vec3d ab = b - a, ac = c - a;
        const double areaL = (abL ^ acL).length();
        const double areaW = (ab ^ ac).length();
        if (!(std::isfinite(areaL) && std::isfinite(areaW))) {
            ++jn.areaNotFiniteEither;
            return;
        }
        ++jn.tris;

        const osg::Matrixd& m = _stack.back();
        const double det = std::fabs(matrixDet3(m));
        if (!jn.hasDet) { jn.minAbsDet = jn.maxAbsDet = det; jn.hasDet = true; }
        else { jn.minAbsDet = std::min(jn.minAbsDet, det); jn.maxAbsDet = std::max(jn.maxAbsDet, det); }
        if (!matrixIsIdentity(m)) { ++jn.underNonIdentityMatrix; }

        // |cr| = |ab|*|ac|*sin(theta): dividing by the edge lengths gives a scale-free
        // degeneracy measure, so a 1e-12 threshold means the same angular flatness at 1 mm
        // and at 800 km. Coincident vertices (denominator 0) classify as degenerate.
        auto relOf = [](double area, double la, double lc) {
            const double d = la * lc;
            if (d == 0.0) { return area == 0.0 ? 0.0 : DBL_MAX; }
            return area / d;
        };
        const double relL = relOf(areaL, abL.length(), acL.length());
        const double relW = relOf(areaW, ab.length(), ac.length());

        const bool zL = areaL == 0.0, zW = areaW == 0.0;
        jn.zeroAreaLocalExact += zL ? 1 : 0;
        jn.zeroAreaWorldExact += zW ? 1 : 0;
        if (zL && zW) { ++jn.zeroAreaBothExact; }
        else if (zL) {
            ++jn.zeroAreaLocalOnlyExact;
            jn.maxWorldAreaOnLocalExactZero = std::max(jn.maxWorldAreaOnLocalExactZero, areaW);
            jn.maxWorldRelativeOnLocalExactZero =
                std::max(jn.maxWorldRelativeOnLocalExactZero, relW);
            if (jn.examples.size() < 6) {
                std::ostringstream os;
                os << "localExactZero worldNonZero areaW=" << areaW << " relW=" << relW
                   << " |det|=" << det
                   << " L(" << aL.x() << ',' << aL.y() << ',' << aL.z() << " ~ "
                   << bL.x() << ',' << bL.y() << ',' << bL.z() << " ~ "
                   << cL.x() << ',' << cL.y() << ',' << cL.z() << ')'
                   << " W(" << a.x() << ',' << a.y() << ',' << a.z() << " ~ "
                   << b.x() << ',' << b.y() << ',' << b.z() << " ~ "
                   << c.x() << ',' << c.y() << ',' << c.z() << ')';
                jn.note(os.str());
            }
        } else if (zW) {
            ++jn.zeroAreaWorldOnlyExact;
            if (jn.examples.size() < 6) {
                std::ostringstream os;
                os << "worldExactZero localNonZero areaL=" << areaL << " relL=" << relL
                   << " |det|=" << det
                   << " L(" << aL.x() << ',' << aL.y() << ',' << aL.z() << " ~ "
                   << bL.x() << ',' << bL.y() << ',' << bL.z() << " ~ "
                   << cL.x() << ',' << cL.y() << ',' << cL.z() << ')'
                   << " W(" << a.x() << ',' << a.y() << ',' << a.z() << " ~ "
                   << b.x() << ',' << b.y() << ',' << b.z() << " ~ "
                   << c.x() << ',' << c.y() << ',' << c.z() << ')';
                jn.note(os.str());
            }
        }

        const bool rL = relL <= 1e-12, rW = relW <= 1e-12;
        jn.relDegenerateLocal += rL ? 1 : 0;
        jn.relDegenerateWorld += rW ? 1 : 0;
        if (rL && !rW) { ++jn.relDegenerateLocalOnly; }
        else if (rW && !rL) { ++jn.relDegenerateWorldOnly; }
    }

    osg::Vec3d world(const osg::Vec3& p) const
    {
        const osg::Vec3d v(p.x(), p.y(), p.z());
        // _world=false is the S1-local arm: without this branch both walkers push the same
        // matrix stack and the "local" column silently becomes a second world measurement.
        return _world ? v * _stack.back() : v;
    }

    void unionOfGeometryBound(osg::Geometry& geom)
    {
        ++stats.boundUnionGeoms;
        const osg::BoundingBox lb = geom.getBoundingBox();
        if (!lb.valid()) {
            ++stats.boundUnionInvalid;
            return;
        }
        const osg::Matrixd& m = _stack.back();
        for (unsigned i = 0; i < 8; ++i) {
            const osg::Vec3d c = osg::Vec3d(lb.corner(i)) * m;
            if (!std::isfinite(c.x()) || !std::isfinite(c.y()) || !std::isfinite(c.z())) { continue; }
            if (!stats.boundUnionValid) {
                for (int a = 0; a < 3; ++a) {
                    stats.boundUnionMin[a] = c[a];
                    stats.boundUnionMax[a] = c[a];
                }
                stats.boundUnionValid = true;
            } else {
                for (int a = 0; a < 3; ++a) {
                    if (c[a] < stats.boundUnionMin[a]) { stats.boundUnionMin[a] = c[a]; }
                    if (c[a] > stats.boundUnionMax[a]) { stats.boundUnionMax[a] = c[a]; }
                }
            }
        }
    }

    void scanVec3(osg::Vec3Array& arr)
    {
        Integrity& it = stats.pos;
        for (const osg::Vec3& p : arr) {
            const osg::Vec3d d = world(p);
            if (!std::isfinite(d.x()) || !std::isfinite(d.y()) || !std::isfinite(d.z())) {
                ++it.nonFiniteVertices;
                it.note("non-finite vertex", d.x(), d.y(), d.z());
                continue;
            }
            if (!(floatSafe(d.x()) && floatSafe(d.y()) && floatSafe(d.z()))) { ++it.floatOverflowVertices; }
            ++it.vertices;
            it.mag[0].add(d.x());
            it.mag[1].add(d.y());
            it.mag[2].add(d.z());
            if (_world) {
                TransformAudit& ta = curAudit();
                ++ta.vertices;
                const double lv[3] = { std::fabs(p.x()), std::fabs(p.y()), std::fabs(p.z()) };
                const double dv[3] = { d.x(), d.y(), d.z() };
                for (int c = 0; c < 3; ++c) {
                    if (lv[c] > ta.localAxisMax[c]) { ta.localAxisMax[c] = lv[c]; }
                    if (!ta.hasWorld || dv[c] < ta.worldAxisMin[c]) { ta.worldAxisMin[c] = dv[c]; }
                    if (!ta.hasWorld || dv[c] > ta.worldAxisMax[c]) { ta.worldAxisMax[c] = dv[c]; }
                }
                ta.hasWorld = true;
            }
        }
    }

    void scanNormals(osg::Vec3Array& arr)
    {
        Integrity& it = stats.normal;
        for (const osg::Vec3& n : arr) {
            ++it.normals;
            const double len = std::sqrt(double(n.x()) * n.x() + double(n.y()) * n.y() + double(n.z()) * n.z());
            if (!std::isfinite(len)) { ++it.nonFiniteNormals; continue; }
            if (len == 0.0) { ++it.zeroLengthNormals; }
            else if (std::fabs(len - 1.0) > 1e-3) { ++it.normalsOffUnit; }
        }
    }

    static const char* modeName(GLenum m)
    {
        switch (m) {
        case osg::PrimitiveSet::POINTS:          return "POINTS";
        case osg::PrimitiveSet::LINES:           return "LINES";
        case osg::PrimitiveSet::LINE_STRIP:      return "LINE_STRIP";
        case osg::PrimitiveSet::LINE_LOOP:       return "LINE_LOOP";
        case osg::PrimitiveSet::TRIANGLES:       return "TRIANGLES";
        case osg::PrimitiveSet::TRIANGLE_STRIP:  return "TRIANGLE_STRIP";
        case osg::PrimitiveSet::TRIANGLE_FAN:    return "TRIANGLE_FAN";
        case osg::PrimitiveSet::QUADS:           return "QUADS";
        case osg::PrimitiveSet::QUAD_STRIP:      return "QUAD_STRIP";
        case osg::PrimitiveSet::POLYGON:         return "POLYGON";
        default:                                 return "OTHER";
        }
    }

    bool _world;
    bool _childFirst;
    std::string _resourceDir;
    std::vector<osg::Matrixd> _stack;
    std::vector<size_t> _auditIdx;
    std::map<const osg::Transform*, size_t> _auditByNode;
    std::set<const osg::StateSet*> _seenStateSets;
    std::set<const osg::Texture*> _seenTextures;
    std::set<const osg::Image*> _seenImages;
};

void writeMagnitudeJson(std::ostream& os, const Magnitude& m)
{
    os << "{\"count\":" << m.count
       << ",\"maxAbs\":" << m.maxAbs
       << ",\"min\":" << (m.hasRange ? m.min : 0.0)
       << ",\"max\":" << (m.hasRange ? m.max : 0.0) << '}';
}

void writeIntegrityJson(std::ostream& os, const char* name, const Integrity& it)
{
    os << '"' << name << "\":{\"vertices\":" << it.vertices
       << ",\"nonFiniteVertices\":" << it.nonFiniteVertices
       << ",\"floatOverflowVertices\":" << it.floatOverflowVertices
       << ",\"triangles\":" << it.triangles
       << ",\"repeatedIndexTris\":" << it.repeatedIndexTris
       << ",\"zeroAreaTris\":" << it.zeroAreaTris
       << ",\"areaNotFiniteTris\":" << it.areaNotFiniteTris
       << ",\"indexOutOfRangeTris\":" << it.indexOutOfRangeTris
       << ",\"maxOffendingIndex\":" << it.maxOffendingIndex
       << ",\"nonFiniteSkippedTris\":" << it.nonFiniteSkippedTris
       << ",\"normals\":" << it.normals
       << ",\"zeroLengthNormals\":" << it.zeroLengthNormals
       << ",\"nonFiniteNormals\":" << it.nonFiniteNormals
       << ",\"normalsOffUnit\":" << it.normalsOffUnit
       << ",\"x\":";
    writeMagnitudeJson(os, it.mag[0]);
    os << ",\"y\":";
    writeMagnitudeJson(os, it.mag[1]);
    os << ",\"z\":";
    writeMagnitudeJson(os, it.mag[2]);
    os << ",\"examples\":[";
    for (size_t i = 0; i < it.examples.size(); ++i) {
        if (i) { os << ','; }
        os << '"' << jsonEscape(it.examples[i]) << '"';
    }
    os << "]}";
}

void writeJointJson(std::ostream& os, const char* name, const JointDegenerate& jn)
{
    os << '"' << name << "\":{\"trisMeasured\":" << jn.tris
       << ",\"skippedEitherNonFinite\":" << jn.skippedEitherNonFinite
       << ",\"areaNotFiniteEither\":" << jn.areaNotFiniteEither
       << ",\"underNonIdentityMatrix\":" << jn.underNonIdentityMatrix
       << ",\"minAbsDet\":" << (jn.hasDet ? jn.minAbsDet : 1.0)
       << ",\"maxAbsDet\":" << (jn.hasDet ? jn.maxAbsDet : 1.0)
       << ",\"zeroAreaLocalExact\":" << jn.zeroAreaLocalExact
       << ",\"zeroAreaWorldExact\":" << jn.zeroAreaWorldExact
       << ",\"zeroAreaBothExact\":" << jn.zeroAreaBothExact
       << ",\"zeroAreaLocalOnlyExact\":" << jn.zeroAreaLocalOnlyExact
       << ",\"zeroAreaWorldOnlyExact\":" << jn.zeroAreaWorldOnlyExact
       << ",\"maxWorldAreaOnLocalExactZero\":" << jn.maxWorldAreaOnLocalExactZero
       << ",\"maxWorldRelativeOnLocalExactZero\":" << jn.maxWorldRelativeOnLocalExactZero
       << ",\"relDegenerateLocal\":" << jn.relDegenerateLocal
       << ",\"relDegenerateWorld\":" << jn.relDegenerateWorld
       << ",\"relDegenerateLocalOnly\":" << jn.relDegenerateLocalOnly
       << ",\"relDegenerateWorldOnly\":" << jn.relDegenerateWorldOnly
       << ",\"examples\":[";
    for (size_t i = 0; i < jn.examples.size(); ++i) {
        if (i) { os << ','; }
        os << '"' << jsonEscape(jn.examples[i]) << '"';
    }
    os << "]}";
}

// Rank by "how far this node's own scale is from 1" first (the unit-scale suspects), then by the
// world extent it produces, and always report the totals so the attribution can be checked
// against the single-arm walkers rather than trusted.
void writeTransformAuditJson(std::ostream& os, const GraphWalker::Stats& s)
{
    auto scaleDeviation = [](const TransformAudit& a) {
        double d = 0.0;
        for (int c = 0; c < 3; ++c) {
            if (a.localScale[c] <= 0.0) { continue; }
            d = std::max(d, std::fabs(std::log(a.localScale[c])));
        }
        return d;
    };
    auto worldExtent = [](const TransformAudit& a) {
        if (!a.hasWorld) { return 0.0; }
        double e = 0.0;
        for (int c = 0; c < 3; ++c) { e = std::max(e, a.worldAxisMax[c] - a.worldAxisMin[c]); }
        return e;
    };

    unsigned long long axesOffUnit = 0, sharedNodes = 0, recordsWithGeometry = 0;
    unsigned long long sumGeometries = 0, sumVertices = 0, sumTriangles = 0;
    double minAxis = 0.0, maxAxis = 0.0;
    bool hasAxis = false;
    for (size_t i = 1; i < s.audits.size(); ++i) {
        const TransformAudit& a = s.audits[i];
        if (a.visits > 1) { ++sharedNodes; }
        if (a.geometries) { ++recordsWithGeometry; }
        for (int c = 0; c < 3; ++c) {
            const double v = a.localScale[c];
            if (v <= 0.0) { continue; }
            if (std::fabs(v - 1.0) > 1e-3) { ++axesOffUnit; }
            if (!hasAxis || v < minAxis) { minAxis = v; }
            if (!hasAxis || v > maxAxis) { maxAxis = v; }
            hasAxis = true;
        }
    }
    for (const TransformAudit& a : s.audits) {
        sumGeometries += a.geometries;
        sumVertices += a.vertices;
        sumTriangles += a.triangles;
    }

    std::vector<size_t> order;
    order.reserve(s.audits.size());
    for (size_t i = 0; i < s.audits.size(); ++i) { order.push_back(i); }
    std::sort(order.begin(), order.end(), [&](size_t x, size_t y) {
        const TransformAudit& a = s.audits[x];
        const TransformAudit& b = s.audits[y];
        const double da = scaleDeviation(a), db = scaleDeviation(b);
        if (da != db) { return da > db; }
        return worldExtent(a) > worldExtent(b);
    });

    os << "\"S1_transformAudit\":{\"transformRecords\":" << (s.audits.empty() ? 0 : s.audits.size() - 1)
       << ",\"recordsWithGeometry\":" << recordsWithGeometry
       << ",\"sharedTransformNodes\":" << sharedNodes
       << ",\"axesOffUnit\":" << axesOffUnit
       << ",\"minAxisScale\":" << (hasAxis ? minAxis : 1.0)
       << ",\"maxAxisScale\":" << (hasAxis ? maxAxis : 1.0)
       << ",\"distinctLocalScales\":" << s.localScaleHistogram.size()
       << ",\"sumGeometries\":" << sumGeometries
       << ",\"sumVertices\":" << sumVertices
       << ",\"sumTriangles\":" << sumTriangles
       << ",\"scaleHistogram\":[";
    {
        size_t emitted = 0;
        for (const auto& kv : s.localScaleHistogram) {
            if (emitted >= 24) { break; }
            if (emitted) { os << ','; }
            ++emitted;
            os << "{\"scale\":\"" << jsonEscape(kv.first) << "\",\"nodes\":" << kv.second << '}';
        }
    }
    os << "],\"topRecords\":[";
    {
        size_t emitted = 0;
        for (const size_t idx : order) {
            if (emitted >= 16) { break; }
            const TransformAudit& a = s.audits[idx];
            if (emitted) { os << ','; }
            ++emitted;
            os << "{\"index\":" << idx
               << ",\"isRootBucket\":" << (a.node == nullptr ? "true" : "false")
               << ",\"visits\":" << a.visits
               << ",\"geometries\":" << a.geometries
               << ",\"vertices\":" << a.vertices
               << ",\"triangles\":" << a.triangles
               << ",\"localScale\":[" << a.localScale[0] << ',' << a.localScale[1] << ','
               << a.localScale[2] << ']'
               << ",\"localScaleDeviation\":" << scaleDeviation(a)
               << ",\"localDet\":" << a.localDet
               << ",\"accumScale\":[" << a.accumScale[0] << ',' << a.accumScale[1] << ','
               << a.accumScale[2] << ']'
               << ",\"accumDetMin\":" << a.accumDetMin
               << ",\"accumDetMax\":" << a.accumDetMax
               << ",\"accumTransLen\":" << a.accumTransLen
               << ",\"accumIdentity\":" << (a.accumIdentity ? "true" : "false")
               << ",\"localAxisMax\":[" << a.localAxisMax[0] << ',' << a.localAxisMax[1] << ','
               << a.localAxisMax[2] << ']'
               << ",\"worldAxisMin\":[" << a.worldAxisMin[0] << ',' << a.worldAxisMin[1] << ','
               << a.worldAxisMin[2] << ']'
               << ",\"worldAxisMax\":[" << a.worldAxisMax[0] << ',' << a.worldAxisMax[1] << ','
               << a.worldAxisMax[2] << ']'
               << ",\"worldExtent\":" << worldExtent(a) << '}';
        }
    }
    os << "]}";
}

} // namespace

int main(int argc, char** argv)
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    osg::setNotifyLevel(osg::NotifySeverity::NOTICE);
    osgGISPlugins::Utils::registerFileAliases();

    osg::ArgumentParser arguments(&argc, argv);
    const std::string input = parseArgument(arguments, "-i", std::string());
    const std::string jsonOut = parseArgument(arguments, "-json", std::string());
    const bool useOptimizer = arguments.find("-noopt") <= 0;
    const bool runSourceModel = arguments.find("-nosm") <= 0;
    const int topLevel = static_cast<int>(parseArgument(arguments, "-top", 8));

    if (input.empty()) {
        std::cerr << "usage: modelstats -i <model.fbx|model.obj> [-json out.json] [-noopt] [-nosm] [-top N]"
                  << std::endl;
        return 1;
    }

    const std::string utf8Input = osgDB::convertStringFromCurrentCodePageToUTF8(input.c_str());
    const std::string originalCwd = osgDB::getCurrentWorkingDirectory();
    const std::string inputPath = osgDB::getFilePath(utf8Input);
    // Mirror model23dtiles: the reader needs the model's own directory as CWD so .fbm
    // textures / mtllib files resolve exactly as they do in a conversion run.
    osgDB::setCurrentWorkingDirectory(inputPath);

    const std::string ext = osgDB::getLowerCaseFileExtension(utf8Input);
    std::string optionStr;
    if (ext == "obj") { optionStr += "noReverseFaces "; }
    if (ext == "fbx") { optionStr += "TessellatePolygons "; }

    osg::ref_ptr<osgDB::Options> ro = new osgDB::Options;
    ro->setOptionString(optionStr);

    const auto t0 = std::chrono::steady_clock::now();
    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(utf8Input, ro.get());
    const auto t1 = std::chrono::steady_clock::now();
    const double readSeconds = std::chrono::duration<double>(t1 - t0).count();

    std::ostringstream buf;
    buf << "{\n\"input\":\"" << jsonEscape(utf8Input) << '"'
        << ",\"ext\":\"" << jsonEscape(ext) << '"'
        << ",\"readOptions\":\"" << jsonEscape(optionStr) << '"'
        << ",\"optimizer\":\"" << (useOptimizer ? "INDEX_MESH" : "none") << '"'
        << ",\"progressCallback\":\"none (model23dtiles installs one unless -sp)\""
        << ",\"readSeconds\":" << readSeconds
        << ",\"loadStatus\":\"" << (node.valid() ? "OK" : "FAIL") << '"';

    if (!node.valid()) {
        buf << ",\"loadError\":\"osgDB::readNodeFile returned null\"}\n";
        std::cout << buf.str() << std::endl;
        if (!jsonOut.empty()) { std::ofstream os(jsonOut); os << buf.str(); }
        osgDB::setCurrentWorkingDirectory(originalCwd);
        return 2;
    }

    osg::ref_ptr<osg::Node> working = node;
    if (useOptimizer) {
        osgUtil::Optimizer optimizer;
        optimizer.optimize(working.get(), osgUtil::Optimizer::INDEX_MESH);
    }

    GraphWalker localWalker(false, inputPath);
    working->accept(localWalker);
    GraphWalker worldWalker(true, inputPath);
    working->accept(worldWalker);
    // Fourth arm, same graph, the previously used (wrong) accumulation order, kept so the
    // superseded numbers stay comparable with the archived probe runs.
    GraphWalker altWalker(true, inputPath, false);
    working->accept(altWalker);
    // The local walker shares the visitor class, so the matrix flags it sets are the
    // authoritative reading of this scene's transform stack.
    worldWalker.stats.nonIdentityWorldMatrices = localWalker.stats.nonIdentityWorldMatrices;
    worldWalker.stats.worldEqualsLocal = localWalker.stats.worldEqualsLocal;
    const GraphWalker::Stats& s = worldWalker.stats;

    buf << ",\"graph\":{\"nodes\":" << s.nodes
        << ",\"groups\":" << s.groups
        << ",\"geodes\":" << s.geodes
        << ",\"transforms\":" << localWalker.stats.transforms
        << ",\"nonIdentityWorldMatrices\":" << s.nonIdentityWorldMatrices
        << ",\"worldEqualsLocal\":" << (s.worldEqualsLocal ? "true" : "false")
        << ",\"drawables\":" << s.drawables
        << ",\"geometries\":" << s.geometries
        << ",\"nonGeometryDrawables\":" << s.nonGeometryDrawables
        << ",\"geometriesWithoutVec3Position\":" << s.geometriesWithoutVec3Position
        << ",\"primitiveSets\":" << s.primitiveSets
        << ",\"nonIndexedPrimitiveSets\":" << s.nonIndexedPrimitiveSets
        << ",\"primitiveSetsWithOutOfRangeIndex\":" << s.primitiveSetsWithOutOfRangeIndex
        << ",\"maxOffendingIndex\":" << s.maxOffendingIndex
        << ",\"vertexArrayValues\":" << s.vertexArrayValues
        << ",\"uvArrays\":" << s.uvArrays
        << ",\"uvNonFinite\":" << s.uvNonFinite
        << ",\"stateSets\":" << s.stateSets
        << ",\"stateSetsUnique\":" << s.stateSetsUnique
        << ",\"blendStateSets\":" << s.blendStateSets
        << ",\"textures\":" << s.textures
        << ",\"texturesUnique\":" << s.texturesUnique
        << ",\"images\":" << s.images
        << ",\"imagesUnique\":" << s.imagesUnique
        << ",\"imagesUnnamed\":" << s.imagesUnnamed
        << ",\"imageBytesLoaded\":" << s.imageBytes
        << ",\"imagesResident\":" << s.imagesResident
        << ",\"imagesDeclaredOnly\":" << s.imagesDeclaredOnly
        << ",\"imageBytesResident\":" << s.imageBytesResident
        << ",\"distinctImageFiles\":" << s.imageFiles.size()
        << ",\"missingImageFiles\":" << s.missingImages.size()
        << ",\"missingImageList\":[";
    for (size_t i = 0; i < s.missingImages.size() && i < static_cast<size_t>(topLevel); ++i) {
        if (i) { buf << ','; }
        buf << '"' << jsonEscape(s.missingImages[i]) << '"';
    }
    buf << "],\"primitiveModes\":{";
    {
        bool first = true;
        for (const auto& kv : s.primModeCounts) {
            if (!first) { buf << ','; }
            first = false;
            buf << '"' << kv.first << "\":" << kv.second;
        }
    }
    buf << "}}";

    buf << ',';
    writeIntegrityJson(buf, "S1_localPosition", localWalker.stats.pos);
    buf << ',';
    writeIntegrityJson(buf, "S1_worldPosition", s.pos);
    buf << ',';
    writeIntegrityJson(buf, "S1_normals", s.normal);
    buf << ',';
    writeJointJson(buf, "S1_jointDegenerate", s.joint);
    buf << ',';
    writeTransformAuditJson(buf, s);
    buf << ',';
    writeIntegrityJson(buf, "S1_parentFirstAccumPosition", altWalker.stats.pos);
    buf << ',';
    writeJointJson(buf, "S1_parentFirstJointDegenerate", altWalker.stats.joint);
    buf << ",\"S1_parentFirstAudit\":{";
    {
        unsigned long long sv = 0, st = 0;
        for (const TransformAudit& t : altWalker.stats.audits) { sv += t.vertices; st += t.triangles; }
        buf << "\"sumAllVertices\":" << sv << ",\"sumAllTriangles\":" << st << '}';
    }

    osg::ComputeBoundsVisitor cbv;
    working->accept(cbv);
    const osg::BoundingBox bb = cbv.getBoundingBox();
    buf << ",\"computeBoundsVisitor\":{\"valid\":" << (bb.valid() ? "true" : "false")
        << ",\"center\":[" << bb.center().x() << ',' << bb.center().y() << ',' << bb.center().z() << ']'
        << ",\"radius\":" << bb.radius()
        << ",\"min\":[" << bb.xMin() << ',' << bb.yMin() << ',' << bb.zMin() << ']'
        << ",\"max\":[" << bb.xMax() << ',' << bb.yMax() << ',' << bb.zMax() << "]}";

    // Ruler self-check, recorded on every run. The world arm is only trustworthy if it agrees
    // with OSG's independent bounds computation. Equality is the WRONG expectation here:
    // osg::ComputeBoundsVisitor folds in every vertex array of a Geometry (normals, colours,
    // and any drawable bound the importer set), while the world arm reads POSITION only. The
    // defensible relation is containment -- the position box must sit inside the CBV box, with
    // zero outward violation -- plus a tight centerDelta/halfDiagDelta. Outward violations or a
    // large relative centre offset mean the world column is unusable and no coordinate
    // conclusion may be drawn from it.
    {
        const double wmin[3] = { s.pos.mag[0].min, s.pos.mag[1].min, s.pos.mag[2].min };
        const double wmax[3] = { s.pos.mag[0].max, s.pos.mag[1].max, s.pos.mag[2].max };
        const double bmin[3] = { bb.xMin(), bb.yMin(), bb.zMin() };
        const double bmax[3] = { bb.xMax(), bb.yMax(), bb.zMax() };
        double wc[3], wh[3];
        for (int c = 0; c < 3; ++c) {
            wc[c] = 0.5 * (wmin[c] + wmax[c]);
            wh[c] = 0.5 * (wmax[c] - wmin[c]);
        }
        const double centerDelta = std::sqrt(std::pow(wc[0] - bb.center().x(), 2)
                                            + std::pow(wc[1] - bb.center().y(), 2)
                                            + std::pow(wc[2] - bb.center().z(), 2));
        const double walkerHalfDiag = std::sqrt(wh[0] * wh[0] + wh[1] * wh[1] + wh[2] * wh[2]);
        unsigned long long outwardAxes = 0;
        double maxOutward = 0.0, maxInwardSlack = 0.0;
        for (int c = 0; c < 3; ++c) {
            const double outLo = bmin[c] - wmin[c];   // >0 when walker extends below CBV
            const double outHi = wmax[c] - bmax[c];   // >0 when walker extends above CBV
            const double slackLo = wmin[c] - bmin[c]; // >=0: CBV reaches further down
            const double slackHi = bmax[c] - wmax[c]; // >=0: CBV reaches further up
            if (outLo > 1e-6 || outHi > 1e-6) ++outwardAxes;
            maxOutward = std::max(maxOutward, std::max(outLo, outHi));
            maxInwardSlack = std::max(maxInwardSlack, std::max(slackLo, slackHi));
        }
        buf << ",\"worldArmVsComputeBounds\":{\"walkerCenter\":[" << wc[0] << ',' << wc[1] << ','
            << wc[2] << "],\"walkerMin\":[" << wmin[0] << ',' << wmin[1] << ',' << wmin[2] << ']'
            << ",\"walkerMax\":[" << wmax[0] << ',' << wmax[1] << ',' << wmax[2] << ']'
            << ",\"walkerHalfDiag\":" << walkerHalfDiag
            << ",\"centerDelta\":" << centerDelta
            << ",\"centerDeltaRelative\":" << (centerDelta / std::max(1e-9, static_cast<double>(bb.radius())))
            << ",\"halfDiagDelta\":" << (walkerHalfDiag - static_cast<double>(bb.radius()))
            << ",\"outwardViolationAxes\":" << outwardAxes
            << ",\"maxOutwardMeters\":" << maxOutward
            << ",\"cbvExtraSlackMeters\":" << maxInwardSlack;
        // Attribution of the CBV excess.
        if (s.boundUnionValid) {
            unsigned cbvOutsideUnion = 0;
            double maxCbvOutside = 0.0;
            for (int c = 0; c < 3; ++c) {
                const double outLo = s.boundUnionMin[c] - bmin[c];
                const double outHi = bmax[c] - s.boundUnionMax[c];
                if (outLo > 1e-3 || outHi > 1e-3) { ++cbvOutsideUnion; }
                maxCbvOutside = std::max(maxCbvOutside, std::max(outLo, outHi));
            }
            buf << ",\"geometryBoundUnion\":{\"geoms\":" << s.boundUnionGeoms
                << ",\"invalidBounds\":" << s.boundUnionInvalid
                << ",\"min\":[" << s.boundUnionMin[0] << ',' << s.boundUnionMin[1] << ','
                << s.boundUnionMin[2] << ']'
                << ",\"max\":[" << s.boundUnionMax[0] << ',' << s.boundUnionMax[1] << ','
                << s.boundUnionMax[2] << ']'
                << ",\"cbvOutsideUnionAxes\":" << cbvOutsideUnion
                << ",\"maxCbvOutsideMeters\":" << maxCbvOutside << '}';
        } else {
            buf << ",\"geometryBoundUnion\":{\"geoms\":" << s.boundUnionGeoms
                << ",\"invalidBounds\":" << s.boundUnionInvalid << ",\"valid\":false}";
        }
        buf << '}';
    }

    if (runSourceModel) {
        using namespace osgGISPlugins::production;
        const auto t2 = std::chrono::steady_clock::now();
        SourceModel model;
        const bool built = model.build(working.get());
        const auto t3 = std::chrono::steady_clock::now();
        unsigned long long smVerts = 0, smNonFinite = 0, smFloatUnsafe = 0;
        double smMaxAbs = 0.0;
        for (const SourceMesh& m : model.meshes) {
            if (!m.positions.valid()) { continue; }
            for (const osg::Vec3& p : *m.positions) {
                ++smVerts;
                const double x = p.x(), y = p.y(), z = p.z();
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) { ++smNonFinite; continue; }
                if (!floatSafe(x) || !floatSafe(y) || !floatSafe(z)) { ++smFloatUnsafe; }
                smMaxAbs = std::max({ smMaxAbs, std::fabs(x), std::fabs(y), std::fabs(z) });
            }
        }
        unsigned long long smTriIndexBad = 0;
        for (const PrimRef& pr : model.prims) {
            unsigned idx[3];
            for (unsigned t = 0; t < pr.triCount; ++t) {
                model.triangleIndices(pr, t, idx);
                if (idx[0] >= smVerts || idx[1] >= smVerts || idx[2] >= smVerts) { ++smTriIndexBad; }
            }
        }
        buf << ",\"S3_sourceModel\":{\"buildOk\":" << (built ? "true" : "false")
            << ",\"buildSeconds\":" << std::chrono::duration<double>(t3 - t2).count()
            << ",\"objects\":" << model.objects.size()
            << ",\"meshes\":" << model.meshes.size()
            << ",\"prims\":" << model.prims.size()
            << ",\"instances\":" << model.instances.size()
            << ",\"features\":" << model.featureCount()
            << ",\"triangles\":" << model.totalTriangles()
            << ",\"vertices\":" << smVerts
            << ",\"nonFiniteVertices\":" << smNonFinite
            << ",\"floatUnsafeVertices\":" << smFloatUnsafe
            << ",\"maxAbsCoordinate\":" << smMaxAbs
            << ",\"triangleIndicesOutOfArray\":" << smTriIndexBad
            << ",\"indexViolations\":" << model.indexViolations.size()
            << ",\"bounds\":{\"xMin\":" << model.bounds.xMin() << ",\"yMin\":" << model.bounds.yMin()
            << ",\"zMin\":" << model.bounds.zMin() << ",\"xMax\":" << model.bounds.xMax()
            << ",\"yMax\":" << model.bounds.yMax() << ",\"zMax\":" << model.bounds.zMax() << '}'
            << ",\"materials\":" << model.materialCount() << '}';

        // The decisive accumulation-order check. SourceModelVisitor pushes the node's local
        // matrix as _current = local * _current (src/3dtiles/production/SourceModel.cpp:239),
        // i.e. child-first, and model.bounds is the union of positions taken through those
        // matrices. The world arm must match it exactly, and the parent-first arm must not:
        // that is what makes "world" mean the scene the pipeline actually exports.
        {
            const double smMin[3] = { model.bounds.xMin(), model.bounds.yMin(), model.bounds.zMin() };
            const double smMax[3] = { model.bounds.xMax(), model.bounds.yMax(), model.bounds.zMax() };
            const double wpMin[3] = { s.pos.mag[0].min, s.pos.mag[1].min, s.pos.mag[2].min };
            const double wpMax[3] = { s.pos.mag[0].max, s.pos.mag[1].max, s.pos.mag[2].max };
            double maxAbsDiff = 0.0;
            for (int c = 0; c < 3; ++c) {
                maxAbsDiff = std::max({ maxAbsDiff, std::fabs(smMin[c] - wpMin[c]),
                                        std::fabs(smMax[c] - wpMax[c]) });
            }
            double maxPfDiff = 0.0;
            const double pfMin[3] = { altWalker.stats.pos.mag[0].min, altWalker.stats.pos.mag[1].min,
                                      altWalker.stats.pos.mag[2].min };
            const double pfMax[3] = { altWalker.stats.pos.mag[0].max, altWalker.stats.pos.mag[1].max,
                                      altWalker.stats.pos.mag[2].max };
            for (int c = 0; c < 3; ++c) {
                maxPfDiff = std::max({ maxPfDiff, std::fabs(smMin[c] - pfMin[c]),
                                       std::fabs(smMax[c] - pfMax[c]) });
            }
            buf << ",\"worldArmVsSourceModelBounds\":{\"maxAxisDelta\":" << maxAbsDiff
                << ",\"parentFirstMaxAxisDelta\":" << maxPfDiff << '}';
        }

        // Instance accounting: SourceModel moves repeated copies of identical geometry into
        // SourceInstance records, so the prim triangle total is legitimately lower than the
        // scene graph's. The identity graphTris == primTris + Σ(triangles of each prototype
        // replicated) is what separates that deduction from silently dropped geometry.
        unsigned long long objectsTris = 0;
        for (const SourceObject& o : model.objects) { objectsTris += o.totalTris; }
        unsigned long long instanceCopyTris = 0;
        unsigned long long instancesWithUnknownPrototype = 0;
        for (const SourceInstance& inst : model.instances) {
            const auto fit = model.featureToObject.find(inst.prototypeFeatureId);
            if (fit == model.featureToObject.end() || fit->second >= model.objects.size()) {
                ++instancesWithUnknownPrototype;
                continue;
            }
            instanceCopyTris += model.objects[fit->second].totalTris;
        }
        const unsigned long long primTris = model.totalTriangles();
        const long long unexplained =
            static_cast<long long>(s.pos.triangles)
            - static_cast<long long>(primTris + instanceCopyTris);

        buf << ",\"triangleAccounting\":{\"graphWorldTriangles\":" << s.pos.triangles
            << ",\"primTriangles\":" << primTris
            << ",\"objectsTotalTris\":" << objectsTris
            << ",\"instanceCopyTriangles\":" << instanceCopyTris
            << ",\"instancesWithUnknownPrototype\":" << instancesWithUnknownPrototype
            << ",\"unexplainedDifference\":" << unexplained << '}';

        buf << ",\"consistency\":{\"worldGraphTriangles\": " << s.pos.triangles
            << ",\"sourceModelTriangles\": " << model.totalTriangles()
            << ",\"vertexArrayValues\": " << s.vertexArrayValues
            << ",\"sourceModelVertices\": " << smVerts
            << ",\"note\":\"sourceModelTriangles counts retained prims only; instance copies are "
               "reconstructed from SourceInstance at materialize time, see triangleAccounting\"}";
    }

    buf << "}\n";

    std::cout << buf.str() << std::endl;
    if (!jsonOut.empty()) {
        std::ofstream os(jsonOut);
        os << buf.str();
        if (!os) { std::cerr << "modelstats: cannot write " << jsonOut << std::endl; }
    }
    osgDB::setCurrentWorkingDirectory(originalCwd);
    return 0;
}
