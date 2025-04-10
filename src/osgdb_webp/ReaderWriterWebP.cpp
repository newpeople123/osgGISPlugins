#include <osgdb_webp/ReaderWriterWebP.h>
#include <osgDB/ConvertUTF>
#include <osgDB/FileNameUtils>
#include <decode.h>
#include <osgDB/FileUtils>

ReaderWriterWebP::ReaderWriterWebP()
{
    supportsExtension("webp", "WebP image format");
}

ReaderWriterWebP::~ReaderWriterWebP()
{
}

const char* ReaderWriterWebP::className() const { return "Google WebP Image Reader/Writer"; }

osgDB::ReaderWriter::ReadResult ReaderWriterWebP::readObject(const std::string& file, const osgDB::ReaderWriter::Options* options) const
{
    return readImage(file, options);
}

osgDB::ReaderWriter::ReadResult ReaderWriterWebP::readObject(std::istream& fin, const Options* options) const
{
    return readImage(fin, options);
}

osgDB::ReaderWriter::ReadResult ReaderWriterWebP::readImage(const std::string& file, const osgDB::ReaderWriter::Options* options) const
{
    const std::string ext = osgDB::getFileExtension(file);
    if (!acceptsExtension(ext))
        return ReadResult::FILE_NOT_HANDLED;

    const std::string fileName = osgDB::findDataFile(file, options);
    if (fileName.empty())
        return ReadResult::FILE_NOT_FOUND;

    osgDB::ifstream f(file.c_str(), std::ios_base::in | std::ios_base::binary);
    if (!f)
        return ReadResult::FILE_NOT_HANDLED;

    ReadResult rr = readImage(f, options);

    if (rr.validImage())
        rr.getImage()->setFileName(file);
    return rr;
}

osgDB::ReaderWriter::ReadResult ReaderWriterWebP::readImage(std::istream& fin, const Options* options) const
{
    osg::Image* image = NULL;

    fin.seekg(0, std::ios::end);
    size_t stream_size = fin.tellg();
    fin.seekg(0, std::ios::beg);

    if (stream_size > 0)
    {
	    char* vp8_buffer = NULL;
	    unsigned long size_of_vp8_image_data = 0;

	    size_of_vp8_image_data = stream_size;
        vp8_buffer = new char[stream_size];

        int version = WebPGetEncoderVersion();
        size_of_vp8_image_data = fin.read(vp8_buffer, size_of_vp8_image_data).gcount();

        WebPDecoderConfig config;
        WebPInitDecoderConfig(&config);
        int status = WebPGetFeatures(reinterpret_cast<const uint8_t*>(vp8_buffer), static_cast<uint32_t>(size_of_vp8_image_data), &config.input);
        if (status == VP8_STATUS_OK)
        {

            unsigned int pixelFormat;
            unsigned int dataType = GL_UNSIGNED_BYTE;

            if (config.input.has_alpha)
            {
                pixelFormat = GL_RGBA;
                config.output.colorspace = MODE_RGBA;
            }
            else
            {
                /*
                  BUG of AMD driver: http://devgurus.amd.com/message/1302663
                  pixelFormat = GL_RGB;
                  config.output.colorspace = MODE_RGB;
                  */
                pixelFormat = GL_RGBA;
                config.output.colorspace = MODE_RGBA;
            }
            int internalFormat = pixelFormat;
            int r = 1;

            image = new osg::Image();
            image->allocateImage(config.input.width, config.input.height, 1, pixelFormat, dataType);

            config.output.u.RGBA.rgba = (uint8_t*)image->data();
            config.output.u.RGBA.stride = image->getRowSizeInBytes();
            config.output.u.RGBA.size = image->getImageSizeInBytes();

            config.options.no_fancy_upsampling = 1;

            config.output.is_external_memory = 1;

            status = WebPDecode(reinterpret_cast<const uint8_t*>(vp8_buffer), static_cast<uint32_t>(size_of_vp8_image_data), &config);
        }
        delete[] vp8_buffer;
    }
    else
    {
        OSG_NOTICE << "read webp image: stream size is zero" << std::endl;
    }

    if (image)
    {
        image->flipVertical();
    }

    return image;
}

osgDB::ReaderWriter::WriteResult ReaderWriterWebP::writeObject(const osg::Object& object, const std::string& file, const osgDB::ReaderWriter::Options* options) const
{
    const osg::Image* image = dynamic_cast<const osg::Image*>(&object);
    if (!image)
        return WriteResult::FILE_NOT_HANDLED;

    return writeImage(*image, file, options);
}

osgDB::ReaderWriter::WriteResult ReaderWriterWebP::writeObject(const osg::Object& object, std::ostream& fout, const Options* options) const
{
    const osg::Image* image = dynamic_cast<const osg::Image*>(&object);
    if (!image)
        return WriteResult::FILE_NOT_HANDLED;

    return writeImage(*image, fout, options);
}

osgDB::ReaderWriter::WriteResult ReaderWriterWebP::writeImage(const osg::Image& img, const std::string& fileName, const osgDB::ReaderWriter::Options* options) const
{
    const std::string ext = osgDB::getFileExtension(fileName);
    if (!acceptsExtension(ext))
        return WriteResult::FILE_NOT_HANDLED;

    int internalFormat = osg::Image::computeNumComponents(img.getPixelFormat());

    osgDB::ofstream f(fileName.c_str(), std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    if (!f)
        return WriteResult::ERROR_IN_WRITING_FILE;

    return writeImage(img, f, options);
}

int ReaderWriterWebP::ostream_writer(const uint8_t* data, const size_t data_size,
    const WebPPicture* const pic)
{

    std::ostream* const out = static_cast<std::ostream*>(pic->custom_ptr);
    return data_size ? static_cast<int>(out->write(reinterpret_cast<const char*>(data), data_size).tellp()) : 1;
}

osgDB::ReaderWriter::WriteResult ReaderWriterWebP::writeImage(const osg::Image& img, std::ostream& fout, const Options* options) const
{
    int internalFormat = osg::Image::computeNumComponents(img.getPixelFormat());

    osg::ref_ptr< osg::Image > flippedImage = new osg::Image(img);
    flippedImage->flipVertical();

    WebPConfig config;
    config.quality = 75;
    config.method = 2;

    if (options)
    {
        std::istringstream iss(options->getOptionString());
        std::string opt;
        while (iss >> opt)
        {
            if (strcmp(opt.c_str(), "lossless") == 0)
            {
                config.lossless = 1;
                config.quality = 100;
            }
            else if (strcmp(opt.c_str(), "hint") == 0)
            {
                std::string v;
                iss >> v;
                if (strcmp(v.c_str(), "picture") == 0)
                {
                    config.image_hint = WEBP_HINT_PICTURE;
                }
                else if (strcmp(v.c_str(), "photo") == 0)
                {
                    config.image_hint = WEBP_HINT_PHOTO;
                }
                else if (strcmp(v.c_str(), "graph") == 0)
                {
                    config.image_hint = WEBP_HINT_GRAPH;
                }
            }
            else if (strcmp(opt.c_str(), "quality") == 0)
            {
                float v;
                iss >> v;
                if (v >= 0.0 && v <= 100.0)
                {
                    config.quality = v;
                }
            }
            else if (strcmp(opt.c_str(), "method") == 0)
            {
                int v;
                iss >> v;
                if (v >= 0 && v <= 6)
                {
                    config.method = v;
                }
            }
        }
    }

    WebPPicture picture;
    WebPPreset preset;

    if (!WebPPictureInit(&picture) || !WebPConfigInit(&config))
    {
        return WriteResult::ERROR_IN_WRITING_FILE;
    }

    picture.width = img.s();
    picture.height = img.t();
    int  bb = img.getRowSizeInBytes();
    switch (img.getPixelFormat())
    {
    case (GL_RGB):
        WebPPictureImportRGB(&picture, flippedImage->data(), img.getRowSizeInBytes());
        break;
    case (GL_RGBA):
        WebPPictureImportRGBA(&picture, flippedImage->data(), img.getRowSizeInBytes());
        break;
    case (GL_LUMINANCE):
        WebPPictureImportRGBX(&picture, flippedImage->data(), img.getRowSizeInBytes());
        
        break;
    default:
        return WriteResult::ERROR_IN_WRITING_FILE;
    }

    switch (img.getPixelFormat())
    {
    case (GL_RGB):
    case (GL_RGBA):
        preset = WEBP_PRESET_DEFAULT;
        if (!WebPConfigPreset(&config, preset, config.quality))
        {
            return WriteResult::ERROR_IN_WRITING_FILE;
        }

        if (!WebPValidateConfig(&config))
        {
            return WriteResult::ERROR_IN_WRITING_FILE;
        }

        picture.writer = ReaderWriterWebP::ostream_writer;
        picture.custom_ptr = static_cast<void*>(&fout);
        if (!WebPEncode(&config, &picture))
        {
            return WriteResult::ERROR_IN_WRITING_FILE;
        }
        break;
    case (GL_LUMINANCE):
        preset = WEBP_PRESET_DEFAULT;
        if (!WebPConfigPreset(&config, preset, config.quality))
        {
            return WriteResult::ERROR_IN_WRITING_FILE;
        }
        config.lossless = 1;

        if (!WebPValidateConfig(&config))
        {
            return WriteResult::ERROR_IN_WRITING_FILE;
        }

        picture.writer = ReaderWriterWebP::ostream_writer;
        picture.custom_ptr = static_cast<void*>(&fout);
        if (!WebPEncode(&config, &picture))
        {
            return WriteResult::ERROR_IN_WRITING_FILE;
        }
        break;

    default:
        return WriteResult::ERROR_IN_WRITING_FILE;
    }

    WebPPictureFree(&picture);
    return WriteResult::FILE_SAVED;
}

// now register with Registry to instantiate the above
// reader/writer.
REGISTER_OSGPLUGIN(webp, ReaderWriterWebP)
