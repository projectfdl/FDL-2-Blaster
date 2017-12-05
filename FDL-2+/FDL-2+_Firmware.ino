//FDL-2+ Blaster Firmware
//Last Update: 2017-5-19

SYSTEM_MODE(SEMI_AUTOMATIC);

// Define the pins we're going to call pinMode on

int pusherEnablePin = D4;
int pusherBrakePin = D6;
int wifiSenseIn = RX;
int escPin = TX;

int frontPusherSw = D0;
int rearPusherSw = D1;

int speedSenseIn = A2;
int rofSenseIn = A3;
int modeSenseIn = A4;

Servo flywheelESC;  // create servo object to control a ESC

int stepperWarmup = 80;

unsigned long disableMillis = millis();
unsigned long lastTriggerUp = 0;
bool firstRun = true;

const int minESCpower = 55;
const int maxESCpower = 160;
const int lowSpinupDelay = 240;
const int highSpinupDelay = 160;

const bool fixedMode = false;
const int fixedPower = 100;
const int fixedSpinup = 200;
const int fixedBurstCount = 100;

String currentSettingsString = "55:160:240:160:0:100:0:100:200:100:2";
struct MySettings {
    int lowPower;
    int highPower;
    int lowSpinup;
    int highSpinup;
    int lowROF;
    int highROF;
    int fixedMode;
    int fixedPower;
    int fixedSpinup;
    int fixedROF;
    int fixedBurstCount;
};
MySettings currentSettingsStruct = { 55, 160, 240, 160, 0, 100, 0, 100, 100, 2 };





// This routine runs only once upon reset
void setup() {

  flywheelESC.attach(escPin);  // attaches pin to servo object

  // Initialize pins
  pinMode(wifiSenseIn, INPUT_PULLDOWN);
  pinMode(speedSenseIn, INPUT);
  pinMode(modeSenseIn, INPUT);
  pinMode(rofSenseIn, INPUT);
  pinMode(pusherEnablePin, OUTPUT);
  pinMode(pusherBrakePin, OUTPUT);

  pinMode(rearPusherSw, INPUT_PULLDOWN);
  pinMode(frontPusherSw, INPUT_PULLDOWN);

  digitalWrite(pusherBrakePin, LOW);

  Particle.variable("settings", currentSettingsString);
  Particle.function("setsettings", settingsHandler);

  initSettings();
}


void loop() {

    if(triggerDown()){

        if(firstRun){
            while(digitalRead(speedSenseIn) == HIGH){
                //flywheelESC.write(180);
                flywheelESC.writeMicroseconds(1860);
                delay(300);
            }

            //flywheelESC.write(0);
            flywheelESC.writeMicroseconds(1000);
            delay(1500);
        }
        else{
            fireBrushlessLoop();
        }
    }
    else{
        flywheelESC.writeMicroseconds(1000);
    }

    if(digitalRead(wifiSenseIn) == HIGH){
      Particle.connect();
    }

    firstRun = false;

    delay(10);
}

boolean triggerDown(){
    return analogRead(speedSenseIn) > 20;
}

int readESCPower(){

    if(currentSettingsStruct.fixedMode > 0){
        return currentSettingsStruct.fixedPower;
    }
    else{
        int val = analogRead(speedSenseIn);
        return map(val, 0, 4094, currentSettingsStruct.lowPower, currentSettingsStruct.highPower);
    }
}

int getSpinup(){
    if(currentSettingsStruct.fixedMode > 0){
        return currentSettingsStruct.fixedSpinup;
    }
    else{
        //return 150;

        int val = analogRead(speedSenseIn);
        return map(val, 0, 4094, currentSettingsStruct.lowSpinup, currentSettingsStruct.highSpinup);
    }
}

int getBurstCount(){
    if(currentSettingsStruct.fixedMode > 0){
        return currentSettingsStruct.fixedBurstCount;
    }
    else{
        int mVal = analogRead(modeSenseIn);

        if(mVal > 4000){
            return 1;
        }
        if(mVal < 1000){
            return 100;
        }
        if(mVal > 3000){
            return 2;
        }
        return 3;
    }
}

int getROF(){
    if(currentSettingsStruct.fixedMode > 0){
        return currentSettingsStruct.fixedROF;
    }
    else{
        int val = analogRead(rofSenseIn);
        int rofPercent = map(val, 0, 4094, 0, 100);
    }
}


void fireBrushlessLoop(){

    long currMills = millis();

    int kickonSpeed = readESCPower();
    flywheelESC.write(kickonSpeed);

    if(lastTriggerUp != 0 && millis() - lastTriggerUp < 1000){
        delay(10);
    }
    else{
        int spinUpVal = getSpinup();
        delay(spinUpVal);
    }

    int burstCount = getBurstCount();

    while(burstCount > 1 && triggerDown()){

        int val = getROF();
        int burstBrakeDelay = map(val, 0, 100, 100, 0);

        currMills = millis();
        while(digitalRead(frontPusherSw) == LOW && millis() < currMills + 100){
            digitalWrite(pusherEnablePin, HIGH);
            delay(6);
        }

        currMills = millis();
        while(digitalRead(frontPusherSw) == HIGH && millis() < currMills + 100){
            digitalWrite(pusherEnablePin, HIGH);
            delay(6);
        }

        burstCount--;

        if(burstBrakeDelay > 5){
            digitalWrite(pusherEnablePin, LOW);
            delayMicroseconds(200);
            digitalWrite(pusherBrakePin, HIGH);
            delay(burstBrakeDelay);
            digitalWrite(pusherBrakePin, LOW);
            delayMicroseconds(200);
        }
    }


    currMills = millis();
    while(digitalRead(frontPusherSw) == LOW && millis() < currMills + 100){
        digitalWrite(pusherEnablePin, HIGH);
        delay(6);
    }


    digitalWrite(pusherEnablePin, LOW);

    delayMicroseconds(200);

    digitalWrite(pusherBrakePin, HIGH);
    delay(20);
    digitalWrite(pusherBrakePin, LOW);

    delayMicroseconds(200);

    digitalWrite(pusherEnablePin, HIGH);
    delayMicroseconds(15000);
    digitalWrite(pusherEnablePin, LOW);



    currMills = millis();
    while(digitalRead(rearPusherSw) == LOW && millis() < currMills + 100){
        delayMicroseconds(200);
    }

    flywheelESC.writeMicroseconds(1000);

    delayMicroseconds(200);

    digitalWrite(pusherBrakePin, HIGH);
    delay(30);
    digitalWrite(pusherBrakePin, LOW);

    delayMicroseconds(200);

    while(triggerDown()){};

    lastTriggerUp = millis();
}




//app settings code
String getSettingsFromEEPROM(){
    MySettings storedSettings;
    EEPROM.get(0, storedSettings);

    String res = String(storedSettings.lowPower) + ":";
    res += String(storedSettings.highPower) + ":";
    res += String(storedSettings.lowSpinup) + ":";
    res += String(storedSettings.highSpinup) + ":";
    res += String(storedSettings.lowROF) + ":";
    res += String(storedSettings.highROF) + ":";
    res += String(storedSettings.fixedMode) + ":";
    res += String(storedSettings.fixedPower) + ":";
    res += String(storedSettings.fixedSpinup) + ":";
    res += String(storedSettings.fixedROF) + ":";
    res += String(storedSettings.fixedBurstCount) + ":";

    return res;
}

void setSettings(String settingsString){

    int firstDelimIndex = 0;
    int nextDelimIndex = settingsString.indexOf(":");
    int _lowPower = settingsString.substring(firstDelimIndex, nextDelimIndex).toInt();
    firstDelimIndex = nextDelimIndex;
    nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
    int _highPower = settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
    firstDelimIndex = nextDelimIndex;
    nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
    int _lowSpinup = settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
    firstDelimIndex = nextDelimIndex;
    nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
    int _highSpinup = settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
    firstDelimIndex = nextDelimIndex;
    nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
    int _lowROF = settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
    firstDelimIndex = nextDelimIndex;
    nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
    int _highROF = settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
    firstDelimIndex = nextDelimIndex;
    nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
    int _fixedMode = settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
    firstDelimIndex = nextDelimIndex;
    nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
    int _fixedPower = settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
    firstDelimIndex = nextDelimIndex;
    nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
    int _fixedSpinup = settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
    firstDelimIndex = nextDelimIndex;
    nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
    int _fixedROF = settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
    firstDelimIndex = nextDelimIndex;
    int _fixedBurstCount = settingsString.substring(firstDelimIndex + 1).toInt();

    currentSettingsStruct = { _lowPower, _highPower, _lowSpinup, _highSpinup, _lowROF, _highROF,
                                    _fixedMode, _fixedPower, _fixedSpinup, _fixedROF, _fixedBurstCount };

    EEPROM.put(0, currentSettingsStruct);
    currentSettingsString = settingsString;
}

void initSettings(){
    MySettings storedSettings;
    EEPROM.get(0, storedSettings);

    //if EEPROM empty all values will come back -1
    if(storedSettings.lowPower == -1){
        storedSettings = {55, 160, 240, 160, 0, 100, 0, 100, 200, 100, 1};
    }

    String res = String(storedSettings.lowPower) + ":";
    res += String(storedSettings.highPower) + ":";
    res += String(storedSettings.lowSpinup) + ":";
    res += String(storedSettings.highSpinup) + ":";
    res += String(storedSettings.lowROF) + ":";
    res += String(storedSettings.highROF) + ":";
    res += String(storedSettings.fixedMode) + ":";
    res += String(storedSettings.fixedPower) + ":";
    res += String(storedSettings.fixedSpinup) + ":";
    res += String(storedSettings.fixedROF) + ":";
    res += String(storedSettings.fixedBurstCount) + ":";

    currentSettingsStruct = storedSettings;
    currentSettingsString = res;
}


// Cloud functions must return int and take one String
int settingsHandler(String settings) {
    setSettings(settings);
    return 0;
}

