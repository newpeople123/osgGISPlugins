#ifndef OSG_GIS_PLUGINS_GLTF_OPTIMIZER_MANAGER_H
#define OSG_GIS_PLUGINS_GLTF_OPTIMIZER_MANAGER_H 1
#include "osgdb_gltf/GltfOptimizer.h"
#include "osgdb_gltf/compress/GltfDracoCompressor.h"
#include "osgdb_gltf/compress/GltfMeshOptCompressor.h"
#include "osgdb_gltf/compress/GltfMeshQuantizeCompressor.h"
#include "osgdb_gltf/GltfMerger.h"
#include <algorithm>

class GltfOptimizerManager
{
private:
    std::vector<GltfOptimizer*> _optimizers;
public:
    GltfOptimizerManager() {};

    void addOptimizer(GltfOptimizer* optimizer)
    {
        // 检查是否已有相同类型的优化器
        for (auto* existingOptimizer : _optimizers)
        {
            if (typeid(*existingOptimizer) == typeid(*optimizer))
            {
                delete optimizer; // 避免内存泄漏
                return;
            }
        }

        // 检查是否要添加的优化器是GltfDracoCompressor类型
        GltfDracoCompressor* dracoCompressor = dynamic_cast<GltfDracoCompressor*>(optimizer);
        if (dracoCompressor)
        {
            // 如果存在GltfMeshOptCompressor或GltfMeshQuantizeCompressor，拒绝添加GltfDracoCompressor
            for (auto* existingOptimizer : _optimizers)
            {
                if (dynamic_cast<GltfMeshOptCompressor*>(existingOptimizer) || dynamic_cast<GltfMeshQuantizeCompressor*>(existingOptimizer))
                {
                    delete optimizer; // 避免内存泄漏
                    return;
                }
            }
        }
        else
        {
            // 如果要添加的是GltfMeshOptCompressor或GltfMeshQuantizeCompressor，且已经存在GltfDracoCompressor，拒绝添加
            for (auto* existingOptimizer : _optimizers)
            {
                if (dynamic_cast<GltfDracoCompressor*>(existingOptimizer))
                {
                    delete optimizer; // 避免内存泄漏
                    return;
                }
            }
        }

        _optimizers.push_back(optimizer);

        // 按照指定顺序排序
        std::sort(_optimizers.begin(), _optimizers.end(), [](GltfOptimizer* a, GltfOptimizer* b) {
            if (dynamic_cast<GltfMerger*>(a)) return true;
            if (dynamic_cast<GltfMerger*>(b)) return false;

            if (dynamic_cast<GltfDracoCompressor*>(a)) return true;
            if (dynamic_cast<GltfDracoCompressor*>(b)) return false;

            if (dynamic_cast<GltfMeshQuantizeCompressor*>(a)) return true;
            if (dynamic_cast<GltfMeshQuantizeCompressor*>(b)) return false;

            return dynamic_cast<GltfMeshOptCompressor*>(a) != nullptr;
            });
    }

    void optimize()
    {
        for (auto* optimizer : _optimizers)
        {
            optimizer->apply();
        }
        if (_optimizers.size() > 0)
        {
            GltfMerger* mergeOptimizer = dynamic_cast<GltfMerger*>(_optimizers[0]);
            if (mergeOptimizer)
                mergeOptimizer->mergeBuffers();
        }
    }

    ~GltfOptimizerManager()
    {
        for (auto* optimizer : _optimizers)
        {
            delete optimizer;
        }
        _optimizers.clear();
    }
};
#endif // !OSG_GIS_PLUGINS_GLTF_OPTIMIZER_MANAGER_H
