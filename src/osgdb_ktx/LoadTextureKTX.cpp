#include <osg/io_utils>
#include <osg/Version>
#include <osg/AnimationPath>
#include <osg/Texture2D>
#include <osg/Geometry>
#include <osgDB/ConvertUTF>
#include <osgDB/FileNameUtils>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <ktx/texture.h>
#include <ktx/gl_format.h>
#include <osgdb_ktx/LoadTextureKTX.h>
#include <thread>
#include <algorithm>
#include <cmath>

inline VkFormat glGetVkFormatFromInternalFormat(GLint glFormat)
{
    switch (glFormat)
    {
        //
        // 8 bits per component
        //
    case GL_R8: return VK_FORMAT_R8_UNORM;                // 1-component, 8-bit unsigned normalized
    case GL_RG8: return VK_FORMAT_R8G8_UNORM;               // 2-component, 8-bit unsigned normalized
    case GL_RGB8: return VK_FORMAT_R8G8B8_UNORM;              // 3-component, 8-bit unsigned normalized
    case GL_RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;             // 4-component, 8-bit unsigned normalized
    case GL_RGB: return VK_FORMAT_R8G8B8_UNORM;              // 3-component, 8-bit unsigned normalized
    case GL_RGBA: return VK_FORMAT_R8G8B8A8_SRGB;             // 4-component, 8-bit unsigned normalized

    case GL_R8_SNORM: return VK_FORMAT_R8_SNORM;          // 1-component, 8-bit signed normalized
    case GL_RG8_SNORM: return VK_FORMAT_R8G8_SNORM;         // 2-component, 8-bit signed normalized
    case GL_RGB8_SNORM: return VK_FORMAT_R8G8B8_SNORM;        // 3-component, 8-bit signed normalized
    case GL_RGBA8_SNORM: return VK_FORMAT_R8G8B8A8_SNORM;       // 4-component, 8-bit signed normalized

    case GL_R8UI: return VK_FORMAT_R8_UINT;              // 1-component, 8-bit unsigned integer
    case GL_RG8UI: return VK_FORMAT_R8G8_UINT;             // 2-component, 8-bit unsigned integer
    case GL_RGB8UI: return VK_FORMAT_R8G8B8_UINT;            // 3-component, 8-bit unsigned integer
    case GL_RGBA8UI: return VK_FORMAT_R8G8B8A8_UINT;           // 4-component, 8-bit unsigned integer

    case GL_R8I: return VK_FORMAT_R8_SINT;               // 1-component, 8-bit signed integer
    case GL_RG8I: return VK_FORMAT_R8G8_SINT;              // 2-component, 8-bit signed integer
    case GL_RGB8I: return VK_FORMAT_R8G8B8_SINT;             // 3-component, 8-bit signed integer
    case GL_RGBA8I: return VK_FORMAT_R8G8B8A8_SINT;            // 4-component, 8-bit signed integer

    case GL_SR8: return VK_FORMAT_R8_SRGB;               // 1-component, 8-bit sRGB
    case GL_SRG8: return VK_FORMAT_R8G8_SRGB;              // 2-component, 8-bit sRGB
    case GL_SRGB8: return VK_FORMAT_R8G8B8_SRGB;             // 3-component, 8-bit sRGB
    case GL_SRGB8_ALPHA8: return VK_FORMAT_R8G8B8A8_SRGB;      // 4-component, 8-bit sRGB

        //
        // 16 bits per component
        //
    case GL_R16: return VK_FORMAT_R16_UNORM;               // 1-component, 16-bit unsigned normalized
    case GL_RG16: return VK_FORMAT_R16G16_UNORM;              // 2-component, 16-bit unsigned normalized
    case GL_RGB16: return VK_FORMAT_R16G16B16_UNORM;             // 3-component, 16-bit unsigned normalized
    case GL_RGBA16: return VK_FORMAT_R16G16B16A16_UNORM;            // 4-component, 16-bit unsigned normalized

    case GL_R16_SNORM: return VK_FORMAT_R16_SNORM;         // 1-component, 16-bit signed normalized
    case GL_RG16_SNORM: return VK_FORMAT_R16G16_SNORM;        // 2-component, 16-bit signed normalized
    case GL_RGB16_SNORM: return VK_FORMAT_R16G16B16_SNORM;       // 3-component, 16-bit signed normalized
    case GL_RGBA16_SNORM: return VK_FORMAT_R16G16B16A16_SNORM;      // 4-component, 16-bit signed normalized

    case GL_R16UI: return VK_FORMAT_R16_UINT;             // 1-component, 16-bit unsigned integer
    case GL_RG16UI: return VK_FORMAT_R16G16_UINT;            // 2-component, 16-bit unsigned integer
    case GL_RGB16UI: return VK_FORMAT_R16G16B16_UINT;           // 3-component, 16-bit unsigned integer
    case GL_RGBA16UI: return VK_FORMAT_R16G16B16A16_UINT;          // 4-component, 16-bit unsigned integer

    case GL_R16I: return VK_FORMAT_R16_SINT;              // 1-component, 16-bit signed integer
    case GL_RG16I: return VK_FORMAT_R16G16_SINT;             // 2-component, 16-bit signed integer
    case GL_RGB16I: return VK_FORMAT_R16G16B16_SINT;            // 3-component, 16-bit signed integer
    case GL_RGBA16I: return VK_FORMAT_R16G16B16A16_SINT;           // 4-component, 16-bit signed integer

    case GL_R16F: return VK_FORMAT_R16_SFLOAT;              // 1-component, 16-bit floating-point
    case GL_RG16F: return VK_FORMAT_R16G16_SFLOAT;             // 2-component, 16-bit floating-point
    case GL_RGB16F: return VK_FORMAT_R16G16B16_SFLOAT;            // 3-component, 16-bit floating-point
    case GL_RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;           // 4-component, 16-bit floating-point

        //
        // 32 bits per component
        //
    case GL_R32UI: return VK_FORMAT_R32_UINT;             // 1-component, 32-bit unsigned integer
    case GL_RG32UI: return VK_FORMAT_R32G32_UINT;            // 2-component, 32-bit unsigned integer
    case GL_RGB32UI: return VK_FORMAT_R32G32B32_UINT;           // 3-component, 32-bit unsigned integer
    case GL_RGBA32UI: return VK_FORMAT_R32G32B32A32_UINT;          // 4-component, 32-bit unsigned integer

    case GL_R32I: return VK_FORMAT_R32_SINT;              // 1-component, 32-bit signed integer
    case GL_RG32I: return VK_FORMAT_R32G32_SINT;             // 2-component, 32-bit signed integer
    case GL_RGB32I: return VK_FORMAT_R32G32B32_SINT;            // 3-component, 32-bit signed integer
    case GL_RGBA32I: return VK_FORMAT_R32G32B32A32_SINT;           // 4-component, 32-bit signed integer

    case GL_R32F: return VK_FORMAT_R32_SFLOAT;              // 1-component, 32-bit floating-point
    case GL_RG32F: return VK_FORMAT_R32G32_SFLOAT;             // 2-component, 32-bit floating-point
    case GL_RGB32F: return VK_FORMAT_R32G32B32_SFLOAT;            // 3-component, 32-bit floating-point
    case GL_RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;           // 4-component, 32-bit floating-point

        //
        // Packed
        //
    case GL_RGB5: return VK_FORMAT_R5G5B5A1_UNORM_PACK16;              // 3-component 5:5:5,       unsigned normalized
    case GL_RGB565: return VK_FORMAT_R5G6B5_UNORM_PACK16;            // 3-component 5:6:5,       unsigned normalized
    case GL_RGBA4: return VK_FORMAT_R4G4B4A4_UNORM_PACK16;             // 4-component 4:4:4:4,     unsigned normalized
    case GL_RGB5_A1: return VK_FORMAT_A1R5G5B5_UNORM_PACK16;           // 4-component 5:5:5:1,     unsigned normalized
    case GL_RGB10_A2: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;          // 4-component 10:10:10:2,  unsigned normalized
    case GL_RGB10_A2UI: return VK_FORMAT_A2R10G10B10_UINT_PACK32;        // 4-component 10:10:10:2,  unsigned integer
    case GL_R11F_G11F_B10F: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;    // 3-component 11:11:10,    floating-point
    case GL_RGB9_E5: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;           // 3-component/exp 9:9:9/5, floating-point

        //
        // S3TC/DXT/BC
        //
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;                  // line through 3D space, 4x4 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;                 // line through 3D space plus 1-bit alpha, 4x4 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT: return VK_FORMAT_BC2_UNORM_BLOCK;                 // line through 3D space plus line through 1D space, 4x4 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: return VK_FORMAT_BC3_UNORM_BLOCK;                 // line through 3D space plus 4-bit alpha, 4x4 blocks, unsigned normalized

    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;                 // line through 3D space, 4x4 blocks, sRGB
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;           // line through 3D space plus 1-bit alpha, 4x4 blocks, sRGB
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT: return VK_FORMAT_BC2_SRGB_BLOCK;           // line through 3D space plus line through 1D space, 4x4 blocks, sRGB
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT: return VK_FORMAT_BC3_SRGB_BLOCK;           // line through 3D space plus 4-bit alpha, 4x4 blocks, sRGB

    case GL_COMPRESSED_RED_RGTC1: return VK_FORMAT_BC4_UNORM_BLOCK;                          // line through 1D space, 4x4 blocks, unsigned normalized
    case GL_COMPRESSED_RG_RGTC2: return VK_FORMAT_BC5_UNORM_BLOCK;                           // two lines through 1D space, 4x4 blocks, unsigned normalized
    case GL_COMPRESSED_SIGNED_RED_RGTC1: return VK_FORMAT_BC4_SNORM_BLOCK;                   // line through 1D space, 4x4 blocks, signed normalized
    case GL_COMPRESSED_SIGNED_RG_RGTC2: return VK_FORMAT_BC5_SNORM_BLOCK;                    // two lines through 1D space, 4x4 blocks, signed normalized
    case GL_LUMINANCE: return VK_FORMAT_R8_UNORM;                   // line through 1D space, 4x4 blocks, signed normalized
    case GL_LUMINANCE16: return VK_FORMAT_R16_UNORM;
    case GL_LUMINANCE_ALPHA: return VK_FORMAT_R8G8_UNORM;                    // two lines through 1D space, 4x4 blocks, signed normalized
    case GL_LUMINANCE16_ALPHA16: return VK_FORMAT_R16G16_UNORM;

    case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT: return VK_FORMAT_BC6H_UFLOAT_BLOCK;            // 3-component, 4x4 blocks, unsigned floating-point
    case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT: return VK_FORMAT_BC6H_SFLOAT_BLOCK;              // 3-component, 4x4 blocks, signed floating-point
    case GL_COMPRESSED_RGBA_BPTC_UNORM: return VK_FORMAT_BC7_UNORM_BLOCK;                    // 4-component, 4x4 blocks, unsigned normalized
    case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM: return VK_FORMAT_BC7_SRGB_BLOCK;              // 4-component, 4x4 blocks, sRGB

        //
        // ETC
        //
    case GL_COMPRESSED_RGB8_ETC2: return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;                          // 3-component ETC2, 4x4 blocks, unsigned normalized
    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2: return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;      // 4-component ETC2 with 1-bit alpha, 4x4 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA8_ETC2_EAC: return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;                     // 4-component ETC2, 4x4 blocks, unsigned normalized

    case GL_COMPRESSED_SRGB8_ETC2: return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;                         // 3-component ETC2, 4x4 blocks, sRGB
    case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2: return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;     // 4-component ETC2 with 1-bit alpha, 4x4 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;              // 4-component ETC2, 4x4 blocks, sRGB

    case GL_COMPRESSED_R11_EAC: return VK_FORMAT_EAC_R11_UNORM_BLOCK;                            // 1-component ETC, 4x4 blocks, unsigned normalized
    case GL_COMPRESSED_RG11_EAC: return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;                           // 2-component ETC, 4x4 blocks, unsigned normalized
    case GL_COMPRESSED_SIGNED_R11_EAC: return VK_FORMAT_EAC_R11_SNORM_BLOCK;                     // 1-component ETC, 4x4 blocks, signed normalized
    case GL_COMPRESSED_SIGNED_RG11_EAC: return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;                    // 2-component ETC, 4x4 blocks, signed normalized

        //
        // PVRTC
        //
    case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG: return VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG;           // 3- or 4-component PVRTC, 16x8 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG: return VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG;           // 3- or 4-component PVRTC,  8x8 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG: return VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG;           // 3- or 4-component PVRTC, 16x8 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG: return VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG;           // 3- or 4-component PVRTC,  4x4 blocks, unsigned normalized

    case GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT: return VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG;     // 4-component PVRTC, 16x8 blocks, sRGB
    case GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT: return VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG;     // 4-component PVRTC,  8x8 blocks, sRGB
    case GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV2_IMG: return VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG;     // 4-component PVRTC,  8x4 blocks, sRGB
    case GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV2_IMG: return VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG;     // 4-component PVRTC,  4x4 blocks, sRGB

        //
        // ASTC
        //
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR: return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;                // 4-component ASTC, 4x4 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR: return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;                // 4-component ASTC, 5x4 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR: return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;                // 4-component ASTC, 5x5 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR: return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;                // 4-component ASTC, 6x5 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR: return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;                // 4-component ASTC, 6x6 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR: return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;                // 4-component ASTC, 8x5 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR: return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;                // 4-component ASTC, 8x6 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR: return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;                // 4-component ASTC, 8x8 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR: return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;               // 4-component ASTC, 10x5 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR: return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;               // 4-component ASTC, 10x6 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR: return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;               // 4-component ASTC, 10x8 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR: return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;              // 4-component ASTC, 10x10 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR: return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;              // 4-component ASTC, 12x10 blocks, unsigned normalized
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR: return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;              // 4-component ASTC, 12x12 blocks, unsigned normalized

    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR: return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;        // 4-component ASTC, 4x4 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR: return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;        // 4-component ASTC, 5x4 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR: return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;        // 4-component ASTC, 5x5 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR: return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;        // 4-component ASTC, 6x5 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR: return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;        // 4-component ASTC, 6x6 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR: return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;        // 4-component ASTC, 8x5 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR: return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;        // 4-component ASTC, 8x6 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR: return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;        // 4-component ASTC, 8x8 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR: return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;       // 4-component ASTC, 10x5 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR: return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;       // 4-component ASTC, 10x6 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR: return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;       // 4-component ASTC, 10x8 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR: return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;      // 4-component ASTC, 10x10 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR: return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;      // 4-component ASTC, 12x10 blocks, sRGB
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR: return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;      // 4-component ASTC, 12x12 blocks, sRGB

        //
        // Depth/stencil
        //
    case GL_DEPTH_COMPONENT16: return VK_FORMAT_D16_UNORM;
    case GL_DEPTH_COMPONENT24: return VK_FORMAT_X8_D24_UNORM_PACK32;
    case GL_DEPTH_COMPONENT32F: return VK_FORMAT_D32_SFLOAT;
    case GL_STENCIL_INDEX8: return VK_FORMAT_S8_UINT;
    case GL_DEPTH24_STENCIL8: return VK_FORMAT_D24_UNORM_S8_UINT;
    case GL_DEPTH32F_STENCIL8: return VK_FORMAT_D32_SFLOAT_S8_UINT;
    default: return VK_FORMAT_UNDEFINED;
    }
}

inline GLint convertUNorm2SRgb(GLint glFormat) {
    switch (glFormat) {
    case GL_R8: return GL_SR8;
    case GL_RG8: return GL_SRG8;
    case GL_RGB8: return GL_SRGB8;
    case GL_RGB: return GL_SRGB8;
    case GL_RGBA8: return GL_SRGB8_ALPHA8;
    default:return glFormat;
    }
}

namespace osg
{
    // 获取像素格式的通道数
    int getNumComponentsForPixelFormat(GLenum pixelFormat)
    {
        switch (pixelFormat)
        {
        case GL_RGB:
        case GL_RGB8:
        case GL_SRGB:
        case GL_SRGB8:
            return 3; // RGB 格式
        case GL_RGBA:
        case GL_RGBA8:
            return 4; // RGBA 格式
        case GL_LUMINANCE:
        case GL_LUMINANCE8:
            return 1; // 单通道亮度格式
        case GL_ALPHA:
        case GL_ALPHA8:
            return 1; // Alpha 通道
        case GL_RG:
        case GL_RG8:
            return 2; // RG 格式
            // 更多格式可以在这里添加...
        default:
            return 1; // 默认返回 1
        }
    }

    // 转换图像数据类型为 GL_UNSIGNED_BYTE
    osg::ref_ptr<osg::Image> convertImageToUnsignedByte(const osg::Image* image)
    {
        if (!image) return nullptr;

        if (image->getDataType() == GL_UNSIGNED_BYTE)
        {
            return dynamic_cast<osg::Image*>(image->clone(osg::CopyOp::DEEP_COPY_ALL));
        }


        // 获取像素格式的通道数
        int numComponents = getNumComponentsForPixelFormat(image->getPixelFormat());

        // 创建新的图像，分配内存
        osg::ref_ptr<osg::Image> result = new osg::Image;
        result->allocateImage(image->s(), image->t(), image->r(),
            image->getPixelFormat(), GL_UNSIGNED_BYTE, numComponents);

        if (result->getTotalDataSize() == 0) return nullptr;

        // 获取源数据和目标数据指针
        const unsigned char* srcData = static_cast<const unsigned char*>(image->data());
        unsigned char* dstData = static_cast<unsigned char*>(result->data());

        GLenum srcDataType = image->getDataType();
        size_t totalPixels = image->s() * image->t() * image->r();

        // 根据源数据类型进行转换
        switch (srcDataType)
        {
        case GL_UNSIGNED_SHORT:
        {
            const unsigned short* src = reinterpret_cast<const unsigned short*>(srcData);
            for (size_t i = 0; i < totalPixels * numComponents; ++i)
            {
                // 将 16 位值映射到 8 位 (0-65535 -> 0-255)
                dstData[i] = static_cast<unsigned char>((src[i] * 255) / 65535);
            }
            break;
        }

        case GL_SHORT:
        {
            const short* src = reinterpret_cast<const short*>(srcData);
            for (size_t i = 0; i < totalPixels * numComponents; ++i)
            {
                // 将有符号 16 位值映射到 8 位 (-32768-32767 -> 0-255)
                int value = src[i] + 32768; // 转换为无符号
                dstData[i] = static_cast<unsigned char>((value * 255) / 65535);
            }
            break;
        }

        case GL_UNSIGNED_INT:
        {
            const unsigned int* src = reinterpret_cast<const unsigned int*>(srcData);
            for (size_t i = 0; i < totalPixels * numComponents; ++i)
            {
                // 将 32 位值映射到 8 位 (0-4294967295 -> 0-255)
                dstData[i] = static_cast<unsigned char>((src[i] * 255) / 4294967295ULL);
            }
            break;
        }

        case GL_INT:
        {
            const int* src = reinterpret_cast<const int*>(srcData);
            for (size_t i = 0; i < totalPixels * numComponents; ++i)
            {
                // 将有符号 32 位值映射到 8 位 (-2147483648-2147483647 -> 0-255)
                unsigned long long value = static_cast<unsigned long long>(src[i] + 2147483648ULL);
                dstData[i] = static_cast<unsigned char>((value * 255) / 4294967295ULL);
            }
            break;
        }

        case GL_FLOAT:
        {
            const float* src = reinterpret_cast<const float*>(srcData);
            for (size_t i = 0; i < totalPixels * numComponents; ++i)
            {
                // 将浮点值映射到 8 位 (0.0-1.0 -> 0-255)
                float clamped = std::max(0.0f, std::min(1.0f, src[i]));
                dstData[i] = static_cast<unsigned char>(clamped * 255.0f + 0.5f);
            }
            break;
        }

        case GL_DOUBLE:
        {
            const double* src = reinterpret_cast<const double*>(srcData);
            for (size_t i = 0; i < totalPixels * numComponents; ++i)
            {
                // 将双精度浮点值映射到 8 位 (0.0-1.0 -> 0-255)
                double clamped = std::max(0.0, std::min(1.0, src[i]));
                dstData[i] = static_cast<unsigned char>(clamped * 255.0 + 0.5);
            }
            break;
        }

        case GL_HALF_FLOAT:
        {
            const unsigned short* src = reinterpret_cast<const unsigned short*>(srcData);
            for (size_t i = 0; i < totalPixels * numComponents; ++i)
            {
                // 将半精度浮点值转换为 8 位
                unsigned short half = src[i];
                float value = 0.0f;

                // 简化的半精度浮点解码
                if (half == 0) {
                    value = 0.0f;
                }
                else {
                    int exp = (half >> 10) & 0x1F;
                    int mantissa = half & 0x3FF;

                    if (exp == 0) {
                        // 非规格化数
                        value = mantissa / 1024.0f;
                    }
                    else if (exp == 31) {
                        // 无穷大或 NaN
                        value = (mantissa == 0) ? 1.0f : 0.0f;
                    }
                    else {
                        // 规格化数
                        value = (1.0f + mantissa / 1024.0f) * std::pow(2.0f, exp - 15);
                    }
                }

                float clamped = std::max(0.0f, std::min(1.0f, value));
                dstData[i] = static_cast<unsigned char>(clamped * 255.0f + 0.5f);
            }
            break;
        }

        default:
            OSG_WARN << "Unsupported data type for conversion to GL_UNSIGNED_BYTE: "
                << std::hex << srcDataType << std::dec << std::endl;
            return nullptr;
        }

        // 复制图像属性
        result->setInternalTextureFormat(image->getInternalTextureFormat());
        result->setFileName(image->getFileName());
        result->setWriteHint(image->getWriteHint());

        return result;
    }

    osg::ref_ptr<osg::Image> loadImageFromKtx(ktxTexture* texture, int layer, int face,
        ktx_size_t imgDataSize, bool noCompress = false)
    {
        bool transcoded = false, compressed = false;
        ktx_error_code_e result = KTX_SUCCESS;
        if (ktxTexture_NeedsTranscoding(texture))
        {
            if (noCompress)
            {
                result = ktxTexture2_TranscodeBasis((ktxTexture2*)texture,
                    ktx_transcode_fmt_e::KTX_TTF_RGBA32, 0);
                compressed = false;
            }
            else
            {
#if defined(OSG_GLES1_AVAILABLE) || defined(OSG_GLES2_AVAILABLE) || defined(OSG_GLES3_AVAILABLE)
                // FIXME: mobile compressing textures
                result = ktxTexture2_TranscodeBasis((ktxTexture2*)texture,
                    ktx_transcode_fmt_e::KTX_TTF_ETC, 0);
#else
                result = ktxTexture2_TranscodeBasis((ktxTexture2*)texture,
                    ktx_transcode_fmt_e::KTX_TTF_BC1_OR_3, 0);
#endif
                compressed = true;
            }
            transcoded = (result == KTX_SUCCESS);
        }

        ktx_uint32_t w = texture->baseWidth, h = texture->baseHeight, d = texture->baseDepth;
        ktx_size_t offset = 0; ktxTexture_GetImageOffset(texture, 0, layer, face, &offset);
        ktx_uint8_t* imgData = ktxTexture_GetData(texture) + offset;

        osg::ref_ptr<osg::Image> image = new osg::Image;
        if (texture->classId == ktxTexture2_c)
        {
            ktxTexture2* tex = (ktxTexture2*)texture;

            GLint glInternalformat = glGetInternalFormatFromVkFormat((VkFormat)tex->vkFormat);
            GLenum glFormat = compressed ? glInternalformat  /* Use compressed format */
                : glGetFormatFromInternalFormat(glInternalformat);
            GLenum glType = glGetTypeFromInternalFormat(glInternalformat);
            if (glFormat == GL_INVALID_VALUE || glType == GL_INVALID_VALUE)
            {
                OSG_WARN << "Failed to get KTX2 file format: VkFormat = "
                    << std::hex << tex->vkFormat << std::dec << std::endl;
                return nullptr;
            }
            else if (transcoded)
            {
                // FIXME: KTX1 transcoding?
                OSG_INFO << "Transcoded format: internalFmt = " << std::hex
                    << glInternalformat << ", pixelFmt = " << glFormat << ", type = "
                    << glType << std::dec << std::endl;
                imgDataSize = ktxTexture_GetImageSize(texture, 0);
            }
            image->allocateImage(w, h, d, glFormat, glType, 4);
            image->setInternalTextureFormat(glInternalformat);
        }
        else
        {
            ktxTexture1* tex = (ktxTexture1*)texture;
            image->allocateImage(w, h, d, tex->glFormat, tex->glType, 4);
            image->setInternalTextureFormat(tex->glInternalformat);
        }
        if (image->getTotalDataSize() == 0) {
            OSG_WARN << "Failed to allocateImage" << std::dec << std::endl;
            // 修复：清理已分配的image对象
            return nullptr;
        }
        memcpy(image->data(), imgData, imgDataSize);
        return image;
    }

    std::vector<osg::ref_ptr<osg::Image>> loadKtxFromObject(ktxTexture* texture)
    {
        std::vector<osg::ref_ptr<osg::Image>> resultArray;
        ktx_uint32_t numLevels = texture->numLevels;
        ktx_size_t imgDataSize = 0;
        if (texture->classId == ktxTexture2_c)
            imgDataSize = ktxTexture_calcImageSize(texture, 0, KTX_FORMAT_VERSION_TWO);
        else
            imgDataSize = ktxTexture_calcImageSize(texture, 0, KTX_FORMAT_VERSION_ONE);

        if (texture->isArray || texture->numFaces > 1)
        {
            if (texture->isArray)
            {
                for (ktx_uint32_t i = 0; i < texture->numLayers; ++i)
                {
                    osg::ref_ptr<osg::Image> image = loadImageFromKtx(texture, i, 0, imgDataSize);
                    if (image.valid()) resultArray.push_back(image);
                }
            }
            else
            {
                for (ktx_uint32_t i = 0; i < texture->numFaces; ++i)
                {
                    osg::ref_ptr<osg::Image> image = loadImageFromKtx(texture, 0, i, imgDataSize);
                    if (image.valid()) resultArray.push_back(image);
                }
            }
        }
        else
        {
            osg::ref_ptr<osg::Image> image = loadImageFromKtx(texture, 0, 0, imgDataSize);
            if (image.valid()) resultArray.push_back(image);
        }
        ktxTexture_Destroy(texture);
        return resultArray;
    }

    std::vector<osg::ref_ptr<osg::Image>> loadKtx(const std::string& file)
    {
        ktxTexture* texture = nullptr;
        ktx_error_code_e result = ktxTexture_CreateFromNamedFile(
            file.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
        if (result != KTX_SUCCESS)
        {
            OSG_WARN << "Unable to read from KTX file: " << file << "\n";
            return std::vector<osg::ref_ptr<osg::Image>>();
        }
        return loadKtxFromObject(texture);
    }

    std::vector<osg::ref_ptr<osg::Image>> loadKtx2(std::istream& in)
    {
        std::string data((std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>());
        if (data.empty()) return std::vector<osg::ref_ptr<osg::Image>>();

        ktxTexture* texture = nullptr;
        ktx_error_code_e result = ktxTexture_CreateFromMemory(
            (const ktx_uint8_t*)data.data(), data.size(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
        if (result != KTX_SUCCESS)
        {
            OSG_WARN << "Unable to read from KTX stream\n";
            return std::vector<osg::ref_ptr<osg::Image>>();
        }
        return loadKtxFromObject(texture);
    }

    ktxTexture* saveImageToKtx2(const osg::Image* image, bool compressed)
    {
        ktxTexture* texture = nullptr;

        ktxTextureCreateInfo createInfo;

        GLint originalFormat = image->getInternalTextureFormat();
        createInfo.glInternalformat = convertUNorm2SRgb(originalFormat);
        createInfo.vkFormat = glGetVkFormatFromInternalFormat(createInfo.glInternalformat);
        createInfo.baseWidth = image->s();
        createInfo.baseHeight = image->t();
        createInfo.baseDepth = image->r();
        createInfo.numDimensions = (image->r() > 1) ? 3 : ((image->t() > 1) ? 2 : 1);
        createInfo.numLevels = osg::Image::computeNumberOfMipmapLevels(image->s(), image->t(), image->r());;
        createInfo.numLayers = 1;
        createInfo.numFaces = 1;
        createInfo.isArray = KTX_FALSE;
        createInfo.generateMipmaps = KTX_FALSE;
        if (createInfo.vkFormat == 0)
        {
            OSG_WARN << "No VkFormat for GL internal format: "
                << std::hex << createInfo.glInternalformat << std::dec << std::endl;
            return nullptr;
        }
        KTX_error_code result = ktxTexture2_Create(
            &createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, (ktxTexture2**)&texture);
        if (result != KTX_SUCCESS)
        {
            OSG_WARN << "Unable to create KTX for saving" << std::endl;
            return nullptr;
        }

        const int layer = 0, face = 0;
        osg::ref_ptr<osg::Image> imgCopy = convertImageToUnsignedByte(image);
        for (size_t level = 0; level < createInfo.numLevels; ++level)
        {
            // 使用位移操作替代pow
            auto levelWidth = osg::maximum<uint32_t>(1, image->s() >> level);
            auto levelHeight = osg::maximum<uint32_t>(1, image->t() >> level);

            
            imgCopy->scaleImage(levelWidth, levelHeight, imgCopy->r());
            const ktx_uint8_t* src = (ktx_uint8_t*)imgCopy->data();

            ktx_size_t imageSize = imgCopy->getTotalSizeInBytes();
            ktx_size_t expectedSize = ktxTexture_GetImageSize(texture, level);
            if (imageSize != expectedSize) {
                OSG_WARN << "Image data size does not match the expected size" << std::endl;
                ktxTexture_Destroy(texture);
                return nullptr;
            }
            result = ktxTexture_SetImageFromMemory(texture, level, layer, face, src, expectedSize);
            if (result != KTX_SUCCESS)
            {
                OSG_WARN << "Unable to save image " << level
                    << " to KTX texture: " << ktxErrorString(result) << std::endl;
                ktxTexture_Destroy(texture); 
                return nullptr;
            }
        }

        // 移除未使用的变量
        if (compressed) {
            switch (createInfo.vkFormat) {
            case VK_FORMAT_R8_UNORM:
            case VK_FORMAT_R8_SRGB:
            case VK_FORMAT_R8G8_UNORM:
            case VK_FORMAT_R8G8_SRGB:
            case VK_FORMAT_R8G8B8_UNORM:
            case VK_FORMAT_R8G8B8_SRGB:
            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_SRGB:
                // Allowed formats
                break;
            default:
                OSG_WARN << "Only the R8, RG8, RGB8, RGBA8 UNORM, and SRGB formats can be encoded in KTX2 compression, "
                    "but format is " << std::to_string(VkFormat(createInfo.vkFormat)) << "." << std::endl;
                return texture;
            }
            ktxBasisParams params = { 0 };
            params.structSize = sizeof(params);
            params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
            params.uastc = KTX_FALSE;

            unsigned int numThreads = std::thread::hardware_concurrency();
            if (numThreads == 0) {
                numThreads = 2;
            }
            params.threadCount = numThreads;
            result = ktxTexture2_CompressBasisEx((ktxTexture2*)texture, &params);
            if (result != KTX_SUCCESS)
            {
                OSG_WARN << "Failed to compress ktxTexture2: "
                    << ktxErrorString(result) << std::endl;
                // 修复：压缩失败时清理资源
                ktxTexture_Destroy(texture);
                return nullptr;
            }
        }
        return texture;
    }

    ktxTexture* saveImageToKtx1(const osg::Image* image) {
        ktxTexture* texture = nullptr;

        ktxTextureCreateInfo createInfo;

        // 修复：防止整数溢出，使用更安全的计算方式
        int max_dim = std::max(image->s(), image->t());

        int numLevels = static_cast<int>(floor(log2(static_cast<double>(max_dim)))) + 1;
        createInfo.glInternalformat = convertUNorm2SRgb(image->getInternalTextureFormat());
        createInfo.vkFormat = glGetVkFormatFromInternalFormat(createInfo.glInternalformat);
        createInfo.baseWidth = image->s();
        createInfo.baseHeight = image->t();
        createInfo.baseDepth = image->r();
        createInfo.numDimensions = (image->r() > 1) ? 3 : ((image->t() > 1) ? 2 : 1);
        createInfo.numLevels = numLevels;
        createInfo.numLayers = 1;
        createInfo.numFaces = 1;
        createInfo.isArray = KTX_FALSE;
        createInfo.generateMipmaps = KTX_FALSE;
        if (createInfo.vkFormat == 0)
        {
            OSG_WARN << "No VkFormat for GL internal format: "
                << std::hex << createInfo.glInternalformat << std::dec << std::endl;
            return nullptr;
        }
        KTX_error_code result = ktxTexture1_Create(
            &createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, (ktxTexture1**)&texture);
        if (result != KTX_SUCCESS)
        {
            OSG_WARN << "Unable to create KTX for saving" << std::endl;
            return nullptr;
        }
        // 修复：使用智能指针管理内存
        osg::ref_ptr<osg::Image> imgCopy = convertImageToUnsignedByte(image);
        for (size_t i = 0; i < numLevels; ++i)
        {
            // 修复：使用位移操作替代pow
            int width = image->s() >> i;
            int height = image->t() >> i;
            width = width > 0 ? width : 1;
            height = height > 0 ? height : 1;
            imgCopy->scaleImage(width, height, imgCopy->r());
            const ktx_uint8_t* src = (ktx_uint8_t*)imgCopy->data();
            const unsigned int level = i;
            // 修复：使用当前缩放后图像的实际大小
            ktx_size_t imageSize = imgCopy->getTotalSizeInBytes();
            result = ktxTexture_SetImageFromMemory(
                static_cast<ktxTexture*>(texture), level, 0, 0,
                src, imageSize);
            if (result != KTX_SUCCESS)
            {
                OSG_WARN << "Unable to save image " << i
                    << " to KTX texture: " << ktxErrorString(result) << std::endl;
                ktxTexture_Destroy(texture); 
                return nullptr;
            }
        }
        return texture;
    }

    bool saveKtx2(const std::string& file, const osg::Image* image, bool compressed)
    {
        ktxTexture* texture = saveImageToKtx2(image, compressed);
        if (texture == nullptr) return false;

        KTX_error_code result = ktxTexture_WriteToNamedFile(texture, file.c_str());
        ktxTexture_Destroy(texture);
        return result == KTX_SUCCESS;
    }

    bool saveKtx2(std::ostream& out, const osg::Image* image, bool compressed)
    {
        ktxTexture* texture = saveImageToKtx2(image, compressed);
        if (texture == nullptr) return false;

        ktx_uint8_t* buffer = nullptr; ktx_size_t length = 0;
        KTX_error_code result = ktxTexture_WriteToMemory(texture, &buffer, &length);
        if (result == KTX_SUCCESS)
        {
            out.write((char*)buffer, length);
            delete buffer;
        }
        ktxTexture_Destroy(texture);
        return result == KTX_SUCCESS;
    }

    bool saveKtx1(const std::string& file, const osg::Image* image)
    {
        ktxTexture* texture = saveImageToKtx1(image);
        if (texture == nullptr) return false;

        KTX_error_code result = ktxTexture_WriteToNamedFile(texture, file.c_str());
        ktxTexture_Destroy(texture);
        return result == KTX_SUCCESS;
    }

    bool saveKtx1(std::ostream& out, const osg::Image* image)
    {
        ktxTexture* texture = saveImageToKtx1(image);
        if (texture == nullptr) return false;

        ktx_uint8_t* buffer = nullptr; ktx_size_t length = 0;
        KTX_error_code result = ktxTexture_WriteToMemory(texture, &buffer, &length);
        if (result == KTX_SUCCESS)
        {
            out.write((char*)buffer, length);
            delete buffer;
        }
        ktxTexture_Destroy(texture);
        return result == KTX_SUCCESS;
    }
}
