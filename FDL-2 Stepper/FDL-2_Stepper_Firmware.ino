// FDL-2 Blaster Firmware

// /u/torukmakto4s code is mixed in with this to control a stepper linearly
// See
// http://torukmakto4.blogspot.com/2018/01/google-drive-link-for-hy-con-part-files.html
// for original the linear stepper source.

SYSTEM_MODE(SEMI_AUTOMATIC);

// Constants from /u/torukmakto4's code

// BOLT DRIVE PARAMETERS - don't mess with these unless you know what you are
// doing
unsigned long startSpeed = 400; // us (default 400 - leave alone)
unsigned long shiftSpeed = 150; // us (default 150)
unsigned long runSpeed = 125;   // us (default 125 for reliable operation on 3S)
double accelPhaseOne = 0.000000253494; // m value for ramp in low speed region
double accelPhaseTwo = 0.000000180748; // m value for ramp in high speed region
double decel = 0.000000439063;         // m value for hard decel

int stepsToGo;
bool boltHomed = 0;
double currSpeed = startSpeed; // to be used by fire() to be aware of motor
                               // speed from last run and mate speed ramp to
                               // that
double stepdelay;              // us

// Former FDL code begins here!
// Define the pins we're going to call pinMode on
int pusherSenseIn = D1;
#define pusherStep                                                             \
  D4 // Keep the above a compile time constant for speed purposes.
int stepperEnable = D6;
int stepperDir = D7;
int wifiSenseIn = RX;
int escPin = TX;

int speedSenseIn = A2;
int triggerSenseIn = A3;
int modeSenseIn = A4;

Servo flywheelESC; // create servo object to control a ESC

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
MySettings currentSettingsStruct = {55, 160, 240, 160, 0, 100,
                                    0,  100, 200, 100, 2};

// This routine runs only once upon reset
void setup() {

  flywheelESC.attach(escPin); // attaches pin to servo object

  // Initialize pins
  // It's important you do this here, inside the setup() function rather than
  // outside it or in the loop function.
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
// Spark firmware interleaves background CPU activity associated with WiFi +
// Cloud activity with your code.
// Make sure none of your code delays or blocks for too long (like more than 5
// seconds), or weird things can happen

void loop() {

  if (triggerDown()) {

    // trigger down at boot, write max throttle to ESC to calibrate
    if (firstRun) {
      while (digitalRead(triggerSenseIn) == HIGH) {
        flywheelESC.writeMicroseconds(1860);
        delay(300);
      }

      flywheelESC.writeMicroseconds(1000);
      delay(1500);
    } else {
      ////kick on steppers, warmup in included in spinup time.
      digitalWrite(stepperEnable, LOW);
      fireBrushlessLoop();
    }
  } else {
    flywheelESC.writeMicroseconds(1000);
  }

  if (digitalRead(wifiSenseIn) == HIGH) {
    Particle.connect();
  }

  firstRun = false;

  if (millis() > disableMillis) {
    digitalWrite(stepperEnable, HIGH);
  }

  delay(10);
}

boolean triggerDown() { return analogRead(speedSenseIn) > 2000; }

int readESCPower() {

  if (currentSettingsStruct.fixedMode > 0) {
    return currentSettingsStruct.fixedPower;
  } else {
    int val = analogRead(speedSenseIn);
    return map(val, 0, 4094, currentSettingsStruct.lowPower,
               currentSettingsStruct.highPower);
  }
}

int getSpinup() {
  if (currentSettingsStruct.fixedMode > 0) {
    return currentSettingsStruct.fixedSpinup;
  } else {
    int val = analogRead(speedSenseIn);
    return map(val, 0, 4094, currentSettingsStruct.lowSpinup,
               currentSettingsStruct.highSpinup);
  }
}

int getBurstCount() {
  if (currentSettingsStruct.fixedMode > 0) {
    return currentSettingsStruct.fixedBurstCount;
  } else {
    int mVal = analogRead(modeSenseIn);

    if (mVal > 4000) {
      return 1;
    }
    if (mVal < 1000) {
      return 100;
    }
    if (mVal > 3000) {
      return 2;
    }
    return 3;
  }
}

int getROF() {
  if (currentSettingsStruct.fixedMode > 0) {
    return currentSettingsStruct.fixedROF;
  } else {
    int val = analogRead(triggerSenseIn);
    int rofPercent = map(val, 0, 4094, 0, 100);
  }
}

// Inserting stepper stepping functions here.
void commutate(double commDelay) {
  // function to commutate the stepper once. note- immediate rising edge,
  // trailing delay
  // Speed should be critical here, so using pinSetFast instead of digitalWrite.
  pinSetFast(pusherStep); // Equivelent to digitalWrite(pusherStep,HIGH)
  delayMicroseconds((unsigned long)(commDelay / 2));
  pinResetFast(pusherStep); // Equivelent to digitalWrite(pusherStep,low);
  delayMicroseconds((unsigned long)(commDelay / 2));
}

bool decelerateBoltToSwitch() {
  // try to gracefully end drivetrain rotation
  // called after the last fire() has returned
  // return true for home and false for not home

  // fire() runs the bolt forward 720 clicks (on 4:1 mode) leaving us 80 to
  // bring it down to ~3rps where we know it can stop instantly and keep
  // position.
  // abort instantly on limit switch low
  stepsToGo = 150;
  stepdelay = currSpeed;
  while ((stepsToGo > 0) && (!pinReadFast(pusherSenseIn))) {
    commutate(stepdelay);
    if (stepdelay < startSpeed) {
      stepdelay = stepdelay * (1 + decel * stepdelay * stepdelay);
    }
    stepsToGo--;
  }
  currSpeed = startSpeed;
  boltHomed = 1;
  return pinReadFast(pusherSenseIn);
}

bool reverseBoltToSwitch() {
  // this is called if decelerateBoltToSwitch() returns false
  stepsToGo = 800; // up to a full rev permitted (TBD)
  // set bolt direction reverse
  digitalWrite(stepperDir, HIGH);
  // run bolt back at idle speed
  while ((!pinReadFast(pusherSenseIn))) {
    commutate(startSpeed);
    stepsToGo--;
    if (stepsToGo == 0) {
      // ran out of angle, die and reset direction
      digitalWrite(stepperDir, LOW);
      return false;
    }
  }
  digitalWrite(stepperDir, LOW);
  return pinReadFast(pusherSenseIn);
}

void stopFiring() {
  if (!decelerateBoltToSwitch()) {
    reverseBoltToSwitch();
  }
}

void fire() {
  // loop called to fire a shot
  // set distance to run bolt forward
  stepsToGo = 720;
  // if continuing a previous instance add 80 steps
  if (!boltHomed) {
    stepsToGo += 80;
  }

  // get start point for first ramp
  if (currSpeed < startSpeed) {
    // bolt already running
    stepdelay = currSpeed;
  } else {
    // bolt not running
    stepdelay = startSpeed;
  }
  // do first ramp if speed below shiftpoint
  while (stepdelay > shiftSpeed) {
    commutate(stepdelay);
    stepdelay = stepdelay * (1 - accelPhaseOne * stepdelay * stepdelay);
    stepsToGo--;
  }
  // do second speed ramp if speed above shift but below running speed
  while (stepdelay > runSpeed) {
    commutate(stepdelay);
    stepdelay = stepdelay * (1 - accelPhaseTwo * stepdelay * stepdelay);
    stepsToGo--;
  }
  // do constant speed run until out of steps
  while (stepsToGo > 0) {
    commutate(stepdelay);
    stepsToGo--;
  }
  currSpeed = stepdelay;
  boltHomed = 0;
}
// Ending inserted stepper functions here.

// On construction, this class revs the flywheel
// Upon exiting scope, this class turns off the flywheels.
// If the trigger is released within a small time frame, consider it a
// em noise blip and abort.
class RevBrushlessMotors {

public:
  void updateSpeedFromSettings() { flywheelESC.write(readESCPower()); }
  RevBrushlessMotors() { revUpBrushlessMotors(); }
  bool isCancelled() { return cancelled; }
  ~RevBrushlessMotors() { brushlessPowerDown(1000); }

private:
  void brushlessPowerDown(double millisToDisable) {
    flywheelESC.writeMicroseconds(1000);
    // sets disable time for 1 sec
    disableMillis = millis() + millisToDisable;
    lastTriggerUp = millis();
  }
  const unsigned long fireCancelWindow = 18;
  bool cancelled = false;
  void revUpBrushlessMotors() {
    // fireCancelWindow : Abort if the trigger is held down for less than this.
    // Avoid blips on the button caused by randomness from firing,
    // but be small enough so that any reasonable human press causes at least
    // one dart to fire. Note if this is more than the spinup delay, will be
    // ignored past the spinup delay.
    const unsigned long timeFiringStarted = millis();
    unsigned long spinupEnd = millis() + getSpinup();
    // kick on flywheels
    int kickonSpeed = readESCPower();
    flywheelESC.write(kickonSpeed);
    // wait for spinup
    // if last trigger up less than a second ago, minimize delay
    if (lastTriggerUp != 0 && millis() - lastTriggerUp < 1000) {
      // If the wheels are already spinning, instead of the long wheel spin
      // up time, then just cut the time down to that of stepperWarmup.
      // But note, the stepper is already warmed up!
      // So stepperWarmup is just an abitary period to wait for the wheels
      // to spin back up, as far as I can tell.
      spinupEnd = millis() + stepperWarmup;
    }
    unsigned long currentTime = millis();
    while (currentTime < spinupEnd) {
      currentTime = millis();
      if (currentTime < (timeFiringStarted + fireCancelWindow) &&
          (!triggerDown())) {
        cancelled = true;
      }
      delay(1);
    }
    flywheelESC.write(readESCPower());
  }
};

// Revoles the stepper an up to burstCount times,
// as long as the trigger is held down.
// updating the flywheel speed while doing so.
void stepperRevolveUpToBurstAmount(int burstCount,
                                   RevBrushlessMotors *revUpBrushlessMotors) {
  // Use a do while loop to ensure we fire  at least once.
  do {
    fire();
    burstCount--;
    // Continue to update the flywheel speed while firing?
    // Why?
    revUpBrushlessMotors->updateSpeedFromSettings();
    // Repeat this do loop until all shots are fired.
  } while (triggerDown() && burstCount > 0);
  // Done with this burst, return the pusher.
  stopFiring();
}

void delayUntilTriggerRelease() {
  while (triggerDown()) {
    delay(1);
  }
}

void fireBrushlessLoop() {
  RevBrushlessMotors revUpBrushlessMotors;
  if (revUpBrushlessMotors.isCancelled()) {
    return;
  }
  int burstCount = getBurstCount();
  stepperRevolveUpToBurstAmount(burstCount, &revUpBrushlessMotors);
  // If the trigger is still held down, then wait until released before
  // returning control to the main loop.
  // If we do not wait, then the main loop will see trigger down, and fire
  // again.
  // Note how we stop the flywheels after this loop
  // This way an user can keep the FDL revved.
  delayUntilTriggerRelease();
}

String getSettingsFromEEPROM() {
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

void setSettings(String settingsString) {

  int firstDelimIndex = 0;
  int nextDelimIndex = settingsString.indexOf(":");
  int _lowPower =
      settingsString.substring(firstDelimIndex, nextDelimIndex).toInt();
  firstDelimIndex = nextDelimIndex;
  nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
  int _highPower =
      settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
  firstDelimIndex = nextDelimIndex;
  nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
  int _lowSpinup =
      settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
  firstDelimIndex = nextDelimIndex;
  nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
  int _highSpinup =
      settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
  firstDelimIndex = nextDelimIndex;
  nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
  int _lowROF =
      settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
  firstDelimIndex = nextDelimIndex;
  nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
  int _highROF =
      settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
  firstDelimIndex = nextDelimIndex;
  nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
  int _fixedMode =
      settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
  firstDelimIndex = nextDelimIndex;
  nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
  int _fixedPower =
      settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
  firstDelimIndex = nextDelimIndex;
  nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
  int _fixedSpinup =
      settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
  firstDelimIndex = nextDelimIndex;
  nextDelimIndex = settingsString.indexOf(":", firstDelimIndex + 1);
  int _fixedROF =
      settingsString.substring(firstDelimIndex + 1, nextDelimIndex).toInt();
  firstDelimIndex = nextDelimIndex;
  int _fixedBurstCount = settingsString.substring(firstDelimIndex + 1).toInt();

  currentSettingsStruct = {_lowPower,   _highPower,      _lowSpinup,
                           _highSpinup, _lowROF,         _highROF,
                           _fixedMode,  _fixedPower,     _fixedSpinup,
                           _fixedROF,   _fixedBurstCount};

  EEPROM.put(0, currentSettingsStruct);
  currentSettingsString = settingsString;
}

void initSettings() {
  MySettings storedSettings;
  EEPROM.get(0, storedSettings);

  // if EEPROM empty all values will come back -1
  if (storedSettings.lowPower == -1) {
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
