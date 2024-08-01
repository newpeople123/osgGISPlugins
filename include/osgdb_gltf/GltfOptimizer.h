#ifndef OSG_GIS_PLUGINS_GLTF_OPTIMIZER_H
#define OSG_GIS_PLUGINS_GLTF_OPTIMIZER_H 1
class GltfOptimizer
{
protected:
	tinygltf::Model& _model;
public:
	GltfOptimizer(tinygltf::Model& model) :_model(model) {}
	virtual void apply() = 0;
};
#endif // !OSG_GIS_PLUGINS_GLTF_OPTIMIZER_H
