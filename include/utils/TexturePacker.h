// MIT License
//
// Copyright (c) 2023 Wang Rui
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#ifndef OSG_GIS_PLUGINS_TEXTUREPACKER_H
#define OSG_GIS_PLUGINS_TEXTUREPACKER_H
#include <osg/Image>
#include <osg/observer_ptr>
namespace osgGISPlugins {
    class TexturePacker :public osg::Referenced
    {
    public:
        TexturePacker(int maxW, int maxH) : _maxWidth(maxW), _maxHeight(maxH), _dictIndex(0) {}
        void setMaxSize(int w, int h) { _maxWidth = w; _maxHeight = h; }
        void clear();

        size_t addElement(osg::ref_ptr<osg::Image> image);
        size_t addElement(int width, int height);
        void removeElement(size_t id);

        osg::Image* pack(size_t& numImages, bool generateResult, bool stopIfFailed = true);
        bool getPackingData(size_t id, double& x, double& y, int& w, int& h);
        size_t getId(osg::Image* image) const;
    protected:
        typedef std::pair<osg::observer_ptr<osg::Image>, osg::Vec4> InputPair;
        std::map<size_t, InputPair> _input, _result;
        int _maxWidth, _maxHeight, _dictIndex;
    };
}
#endif // OSG_GIS_PLUGINS_TEXTUREPACKER_H
