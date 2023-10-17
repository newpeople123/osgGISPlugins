#include <osgdb_b3dm/ReaderWriterB3DM.h>
#include <osgDB/FileNameUtils>
#include <osgUtil/Optimizer>
#include <osg/MatrixTransform>
#include <osgUtil/Statistics>
#include <osgUtil/Simplifier>
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

class B3dmOptimizer :public osg::NodeVisitor {
public:
    B3dmOptimizer() :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {};
    void apply(osg::MatrixTransform& xform)
    {
        osg::ref_ptr<osg::Group> replaceNode = new osg::Group;
        for (unsigned int j = 0; j < xform.asGroup()->getNumChildren(); j++) {
            replaceNode->addChild(xform.asGroup()->getChild(j));
        }
        apply(static_cast<osg::Group&>(*replaceNode.get()));
        xform.asGroup()->removeChildren(0, xform.asGroup()->getNumChildren());
        for (unsigned int j = 0; j < replaceNode->getNumChildren(); j++) {
            xform.asGroup()->addChild(replaceNode->getChild(j));
        }
    }
    void apply(osg::Group& group) {
        _optimizer.optimize(group.asGroup(), osgUtil::Optimizer::SHARE_DUPLICATE_STATE);
        _optimizer.optimize(group.asGroup(), osgUtil::Optimizer::MERGE_GEODES);
        _optimizer.optimize(group.asGroup(), osgUtil::Optimizer::INDEX_MESH);
        _optimizer.optimize(group.asGroup(), osgUtil::Optimizer::SHARE_DUPLICATE_STATE);
        osgUtil::Optimizer::MergeGeometryVisitor mgv;
        mgv.setTargetMaximumNumberOfVertices(10000000);//100w
        group.accept(mgv);
        for (unsigned int i = 0; i < group.getNumChildren(); i++) {
            osg::ref_ptr<osg::Node> child = group.getChild(i);
            if (typeid(*child.get()) == typeid(osg::MatrixTransform)) {
                osg::ref_ptr<osg::Group> replaceChild = new osg::Group;
                for (unsigned int j = 0; j < child->asGroup()->getNumChildren(); j++) {
                    replaceChild->addChild(child->asGroup()->getChild(j));
                }
                apply(static_cast<osg::Group&>(*replaceChild.get()));
                child->asGroup()->removeChildren(0, child->asGroup()->getNumChildren());
                for (unsigned int j = 0; j < replaceChild->getNumChildren(); j++) {
                    child->asGroup()->addChild(replaceChild->getChild(j));
                }
            }
            else if (typeid(*child.get()) == typeid(osg::Group)) {
                apply(static_cast<osg::Group&>(*child.get()));
            }
        }
    }
private:
    osgUtil::Optimizer _optimizer;
};

class StateSetOptimizerVisitor : public osg::NodeVisitor {
public:
    StateSetOptimizerVisitor() : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {}

    void apply(osg::Drawable& node) override {
        osg::ref_ptr<osg::StateSet> stateSet = node.getStateSet();
        bool isNewStateSet = true;
        if (stateSet) {
            test.push_back(stateSet);

            for (auto& item : _geometryStateSets) {
                if (areStateSetsEqual(item.first, stateSet)) {
                    std::vector<osg::ref_ptr<osg::Drawable>>& s = item.second;
                    s.push_back(node.asGeometry());
                    isNewStateSet = false;
                    break;
                }
            }
            if (isNewStateSet) {
                std::vector<osg::ref_ptr<osg::Drawable>> nodes;
                nodes.push_back(node.asGeometry());
                _geometryStateSets.insert(std::make_pair(stateSet, nodes));
            }
        }
        traverse(node);
    }
    //void apply(osg::Geode& geode) override {
    //    osg::ref_ptr<osg::StateSet> stateSet = geode.getStateSet();
    //    bool isNewStateSet = true;
    //    if (stateSet) {
    //        for (auto& item : _geodeStateSets) {
    //            if (areStateSetsEqual(item.first, stateSet)) {
    //                std::vector<osg::ref_ptr<osg::Geode>>& s = item.second;
    //                s.push_back(geode.asGeode());
    //                isNewStateSet = false;
    //                break;
    //            }
    //        }
    //        if (isNewStateSet) {
    //            std::vector<osg::ref_ptr<osg::Geode>> nodes;
    //            nodes.push_back(geode.asGeode());
    //            _geodeStateSets.insert(std::make_pair(stateSet, nodes));
    //        }
    //    }
    //    traverse(geode);
    //}
    void sharedDuplicateState() {
        for (auto item1 : _geometryStateSets) {
            osg::StateSet* state = item1.first.get();
            for (auto& item2 : item1.second) {
                item2->setStateSet(state);
            }
        }
        for (auto item1 : _geodeStateSets) {
            osg::StateSet* state = item1.first.get();
            for (auto& item2 : item1.second) {
                item2->setStateSet(state);
            }
        }
    }

public:
    std::map<osg::ref_ptr<osg::StateSet>, std::vector<osg::ref_ptr<osg::Drawable>>> _geometryStateSets;
    std::map<osg::ref_ptr<osg::StateSet>, std::vector<osg::ref_ptr<osg::Geode>>> _geodeStateSets;
    std::vector<osg::ref_ptr<osg::StateSet>> test;

    bool areModeListEqual(const osg::StateSet::ModeList& modeList1, const osg::StateSet::ModeList& modeList2) {
        if (modeList1.size() != modeList2.size()) {
            return false;
        }

        for (const auto& mode : modeList1) {
            osg::StateAttribute::GLModeValue value1 = mode.second;
            osg::StateAttribute::GLModeValue value2 = modeList2.at(mode.first);

            if (value1 != value2) {
                return false;
            }
        }

        return true;
    }
    bool areImageEqual(const osg::Image* img1, const osg::Image* img2) {
        if (!img1 || !img2) {
            return false;
        }
        if (img1->getFileName() != img2->getFileName()) {
            return false;
        }
        if (img1->getTotalDataSize() != img2->getTotalDataSize()) {
            return false;
        }
        return true;
    }
    bool areTextureEqual(const osg::Texture* texure1, const osg::Texture* texure2) {
        if (!texure1 || !texure2) {
            return false;
        }
        if (texure1->getNumImages() != texure2->getNumImages()) {
            return false;
        }
        if (texure1->getFilter(osg::Texture::MIN_FILTER)!= texure2->getFilter(osg::Texture::MIN_FILTER)) {
            return false;
        }
        if (texure1->getFilter(osg::Texture::MAG_FILTER) != texure2->getFilter(osg::Texture::MAG_FILTER)) {
            return false;
        }
        if (texure1->getWrap(osg::Texture::WRAP_S) != texure2->getWrap(osg::Texture::WRAP_S)) {
            return false;
        }
        if (texure1->getWrap(osg::Texture::WRAP_T) != texure2->getWrap(osg::Texture::WRAP_T)) {
            return false;
        }
        if (texure1->getWrap(osg::Texture::WRAP_R) != texure2->getWrap(osg::Texture::WRAP_R)) {
            return false;
        }
        for (unsigned int i = 0; i < texure1->getNumImages(); i++) {
            const osg::Image* img1 = texure1->getImage(i);
            const osg::Image* img2 = texure2->getImage(i);
            if (areImageEqual(img1, img2) == false) {
                return false;
            }
        }
        return true;
    }
    bool areAttributeListEqual(const osg::StateSet::AttributeList& attrList1, const osg::StateSet::AttributeList& attrList2) {
        if (attrList1.size() != attrList2.size()) {
            return false;
        }

        for (const auto& attributePair : attrList1) {
            const osg::StateAttribute* attr1 = attributePair.second.first.get();
            const osg::StateAttribute* attr2 = attrList2.at(attributePair.first).first.get();
            if (!attr1 || !attr2) {
                return false;
            }
            if (attr1->getType() != attr2->getType()) {
                return false;
            }
            if (attr1->getType() == osg::StateAttribute::TEXTURE)
            {
                const osg::Texture* texure1 = attr1->asTexture();
                const osg::Texture* texure2 = attr2->asTexture();
                return areTextureEqual(texure1, texure2);
            }

        }

        return true;
    }
    bool areStateSetsEqual(osg::StateSet* stateSet1, osg::StateSet* stateSet2) {
        if (!stateSet1 || !stateSet2) {
            return false;
        }

        if (areModeListEqual(stateSet1->getModeList(), stateSet2->getModeList()) == false) {
            return false;
        }

        if (areAttributeListEqual(stateSet1->getAttributeList(), stateSet2->getAttributeList()) == false) {
            return false;
        }
        if (stateSet1->getNumTextureAttributeLists() != stateSet2->getNumTextureAttributeLists()) {
            return false;
        }
        for (unsigned int i = 0; i < stateSet1->getNumTextureAttributeLists(); i++) {
            osg::Texture* texture1 = dynamic_cast<osg::Texture*>(stateSet1->getTextureAttribute(i, osg::StateAttribute::TEXTURE));
            osg::Texture* texture2 = dynamic_cast<osg::Texture*>(stateSet2->getTextureAttribute(i, osg::StateAttribute::TEXTURE));
            if (areTextureEqual(texture1, texture2) == false) {
                return false;
            }
        }
        // Compare other state parameters...

        return true;
    }
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
    osgUtil::Simplifier simplifier;
    simplifier.setSampleRatio(0.1);
    node->accept(simplifier);
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

    StateSetOptimizerVisitor stateSetOptimize;
    copyNode->accept(stateSetOptimize);
    stateSetOptimize.sharedDuplicateState();

    B3dmOptimizer b3dmOptimizer;
    copyNode->accept(b3dmOptimizer);

    osgUtil::StatsVisitor stats;
    if (osg::getNotifyLevel() >= osg::INFO)
    {
        stats.reset();
        copyNode->accept(stats);
        stats.totalUpStats();
        OSG_NOTICE << std::endl << "Stats after:" << std::endl;
        stats.print(osg::notify(osg::NOTICE));
    }
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
    std::stringstream compressionInput;
    std::ostream* output = &fout;

    output->write(b3dm_buf.data(), b3dm_buf.size());
    output->write("\0\0\0", gltfDataPadding);
    output->write("\0\0\0", padding);
    fout.close();
    //nc_node.unref_nodelete();
    return WriteResult::FILE_SAVED;
}
REGISTER_OSGPLUGIN(b3dm, ReaderWriterB3DM)
