//FDL-2 Blaster Firmware
//Last Update: 2016-11-14

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
unsigned long flywheelSpinup = 100;

unsigned long disableMillis = millis();
unsigned long lastTriggerUp = 0;
bool firstRun = true;


// This routine runs only once upon reset
void setup() {
    
  flywheelESC.attach(escPin);  // attaches pin to servo object
    
  // Initialize pins 
  // It's important you do this here, inside the setup() function rather than outside it or in the loop function.
  pinMode(pusherStep, OUTPUT);
  pinMode(stepperEnable, OUTPUT);
  pinMode(pusherSenseIn, INPUT_PULLDOWN);
  pinMode(wifiSenseIn, INPUT_PULLDOWN);
  pinMode(speedSenseIn, INPUT);
  pinMode(modeSenseIn, INPUT);
  pinMode(triggerSenseIn, INPUT);
  
  pinMode(stepperDir, OUTPUT);
  digitalWrite(stepperDir, LOW);
  
  digitalWrite(stepperEnable, HIGH); // Turn off steppers (HIGH)
}

// This routine gets called repeatedly, like once every 5-15 milliseconds.
// Spark firmware interleaves background CPU activity associated with WiFi + Cloud activity with your code. 
// Make sure none of your code delays or blocks for too long (like more than 5 seconds), or weird things can happen.
void loop() {
  
    if(triggerDown()){
        
        if(firstRun){
            while(digitalRead(triggerSenseIn) == HIGH){
                flywheelESC.write(180); 
            }
        
            flywheelESC.write(0); 
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
    int val = analogRead(speedSenseIn);
    return map(val, 0, 4094, 55, 115);
}

int getSpinup(){
    int val = analogRead(speedSenseIn);
    return map(val, 0, 4094, 240, 100);
}

int getFireMode(){
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
        
        while(millis() < spinupEnd ){ delay(1); }
        
        int burstCount = getFireMode(); 
        
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
    
    int val = analogRead(triggerSenseIn);
    int accStepsStart = map(val, 0, 4094, 400, 20);
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

