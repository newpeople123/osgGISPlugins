#ifndef READERWRITERGLTF_H
#define READERWRITERGLTF_H 1
#include <osgDB/ReaderWriter>
#include "osgdb_gltf/Osg2Gltf.h"
class ReaderWriterGLTF:public osgDB::ReaderWriter
{
public:
    ReaderWriterGLTF() {
        supportsExtension("gltf", "GLTF format");
        supportsExtension("glb", "GLB format");
        supportsExtension("b3dm", "B3DM format");
        supportsOption("eb", "eb is short for embedBuffers, determines whether to embed buffers into the resulting file");
        supportsOption("pp", "pp is short for prettyPrint, determines whether the JSON in the resulting file is formatted");
        supportsOption("q", "q is short for quantize, enables vector quantization; if quantization is enabled, Draco cannot be used");
        supportsOption("ct=<string>", "ct is short for compression type, possible values are: draco, meshopt; default is none");
        supportsOption("vp=<number>", "use number-bit quantization for positions (for Quantize: default: 14; range: 1-17; "
            "for Draco: default: 14; range: 12-16; higher values result in lower compression but higher precision); the value must be an integer");
        supportsOption("vt=<number>", "use number-bit quantization for texture coordinates (for Quantize: default: 12; range: 1-16; "
            "for Draco: default: 12; range: 10-14; higher values result in lower compression but higher precision); the value must be an integer");
        supportsOption("vn=<number>", "use number-bit quantization for normals and tangents (for Quantize: default: 8; range: 1-16; "
            "for Draco: default: 10; range: 8-12; higher values result in lower compression but higher precision); the value must be an integer");
        supportsOption("vc=<number>", "use number-bit quantization for colors (for Quantize: default: 8; range: 1-16; "
            "for Draco: default: 8; range: 8-10; higher values result in lower compression but higher precision); the value must be an integer");
        supportsOption("vg=<number>", "use number-bit quantization for generics (only effective when ct is draco, "
            "default: 16; range: 14-18; higher values result in lower compression but higher precision); the value must be an integer");
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
};
#endif // !READERWRITERGLTF_H
