#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    ofBackground(80);
    parameters.setName("settings");
    ofParameter<std::string> save_type_param("save_type", "hap");
    parameters.add(save_type_param);

    ofFile settings_file("settings.xml");
    if (settings_file.exists())
    {
        ofXml xml(settings_file.path());
        xml.deserialize(parameters);
    }
}

//--------------------------------------------------------------
void ofApp::update(){

}

//--------------------------------------------------------------
void ofApp::draw(){
    if (image.isLoaded())
    {
        ofSetColor(255, 255, 255);
        ofRectangle image_rect(0, 0, image.getWidth(), image.getHeight());
        ofRectangle drawable = ofGetWindowRect();
        drawable.y += 30;
        drawable.height -= 30;
        image_rect.scaleTo(drawable);

        image.draw(image_rect.x, image_rect.y, image_rect.width, image_rect.height);
    } else {
        ofDrawBitmapString("Drag images to the window to create Hap versions.", 10, 40);
        ofDrawBitmapString("Drag Hap images to the window to view them.", 10, 60);
    }
    std::string mode("Images will be saved as ");
    mode += ofxHapImage::imageTypeDescription(savedImageType());
    mode += ". Press 1, 2 or 3 to change the format.";
    ofDrawBitmapString(mode, 10, 20);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    bool changed = false;
    switch (key) {
        case '1':
            parameters["save_type"].cast<std::string>() = "hap";
            changed = true;
            break;
        case '2':
            parameters["save_type"].cast<std::string>() = "hap-alpha";
            changed = true;
            break;
        case '3':
            parameters["save_type"].cast<std::string>() = "hap-q";
            changed = true;
            break;
        default:
            break;
    }
    if (changed)
    {
        ofXml xml;
        xml.serialize(parameters);
        xml.save("settings.xml");
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {
    ofxHapImage::ImageType save_type = savedImageType();
    for (std::vector<std::string>::const_iterator it = dragInfo.files.begin(); it < dragInfo.files.end(); ++ it) {
        ofFile file(*it, ofFile::Reference);
        fileDropped(file, save_type);
    }
    if (dragInfo.files.size() > 0)
    {
        ofSetWindowTitle(ofFilePath::getFileName(dragInfo.files[0]));
    }
    else
    {
        ofSetWindowTitle("");
    }
}

ofxHapImage::ImageType ofApp::savedImageType()
{
    std::string type_string = parameters.getString("save_type");
    if (type_string == "hap-q")
    {
        return ofxHapImage::IMAGE_TYPE_HAP_Q;
    }
    else if (type_string == "hap-alpha")
    {
        return ofxHapImage::IMAGE_TYPE_HAP_ALPHA;
    }
    else
    {
        return ofxHapImage::IMAGE_TYPE_HAP;
    }
}

void ofApp::fileDropped(const ofFile& file, ofxHapImage::ImageType save_type)
{
    if (file.isDirectory())
    {
        ofDirectory directory(file.getAbsolutePath());
        directory.listDir();
        for (int i = 0; i < directory.size(); i++) {
            fileDropped(directory[i], save_type);
        }
    }
    else if (file.getExtension() == ofxHapImage::HapImageFileExtension())
    {
        image.loadImage(file);
    }
    else
    {
        std::string save_name = ofFilePath::removeExt(file.getAbsolutePath()) + "." + ofxHapImage::HapImageFileExtension();
        ofFile existing_save(save_name, ofFile::Reference);
        if (existing_save.exists())
        {
            /*
             Be lazy and use a previous save if one exists
             */
            image.loadImage(existing_save);
        }
        else
        {
            /*
             Convert the image to Hap and save it
             */
            ofImage original(file);
            image.loadImage(original, save_type);
            image.saveImage(save_name);
        }
    }
}
