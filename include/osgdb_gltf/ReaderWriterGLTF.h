#ifndef READERWRITERGLTF_H
#define READERWRITERGLTF_H 1
#include <osgDB/ReaderWriter>
#include "osgdb_gltf/Osg2Gltf.h"
class ReaderWriterGLTF:public osgDB::ReaderWriter
{
public:
    ReaderWriterGLTF() {
        supportsExtension("gltf", "gltf format");
        supportsExtension("glb", "glb format");
        supportsOption("embedImages", "");
        supportsOption("embedBuffers", "");
        supportsOption("prettyPrint", "");
        supportsOption("textureType=<value>", "value enum values:jpg、png、webp、ktx、ktx2，default value is png");
        supportsOption("compressionType=<value>", "value enum values:draco、meshopt,default is none");
        supportsOption("comporessLevel=<string>", "default is medium,value enum values:low、medium、high");
        //supportsOption("batchId", "Enable batchid attribute");

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
