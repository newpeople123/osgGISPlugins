#ifndef OSG_GIS_PLUGINS_GLTF_PROCESSOR_MANAGER_H
#define OSG_GIS_PLUGINS_GLTF_PROCESSOR_MANAGER_H 1
#include "osgdb_gltf/GltfProcessor.h"
#include "osgdb_gltf/compress/GltfDracoCompressor.h"
#include "osgdb_gltf/compress/GltfMeshOptCompressor.h"
#include "osgdb_gltf/compress/GltfMeshQuantizeCompressor.h"
#include "osgdb_gltf/GltfMerger.h"
#include <algorithm>
namespace osgGISPlugins {
    class GltfProcessorManager
    {
    private:
        std::vector<GltfProcessor*> _processors;
    public:
        GltfProcessorManager() {};

        void addProcessor(GltfProcessor* processor)
        {
            // 检查是否已有相同类型的优化器
            for (auto* existingProcessor : _processors)
            {
                if (typeid(*existingProcessor) == typeid(*processor))
                {
                    delete processor; // 避免内存泄漏
                    return;
                }
            }

            // 检查是否要添加的优化器是GltfDracoCompressor类型
            GltfDracoCompressor* dracoCompressor = dynamic_cast<GltfDracoCompressor*>(processor);
            if (dracoCompressor)
            {
                // 如果存在GltfMeshOptCompressor或GltfMeshQuantizeCompressor，拒绝添加GltfDracoCompressor
                for (auto* existingProcessor : _processors)
                {
                    if (dynamic_cast<GltfMeshOptCompressor*>(existingProcessor) || dynamic_cast<GltfMeshQuantizeCompressor*>(existingProcessor))
                    {
                        delete processor; // 避免内存泄漏
                        return;
                    }
                }
            }
            else
            {
                // 如果要添加的是GltfMeshOptCompressor或GltfMeshQuantizeCompressor，且已经存在GltfDracoCompressor，拒绝添加
                for (auto* existingProcessor : _processors)
                {
                    if (dynamic_cast<GltfDracoCompressor*>(existingProcessor))
                    {
                        delete processor; // 避免内存泄漏
                        return;
                    }
                }
            }

            _processors.push_back(processor);

            // 按照指定顺序排序
            std::sort(_processors.begin(), _processors.end(), [](GltfProcessor* a, GltfProcessor* b) {
                if (dynamic_cast<GltfMerger*>(a)) return true;
                if (dynamic_cast<GltfMerger*>(b)) return false;

                if (dynamic_cast<GltfDracoCompressor*>(a)) return true;
                if (dynamic_cast<GltfDracoCompressor*>(b)) return false;

                if (dynamic_cast<GltfMeshQuantizeCompressor*>(a)) return true;
                if (dynamic_cast<GltfMeshQuantizeCompressor*>(b)) return false;

                return dynamic_cast<GltfMeshOptCompressor*>(a) != nullptr;
                });
        }

        void process()
        {
            for (auto* processor : _processors)
            {
                processor->apply();
            }
            if (_processors.size() > 0)
            {
                GltfMerger* mergeProcessor = dynamic_cast<GltfMerger*>(_processors[_processors.size() - 1]);
                if (mergeProcessor)
                    mergeProcessor->mergeBuffers();
            }
        }

        ~GltfProcessorManager()
        {
            for (auto* processor : _processors)
            {
                delete processor;
            }
            _processors.clear();
        }
    };
}
#endif // !OSG_GIS_PLUGINS_GLTF_PROCESSOR_MANAGER_H
