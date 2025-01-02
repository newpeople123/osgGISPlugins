#include <osg/ArgumentParser>
#include <osgDB/Options>
#include <osgDB/ConvertUTF>
#include <osgDB/ReadFile>
#include <iostream>
#include <osgDB/FileUtils>
#include <osg/MatrixTransform>
#include <osgUtil/SmoothingVisitor>
#include <osg/PositionAttitudeTransform>
#include <proj.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "3dtiles/Tileset.h"
#include "3dtiles/hlod/QuadtreeBuilder.h"
#include "3dtiles/hlod/OctreeBuilder.h"
#include <osg/CoordinateSystemNode>
#include <osg/ComputeBoundsVisitor>
#include <osgDB/FileNameUtils>
class CoordinateTransformVisitor :public osg::NodeVisitor
{
private:
	PJ* _pj;
	osg::Matrixd _worldToLocal;
	osg::Matrixd _currentMatrix; // 当前矩阵，用于继承和变换

	osg::Vec3d transformVertex(const osg::Vec3d& vertex) {
		osg::Vec3d worldPosition = vertex * _currentMatrix;
		PJ_COORD inputCoord = proj_coord(worldPosition.x(), -worldPosition.z(), worldPosition.y(), 0);
		PJ_COORD outputCoord = proj_trans(_pj, PJ_FWD, inputCoord);
		osg::Vec3d projected(outputCoord.xyz.x, outputCoord.xyz.y, outputCoord.xyz.z);
		osg::Vec3d localPosition = projected * _worldToLocal;

		osg::Quat quat(-osg::PI_2, osg::Vec3(1.0, 0.0, 0.0));
		return quat * localPosition;
	}
public:
	CoordinateTransformVisitor(PJ* pj, const osg::Matrixd worldToLocal) :_pj(pj), _worldToLocal(worldToLocal) ,osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {


	}

	void apply(osg::Transform& transform) override {
		osg::Matrixd localMatrix;
		transform.computeLocalToWorldMatrix(localMatrix, this);
		osg::Matrixd savedMatrix = _currentMatrix;
		_currentMatrix *= localMatrix; // 更新当前矩阵
		traverse(transform);
		_currentMatrix = savedMatrix; // 恢复父节点矩阵
	}

	void apply(osg::Drawable& drawable) override {
		if (drawable.getDataVariance() == osg::Object::DataVariance::STATIC)
			return;

		osg::Geometry* geometry = drawable.asGeometry();
		if (geometry) {
			osg::ref_ptr<osg::Vec3Array> positions = dynamic_cast<osg::Vec3Array*>(geometry->getVertexArray());
			if (positions) {
				std::transform(positions->begin(), positions->end(), positions->begin(),
					[this](const osg::Vec3d& vertex) {
						return transformVertex(vertex);
					});
				geometry->dirtyBound();
				geometry->computeBound();
			}
		}
	}
};

// 设置控台编码
void initConsole()
{
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#else
	setlocale(LC_ALL, "en_US.UTF-8");
#endif // _WIN32
}

// 注册文件扩展名别名
void registerFileAliases() {
	osgDB::Registry* instance = osgDB::Registry::instance();
	instance->addFileExtensionAlias("glb", "gltf");
	instance->addFileExtensionAlias("b3dm", "gltf");
	instance->addFileExtensionAlias("i3dm", "gltf");
	instance->addFileExtensionAlias("ktx2", "ktx");
	instance->setReadFileCallback(new ProgressReportingFileReadCallback);
}

// 读取模型文件
osg::ref_ptr<osg::Node> readModelFile(const std::string& input) {
	osg::ref_ptr<osgDB::Options> readOptions = new osgDB::Options;
	//readOptions->setOptionString("TessellatePolygons ShowProgress");
	readOptions->setOptionString("ShowProgress");

	osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(input, readOptions.get());

	if (!node) {
		OSG_FATAL << "Error: Cannot read 3D model file!" << '\n';
	}

	return node;
}

// 应用优化器
void applyOptimizer(osg::ref_ptr<osg::Node>& node) {
	osgUtil::Optimizer optimizer;
	optimizer.optimize(node.get(), osgUtil::Optimizer::INDEX_MESH);
}

// 重计算法线
void recomputeNormals(osg::ref_ptr<osg::Node>& node) {
	osgUtil::SmoothingVisitor smoothingVisitor;
	node->accept(smoothingVisitor);
}

// 解析单个命令行参数
template <typename T>
T parseArgument(osg::ArgumentParser& arguments, const std::string& option, const T& defaultValue) {
	T value = defaultValue;
	arguments.read(option, value);
	return value;
}

// 创建投影转换
PJ* createProjection(PJ_CONTEXT* ctx, const std::string& srcEpsg, const std::string& dstEpsg) {
	PJ* proj = proj_create_crs_to_crs(ctx, srcEpsg.c_str(), dstEpsg.c_str(), nullptr);
	if (!proj) {
		OSG_FATAL << "Error creating projections from EPSG codes!" << std::endl;
		return nullptr;
	}
	PJ* projTransform = proj_normalize_for_visualization(ctx, proj);
	if (!projTransform) {
		OSG_FATAL << "Failed to normalize PROJ transformation!" << std::endl;
		proj_destroy(proj);
		return nullptr;
	}
	return projTransform;
}

// 应用偏移量、朝向
osg::ref_ptr<osg::MatrixTransform> applyTranslationAndUpAxis(osg::ref_ptr<osg::Node>& node, const double translationX, const double translationY, const double translationZ, const std::string upAxis,const std::string ext)
{

	osg::Vec3d datumPoint = osg::Vec3d(-translationX, -translationY, -translationY);
	osg::ref_ptr<osg::MatrixTransform> xtransform = new osg::MatrixTransform;
	// FBX无论什么轴向上，读取后都会变成Y轴向上，这里就不用再特殊处理了
	if (ext != "fbx")
	{
		osg::Matrixd matrix;
		if (upAxis == "X")
		{
			const osg::Quat quat = osg::Quat(-osg::PI_2, osg::Vec3d(0.0, 0.0, 1.0));
			matrix.makeRotate(quat);
			matrix.setTrans(quat * datumPoint);
		}
		else if (upAxis == "Z")
		{
			const osg::Quat quat = osg::Quat(-osg::PI_2, osg::Vec3(1.0, 0.0, 0.0));
			matrix.makeRotate(quat);
			matrix.setTrans(quat * datumPoint);
		}
		else if (upAxis == "Y")
		{
			matrix.setTrans(datumPoint);
		}
		xtransform->setMatrix(matrix);
	}
	xtransform->addChild(node);
	return xtransform;
}

// 将投影坐标的模型进行坐标转换
void applyProjection(osg::ref_ptr<osg::Node>& node, const std::string epsg, double& latitude, double& longitude, double& height)
{
	if (!epsg.empty())
	{
			node->computeBound();
			const osg::BoundingSphere bs = node->getBound();
			if (!bs.valid())
			{
				OSG_FATAL << "Error:Invalid model bounding box!" << std::endl;
				exit(0);
			}
			const osg::Vec3d center = bs.center();
			const string dstEpsg = "EPSG:4326";
			const string srcEpsg = "EPSG:" + epsg;
			PJ_CONTEXT* ctx = proj_context_create();
			if (!ctx)
			{
				OSG_FATAL << "Error creating projection context!" << std::endl;
				exit(0);
			}
			const char* searchPath[] = { "./share/proj" };
			proj_context_set_search_paths(ctx, 1, searchPath);
			const char* dbPath = proj_context_get_database_path(ctx);
			if (!dbPath) {
				OSG_FATAL << "Proj database not found. Please check your search path." << std::endl;
				proj_context_destroy(ctx);
				exit(0);
			}

			PJ* projTo4326 = createProjection(ctx, srcEpsg, dstEpsg);

			PJ_COORD inputCoord = proj_coord(center.x(), -center.z(), center.y(), 0);
			PJ_COORD outputCoord = proj_trans(projTo4326, PJ_FWD, inputCoord);
			latitude = outputCoord.xyz.y;
			longitude = outputCoord.xyz.x;
			height = outputCoord.xyz.z;

			const osg::EllipsoidModel ellipsoidModel;
			osg::Matrixd localToWorld;
			ellipsoidModel.computeLocalToWorldTransformFromLatLongHeight(osg::DegreesToRadians(latitude), osg::DegreesToRadians(longitude), height, localToWorld);
			PJ* projTo4978 = createProjection(ctx, srcEpsg, "EPSG:4978");
			CoordinateTransformVisitor ctv(projTo4978, osg::Matrixd::inverse(localToWorld));
			node->accept(ctv);
			proj_destroy(projTo4326);
			proj_destroy(projTo4978);
			proj_context_destroy(ctx);
	}
}

int main(int argc, char** argv)
{
	initConsole();

	registerFileAliases();

	// use an ArgumentParser object to manage the program arguments.
	osg::ArgumentParser arguments(&argc, argv);
	osg::ApplicationUsage* usage = arguments.getApplicationUsage();
	usage->setDescription(arguments.getApplicationName() + ",that is used to convert 3D model to 3dtiles.");
	usage->setCommandLineUsage("model23dtiles.exe -i C:\\input\\test.fbx -o C:\\output\\test -lat 116.0 -lng 39.0 -height 300.0 -tf ktx2");
	usage->addCommandLineOption("-i <file>", "input 3D model file full path,must be a file.");
	usage->addCommandLineOption("-o <folder>", "output 3dtiles path,must be a directory.");
	usage->addCommandLineOption("-tf <png/jpg/webp/ktx2>", "texture format,option values are png、jpg、webp、ktx2，default value is jpg.");
	usage->addCommandLineOption("-vf <draco/meshopt/quantize/quantize_meshopt>", "vertex format,option values are draco、meshopt、quantize、quantize_meshopt,default is none.");
	usage->addCommandLineOption("-t <quad/oc>", " tree format,option values are quad、oc,default is oc.");
	usage->addCommandLineOption("-ratio <number>", "Simplified ratio of intermediate nodes.default is 0.5.");
	usage->addCommandLineOption("-lat <number>", "datum point's latitude.");
	usage->addCommandLineOption("-lng <number>", "datum point's longitude.");
	usage->addCommandLineOption("-height <number>", "datum point's height.");
	usage->addCommandLineOption("-translationX <number>", "The x-coordinate of the model datum point,default is 0, must be a power of 2.");
	usage->addCommandLineOption("-translationY <number>", "The y-coordinate of the model datum point,default is 0, must be a power of 2.");
	usage->addCommandLineOption("-translationZ <number>", "The z-coordinate of the model datum point,default is 0, must be a power of 2.");
	usage->addCommandLineOption("-upAxis <X/Y/Z>", "Indicate which axis of the model is facing upwards,default is Y");
	usage->addCommandLineOption("-maxTextureWidth <number>", "single texture's max width,default is 256.");
	usage->addCommandLineOption("-maxTextureHeight <number>", "single texture's max height,default is 256.");
	usage->addCommandLineOption("-maxTextureAtlasWidth <number>", "textrueAtlas's max width,default is 2048.");
	usage->addCommandLineOption("-maxTextureAtlasHeight <number>", "textrueAtlas's max height,default is 2048.");
	usage->addCommandLineOption("-comporessLevel <low/medium/high>", "draco/quantize/quantize_meshopt compression level,default is medium.");
	usage->addCommandLineOption("-recomputeNormal", "Recalculate normals.");
	usage->addCommandLineOption("-unlit", "Enable KHR_materials_unlit, the model is not affected by lighting.");
	usage->addCommandLineOption("-epsg", "Specify the projection coordinate system, which is mutually exclusive with -lat、-lng、height.");


	usage->addCommandLineOption("-h or --help", "Display command line parameters.");

	// if user request help write it out to cout.
	if (arguments.read("-h") || arguments.read("--help"))
	{
		usage->write(std::cout);
		return 1;
	}



	OSG_NOTICE << "Resolving parameters..." << std::endl;

	const std::string textureFormat = parseArgument(arguments, "-tf", std::string("jpg"));
	const std::string vertexFormat = parseArgument(arguments, "-vf", std::string("none"));
	const std::string treeFormat = parseArgument(arguments, "-t", std::string("quad"));
	const std::string comporessLevel = parseArgument(arguments, "-comporessLevel", std::string("medium"));
	const std::string epsg = parseArgument(arguments, "-epsg", std::string());
	const std::string upAxis = parseArgument(arguments, "-upAxis", std::string("Y"));

	const double translationX = parseArgument(arguments, "-translationX", 0.0);
	const double translationY = parseArgument(arguments, "-translationY", 0.0);
	const double translationZ = parseArgument(arguments, "-translationZ", 0.0);
	const double ratio = parseArgument(arguments, "-ratio", 0.5);

	double latitude = parseArgument(arguments, "-lat", 30.0);
	double longitude = parseArgument(arguments, "-lng", 116.0);
	double height = parseArgument(arguments, "-height", 300.0);

	const int maxTextureHeight = parseArgument(arguments, "-maxTextureHeight", 256);
	const int maxTextureWidth = parseArgument(arguments, "-maxTextureWidth", 256);
	const int maxTextureAtlasHeight = parseArgument(arguments, "-maxTextureAtlasHeight", 2048);
	const int maxTextureAtlasWidth = parseArgument(arguments, "-maxTextureAtlasWidth", 2048);

	std::string input = parseArgument(arguments, "-i", std::string(R"(E:\Code\2023\Other\data\data\dgn\工厂三维总装.fbx)"));
	std::string output = parseArgument(arguments, "-o", std::string(R"(D:\nginx-1.22.1\html\3dtiles\工厂三维总装)"));
#ifndef NDEBUG
#else
	input = osgDB::convertStringFromCurrentCodePageToUTF8(input.c_str());
	output = osgDB::convertStringFromCurrentCodePageToUTF8(output.c_str());
#endif // !NDEBUG

	if (input.empty() || output.empty()) {
		OSG_FATAL << "Input or output path is missing!" << '\n';
		return 0;
	}

	OSG_NOTICE << "Reading model file..." << std::endl;
	osg::ref_ptr<osg::Node> node = readModelFile(input);
	if (!node.valid())
	{
		OSG_FATAL << "Error:can not read 3d model file!" << '\n';
		return 0;
	}
	try
	{
		applyOptimizer(node);

		if (arguments.find("-recomputeNormal") > 0)
		{
			recomputeNormals(node);
		}

		const std::string ext = osgDB::getLowerCaseFileExtension(input);
		osg::ref_ptr<osg::MatrixTransform> xtransform = applyTranslationAndUpAxis(node, translationX, translationY, translationZ, upAxis, ext);
		osg::ref_ptr<osg::Node> tNode = xtransform->asNode();
		applyProjection(tNode, epsg, latitude, longitude, height);

		std::string optionsStr = "";
		if (vertexFormat == "draco")
		{
			optionsStr += " ct=draco";
			if (comporessLevel == "low")
			{
				optionsStr += " vp=16 vt=14 vc=10 vn=12 vg=18";
			}
			else if (comporessLevel == "high")
			{
				optionsStr += " vp=12 vt=12 vc=8 vn=8 vg=14";
			}
		}
		else if (vertexFormat == "meshopt")
		{
			optionsStr += " ct=meshopt";
		}
		else if (vertexFormat == "quantize")
		{
			optionsStr += " quantize";
			if (comporessLevel == "low")
			{
				optionsStr += " vp=16 vt=14 vn=12 vc=12 ";
			}
			else if (comporessLevel == "high")
			{
				optionsStr += " vp=10 vt=8 vn=4 vc=4 ";
			}
		}
		else if (vertexFormat == "quantize_meshopt")
		{
			optionsStr += " ct=meshopt quantize";
			if (comporessLevel == "low")
			{
				optionsStr += " quantize vp=16 vt=14 vn=12 vc=12 ";
			}
			else if (comporessLevel == "high")
			{
				optionsStr += " quantize vp=10 vt=8 vn=4 vc=4 ";
			}
		}
		if (arguments.find("-unlit") > 0)
		{
			optionsStr += " unlit";
		}

		Tileset::Config config;
		config.latitude = latitude;
		config.longitude = longitude;
		config.height = height;
		config.tileConfig.simplifyRatio = ratio;
		config.tileConfig.path = output;
		config.tileConfig.gltfTextureOptions.maxTextureWidth = maxTextureWidth;
		config.tileConfig.gltfTextureOptions.maxTextureHeight = maxTextureHeight;
		config.tileConfig.gltfTextureOptions.maxTextureAtlasWidth = maxTextureAtlasWidth;
		config.tileConfig.gltfTextureOptions.maxTextureAtlasHeight = maxTextureAtlasHeight;
		config.tileConfig.gltfTextureOptions.ext = "." + textureFormat;
#ifdef _WIN32
		config.tileConfig.gltfTextureOptions.cachePath = config.tileConfig.path + "\\textures";
#else
		config.gltfTextureOptions.cachePath = config.path + "/textures";
#endif
		osgDB::makeDirectory(config.tileConfig.gltfTextureOptions.cachePath);
		config.tileConfig.options->setOptionString(optionsStr);

		TreeBuilder* treeBuilder = new QuadtreeBuilder;
		if (treeFormat == "oc")
			treeBuilder = new OctreeBuilder;
		OSG_NOTICE << "Building " + treeFormat << " tree..." << std::endl;
		osg::ref_ptr<Tileset> tileset = new Tileset(xtransform, *treeBuilder,config);

		OSG_NOTICE << "Exporting 3dtiles..." << std::endl;
		const bool result = tileset->write();

		if (result)
		{
			OSG_NOTICE << "Successfully converted " + input + " to 3dtiles!" << std::endl;
		}
		else
		{
			OSG_FATAL << "Failed converted " + input + " to 3dtiles..." << std::endl;
		}
		delete treeBuilder;

	}
	catch (const std::invalid_argument& e)
	{
		OSG_FATAL << "invalid input: " << e.what() << '\n';
	}
	catch (const std::out_of_range& e)
	{
		OSG_FATAL << "value out of range: " << e.what() << '\n';
	}


	return 1;
}
