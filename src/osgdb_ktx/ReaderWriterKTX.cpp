#include <osgdb_ktx/ReaderWriterKTX.h>
#include <osgdb_ktx/LoadTextureKTX.h>
#include <osgViewer/Viewer>
ReaderWriterKTX::ReaderWriterKTX()
{
    supportsExtension("verse_ktx", "osgVerse pseudo-loader");
    supportsExtension("ktx", "KTX texture file");
    supportsOption("SavingCubeMap", "Save KTX cubemap data");
    supportsOption("Version=<value>", "ktx version enum values:1.0、2.0，default value is 2.0");
}

const char* ReaderWriterKTX::className() const
{
    return "[osgVerse] KTX texture reader";
}

osgDB::ReaderWriter::ReadResult ReaderWriterKTX::readImage(const std::string& path, const Options* options) const
{
    std::string fileName(path);
    std::string ext = osgDB::getLowerCaseFileExtension(path);
    if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

    const bool usePseudo = (ext == "verse_ktx");
    if (usePseudo)
    {
        fileName = osgDB::getNameLessExtension(path);
        ext = osgDB::getFileExtension(fileName);
    }

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
    std::string fileName(path);
    std::string ext = osgDB::getLowerCaseFileExtension(path);
    if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

    bool usePseudo = (ext == "verse_ktx");
    if (usePseudo)
    {
        fileName = osgDB::getNameLessExtension(path);
        ext = osgDB::getFileExtension(fileName);
    }
    bool isKtx2 = true;
    if (options)
    {
        std::istringstream iss(options->getOptionString());
        std::string opt;
        while (iss >> opt)
        {
            // split opt into pre= and post=
            std::string key;

            size_t found = opt.find('=');
            if (found != std::string::npos)
            {
	            std::string val;
	            key = opt.substr(0, found);
                val = opt.substr(found + 1);
            }
            else
            {
                key = opt;
            }

            //if (key == "Version") {
            //    if (val != "2.0") {
            //        isKtx2 = false;
            //    }
            //}
        }
    }

    osg::Image* imagePtr = const_cast<osg::Image*>(&image);
    imagePtr->flipVertical();

    bool result = osg::saveKtx(fileName, imagePtr, isKtx2);
    return result ? WriteResult::FILE_SAVED : WriteResult::ERROR_IN_WRITING_FILE;
}

osgDB::ReaderWriter::WriteResult ReaderWriterKTX::writeImage(const osg::Image& image, std::ostream& fout,
    const Options* options) const
{
	const bool isKtx2 = true;
    if (options)
    {
        std::istringstream iss(options->getOptionString());
        std::string opt;
        while (iss >> opt)
        {
            // split opt into pre= and post=
            std::string key;

            const size_t found = opt.find('=');
            if (found != std::string::npos)
            {
	            std::string val;
	            key = opt.substr(0, found);
                val = opt.substr(found + 1);
            }
            else
            {
                key = opt;
            }

            //if (key == "Version") {
            //    if (val != "2.0") {
            //        isKtx2 = false;
            //    }
            //}
        }
    }

    osg::Image* imagePtr = const_cast<osg::Image*>(&image);

	const bool result = osg::saveKtx2(fout, imagePtr, isKtx2);
    return result ? WriteResult::FILE_SAVED : WriteResult::ERROR_IN_WRITING_FILE;
}


