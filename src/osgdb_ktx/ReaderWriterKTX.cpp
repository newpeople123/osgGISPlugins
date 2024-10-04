#include <osgdb_ktx/ReaderWriterKTX.h>
#include <osgdb_ktx/LoadTextureKTX.h>
#include <osgDB/ConvertUTF>
ReaderWriterKTX::ReaderWriterKTX()
{
    supportsExtension("ktx2", "KTX texture 2.0 file");
    supportsExtension("ktx", "KTX texture 1.0 file");
}

const char* ReaderWriterKTX::className() const
{
    return "KTX texture reader";
}

osgDB::ReaderWriter::ReadResult ReaderWriterKTX::readImage(const std::string& path, const Options* options) const
{
    std::string fileName = osgDB::convertStringFromUTF8toCurrentCodePage(path.c_str());;
    std::string ext = osgDB::getLowerCaseFileExtension(path);
    if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

    const std::vector<osg::ref_ptr<osg::Image>> images = osg::loadKtx(fileName);
    if (images.size() > 1)
    {
        osg::ref_ptr<osg::ImageSequence> seq = new osg::ImageSequence;
        for (size_t i = 0; i < images.size(); ++i) seq->addImage(images[i]);
        return seq;
    }
    return images.empty() ? ReadResult::ERROR_IN_READING_FILE : ReadResult(images[0]);
}

osgDB::ReaderWriter::ReadResult ReaderWriterKTX::readImage(std::istream& fin, const Options*) const
{
    const std::vector<osg::ref_ptr<osg::Image>> images = osg::loadKtx2(fin);
    if (images.size() > 1)
    {
        osg::ref_ptr<osg::ImageSequence> seq = new osg::ImageSequence;
        for (size_t i = 0; i < images.size(); ++i) seq->addImage(images[i]);
        return seq;
    }
    return images.empty() ? ReadResult::ERROR_IN_READING_FILE : ReadResult(images[0]);
}

osgDB::ReaderWriter::WriteResult ReaderWriterKTX::writeImage(const osg::Image& image, const std::string& path,
    const Options* options) const
{
    std::string fileName = osgDB::convertStringFromUTF8toCurrentCodePage(path.c_str());;
    std::string ext = osgDB::getLowerCaseFileExtension(path);
    if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

    osg::Image* imagePtr = const_cast<osg::Image*>(&image);
    imagePtr->flipVertical();
    if (ext == "ktx2") {
        const bool result = osg::saveKtx2(fileName, imagePtr, true);
        return result ? WriteResult::FILE_SAVED : WriteResult::ERROR_IN_WRITING_FILE;
    }
    else {
        const bool result = osg::saveKtx1(fileName, imagePtr);
        return result ? WriteResult::FILE_SAVED : WriteResult::ERROR_IN_WRITING_FILE;
    }
}

osgDB::ReaderWriter::WriteResult ReaderWriterKTX::writeImage(const osg::Image& image, std::ostream& fout,
    const Options* options) const
{
    osg::Image* imagePtr = const_cast<osg::Image*>(&image);
    const bool result = osg::saveKtx2(fout, imagePtr, true);
    return result ? WriteResult::FILE_SAVED : WriteResult::ERROR_IN_WRITING_FILE;
}


