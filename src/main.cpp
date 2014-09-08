#include "ofMain.h"
#include "ofxSimpleSerial.h"
#include <vector>
#include "ofxArtnet.h"
#include "ofxOsc.h"

#define MAX_BEAT_NUM 256
#define SYNC_TIME 70

class ofApp : public ofBaseApp{
    
    //Serial
    ofxSimpleSerial	serial;
    bool serialRead;
    
    //beat count
    vector<int> currentPeak;
    vector<int> beat_diff;
    vector<int> beat_value_a;
    vector<int> beat_value_b;
    vector<int> bpm_a;
    vector<int> bpm_b;
    int bpm_average_a;
    int bpm_average_b;
    
    vector<float> peak_a;
    vector<float> peak_b;
    
    long long old_peaktime_a;
    long long old_peaktime_b;
    
    //
    ofTrueTypeFont font;
    
    //Artnet
    ofxArtnet artnet;
    unsigned char dmxData[512];
    
    //
    int brightness;
    
    //OSC
    ofxOscSender sender;
    
    //Mute
    int mute = 1;
    
    
    //--------------------------------------------------------------
    void setup(){
        ofBackground(0);
        ofSetFrameRate(60);
        ofSetVerticalSync(true);
        
        setupSerial();
        
        currentPeak.resize(2);
        font.loadFont("vag.ttf", 240);
        old_peaktime_a = 0;
        old_peaktime_b = 0;
        bpm_average_a = 0;
        bpm_average_b = 0;
        
        
        //Artnet
        artnet.setup("10.20.24.19");
        for(int i = 0; i<512; i++) {
            dmxData[i] = 0;
        }
        
        brightness = 0;
        
        //Sender
        sender.setup("10.20.24.101", 3331);
    }
    
    void setupSerial() {
        serial.setup("/dev/tty.usbmodem641", 9600);
        serial.startContinuousRead();
        ofAddListener(serial.NEW_MESSAGE,this,&ofApp::onSerialMessage);
    }
    
    //--------------------------------------------------------------
    void update(){
        if(brightness > 0){
            brightness-=4;
        }
        
        updateBeatValue();
        updateBPM();
        updateArtnet();
        
        ofxOscMessage msg1,msg2;
        msg1.setAddress("/Ch1/");
        msg1.addIntArg(beat_value_a[0]*mute);
        msg1.addIntArg(bpm_average_a*mute);
        sender.sendMessage(msg1);
        msg2.setAddress("/Ch2/");
        msg2.addIntArg(beat_value_b[0]*mute);
        msg2.addIntArg(bpm_average_b*mute);
        sender.sendMessage(msg2);
    }
    
    void updateArtnet() {
        dmxData[0] = max(brightness,0)*mute;
        artnet.sendDmx("10.20.24.22", dmxData, 512);
    }
    
    void updateBPM() {
        long min,max;
        float norm;
        
        //a
        min = *min_element(beat_value_a.begin(), beat_value_a.end());
        max = *max_element(beat_value_a.begin(), beat_value_a.end())-min;
        if(max > 100000){
            max = 0;
        }
        norm = float(beat_value_a[0])/float(max);
        peak_a.insert(peak_a.begin(), norm);
        if(peak_a.size() > 16){
            peak_a.resize(16);
        }
        if(peak_a[7]>0.5 && peak_a[7] == (*max_element(peak_a.begin(), peak_a.end()))){
            unsigned long long current = ofGetElapsedTimeMillis();
            long diff = long(current - old_peaktime_a);
            int b = int(60000/diff);
            
            if(b > 30 && b < 150) {
                cout << "peak_a : " << b << endl;
                bpm_a.insert(bpm_a.begin(), b);
                if(bpm_a.size() > 3){
                    bpm_a.resize(3);
                }
                bpm_average_a = accumulate(bpm_a.begin(), bpm_a.end(), 0)/bpm_a.size();
                old_peaktime_a = current;
                
                if(abs(old_peaktime_a-old_peaktime_b) < SYNC_TIME){
                    cout << abs(old_peaktime_a-old_peaktime_b) << endl;
                    checkSync();
                }
            }
        }
        
        //b
        min = *min_element(beat_value_b.begin(), beat_value_b.end());
        max = *max_element(beat_value_b.begin(), beat_value_b.end())-min;
        norm = float(beat_value_b[0])/float(max);
        peak_b.insert(peak_b.begin(), norm);
        if(peak_b.size() > 16){
            peak_b.resize(16);
        }
        if(peak_b[7]>0.5 && peak_b[7] == (*max_element(peak_b.begin(), peak_b.end()))){
            unsigned long long current = ofGetElapsedTimeMillis();
            long diff = long(current - old_peaktime_b);
            int b = int(60000/diff);
            
            if(b > 30 && b < 150) {
                cout << "peak_b : " << b << endl;
                bpm_b.insert(bpm_b.begin(), b);
                if(bpm_b.size() > 3){
                    bpm_b.resize(3);
                }
                bpm_average_b = accumulate(bpm_b.begin(), bpm_b.end(), 0)/bpm_b.size();
                old_peaktime_b = current;
                
                if(abs(old_peaktime_a-old_peaktime_b) < SYNC_TIME){
                    cout << abs(old_peaktime_a-old_peaktime_b) << endl;
                    checkSync();
                }
            }
        }
        
        
    }
    void checkSync(){
        cout << "sync" << endl;
        if(mute==0)return;
        
        brightness += 255;
        
        ofxOscMessage msg;
        msg.setAddress("/sync");
        msg.addIntArg(bpm_average_a);
        msg.addIntArg(bpm_average_b);
        sender.sendMessage(msg);
    }
    
    void updateBeatValue(){
        beat_value_a.insert(beat_value_a.begin(), currentPeak[0]);
        beat_value_b.insert(beat_value_b.begin(), currentPeak[1]);
        
        beat_diff.insert(beat_diff.begin(), (1024-abs(beat_value_a[0]-beat_value_b[0]))*float(beat_value_a[0])/1024.0*float(beat_value_b[0])/1024.0);
        
        if(beat_value_a.size() > MAX_BEAT_NUM) {
            beat_value_a.resize(MAX_BEAT_NUM);
        }
        if(beat_value_b.size() > MAX_BEAT_NUM) {
            beat_value_b.resize(MAX_BEAT_NUM);
        }
        if(beat_diff.size() > MAX_BEAT_NUM) {
            beat_diff.resize(MAX_BEAT_NUM);
        }
        
    }
    
    
    //--------------------------------------------------------------
    void draw(){
        ofSetColor(255,255,255,100);
        ofSetLineWidth(1);
        ofLine(0, ofGetHeight()*7/8, ofGetWidth(), ofGetHeight()*7/8);
        ofLine(0, ofGetHeight()*7/8-ofGetHeight()/4*3, ofGetWidth(), ofGetHeight()*7/8-ofGetHeight()/4*3);
        
        drawWave();
        
        ofSetColor(255);
        
        font.drawString(ofToString(bpm_average_a), 100, ofGetHeight()-100);
        font.drawString(ofToString(bpm_average_b), ofGetWidth()-450, ofGetHeight()-100);
        
        
    }
    
    void drawWave() {
        ofSetLineWidth(3);
        int waveheight = ofGetHeight()/4*3;
        int step = ofGetWidth()/MAX_BEAT_NUM;
        int oldX = ofGetWidth();
        int oldY = ofGetHeight()/2;
        
        ofNoFill();
        ofSetColor(255, 0, 0);
        for (int i = 0; i < beat_value_a.size(); i++) {
            int newY = (ofGetHeight()*7/8)-(waveheight*beat_value_a[i])/1024;
            int newX = oldX-step;
            ofLine(oldX, oldY, newX, newY);
            oldX -= step;
            oldY = newY;
        }
        
        
        oldX = ofGetWidth();
        oldY = ofGetHeight()/2;
        ofSetColor(0, 255, 0);
        for (int i = 0; i < beat_value_b.size(); i++) {
            int newY = (ofGetHeight()*7/8)-(waveheight*beat_value_b[i])/1024;
            int newX = oldX-step;
            ofLine(oldX, oldY, newX, newY);
            oldX -= step;
            oldY = newY;
        }
        
        oldX = ofGetWidth();
        oldY = ofGetHeight()/2;
        ofSetColor(0, 0, 255);
        for (int i = 0; i < beat_diff.size(); i++) {
            int newY = (ofGetHeight()*7/8)-(waveheight*beat_diff[i])/1024;
            int newX = oldX-step;
            ofLine(oldX, oldY, newX, newY);
            oldX -= step;
            oldY = newY;
        }
    }
    
    //--------------------------------------------------------------
    void onSerialMessage(string & message) {
        vector<string> peak = ofSplitString(message, ",");
        if(peak.size() == 2) {
            currentPeak[0] = ofToInt(peak[0]);
            currentPeak[1] = ofToInt(peak[1]);
        }
        
    }
    
    void keyPressed(int key){
        if(key == 'f'){
            ofToggleFullscreen();
        }else if(key == ' ') {
            if(mute == 0){
                mute = 1;
            }else {
                mute = 0;
            }
            cout << mute << endl;
        }
    }
    
    void exit() {
        dmxData[0] = 0;
        artnet.sendDmx("10.20.24.22", dmxData, 512);
    }
};

//========================================================================
int main( ){
	ofSetupOpenGL(960,540, OF_WINDOW);
	ofRunApp( new ofApp());
}
