#ifndef OSG_GIS_PLUINS_BOUNDING_VOLUME_H
#define OSG_GIS_PLUINS_BOUNDING_VOLUME_H
#include <osg/Object>
#include <nlohmann/json.hpp>
namespace osgGISPlugins
{
	using namespace std;
	using json = nlohmann::json;
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

		osg::Object* cloneType() const override { return new BoundingVolume(); }

		osg::Object* clone(const osg::CopyOp& copyop) const override { return new BoundingVolume(*this, copyop); }

		const char* libraryName() const override { return "osgGISPlugins"; }

		const char* className() const override { return "BoundingVolume"; }

		void computeBox(const osg::ref_ptr<osg::Node>& node);

		void computeSphere(const osg::ref_ptr<osg::Node>& node);

		void computeRegion(osg::ref_ptr<osg::Node> node, double latitude, double longitude, double height);
	};
}
#endif // !OSG_GIS_PLUINS_BOUNDING_VOLUME_H
