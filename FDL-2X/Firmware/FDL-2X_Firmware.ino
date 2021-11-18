#include <MicroView.h>
#include <Encoder.h>
#include <Servo.h>
#include <EEPROM.h>

Encoder myEnc(2, 3);
Servo flywheelESC; 

//PINS
int pusherBrakePin = 0;
int pusherEnablePin = 1;
int escPin = 5;
int buzzerPin = 6;
int triggerPin = A0;
int pusherSwitchFPin = A1;
int pusherSwitchRPin = A2;
int voltMeterPin = A3;
int resetBtnPin = A4;
int menuBtnPin = A5;

//MENUS
#define batteryCheckLength 6
float batteryCheck[batteryCheckLength];
unsigned long lastBatteryCheck = 0;
unsigned long batteryCheckDelay = 300;
int batteryCheckIndex = 0;
float batteryCheckSum = 0.0;

#define knobMenuLength 12
String knobMenuArray[knobMenuLength] = 
{
  "Speed  ",
  "ROF    ",
  "Burst  ",
  "Counter", 
  "MinSpin",
  "MaxSpin", 
  "MinSpd ",
  "MaxSpd ",
  "Load   ",
  "Save   ",
  "Bright ",
  "Sound "
};
int knobMenuIndex = 0;

#define burstMenuLength 4
String burstMenuArray[burstMenuLength] = {"1","2","3","F"};
int burstMenuIndex = 0;

#define soundMenuLength 2
String soundMenuArray[soundMenuLength] = {"ON ","OFF"};
int soundMenuIndex = 0;

#define presetMenuLength 4
String presetMenuArray[presetMenuLength] = {"Back"," 1  "," 2  "," 3  "};
int presetMenuIndex = 0;



MicroViewWidget *mainGauge;
MicroViewWidget *voltMeter;

int versionNumber = 1;

int speedValue = 50;
int rofValue = 100;
int counterValue = 0;
int magSizeValue = 18;
int brightnessValue = 100;
int minSpeedValue = 0;
int maxSpeedValue = 100;

int minSpinupValue = 300;
int maxSpinupValue = 180;

bool liveKnobScrollMode = false;
bool countResetWasDown = false;
bool menuBtnWasDown = false;
bool firstMenuRun = true;

bool speedLockedOut = false;

bool firstRun = true;

unsigned long lastTriggerUp = 0;
unsigned long lastSettingsSave = 0;
unsigned long lastBatAlarm = 0;

long encoderChange = 0;

float vPow = 5.12; //5.27
float r1 = 100000;
float r2 = 10000;

struct BlasterSettings {
  int versionNumber;
  int speedValue;
  int rofValue;
  int burstCount;
  int minSpinupValue;
  int maxSpinupValue;
  int minSpeed;
  int maxSpeed;
  int brightness;
  bool soundOn;
  int magSize;
};
BlasterSettings currentSettings = { 1, 80, 100, 1, 300, 180, 0, 100, 100, true, 18 }; 

BlasterSettings presets[4] = 
{
  { 1, 80, 100, 1, 300, 180, 0, 100, 100, true, 18 },
  { 1, 30, 50, 1, 300, 180, 0, 50, 0, true, 6 },
  { 1, 50, 100, 3, 300, 180, 0, 70, 50, false, 12 },
  { 1, 80, 100, 2, 300, 180, 0, 100, 100, true, 18 }
};


void setup() {

  flywheelESC.attach(escPin); 
  flywheelESC.writeMicroseconds(1000);
  
  uView.begin();// start MicroView
  uView.clear(PAGE);// clear page
  uView.display();

  pinMode(voltMeterPin, INPUT);
  
  pinMode(menuBtnPin, INPUT_PULLUP);
  pinMode(resetBtnPin, INPUT_PULLUP);
  pinMode(triggerPin, INPUT_PULLUP);
  pinMode(pusherSwitchFPin, INPUT_PULLUP);
  pinMode(pusherSwitchRPin, INPUT_PULLUP);

  pinMode(pusherEnablePin, OUTPUT);
  pinMode(pusherBrakePin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  //uView.drawBitmap(bitmapTest);
  //uView.display();

  renderSplash();

  mainGauge = new MicroViewGauge(16,30,0,100,WIDGETSTYLE0 + WIDGETNOVALUE);
  voltMeter = new MicroViewSlider(56, 14, 109, 126, WIDGETSTYLE2 + WIDGETNOVALUE);

  mainGauge->reDraw();
  voltMeter->reDraw();
  
  initSettings();
  loadSettings(0);

  //init battery average read values
  for(int x = 0; x < batteryCheckLength; x++){
    batteryCheck[x] = 12.0;
  }
  lastBatteryCheck = millis();
  batteryCheckSum = 12.0 * batteryCheckLength;
  
  renderScreen();
  delay(350);
  
  tone(6, 2000, 350);

  if(counterResetDown()){
    delay(500);

    tone(6, 3000, 200);
    delay(300);
    tone(6, 3000, 200);

    speedLockedOut = true;
  }
}


void loop() {

  //check for trigger
  if(triggerDown()){
    fireSequence();
  }
  else{
    flywheelESC.writeMicroseconds(1000);
  }

  //check for counter reset
  if(counterResetDown() && !speedLockedOut){
    if(!countResetWasDown){
      counterValue = magSizeValue;
    }
    countResetWasDown = true;
  }
  else{
    countResetWasDown = false;
  }
  
  renderScreen();

  if(lastSettingsSave != 0 && millis() - lastSettingsSave < 1000){
     //don't save
  }
  else{
     writeSettings(0);
     lastSettingsSave = millis();
  }
  
}

void writeSettings(int presetNumber){

  if((currentSettings.versionNumber != versionNumber||
  currentSettings.speedValue != speedValue ||
  currentSettings.rofValue != rofValue ||
  currentSettings.burstCount != getBurstCount() ||
  currentSettings.minSpinupValue != minSpinupValue ||
  currentSettings.maxSpinupValue != maxSpinupValue ||
  currentSettings.minSpeed != minSpeedValue ||
  currentSettings.maxSpeed != maxSpeedValue ||
  currentSettings.magSize != magSizeValue ||
  currentSettings.brightness != brightnessValue ||
  currentSettings.soundOn != (soundMenuIndex == 0)) ||
  presetNumber > 0){
    currentSettings = { 
      versionNumber, 
      speedValue,
      rofValue,
      getBurstCount(),
      minSpinupValue,
      maxSpinupValue,
      minSpeedValue,
      maxSpeedValue,
      brightnessValue,
      soundMenuIndex == 0,
      magSizeValue};

    int memLoc = presetNumber * 100;
      
    EEPROM.put(memLoc, currentSettings);
    //if(soundMenuIndex == 0){
    //  tone(6,3000,50);
    //}
  }
}

void loadSettings(int presetNumber){

  int memLoc = presetNumber * 100;
  EEPROM.get(memLoc, currentSettings);
    
  //if EEPROM empty all values will come back -1
  if(currentSettings.versionNumber == -1 || currentSettings.versionNumber != versionNumber){
      currentSettings = { 1, 80, 100, 1, 300, 150, 0, 100, 100, true, 18 };
  }
  
  speedValue = currentSettings.speedValue;
  rofValue = currentSettings.rofValue;
  
  int readBurst = constrain(currentSettings.burstCount, 1, 4);
  burstMenuIndex = readBurst - 1;
  
  minSpinupValue = currentSettings.minSpinupValue;
  maxSpinupValue = currentSettings.maxSpinupValue;
  minSpeedValue = currentSettings.minSpeed;
  maxSpeedValue = currentSettings.maxSpeed;
  magSizeValue = currentSettings.magSize;
  brightnessValue = currentSettings.brightness;

  if(presetNumber == 0){
    soundMenuIndex = currentSettings.soundOn ? 0 : 1;
  }
  
}

void savePreset(int presetIndex){

  writeSettings(presetIndex);
  
  tone(6, 2000, 50);
  delay(100);
  tone(6, 2000, 100);
  
}

void loadPreset(int presetIndex){

  loadSettings(presetIndex);
  
  tone(6, 4000, 50);
  delay(100);
  tone(6, 4000, 100);
  
}

void initSettings(){

  BlasterSettings initTestStruct;

  for(int i = 0; i < 4; i++){

    int memLoc = i * 100;
    
    EEPROM.get(memLoc, initTestStruct);
    
    ////if EEPROM empty all values will come back -1
    if(initTestStruct.versionNumber == -1){
        initTestStruct = presets[i];
        EEPROM.put(memLoc, initTestStruct);
    }
  }
}


bool fireSequence(){
    
    long currMills = millis();
    
    int kickonSpeed = readESCPower();
    //flywheelESC.write(kickonSpeed); 
    flywheelESC.writeMicroseconds(kickonSpeed);

    int spinupDelay = getSpinup();

    //tone(6, 3000);
    
    if(lastTriggerUp != 0 && millis() - lastTriggerUp < 1000){
       delay(10);
    }
    else{
       delay(spinupDelay); 
    }

    //noTone(6);
    
    int burstCount = getBurstCount();

    long counter = 0;
    
    while(burstCount > 1 && triggerDown()){
        
        int burstBreakDelay = map(rofValue, 0, 100, 100, 0);
        
        counter = 50;
        while(digitalRead(pusherSwitchFPin) == HIGH && counter > 0){
            digitalWrite(pusherEnablePin, HIGH);
            delay(6);
            counter--;
        }
        
        counter = 50;
        while(digitalRead(pusherSwitchFPin) == LOW && counter > 0){
            digitalWrite(pusherEnablePin, HIGH);
            delay(6);
            counter--;
        }
        
        burstCount--;
        
        if(burstBreakDelay > 5){
            digitalWrite(pusherEnablePin, LOW);
            delayMicroseconds(200);
            digitalWrite(pusherBrakePin, HIGH);
            delay(burstBreakDelay);
            digitalWrite(pusherBrakePin, LOW);
            delayMicroseconds(200);
        }
        
        counterValue--;
        counterValue = max(counterValue, 0);
        renderScreen();
    }

    //tone(6, 4000);

    counter = 50;
    while(digitalRead(pusherSwitchFPin) == HIGH && counter > 0){
      digitalWrite(pusherEnablePin, HIGH);
      delay(5);
      counter--;
    }
    
    digitalWrite(pusherEnablePin, LOW);
    
    delayMicroseconds(500);
    
    digitalWrite(pusherBrakePin, HIGH);
    delay(30);
    digitalWrite(pusherBrakePin, LOW);
    
    delayMicroseconds(500);

    digitalWrite(pusherEnablePin, HIGH);
    delayMicroseconds(16000);
    digitalWrite(pusherEnablePin, LOW);
    
    counter = 1000;
    while(digitalRead(pusherSwitchRPin) == HIGH && counter > 0){
        delayMicroseconds(200);
        counter--;
    }
    
    digitalWrite(pusherEnablePin, LOW);
    flywheelESC.writeMicroseconds(1000);

    delayMicroseconds(200);
    
    digitalWrite(pusherBrakePin, HIGH);
    delay(30);
    digitalWrite(pusherBrakePin, LOW);
    
    delayMicroseconds(200);

    counterValue--;
    counterValue = max(counterValue, 0);

    int toneFreq = 4000;

    renderScreen();
    
    while(triggerDown()){};
    
    lastTriggerUp = millis();
    
    //noTone(6);
    
    return true;
}

boolean triggerDown(){
  return digitalRead(triggerPin) == LOW;
}

boolean counterResetDown(){
  return digitalRead(resetBtnPin) == LOW;
}

int readESCPower(){
  return map(speedValue, 0, 100, 1285, 1860);
}

int getSpinup(){
  return map(speedValue, 0, 100, minSpinupValue, maxSpinupValue);
}

int getBurstCount(){
  if(burstMenuIndex >= burstMenuLength - 1){
    return 100;
  }
  else{
    return burstMenuIndex + 1;
  }
}

int getROF(){
  return rofValue;
}


/////////////
///RENDER
/////////////
void renderScreen(){

  int contrastValue = map(brightnessValue, 0, 100, 0, 255);
  uView.contrast(contrastValue);

  if(speedLockedOut){
    renderLockIndicator();
  }
  else{
    renderCounter();
  }
  
  renderVoltMeter();
    
  if(liveKnobScrollMode){
    renderKnobScrollMenu();
  }
  else{
    switch(knobMenuIndex){
      case 0:
        renderGauge(speedValue, "Speed", 0, 100, minSpeedValue, maxSpeedValue);
        break;
      case 1:
        renderGauge(rofValue, "ROF", 0, 100, 0, 100);
        break;
      case 2:
        renderBurstMenu();
        break;
      case 3:
        renderCounterMenu();
        break;
      case 4:
        if(!speedLockedOut){
          renderGauge(minSpinupValue, "Spn Min", 100, 500, 100, 500);
        }
        break;
      case 5:
        if(!speedLockedOut){
          renderGauge(maxSpinupValue, "Spn Max", 100, 500, 100, 500);
        }
        break;
      case 6:
        if(!speedLockedOut){
          renderGauge(minSpeedValue, "Spd Min", 0, 100, 0, maxSpeedValue);
        }
        break;
      case 7:
        if(!speedLockedOut){
          renderGauge(maxSpeedValue, "Spd Max", 0, 100, minSpeedValue, 100);
        }
        break;
      case 8:
        if(!speedLockedOut){
          renderPresetMenu(presetMenuIndex, "Load", presetMenuArray, presetMenuLength);
        }
        break;
      case 9:
        if(!speedLockedOut){
          renderPresetMenu(presetMenuIndex, "Save", presetMenuArray, presetMenuLength);
        }
        break;
      case 10:
        renderGauge(brightnessValue, "Bright ", 0, 100, 0, 100);
        break;
      case 11:
        renderSoundMenu();
        break;
      default:
        break;
    }
  }
  

  //look for rot switch press
  if(!digitalRead(menuBtnPin)){ 
    if(!menuBtnWasDown){

      if(liveKnobScrollMode){
        //tone(6,2000,50);
      }
      else{
        if(knobMenuIndex == 8 && presetMenuIndex > 0){
          loadPreset(presetMenuIndex); 
          presetMenuIndex = 0;
        }
        if(knobMenuIndex == 9  && presetMenuIndex > 0){
          savePreset(presetMenuIndex); 
          presetMenuIndex = 0; 
        }
      }

      liveKnobScrollMode = !liveKnobScrollMode;

      if(speedLockedOut && knobMenuIndex >= 4 && knobMenuIndex <= 9){
        liveKnobScrollMode = true;
      }
      
      uView.clear(PAGE);
      firstMenuRun = true;
      myEnc.write(0);
    }
    menuBtnWasDown = true;
  }
  else{
    menuBtnWasDown = false;
  }
  
}





void renderVoltMeter(){
  
  if(firstMenuRun){
    voltMeter->reDraw();
  }

  if(millis() > lastBatteryCheck + batteryCheckDelay){

    float v = (analogRead(voltMeterPin) * vPow) / 1024.0;
    float v2 = v / (r2 / (r1 + r2));

    v2 *= 10;
    int v2Int = (int)v2;
    v2 = (float)v2Int / 10;

    //uView.setCursor(34,0);
    //uView.print(v2);
    //tone(6, 2000, 20);

    batteryCheckSum -= batteryCheck[batteryCheckIndex];
    batteryCheck[batteryCheckIndex] = v2;
    batteryCheckSum += v2;
    batteryCheckIndex++;
    if(batteryCheckIndex >= batteryCheckLength){
      batteryCheckIndex = 0;
    }
    lastBatteryCheck = millis();
    
  }

  float voltLevel = batteryCheckSum / batteryCheckLength;
  voltMeter->setValue(voltLevel * 10);

  if(voltLevel < 11.0){
    if(lastBatAlarm + 3000 < millis()){
      batteryWarning();
      lastBatAlarm = millis();
    }
  }

  if(voltLevel < 10.9){
    if(lastBatAlarm + 1500 < millis()){
      batteryWarning();
      lastBatAlarm = millis();
    }
  }
  
  if(voltLevel < 10.8){
    if(lastBatAlarm + 1000 < millis()){
      batteryCritical();
      lastBatAlarm = millis();
    }
  }

  if(voltLevel < 10.7){
    while(true){
      digitalWrite(pusherEnablePin, LOW);
      flywheelESC.writeMicroseconds(1000);
      
      batteryCritical();
      delay(200);
    }
  }

  uView.setCursor(56,39);
  uView.print("B");
    
}

void batteryWarning(){
  tone(6, 2400);
  delay(140);
  tone(6, 4000);
  delay(140);
  tone(6, 1200);
  delay(200);
  noTone(6);
}

void batteryCritical(){
  tone(6, 2400, 140);
  delay(200);
  tone(6, 4000, 140);
  delay(200);
  tone(6, 2400, 140);
  delay(200);
  tone(6, 4000, 140);
}

void renderCounter(){

  uView.setCursor(34,0);
  
  if(counterValue < 0){
    counterValue = 0;
  }

  //if(counterValue <= 0){
  //  uView.print("EMPTY");
  //}
  //else{
    if(counterValue < 10){
      uView.print(" ");
    }
    if(magSizeValue < 10){
      uView.print(" ");
    }
    uView.print(counterValue);
    uView.print("/");
    uView.print(magSizeValue);
    uView.setFontType(0);
  //}
  
    
}

void renderLockIndicator(){

  uView.setCursor(34,0);
  uView.print("SLOCK");
  uView.setFontType(0);
    
}

void renderBurstMenu(){

  encoderChange += myEnc.read();
  myEnc.write(0);
  
  if(abs(encoderChange) >= 4){
    
    burstMenuIndex += encoderChange / 4;
    burstMenuIndex = constrain(burstMenuIndex, 0, 3);
    encoderChange = 0;
  }

  uView.setCursor(6,14);
  uView.print("Burst");

  uView.setFontType(1);
  uView.setCursor(18,26);
  uView.print(burstMenuArray[burstMenuIndex]);
  
  uView.display();
  firstMenuRun = false;
  uView.setFontType(0);
  
}

void renderSoundMenu(){

  encoderChange += myEnc.read();
  myEnc.write(0);
  
  if(abs(encoderChange) >= 4){
    
    soundMenuIndex += encoderChange / 4;
    soundMenuIndex = constrain(soundMenuIndex, 0, soundMenuLength - 1);
    encoderChange = 0;
  }

  uView.setCursor(6,14);
  uView.print("Sound");

  uView.setFontType(1);
  uView.setCursor(18,26);
  uView.print(soundMenuArray[soundMenuIndex]);
  
  uView.display();
  firstMenuRun = false;
  uView.setFontType(0);
  
}

void renderPresetMenu(int &menuIndex, String label, String menu[], int menuLength){

  encoderChange += myEnc.read();
  myEnc.write(0);
  
  if(abs(encoderChange) >= 4){
    
    menuIndex += encoderChange / 4;
    menuIndex = constrain(menuIndex, 0, menuLength - 1);
    encoderChange = 0;
  }

  uView.setCursor(10,14);
  uView.print(label);

  uView.setFontType(1);
  uView.setCursor(10,26);
  uView.print(menu[menuIndex]);
  
  uView.display();
  firstMenuRun = false;
  uView.setFontType(0);
  
}



void renderCounterMenu(){

  encoderChange += myEnc.read();
  myEnc.write(0);
  
  if(abs(encoderChange) >= 4){
    magSizeValue += encoderChange / 4;
    magSizeValue = constrain(magSizeValue, 1, 50);
    encoderChange = 0;
  }

  uView.setCursor(2,10);
  uView.print("Counter");

  uView.setFontType(1);
  uView.setCursor(10,26);

  if(magSizeValue < 10){
    uView.print(" ");
  }
  
  uView.print(magSizeValue);
  
  uView.display();
  firstMenuRun = false;
  uView.setFontType(0);
  
}


void renderKnobScrollMenu(){

  encoderChange += myEnc.read();
  myEnc.write(0);
  
  if(abs(encoderChange) >= 8){
    knobMenuIndex += encoderChange / 8;//was 4
    knobMenuIndex = constrain(knobMenuIndex, 0, knobMenuLength - 1);
    encoderChange = 0;
    if(soundMenuIndex == 0){
      tone(6,2000,10);
    }
  }

  uView.setCursor(0,0);
  uView.print("Mode");

  //uView.setFontType(1);
  uView.setCursor(8,26);
  uView.print(knobMenuArray[knobMenuIndex]);
  
  uView.display();
  firstMenuRun = false;
  uView.setFontType(0);
  
}


void renderGauge(int &gaugeValue, String label, int gaugeMin, int gaugeMax, int valueMin, int valueMax){

  mainGauge->setMinValue(gaugeMin);
  mainGauge->setMaxValue(gaugeMax);
  
  if(firstMenuRun){
    mainGauge->reDraw();
    myEnc.write(gaugeValue);
    firstMenuRun = false;
  }

  uView.setCursor(0,4);
  uView.print(label);
  
  gaugeValue = myEnc.read();
  if(gaugeValue > valueMax || gaugeValue < valueMin){
    gaugeValue = constrain(gaugeValue, valueMin, valueMax);
    myEnc.write(gaugeValue);
  }

  uView.setCursor(30,40);
  
  uView.print(gaugeValue);
  if(gaugeValue < 10){
    uView.print(" ");
  }
  if(gaugeValue < 100){
    uView.print(" ");
  }

  mainGauge->setValue(gaugeValue);
  uView.display();
}


void renderSplash(){

  uView.setFontType(1);
  uView.setCursor(6,14);
  uView.print("F");
  uView.display();
  delay(50);
  uView.print("D");
  uView.display();
  delay(50);
  uView.print("L");
  uView.display();
  delay(50);
  uView.print("-");
  uView.display();
  delay(50);
  uView.print("2");
  uView.display();
  delay(50);
  uView.print("X");
  uView.display();

  delay(400);
  
  uView.setFontType(0);
  
  delay(200);
  uView.clear(PAGE);
  uView.display();
  
}

/*
uint8_t bitmapTest [] = {
0x00, 0x00, 0xFC, 0x04, 0x04, 0x04, 0xC4, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x7C, 0x00,
0xFC, 0x04, 0x04, 0x04, 0xC4, 0x44, 0x44, 0x44, 0xC4, 0x84, 0x04, 0x0C, 0x18, 0x30, 0xE0, 0x00,
0x00, 0xFC, 0x04, 0x04, 0x04, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xC7, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x7C, 0x00,
0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0xFF, 0x00, 0x00, 0x00, 0x1F, 0x10, 0x10, 0x10, 0x18, 0x06, 0x83, 0xC0, 0x60, 0x30, 0x1F, 0x00,
0x00, 0xFF, 0x00, 0x00, 0x00, 0x1F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xF0, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x80, 0x80, 0xC0, 0x60, 0x20,
0x20, 0x61, 0xC1, 0x81, 0x81, 0x01, 0x01, 0x01, 0xC1, 0xA1, 0x21, 0x21, 0xE1, 0x81, 0x00, 0x00,
0x00, 0x80, 0xE0, 0x20, 0x20, 0xE0, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x10, 0x10, 0x18, 0x0C,
0x84, 0xCC, 0x78, 0x00, 0xC1, 0x79, 0x0F, 0x00, 0x00, 0x07, 0xCC, 0x70, 0x00, 0x83, 0x86, 0x06,
0x43, 0xF1, 0x1C, 0x06, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x2C, 0x26, 0x22, 0x23,
0x21, 0x20, 0x20, 0x23, 0x23, 0x22, 0x3E, 0x00, 0x3C, 0x23, 0x21, 0x30, 0x1C, 0x07, 0x07, 0x1C,
0x30, 0x23, 0x26, 0x3C, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
*/

