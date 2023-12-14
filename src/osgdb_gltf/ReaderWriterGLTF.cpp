#include <osgdb_gltf/ReaderWriterGLTF.h>
#include <osg/MatrixTransform>
#include <utils/TextureOptimizier.h>
#include <osgDB/FileNameUtils>
osgDB::ReaderWriter::ReadResult ReaderWriterGLTF::readNode(const std::string& filenameInit,
    const Options* options) const {

    std::cout << "Error:this gltf plugins can't read gltf to osg,because it is for processing data rather than displaying it in osg engine!" << std::endl;
    return ReadResult::ERROR_IN_READING_FILE;
}
osgDB::ReaderWriter::WriteResult ReaderWriterGLTF::writeNode(
    const osg::Node& node,
    const std::string& filename,
    const Options* options) const {
    std::string ext = osgDB::getLowerCaseFileExtension(filename);
    if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

    bool embedImages = false, embedBuffers = false, prettyPrint = false, isBinary = ext == "glb";
    TextureType textureType = TextureType::PNG;
    CompressionType comporessionType = CompressionType::NONE;
    int comporessLevel = 1;
    if (options)
    {
	    std::string compressionTypeStr;
	    std::string textureTypeStr;
	    std::istringstream iss(options->getOptionString());
        std::string opt;
        while (iss >> opt)
        {
            if (opt == "embedImages")
            {
                embedImages = true;
            }
            else if (opt == "embedBuffers")
            {
                embedBuffers = true;
            }
            else if (opt == "prettyPrint")
            {
                prettyPrint = true;
            }

            // split opt into pre= and post=
            std::string key;
            std::string val;

            size_t found = opt.find("=");
            if (found != std::string::npos)
            {
                key = opt.substr(0, found);
                val = opt.substr(found + 1);
            }
            else
            {
                key = opt;
            }
            if (key == "textureType")
            {             
                textureTypeStr = osgDB::convertToLowerCase(val);
                if (textureTypeStr == "ktx") {
                    textureType = TextureType::KTX;
                }else if (textureTypeStr == "ktx2") {
                    textureType = TextureType::KTX2;
                }else if (textureTypeStr == "jpg") {
                    textureType = TextureType::JPG;
                }else if (textureTypeStr == "webp") {
                    textureType = TextureType::WEBP;
                }
            }
            else if (key == "compressionType")
            {
                compressionTypeStr = osgDB::convertToLowerCase(val);
                if (compressionTypeStr == "draco") {
                    comporessionType = CompressionType::DRACO;
                }
                else if (compressionTypeStr == "meshopt") {
                    comporessionType = CompressionType::MESHOPT;
                }
            }
            else if (key == "comporessLevel") {
                if (val == "low") {
                    comporessLevel = 0;
                }
                else if (val == "high") {
                    comporessLevel = 2;
                }
                else {
                    comporessLevel = 1;
                }
            }
        }
    }

    OsgToGltf osg2gltf(textureType, comporessionType, comporessLevel);
    osg::Node& nc_node = const_cast<osg::Node&>(node); // won't change it, promise :)
    osg::ref_ptr<osg::Node> copyNode = osg::clone(nc_node.asNode(), osg::CopyOp::DEEP_COPY_ALL);//(osg::Node*)(nc_node.clone(osg::CopyOp::DEEP_COPY_ALL));
    // GLTF uses a +X=right +y=up -z=forward coordinate system,but if using osg to process external data does not require this
    //osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform;
    //transform->setMatrix(osg::Matrixd::rotate(osg::Vec3d(0.0, 0.0, 1.0), osg::Vec3d(0.0, 1.0, 0.0)));
    //transform->addChild(&nc_node);;
    //transform->accept(osg2gltf);
    //transform->removeChild(&nc_node);
    TextureOptimizerProxy* to = new TextureOptimizerProxy(copyNode, textureType,2048);
    delete to;
    GeometryNodeVisitor gnv;
    copyNode->accept(gnv);
    TransformNodeVisitor tnv;
    copyNode->accept(tnv);
    copyNode->accept(osg2gltf);//if using osg to process external data
    tinygltf::Model gltfModel = osg2gltf.getGltf();
    copyNode.release();
    tinygltf::TinyGLTF writer;
    bool isSuccess = writer.WriteGltfSceneToFile(
        &gltfModel,
        filename,
        embedImages,           // embedImages
        embedBuffers,           // embedBuffers
        prettyPrint,           // prettyPrint
        true);
    return isSuccess ? WriteResult::FILE_SAVED : WriteResult::ERROR_IN_WRITING_FILE;
}
REGISTER_OSGPLUGIN(gltf, ReaderWriterGLTF)
