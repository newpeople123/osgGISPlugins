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

    virtual const char* className() const;

    virtual ReadResult readObject(std::istream& fin, const osgDB::ReaderWriter::Options* options) const
    {
        return readImage(fin, options);
    }
    
    virtual ReadResult readImage(std::istream& fin,const osgDB::ReaderWriter::Options* =NULL) const;

    virtual ReadResult readObject(const std::string& file, const osgDB::ReaderWriter::Options* options) const
    {
        return readImage(file, options);
    }
    virtual ReadResult readImage(const std::string& path, const Options* options) const;
    virtual WriteResult writeImage(const osg::Image &image, const std::string& file, const osgDB::ReaderWriter::Options* options) const;
    virtual WriteResult writeImage(const osg::Image& image, std::ostream& fout, const Options* options) const;
};
REGISTER_OSGPLUGIN(ktx, ReaderWriterKTX)