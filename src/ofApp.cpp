#include "ofApp.h"
#include <functional>

//--------------------------------------------------------------
void ofApp::setup(){
        receiver.setup(PORT);
}

//--------------------------------------------------------------
void ofApp::update(){
        while (receiver.hasWaitingMessages()) {
                ofxOscMessage m;
                receiver.getNextMessage(&m);

                if (m.getAddress() == "/play") {
                        string sample = m.getArgAsString(3);

                        ofVideoPlayer* video = new ofVideoPlayer();

                        video->load(sample + ".mpg");

                        videos.push_back(video);

                        // video->play();

                        ofLogNotice("/play") << sample;
                }
                else {
                        ofLogWarning("Unknown address called") << m.getAddress();
                }
        }

        for (std::list<ofVideoPlayer*>::iterator it = videos.begin(); it != videos.end(); it++) {
                ofLogNotice("update") << (*it)->getMoviePath();
                (*it)->update();
        }

        if (!videos.empty()) {
                ofVideoPlayer* next = videos.front();

                while (next && ((next->getCurrentFrame() + videos.size()) >= next->getTotalNumFrames())) {
                        ofLogNotice("stop:") << next->getMoviePath();
                        videos.pop_front();
                        next->closeMovie();
                        next = videos.front();
                }

                if (next && next->isLoaded()) {
                        videos.pop_front();

                        videos.push_back(next);

                        ofLogNotice("play:") << next->getMoviePath() << "@" << next->getCurrentFrame() << "/" << next->getTotalNumFrames() << "(" << videos.size() << ")";
                        // int nextFrame = next->getCurrentFrame() + videos.size();
                        // next->nextFrame();
                        // next->setFrame(nextFrame);
                        // ofLogNotice("now:") << nextFrame;
                        currentPlayer = next;
                }
        }
}

//
// bool ofApp::isVideoEnding(const ofVideoPlayer& video) {
//   int current = video.getCurrentFrame();
//   int max = video.getTotalNumFrames();
//
//   return ((current + videos.size()) <= max);
// }

//--------------------------------------------------------------
void ofApp::draw(){
        if (currentPlayer) {
          ofLogNotice("drawing") << currentPlayer->getCurrentFrame();
                currentPlayer->nextFrame();
                currentPlayer->draw(0,0);
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
void ofApp::dragEvent(ofDragInfo dragInfo){

}
