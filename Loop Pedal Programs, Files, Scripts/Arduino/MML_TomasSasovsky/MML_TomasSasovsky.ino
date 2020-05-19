#include <FastLED.h>

#define LED_PIN 2
#define NUM_LEDS 5
CRGB leds[NUM_LEDS];

// PINS FOR BUTTONS
int recPlayButton = 3;
int stopButton = 4;
int undoButton = 5;
int modeButton = 6;
int track1Button = 7;
int track2Button = 8;
int track3Button = 9;
int track4Button = 10;
int clearButton = 11;
int bankButton = 12;
int resetButton = A0;

int playMode = LOW;   // LOW = Rec mode, HIGH = Play Mode
int reading;           // reads the button
int previous = LOW;    // previous reading
long time = 0;         // the last time the output pin was toggled
long debounce = 150;   // the debounce time, increase if the output flickers (was 150)

String Tr1State = "empty";
String Tr2State = "empty";
String Tr3State = "empty";
String Tr4State = "empty";
int selectedTrack = 1;
double recTime;
boolean firstRecording = true;

String buttonpress = "";
String lastbuttonpress = "";
int postpressdelay = 300;
boolean calibrate = false;
int midichannel = 0x90;
double recordingTime;
String lastdone = "";

void setup() {
  Serial.begin(38400);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  pinMode (recPlayButton, INPUT_PULLUP);  
  pinMode (stopButton, INPUT_PULLUP);
  pinMode (undoButton, INPUT_PULLUP);
  pinMode (modeButton, INPUT_PULLUP); 
  pinMode (track1Button, INPUT_PULLUP);
  pinMode (track2Button, INPUT_PULLUP);
  pinMode (track3Button, INPUT_PULLUP);
  pinMode (track4Button, INPUT_PULLUP);
  pinMode (clearButton, INPUT_PULLUP);
  pinMode (bankButton, INPUT_PULLUP);
  pinMode (resetButton, INPUT_PULLUP);
  startLEDs();
  setLEDs();
}

void loop() {
  reading = digitalRead(modeButton);
  if (reading == LOW && previous == HIGH && millis() - time > debounce){
    if (playMode == LOW){
      playMode = HIGH;          //entering play mode
      Serial.write(midichannel);
      Serial.write(0x29);
      Serial.write(0x45);       //medium velocity = Note ON
    }else{
      playMode = LOW;           //entering rec mode
      Serial.write(midichannel);
      Serial.write(0x2B);
      Serial.write(0x45);       //medium velocity = Note ON
      if (Tr1State != "muted" && Tr1State != "empty") Tr1State = "playing";
      if (Tr2State != "muted" && Tr2State != "empty") Tr2State = "playing";
      if (Tr3State != "muted" && Tr3State != "empty") Tr3State = "playing";
      if (Tr4State != "muted" && Tr4State != "empty") Tr4State = "playing";
    }
    time = millis();
    setLEDs();
  }
  previous = reading;

  buttonpress = "released";
  if (digitalRead(recPlayButton) == LOW) buttonpress = "RecPlay";
  if (digitalRead(stopButton) == LOW) buttonpress = "Stop";
  if (digitalRead(undoButton) == LOW) buttonpress = "Undo";
  if (digitalRead(track1Button) == LOW) buttonpress = "Track1";
  if (digitalRead(track2Button) == LOW) buttonpress = "Track2";
  if (digitalRead(track3Button) == LOW) buttonpress = "Track3";
  if (digitalRead(track4Button) == LOW) buttonpress = "Track4";
  if (digitalRead(clearButton) == LOW) buttonpress = "Clear";
  if (digitalRead(bankButton) == LOW) buttonpress = "Bank";
  if (digitalRead(resetButton) == LOW) buttonpress = "Reset";

  if (buttonpress != lastbuttonpress && buttonpress != "released"){  //any mode
    if (buttonpress == "Stop"){
        if (Tr1State != "empty") Tr1State = "muted";
        if (Tr2State != "empty") Tr2State = "muted";
        if (Tr3State != "empty") Tr3State = "muted";
        if (Tr4State != "empty") Tr4State = "muted";
        setLEDs();
        sendNote(0x2A); 
      }
    if ((buttonpress == "Bank") && selectedTrack < 4){
      sendNote(0x20);
    }
    if (buttonpress == "Clear"){
      if(selectedTrack == 1 && Tr1State != "empty") Tr1State = "empty";
      if(selectedTrack == 2 && Tr2State != "empty") Tr2State = "empty";
      if(selectedTrack == 3 && Tr3State != "empty") Tr3State = "empty";
      if(selectedTrack == 4 && Tr4State != "empty") Tr4State = "empty";
      sendNote(0x1E);
    }    
    if (buttonpress == "Reset"){
      Tr1State = "empty";
      Tr2State = "empty";
      Tr3State = "empty";
      Tr4State = "empty";
      selectedTrack = 1;
      firstRecording = true;
      Serial.write(midichannel);
      Serial.write(0x2B);
      Serial.write(0x45);       //medium velocity = Note ON
      playMode = LOW;           //into record mode
      setLEDs();
      sendNote(0x1F);
    }
    if (buttonpress == "Undo"){
      if (Tr1State == "recording" || Tr1State == "overdubbing") Tr1State = "playing";
      if (Tr2State == "recording" || Tr2State == "overdubbing") Tr2State = "playing";
      if (Tr3State == "recording" || Tr3State == "overdubbing") Tr3State = "playing";
      if (Tr4State == "recording" || Tr4State == "overdubbing") Tr4State = "playing";
      sendNote(0x22);
    }
  }

  if (playMode == LOW){                                                                       //if the current mode is "Record Mode"
    if (buttonpress == "Track1"){
      if (Tr1State == "playing" || Tr1State == "empty"){
        Tr1State = "recording";
        if (Tr2State == "recording" || Tr2State == "overdubbing") Tr2State = "playing";
        if (Tr3State == "recording" || Tr3State == "overdubbing") Tr3State = "playing";
        if (Tr4State == "recording" || Tr4State == "overdubbing") Tr4State = "playing";
      }else if (Tr1State == "recording") Tr1State = "playing";
      firstRecording = false;
      selectedTrack = 1;
      setLEDs();
      sendNote(0x23);
    }
    if (buttonpress == "Track2"){
      if (Tr2State == "playing" || Tr2State == "empty"){
        Tr2State = "recording";
        if (Tr1State == "recording" || Tr1State == "overdubbing") Tr1State = "playing";
        if (Tr3State == "recording" || Tr3State == "overdubbing") Tr3State = "playing";
        if (Tr4State == "recording" || Tr4State == "overdubbing") Tr4State = "playing";
      }else if (Tr2State == "recording") Tr2State = "playing";
      firstRecording = false;
      selectedTrack = 2;
      setLEDs();
      sendNote(0x24);
    }
    if (buttonpress == "Track3"){
      if (Tr3State == "playing" || Tr3State == "empty"){
        Tr3State = "recording";
        if (Tr1State == "recording" || Tr1State == "overdubbing") Tr1State = "playing";
        if (Tr2State == "recording" || Tr2State == "overdubbing") Tr2State = "playing";
        if (Tr4State == "recording" || Tr4State == "overdubbing") Tr4State = "playing";
      }else if (Tr3State == "recording") Tr3State = "playing";
      firstRecording = false;
      selectedTrack = 3;
      setLEDs();
      sendNote(0x25);
    }
    if (buttonpress == "Track4"){
      if (Tr4State == "playing" || Tr4State == "empty"){
        Tr4State = "recording";
        if (Tr1State == "recording" || Tr1State == "overdubbing") Tr1State = "playing";
        if (Tr2State == "recording" || Tr2State == "overdubbing") Tr2State = "playing";
        if (Tr3State == "recording" || Tr3State == "overdubbing") Tr3State = "playing";
      }else if (Tr4State == "recording") Tr4State = "playing";
      firstRecording = false;
      selectedTrack = 4;
      setLEDs();
      sendNote(0x26);
    }
    if (buttonpress == "RecPlay"){ //problem, goes automatically to the second track and records
      if (selectedTrack == 1 && firstRecording){
        if (Tr1State != "recording" || Tr1State != "overdubbing"){
          Tr1State = "recording", buttonpress = "released";
          if (Tr2State == "recording" || Tr2State == "overdubbing") Tr2State = "playing";
          if (Tr3State == "recording" || Tr3State == "overdubbing") Tr3State = "playing";
          if (Tr4State == "recording" || Tr4State == "overdubbing") Tr4State = "playing";
        }else if (Tr1State == "recording") Tr1State = "overdubbing";
        else if (Tr1State == "overdubbing") Tr1State = "playing", firstRecording = false, selectedTrack = 2;
        setLEDs();
        sendNote(0x27);
      }
      if (selectedTrack == 1 && !firstRecording){
        if (Tr1State == "playing" || Tr1State == "empty"){
          Tr1State = "recording", buttonpress = "released";
          if (Tr2State == "recording" || Tr2State == "overdubbing") Tr2State = "playing";
          if (Tr3State == "recording" || Tr3State == "overdubbing") Tr3State = "playing";
          if (Tr4State == "recording" || Tr4State == "overdubbing") Tr4State = "playing";
        }else if (Tr1State == "recording") Tr1State = "playing", selectedTrack = 2;
        setLEDs();
        sendNote(0x27);
       }
       if (selectedTrack == 2 && firstRecording){
         if (Tr2State != "recording" || Tr2State != "overdubbing"){
           Tr2State = "recording", buttonpress = "released";
           if (Tr1State == "recording" || Tr1State == "overdubbing") Tr1State = "playing";
           if (Tr3State == "recording" || Tr3State == "overdubbing") Tr3State = "playing";
           if (Tr4State == "recording" || Tr4State == "overdubbing") Tr4State = "playing";
         }else if (Tr2State == "recording") Tr2State = "overdubbing";
         else if (Tr2State == "overdubbing") Tr2State = "playing", firstRecording = false, selectedTrack = 3;
         setLEDs();
         sendNote(0x27);
       }
       if (selectedTrack == 2 && !firstRecording){
         if (Tr2State == "playing" || Tr2State == "empty"){
           Tr2State = "recording", buttonpress = "released";
           if (Tr1State == "recording" || Tr1State == "overdubbing") Tr1State = "playing";
           if (Tr3State == "recording" || Tr3State == "overdubbing") Tr3State = "playing";
           if (Tr4State == "recording" || Tr4State == "overdubbing") Tr4State = "playing";
         }else if (Tr2State == "recording") Tr2State = "playing", selectedTrack = 3;
         setLEDs();
         sendNote(0x27);
       }
      if (selectedTrack == 3 && firstRecording){
         if (Tr3State != "recording" || Tr3State != "overdubbing"){
           Tr3State = "recording", buttonpress = "released";
           if (Tr1State == "recording" || Tr1State == "overdubbing") Tr1State = "playing";
           if (Tr2State == "recording" || Tr2State == "overdubbing") Tr2State = "playing";
           if (Tr4State == "recording" || Tr4State == "overdubbing") Tr4State = "playing";
        }else if (Tr3State == "recording") Tr3State = "overdubbing";
         else if (Tr3State == "overdubbing") Tr3State = "playing", firstRecording = false, selectedTrack = 4;
         setLEDs();
         sendNote(0x27);
      }
      if (selectedTrack == 3 && !firstRecording){
        if (Tr3State == "playing" || Tr3State == "empty"){
          Tr3State = "recording", buttonpress = "released";
          if (Tr1State == "recording" || Tr1State == "overdubbing") Tr1State = "playing";
          if (Tr2State == "recording" || Tr2State == "overdubbing") Tr2State = "playing";
          if (Tr4State == "recording" || Tr4State == "overdubbing") Tr4State = "playing";
        }else if (Tr3State == "recording") Tr3State = "playing", selectedTrack = 4;
        setLEDs();
        sendNote(0x27);
      }
      if (selectedTrack == 4 && firstRecording){
        if (Tr4State != "recording" || Tr4State != "overdubbing"){
          Tr4State = "recording", buttonpress = "released";
          if (Tr1State == "recording" || Tr1State == "overdubbing") Tr1State = "playing";
          if (Tr2State == "recording" || Tr2State == "overdubbing") Tr2State = "playing";
          if (Tr3State == "recording" || Tr3State == "overdubbing") Tr3State = "playing";
        }else if (Tr4State == "recording") Tr4State = "overdubbing";
        else if (Tr4State == "overdubbing") Tr4State = "playing", selectedTrack = 1, firstRecording = false;
        setLEDs();
        sendNote(0x27); 
        
      }
      if (selectedTrack == 4 && !firstRecording){
        if (Tr4State == "playing" || Tr4State == "empty"){
          Tr4State = "recording", buttonpress = "released";
          if (Tr1State == "recording" || Tr1State == "overdubbing") Tr1State = "playing";
          if (Tr2State == "recording" || Tr2State == "overdubbing") Tr2State = "playing";
          if (Tr3State == "recording" || Tr3State == "overdubbing") Tr3State = "playing";
        }else if (Tr4State == "recording") Tr4State = "playing", selectedTrack = 1;
        setLEDs();
        sendNote(0x27); 
      }
    }
  }

  if (playMode == HIGH){
    if (buttonpress == "RecPlay"){
      if (Tr1State != "empty") Tr1State = "playing";
      if (Tr2State != "empty") Tr2State = "playing";
      if (Tr3State != "empty") Tr3State = "playing";
      if (Tr4State != "empty") Tr4State = "playing";
      setLEDs();
      sendNote(0x31);
    }
    if (buttonpress == "Track1"){
      if (Tr1State == "muted") Tr1State = "playing";
      else Tr1State = "muted";
      selectedTrack = 1;
      setLEDs();
      sendNote(0x2C);
      }
    if (buttonpress == "Track2"){
      if (Tr2State == "muted") Tr2State = "playing";
      else Tr2State = "muted";
      selectedTrack = 2;
      setLEDs();
      sendNote(0x2D);
    }
    if (buttonpress == "Track3"){
      if (Tr3State == "muted") Tr3State = "playing";
      else Tr3State = "muted";
      selectedTrack = 3;
      setLEDs();
      sendNote(0x2E);
    }
    if (buttonpress == "Track4"){
      if (Tr4State == "muted") Tr4State = "playing";
      else Tr4State = "muted";
      selectedTrack = 4;
      setLEDs();
      sendNote(0x2F);
    }
  }
  lastbuttonpress = buttonpress;
}

void ringLEDs(){
    
}
void sendNote(int pitch){
  Serial.write(midichannel);
  Serial.write(pitch);
  Serial.write(0x45);  //medium velocity = Note ON
  delay(postpressdelay);
}
void setLEDs(){
  if (playMode == LOW) leds[0] = CRGB(255,0,0);
  if (playMode == HIGH) leds[0] = CRGB(0,255,0);
  
  if (Tr1State == "playing") leds[1] = CRGB(0,255,0);
  if (Tr1State == "recording" || Tr1State == "overdubbing") leds[1] = CRGB(255,0,0);
  if (Tr1State == "muted") leds[1] = CRGB(64,224,208);
  else leds[1] = CRGB(0,0,0);
  
  if (Tr2State == "playing") leds[2] = CRGB(0,255,0);
  if (Tr2State == "recording" || Tr2State == "overdubbing") leds[2] = CRGB(255,0,0);
  if (Tr2State == "muted") leds[2] = CRGB(64,224,208);
  else leds[2] = CRGB(0,0,0);

  if (Tr3State == "playing") leds[3] = CRGB(0,255,0);
  if (Tr3State == "recording" || Tr3State == "overdubbing") leds[3] = CRGB(255,0,0);
  if (Tr3State == "muted") leds[3] = CRGB(64,224,208);
  else leds[3] = CRGB(0,0,0);
  
  if (Tr4State == "playing") leds[4] = CRGB(0,255,0);
  if (Tr4State == "recording" || Tr4State == "overdubbing") leds[4] = CRGB(255,0,0);
  if (Tr4State == "muted") leds[4] = CRGB(64,224,208);
  else leds[4] = CRGB(0,0,0);
  FastLED.show();
}
void startLEDs(){
  for(int i = 0; i < 3; i++){
    leds[0, 1, 2, 3, 4] = CRGB(255,0,0);
    delay(250);
    leds[0, 1, 2, 3, 4] = CRGB(0,0,0);
    delay(250);
  }
  FastLED.show();
}
