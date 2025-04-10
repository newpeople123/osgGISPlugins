#ifndef OSG_GIS_PLUGINS_OSG2GLTF_H
#define OSG_GIS_PLUGINS_OSG2GLTF_H 1
#include <osg/NodeVisitor>
#include "material/GltfPbrMRMaterial.h"
#include "compress/GltfCompressor.h"
#include <stack>
#include <osg/PrimitiveSet>

namespace osgGISPlugins {
    class Osg2Gltf :public osg::NodeVisitor {
        typedef std::map<osg::ref_ptr<const osg::Node>, int> OsgNodeSequenceMap;
        typedef std::map<osg::ref_ptr<const osg::BufferData>, int> ArraySequenceMap;
        typedef std::map<osg::ref_ptr<const osg::Array>, int> AccessorSequenceMap;
        typedef std::vector<osg::ref_ptr<osg::StateSet>> StateSetStack;

        std::vector<osg::ref_ptr<osg::Texture>> _textures;

        std::stack<tinygltf::Node*> _gltfNodeStack;
        OsgNodeSequenceMap _osgNodeSeqMap;
        ArraySequenceMap _buffers;
        ArraySequenceMap _bufferViews;
        ArraySequenceMap _accessors;
        StateSetStack _ssStack;
        osg::ref_ptr<osg::Geode> _cacheGeode = nullptr;
        int _vco = 1;
        double _ratio = 1.0;
        std::vector<std::string> _imgNames;
        tinygltf::Model _model;

        void push(tinygltf::Node& gnode);

        void pop();

        bool pushStateSet(osg::StateSet* stateSet);

        void popStateSet();

        static unsigned getBytesInDataType(GLenum dataType);

        static unsigned getBytesPerElement(const osg::Array* data);

        static unsigned getBytesPerElement(const osg::DrawElements* data);

        int getOrCreateBuffer(const osg::BufferData* data);

        int getOrCreateBufferView(const osg::BufferData* data, GLenum target);

        int getOrCreateAccessor(const osg::Array* data, osg::PrimitiveSet* pset, tinygltf::Primitive& prim, const std::string& attr);

        void setPositionAccessor(const osg::Array* data, osg::PrimitiveSet* pset, tinygltf::Primitive& prim, tinygltf::Accessor& accessor);

        int getCurrentMaterial();

        int getOrCreateTexture(const osg::ref_ptr<osg::Texture>& osgTexture);

        int getOsgTexture2Material(tinygltf::Material& gltfMaterial, const osg::ref_ptr<osg::Texture>& osgTexture);

        int getOsgMaterial2Material(tinygltf::Material& gltfMaterial, const osg::ref_ptr<GltfMaterial>& osgGltfMaterial);

        int getCurrentMaterial(const tinygltf::Material& gltfMaterial) const;

        void apply(osg::Node& node) override;

        void apply(osg::Group& group) override;

        void apply(osg::MatrixTransform& xform) override;

        void apply(osg::Drawable& drawable) override;

        static void flipGltfTextureYAxis(KHR_texture_transform& texture_transform_extension);
    public:
        META_NodeVisitor(osgGISPlugins, Osg2Gltf)

        Osg2Gltf();

        tinygltf::Model getGltfModel();

        ~Osg2Gltf() override;
    };
}

#endif // !OSG_GIS_PLUGINS_OSG2GLTF_H
