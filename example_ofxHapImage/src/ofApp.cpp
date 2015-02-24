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
    if (images.size() > 0)
    {
        ofSetColor(255, 255, 255);
        ofRectangle image_rect(0, 0, images[0].getWidth(), images[0].getHeight());
        ofRectangle drawable = ofGetWindowRect();
        drawable.y += 30;
        drawable.height -= 30;
        image_rect.scaleTo(drawable);

        images[0].draw(image_rect.x, image_rect.y, image_rect.width, image_rect.height);
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
    images.clear();
    ofxHapImage::ImageType save_type = savedImageType();
    for (std::vector<std::string>::const_iterator it = dragInfo.files.begin(); it < dragInfo.files.end(); ++ it) {
        if (ofFilePath::getFileExt(*it) == ofxHapImage::HapImageFileExtension())
        {
            images.push_back(ofxHapImage(*it));
        }
        else
        {
            ofImage original(*it);
            ofxHapImage image(original, save_type);
            std::string name = ofFilePath::removeExt(*it) + "." + ofxHapImage::HapImageFileExtension();
            image.saveImage(name);
            images.push_back(image);
        }
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

