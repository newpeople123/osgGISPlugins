#include "utils/TexturePacker.h"
#include <osg/io_utils>
#include <osg/ImageUtils>
#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>
using namespace osgGISPlugins;
void TexturePacker::clear() {
    _input.clear();
    _result.clear();
    _dictIndex = 0;
}

size_t TexturePacker::addElement(osg::Image* image) {
    _input[++_dictIndex] = InputPair(image, osg::Vec4());
    return _dictIndex;
}

size_t TexturePacker::addElement(int w, int h) {
    _input[++_dictIndex] = InputPair(NULL, osg::Vec4(0, 0, w, h));
    return _dictIndex;
}

void TexturePacker::removeElement(size_t id) {
    if (_input.find(id) != _input.end())
        _input.erase(_input.find(id));
}

osg::Image* TexturePacker::pack(size_t& numImages, bool generateResult, bool stopIfFailed)
{
    //纹理打包上下文
    stbrp_context context;
    //初始化指针、总宽度和总高度
    int ptr = 0, totalW = 0, totalH = 0;
    //计算最大的尺寸，是_maxWidth和_maxHeight的两倍
    int maxSize = osg::maximum(_maxWidth, _maxHeight) * 2;
    //为存储节点分配内存
    stbrp_node* nodes = (stbrp_node*)malloc(sizeof(stbrp_node) * maxSize);
    //初始化节点内存为零
    if (nodes) memset(nodes, 0, sizeof(stbrp_node) * maxSize);

    stbrp_rect* rects = (stbrp_rect*)malloc(sizeof(stbrp_rect) * _input.size());
    for (std::map<size_t, InputPair>::iterator itr = _input.begin(); itr != _input.end(); ++itr, ++ptr) {
        stbrp_rect& r = rects[ptr];
        InputPair& pair = itr->second;
        //设置矩形的id
        r.id = itr->first;
        //初始化矩形的打包状态为未打包
        r.was_packed = 0;
        //设置矩形的初始位置
        r.x = 0;
        //设置矩形的宽度
        r.w = pair.first.valid() ? pair.first->s() : pair.second[2];
        //设置矩形的初始位置
        r.y = 0;
        //设置矩形的高度
        r.h = pair.first.valid() ? pair.first->t() : pair.second[3];
    }

    //初始化打包目标区域
    stbrp_init_target(&context, _maxWidth, _maxHeight, nodes, maxSize);
    //进行打包操作
    stbrp_pack_rects(&context, rects, _input.size());
    //释放节点
    free(nodes);
    //清除结果集并重置指针
    _result.clear();
    ptr = 0;

    //遍历打包后的矩形，检查每个矩形是否成功打包
    //计算打包后的总宽度和总高度
    //将打包结果存储到_result中
    //释放矩形内存，并设置numImages为结果的大小
    //如果generateResult为false，则返回NULL
    osg::observer_ptr<osg::Image> validChild;
    for (std::map<size_t, InputPair>::iterator itr = _input.begin(); itr != _input.end(); ++itr, ++ptr) {
        InputPair& pair = itr->second;
        stbrp_rect& r = rects[ptr];
        osg::Vec4 v(r.x, r.y, r.w, r.h);

        if (r.id != itr->first || !r.was_packed) {
            OSG_INFO << "[TexturePacker] Bad packing element: " << pair.first->getFileName()
                << ", order = " << ptr << "/" << _input.size() << ", rect = " << v
                << ", packed = " << r.was_packed << std::endl;
            if (stopIfFailed) return NULL;
            else continue;
        }

        if (totalW < (r.x + r.w)) totalW = r.x + r.w;
        if (totalH < (r.y + r.h)) totalH = r.y + r.h;
        _result[itr->first] = std::make_pair(pair.first, v);
        if (pair.first.valid()) validChild = pair.first;
    }
    free(rects);
    numImages = _result.size();
    if (!generateResult) return NULL;

    osg::ref_ptr<osg::Image> total = new osg::Image;
    if (validChild.valid()) {
        total->allocateImage(totalW, totalH, 1,
            validChild->getPixelFormat(), validChild->getDataType());
        total->setInternalTextureFormat(validChild->getInternalTextureFormat());
    }
    else
        total->allocateImage(totalW, totalH, 1, GL_RGBA, GL_UNSIGNED_BYTE);

    for (std::map<size_t, InputPair>::iterator itr = _result.begin();
        itr != _result.end(); ++itr)
    {
        InputPair& pair = itr->second;
        const osg::Vec4& r = itr->second.second;
        if (!pair.first.valid()) continue;

        if (!osg::copyImage(pair.first.get(), 0, 0, 0, r[2], r[3], 1,
            total.get(), r[0], r[1], 0))
        {
            OSG_WARN << "[TexturePacker] Failed to copy image " << itr->first << std::endl;
        }
    }
    return total.release();
}

bool TexturePacker::getPackingData(size_t id, double& x, double& y, int& w, int& h)
{
    if (_result.find(id) != _result.end()) {
        const osg::Vec4& rect = _result[id].second;
        x = rect[0];
        y = rect[1];
        w = rect[2];
        h = rect[3];
        return true;
    }
    return false;
}

size_t TexturePacker::getId(osg::Image* image) const
{
    for (const auto& entry : _input)
    {
        if (entry.second.first.get() == image ||
            (entry.second.first->getName() == image->getName() &&
                entry.second.first->getFileName() == image->getFileName() &&
                entry.second.first->s() == image->s() &&
                entry.second.first->t() == image->t() &&
                entry.second.first->r() == image->r() &&
                entry.second.first->getInternalTextureFormat() == image->getInternalTextureFormat()
                )
            )
            return entry.first;
    }
    return 0;
}
