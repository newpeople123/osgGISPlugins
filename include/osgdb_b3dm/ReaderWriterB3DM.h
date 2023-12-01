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
    }
    const char* className() const { return "b3dm reader/writer"; }
    virtual ReadResult readObject(const std::string& filename, const Options* options) const
    {
        return readNode(filename, options);
    }

    virtual WriteResult writeObject(const osg::Node& node, const std::string& filename, const Options* options) const
    {
        return writeNode(node, filename, options);
    }

    virtual ReadResult readNode(const std::string& filename, const Options*) const;
    virtual WriteResult writeNode(const osg::Node& node, const std::string& filename, const Options*) const;
    tinygltf::Model convertOsg2Gltf(osg::ref_ptr<osg::Node> node, const Options* options) const;
};
#endif // !READERWRITERB3DM_H
