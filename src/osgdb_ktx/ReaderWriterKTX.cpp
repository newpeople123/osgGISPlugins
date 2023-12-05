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

    bool usePseudo = (ext == "verse_ktx");
    if (usePseudo)
    {
        fileName = osgDB::getNameLessExtension(path);
        ext = osgDB::getFileExtension(fileName);
    }

    std::vector<osg::ref_ptr<osg::Image>> images = osg::loadKtx(fileName);
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
    std::vector<osg::ref_ptr<osg::Image>> images = osg::loadKtx2(fin);
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
            std::string val;

            size_t found = opt.find("=");
            if (found != std::string::npos)
            {
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
    osg::ImageSequence* seq = dynamic_cast<osg::ImageSequence*>(imagePtr);
    std::vector<osg::Image*> imageList;
    if (seq)
    {
        bool useCubemap = false;
        if (options)
        {
            std::string scm = options->getPluginStringData("SavingCubeMap");
            std::transform(scm.begin(), scm.end(), scm.begin(), tolower);
            useCubemap = (scm == "true" || atoi(scm.c_str()) > 0);
        }
#if OSG_VERSION_GREATER_THAN(3, 3, 0)
        for (size_t i = 0; i < seq->getNumImageData(); ++i)
#else
        for (size_t i = 0; i < seq->getNumImages(); ++i)
#endif
            imageList.push_back(seq->getImage(i));

        bool result = osg::saveKtx(fileName, useCubemap, imageList, isKtx2);
        return result ? WriteResult::FILE_SAVED : WriteResult::ERROR_IN_WRITING_FILE;
    }
    else
        imageList.push_back(imagePtr);

    bool result = osg::saveKtx(fileName, false, imageList, isKtx2);
    return result ? WriteResult::FILE_SAVED : WriteResult::ERROR_IN_WRITING_FILE;
}

osgDB::ReaderWriter::WriteResult ReaderWriterKTX::writeImage(const osg::Image& image, std::ostream& fout,
    const Options* options) const
{
    bool isKtx2 = true;
    if (options)
    {
        std::istringstream iss(options->getOptionString());
        std::string opt;
        while (iss >> opt)
        {
            // split opt into pre= and post=
            std::string key;
            std::string val;

            size_t found = opt.find("=");
            if (found != std::string::npos)
            {
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
    osg::ImageSequence* seq = dynamic_cast<osg::ImageSequence*>(imagePtr);
    std::vector<osg::Image*> imageList;
    if (seq)
    {
        bool useCubemap = false;
        if (options)
        {
            std::string scm = options->getPluginStringData("SavingCubeMap");
            std::transform(scm.begin(), scm.end(), scm.begin(), tolower);
            useCubemap = (scm == "true" || atoi(scm.c_str()) > 0);
        }
#if OSG_VERSION_GREATER_THAN(3, 3, 0)
        for (size_t i = 0; i < seq->getNumImageData(); ++i)
#else
        for (size_t i = 0; i < seq->getNumImages(); ++i)
#endif
            imageList.push_back(seq->getImage(i));

        bool result = osg::saveKtx2(fout, useCubemap, imageList, isKtx2);
        return result ? WriteResult::FILE_SAVED : WriteResult::ERROR_IN_WRITING_FILE;
    }
    else
        imageList.push_back(imagePtr);

    bool result = osg::saveKtx2(fout, false, imageList, isKtx2);
    return result ? WriteResult::FILE_SAVED : WriteResult::ERROR_IN_WRITING_FILE;
}


