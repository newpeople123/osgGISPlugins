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
#include <unordered_set>
#include <unordered_map>
using namespace indicators;
namespace osgGISPlugins
{
	class Utils {
	public:
		static void pushStateSet2UniqueStateSets(osg::ref_ptr<osg::StateSet> stateSet, std::vector<osg::ref_ptr<osg::StateSet>>& uniqueStateSets);

		static bool isRepeatTexCoords(osg::ref_ptr<osg::Vec2Array> texCoords);

		static bool compareMatrix(const osg::Matrixd& lhs, const osg::Matrixd& rhs);

		static bool compareMatrixs(const std::vector<osg::Matrixd>& lhses, const std::vector<osg::Matrixd>& rhses);

		static bool compareStateSet(const osg::ref_ptr<osg::StateSet>& stateSet1, const osg::ref_ptr<osg::StateSet>& stateSet2);

		static bool comparePrimitiveSet(const osg::ref_ptr<osg::PrimitiveSet>& pSet1, const osg::ref_ptr<osg::PrimitiveSet>& pSet2);

		static bool compareGeometry(const osg::ref_ptr<osg::Geometry>& geom1, const osg::ref_ptr<osg::Geometry>& geom2);

		static bool compareGeode(const osg::ref_ptr<osg::Geode>& geode1, const osg::ref_ptr<osg::Geode>& geode2);

		static bool compareDrawElements(const osg::DrawElements* de1, const osg::DrawElements* de2);

		static bool compareDrawArrays(const osg::DrawArrays* da1, const osg::DrawArrays* da2);

		static bool compareArray(const osg::Array* array1, const osg::Array* array2);

		static bool compareUniformValue(const osg::Uniform& u1, const osg::Uniform& u2);

		template<typename T, typename DrawElementsType>
		static bool compareDrawElementsTemplate(const DrawElementsType* de1,
			const DrawElementsType* de2)
		{
			if (!de1 || !de2) return false;

			if (de1->getNumIndices() != de2->getNumIndices())
				return false;

			for (unsigned int i = 0; i < de1->getNumIndices(); ++i)
			{
				if (de1->index(i) != de2->index(i))
					return false;
			}
			return true;
		}

		// 主模板声明 - 只声明不定义，强制使用特化版本
		template<typename VecType>
		static bool compareVec(const typename VecType::value_type& v1,
			const typename VecType::value_type& v2);

		// 主比较函数保持不变
		template<typename T>
		static bool compareVecArray(const T* array1, const T* array2)
		{
			if (!array1 || !array2) return false;
			if (array1->size() != array2->size()) return false;

			for (size_t i = 0; i < array1->size(); ++i)
			{
				if (!compareVec<T>(array1->at(i), array2->at(i)))
					return false;
			}
			return true;
		}

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
			ProgressReportingFileReadCallback() : crt(nullptr) {}
			~ProgressReportingFileReadCallback()
			{
				//if (crt != nullptr)
				//{
				//	crt->join();
				//	delete crt;
				//	crt = nullptr;
				//}
			}
			virtual osgDB::ReaderWriter::ReadResult readNode(const std::string& file, const osgDB::Options* option);
		};

		class TriangleCounterVisitor : public osg::NodeVisitor
		{
		public:
			META_NodeVisitor(osgGISPlugins, TriangleCounterVisitor)

				unsigned int count = 0;

			TriangleCounterVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

			void apply(osg::Drawable& drawable) override;

		private:
			unsigned int calculateTriangleCount(const GLenum mode, const unsigned int count);
		};

		class TextureCounterVisitor :public osg::NodeVisitor
		{
		public:
			META_NodeVisitor(osgGISPlugins, TextureCounterVisitor)

				TextureCounterVisitor() :osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

			void apply(osg::Drawable& drawable) override;

			unsigned int getCount() const
			{
				return _stateSets.size();
			}
		private:
			std::vector<osg::ref_ptr<osg::StateSet>> _stateSets;
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

		class DrawcallCommandCounterVisitor :public osg::NodeVisitor
		{
		public:
			DrawcallCommandCounterVisitor():osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN){}

			void apply(osg::Transform& transform) override;

			void apply(osg::Drawable& drawable) override;

			unsigned int getCount() const;
		private:
			void pushMatrix(const osg::Matrix& matrix);

			void popMatrix();

			std::unordered_map<osg::Matrixd, std::vector<osg::ref_ptr<osg::Geometry>>, Utils::MatrixHash, Utils::MatrixEqual> _matrixGeometryMap;
			osg::Matrix _currentMatrix;
			std::vector<osg::Matrix> _matrixStack;
		};

		// 设置控台编码
		static void initConsole();

		// 注册文件扩展名别名
		static void registerFileAliases();
	};
}
#endif // !OSG_GIS_PLUGINS_UTILS_H
