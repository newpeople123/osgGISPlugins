#ifndef READERWRITERGLTF_H
#define READERWRITERGLTF_H 1
#include <osgDB/ReaderWriter>
#include "osgdb_gltf/Osg2Gltf.h"
#include "osgdb_gltf/b3dm/B3DM.h"
#include "osgdb_gltf/b3dm/BatchTableHierarchy.h"
using namespace osgGISPlugins;
class ReaderWriterGLTF:public osgDB::ReaderWriter
{
private:
    template<class T>
    void putVal(std::vector<unsigned char>& buf, T val) const;
public:
    ReaderWriterGLTF() {
        supportsExtension("gltf", "GLTF format");

        supportsExtension("glb", "GLB format");

        supportsExtension("b3dm", "3D Tiles Batch 3D Model");

        supportsOption("eb",
            "eb (embedBuffers): Embeds buffers directly into the resulting file. "
            "This option allows you to include all buffer data within the GLTF/GLB file itself.");

        supportsOption("pp",
            "pp (prettyPrint): Formats the JSON in the resulting file for readability. "
            "Enabling this option will make the JSON output more human-readable, but may increase the file size slightly.");

        supportsOption("noMergeMaterial",
            "noMergeMaterial: If this option is set, materials will not be merged. "
            "By default, materials are merged to reduce draw calls, which can improve rendering performance. "
            "However, merging materials may increase GPU memory usage and will modify texture coordinates. "
            "Note: Only materials with baseColorTexture are merged, and only TEXCOORD_0 is processed.");

        supportsOption("noMergeMesh",
            "noMergeMesh: If this option is set, meshes will not be merged. "
            "By default, meshes are merged to reduce draw calls, typically when used together with material merging "
            "(i.e., when noMergeMaterial is not set). "
            "Merging meshes can enhance rendering performance by reducing draw calls, but it may also increase GPU memory usage.");

        supportsOption("quantize",
            "quantize: Enables vector quantization. If enabled, it will compress the data vectors, "
            "but Draco compression cannot be used simultaneously.");

        supportsOption("ct=<string>",
            "ct (compression type): Specifies the type of compression to use. "
            "Possible values: draco, meshopt. Default: none.");

        supportsOption("vp=<number>",
            "vp (position quantization): Uses number-bit quantization for positions. "
            "For Quantize: default: 14, range: 1-17. "
            "For Draco: default: 14, range: 12-16. "
            "Higher values result in lower compression but higher precision. Must be an integer.");

        supportsOption("vt=<number>",
            "vt (texture coordinate quantization): Uses number-bit quantization for texture coordinates. "
            "For Quantize: default: 12, range: 1-16. "
            "For Draco: default: 12, range: 10-14. "
            "Higher values result in lower compression but higher precision. Must be an integer.");

        supportsOption("vn=<number>",
            "vn (normal and tangent quantization): Uses number-bit quantization for normals and tangents. "
            "For Quantize: default: 8, range: 1-16. "
            "For Draco: default: 10, range: 8-12. "
            "Higher values result in lower compression but higher precision. Must be an integer.");

        supportsOption("vc=<number>",
            "vc (color quantization): Uses number-bit quantization for colors. "
            "For Quantize: default: 8, range: 1-16. "
            "For Draco: default: 8, range: 8-10. "
            "Higher values result in lower compression but higher precision. Must be an integer.");

        supportsOption("vg=<number>",
            "vg (generic quantization): Uses number-bit quantization for generic vertex attributes. "
            "Effective only when ct is draco. Default: 16, range: 14-18. "
            "Higher values result in lower compression but higher precision. Must be an integer.");

    }

    const char* className() const override { return "GLTF reader/writer"; }

    ReadResult readObject(const std::string& filename, const Options* options) const override
    {
        return readNode(filename, options);
    }

    virtual WriteResult writeObject(const osg::Node& node, const std::string& filename, const Options* options) const
    {
        return writeNode(node, filename, options);
    }

    ReadResult readNode(const std::string& filename, const Options*) const override;
    WriteResult writeNode(const osg::Node&, const std::string& filename, const Options*) const override;

    std::string createFeatureTableJSON(const osg::Vec3& center,  unsigned short batchLength) const;

    std::string createBatchTableJSON(BatchTableHierarchyVisitor& batchTableHierarchyVisitor) const;

    WriteResult writeB3DMFile(const std::string& filename, const B3DMFile& b3dmFile) const;
};
#endif // !READERWRITERGLTF_H

template<class T>
inline void ReaderWriterGLTF::putVal(std::vector<unsigned char>& buffer, T val) const
{
    unsigned char const* p = reinterpret_cast<unsigned char const*>(&val);
    buffer.insert(buffer.end(), p, p + sizeof(T));
}
