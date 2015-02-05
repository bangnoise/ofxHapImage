#include "ofxHapImage.h"
#include <hap.h>
#include <squish.h>
#if defined(__APPLE__)
#else
#include <ppl.h>
#endif

static void ofxHapImageHapDecodeCallback(HapDecodeWorkFunction function, void *p, unsigned int count, void *info)
{
#if defined(__APPLE__)
    dispatch_apply(count, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(size_t index) {
        function(p, (unsigned int)index);
    });
#else
    concurrency::parallel_for((unsigned int)0, count, [&](unsigned int i) {
        function(p, i);
    });
#endif
}

static int ofxHapRoundUpToMultipleOf4(int n)
{
    if(0 != (n & 3))
        n = (n + 3) & ~3;
    return n;
}

std::string ofxHapImage::HapImageFileExtension()
{
    return "hpz";
}

ofxHapImage::ofxHapImage() :
texture_needs_update_(true), width_(0), height_(0), type_(IMAGE_TYPE_HAP)
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
    unsigned int width;
    unsigned int height;
    unsigned int dimensions_valid;
    unsigned int format;

    unsigned int result = HapGetFrameDetails(buffer.getBinaryBuffer(), buffer.size(), &width, &height, &dimensions_valid, &format);
    if (result == HapResult_No_Error && dimensions_valid != 0)
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
        size_t decompressed_size = ofxHapRoundUpToMultipleOf4(width) * ofxHapRoundUpToMultipleOf4(height);
        unsigned long output_buffer_bytes_used;

        if (format == HapTextureFormat_RGB_DXT1)
        {
            decompressed_size /= 2;
        }
        if (dxt_buffer_.size() != decompressed_size)
        {
            dxt_buffer_.allocate(decompressed_size + 1); // TODO: bug in ofBuffer adds 1 to every size except in allocate()
        }
        result = HapDecode(buffer.getBinaryBuffer(), buffer.size(), ofxHapImageHapDecodeCallback, NULL, dxt_buffer_.getBinaryBuffer(), dxt_buffer_.size(), &output_buffer_bytes_used, &format);
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
    ofImageType input_type = image.getPixelsRef().getImageType();
    if (input_type != OF_IMAGE_COLOR_ALPHA)
    {
        image = ofImage(image); // TODO: most efficient up/down-sample mechanism
        image.setImageType(OF_IMAGE_COLOR_ALPHA);
    }
    // Initial calculation gives largest size, for Hap Alpha and Hap Q
    size_t dxt_size = ofxHapRoundUpToMultipleOf4(image.getWidth() * ofxHapRoundUpToMultipleOf4(image.getHeight()));
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
        default:
            result = false;
            break;
    }
    if (result == true)
    {
        if (dxt_buffer_.size() != dxt_size)
        {
            dxt_buffer_.allocate(dxt_size + 1); // TODO: bug in ofBuffer means it adds 1 to every size-related action except here
        }
        squish::CompressImage(image.getPixels(), image.getWidth(), image.getHeight(), dxt_buffer_.getBinaryBuffer(), squish::kDxt1 | squish::kColourClusterFit);
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

void ofxHapImage::saveImage(const ofFile &file)
{
    saveImage(file.getAbsolutePath());
}

void ofxHapImage::saveImage(const std::string &fileName)
{
    ofBuffer buffer;
    saveImage(buffer);
    ofBufferToFile(fileName, buffer, true);
}

void ofxHapImage::saveImage(ofBuffer &buffer)
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
    buffer.allocate(HapMaxEncodedLength(dxt_buffer_.size(), format, 1) + 1); // TODO: ofBuffer allocate -1 bug, see other notes
    unsigned long buffer_used = 0;
    unsigned int result = HapEncode(dxt_buffer_.getBinaryBuffer(),
                                    dxt_buffer_.size(),
                                    format,
                                    width_, height_,
                                    HapCompressorSnappy,
                                    1,
                                    buffer.getBinaryBuffer(),
                                    buffer.size(),
                                    &buffer_used);
    if (result != HapResult_No_Error)
    {
        buffer.clear();
    }
}

float ofxHapImage::getWidth()
{
    return width_;
}

float ofxHapImage::getHeight()
{
    return height_;
}

ofxHapImage::ImageType ofxHapImage::getImageType()
{
    return type_;
}

ofTexture& ofxHapImage::getTextureReference()
{
    if (texture_needs_update_ && dxt_buffer_.size() > 0 && width_ > 0 && height_ > 0)
    {
        /*
         Prepare our texture for DXT upload
         */
        GLint internal_type = (type_ == IMAGE_TYPE_HAP ? GL_COMPRESSED_RGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);

        unsigned int rounded_width = ofxHapRoundUpToMultipleOf4(width_);
        unsigned int rounded_height = ofxHapRoundUpToMultipleOf4(height_);

        if (texture_.getWidth() != rounded_width
            || texture_.getHeight() != rounded_height
            || texture_.getTextureData().glTypeInternal != internal_type)
        {
            ofTextureData texData;
            texData.width = rounded_width;
            texData.height = rounded_height;
            texData.textureTarget = GL_TEXTURE_2D;
            texData.glTypeInternal = internal_type;
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
        glTextureRangeAPPLE(GL_TEXTURE_2D, dxt_buffer_.size(), dxt_buffer_.getBinaryBuffer());
        glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
#endif

        glCompressedTexSubImage2D(GL_TEXTURE_2D,
                                  0,
                                  0,
                                  0,
                                  rounded_width,
                                  rounded_height,
                                  internal_type,
                                  dxt_buffer_.size(),
                                  dxt_buffer_.getBinaryBuffer());
        texture_.unbind();
        
        glPopClientAttrib();

        texture_needs_update_ = false;
    }
    return texture_;
}

void ofxHapImage::draw(float x, float y)
{
    draw(x, y, getWidth(), getHeight());
}

void ofxHapImage::draw(float x, float y, float w, float h)
{
    if (getTextureReference().isAllocated())
    {
        getTextureReference().draw(x, y, w, h);
    }
}
