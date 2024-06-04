#include "3dtiles/optimizer/mesh/PmpMeshOptimizer.h"
#include <pmp/algorithms/utilities.h>
#include <pmp/algorithms/remeshing.h>
#include <pmp/algorithms/decimation.h>
#include <pmp/algorithms/hole_filling.h>
void PmpMeshOptimizer::reindexMesh(osg::ref_ptr<osg::Geometry> geom)
{
	return;
}

void PmpMeshOptimizer::simplifyMesh(osg::ref_ptr<osg::Geometry> geom, const float simplifyRatio)
{
	return;

}

pmp::SurfaceMesh* PmpMeshOptimizer::convertOsg2Pmp(osg::ref_ptr<osg::Geometry> geom)
{
	pmp::SurfaceMesh* pmpMesh = new pmp::SurfaceMesh;

	pmp::VertexProperty<pmp::Normal> pmpNormals = pmpMesh->vertex_property<pmp::Normal>("v:normal");
	pmp::VertexProperty<pmp::Color> pmpColors = pmpMesh->vertex_property<pmp::Color>("v:color");
	pmp::VertexProperty<pmp::TexCoord> pmpTexcoords = pmpMesh->vertex_property<pmp::TexCoord>("v:tex");

	osg::ref_ptr<osg::Vec3Array> normals = dynamic_cast<osg::Vec3Array*>(geom->getNormalArray());
	osg::ref_ptr<osg::Vec3Array> colors = dynamic_cast<osg::Vec3Array*>(geom->getColorArray());
	osg::ref_ptr<osg::Vec2Array> texcoords = dynamic_cast<osg::Vec2Array*>(geom->getTexCoordArray(0));
	if (normals->empty()) pmpMesh->remove_vertex_property(pmpNormals);
	if (colors->empty()) pmpMesh->remove_vertex_property(pmpColors);
	if (texcoords->empty()) pmpMesh->remove_vertex_property(pmpTexcoords);

	osg::ref_ptr<osg::Vec3Array> vertices = dynamic_cast<osg::Vec3Array*>(geom->getVertexArray());
    bool withNormals = (normals->size() >= vertices->size());
    bool withColors = (colors->size() >= vertices->size());
    bool withUVs = (texcoords->size() >= vertices->size());
    for (size_t i = 0; i < vertices->size(); ++i)
    {
        const osg::Vec3& v = vertices->at(i);
        pmp::Vertex vec = pmpMesh->add_vertex(pmp::Point(v[0], v[1], v[2]));
        if (withNormals) pmpNormals[vec] = pmp::Normal(normals->at(i).x(), normals->at(i).y(), normals->at(i).z());
        if (withColors) pmpColors[vec] = pmp::Color(colors->at(i).x(), colors->at(i).y(), colors->at(i).z());
        if (withUVs) pmpTexcoords[vec] = pmp::TexCoord(texcoords->at(i).x(), texcoords->at(i).y());
    }

    const std::vector<unsigned int>& indices = collector->getTriangles();
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        std::vector<pmp::Vertex> face;
        face.push_back(pmp::Vertex(indices[i + 0]));
        face.push_back(pmp::Vertex(indices[i + 1]));
        face.push_back(pmp::Vertex(indices[i + 2]));

        try { _mesh->add_face(face); }
        catch (pmp::TopologyException& e)
        {
            OSG_WARN << "[MeshTopology] " << e.what() << std::endl;
        }
    }

	return pmpMesh;
}
