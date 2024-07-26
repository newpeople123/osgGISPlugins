#include <osgDB/Options>
#include <osgDB/Registry>
#include <osgdb_gltf/ReaderWriterGLTF.h>
#include <osgDB/FileNameUtils>
#include "osgdb_gltf/Osg2Gltf.h"
#include "osgdb_gltf/compress/GltfDracoCompressor.h"
#include "osgdb_gltf/compress/GltfMeshOptCompressor.h"
#include "osgdb_gltf/compress/GltfMeshQuantizeCompressor.h"
#include "utils/FlattenTransformVisitor.h"

osgDB::ReaderWriter::ReadResult ReaderWriterGLTF::readNode(const std::string& filenameInit,
    const Options* options) const {

    osg::notify(osg::FATAL) << "Error: this gltf plugin can't read gltf to osg, because it is for processing data rather than displaying it in osg engine!" << std::endl;
    return ReadResult::ERROR_IN_READING_FILE;
}

osgDB::ReaderWriter::WriteResult ReaderWriterGLTF::writeNode(
    const osg::Node& node,
    const std::string& filename,
    const Options* options) const {
    std::string ext = osgDB::getLowerCaseFileExtension(filename);
    if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

    osg::Node& nc_node = const_cast<osg::Node&>(node); // won't change it, promise :)
    osg::ref_ptr<osg::Node> copyNode = osg::clone(nc_node.asNode(), osg::CopyOp::DEEP_COPY_ALL);

    Osg2Gltf osg2gltf;
    copyNode->accept(osg2gltf);
    tinygltf::Model gltfModel = osg2gltf.getGltfModel();

    const bool embedImages = true;
    bool embedBuffers = false, prettyPrint = false, isBinary = ext == "glb";
    bool quantize = false;
    GltfDracoCompressor::DracoCompressionOptions dracoCompressOption;
    GltfMeshQuantizeCompressor::MeshQuantizeCompressionOptions quantizeCompressOption;

    if (options) {
        std::istringstream iss(options->getOptionString());
        std::string opt;

        while (iss >> opt) {
            if (opt == "q") {
                quantize = true;
            }
            else if (opt == "eb") {
                embedBuffers = true;
            }
            else if (opt == "pp") {
                prettyPrint = true;
            }

            std::string key;
            std::string val;
            size_t found = opt.find("=");
            if (found != std::string::npos) {
                key = opt.substr(0, found);
                val = opt.substr(found + 1);
            }
            else {
                key = opt;
            }

            if (key == "vp") {
                dracoCompressOption.PositionQuantizationBits = std::atoi(val.c_str());
                quantizeCompressOption.PositionQuantizationBits = dracoCompressOption.PositionQuantizationBits;
            }
            else if (key == "vt") {
                dracoCompressOption.TexCoordQuantizationBits = std::atoi(val.c_str());
                quantizeCompressOption.TexCoordQuantizationBits = dracoCompressOption.TexCoordQuantizationBits;
            }
            else if (key == "vn") {
                dracoCompressOption.NormalQuantizationBits = std::atoi(val.c_str());
                quantizeCompressOption.NormalQuantizationBits = dracoCompressOption.NormalQuantizationBits;
            }
            else if (key == "vc") {
                dracoCompressOption.ColorQuantizationBits = std::atoi(val.c_str());
                quantizeCompressOption.ColorQuantizationBits = dracoCompressOption.ColorQuantizationBits;
            }
            else if (key == "vg") {
                dracoCompressOption.GenericQuantizationBits = std::atoi(val.c_str());
            }
        }

        iss.clear();
        iss.str(options->getOptionString());
        while (iss >> opt) {
            std::string key;
            std::string val;
            size_t found = opt.find("=");
            if (found != std::string::npos) {
                key = opt.substr(0, found);
                val = opt.substr(found + 1);
            }
            else {
                key = opt;
            }

            if (key == "ct") {
                const std::string compressionTypeStr = osgDB::convertToLowerCase(val);
                if (compressionTypeStr == "draco") {
                    if (quantize) {
                        osg::notify(osg::WARN) << "Warning: Draco compression and vector quantization cannot be enabled simultaneously. If both are enabled, only vector quantization will be performed!" << std::endl;
                        FlattenTransformVisitor ftv;
                        copyNode->accept(ftv);
                        GltfMeshQuantizeCompressor compressor(gltfModel, quantizeCompressOption);
                    }
                    else {
                        GltfDracoCompressor compressor(gltfModel, dracoCompressOption);
                    }
                }
                else if (compressionTypeStr == "meshopt") {
                    if (quantize) {
                        FlattenTransformVisitor ftv;
                        copyNode->accept(ftv);
                        GltfMeshQuantizeCompressor compressor(gltfModel, quantizeCompressOption);
                    }
                    GltfMeshOptCompressor compressor(gltfModel);
                }
            }
        }
    }

    if (ext != "b3dm") {
        try {
            tinygltf::TinyGLTF writer;
            bool isSuccess = writer.WriteGltfSceneToFile(
                &gltfModel,
                filename,
                embedImages,           // embedImages
                embedBuffers,           // embedBuffers
                prettyPrint,           // prettyPrint
                isBinary);
            copyNode = nullptr;
            return isSuccess ? WriteResult::FILE_SAVED : WriteResult::ERROR_IN_WRITING_FILE;
        }
        catch (const std::exception& e) {
            osg::notify(osg::FATAL) << "Exception caught while writing file: " << e.what() << std::endl;
            copyNode = nullptr;
            return WriteResult::ERROR_IN_WRITING_FILE;
        }
    }

    copyNode = nullptr;
    return WriteResult::ERROR_IN_WRITING_FILE;
}

REGISTER_OSGPLUGIN(gltf, ReaderWriterGLTF)
