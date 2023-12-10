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
#include <osgGA/GUIEventAdapter>
#include <osgViewer/ViewerEventHandlers>
#include <utils/TextureAtlas.h>
#include <utils/TextureOptimizier.h>
using namespace std;

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

void preview_img(osg::ref_ptr<osg::Image>& image) {
    //osg::ref_ptr<osg::Image> image = osgDB::readImageFile("E://Code//2022//GIS//C++//anqing-data//output//FAC_jianchajing01.jpg");
    
    if (image) {
        unsigned char bu1 = image->data()[0];
        //unsigned char bu2 = image->data()[1];
        //unsigned char bu3 = image->data()[2];
        //unsigned char bu4 = image->data()[1222];
        //unsigned char bu5 = image->data()[14222];
        //osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
        //osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
        //vertices->push_back(osg::Vec3(-1.0f, -1.0f, 0.0f));
        //vertices->push_back(osg::Vec3(1.0f, -1.0f, 0.0f));
        //vertices->push_back(osg::Vec3(1.0f, 1.0f, 0.0f));
        //vertices->push_back(osg::Vec3(-1.0f, 1.0f, 0.0f));
        //geometry->setVertexArray(vertices);
        //osg::ref_ptr<osg::Vec2Array> texcoords = new osg::Vec2Array;
        //texcoords->push_back(osg::Vec2(0.0f, 0.0f));
        //texcoords->push_back(osg::Vec2(1.0f, 0.0f));
        //texcoords->push_back(osg::Vec2(1.0f, 1.0f));
        //texcoords->push_back(osg::Vec2(0.0f, 1.0f));
        //geometry->setTexCoordArray(0, texcoords);
        //geometry->addPrimitiveSet(new osg::DrawArrays(GL_QUADS, 0, 4));
        //osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D(image);
        //osg::ref_ptr<osg::StateSet> stateset = geometry->getOrCreateStateSet();
        //stateset->setTextureAttributeAndModes(0, texture);
        //osg::ref_ptr<osg::Geode> geode = new osg::Geode;
        //geode->addDrawable(geometry);
        //osg::ref_ptr<osg::Group> root = new osg::Group;
        //root->addChild(geode);
        //osgViewer::Viewer viewer;
        //viewer.setSceneData(root);
        //viewer.run();
    }
    else {
        std::cerr << "Error:image is null!" << std::endl;
    }
    image.release();
}
class TextureVisitor2 :public osg::NodeVisitor
{
public:
    TextureVisitor2() :osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {

    }
    void apply(osg::Texture& texture) {
        for (unsigned int i = 0; i < texture.getNumImages(); i++) {
            osg::ref_ptr<osg::Image> img = texture.getImage(i);
            std::cout << img->getOrigin() << std::endl;
        }
    }
    void apply(osg::StateSet& stateset) {
        for (unsigned int i = 0; i < stateset.getTextureAttributeList().size(); ++i)
        {
            osg::StateAttribute* sa = stateset.getTextureAttribute(i, osg::StateAttribute::TEXTURE);
            osg::Texture* texture = dynamic_cast<osg::Texture*>(sa);
            if (texture)
            {
                apply(*texture);
            }
        }
    }
    void apply(osg::Node& node) {
        osg::StateSet* ss = node.getStateSet();
        osg::ref_ptr<osg::Geometry> geom = node.asGeometry();
        if (ss) {
            apply(*ss);
        }
        traverse(node);
    }

};
void testTextureAtlas() {
    osg::ref_ptr<osg::Node> node = osgDB::readRefNodeFile("E:\\Code\\2023\\Other\\data\\2.FBX");
    node = nullptr;
    osg::Node* node1 = node.get();
    node1 = NULL;
    TextureOptimizer* to = new TextureOptimizer(node, JPG,4096);
    delete to;
    //osg::ref_ptr<osgDB::Options> option = new osgDB::Options;
    //option->setOptionString("embedImages embedBuffers prettyPrint isBinary compressionType=none textureType=jpg");
    //osgDB::writeNodeFile(*node.get(), "D:\\nginx-1.22.1\\html\\2.gltf", option);
    TextureVisitor* tv2 = new TextureVisitor;
    node->accept(*tv2);
    delete tv2;
    int count = node->referenceCount();
    for (int i = 0; i < count; ++i)
        node.release();
    node = nullptr;
    osgViewer::Viewer viewer1;
    viewer1.setUpViewInWindow(100, 100, 800, 600); // (x, y, width, height)
    viewer1.setSceneData(node.get());
    viewer1.addEventHandler(new osgViewer::WindowSizeHandler);//全屏  快捷键f
    viewer1.addEventHandler(new osgViewer::StatsHandler);//查看帧数 s
    viewer1.addEventHandler(new osgViewer::ScreenCaptureHandler);//截图  快捷键 c
    viewer1.run();

    const std::string basePath = "E:\\Code\\2023\\Other\\data\\芜湖水厂总装.fbm\\";
    osg::ref_ptr<osg::Image> img1 = osgDB::readImageFile(basePath + "440305A020GHJZA010.png");
    osg::ref_ptr<osg::Image> img2 = osgDB::readImageFile(basePath + "ditieshuniu05.png");
    osg::ref_ptr<osg::Image> img3 = osgDB::readImageFile(basePath + "f225c9bf0b1e3bae5076ad4b25fbdf9.png");
    osg::ref_ptr<osg::Image> img4 = osgDB::readImageFile(basePath + "440305A003GHJZA001T008.png");
    osg::ref_ptr<osg::Image> img5 = osgDB::readImageFile(basePath + "栏杆.png");
    osg::ref_ptr<osg::Image> img6 = osgDB::readImageFile(basePath + "栏杆2.png");

    //osg::ref_ptr<osg::Image> img5 = osgDB::readImageFile(basePath + "concrete03.jpg");
    //osg::ref_ptr<osg::Image> img6 = osgDB::readImageFile(basePath + "dt002.jpg");


    TextureAtlas* textureAtlas = new TextureAtlas(TextureAtlasOptions(osg::Vec2(128.0,128.0), osg::Vec2(4096, 4096), GL_RGBA, 1));
    int a1 = textureAtlas->addImage(img1);
    int a2 = textureAtlas->addImage(img2);
    int a3 = textureAtlas->addImage(img3);
    int a4 = textureAtlas->addImage(img4);
    int a5 = textureAtlas->addImage(img5);
    int a6 = textureAtlas->addImage(img6);
    //preview_img(textureAtlas->_texture.get());
    //osgDB::writeImageFile(*textureAtlas->_texture.get(), "E:\\Code\\2023\\Other\\data\\test.jpg");
    std::cout << std::endl;
}
void createCube(osg::Vec3 offset,double scale,osg::Vec4 color,std::string filename) {
    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();

    // 创建顶点数组
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
    vertices->push_back(offset + osg::Vec3(-0.5, -0.5, -0.5) * scale);
    vertices->push_back(offset + osg::Vec3(0.5, -0.5, -0.5) * scale);
    vertices->push_back(offset + osg::Vec3(0.5, 0.5, -0.5) * scale);
    vertices->push_back(offset + osg::Vec3(-0.5, 0.5, -0.5) * scale);
    vertices->push_back(offset + osg::Vec3(-0.5, -0.5, 0.5) * scale);
    vertices->push_back(offset + osg::Vec3(0.5, -0.5, 0.5) * scale);
    vertices->push_back(offset + osg::Vec3(0.5, 0.5, 0.5) * scale);
    vertices->push_back(offset + osg::Vec3(-0.5, 0.5, 0.5) * scale);

    // 设置顶点数组
    geometry->setVertexArray(vertices);

    // 创建颜色数组
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;
    colors->push_back(osg::Vec4(color)); // 红色
    colors->push_back(osg::Vec4(color)); // 绿色
    colors->push_back(osg::Vec4(color)); // 蓝色
    colors->push_back(osg::Vec4(color)); // 黄色
    colors->push_back(osg::Vec4(color)); // 洋红色
    colors->push_back(osg::Vec4(color)); // 青色
    colors->push_back(osg::Vec4(color)); // 白色
    colors->push_back(osg::Vec4(color)); // 灰色

    // 设置颜色数组
    geometry->setColorArray(colors, osg::Array::BIND_PER_VERTEX);
    // 创建四边形图元
    osg::ref_ptr<osg::DrawElementsUInt> quad =
        new osg::DrawElementsUInt(osg::PrimitiveSet::QUADS, 0);
    quad->push_back(0); quad->push_back(1); quad->push_back(2); quad->push_back(3);
    quad->push_back(4); quad->push_back(5); quad->push_back(6); quad->push_back(7);
    quad->push_back(0); quad->push_back(1); quad->push_back(5); quad->push_back(4);
    quad->push_back(1); quad->push_back(2); quad->push_back(6); quad->push_back(5);
    quad->push_back(2); quad->push_back(3); quad->push_back(7); quad->push_back(6);
    quad->push_back(3); quad->push_back(0); quad->push_back(4); quad->push_back(7);
    geometry->addPrimitiveSet(quad);
    osgUtil::SmoothingVisitor sv;
    geometry->accept(sv);
    // 将Geometry添加到Geode
    geode->addDrawable(geometry);

    // 添加Material以启用顶点颜色
    osg::ref_ptr<osg::Material> material = new osg::Material();
    material->setColorMode(osg::Material::ColorMode::OFF);
    geode->getOrCreateStateSet()->setAttributeAndModes(material, osg::StateAttribute::ON);

    // 将Geode添加到场景图
    osg::ref_ptr<osg::Group> root = new osg::Group();
    root->addChild(geode);

    //osgViewer::Viewer viewer1;
    //viewer1.setUpViewInWindow(100, 100, 800, 600); // (x, y, width, height)
    //viewer1.setSceneData(root.get());
    //viewer1.addEventHandler(new osgViewer::WindowSizeHandler);//全屏  快捷键f
    //viewer1.addEventHandler(new osgViewer::StatsHandler);//查看帧数 s
    //viewer1.addEventHandler(new osgViewer::ScreenCaptureHandler);//截图  快捷键 c
    //viewer1.run();

    osg::ref_ptr<osgDB::Options> option = new osgDB::Options;
    option->setOptionString("embedImages embedBuffers prettyPrint isBinary compressionType=none textureType=jpg");
    osgDB::writeNodeFile(*root.get(), "D:\\nginx-1.22.1\\html\\3dtiles\\newPixel\\"+filename, option);
}
void test() {
    osg::ref_ptr<osg::Node> node = osgDB::readRefNodeFile("E:\\Code\\2023\\Other\\data\\2.FBX");
    node = NULL;
}


int main() {
    setlocale(LC_ALL, "en_US.UTF-8");
    //testOsgdb_webp();
    //testOsgdb_fbx();
 
 //   osg::ref_ptr<osg::Node> node = osgDB::readNodeFile("D:\\Data\\龙翔桥站厅.FBX");
 //   osgViewer::Viewer viewer1;
	//viewer1.setUpViewInWindow(100, 100, 800, 600); // (x, y, width, height)
	//viewer1.setSceneData(node.get());
	//viewer1.addEventHandler(new osgViewer::WindowSizeHandler);//全屏  快捷键f
	//viewer1.addEventHandler(new osgViewer::StatsHandler);//查看帧数 s
	//viewer1.addEventHandler(new osgViewer::ScreenCaptureHandler);//截图  快捷键 c
	//viewer1.run();
 //   osg::ref_ptr<osgDB::Options> option = new osgDB::Options;
 //   option->setOptionString("embedImages embedBuffers prettyPrint isBinary compressionType=none textureType=jpg");
 //   osgDB::writeNodeFile(*node.get(), "D:\\nginx-1.24.0\\html\\3dtiles\\1.gltf", option);
 //   TextureVisitor2 tv2;
 //   node->accept(tv2);

    //createCube(osg::Vec3(), 10000, osg::Vec4(1.0, 0.0, 0.0, 1.0),"root.b3dm");
    //createCube(osg::Vec3(10000,0, 10000), 1000, osg::Vec4(0.0, 1.0, 0.0, 1.0),"0\\L0_0_0_0.b3dm");
    //createCube(osg::Vec3(-10000, 0, 10000), 1000, osg::Vec4(0.0, 0.0, 1.0, 1.0), "0\\L0_0_1_0.b3dm");
    //createCube(osg::Vec3(10000, 0, -10000), 1000, osg::Vec4(1.0, 1.0, 0.0, 1.0), "0\\L0_1_0_0.b3dm");
    //createCube(osg::Vec3(-10000, 0, -10000), 1000, osg::Vec4(0.0, 1.0, 1.0, 1.0), "0\\L0_1_1_0.b3dm");

    //test();
    //std::cout << 1 << std::endl;
    //std::cout << 2 << std::endl;
    //std::cout << 3 << std::endl;

    const std::string basePath = "D:\\Data\\3\\DTD0709017.png";
    osg::ref_ptr<osg::Image> img1 = osgDB::readImageFile("fe2da2f2.jpg");
    preview_img(img1);
    img1->flipVertical();
    preview_img(img1);

    //osg::ref_ptr<osg::Image> img = osgDB::readImageFile("C:\\Users\\ecidi-cve\\Desktop\\1.jpg");
    //img->scaleImage(800, 600, 1);
    //osgDB::writeImageFile(*img.get(),"C:\\Users\\ecidi-cve\\Desktop\\2.jpg");
    return 1;

}
