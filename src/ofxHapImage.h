#pragma once
#include "ofMain.h"

// TODO: ofImage from ofxHapImage

class ofxHapImage : public ofAbstractImage {
public:
    enum ImageType {
        IMAGE_TYPE_HAP,
        IMAGE_TYPE_HAP_ALPHA,
        IMAGE_TYPE_HAP_Q
    };

    /*
     The file extension for Hap Images
     */
    static std::string HapImageFileExtension();

    /*
     A textual description of an ImageType (eg "Hap")
     */
    static std::string imageTypeDescription(ImageType type);

    ofxHapImage();
    virtual ~ofxHapImage();

    /*
     Load an existing Hap image
     */
    ofxHapImage(const ofFile& file);

    ofxHapImage(const std::string& filename);

    ofxHapImage(const ofBuffer& buffer);

    /*
     Create a new Hap image
     */
    ofxHapImage(ofImage& image, ofxHapImage::ImageType type);

    /*
     Load an existing Hap image
     */
    bool loadImage(const ofFile& file);
    
    bool loadImage(const std::string& filename);

    bool loadImage(const ofBuffer& buffer);

    /*
     Create a  new Hap image
     */
    bool loadImage(ofImage& image, ofxHapImage::ImageType type);

    /*
     Save a Hap image
     */
    void saveImage(const std::string& fileName);

    void saveImage(ofBuffer& buffer);

    void saveImage(ofFile& file);

    /*
     Drawing
     */
    void draw(float x, float y);

    void draw(float x, float y, float w, float h);

    float getHeight();
    
    float getWidth();

    ofxHapImage::ImageType getImageType();

    ofTexture& getTextureReference();

    /*
     When using the texture for IMAGE_TYPE_HAP_Q, drawing requires the use of this shader
     */
    ofShader& getShaderReference();

    // This is ignored, we always use a texture
    void setUseTexture(bool use_texture) {};

private:
    bool saveImage(std::vector<char>& destination);
    ofBuffer dxt_buffer_;
    ofTexture texture_;
    ofShader shader_;
    bool texture_needs_update_;
    ofxHapImage::ImageType type_;
    unsigned int width_;
    unsigned int height_;
};
