#pragma once
#include "ofMain.h"

// TODO: ofImage from ofxHapImage

class ofxHapImage : public ofAbstractImage {
public:
    enum ofxHapType {
        OFX_HAPIMAGE_HAP,
        OFX_HAPIMAGE_HAP_ALPHA,
        OFX_HAPIMAGE_HAP_Q // TODO: Not supported yet
    };

    static std::string HapImageFileExtension();

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
    ofxHapImage(ofImage& image, ofxHapType type);

    /*
     Load an existing Hap image
     */
    bool loadImage(const ofFile& file);
    
    bool loadImage(const std::string& filename);

    bool loadImage(const ofBuffer& buffer);

    /*
     Create a  new Hap image
     */
    bool loadImage(ofImage& image, ofxHapType type);

    /*
     Save a Hap image
     */
    void saveImage(const std::string& fileName);

    void saveImage(ofBuffer& buffer);

    void saveImage(const ofFile& file);

    /*
     Drawing
     */
    void draw(float x, float y);

    void draw(float x, float y, float w, float h);

    float getHeight();
    
    float getWidth();

    ofTexture& getTextureReference();

    // This is ignored, we always use a texture
    void setUseTexture(bool use_texture) {};

private:
    ofBuffer dxt_buffer_;
    ofTexture texture_;
    bool texture_needs_update_;
    ofxHapType type_;
    unsigned int width_;
    unsigned int height_;
};
