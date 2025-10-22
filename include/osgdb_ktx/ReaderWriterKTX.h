/* -*-c++-*- OpenSceneGraph - Copyright (C) 2012 APX Labs, LLC
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/
#include <osg/io_utils>
#include <osg/Version>
#include <osg/Image>
#include <osg/ImageSequence>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/Registry>


class ReaderWriterKTX : public osgDB::ReaderWriter
{
public:

    ReaderWriterKTX();

    const char* className() const override;

    ReadResult readObject(std::istream& fin, const osgDB::ReaderWriter::Options* options) const override
    {
        return readImage(fin, options);
    }

    ReadResult readImage(std::istream& fin,const osgDB::ReaderWriter::Options* = nullptr) const override;

    ReadResult readObject(const std::string& file, const osgDB::ReaderWriter::Options* options) const override
    {
        return readImage(file, options);
    }

    ReadResult readImage(const std::string& file, const Options* options) const override;
    WriteResult writeImage(const osg::Image &image, const std::string& file, const osgDB::ReaderWriter::Options* options) const override;
    WriteResult writeImage(const osg::Image& image, std::ostream& fout, const Options* options) const override;
};
REGISTER_OSGPLUGIN(ktx, ReaderWriterKTX)
