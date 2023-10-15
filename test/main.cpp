#include <osg/Node>
#include <osgDB/ReadFile>
#include <osgDB/Registry>
#include <osgViewer/Viewer>
#include <osgDB/WriteFile>
#include <osg/Material>
#include <osg/ImageSequence>
#include <osg/Texture2D>
#include <utils/GltfPbrMetallicRoughnessMaterial.h>
#include <osg/Math>
#include <osg/Matrix>
#include <osg/MatrixTransform>
#include <iostream>
#include <osgUtil/Tessellator>
#include <osgUtil/SmoothingVisitor>
#include <osgUtil/Optimizer>
#include <osgDB/ConvertUTF>
#include <codecvt>
using namespace std;
/*


    if (true) {

        if (pNode) {
            FbxProperty topProp = pNode->GetNextProperty(pNode->GetFirstProperty());
            FbxPropertyFlags
            FbxString ss;
            while (topProp.IsValid())
            {
                FbxDataType dataType = topProp.GetPropertyDataType();
                EFbxType eType = dataType.GetType();
                switch (eType)
                {
                case fbxsdk::eFbxUndefined:
                    break;
                case fbxsdk::eFbxChar:
                    break;
                case fbxsdk::eFbxUChar:
                    break;
                case fbxsdk::eFbxShort:
                    break;
                case fbxsdk::eFbxUShort:
                    break;
                case fbxsdk::eFbxUInt:
                    std::cout << pNode->GetName() << " propName:" << topProp.GetName() << "  " << "  " << topProp.Get<unsigned int>() << "--" << std::endl;
                    break;
                case fbxsdk::eFbxLongLong:
                    break;
                case fbxsdk::eFbxULongLong:
                    break;
                case fbxsdk::eFbxHalfFloat:
                    break;
                case fbxsdk::eFbxBool:
                    std::cout << pNode->GetName() << " propName:" << topProp.GetName() << "  " << "  " << topProp.Get<bool>() << "--" << std::endl;
                    break;
                case fbxsdk::eFbxInt:
                    std::cout << pNode->GetName() << " propName:" << topProp.GetName() << "  " << "  " << topProp.Get<int>() << "--" << std::endl;
                    break;
                case fbxsdk::eFbxFloat:
                    std::cout << pNode->GetName() << " propName:" << topProp.GetName() << "  " << "  " << topProp.Get<float>() << "--" << std::endl;
                    break;
                case fbxsdk::eFbxDouble:
                    std::cout << pNode->GetName() << " propName:" << topProp.GetName() << "  " << "  " << topProp.Get<double>() << "--" << std::endl;
                    break;
                case fbxsdk::eFbxDouble2:
                    break;
                case fbxsdk::eFbxDouble3:
                    break;
                case fbxsdk::eFbxDouble4:
                    break;
                case fbxsdk::eFbxDouble4x4:
                    break;
                case fbxsdk::eFbxEnum:
                    break;
                case fbxsdk::eFbxEnumM:
                    break;
                case fbxsdk::eFbxString:
                    std::cout << pNode->GetName() << " propName:" << topProp.GetName() << "  " << "  " << topProp.Get<std::string>() << "--" << std::endl;
                    break;
                case fbxsdk::eFbxTime:
                    break;
                case fbxsdk::eFbxReference:
                    break;
                case fbxsdk::eFbxBlob:
                    break;
                case fbxsdk::eFbxDistance:
                    break;
                case fbxsdk::eFbxDateTime:
                    break;
                case fbxsdk::eFbxTypeCount:
                    break;
                default:
                    break;
                }
                topProp = pNode->GetNextProperty(topProp);
            }
        }
    }
*/
class MyGetValueVisitor : public osg::ValueObject::GetValueVisitor
{
public:
    virtual void apply(bool value) { std::cout << " bool " << value; }
    virtual void apply(char value) { std::cout << " char " << value; }
    virtual void apply(unsigned char value) { std::cout << " uchar " << value; }
    virtual void apply(short value) { std::cout << " short " << value; }
    virtual void apply(unsigned short value) { std::cout << " ushort " << value; }
    virtual void apply(int value) { std::cout << " int " << value; }
    virtual void apply(unsigned int value) { std::cout << " uint " << value; }
    virtual void apply(float value) { std::cout << " float " << value; }
    virtual void apply(double value) { std::cout << " double " << value; }
    virtual void apply(const std::string& value) { std::cout << " string " << value; }
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
};
void TraverseMaterials(osg::Node* node) {
    if (!node) {
        return;
    }

    osg::Group* groupNode = dynamic_cast<osg::Group*>(node);
    if (groupNode) {
        for (unsigned int i = 0; i < groupNode->getNumChildren(); ++i) {
            TraverseMaterials(groupNode->getChild(i));
        }
    }

    osg::Geode* geode = node->asGeode();
    if (geode) {
        for (unsigned int i = 0; i < geode->getNumDrawables(); ++i) {
            osg::Drawable* drawable = geode->getDrawable(i);
            osg::Geometry* geometry = dynamic_cast<osg::Geometry*>(drawable);
            if (geometry) {
                geometry->setColorArray(NULL);
                osg::StateSet* stateSet = geometry->getStateSet();
                if (stateSet) {
                    osg::Texture* osgTexture1 = dynamic_cast<osg::Texture*>(stateSet->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
                    osg::Texture* osgTexture2 = dynamic_cast<osg::Texture*>(stateSet->getTextureAttribute(1, osg::StateAttribute::TEXTURE));

                    osg::Material* material = dynamic_cast<osg::Material*>(stateSet->getAttribute(osg::StateAttribute::MATERIAL));
                    if (material) {
                        osg::Vec4 ambient = material->getAmbient(osg::Material::FRONT_AND_BACK);
                        osg::Vec4 diffuse = material->getDiffuse(osg::Material::FRONT_AND_BACK);
                        osg::Vec4 specular = material->getSpecular(osg::Material::FRONT_AND_BACK);
                    }
                }
            }
        }
    }
}

osg::ref_ptr<osg::Node> tesslatorGeometry()
{
    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    osg::ref_ptr<osg::Geometry> geomTran = new osg::Geometry;
    osg::ref_ptr<osg::Geometry> geomLeft = new osg::Geometry;
    osg::ref_ptr<osg::Geometry> geomLeftTran = new osg::Geometry;
    geode->addDrawable(geom.get());
    geode->addDrawable(geomTran.get());
    geode->addDrawable(geomLeft.get());
    geode->addDrawable(geomLeftTran.get());
    const float wall[4][3] = {
        {2200.0,0.0,1130.0},
        {2600.0,0.0,1130.0},
        {2600.0,0.0,1340.0},
        {2200.0,0.0,1340.0} };

    const float wallTran[3][3] = {
        {2600.0,0.0,1340.0},
        {2400.0,200.0,1440.0},
        {2200.0,0.0,1340.0},
    };

    const float wallLeft[4][3] = {
        {2200.0,0.0,1130.0},
        {2200.0,0.0,1340.0},
        {2200.0,400.0,1340.0},
        {2200.0,400.0,1130.0}
    };

    const float wallLeftTran[3][3] = {
        {2200.0,0.0,1340.0},
        {2400.0,200.0,1440.0},
        {2200.0,400.0,1340.0}
    };

    const float door[4][3] = {
        {2360.0,0.0,1130.0},
        {2440.0,0.0,1130.0},
        {2440.0,0.0,1230.0},
        {2360.0,0.0,1230.0} };

    const float windows[16][3] = {
        {2240.0,0.0,1180.0},
        {2330.0,0.0,1180.0},
        {2330.0,0.0,1220.0},
        {2240.0,0.0,1220.0},
        {2460.0,0.0,1180.0},
        {2560.0,0.0,1180.0},
        {2560.0,0.0,1220.0},
        {2460.0,0.0,1220.0},
        {2240.0,0.0,1280.0},
        {2330.0,0.0,1280.0},
        {2330.0,0.0,1320.0},
        {2240.0,0.0,1320.0},
        {2460.0,0.0,1280.0},
        {2560.0,0.0,1280.0},
        {2560.0,0.0,1320.0},
        {2460.0,0.0,1320.0}
    };

    osg::ref_ptr<osg::Vec3Array> coords = new osg::Vec3Array();
    geom->setVertexArray(coords.get());
    osg::ref_ptr<osg::Vec3Array> coordsLeft = new osg::Vec3Array();
    geomLeft->setVertexArray(coordsLeft.get());
    osg::ref_ptr<osg::Vec3Array> coordsTran = new osg::Vec3Array();
    geomTran->setVertexArray(coordsTran.get());
    osg::ref_ptr<osg::Vec3Array> coordsLeftTran = new osg::Vec3Array();
    geomLeftTran->setVertexArray(coordsLeftTran.get());

    osg::ref_ptr<osg::Vec3Array> normal = new osg::Vec3Array();
    normal->push_back(osg::Vec3(0.0, -1.0, 0.0));
    geom->setNormalArray(normal.get());
    geom->setNormalBinding(osg::Geometry::BIND_OVERALL);

    osg::ref_ptr<osg::Vec3Array> normalTran = new osg::Vec3Array();
    normalTran->push_back(osg::Vec3(0.0, -1.0, 0.0));
    geomTran->setNormalArray(normalTran.get());
    geomTran->setNormalBinding(osg::Geometry::BIND_OVERALL);

    osg::ref_ptr<osg::Vec3Array> normalLeft = new osg::Vec3Array();
    normalLeft->push_back(osg::Vec3(-1.0, 0.0, 0.0));
    geomLeft->setNormalArray(normalLeft.get());
    geomLeft->setNormalBinding(osg::Geometry::BIND_OVERALL);

    osg::ref_ptr<osg::Vec3Array> normalLeftTran = new osg::Vec3Array();
    normalLeftTran->push_back(osg::Vec3(-1.0, 0.0, 0.0));
    geomLeftTran->setNormalArray(normalLeftTran.get());
    geomLeftTran->setNormalBinding(osg::Geometry::BIND_OVERALL);


    for (int i = 0; i < 4; i++)
    {
        coords->push_back(osg::Vec3(wall[i][0], wall[i][1], wall[i][2]));
    }
    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS, 0, 4));

    for (int i = 0; i < 3; i++)
    {
        coordsTran->push_back(osg::Vec3(wallTran[i][0], wallTran[i][1], wallTran[i][2]));
    }
    geomTran->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 3));

    for (int i = 0; i < 4; i++)
    {
        coordsLeft->push_back(osg::Vec3(wallLeft[i][0], wallLeft[i][1], wallLeft[i][2]));
    }
    geomLeft->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS, 0, 4));

    for (int i = 0; i < 3; i++)
    {
        coordsLeftTran->push_back(osg::Vec3(wallLeftTran[i][0], wallLeftTran[i][1], wallLeftTran[i][2]));
    }
    geomLeftTran->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 3));

    for (int i = 0; i < 4; i++)
    {
        coords->push_back(osg::Vec3(door[i][0], door[i][1], door[i][2]));
    }

    for (int i = 0; i < 16; i++)
    {
        coords->push_back(osg::Vec3(windows[i][0], windows[i][1], windows[i][2]));
    }
    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS, 4, 20));


    osg::ref_ptr<osgUtil::Tessellator> tscx = new osgUtil::Tessellator();

    tscx->setTessellationType(osgUtil::Tessellator::TESS_TYPE_GEOMETRY);

    tscx->setBoundaryOnly(false);

    tscx->setWindingType(osgUtil::Tessellator::TESS_WINDING_ODD);

    tscx->retessellatePolygons(*(geom.get()));

    osg::Matrix mat = osg::Matrix::rotate(osg::inDegrees(-90.0f), 1.0f, 0.0f, 0.0f);
    osg::ref_ptr<osg::MatrixTransform> matTransNode = new osg::MatrixTransform;
    matTransNode->setMatrix(mat);
    matTransNode->addChild(geode);
    return matTransNode.get();
}

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
inline std::string utf8_to_gbk(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    std::wstring tmp_wstr = conv.from_bytes(str);
    //GBK locale name in windows
    const char* GBK_LOCALE_NAME = ".936";
    std::wstring_convert<std::codecvt_byname<wchar_t, char, mbstate_t>> convert(new std::codecvt_byname<wchar_t, char, mbstate_t>(GBK_LOCALE_NAME));
    return convert.to_bytes(tmp_wstr);
}

inline std::string gbk_to_utf8(const std::string& str)
{
    //GBK locale name in windows
    const char* GBK_LOCALE_NAME = ".936";
    std::wstring_convert<std::codecvt_byname<wchar_t, char, mbstate_t>> convert(new std::codecvt_byname<wchar_t, char, mbstate_t>(GBK_LOCALE_NAME));
    std::wstring tmp_wstr = convert.from_bytes(str);
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cv2;
    return cv2.to_bytes(tmp_wstr);
}
void testOsgdb_fbx() {
    clock_t start, end;
    start = clock();
    string filename = "D:\\Data\\芜湖水厂总装1.fbx";
    osg::setNotifyLevel(osg::ALWAYS);
    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(filename);
    //TraverseMaterials(node);
    osg::ref_ptr<osgDB::Options> option = new osgDB::Options;
    //B3dmOptimizer b3dmOptimizer;
    //node->accept(b3dmOptimizer);
    //osgViewer::Viewer viewer1;
    //viewer1.setSceneData(node.get());
    //viewer1.run();
    //option->setOptionString("embedImages embedBuffers prettyPrint compressionType=none");
    //osgDB::writeNodeFile(*node.get(), "D:\\nginx-1.22.1\\html\\3dtiles\\1\\tile-optimizer.gltf", option);
    //option->setOptionString("embedImages  compressionType=meshopt");
    //osgDB::writeNodeFile(*node.get(), "D:\\nginx-1.22.1\\html\\3dtiles\\1\\tile-optimizer-meshopt.gltf", option);
    //option->setOptionString("embedImages compressionType=draco");
    //osgDB::writeNodeFile(*node.get(), "D:\\nginx-1.22.1\\html\\3dtiles\\1\\tile-optimizer-draco.gltf", option);
    
    option->setOptionString("embedImages embedBuffers prettyPrint isBinary compressionType=none");
    osgDB::writeNodeFile(*node.get(), "D:\\nginx-1.24.0\\html\\1.gltf", option);

    //osgUtil::SmoothingVisitor smoothVisitor;
    //node->accept(smoothVisitor);
    //option->setOptionString("embedImages embedBuffers prettyPrint textureType=jpg");
    //osgDB::writeNodeFile(*node.get(), "./pbr-jpg.gltf", option);
    //option->setOptionString("embedImages embedBuffers prettyPrint textureType=webp");
    //osgDB::writeNodeFile(*node.get(), "./pbr-webp.gltf", option);
    //option->setOptionString("embedImages embedBuffers prettyPrint textureType=ktx");
    //osgDB::writeNodeFile(*node.get(), "./pbr-ktx.gltf", option);
    //option->setOptionString("embedImages embedBuffers prettyPrint textureType=png");
    //osgDB::writeNodeFile(*node.get(), "./pbr-png.gltf", option);



    //osgDB::FilePathList& pluginList = osgDB::Registry::instance()->getLibraryFilePathList();

    ////pluginList.insert("E:/SDK/vs2019_64bit_osg/osg365/build/bin/osgPlugins-3.6.5")
    //std::cout << "osgDB plugin search paths:" << std::endl;
    //for (const auto& path : pluginList)
    //{
    //    std::cout << path << std::endl;
    //}

    end = clock();
    std::cout << "time = " << double(end - start) / CLOCKS_PER_SEC << "s" << std::endl;
}
void testOsgdb_webp() {
    osg::ref_ptr<osg::Image> Tank_Roughness = osgDB::readImageFile("C://Users//ecidi-cve//Downloads//31-gas-tank//pbr.fbm//Gas Tank_Roughness.png");
    osg::ref_ptr<osg::Image> Tank_Metallic = osgDB::readImageFile("C://Users//ecidi-cve//Downloads//31-gas-tank//pbr.fbm//Gas Tank_Metallic.png");
    GltfPbrMetallicRoughnessMaterial* pbr = new GltfPbrMetallicRoughnessMaterial;
    osg::ref_ptr<osg::Image> img = pbr->mergeImagesToMetallicRoughnessImage(Tank_Metallic, Tank_Roughness);

    osg::ref_ptr<osg::Image> img1 = osgDB::readImageFile("C://Users//ecidi-cve//Downloads//31-gas-tank//pbr.fbm//Gas Tank_Metallic.png");


    unsigned char* data = img1->data();
    std::string utf8Str = reinterpret_cast<char*>(data); // 将数据从当前代码页转换为UTF-8编码的std::string对象
    const unsigned char* utf8Data = reinterpret_cast<const unsigned char*>(utf8Str.c_str()); // 将std::string对象转换为unsigned char*指针
    unsigned int dataSize = utf8Str.size();

    osgDB::writeImageFile(*img1.get(), "./osgdb_webp_test.webp");
}
void testOsgdb_ktx() {
    osg::ref_ptr<osg::Image> img = osgDB::readImageFile("C://Users//ecidi-cve//Downloads//31-gas-tank//pbr.fbm//1.png");
    osgDB::writeImageFile(*img.get(), "./osgdb_ktx_test.ktx");
}

void preview_img() {
    osg::ref_ptr<osg::Image> image = osgDB::readImageFile("E://Code//2022//GIS//C++//anqing-data//output//FAC_jianchajing01.jpg");
    
    if (image) {
        unsigned char bu1 = image->data()[0];
        unsigned char bu2 = image->data()[1];
        unsigned char bu3 = image->data()[2];
        unsigned char bu4 = image->data()[1222];
        unsigned char bu5 = image->data()[14222];
        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        vertices->push_back(osg::Vec3(-1.0f, -1.0f, 0.0f));
        vertices->push_back(osg::Vec3(1.0f, -1.0f, 0.0f));
        vertices->push_back(osg::Vec3(1.0f, 1.0f, 0.0f));
        vertices->push_back(osg::Vec3(-1.0f, 1.0f, 0.0f));
        geometry->setVertexArray(vertices);
        osg::ref_ptr<osg::Vec2Array> texcoords = new osg::Vec2Array;
        texcoords->push_back(osg::Vec2(0.0f, 0.0f));
        texcoords->push_back(osg::Vec2(1.0f, 0.0f));
        texcoords->push_back(osg::Vec2(1.0f, 1.0f));
        texcoords->push_back(osg::Vec2(0.0f, 1.0f));
        geometry->setTexCoordArray(0, texcoords);
        geometry->addPrimitiveSet(new osg::DrawArrays(GL_QUADS, 0, 4));
        osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D(image);
        osg::ref_ptr<osg::StateSet> stateset = geometry->getOrCreateStateSet();
        stateset->setTextureAttributeAndModes(0, texture);
        osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        geode->addDrawable(geometry);
        osg::ref_ptr<osg::Group> root = new osg::Group;
        root->addChild(geode);
        osgViewer::Viewer viewer;
        viewer.setSceneData(root);
        viewer.run();
    }
    else {
        std::cerr << "Error:image is null!" << std::endl;
    }
}
int main() {
    //testOsgdb_webp();
    testOsgdb_fbx();
    //preview_img();
    //osg::ref_ptr<osg::Image> img = osgDB::readImageFile("C:\\Users\\ecidi-cve\\Desktop\\1.jpg");
    //img->scaleImage(800, 600, 1);
    //osgDB::writeImageFile(*img.get(),"C:\\Users\\ecidi-cve\\Desktop\\2.jpg");
    return 1;

}
