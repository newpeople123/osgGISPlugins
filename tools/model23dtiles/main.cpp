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
#include "3dtiles/hlod/QuadTreeBuilder.h"
#include "3dtiles/hlod/OcTreeBuilder.h"
#include "3dtiles/hlod/KDTreeBuilder.h"
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
	CoordinateTransformVisitor(PJ* pj, const osg::Matrixd worldToLocal) :_pj(pj), _worldToLocal(worldToLocal), osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {


	}

	void apply(osg::Transform& transform) override {
		osg::Matrixd localMatrix;
		transform.computeLocalToWorldMatrix(localMatrix, this);
		osg::Matrixd savedMatrix = _currentMatrix;
		_currentMatrix.preMult(localMatrix); // 更新当前矩阵
		traverse(transform);
		if (osg::MatrixTransform* matrixTransform = transform.asMatrixTransform())
		{
			matrixTransform->setMatrix(osg::Matrix::identity());
		}
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

// 读取模型文件
osg::ref_ptr<osg::Node> readModelFile(const std::string& input, const bool bSP) {
	osg::ref_ptr<osgDB::Options> readOptions = new osgDB::Options;
#ifndef NDEBUG
	if(bSP)
		readOptions->setOptionString("ShowProgress");
#else
	readOptions->setOptionString("TessellatePolygons");
#endif // !NDEBUG

	osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(input, readOptions.get());

	if (!node) {
		OSG_NOTICE << "Error: Cannot read 3D model file!" << '\n';
	}

	return node;
}

// 应用优化器
void applyOptimizer(osg::ref_ptr<osg::Node>& node) {
	osgUtil::Optimizer optimizer;
	optimizer.optimize(node.get(), osgUtil::Optimizer::INDEX_MESH);
}

// 重计算法线
void recomputeNormals(osg::ref_ptr<osg::Node>& node, const bool faceModel) {
	osgUtil::SmoothingVisitor sv;
	if (faceModel)
		sv.setCreaseAngle(0.0);
	node->accept(sv);
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
		OSG_NOTICE << "Error creating projections from EPSG codes!" << std::endl;
		return nullptr;
	}
	PJ* projTransform = proj_normalize_for_visualization(ctx, proj);
	if (!projTransform) {
		OSG_NOTICE << "Failed to normalize PROJ transformation!" << std::endl;
		proj_destroy(proj);
		return nullptr;
	}
	return projTransform;
}

// 应用偏移量、朝向
osg::ref_ptr<osg::MatrixTransform> applyTranslationAndUpAxis(
	osg::ref_ptr<osg::Node>& node,
	const double translationX, const double translationY, const double translationZ,
	const double scaleX, const double scaleY, const double scaleZ,
	const std::string upAxis,
	const std::string ext)
{
	// datumPoint 是 Z轴向上的坐标
	osg::Vec3d datumPointZUp(translationX, translationY, translationZ);
	osg::Vec3d datumPointYUp;

	// 将 datumPoint 从 Z 向上转换为 Y 向上坐标系
	datumPointYUp.set(
		datumPointZUp.x(),
		datumPointZUp.z(),  // Z -> Y
		-datumPointZUp.y()  // Y -> -Z
	);

	osg::Vec3d scaleZUp(scaleX, scaleY, scaleZ);
	osg::Vec3d scaleYUp;
	scaleYUp.set(
		scaleZUp.x(),
		scaleZUp.z(),  // Z -> Y
		scaleZUp.y()  // Y -> Z//缩放是无方向性的正数比例因子，不需要为了坐标系切换而改变符号
	);

	osg::Matrixd matrix;
	osg::Quat rotation = osg::Quat();

	// FBX 始终是 Y 向上，不需要转换
	if (ext != "fbx")
	{
		if (upAxis == "Z")
		{
			// Z 向上 -> Y 向上（绕 X 轴 -90 度）
			rotation = osg::Quat(-osg::PI_2, osg::Vec3d(1.0, 0.0, 0.0));
		}
		else if (upAxis == "X")
		{
			// X 向上 -> Y 向上（绕 Z 轴 +90 度）
			rotation = osg::Quat(-osg::PI_2, osg::Vec3d(0.0, 0.0, 1.0));
		}
		matrix.makeRotate(rotation);
		matrix.setTrans(datumPointYUp);  // 注意：平移不受旋转影响，已经转换好了
	}
	else
	{
		// FBX 默认是 Y 向上，直接平移
		matrix.setTrans(datumPointYUp);
	}
	matrix.preMultScale(scaleYUp);

	osg::ref_ptr<osg::MatrixTransform> xtransform = new osg::MatrixTransform;
	xtransform->setMatrix(matrix);
	xtransform->addChild(node);
	return xtransform;
}


// 将投影坐标的模型进行坐标转换
void applyProjection(osg::ref_ptr<osg::Node>& node, const std::string epsg, double& latitude, double& longitude, double& altitude)
{
	if (!epsg.empty())
	{
			node->computeBound();
			const osg::BoundingSphere bs = node->getBound();
			if (!bs.valid())
			{
				OSG_NOTICE << "Error:Invalid model bounding box!" << std::endl;
				exit(0);
			}
			const osg::Vec3d center = bs.center();
			const string dstEpsg = "EPSG:4326";
			const string srcEpsg = "EPSG:" + epsg;
			PJ_CONTEXT* ctx = proj_context_create();
			if (!ctx)
			{
				OSG_NOTICE << "Error creating projection context!" << std::endl;
				exit(0);
			}
			const char* searchPath[] = { "./share/proj" };
			proj_context_set_search_paths(ctx, 1, searchPath);
			const char* dbPath = proj_context_get_database_path(ctx);
			if (!dbPath) {
				OSG_NOTICE << "Proj database not found. Please check your search path." << std::endl;
				proj_context_destroy(ctx);
				exit(0);
			}

			PJ* projTo4326 = createProjection(ctx, srcEpsg, dstEpsg);

			PJ_COORD inputCoord = proj_coord(center.x(), -center.z(), center.y(), 0);
			PJ_COORD outputCoord = proj_trans(projTo4326, PJ_FWD, inputCoord);
			latitude = outputCoord.xyz.y;
			longitude = outputCoord.xyz.x;
			altitude = outputCoord.xyz.z;

			const osg::EllipsoidModel ellipsoidModel;
			osg::Matrixd localToWorld;
			ellipsoidModel.computeLocalToWorldTransformFromLatLongHeight(osg::DegreesToRadians(latitude), osg::DegreesToRadians(longitude), altitude, localToWorld);
			PJ* projTo4978 = createProjection(ctx, srcEpsg, "EPSG:4978");
			CoordinateTransformVisitor ctv(projTo4978, osg::Matrixd::inverse(localToWorld));
			node->accept(ctv);
			proj_destroy(projTo4326);
			proj_destroy(projTo4978);
			proj_context_destroy(ctx);
	}
}

void printUsage()
{
	// =================== Input / Output ===================
	osg::notify(osg::NOTICE) << "input / output:" << std::endl;
	osg::notify(osg::NOTICE) << "    -i <file>           - Input file" << std::endl;
	osg::notify(osg::NOTICE) << "    -o <folder>         - Output folder" << std::endl;
	osg::notify(osg::NOTICE) << std::endl;

	// =================== Coordinate Parameters ===================
	osg::notify(osg::NOTICE) << "coordinate parameters:" << std::endl;
	osg::notify(osg::NOTICE) << "    -lat <number>       - Latitude. Default is 30.0" << std::endl;
	osg::notify(osg::NOTICE) << "    -lng <number>       - Longitude. Default is 116.0" << std::endl;
	osg::notify(osg::NOTICE) << "    -alt <number>       - Altitude in meters. Default is 300" << std::endl;
	osg::notify(osg::NOTICE) << "    -epsg <code>        - Use EPSG code when model is in projected coordinate system" << std::endl;
	osg::notify(osg::NOTICE) << "                          Mutually exclusive with -lat/-lng/-alt, but can be used with -tx/-ty/-tz" << std::endl;
	osg::notify(osg::NOTICE) << std::endl;

	// =================== Transform Parameters ===================
	osg::notify(osg::NOTICE) << "transform parameters:" << std::endl;
	osg::notify(osg::NOTICE) << "    -tx <number>        - X offset of model origin. Default is 0.0" << std::endl;
	osg::notify(osg::NOTICE) << "    -ty <number>        - Y offset of model origin. Default is 0.0" << std::endl;
	osg::notify(osg::NOTICE) << "    -tz <number>        - Z offset of model origin. Default is 0.0" << std::endl;
	osg::notify(osg::NOTICE) << "    -sx <number>        - Scale on X axis. Default is 1.0" << std::endl;
	osg::notify(osg::NOTICE) << "    -sy <number>        - Scale on Y axis. Default is 1.0" << std::endl;
	osg::notify(osg::NOTICE) << "    -sz <number>        - Scale on Z axis. Default is 1.0" << std::endl;
	osg::notify(osg::NOTICE) << "    -up <X/Y/Z>         - Model up direction. Options: X, Y, Z. Default is Y" << std::endl;
	osg::notify(osg::NOTICE) << std::endl;

	// =================== Tile Structure ===================
	osg::notify(osg::NOTICE) << "tile structure:" << std::endl;
	osg::notify(osg::NOTICE) << "    -t <quad/oc/kd>     - Tile structure type. Default is quad" << std::endl;
	osg::notify(osg::NOTICE) << std::endl;

	// =================== Compression & Simplification ===================
	osg::notify(osg::NOTICE) << "compression & simplification:" << std::endl;
	osg::notify(osg::NOTICE) << "    -r <number>                                   - Simplification ratio for intermediate nodes. Default is 0.5" << std::endl;
	osg::notify(osg::NOTICE) << "    -tf <png/jpg/webp/ktx2>                       - Texture format. Default is ktx2" << std::endl;
	osg::notify(osg::NOTICE) << "    -vf <draco/meshopt/quantize/quantize_meshopt> - Vertex compression format. No default" << std::endl;
	osg::notify(osg::NOTICE) << "    -cl <low/medium/high>                         - Compression level. Default is medium" << std::endl;
	osg::notify(osg::NOTICE) << "                                                  Only valid with quantize, quantize_meshopt, or draco" << std::endl;
	osg::notify(osg::NOTICE) << std::endl;

	// =================== Performance Limits ===================
	osg::notify(osg::NOTICE) << "performance limits:" << std::endl;
	osg::notify(osg::NOTICE) << "    -tri <number>       - Max triangles per tile. Default is 200000" << std::endl;
	osg::notify(osg::NOTICE) << "    -dc <number>        - Max draw calls per tile. Default is 20" << std::endl;
	osg::notify(osg::NOTICE) << std::endl;

	// =================== Texture Size ===================
	osg::notify(osg::NOTICE) << "texture size:" << std::endl;
	osg::notify(osg::NOTICE) << "    -tw <number>        - Max texture width (power of 2). Default is 256" << std::endl;
	osg::notify(osg::NOTICE) << "    -th <number>        - Max texture height (power of 2). Default is 256" << std::endl;
	osg::notify(osg::NOTICE) << "    -aw <number>        - Max texture atlas width. Default is 2048" << std::endl;
	osg::notify(osg::NOTICE) << "                          Must be power of 2 and >= tw, otherwise no atlas is created" << std::endl;
	osg::notify(osg::NOTICE) << "    -ah <number>        - Max texture atlas height. Default is 2048" << std::endl;
	osg::notify(osg::NOTICE) << "                          Must be power of 2 and >= th, otherwise no atlas is created" << std::endl;
	osg::notify(osg::NOTICE) << std::endl;

	// =================== Transformation Mode ===================
	osg::notify(osg::NOTICE) << "transformation mode:" << std::endl;
	osg::notify(osg::NOTICE) << "    -nft                - Do not apply transform matrix to vertices" << std::endl;
	osg::notify(osg::NOTICE) << "                          Default: applied (faster rendering but possible vertex precision loss)" << std::endl;
	osg::notify(osg::NOTICE) << std::endl;

	// =================== Other ===================
	osg::notify(osg::NOTICE) << "other options:" << std::endl;
	osg::notify(osg::NOTICE) << "    -nrm                - Recalculate normals" << std::endl;
	osg::notify(osg::NOTICE) << "    -nm <v/f>           - Used with -nrm. Normal mode: v = vertex (for smooth), f = face (default, for hard edges)" << std::endl;
	osg::notify(osg::NOTICE) << "    -unlit              - Enable KHR_materials_unlit extension (for baked models)" << std::endl;
	osg::notify(osg::NOTICE) << "    -gn                 - Generate normal map (Sobel) and tangents" << std::endl;
	osg::notify(osg::NOTICE) << "                          Improves visuals but increases processing time and file size" << std::endl;
	osg::notify(osg::NOTICE) << std::endl;
}

int main(int argc, char** argv)
{
	Utils::initConsole();

	osgDB::Registry* instance = Utils::registerFileAliases();

	// use an ArgumentParser object to manage the program arguments.
	osg::ArgumentParser arguments(&argc, argv);
	osg::ApplicationUsage* usage = arguments.getApplicationUsage();
	usage->setDescription(arguments.getApplicationName() + ", a tool to convert 3D models into 3D Tiles.");
	usage->setCommandLineUsage("model23dtiles.exe -i C:\\input\\test.fbx -o C:\\output\\test -lat 39.0 -lng 116.0 -h 300.0 -tf ktx2");
	usage->addCommandLineOption("-h/--help", "Display help information");
	
	
	// Display help if requested
	if (arguments.read("-h") || arguments.read("--help")) {
		osg::setNotifyLevel(osg::NOTICE);
		printUsage();
		return 100;
	}

	// Parse arguments
	const std::string textureFormat = parseArgument(arguments, "-tf", std::string("ktx2"));
	const std::string vertexFormat = parseArgument(arguments, "-vf", std::string("none"));
	const std::string treeFormat = parseArgument(arguments, "-t", std::string("quad"));
	const std::string compressLevel = parseArgument(arguments, "-cl", std::string("medium"));
	const std::string epsg = parseArgument(arguments, "-epsg", std::string());
	const std::string upAxis = parseArgument(arguments, "-up", std::string("Y"));

	const double translationX = parseArgument(arguments, "-tx", 0.0);
	const double translationY = parseArgument(arguments, "-ty", 0.0);
	const double translationZ = parseArgument(arguments, "-tz", 0.0);
	const double scaleX = parseArgument(arguments, "-sx", 1.0);
	const double scaleY = parseArgument(arguments, "-sy", 1.0);
	const double scaleZ = parseArgument(arguments, "-sz", 1.0);
	const double ratio = parseArgument(arguments, "-sr", 0.5);

	double latitude = parseArgument(arguments, "-lat", 30.0);
	double longitude = parseArgument(arguments, "-lng", 116.0);
	double altitude = parseArgument(arguments, "-alt", 300.0);

	const int maxTextureWidth = parseArgument(arguments, "-tw", 256);
	const int maxTextureHeight = parseArgument(arguments, "-th", 256);
	const int maxTextureAtlasWidth = parseArgument(arguments, "-aw", 2048);
	const int maxTextureAtlasHeight = parseArgument(arguments, "-ah", 2048);

	const unsigned int maxTriangleCount = parseArgument(arguments, "-tri", 200000);
	const unsigned int maxDrawcallCommandCount = parseArgument(arguments, "-dc", 20);


	std::string input = parseArgument(arguments, "-i", std::string());
	std::string output = parseArgument(arguments, "-o", std::string());
#ifndef NDEBUG
#else
	input = osgDB::convertStringFromCurrentCodePageToUTF8(input.c_str());
	output = osgDB::convertStringFromCurrentCodePageToUTF8(output.c_str());
#endif // !NDEBUG

	if (input.empty() || output.empty()) {
		OSG_NOTICE << "Input or output path is missing!" << '\n';
		usage->write(std::cout);
		return 1;
	}

	OSG_NOTICE << "Resolving parameters..." << std::endl;

	OSG_NOTICE << "Reading model file..." << std::endl;

	bool showProgress = true;
	if (arguments.find("-sp") > 0)
		showProgress = false;
	if(showProgress)
		instance->setReadFileCallback(new Utils::ProgressReportingFileReadCallback);
	osg::ref_ptr<osg::Node> node = readModelFile(input, showProgress);
	if (!node.valid())
	{
		OSG_NOTICE << "Error:can not read 3d model file!" << '\n';
		return 2;
	}
	try
	{
		applyOptimizer(node);

		const std::string ext = osgDB::getLowerCaseFileExtension(input);
		osg::ref_ptr<osg::MatrixTransform> xtransform = applyTranslationAndUpAxis(node, translationX, translationY, translationZ, scaleX, scaleY, scaleZ, upAxis, ext);
		osg::ref_ptr<osg::Node> tNode = xtransform->asNode();
		applyProjection(tNode, epsg, latitude, longitude, altitude);

		std::string optionsStr = "";
		if (vertexFormat == "draco")
		{
			optionsStr += " ct=draco";
			if (compressLevel == "low")
			{
				optionsStr += " vp=16 vt=14 vc=10 vn=12 vg=18";
			}
			else if (compressLevel == "high")
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
			if (compressLevel == "low")
			{
				optionsStr += " vp=16 vt=14 vn=12 vc=12 ";
			}
			else if (compressLevel == "high")
			{
				optionsStr += " vp=10 vt=8 vn=4 vc=4 ";
			}
		}
		else if (vertexFormat == "quantize_meshopt")
		{
			optionsStr += " ct=meshopt quantize";
			if (compressLevel == "low")
			{
				optionsStr += " quantize vp=16 vt=14 vn=12 vc=12 ";
			}
			else if (compressLevel == "high")
			{
				optionsStr += " quantize vp=10 vt=8 vn=4 vc=4 ";
			}
		}
		if (arguments.find("-unlit") > 0)
		{
			optionsStr += " unlit";
		}

		// 变换完成后再计算法线
		if (arguments.find("-nrm") > 0)
		{
			const std::string normalMode = parseArgument(arguments, "-nm", std::string("f"));
			if (normalMode == "v")
				recomputeNormals(node, false);
			else if (normalMode == "f")
				recomputeNormals(node, true);
		}

		Tileset::Config config;
		config.latitude = latitude;
		config.longitude = longitude;
		config.altitude = altitude;
		config.tileConfig.simplifyRatio = ratio;
		config.tileConfig.path = output;
		config.tileConfig.gltfTextureOptions.maxTextureWidth = maxTextureWidth;
		config.tileConfig.gltfTextureOptions.maxTextureHeight = maxTextureHeight;
		config.tileConfig.gltfTextureOptions.maxTextureAtlasWidth = maxTextureAtlasWidth;
		config.tileConfig.gltfTextureOptions.maxTextureAtlasHeight = maxTextureAtlasHeight;
		config.tileConfig.gltfTextureOptions.ext = "." + textureFormat;
		if (arguments.find("-nft") > 0)
			config.tileConfig.gltfOptimizerOptions &= ~GltfOptimizer::FLATTEN_TRANSFORMS;
		if (arguments.find("-gn") > 0)
			config.tileConfig.gltfOptimizerOptions |= GltfOptimizer::GENERATE_NORMAL_TEXTURE;

#ifdef _WIN32
		config.tileConfig.gltfTextureOptions.cachePath = config.tileConfig.path + "\\textures";
#else
		config.tileConfig.gltfTextureOptions.cachePath = config.tileConfig.path + "/textures";
#endif
		osgDB::makeDirectory(config.tileConfig.gltfTextureOptions.cachePath);
		config.tileConfig.options->setOptionString(optionsStr);

		TreeBuilder::BuilderConfig treeConfig;
		treeConfig.setMaxDrawcallCommandCount(maxDrawcallCommandCount);
		treeConfig.setMaxTriagnleCount(maxTriangleCount);

		TreeBuilder* treeBuilder = new QuadTreeBuilder(treeConfig);
		if (treeFormat == "oc")
			treeBuilder = new OcTreeBuilder(treeConfig);
		else if (treeFormat == "kd")
			treeBuilder = new KDTreeBuilder(treeConfig);
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
			OSG_NOTICE << "Failed converted " + input + " to 3dtiles..." << std::endl;
			delete treeBuilder;
			return 3;
		}
		delete treeBuilder;

	}
	catch (const std::invalid_argument& e)
	{
		OSG_NOTICE << "invalid input: " << e.what() << '\n';
		return 4;
	}
	catch (const std::out_of_range& e)
	{
		OSG_NOTICE << "value out of range: " << e.what() << '\n';
		return 5;
	}


	return 0;
}
