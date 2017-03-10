//FDL-2 Blaster Firmware
//Last Update: 2017-1-23

SYSTEM_MODE(SEMI_AUTOMATIC);

// Define the pins we're going to call pinMode on
int pusherSenseIn = D1;
int pusherStep = D4;
int stepperEnable = D6;
int stepperDir = D7;
int wifiSenseIn = RX;
int escPin = TX;

int speedSenseIn = A2;
int triggerSenseIn = A3;
int modeSenseIn = A4;

Servo flywheelESC;  // create servo object to control a ESC

int stepperWarmup = 80;

unsigned long disableMillis = millis();
unsigned long lastTriggerUp = 0;
bool firstRun = true;

const int minESCpower = 55;
const int maxESCpower = 115;
const int lowSpinupDelay = 240;
const int highSpinupDelay = 160;

const bool fixedMode = false;
const int fixedPower = 100;
const int fixedSpinup = 200;
const int fixedBurstCount = 100; 

String currentSettingsString = "55:115:240:160:0:100:0:100:200:100:2";
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
MySettings currentSettingsStruct = { 55, 115, 240, 160, 0, 100, 0, 100, 200, 100, 2 };


// This routine runs only once upon reset
void setup() {
    
  flywheelESC.attach(escPin);  // attaches pin to servo object
    
  // Initialize pins 
  // It's important you do this here, inside the setup() function rather than outside it or in the loop function.
  pinMode(pusherSenseIn, INPUT_PULLDOWN);
  pinMode(wifiSenseIn, INPUT_PULLDOWN);
  pinMode(speedSenseIn, INPUT);
  pinMode(modeSenseIn, INPUT);
  pinMode(triggerSenseIn, INPUT);
  pinMode(pusherStep, OUTPUT);
  pinMode(stepperEnable, OUTPUT);
  pinMode(stepperDir, OUTPUT);
  
  digitalWrite(stepperEnable, HIGH); // Turn off steppers (HIGH)
  digitalWrite(stepperDir, LOW);
  
  Particle.variable("settings", currentSettingsString);
  Particle.function("setsettings", settingsHandler);
  
  initSettings();
}

// This routine gets called repeatedly, like once every 5-15 milliseconds.
// Spark firmware interleaves background CPU activity associated with WiFi + Cloud activity with your code. 
// Make sure none of your code delays or blocks for too long (like more than 5 seconds), or weird things can happen.
void loop() {
  
    if(triggerDown()){
        
        if(firstRun){
            while(digitalRead(triggerSenseIn) == HIGH){
                flywheelESC.writeMicroseconds(1860);
                delay(300);
            }
        
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
    
    if(millis() > disableMillis){
        digitalWrite(stepperEnable, HIGH);
    }
    
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
        int val = analogRead(triggerSenseIn);
        int rofPercent = map(val, 0, 4094, 0, 100);
    }
}


void fireBrushlessLoop(){
    
    unsigned long spinupEnd = millis() + getSpinup();
    
    //kick on flywheels
    int kickonSpeed = readESCPower();
    
    flywheelESC.write(kickonSpeed); 
    
    ////kick on steppers, wait for warmup
    digitalWrite(stepperEnable, LOW);
    
    //still holding the trigger?
    if(triggerDown()){
        
        //wait for spinup
        //if last trigger up less than a second ago, minimize delay
        if(lastTriggerUp != 0 && millis() - lastTriggerUp < 1000){
            spinupEnd = millis() + stepperWarmup;
        }
        
        while(millis() < spinupEnd){ delay(1); }
        
        int burstCount = getBurstCount(); 
        
        flywheelESC.write(readESCPower());
        
        while(triggerDown()){
            
            if(!spinPusherToSwitch()){
                reverseToPusherSwitch();
            }
            burstCount--;
            
            if(triggerDown() && burstCount > 0){
                
                flywheelESC.write(readESCPower()); 
            }
            else{
                brushlessPowerDown(1000);
                
                while(triggerDown()){
                    delay(1);
                }
                
                return;
            }
        }
        brushlessPowerDown(1000);
    }
    else{ 
        brushlessPowerDown(1000);
        return;
    }
    
}

void brushlessPowerDown(double millisToDisable){
    
    flywheelESC.writeMicroseconds(1000);
    
    //sets disable time for 1 sec
    disableMillis = millis() + millisToDisable;
    
    lastTriggerUp = millis();
}

bool spinPusherToSwitch(){
    
    //800 full spin
    //spin enough to let go of the switch (1/2 wayish)
    
    int rofPercent = getROF();
    
    int accStepsStart = map(rofPercent, 0, 100, 400, 80);
    
    int accStepsMid = 400 - accStepsStart;
    
    int accStartDelay = 460; 
    int accMidDelay = 150; 
    int accEndDelay = 110;
    
    StepRange(pusherStep, accStartDelay, accMidDelay, accStepsStart);
    StepRange(pusherStep, accMidDelay, accEndDelay, accStepsMid);

    for(int stepIndex = 0; stepIndex < 500; stepIndex++){
       StepRange(pusherStep, accEndDelay, accEndDelay, 1); 
       
       if(digitalRead(pusherSenseIn) == HIGH){
           return true;
       }
    }
    
    return false;
}

bool reverseToPusherSwitch(){
    
    digitalWrite(stepperDir, HIGH);
    
    for(int stepIndex = 0; stepIndex < 1000; stepIndex++){
        
        if(digitalRead(pusherSenseIn) == LOW){
           StepDelay(pusherStep, 400, 1);
        }
        else{
            break;
        }
        
    }
    
    digitalWrite(stepperDir, LOW);
    
    return digitalRead(pusherSenseIn) == HIGH;
}



void StepDelay(int stepperPin, double delayMicros, int steps){
     for (int index = 0 ; index < steps ; index ++) {
        digitalWrite(stepperPin, HIGH); 
        delayMicroseconds(delayMicros / 2); 
        digitalWrite(stepperPin, LOW); 
        delayMicroseconds(delayMicros / 2); 
    }
}

void StepRange(int stepperPin, double startDelay, double endDelay, double steps){
    
    double delayChangePerStep = (endDelay - startDelay) / steps;
    
    double loopDelay = startDelay;
    
    for (int index = 0 ; index < steps ; index += 1) {
        digitalWrite(stepperPin, HIGH); 
        delayMicroseconds(loopDelay / 2); 
        digitalWrite(stepperPin, LOW); 
        delayMicroseconds(loopDelay / 2); 
        
        loopDelay += delayChangePerStep;
    }
}


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
        storedSettings = {55, 115, 240, 160, 0, 100, 0, 100, 200, 100, 1};
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



