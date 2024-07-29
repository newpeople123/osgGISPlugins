#ifndef OSG_GIS_PLUGINS_B3DM_H
#define OSG_GIS_PLUGINS_B3DM_H 1
struct B3DMHeader {
    char magic[4] = { 'b', '3', 'd', 'm' };
    uint32_t version = 1;
    uint32_t byteLength = 0;
    uint32_t featureTableJSONByteLength = 0;
    uint32_t featureTableBinaryByteLength = 0;
    uint32_t batchTableJSONByteLength = 0;
    uint32_t batchTableBinaryByteLength = 0;
};

struct B3DMFile {
    B3DMHeader header;
    std::string featureTableJSON;
    std::string featureTableBinary;
    std::string batchTableJSON;
    std::string batchTableBinary;
    std::string glbData;
    int glbPadding = 0;
    int totalPadding = 0;

    void calculateHeaderSizes() {
        header.featureTableJSONByteLength = featureTableJSON.size();
        header.featureTableBinaryByteLength = featureTableBinary.size();
        header.batchTableJSONByteLength = batchTableJSON.size();
        header.batchTableBinaryByteLength = batchTableBinary.size();
        glbPadding = 4 - (glbData.size() % 4);
        if (glbPadding == 4) glbPadding = 0;
        totalPadding = 8 - ((28 + featureTableJSON.size() + batchTableJSON.size() + glbData.size() + glbPadding) % 8);
        if (totalPadding == 8) totalPadding = 0;
        header.byteLength = 28 + featureTableJSON.size() + batchTableJSON.size() + glbData.size() + glbPadding + totalPadding;
    }
};

#endif // !OSG_GIS_PLUGINS_B3DM_H
