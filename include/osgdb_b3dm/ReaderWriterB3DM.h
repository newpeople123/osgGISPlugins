#ifndef READERWRITERB3DM_H
#define READERWRITERB3DM_H
#include <osgDB/Registry>
#include <osgDB/ReaderWriter>
#include <utils/OsgToGltf.h>
class ReaderWriterB3DM :public osgDB::ReaderWriter {
public:
    ReaderWriterB3DM() {
        supportsExtension("b3dm", "gltf format");
        supportsOption("textureType=<value>", "value enum values:jpg、png、webp、ktx、ktx2,default value is png");
        supportsOption("compressionType=<value>", "value enum values:draco、meshopt,default is none");
        supportsOption("batchId=<true/false>", "default is true");
        supportsOption("optimizer=<true/false>", "default is true");
        supportsOption("textureMaxSize=<number>", "default is 4096.0");
        supportsOption("comporessLevel=<string>", "default is medium,value enum values:low、medium、high");
        supportsOption("ratio=<number>", "default is 0.5");
    }
    const char* className() const override { return "b3dm reader/writer"; }

    ReadResult readObject(const std::string& filename, const Options* options) const override
    {
        return readNode(filename, options);
    }

    virtual WriteResult writeObject(const osg::Node& node, const std::string& filename, const Options* options) const
    {
        return writeNode(node, filename, options);
    }

    ReadResult readNode(const std::string& filename, const Options*) const override;
    WriteResult writeNode(const osg::Node& node, const std::string& filename, const Options*) const override;
    tinygltf::Model convertOsg2Gltf(osg::ref_ptr<osg::Node> node, const Options* options) const;
};
#endif // !READERWRITERB3DM_H
