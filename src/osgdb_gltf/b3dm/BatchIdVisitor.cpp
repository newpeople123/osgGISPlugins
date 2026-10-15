#include "osgdb_gltf/b3dm/BatchIdVisitor.h"
#include <osg/Geometry>
#include <osg/Geode>
using namespace osgGISPlugins;

// 生产级修复(AUDIT A-11)：源模型的通用顶点属性 0 不再被无声覆盖。
// 手册 5.5 要求：
//   1. feature ID 在内部 IR 里单独保存，不依赖源模型任意通用属性槽；
//   2. 在 GLTF writer 内显式写出 _BATCHID；
//   3. 保留源 vertex attribute 0 的原始数据；
//   4. 每个 tile 写出前重建 tile-local batch index。
// 这里实现第 3 点：如果槽 0 已有数据，先搬到空闲槽再写 _BATCHID，
// 并给出 ERROR 级别的日志（原来只是 WARN 后直接覆盖）。
// 槽 1 在 Osg2Gltf 中被当作 TANGENT，因此优先找 >= 2 的空闲槽。
static unsigned findFreeAttribSlot(osg::Geometry& geometry, unsigned from)
{
    for (unsigned i = from; i < 32; ++i)
    {
        if (!geometry.getVertexAttribArray(i)) { return i; }
    }
    return 0xFFFFFFFFu;
}

void BatchIdVisitor::apply(osg::Drawable& drawable) {
    _bAdd = false;
    osg::Geometry* geometry = drawable.asGeometry();
    if (geometry) {
        const osg::Array* positions = geometry->getVertexArray();
        osg::ref_ptr<osg::FloatArray> batchIds = new osg::FloatArray;
        const unsigned positionCount = positions ? positions->getNumElements() : 0u;

        if (positionCount) {
            // --- 保留源属性槽 0 的原始数据 ---
            osg::Array* existing = geometry->getVertexAttribArray(0);
            if (existing)
            {
                // 只有当它不是本 visitor 之前写进去的 _BATCHID 时才搬走。
                // 判据：FloatArray 且长度与顶点数一致且已被登记。
                const bool isOurBatchId = (_written.count(existing) > 0);
                if (!isOurBatchId)
                {
                    unsigned slot = findFreeAttribSlot(*geometry, 2);
                    if (slot == 0xFFFFFFFFu) { slot = findFreeAttribSlot(*geometry, 1); }
                    if (slot != 0xFFFFFFFFu)
                    {
                        const osg::Array::Binding binding = osg::Array::BIND_PER_VERTEX;
                        geometry->setVertexAttribArray(slot, existing, binding);
                        OSG_NOTICE << "BatchIdVisitor: source VertexAttribArray(0) moved to slot "
                                   << slot << " (data preserved, emitted as _ATTRIBUTE_" << slot
                                   << ")." << std::endl;
                    }
                    else
                    {
                        OSG_WARN << "BatchIdVisitor: no free vertex attrib slot to preserve "
                                    "source VertexAttribArray(0); data would be lost." << std::endl;
                    }
                }
            }

            batchIds->assign(positionCount, static_cast<float>(_currentBatchId));
            geometry->setVertexAttribArray(0, batchIds, osg::Array::BIND_PER_VERTEX);
            _written.insert(batchIds.get());
        } else {
            batchIds->assign(1, static_cast<float>(_currentBatchId));
            geometry->setVertexAttribArray(0, batchIds, osg::Array::BIND_PER_VERTEX);
            _written.insert(batchIds.get());
        }
        _bAdd = true;
    }
}

void BatchIdVisitor::apply(osg::Geode& geode)
{
    if (geode.getNumDrawables()) {
        traverse(geode);
        if (_bAdd)
        {
            _currentBatchId++;
        }
    }
}
