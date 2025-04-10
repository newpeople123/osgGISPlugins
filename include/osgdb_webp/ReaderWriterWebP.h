#pragma once
/* -*-c++-*- */
/* osgEarth - Geospatial SDK for OpenSceneGraph
 * Copyright 2020 Pelican Mapping
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <osgDB/Registry>

#include <string>
#include <iomanip>

#include "webp/encode.h"

#if TARGET_OS_IPHONE
#include "strings.h"
#define stricmp strcasecmp
#endif

using namespace osg;

class ReaderWriterWebP : public osgDB::ReaderWriter
{
public:
    ReaderWriterWebP();

    ~ReaderWriterWebP() override;

    const char* className() const override;

    ReadResult readObject(const std::string& file, const osgDB::ReaderWriter::Options* options) const override;

    ReadResult readObject(std::istream& fin, const Options* options) const override;

    ReadResult readImage(const std::string& file, const osgDB::ReaderWriter::Options* options) const override;

    ReadResult readImage(std::istream& fin, const Options* options) const override;

    WriteResult writeObject(const osg::Object& object, const std::string& file, const osgDB::ReaderWriter::Options* options) const override;

    WriteResult writeObject(const osg::Object& object, std::ostream& fout, const Options* options) const override;

    WriteResult writeImage(const osg::Image& img, const std::string& fileName, const osgDB::ReaderWriter::Options* options) const override;

    static int ostream_writer(const uint8_t* data, size_t data_size,
        const WebPPicture* pic);

    WriteResult writeImage(const osg::Image& img, std::ostream& fout, const Options* options) const override;
};
