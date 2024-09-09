#ifndef OSG_GIS_PLUGINS_B3DM_H
#define OSG_GIS_PLUGINS_B3DM_H 1
#include <osg/Vec3>
namespace osgGISPlugins {
    struct Base3DModelHeader {
        char magic[4];                        // 文件的magic标志
        uint32_t version = 1;                 // 版本号
        uint32_t byteLength = 0;              // 文件总长度
        uint32_t featureTableJSONByteLength = 0; // featureTable JSON的长度
        uint32_t featureTableBinaryByteLength = 0; // featureTable 二进制数据的长度
        uint32_t batchTableJSONByteLength = 0;    // batchTable JSON的长度
        uint32_t batchTableBinaryByteLength = 0;  // batchTable 二进制数据的长度

        Base3DModelHeader(const char* magicChars) {
            std::copy(magicChars, magicChars + 4, magic);
        }
    };

    struct Base3DModelFile {
        Base3DModelHeader header;
        std::string featureTableJSON;
        std::string featureTableBinary;
        std::string batchTableJSON;
        std::string batchTableBinary;
        std::string glbData;
        int glbPadding = 0;
        int totalPadding = 0;

        Base3DModelFile(const char* magicChars) : header(magicChars) {}

        virtual int headerSize() const = 0; // 子类实现，返回头部的大小

        void calculateHeaderSizes() {
            header.featureTableJSONByteLength = featureTableJSON.size();
            header.featureTableBinaryByteLength = featureTableBinary.size();
            header.batchTableJSONByteLength = batchTableJSON.size();
            header.batchTableBinaryByteLength = batchTableBinary.size();
            glbPadding = 4 - (glbData.size() % 4);
            if (glbPadding == 4) glbPadding = 0;
            totalPadding = 8 - ((headerSize() + featureTableJSON.size() + batchTableJSON.size() + glbData.size() + glbPadding) % 8);
            if (totalPadding == 8) totalPadding = 0;
            header.byteLength = headerSize() + featureTableJSON.size() + batchTableJSON.size() + glbData.size() + glbPadding + totalPadding;
        }
    };

    struct B3DMFile : public Base3DModelFile {
        B3DMFile() : Base3DModelFile("b3dm") {}

        int headerSize() const override {
            return 28;
        }
    };

    struct I3DMFile : public Base3DModelFile {
        uint32_t gltfFormat = 1;

        I3DMFile() : Base3DModelFile("i3dm") {}

        int headerSize() const override {
            return 32; 
        }

        static std::pair<int, int> encodeNormalToOct32P(const osg::Vec3f& normal) {
            float absSum = std::fabs(normal.x()) + std::fabs(normal.y()) + std::fabs(normal.z());
            osg::Vec2f octNormal = normal.x() == 0 && normal.y() == 0 ? osg::Vec2f(0, 0) :
                osg::Vec2f(normal.x(), normal.y()) * (1.0f / absSum);
            if (normal.z() < 0) {
                octNormal.set(
                    (1 - std::fabs(octNormal.y())) * (octNormal.x() < 0 ? -1.0f : 1.0f),
                    (1 - std::fabs(octNormal.x())) * (octNormal.y() < 0 ? -1.0f : 1.0f)
                );
            }
            int x = static_cast<int>((octNormal.x() * 0.5f + 0.5f) * 65535);
            int y = static_cast<int>((octNormal.y() * 0.5f + 0.5f) * 65535);
            return std::make_pair(x, y);
        }
    };

}
#endif // !OSG_GIS_PLUGINS_B3DM_H
