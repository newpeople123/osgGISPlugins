#ifndef OSG_GIS_PLUINS_BOUNDING_VOLUME_H
#define OSG_GIS_PLUINS_BOUNDING_VOLUME_H
#include <osg/Object>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using namespace std;
namespace osgGISPlugins
{
	class BoundingVolume :public osg::Object {
	public:
		vector<double> region;
		vector<double> box;
		vector<double> sphere;
		BoundingVolume() = default;

		BoundingVolume(const BoundingVolume& other, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
			: osg::Object(other, copyop),
			region(other.region),
			box(other.box),
			sphere(other.sphere) {}

		void fromJson(const json& j);

		json toJson() const;

		virtual osg::Object* cloneType() const { return new BoundingVolume(); }

		virtual osg::Object* clone(const osg::CopyOp& copyop) const { return new BoundingVolume(*this, copyop); }

		virtual const char* libraryName() const { return "osgGISPlugins"; }

		virtual const char* className() const { return "BoundingVolume"; }

		void computeBox(osg::ref_ptr<osg::Node> node);

		void computeSphere(osg::ref_ptr<osg::Node> node);

		void computeRegion(osg::ref_ptr<osg::Node> node, const double latitude, const double longitude, const double height);
	};
}
#endif // !OSG_GIS_PLUINS_BOUNDING_VOLUME_H
