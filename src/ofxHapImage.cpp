#include "ofxHapImage.h"
#include <hap.h>
#include <squish.h>
#include <YCoCg.h>
#include <YCoCgDXT.h>
#if defined(TARGET_WIN32)
#include <ppl.h>
#endif

// Must be a multiple of 4
#define kofxHapImageMTChunkHeight 32

#define kofxHapImageEncodeChunkCount 4

namespace ofxHapImagePrivate {
    static void decodeCallback(HapDecodeWorkFunction function, void *p, unsigned int count, void *info)
    {
#if defined(TARGET_OSX)
        dispatch_apply(count, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(size_t index) {
            function(p, (unsigned int)index);
        });
#elif defined(TARGET_WIN32)
        concurrency::parallel_for((unsigned int)0, count, [&](unsigned int i) {
            function(p, i);
        });
#else
        // TODO: multi-threaded on Linux
        for (unsigned int i = 0; i < count; i++) {
            function(p, i);
        }
#endif
    }

    static int roundUpToMultipleOf4(int n)
    {
        if(0 != (n & 3))
            n = (n + 3) & ~3;
        return n;
    }

    const string YCoCgVertexShader = "void main(void)\
    {\
    gl_Position = ftransform();\
    gl_TexCoord[0] = gl_MultiTexCoord0;\
    }";

    const string YCoCgFragmentShader = "uniform sampler2D cocgsy_src;\
    const vec4 offsets = vec4(-0.50196078431373, -0.50196078431373, 0.0, 0.0);\
    void main()\
    {\
    vec4 CoCgSY = texture2D(cocgsy_src, gl_TexCoord[0].xy);\
    CoCgSY += offsets;\
    float scale = ( CoCgSY.z * ( 255.0 / 8.0 ) ) + 1.0;\
    float Co = CoCgSY.x / scale;\
    float Cg = CoCgSY.y / scale;\
    float Y = CoCgSY.w;\
    vec4 rgba = vec4(Y + Co - Cg, Y + Cg, Y - Co - Cg, 1.0);\
    gl_FragColor = rgba;\
    }";
}

std::string ofxHapImage::HapImageFileExtension()
{
    return "hpz";
}

std::string ofxHapImage::imageTypeDescription(ofxHapImage::ImageType type)
{
    switch (type) {
        case IMAGE_TYPE_HAP:
            return "Hap";
        case IMAGE_TYPE_HAP_ALPHA:
            return "Hap Alpha";
        case IMAGE_TYPE_HAP_Q:
            return "Hap Q";
        default:
            return "Unknown";
    }
}

ofxHapImage::ofxHapImage() :
texture_needs_update_(true), type_(IMAGE_TYPE_HAP), width_(0), height_(0)
{

}

ofxHapImage::~ofxHapImage()
{

}

ofxHapImage::ofxHapImage(const std::string& filename)
{
    loadImage(filename);
}

ofxHapImage::ofxHapImage(const ofFile& file)
{
    loadImage(file);
}

ofxHapImage::ofxHapImage(const ofBuffer& buffer)
{
    loadImage(buffer);
}

ofxHapImage::ofxHapImage(ofImage& image, ofxHapImage::ImageType type)
{
    loadImage(image, type);
}

bool ofxHapImage::loadImage(const std::string &filename)
{
    if (ofFilePath::getFileExt(filename) == HapImageFileExtension())
    {
        ofBuffer buffer = ofBufferFromFile(filename, true);
        return loadImage(buffer);
    }
    else
    {
        dxt_buffer_.clear();
        return false;
    }
}

bool ofxHapImage::loadImage(const ofFile &file)
{
    return loadImage(file.getAbsolutePath());
}

bool ofxHapImage::loadImage(const ofBuffer &buffer)
{
    // TODO: we could postpone this until we need the dxt data
    // so that loadImage() -> saveImage() doesn't do a needless decode/encode cycle
    unsigned int count;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int dimensions_valid;
    unsigned int format;
    const void *frame = nullptr;
    unsigned long frame_size = 0;
    unsigned int result = HapImageReadHeader(buffer.getData(), buffer.size(), &width, &height, &frame);
    if (result == HapResult_No_Error)
    {
        frame_size = buffer.size() - (((const char *)frame) - buffer.getData());
        // TODO: deal with count != 1 at least by error
        result = HapGetFrameTextureCount(frame, frame_size, &count);
    }
    if (result == HapResult_No_Error)
    {
        result = HapGetFrameTextureFormat(frame, frame_size, 0, &format);
    }
    if (result == HapResult_No_Error && width != 0 && height != 0)
    {
        switch (format) {
            case HapTextureFormat_RGB_DXT1:
                type_ = IMAGE_TYPE_HAP;
                break;
            case HapTextureFormat_RGBA_DXT5:
                type_ = IMAGE_TYPE_HAP_ALPHA;
                break;
            case HapTextureFormat_YCoCg_DXT5:
                type_ = IMAGE_TYPE_HAP_Q;
                break;
            default:
                break;
        }
        width_ = width;
        height_ = height;
        long decompressed_size = ofxHapImagePrivate::roundUpToMultipleOf4(width) * ofxHapImagePrivate::roundUpToMultipleOf4(height);
        unsigned long output_buffer_bytes_used;

        if (format == HapTextureFormat_RGB_DXT1)
        {
            decompressed_size /= 2;
        }
        if (dxt_buffer_.size() != decompressed_size)
        {
            dxt_buffer_.allocate(decompressed_size);
        }
        result = HapDecode(frame, frame_size, 0, ofxHapImagePrivate::decodeCallback, NULL, dxt_buffer_.getData(), dxt_buffer_.size(), &output_buffer_bytes_used, &format);
    }
    if (result == HapResult_No_Error)
    {
        texture_needs_update_ = true;
        return true;
    }
    else
    {
        width_ = height_ = 0;
        texture_.clear();
        dxt_buffer_.clear();
        return false;
    }
}

bool ofxHapImage::loadImage(ofImage &image, ofxHapImage::ImageType type)
{
    ofImageType input_type = image.getPixels().getImageType();
    if (input_type != OF_IMAGE_COLOR_ALPHA)
    {
        image = ofImage(image); // TODO: most efficient up/down-sample mechanism
        image.setImageType(OF_IMAGE_COLOR_ALPHA);
    }
    // Initial calculation gives largest size, for Hap Alpha and Hap Q
    long dxt_size = ofxHapImagePrivate::roundUpToMultipleOf4(image.getWidth()) * ofxHapImagePrivate::roundUpToMultipleOf4(image.getHeight());
    int squish_flags = squish::kColourClusterFit;
    bool result = true;
    switch (type) {
        case IMAGE_TYPE_HAP:
            dxt_size /= 2;
            squish_flags |= squish::kDxt1;
            break;
        case IMAGE_TYPE_HAP_ALPHA:
            squish_flags |= squish::kDxt5;
            break;
        case IMAGE_TYPE_HAP_Q:
            break;
        default:
            result = false;
            break;
    }
    if (result == true)
    {
        if (dxt_buffer_.size() != dxt_size)
        {
            dxt_buffer_.allocate(dxt_size);
        }
        unsigned int divisions = image.getHeight() / kofxHapImageMTChunkHeight;
        if ((int)image.getHeight() % kofxHapImageMTChunkHeight != 0)
        {
            divisions++;
        }
        ofPixels pixels = image.getPixels();
        size_t dxt_bytes_per_division = ofxHapImagePrivate::roundUpToMultipleOf4(image.getWidth()) * kofxHapImageMTChunkHeight;
        if (type == IMAGE_TYPE_HAP)
        {
            dxt_bytes_per_division /= 2;
        }
#if defined(TARGET_OSX)
        dispatch_apply(divisions, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(size_t index) {
#elif defined(TARGET_WIN32)
        concurrency::parallel_for((unsigned int)0, divisions, [&](unsigned int index) {
#else
        for (unsigned int index = 0; index < divisions; index++) {
#endif
            int chunk_height = MIN(kofxHapImageMTChunkHeight, image.getHeight() - (kofxHapImageMTChunkHeight * index));
            if (type == IMAGE_TYPE_HAP_Q)
            {
                // First convert RGBA to YCoCg
                std::vector<uint8_t> ycocg(chunk_height * image.getWidth() * 4);
                ConvertRGB_ToCoCg_Y8888(static_cast<const uint8_t *>(&pixels[pixels.getPixelIndex(0, index * kofxHapImageMTChunkHeight)]),
                                        &ycocg[0],
                                        image.getWidth(),
                                        chunk_height,
                                        image.getWidth() * 4,
                                        image.getWidth() * 4,
                                        0);
                // Convert YCoCg to YCoCgDXT
                CompressYCoCgDXT5(static_cast<byte *>(&ycocg[0]),
                                  reinterpret_cast<byte *>(dxt_buffer_.getData() + (dxt_bytes_per_division * index)),
                                  image.getWidth(),
                                  chunk_height,
                                  image.getWidth() * 4);
            }
            else
            {
                squish::CompressImage(&pixels[pixels.getPixelIndex(0, index * kofxHapImageMTChunkHeight)],
                                      image.getWidth(),
                                      chunk_height,
                                      dxt_buffer_.getData() + (dxt_bytes_per_division * index),
                                      squish_flags);
            }
#if defined(TARGET_LINUX)
        }
#else
        });
#endif
        type_ = type;
        width_ = image.getWidth();
        height_ = image.getHeight();
        texture_needs_update_ = true;
    }
    else
    {
        width_ = height_ = 0;
        texture_.clear();
        dxt_buffer_.clear();
    }
    return result;
}

void ofxHapImage::saveImage(ofFile &file)
{
    file.changeMode(ofFile::ReadWrite, true);
    ofBuffer buffer;
    saveImage(buffer);
    file << buffer;
}

void ofxHapImage::saveImage(const std::string &fileName)
{
    std::ofstream file_out;
    file_out.open(ofToDataPath(fileName).c_str(), std::ios::binary);
    if (file_out.is_open())
    {
        std::vector<char> destination;
        if (saveImage(destination))
        {
            file_out.write(&destination[0], destination.size());
        }
        file_out.close();
    }
    else
    {
        ofLogError("ofxHapImage", "Couldn't open file in saveImage()");
    }
}

void ofxHapImage::saveImage(ofBuffer &buffer)
{
    /*
     ofBuffer doesn't allow a buffer to be shrunk, so we have to encode to a vector then
     copy to the buffer afterwards
     */
    std::vector<char> destination;
    if (saveImage(destination) == true)
    {
        buffer.set(&destination[0], destination.size());
    }
    else
    {
        buffer.clear();
    }
}

bool ofxHapImage::saveImage(std::vector<char>& destination)
{
    unsigned int format;
    switch (type_) {
        case IMAGE_TYPE_HAP:
            format = HapTextureFormat_RGB_DXT1;
            break;
        case IMAGE_TYPE_HAP_ALPHA:
            format = HapTextureFormat_RGBA_DXT5;
            break;
        case IMAGE_TYPE_HAP_Q:
            format = HapTextureFormat_YCoCg_DXT5;
            break;
        default:
            break;
    }
    unsigned long tex_size = dxt_buffer_.size();
    unsigned int chunk_count = kofxHapImageEncodeChunkCount;
    destination.resize(HapMaxEncodedLength(1, &tex_size, &format, &chunk_count) + 16);
    unsigned long buffer_used = 0;
    const void *input = dxt_buffer_.getData();
    unsigned int compressor = HapCompressorSnappy;

    unsigned int result = HapImageWriteHeader(width_, height_, &destination[0], destination.size(), &buffer_used);
    if (result == HapResult_No_Error)
    {
        unsigned long header_used = buffer_used;
        result = HapEncode(1,
                           &input,
                           &tex_size,
                           &format,
                           &compressor,
                           &chunk_count,
                           &destination[buffer_used],
                           destination.size() - buffer_used,
                           &buffer_used);
        buffer_used += header_used;
    }
    if (result == HapResult_No_Error)
    {
        destination.resize(buffer_used);
        return true;
    }
    else
    {
        destination.clear();
        return false;
    }
}

float ofxHapImage::getWidth() const
{
    return width_;
}

float ofxHapImage::getHeight() const
{
    return height_;
}

ofxHapImage::ImageType ofxHapImage::getImageType()
{
    return type_;
}

ofTexture& ofxHapImage::getTexture()
{
    prepareTexture();
    return texture_;
}

const ofTexture& ofxHapImage::getTexture() const
{
    prepareTexture();
    return texture_;
}

void ofxHapImage::prepareTexture() const
{
    if (texture_needs_update_ && dxt_buffer_.size() > 0 && width_ > 0 && height_ > 0)
    {
        /*
         Prepare our texture for DXT upload
         */
        GLint internal_type = (type_ == IMAGE_TYPE_HAP ? GL_COMPRESSED_RGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);

        if (texture_.getWidth() != width_
            || texture_.getHeight() != height_
            || texture_.getTextureData().glInternalFormat != internal_type)
        {
            ofTextureData texData;
            texData.width = width_;
            texData.height = height_;
            texData.textureTarget = GL_TEXTURE_2D;
            texData.glInternalFormat = internal_type;
            texture_.allocate(texData, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);
        }

#if defined(TARGET_OSX)
        texture_.bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_STORAGE_HINT_APPLE , GL_STORAGE_SHARED_APPLE);
        texture_.unbind();
#endif

        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);

        texture_.bind();


#if defined(TARGET_OSX)
        glTextureRangeAPPLE(GL_TEXTURE_2D, dxt_buffer_.size(), const_cast<char *>(dxt_buffer_.getData()));
        glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
#endif

        glCompressedTexSubImage2D(GL_TEXTURE_2D,
                                  0,
                                  0,
                                  0,
                                  width_,
                                  height_,
                                  internal_type,
                                  dxt_buffer_.size(),
                                  dxt_buffer_.getData());
        texture_.unbind();

        glPopClientAttrib();

        texture_needs_update_ = false;
    }
}

ofShader& ofxHapImage::getShader() const
{
    if (!shader_.isLoaded())
    {
        bool success = shader_.setupShaderFromSource(GL_VERTEX_SHADER, ofxHapImagePrivate::YCoCgVertexShader);
        if (success)
        {
            success = shader_.setupShaderFromSource(GL_FRAGMENT_SHADER, ofxHapImagePrivate::YCoCgFragmentShader);
        }
        if (success)
        {
            success = shader_.linkProgram();
        }
    }
    return shader_;
}

void ofxHapImage::draw(float x, float y) const
{
    draw(x, y, getWidth(), getHeight());
}

void ofxHapImage::draw(float x, float y, float w, float h) const
{
    if (getTexture().isAllocated())
    {
        if (type_ == IMAGE_TYPE_HAP_Q)
        {
            getShader().begin();
        }
        getTexture().draw(x, y, w, h);
        if (type_ == IMAGE_TYPE_HAP_Q)
        {
            getShader().end();
        }
    }
}

bool ofxHapImage::isLoaded() const
{
    return dxt_buffer_.size() > 0;
}
