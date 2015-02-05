#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    save_type = ofxHapImage::IMAGE_TYPE_HAP; // TODO: allow config type
}

//--------------------------------------------------------------
void ofApp::update(){

}

//--------------------------------------------------------------
void ofApp::draw(){
    if (images.size() > 0)
    {
        ofSetColor(255, 255, 255);
        images[0].draw((ofGetWindowWidth() / 2) - (images[0].getWidth() / 2), (ofGetWindowHeight() / 2) - (images[0].getHeight() / 2));
    } else {
        ofDrawBitmapString("Drag images to the window to create Hap versions.", 10, 20);
        ofDrawBitmapString("Drag Hap images to the window to view them.", 10, 40);
    }
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

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
