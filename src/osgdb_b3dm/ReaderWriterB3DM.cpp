#include <osgdb_b3dm/ReaderWriterB3DM.h>
#include <osgDB/FileNameUtils>
#include <osgUtil/Optimizer>
#include <osg/MatrixTransform>
#include <osgUtil/Statistics>
#include <utils/TextureOptimizier.h>
class UserDataVisitor : public osg::ValueObject::GetValueVisitor
{
public:
    UserDataVisitor(json& item) :_item(item) {};
    virtual void apply(bool value) { _item.push_back(value); }
    virtual void apply(char value) { _item.push_back(value); }
    virtual void apply(unsigned char value) { _item.push_back(value);  }
    virtual void apply(short value) { _item.push_back(value); }
    virtual void apply(unsigned short value) { _item.push_back(value); }
    virtual void apply(int value) { _item.push_back(value); }
    virtual void apply(unsigned int value) { _item.push_back(value); }
    virtual void apply(float value) { _item.push_back(value); }
    virtual void apply(double value) { _item.push_back(value); }
    virtual void apply(const std::string& value) { _item.push_back(value); }
    //virtual void apply(const osg::Vec2f& value) { OSG_NOTICE << " Vec2f " << value; }
    //virtual void apply(const osg::Vec3f& value) { OSG_NOTICE << " Vec3f " << value; }
    //virtual void apply(const osg::Vec4f& value) { OSG_NOTICE << " Vec4f " << value; }
    //virtual void apply(const osg::Vec2d& value) { OSG_NOTICE << " Vec2d " << value; }
    //virtual void apply(const osg::Vec3d& value) { OSG_NOTICE << " Vec3d " << value; }
    //virtual void apply(const osg::Vec4d& value) { OSG_NOTICE << " Vec4d " << value; }
    //virtual void apply(const osg::Quat& value) { OSG_NOTICE << " Quat " << value; }
    //virtual void apply(const osg::Plane& value) { OSG_NOTICE << " Plane " << value; }
    //virtual void apply(const osg::Matrixf& value) { OSG_NOTICE << " Matrixf " << value; }
    //virtual void apply(const osg::Matrixd& value) { OSG_NOTICE << " Matrixd " << value; }
private:
    json& _item;
};

class BatchIdVisitor :public osg::NodeVisitor {
public:
    BatchIdVisitor() :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {};
    virtual void apply(osg::Node& node) {
        traverse(node);
    };
    virtual void apply(osg::Group& group) {
        traverse(group);
    };
    virtual void apply(osg::Geode& geode) {
        traverse(geode);
        if (geode.getNumDrawables() > 0) {
            osg::ref_ptr<osg::UserDataContainer> udc = geode.getUserDataContainer();
            _userDatas.push_back(udc);
            _batchId++;
        }
    };
    virtual void apply(osg::Drawable& drawable) {
        osg::Geometry* geom = drawable.asGeometry();
        osg::Vec3Array* positions = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
        osg::ref_ptr<osg::FloatArray> batchIds = new osg::FloatArray;
        batchIds->assign(positions->size(), _batchId);
        osg::Array* vertexAttrib = geom->getVertexAttribArray(0);
        if (vertexAttrib) {
            OSG_WARN << "Waring: geometry's VertexAttribArray(0 channel) is not Null,it will be overwritten!" << std::endl;
        }

        geom->setVertexAttribArray(0, batchIds, osg::Array::BIND_PER_VERTEX);
    };
    float getBatchId() {
        return _batchId;
    };
    std::vector<osg::ref_ptr<osg::UserDataContainer>> getUserDatas() {
        return _userDatas;
    }
private:
    float _batchId = 0;
    std::vector<osg::ref_ptr<osg::UserDataContainer>> _userDatas;
};
template<class T>
void put_val(std::vector<unsigned char>& buf, T val) {
    buf.insert(buf.end(), (unsigned char*)&val, (unsigned char*)&val + sizeof(T));
}

template<class T>
void put_val(std::string& buf, T val) {
    buf.append((unsigned char*)&val, (unsigned char*)&val + sizeof(T));
}

tinygltf::Model ReaderWriterB3DM::convertOsg2Gltf(osg::ref_ptr<osg::Node> node, const Options* options) const {

    bool embedImages = true, embedBuffers = true, prettyPrint = false, isBinary = true;
    std::string textureTypeStr, compressionTypeStr;
    TextureType textureType = TextureType::PNG;
    CompressionType comporessionType = CompressionType::NONE;
    int comporessLevel = 1;
    if (options)
    {
        std::istringstream iss(options->getOptionString());
        std::string opt;
        while (iss >> opt)
        {
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
                }
                else if (textureTypeStr == "ktx2") {
                    textureType = TextureType::KTX2;
                }
                else if (textureTypeStr == "jpg") {
                    textureType = TextureType::JPG;
                }
                else if (textureTypeStr == "webp") {
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

    //1
    TextureOptimizer* to = new TextureOptimizer(node, textureType);
    delete to;
    //2
    GeometryNodeVisitor gnv;
    node->accept(gnv);
    //3
    TransformNodeVisitor tnv;
    node->accept(tnv);
    osgUtil::StatsVisitor stats;
    if (osg::getNotifyLevel() >= osg::INFO)
    {
        stats.reset();
        node->accept(stats);
        stats.totalUpStats();
        OSG_NOTICE << std::endl << "Stats after:" << std::endl;
        stats.print(osg::notify(osg::NOTICE));
    }

    OsgToGltf osg2gltf(textureType, comporessionType, comporessLevel);


    // GLTF uses a +X=right +y=up -z=forward coordinate system,but if using osg to process external data does not require this
    //osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform;
    //transform->setMatrix(osg::Matrixd::rotate(osg::Vec3d(0.0, 0.0, 1.0), osg::Vec3d(0.0, 1.0, 0.0)));
    //transform->addChild(&nc_node);;
    //transform->accept(osg2gltf);
    //transform->removeChild(&nc_node);

    node->accept(osg2gltf);//if using osg to process external data
    tinygltf::Model gltfModel = osg2gltf.getGltf();
    tinygltf::TinyGLTF writer;

    return gltfModel;
}
osgDB::ReaderWriter::ReadResult ReaderWriterB3DM::readNode(const std::string& filenameInit,
    const Options* options) const {
    OSG_WARN << "Error:this b3dm plugins can't read b3dm to osg,because it is for processing data rather than displaying it in osg engine!" << std::endl;
    return ReadResult::ERROR_IN_READING_FILE;
}
osgDB::ReaderWriter::WriteResult ReaderWriterB3DM::writeNode(
    const osg::Node& node,
    const std::string& filename,
    const Options* options) const {
    std::string ext = osgDB::getLowerCaseFileExtension(filename);
    if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;
    osg::Node& nc_node = const_cast<osg::Node&>(node); // won't change it, promise :)
    nc_node.ref();
    osg::ref_ptr<osg::Node> copyNode = osg::clone(nc_node.asNode(), osg::CopyOp::DEEP_COPY_ALL);//(osg::Node*)(nc_node.clone(osg::CopyOp::DEEP_COPY_ALL));
    nc_node.unref_nodelete();
    BatchIdVisitor batchidVisitor;
    copyNode->accept(batchidVisitor);

    tinygltf::Model& gltfModel = convertOsg2Gltf(copyNode, options);
    copyNode.release();
    tinygltf::TinyGLTF writer;
    std::ostringstream gltfBuf;
    writer.WriteGltfSceneToStream(&gltfModel, gltfBuf, true, true);

    std::string glb_buf = gltfBuf.str();
    int gltfDataPadding = 4 - (glb_buf.length() % 4);
    if (gltfDataPadding == 4) gltfDataPadding = 0;

    std::string b3dm_buf;
    const unsigned int batchIdCount = batchidVisitor.getBatchId();
    std::string feature_json_string;
    feature_json_string += "{\"BATCH_LENGTH\":";
    feature_json_string += std::to_string(batchIdCount);
    feature_json_string += "}";
    while ((feature_json_string.size() + 28) % 8 != 0) {
        feature_json_string.push_back(' ');
    }
    json batch_json;
    batch_json["extensions"] = json::object();
    batch_json["extensions"]["3DTILES_batch_table_hierarchy"] = json::object();
    batch_json["extensions"]["3DTILES_batch_table_hierarchy"]["classes"] = json::array();

    std::vector<osg::ref_ptr<osg::UserDataContainer>> userDatas = batchidVisitor.getUserDatas();
    std::vector<json> types;
    for (unsigned int i = 0; i < userDatas.size(); i++) {
        osg::ref_ptr<osg::UserDataContainer> udc = userDatas.at(i);
        if (udc)
        {
            json typeInstances=json::object();
            for (unsigned int j = 0; j < udc->getNumUserObjects(); ++j)
            {
                osg::Object* userObject = udc->getUserObject(j);

                std::string key = userObject->getName();
                typeInstances[key] = json::array();
            }
            if (std::find(types.begin(), types.end(), typeInstances) == types.end()) {
                types.push_back(typeInstances);
            }
        }
    }
    if (types.size()) {
        batch_json["extensions"]["3DTILES_batch_table_hierarchy"]["classIds"] = json::array();//custom
        for (unsigned int i = 0; i < batchIdCount; i++) {
            batch_json["extensions"]["3DTILES_batch_table_hierarchy"]["classIds"].push_back(-1);
        }
        unsigned int instancesLength = 0;
        for (int i = 0; i < types.size(); i++) {
            json item = json::object();
            json type = types.at(i);
            type["name"] = "type" + std::to_string(i);//You can customize according to your needs name
            std::vector<unsigned int> batchIds;
            for (unsigned int j = 0; j < userDatas.size(); j++) {
                osg::ref_ptr<osg::UserDataContainer> udc = userDatas.at(j);
                unsigned int batchId = j;
                if (udc)
                {
                    bool isEqualType = true;
                    for (unsigned int k = 0; k < udc->getNumUserObjects(); ++k)
                    {
                        osg::Object* userObject = udc->getUserObject(k);

                        std::string key = userObject->getName();

                        if (type.find(key) == type.end()) {
                            isEqualType = false;
                            break;
                        }
                    }
                    if (isEqualType) {
                        for (unsigned int k = 0; k < udc->getNumUserObjects(); ++k)
                        {
                            osg::Object* userObject = udc->getUserObject(k);

                            std::string key = userObject->getName();

                            osg::ValueObject* valueObject = dynamic_cast<osg::ValueObject*>(userObject);
                            UserDataVisitor mgv(type[key]);
                            valueObject->get(mgv);
                        }
                        batchIds.push_back(batchId);
                    }
                }
            }

            item["length"] = batchIds.size();
            item["instances"] = type;
            batch_json["extensions"]["3DTILES_batch_table_hierarchy"]["classes"].push_back(item);
            instancesLength += batchIds.size();
            for (unsigned int j = 0; j < batchIds.size(); j++) {
                batch_json["extensions"]["3DTILES_batch_table_hierarchy"]["classIds"].at(batchIds.at(j)) = i;
            }
        }
        batch_json["extensions"]["3DTILES_batch_table_hierarchy"]["instancesLength"] = instancesLength;
    }
    else {
        batch_json = json::object();
    }
    //if (attributes.size() > 0) {
    //	for (auto it : attributes) {
    //		const std::string key = it.first;
    //		const std::vector<std::string > vals = it.second;
    //		batch_json[key] = vals;
    //	}
    //}
    std::string batch_json_string = batch_json.dump();
    while (batch_json_string.size() % 8 != 0) {
        batch_json_string.push_back(' ');
    }

    int feature_json_len = feature_json_string.size();
    int feature_bin_len = 0;
    int batch_json_len = batch_json_string.size();
    int batch_bin_len = 0;
    int total_len = 28 + feature_json_len + batch_json_len + glb_buf.size() + gltfDataPadding;
    int padding = 0;
    if (total_len % 8 != 0)
    {
        padding = 8 - (total_len % 8);
    }
    total_len += padding;

    b3dm_buf += "b3dm";
    int version = 1;
    put_val(b3dm_buf, version);
    put_val(b3dm_buf, total_len);
    put_val(b3dm_buf, feature_json_len);
    put_val(b3dm_buf, feature_bin_len);
    put_val(b3dm_buf, batch_json_len);
    put_val(b3dm_buf, batch_bin_len);

    b3dm_buf.append(feature_json_string.begin(), feature_json_string.end());
    b3dm_buf.append(batch_json_string.begin(), batch_json_string.end());
    b3dm_buf.append(glb_buf);
    std::fstream fout(filename, std::ios::out | std::ios::binary);
    if (fout.is_open()) {
        std::stringstream compressionInput;
        std::ostream* output = &fout;

        output->write(b3dm_buf.data(), b3dm_buf.size());
        output->write("\0\0\0", gltfDataPadding);
        output->write("\0\0\0", padding);
        fout.close();
        //nc_node.unref_nodelete();
        return WriteResult::FILE_SAVED;
    }
    return WriteResult::ERROR_IN_WRITING_FILE;
}
REGISTER_OSGPLUGIN(b3dm, ReaderWriterB3DM)
