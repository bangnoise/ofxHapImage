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
     Is loaded
     */
    bool isLoaded() const;

    /*
     Save a Hap image
     */
    void saveImage(const std::string& fileName);

    void saveImage(ofBuffer& buffer);

    void saveImage(ofFile& file);

    /*
     Drawing
     */
    virtual void draw(float x, float y) const override;

    virtual void draw(float x, float y, float w, float h) const override;

    virtual float getHeight() const override;
    
    virtual float getWidth() const override;

    ofxHapImage::ImageType getImageType();

    virtual ofTexture& getTexture() override;

    virtual const ofTexture & getTexture() const override;

    /*
     When using the texture for IMAGE_TYPE_HAP_Q, drawing requires the use of this shader
     */
    ofShader& getShader() const;

    // This is ignored, we always use a texture
    virtual void setUseTexture(bool use_texture) override {};

    virtual bool isUsingTexture() const override { return true; };

private:
    bool saveImage(std::vector<char>& destination);
    void prepareTexture() const;
    ofBuffer dxt_buffer_;
    mutable ofTexture texture_;
    mutable ofShader shader_;
    mutable bool texture_needs_update_;
    ofxHapImage::ImageType type_;
    unsigned int width_;
    unsigned int height_;
};
