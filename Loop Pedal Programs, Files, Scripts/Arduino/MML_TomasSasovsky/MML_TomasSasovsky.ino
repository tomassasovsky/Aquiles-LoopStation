/*
Tomás Sasovsky 23 - 09 - 2020
*/

#define FASTLED_HAS_CLOCKLESS
#include <FastLED.h>    //library for controlling the LEDs
#include <EEPROM.h>    //Library to store the potentiometer values
#define DEBOUNCE 10  // how many ms to debounce, 5+ ms is usually plenty

byte buttons[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, A2};
#define buttonsQuantity sizeof(buttons)

//track if a button is just pressed, just released, or 'currently pressed' 
byte pressed[buttonsQuantity], justPressed[buttonsQuantity], justReleased[buttonsQuantity];
byte previousKeystate[buttonsQuantity], currentKeystate[buttonsQuantity];
byte lastPressed;

#define pinLEDs 2
#define qtyLEDs 19
#define typeOfLEDs WS2812B    //type of LEDs i'm using
#define nonRingLEDs 7
#define modeLED 12
#define Tr1LED 13
#define Tr2LED 14
#define Tr3LED 15
#define Tr4LED 16
#define clearLED 17
#define X2LED 18
#define dataPin A1
#define clockPin A0
#define swPin A2
CRGB LEDs[qtyLEDs]; 

int lastState;
int state;
byte counterLEDs = 0;
byte vol[4];

unsigned long time = 0;    //the last time the output pin was toggled
unsigned long lastClear = 0;
unsigned long timeVolume = 0;
unsigned long timeSleepMode = 0;
unsigned long lastLEDsMillis = 0;
unsigned long previousBreathe = 0;
#define debounceTime 150    //the debounce time, increase if the output flickers
#define doublePressClearTime 750

char State[4] = {'E', 'E', 'E', 'E'};

//the next tags are to make it simpler to select a track when used in a function
#define TR1 0
#define TR2 1
#define TR3 2
#define TR4 3

// constants for min and max PWM
#define minBrightness 15
#define maxBrightness 240

byte ringPosition = 0;  //the led ring has 12 leds. This variable is to set the position in which the first lit led is during the animation.
byte setRingColour = 0;    //yellow is 35/50 = 45, red is 0, green is 96
#define ringSpeed  54.6875    //46.875 62.5

/*
when Rec/Play is pressed in Play Mode, every track gets played, no matter if they are empty or not.
because of this, we need to keep an even tighter tracking of each of the track's states
so if Rec/Play is pressed in Play Mode and, for example, the track 3 is empty, the corresponding variable (Tr3PlayedWRecPlay) turns true
likewise, when the button Stop is pressed, every track gets muted, so every one of these variables turn false.
*/
bool playedWithRecPlay[4] = {false, false, false, false};
bool pressedInStop[4] = {false, false, false, false};    //when a track is pressed-in-stop, it gets focuslocked

byte selectedTrack = TR1;    //this variable is to keep constant track of the selected track
byte selectedTrackLED;
byte fadeValue = 100;
unsigned int x2timesPressed = 1;
bool x2pressed = false;
bool firstRecording = true;    //this track is only true when the pedal has not been used to record yet.
bool stopMode = false;
bool stopModeUsed = false;
bool playMode = false;    //false = Rec mode, true = Play Mode
bool sleepMode = false;
bool startLEDsDone = false;
bool doublePressClear = false;
bool doublePressPlay = false;
bool fadeDirection = true;    //State Variable for Fade Direction

#define midichannel 0x90
//31250 - 38400
void setup() {
  Serial.begin(31250);    //to use only the Arduino UNO (Atmega16u2). You must upload another bootloader to use it as a native MIDI-USB device.
  FastLED.addLeds<typeOfLEDs, pinLEDs, GRB>(LEDs, qtyLEDs);    //declare LEDs
  for (byte i = 0; i < buttonsQuantity; i++) pinMode(buttons[i], INPUT_PULLUP);
  pinMode(dataPin, INPUT_PULLUP);
  pinMode(clockPin, INPUT_PULLUP);
  pinMode(swPin, INPUT_PULLUP);
  pinMode(A3, INPUT_PULLUP);
  for (byte i = 0; i < 4; i++) vol[i] = min(vol[i], 127);
  if (digitalRead(A3) == LOW) {
    for (byte i = 0; i < 4; i++) EEPROM.write(i, 127);
  }
  for (byte i = 0; i < 4; i++) {
    if (EEPROM.read(i) < 128) vol[i] = EEPROM.read(i);
    else vol[i] = 127;
  }
  selectedTrack = min(selectedTrack, 3);
  x2timesPressed = min(x2timesPressed, 256);
  lastState = digitalRead(clockPin);
  sendNote(0x1F);    //resets the pedal
}

void loop() {
  if ((State[TR1] != 'E' || State[TR2] != 'E' || State[TR3] != 'E' || State[TR4] != 'E') && !stopMode) ringLEDs();    //the led ring spins when the pedal has been used.
  if (State[TR1] == 'E' && State[TR2] == 'E' && State[TR3] == 'E' && State[TR4] == 'E' && !firstRecording) reset();   //reset the pedal when every track is empty but it has been used (for example, when the only track playing is Tr1 and you clear it, it resets the whole pedal)
  if (digitalRead(11) == HIGH) doublePressClear = false; 

  if (pressedInStop[TR1] || pressedInStop[TR2] || pressedInStop[TR3] || pressedInStop[TR4]) stopModeUsed = true;
  else stopModeUsed = false;

  if (millis() - timeVolume > 3000) {
    if (firstRecording) {
      sendAllVolumes();
      timeVolume = millis();
    }
  }

  if (millis() - timeSleepMode > 600000) { //15000
    if (firstRecording && State[TR1] == 'E' && State[TR2] == 'E' && State[TR3] == 'E' && State[TR4] == 'E') {
      sleepMode = true;
      timeSleepMode = millis();
    }
  }

  if (millis() - time > debounceTime) {
    switch(pressedButton()) {
      case 0:
        if (sleepMode) resetSleepMode();
        else if (!playMode) RecPlay();
        else playRecPlay();
        doublePressClear = false;
        time = millis();
        break;
      case 1:
        if (sleepMode) resetSleepMode();
        else if (!playMode) recModeStop();
        else playModeStop();
        doublePressClear = false;
        doublePressPlay = false;
        lastPressed = 1;
        time = millis();
        break;
      case 2:
        if (sleepMode) resetSleepMode();
        else undoButton();
        doublePressClear = false;
        lastPressed = 2;
        time = millis();
        break;
      case 3:
        if (sleepMode) resetSleepMode();
        else modeButton();
        doublePressClear = false;
        doublePressPlay = false;
        lastPressed = 3;
        time = millis();
        break;
      case 4:
        if (sleepMode) resetSleepMode();
        else pressedTrack(TR1);
        doublePressClear = false;
        lastPressed = 4;
        time = millis();
        break;
      case 5:
        if (sleepMode) resetSleepMode();
        else pressedTrack(TR2);
        doublePressClear = false;
        lastPressed = 5;
        time = millis();
        break;
      case 6:
        if (sleepMode) resetSleepMode();
        else pressedTrack(TR3);
        doublePressClear = false;
        lastPressed = 6;
        time = millis();
        break;
      case 7:
        if (sleepMode) resetSleepMode();
        else pressedTrack(TR4);
        doublePressClear = false;
        lastPressed = 7;
        time = millis();
        break;
      case 8:
        if (sleepMode) resetSleepMode();
        else clearButton();
        break;
      case 9:
        if (sleepMode) resetSleepMode();
        else X2Button();
        doublePressClear = false;
        doublePressPlay = false;
        lastPressed = 9;
        time = millis();
        break;
      case 10:
        if (sleepMode) resetSleepMode();
        else nextTrack();
        doublePressClear = false;
        doublePressPlay = false;
        lastPressed = 10;
        time = millis();
        break;
      case 11:
        lastPressed = 11;
        time = millis();
        break;
      default:
        break;
    }
    resetButton();
  }
  rotaryEncoder();
  if (!startLEDsDone) startLEDs();
  else setLEDs();
}

void checkButtons() {
  static byte previousstate[buttonsQuantity];
  static byte currentState[buttonsQuantity];
  static unsigned long lastTime;
  byte index;
  if (millis() < lastTime) lastTime = millis();
  if ((lastTime + DEBOUNCE) > millis()) return;
  // ok we have waited DEBOUNCE milliseconds, lets reset the timer
  lastTime = millis();
  for (index = 0; index < buttonsQuantity; index++) {
    justPressed[index] = 0;       //when we start, we clear out the "just" indicators
    justReleased[index] = 0;
    currentState[index] = digitalRead(buttons[index]);   //read the button
    if (currentState[index] == previousstate[index]) {
      if ((pressed[index] == LOW) && (currentState[index] == LOW)) {
        // just pressed
        justPressed[index] = 1;
      } else if ((pressed[index] == HIGH) && (currentState[index] == HIGH)) {
        justReleased[index] = 1; // just released
      }
      pressed[index] = !currentState[index];  //remember, digital HIGH means NOT pressed
    }
    previousstate[index] = currentState[index]; //keep a running tally of the buttons
  }
} 
byte pressedButton() {
  byte thisSwitch = 255;
  checkButtons();  //check the switches &amp; get the current state
  for (byte i = 0; i < buttonsQuantity; i++) {
    currentKeystate[i]=justPressed[i];
    if (currentKeystate[i] != previousKeystate[i]) {
      if (currentKeystate[i]) thisSwitch=i;
    }
    previousKeystate[i]=currentKeystate[i];
  }
  return thisSwitch;
}
void sendNote(int note) {
  Serial.write(midichannel);    //sends the notes in the selected channel
  Serial.write(note);    //sends note
  Serial.write(0x45);    //medium velocity = Note ON
}
void setLEDs() {
  switch (selectedTrack) {
    case TR1: 
      selectedTrackLED = Tr1LED;
      break;
    case TR2:
      selectedTrackLED = Tr2LED;
      break;
    case TR3:
      selectedTrackLED = Tr3LED;
      break;
    case TR4:
      selectedTrackLED = Tr4LED;
      break;
    default:
      break;
  }

  if (digitalRead(11) == HIGH) LEDs[clearLED] = CRGB(0,0,0);    //turn OFF the Clear LED when the button is NOT pressed
  if (digitalRead(12) == HIGH) LEDs[X2LED] = CRGB(0,0,0);    //turn OFF the X2 LED when the button is NOT pressed
  
  if (State[TR1] == 'P') LEDs[Tr1LED] = CRGB(0,200,0);    //when track 1 is playing, turn on it's LED, colour green
  else if (State[TR1] == 'M' || State[TR1] == 'E') LEDs[Tr1LED] = CRGB(0,0,0);    //when track 1 is muted or empty, turn off it's LED
  
  if (State[TR2] == 'P') LEDs[Tr2LED] = CRGB(0,200,0);    //when track 2 is playing, turn on it's LED, colour green
  else if (State[TR2] == 'M' || State[TR2] == 'E') LEDs[Tr2LED] = CRGB(0,0,0);    //when track 2 is muted or empty, turn off it's LED
  
  if (State[TR3] == 'P') LEDs[Tr3LED] = CRGB(0,200,0);    //when track 3 is playing, turn on it's LED, colour green
  else if (State[TR3] == 'M' || State[TR3] == 'E') LEDs[Tr3LED] = CRGB(0,0,0);    //when track 3 is muted or empty, turn off it's LED
  
  if (State[TR4] == 'P') LEDs[Tr4LED] = CRGB(0,200,0);    //when track 4 is playing, turn on it's LED, colour green
  else if (State[TR4] == 'M' || State[TR4] == 'E') LEDs[Tr4LED] = CRGB(0,0,0);    //when track 4 is muted or empty, turn off it's LED
 
  if (stopModeUsed && stopMode) {
    for (byte i = 0; i <= 3; i++) {
      if (State[i] == 'M' && pressedInStop[i]) {
        switch (i) {
          case TR1: 
            LEDs[Tr1LED] = CRGB(0,200,0);
            break;
          case TR2: 
            LEDs[Tr2LED] = CRGB(0,200,0);
            break;
          case TR3:
            LEDs[Tr3LED] = CRGB(0,200,0);
            break;
          case TR4:
            LEDs[Tr4LED] = CRGB(0,200,0);
            break;
          default:
            break;
        }
      }
    }
  }

  if (!sleepMode) {
    if (!playMode) LEDs[modeLED] = CRGB(255,0,0);    //when we are in Record mode, the LED is red
    else LEDs[modeLED] = CRGB(0,200,0);    //when we are in Play mode, the LED is green
    if (State[selectedTrack] == 'P') LEDs[selectedTrackLED] = CRGB(0,breatheAnimation(),0);
    if (State[selectedTrack] == 'R' || State[selectedTrack] == 'O') LEDs[selectedTrackLED] = CRGB(breatheAnimation(),0,0); //when selected track is recording or overdubing, turn on it's LED, colour red
    else if (State[selectedTrack] == 'E' && !playMode) LEDs[selectedTrackLED] = CRGB(0,0,breatheAnimation());
  } else LEDs[modeLED] = CRGB(0,0,0);    //when we are in sleep mode
  
  //the setRingColour variable sets the LED Ring's colour. 0 is red, 60 is yellow(ish), 96 is green.
  if ((State[TR1] == 'R' || State[TR2] == 'R' || State[TR3] == 'R' || State[TR4] == 'R') && firstRecording) setRingColour = 0;    //sets the led ring to red (recording)
  else if ((State[TR1] == 'R' || State[TR2] == 'R' || State[TR3] == 'R' || State[TR4] == 'R') && !firstRecording) setRingColour = 60;    //sets the led ring to yellow (overdubbing)
  else if (State[TR1] == 'O' || State[TR2] == 'O' || State[TR3] == 'O' || State[TR4] == 'O') setRingColour = 60;    //sets the led ring to yellow (overdubbing)
  else if (((State[TR1] == 'M' || State[TR1] == 'E') && (State[TR2] == 'M' || State[TR2] == 'E') && (State[TR3] == 'M' || State[TR3] == 'E') && (State[TR4] == 'M' || State[TR4] == 'E')) && !firstRecording && !playedWithRecPlay[TR1] && !playedWithRecPlay[TR2] && !playedWithRecPlay[TR3] && !playedWithRecPlay[TR4]) stopMode = true;    //stop the spinning ring animation when all tracks are muted
  else setRingColour = 96;    //sets the led ring to green (playing)
  
  FastLED.show();    //updates the led states
}
void reset() {    //function to reset the pedal
  sendNote(0x1F);    //sends note that resets Mobius
  selectedTrack = TR1;    //selects the 1rst track
  sendAllVolumes();
  resetSleepMode();
  //empties the tracks:
  for (byte i = 0; i < 4; i++) {
    State[i] = 'E';
    pressedInStop[i] = false;
    playedWithRecPlay[i] = false;
  }
  doublePressClear = false;
  stopModeUsed = false;
  stopMode = false;
  firstRecording = true;    //sets the variable firstRecording to true
  playMode = false;    //into record mode
  ringPosition = 0;    //resets the ring position
  x2timesPressed = 1;
  x2pressed = false;
  startLEDsDone = false;
}
void startLEDs() {    //animation to show the pedal has been reset
  if (millis() - lastLEDsMillis > 150) {
    if (counterLEDs % 2 == 0 && counterLEDs < 6) fill_solid(LEDs, qtyLEDs, CRGB(0, 70, 63)), counterLEDs++;
    else if (counterLEDs % 2 != 0 && counterLEDs < 6) FastLED.clear(), counterLEDs++;
    FastLED.show();
    if (counterLEDs == 6) startLEDsDone = true, counterLEDs = 0;
    lastLEDsMillis = millis();
  }
  setRingColour = 0;    //sets the led ring colour to green
}
void ringLEDs() {    //function that makes the spinning animation for the led ring
  FastLED.setBrightness(255);
  EVERY_N_MILLISECONDS(ringSpeed) {    //this gets an entire spin in 3/4 of a second (0.75s) Half a second would be 31.25, a second would be 62.5
    fadeToBlackBy(LEDs, qtyLEDs - nonRingLEDs, 70); //Dims the LEDs by 64/256 (1/4) and thus sets the trail's length. Also set limit to the ring LEDs quantity and exclude the mode and track ones (qtyLEDs-7)
    LEDs[ringPosition] = CHSV(setRingColour, 255, 255);    //Sets the LED's hue according to the potentiometer rotation    
    ringPosition++;    //Shifts all LEDs one step in the currently active direction    
    if (ringPosition == qtyLEDs - nonRingLEDs) ringPosition = 0;    //If one end is reached, reset the position to loop around
    FastLED.show();    //Finally, display all LED's data (illuminate the LED ring)
  }
}
void sendAllVolumes() {
  for (byte i = 0; i < 4; i++) {
    Serial.write(176);    //176 = CC Command
    Serial.write(i);    // Which value: if the selected Track is 1, the value sent will be 1; If it's 2, the value 2 will be sent and so on.
    Serial.write(vol[i]);
  }
}
void sendVolume() {
  Serial.write(176);    //176 = CC Command
  Serial.write(selectedTrack);    // Which value: if the selected Track is 1, the value sent will be 1; If it's 2, the value 2 will be sent and so on.
  Serial.write(vol[selectedTrack]);
  EEPROM.write(selectedTrack, vol[selectedTrack]);
}
void RecPlay() {
  sendVolume();
  sendNote(0x27);    //send the note
  stopMode = false;
  if (firstRecording) {
    if (State[selectedTrack] != 'R') State[selectedTrack] = 'R';    //if the track is either empty or playing, record
    else State[selectedTrack] = 'O', firstRecording = false;    //if the track is recording, overdub
  } else {
    if (State[selectedTrack] == 'P' || State[selectedTrack] == 'E') {
      for (byte i = 0; i < 4; i++) {
        if (State[i] == 'R' || State[i] == 'O') State[i] = 'P'; //if another track is being recorded, stop and play it
      }
      State[selectedTrack] = 'O';   //if it's playing or empty, overdub
    } else if (State[selectedTrack] == 'R') State[selectedTrack] = 'P';
    else if (State[selectedTrack] == 'O') State[selectedTrack] = 'P';
    else if (State[selectedTrack] == 'M') State[selectedTrack] = 'R';    //if the track is muted, record
  }
}
void playRecPlay() {
  sendNote(0x31);    //send the note
  if (stopMode) {
    ringPosition = 0;    //if we are in stop mode, make the ring spin from the position 0
    if (pressedInStop[selectedTrack]) {
      fadeValue = maxBrightness;
      fadeDirection = false;
    } else {
      fadeValue = minBrightness;
      fadeDirection = true;
    }
  }
  if (!stopModeUsed) {    //if stop Mode hasn't been used, just play every track that isn't empty: 
    for (byte i = 0; i < 4; i++) {
      if (State[i] != 'E') State[i] = 'P';
      else if (State[i] == 'E') playedWithRecPlay[i] = true;
    }
  } else if (!doublePressPlay && stopModeUsed) {    //if stop mode has been used and the previous pressed button isn't RecPlay
    if (!pressedInStop[TR1] && !pressedInStop[TR2] && !pressedInStop[TR3] && !pressedInStop[TR4]) { //if none of the tracks has been pressed in stop, play every track that isn't empty:
      for (byte i = 0; i < 4; i++) {
        if (State[i] != 'E') State[i] = 'P';
        else if (State[i] == 'E') playedWithRecPlay[i] = true;
      }
    } else {    //if a track has been pressed and it isn't empty, play it
      for (byte i = 0; i < 4; i++) {
        if (pressedInStop[i]) State[i] = 'P';
      }
      doublePressPlay = true;
    }
  } else if (doublePressPlay && stopModeUsed) {    //if the previous pressed button is RecPlay, play every track that is not empty:
    if (checkPressedInStop(TR1) && checkPressedInStop(TR2) && checkPressedInStop(TR3) && checkPressedInStop(TR4)) {
      for (byte i = 0; i < 4; i++) {
        if (pressedInStop[i]) State[i] = 'P';
      }
    } else {
      for (byte i = 0; i < 4; i++) {
        if (State[i] != 'E') State[i] = 'P';
        else if (State[i] == 'E') playedWithRecPlay[i] = true;
      }
    }
  }
  stopMode = false;
  doublePressPlay = true;
}
void pressedTrack(byte track) {
  if (!playMode) {
    switch (track) {
      case TR1:
        sendNote(0x23);
        break;
      case TR2:
        sendNote(0x24);
        break;
      case TR3:
        sendNote(0x25);
        break;
      case TR4:
        sendNote(0x26);
        break;
      default:
        break;
    }
    selectedTrack = track;    //select the track
    stopMode = false;
    char prevState = State[track];
    for (byte i = 0; i < 4; i++) {
      if (State[i] == 'O') State[i] = 'P'; //if another track is being recorded, stop and play it
      else if (State[i] == 'R') State[i] = 'P', firstRecording = false;
    }
    if (prevState == 'E' || prevState == 'M') {    //if it's playing, empty or muted, record it
      if (prevState == 'M') fadeValue = minBrightness, fadeDirection = true;
      if (firstRecording) State[track] = 'R';
      else State[track] = 'O';
    } else if (prevState == 'P' || prevState == 'R') {
      State[track] = 'O', firstRecording = false;    //if the track is recording, play it
    }
    else if (prevState == 'O') {
      State[track] = 'P', firstRecording = false;    //if the track is overdubbing, play it
    }
  } else {
    if (State[track] != 'E') {
      switch (track) {
        case TR1:
          sendNote(0x2C);
          break;
        case TR2:
          sendNote(0x2D);
          break;
        case TR3:
          sendNote(0x2E);
          break;
        case TR4:
          sendNote(0x2F);
          break;
        default:
          break;
      }
      if (stopMode) {
        pressedInStop[track] = !pressedInStop[track];    //toggle between the track being pressed in stop mode and not
        selectedTrack = track;    //select the track
      } else {
        if (pressedInStop[track] && State[track] == 'P') {
          State[track] = 'M';
        }
        else if (pressedInStop[track] && State[track] == 'M') {
          doublePressPlay = true;
          State[track] = 'P';
          stopModeUsed = false;
          for (byte i = 0; i < 4; i++) pressedInStop[i] = false;
        }
        else if (!pressedInStop[track]) {    //if it wasn't pressed in stop
          if (State[track] == 'P') State[track] = 'M';    //and it is playing, mute it
          else if (State[track] == 'M') {    //else if it is muted, play it
            //reset everything in stopMode:
            State[track] = 'P';
            fadeValue = minBrightness;
            fadeDirection = true;
            stopModeUsed = false;
            for (byte i = 0; i < 4; i++) pressedInStop[i] = false;
          }
        }
      }
      selectedTrack = track;    //select the track
    }
  }
}
void recModeStop() {
  if (State[selectedTrack] != 'E' && !firstRecording) {
    sendNote(0x28);    //send the note
    State[selectedTrack] = 'M';
  }
}
void playModeStop() {
  sendNote(0x2A);    //send the note
  for (byte i = 0; i < 4; i++) {
    if (State[i] != 'E') State[i] = 'M'; //if another track is being recorded, stop and play it
    playedWithRecPlay[i] = false;
  }
  stopMode = true;    //make the stopMode variable and mode true
}
void undoButton() {
  if (!playMode && !stopMode && lastPressed != 1 && lastPressed != 11 && !doublePressPlay && State[selectedTrack] != 'R' && State[selectedTrack] != 'O') {
    if (x2timesPressed == 1 && !x2pressed) sendNote(0x22);    //send a note that undoes the last thing it did
    else if (x2timesPressed != 1) {
      sendNote(0x21);
      x2timesPressed = x2timesPressed/2;
      for (byte i = 0; i < 4; i++) {
        if (State[i] == 'M') State[i] = 'P'; //play a track if it is muted
        else if (State[i] == 'E') playedWithRecPlay[i] = "true";
      }
    }
  }
}
void modeButton() {
  if (playMode == false) {    //if the current mode is Record Mode
    sendNote(0x29);    //send a note so that the pedal knows we've change modes
    playMode = true;    //enter play mode
  } else {
    sendNote(0x2B);
    playMode = false;    //entering rec mode
    if (State[selectedTrack] != 'P' && State[selectedTrack] != 'M' && !pressedInStop[selectedTrack]) {
      fadeValue = minBrightness;
      fadeDirection = true;
    }
  }
  //if any of the tracks is recording when pressing the button, play them:
  for (byte i = 0; i < 4; i++) {
    if (State[i] != 'M' && State[i] != 'E') State[i] = 'P';
    pressedInStop[i] = false;
  }
  // reset the focuslock function:
  stopModeUsed = false;
}
void X2Button() {
  if (!stopModeUsed && !stopMode) {
    if (((State[selectedTrack] == 'P' || State[selectedTrack] == 'M' || State[selectedTrack] == 'E') && !firstRecording) && x2timesPressed < 256) {
      if (State[selectedTrack] == 'M') fadeValue = minBrightness, fadeDirection = false;
      sendNote(0x20);
      if (x2timesPressed != 256) LEDs[X2LED] = CRGB(0,0,255);    //turn ON the X2 LED when the button is pressed and the pedal has been used
      else LEDs[X2LED] = CRGB(255,0,0);
      x2timesPressed = x2timesPressed*2;
      x2pressed = true;
      for (byte i = 0; i < 4; i++) {
        if (State[i] == 'M') State[i] = 'P'; //play a track if it is muted
        else if (State[i] == 'E') playedWithRecPlay[i] = "true";
      }
    } else LEDs[X2LED] = CRGB(255,0,0);
  } else LEDs[X2LED] = CRGB(255,0,0);
}
void clearButton() {
  if (!stopModeUsed && !stopMode) {    //if we can clear the selected track, do it
    sendNote(0x1E);
    if (!firstRecording) {
      if (State[selectedTrack] == 'E') LEDs[clearLED] = CRGB(255,0,0);
      else if (State[selectedTrack] == 'P' || State[selectedTrack] == 'O' || State[selectedTrack] == 'R') LEDs[clearLED] = CRGB(0,0,255);
      else if (State[selectedTrack] != 'P' && State[selectedTrack] != 'E') LEDs[clearLED] = CRGB(255,0,0);
    } else reset(), lastClear = millis(), LEDs[clearLED] = CRGB(255,0,0);
    doublePressClear = true, lastClear = millis();    //activate the function that resets everything if we press the Clear button again
    if (State[selectedTrack] == 'P' || State[selectedTrack] == 'O' || (State[selectedTrack] == 'R' && !firstRecording)) {
      State[selectedTrack] = 'E';
      playedWithRecPlay[selectedTrack] = false;
      pressedInStop[selectedTrack] = false;
    } else doublePressClear = true, lastClear = millis();
  } else if (stopMode) doublePressClear = true, lastClear = millis(), LEDs[clearLED] = CRGB(255,0,0);
  else reset(), lastClear = millis(), LEDs[clearLED] = CRGB(255,0,0);
}
void resetButton() {
  if (millis() - lastClear > doublePressClearTime) {    //debounce
    if (digitalRead(11) == LOW && doublePressClear) {
      reset();    //reset everything when Clear is pressed twice
    }
  }
}
void rotaryEncoder() {
  state = digitalRead(clockPin);
  if (state != lastState) {
    if (digitalRead(dataPin) != state) vol[selectedTrack]++;
    else vol[selectedTrack]--;
    resetSleepMode();
    sendVolume();
  }
  lastState = state;
}
void nextTrack() {
  bool wasRecording = false;
  for (byte i = 0; i < 4; i++) {
    if (State[i] == 'R' || State[i] == 'O') {
      State[i] = 'P'; //if another track is being recorded, stop and play it
      wasRecording = true;
      firstRecording = false;
    }
  }
  if (selectedTrack < TR4) selectedTrack++;
  else selectedTrack = TR1;
  if (wasRecording) State[selectedTrack] = 'O';
  sendNote(0x32);
}
void resetSleepMode() {
  if (sleepMode) {
    fadeValue = minBrightness, fadeDirection = true;
    sleepMode = false;
  } 
  timeSleepMode = millis();
}
byte breatheAnimation() {
  if (millis() - previousBreathe >= 1) {
    if (fadeDirection) {
      fadeValue++;
      if (fadeValue >= maxBrightness) fadeValue = maxBrightness, fadeDirection = false;
    } else {
      fadeValue--;
      if (fadeValue <= minBrightness) fadeValue = minBrightness, fadeDirection = true;
    }
    previousBreathe = millis();
  }
  return fadeValue;
}
bool checkPressedInStop(byte track) {
  if (State[track] != 'E') {
    if (pressedInStop[track]) {
      if (State[track] == 'M') return true;
      else return false;
    } else return true;
  } else return true;
}