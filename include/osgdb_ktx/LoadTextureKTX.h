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
#include <osg/Texture2D>
#include <iterator>
#include <ktx.h>

namespace osg
{
    std::vector<osg::ref_ptr<osg::Image>> loadKtx(const std::string& file);
    std::vector<osg::ref_ptr<osg::Image>> loadKtx2(std::istream& in);

    bool saveKtx2(const std::string& file,const osg::Image* image, bool compressed = true);
    bool saveKtx2(std::ostream& out,const osg::Image* image, bool compressed = true);
    bool saveKtx1(const std::string& file, const osg::Image* image);
    bool saveKtx1(std::ostream& out, const osg::Image* image);
}
