#pragma once


#define PORT 7771


#include "ofMain.h"
#include "ofxOsc.h"

class ofApp : public ofBaseApp{
	protected:
		std::list<ofVideoPlayer*> videos;
		ofxOscReceiver receiver;
		ofVideoPlayer* currentPlayer;
		// bool isVideoEnding(const ofVideoPlayer& video);
	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

};
