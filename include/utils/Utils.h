#ifndef OSG_GIS_PLUGINS_UTILS_H
#define OSG_GIS_PLUGINS_UTILS_H
#include <osg/Node>
#include <osg/NodeVisitor>
#include <osg/Geometry>
#include <osg/PrimitiveSet>
#include <osg/StateSet>
#include <osg/Material>
#include <osg/Geode>
#include <osg/Texture2D>
#include <osg/Math>
#include <osg/ComputeBoundsVisitor>
#include <set>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/ConvertUTF>
#include <iostream>
#include <iomanip>  // 用于输出格式化的百分比
#include <fstream>  // 用于获取文件大小
#include <indicators/cursor_control.hpp>
#include <indicators/progress_bar.hpp>
#include "osgdb_gltf/material/GltfPbrMRMaterial.h"
#include "osgdb_gltf/material/GltfPbrSGMaterial.h"
using namespace indicators;
namespace osgGISPlugins
{
	const static double EPSILON = 0.001;

	class MaxGeometryVisitor : public osg::NodeVisitor
	{
	public:
		osg::BoundingBox maxBB;          // 最大包围盒
		osg::Geometry* geom;

		MaxGeometryVisitor()
			: osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN) {}

		virtual void apply(osg::Transform& transform) override;

		virtual void apply(osg::Geode& geode) override;

		void pushMatrix(const osg::Matrix& matrix);

		void popMatrix();

		osg::Matrix _currentMatrix;
		std::vector<osg::Matrix> _matrixStack;
	};

	class CRenderingThread : public OpenThreads::Thread
	{
	public:

		ProgressBar bar{
			option::BarWidth{50},
			option::PrefixText("Reading model file:"),
			option::Start{"["},
			option::Fill{"="},
			option::Lead{">"},
			option::Remainder{" "},
			option::End{"]"},
			option::ShowElapsedTime{true},
			option::ShowRemainingTime{true},
			option::ShowPercentage{true}
		};

		CRenderingThread(osgDB::ifstream* fin) :_fin(fin)
		{
			show_console_cursor(false);
			fin->seekg(0, std::ifstream::end);
			_length = fin->tellg();
			fin->seekg(0, std::ifstream::beg);

		};
		virtual ~CRenderingThread() {};

		void stop();

		virtual void run();

	protected:
		osgDB::ifstream* _fin;
		int _length;
		bool _bStop = false;
	};

	class ProgressReportingFileReadCallback : public osgDB::Registry::ReadFileCallback
	{
	public:
		CRenderingThread* crt;
		typedef osgDB::ReaderWriter::ReadResult ReadResult;
		ProgressReportingFileReadCallback() {}
		~ProgressReportingFileReadCallback()
		{
			if (crt)
			{
				crt->join();
				delete crt;
				crt = nullptr;
			}
		}
		virtual osgDB::ReaderWriter::ReadResult readNode(const std::string& file, const osgDB::Options* option);
	};

	class TriangleCountVisitor : public osg::NodeVisitor
	{
	public:
		META_NodeVisitor(osgGISPlugins, TriangleCountVisitor)

			unsigned int count = 0;

		TriangleCountVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

		void apply(osg::Drawable& drawable) override;

	private:
		unsigned int calculateTriangleCount(const GLenum mode, const unsigned int count);
	};

	class TextureCountVisitor :public osg::NodeVisitor
	{
	public:
		META_NodeVisitor(osgGISPlugins, TextureCountVisitor)

			unsigned int count = 0;

		TextureCountVisitor() :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

		void apply(osg::Drawable& drawable) override;
	private:
		std::set<std::string> _names;
		void countTextures(osg::StateSet* stateSet);
	};

	class TextureMetricsVisitor : public osg::NodeVisitor
	{
		int _resolution = 0.0;
		double _minTexelDensity; // 用于记录最小纹素密度
	public:
		double diagonalLength = 0.0;
		TextureMetricsVisitor(int resolution) : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
		{
			_minTexelDensity = FLT_MAX;
			_resolution = resolution;
			_currentMatrix.makeIdentity();
		}

		virtual void apply(osg::Transform& transform) override;

		virtual void apply(osg::Geode& geode) override;

	private:
		void pushMatrix(const osg::Matrix& matrix);

		void popMatrix();

		osg::Matrix _currentMatrix;
		std::vector<osg::Matrix> _matrixStack;
	};

	class Utils {
	public:

		static bool compareMatrix(const osg::Matrixd& lhs, const osg::Matrixd& rhs);

		static bool compareMatrixs(const std::vector<osg::Matrixd>& lhses, const std::vector<osg::Matrixd>& rhses);

		static bool compareStateSet(osg::ref_ptr<osg::StateSet> stateSet1, osg::ref_ptr<osg::StateSet> stateSet2);

		static bool comparePrimitiveSet(osg::ref_ptr<osg::PrimitiveSet> pSet1, osg::ref_ptr<osg::PrimitiveSet> pSet2);

		static bool compareGeometry(osg::ref_ptr<osg::Geometry> geom1, osg::ref_ptr<osg::Geometry> geom2);

		static bool compareGeode(osg::Geode& geode1, osg::Geode& geode2);
	};

	struct MatrixEqual {
		bool operator()(const osg::Matrixd& lhs, const osg::Matrixd& rhs) const {
			return Utils::compareMatrix(lhs, rhs);
		}
	};

	struct MatrixsEqual {
		bool operator()(const std::vector<osg::Matrixd>& lhses, const std::vector<osg::Matrixd>& rhses) const {
			return Utils::compareMatrixs(lhses, rhses);
		}
	};

	struct MatrixHash {
		std::size_t operator()(const osg::Matrixd& matrix) const {
			std::size_t seed = 0;
			for (int i = 0; i < 4; ++i) {
				for (int j = 0; j < 4; ++j) {
					seed ^= std::hash<double>()(matrix(i, j)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
				}
			}
			return seed;
		}
	};

	struct MatrixsHash {
		std::size_t operator()(const std::vector<osg::Matrixd>& matrixs) const {
			std::size_t seed = 0;
			for (const auto matrix : matrixs)
			{
				for (int i = 0; i < 4; ++i) {
					for (int j = 0; j < 4; ++j) {
						seed ^= std::hash<double>()(matrix(i, j)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
					}
				}
			}
			return seed;
		}
	};
}
#endif // !OSG_GIS_PLUGINS_UTILS_H
