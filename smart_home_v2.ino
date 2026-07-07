#include <Servo.h>
#include <DHT.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>

unsigned long smokeFanStart = 0;
bool smokeFanRunning = false;
const unsigned long smokeFanDuration = 3000;

// ─────────────────────────────────────────
//  LED PINS
// ─────────────────────────────────────────
int ledRed    = 27;
int ledYellow = 23;
int ledGreen  = 29;
int ledBlue   = 25;

// ─────────────────────────────────────────
//  LED / BUZZER TIMERS
// ─────────────────────────────────────────
unsigned long redStart    = 0;
unsigned long blueStart   = 0;
unsigned long greenStart  = 0;
unsigned long yellowStart = 0;
const int ledDuration     = 3000;

// ─────────────────────────────────────────
//  OUTPUT PINS
// ─────────────────────────────────────────
int fan       = 16;
int waterPump = 4;
int buzzer    = 53;

unsigned long buzzerStart = 0;
bool buzzerActive         = false;
bool fanActive            = false;
bool fireMode             = false;
bool smokeMode = false;

// ─────────────────────────────────────────
//  SENSOR PINS
// ─────────────────────────────────────────
int sensorFlame    = A15;
int sensorSmoke    = A9;
#define TRIG_PIN   14
#define ECHO_PIN   15
#define DHTPIN     19
#define DHTTYPE    DHT11

DHT dht(DHTPIN, DHTTYPE);

// ─────────────────────────────────────────
//  THRESHOLDS
// ─────────────────────────────────────────
int thresholdSmoke    = 900;
int thresholdDHT      = 30;
int thresholdFlame    = 100;
int thresholdDistance = 30;

// ─────────────────────────────────────────
//  MODES & FLAGS
// ─────────────────────────────────────────
bool automaticMode = true;

// ─────────────────────────────────────────
//  INTRUSION / MOTION
// ─────────────────────────────────────────
bool intrusionMode             = false;
unsigned long lastMotionTime   = 0;
const unsigned long motionDisplayTime = 10000;

// ─────────────────────────────────────────
//  SERVO PINS & OBJECTS
// ─────────────────────────────────────────
Servo servoDoor1;
Servo servoDoor2;
Servo servoWindow1;
Servo servoWindow2;

int servoDoor1pin   = 37;
int servoDoor2pin   = 39;
int servoWindow1pin = 31;
int servoWindow2pin = 33;

int closedPosition = 0;
int openPosition   = 180;

// ─────────────────────────────────────────
//  IR REMOTE
// ─────────────────────────────────────────
int remoteReceiver = 2;
IRrecv RR(remoteReceiver);
decode_results results;

// ─────────────────────────────────────────
//  KEYPAD
// ─────────────────────────────────────────
const byte rows = 4;
const byte cols = 4;
char hexaKeys[rows][cols] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[rows] = {13, 12, 11, 10};
byte colPins[cols] = {9, 8, 7, 6};
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, rows, cols);

String input     = "";
String password  = "159";
String resetCode = "123123#";
int tries        = 0;
bool locked      = false;

// ─────────────────────────────────────────
//  LCD
// ─────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

int lcdMode                = 0;
unsigned long lcdAlertTime = 0;
const int alertDuration    = 2000;
String currentAlert        = "";

int systemModeDisplay          = 0;
unsigned long modeDisplayStart = 0;
const int modeDisplayDuration  = 3000;
String modeText                = "";

// ─────────────────────────────────────────
//  COMPONENT STATUS VARIABLES
//  "free"   → automatic mode controls it
//  "locked" → Telegram/manual has locked it; auto cannot override
//
//  Possible values per component:
//    doors:     "open" | "closed" | "free"
//    windows:   "open" | "closed" | "free"
//    fan:       "on"   | "off"    | "free"
//    buzzer:    "on"   | "off"    | "free"
//    waterPump: "on"   | "off"    | "free"
// ─────────────────────────────────────────
String doorsStatus     = "free";
String windowsStatus   = "free";
String fanStatus       = "free";
String buzzerStatus    = "free";
String waterPumpStatus = "free";

// ─────────────────────────────────────────
//  ULTRASONIC
// ─────────────────────────────────────────
long readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 999;
  return duration / 58;
}

// ─────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────
void setup() {
  lcd.init();
  lcd.backlight();

  RR.enableIRIn();
  Serial.begin(9600);

  pinMode(ledRed,    OUTPUT);
  pinMode(ledYellow, OUTPUT);
  pinMode(ledGreen,  OUTPUT);
  pinMode(ledBlue,   OUTPUT);

  pinMode(sensorFlame, INPUT);
  pinMode(sensorSmoke, INPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(buzzer,    OUTPUT);
  pinMode(fan,       OUTPUT);
  pinMode(waterPump, OUTPUT);

  dht.begin();
}

// ─────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────
void loop() {
  static bool lastNear = false;

  remoteControl();
  keypad();
  handleSerial();

  int   flameValue    = analogRead(sensorFlame);
  int   smokeValue    = analogRead(sensorSmoke);
  float tempValue     = dht.readTemperature();
  long  distanceValue = readDistance();
  bool  objectNear    = (distanceValue < thresholdDistance);
  Serial.println(distanceValue);

  if (automaticMode) {
    // Smoke detection with state tracking
    if (smokeValue > thresholdSmoke) {

      // trigger only once
      if (!smokeMode) {
        smoke(smokeValue);
        smokeMode = true;
      }

    } else {

      // smoke cleared
      smokeMode = false;

      // don't instantly stop fan
      // let timer handle it

      if (buzzerStatus == "free") {
        digitalWrite(buzzer, LOW);
        buzzerActive = false;
      }

      if (buzzerStatus == "free") {
        digitalWrite(buzzer, LOW);
        buzzerActive = false;
      }
    }
    if (tempValue  > thresholdDHT)    humidity();
    if (flameValue < thresholdFlame)  fire();
    if (objectNear && !lastNear)      motion();
  }

  lastNear = objectNear;

  updateLCD(smokeValue, flameValue, distanceValue, (int)tempValue);
  systemUpdate();
  delay(200);
}

// ─────────────────────────────────────────
//  SERIAL / TELEGRAM COMMAND HANDLER
// ─────────────────────────────────────────
void handleSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  // ── Doors ──  
  if (cmd == "DOORS_OPEN") {
    doorsStatus = "open";
    openDoors();
    Serial.println("STATUS:DOORS=OPEN(LOCKED)");
  }
  else if (cmd == "DOORS_CLOSE") {
    doorsStatus = "closed";
    closeDoors();
    Serial.println("STATUS:DOORS=CLOSED(LOCKED)");
  }
  else if (cmd == "DOORS_FREE") {         // hand back to automatic mode
    doorsStatus = "free";
    Serial.println("STATUS:DOORS=FREE");
  }

  // ── Windows ──
  else if (cmd == "WINDOWS_OPEN") {
    windowsStatus = "open";
    openWindows();
    Serial.println("STATUS:WINDOWS=OPEN(LOCKED)");
  }
  else if (cmd == "WINDOWS_CLOSE") {
    windowsStatus = "closed";
    closeWindows();
    Serial.println("STATUS:WINDOWS=CLOSED(LOCKED)");
  }
  else if (cmd == "WINDOWS_FREE") {
    windowsStatus = "free";
    Serial.println("STATUS:WINDOWS=FREE");
  }

  // ── Fan ──
  else if (cmd == "FAN_ON") {
    fanStatus = "on";
    activateFanFull();
    Serial.println("STATUS:FAN=ON(LOCKED)");
  }
  else if (cmd == "FAN_OFF") {
    fanStatus = "off";
    deactivateFan();
    Serial.println("STATUS:FAN=OFF(LOCKED)");
  }
  else if (cmd == "FAN_FREE") {
    fanStatus = "free";
    Serial.println("STATUS:FAN=FREE");
  }

  // ── Buzzer ──
  else if (cmd == "BUZZER_ON") {
    buzzerStatus = "on";
    digitalWrite(buzzer, HIGH);
    buzzerActive = true;
    buzzerStart  = millis();
    Serial.println("STATUS:BUZZER=ON(LOCKED)");
  }
  else if (cmd == "BUZZER_OFF") {
    buzzerStatus = "off";
    digitalWrite(buzzer, LOW);
    buzzerActive = false;
    Serial.println("STATUS:BUZZER=OFF(LOCKED)");
  }
  else if (cmd == "BUZZER_FREE") {
    buzzerStatus = "free";
    Serial.println("STATUS:BUZZER=FREE");
  }

  // ── Water pump ──
  else if (cmd == "WATERPUMP_ON") {
    waterPumpStatus = "on";
    digitalWrite(waterPump, HIGH);
    Serial.println("STATUS:WATERPUMP=ON(LOCKED)");
  }
  else if (cmd == "WATERPUMP_OFF") {
    waterPumpStatus = "off";
    digitalWrite(waterPump, LOW);
    Serial.println("STATUS:WATERPUMP=OFF(LOCKED)");
  }
  else if (cmd == "WATERPUMP_FREE") {
    waterPumpStatus = "free";
    Serial.println("STATUS:WATERPUMP=FREE");
  }

  // ── Status report ──
  else if (cmd == "STATUS") {
    Serial.print("STATUS:");
    Serial.print("DOORS=");     Serial.print(doorsStatus);     Serial.print("|");
    Serial.print("WINDOWS=");   Serial.print(windowsStatus);   Serial.print("|");
    Serial.print("FAN=");       Serial.print(fanStatus);       Serial.print("|");
    Serial.print("BUZZER=");    Serial.print(buzzerStatus);    Serial.print("|");
    Serial.print("WATERPUMP="); Serial.print(waterPumpStatus); Serial.print("|");
    Serial.print("MODE=");      Serial.println(automaticMode ? "AUTO" : "MANUAL");
  }
}

// ─────────────────────────────────────────
//  DOOR & WINDOW CONTROL
//  These always physically move the servos.
//  The status variable is what prevents auto
//  from calling these functions again.
// ─────────────────────────────────────────
void closeDoors() {
  servoDoor1.attach(servoDoor1pin);
  servoDoor2.attach(servoDoor2pin);
  servoDoor1.write(closedPosition);
  servoDoor2.write(closedPosition);
  delay(500);
  servoDoor1.detach();
  servoDoor2.detach();
}

void openDoors() {
  servoDoor1.attach(servoDoor1pin);
  servoDoor2.attach(servoDoor2pin);
  servoDoor1.write(openPosition);
  servoDoor2.write(openPosition);
  delay(500);
  servoDoor1.detach();
  servoDoor2.detach();
}

void openWindows() {
  servoWindow1.attach(servoWindow1pin);
  servoWindow2.attach(servoWindow2pin);
  servoWindow1.write(openPosition);
  servoWindow2.write(openPosition);
  delay(500);
  servoWindow1.detach();
  servoWindow2.detach();
}

void closeWindows() {
  servoWindow1.attach(servoWindow1pin);
  servoWindow2.attach(servoWindow2pin);
  servoWindow1.write(closedPosition);
  servoWindow2.write(closedPosition);
  delay(500);
  servoWindow1.detach();
  servoWindow2.detach();
}

// ─────────────────────────────────────────
//  FAN
// ─────────────────────────────────────────
void activateFan(int smokeValue) {
  if (smokeValue > thresholdSmoke) {
    analogWrite(fan, 255);
  } else {
    analogWrite(fan, 120);
  }
  fanActive = true;
}

void activateFanFull() {
  analogWrite(fan, 255);
  fanActive = true;
}

void deactivateFan() {
  analogWrite(fan, 0);
  fanActive = false;
}

// ─────────────────────────────────────────
//  ALERT FUNCTIONS
//  Each checks the relevant status variable
//  before touching a locked component.
// ─────────────────────────────────────────
void fire() {
  if (fireMode) return;  // already triggered, don't repeat
  fireMode = true;

  currentAlert = "FIRE DETECTED";
  lcdMode      = 1;
  lcdAlertTime = millis();

  digitalWrite(ledRed, HIGH);
  redStart = millis();

  if (buzzerStatus == "free") {
    digitalWrite(buzzer, HIGH);
    buzzerStart  = millis();
    buzzerActive = true;
  }

  if (windowsStatus == "free") openWindows();
  if (doorsStatus   == "free") openDoors();

  Serial.println("FIRE");
}

void smoke(int smokeValue) {

  currentAlert = "SMOKE DETECTED";
  lcdMode      = 1;
  lcdAlertTime = millis();

  digitalWrite(ledBlue, HIGH);
  blueStart = millis();

  // Buzzer
  if (buzzerStatus == "free") {
    digitalWrite(buzzer, HIGH);
    buzzerStart  = millis();
    buzzerActive = true;
  }

  // Open windows
  if (windowsStatus == "free") {
    openWindows();
  }

  // Fan + keep-alive timer
  if (fanStatus == "free") {

    activateFan(smokeValue);

    // restart the timer every time smoke is detected
    smokeFanStart   = millis();
    smokeFanRunning = true;
  }

  Serial.println("SMOKE");
}

void humidity() {
  currentAlert = "HIGH HUMIDITY";
  lcdMode      = 1;
  lcdAlertTime = millis();

  digitalWrite(ledGreen, HIGH);
  greenStart = millis();

  if (buzzerStatus == "free") {
    digitalWrite(buzzer, HIGH);
    buzzerStart  = millis();
    buzzerActive = true;
  }

  if (fanStatus == "free") activateFan(0);

  Serial.println("HIGH HUMIDITY");
}

void motion() {
  if (intrusionMode) return;

  intrusionMode  = true;
  lastMotionTime = millis();

  currentAlert = "INTRUSION ALERT";
  lcdMode      = 1;
  lcdAlertTime = millis();

  digitalWrite(ledYellow, HIGH);
  yellowStart = millis();

  if (buzzerStatus == "free") {
    digitalWrite(buzzer, HIGH);
    buzzerStart  = millis();
    buzzerActive = true;
  }

  if (doorsStatus   == "free") closeDoors();
  if (windowsStatus == "free") closeWindows();

  Serial.println("MOTION");
}

// ─────────────────────────────────────────
//  KEYPAD
// ─────────────────────────────────────────
void keypad() {
  char key = customKeypad.getKey();
  if (!key) return;

  if (key == '*') {
    input = "";
    Serial.println("\nInput Cleared");
    return;
  }

  Serial.print(key);
  input += key;

  if (locked) {
    if (input.length() == 7) {
      if (input == resetCode) {
        Serial.println("\nFactory Reset — system unlocked.");
        tries  = 0;
        locked = false;
      } else {
        Serial.println("\nWrong Reset Code.");
      }
      input = "";
    }
    return;
  }

  if (input.length() == 3) {
    if (input == password) {
      Serial.println("\nCorrect Password");
      // Keypad always overrides the lock for the door (physical entry)
      openDoors();
      delay(3000);
      closeDoors();
      // Restore whatever the Telegram lock said afterwards
      if (doorsStatus == "open")   openDoors();
      if (doorsStatus == "closed") closeDoors();
    } else {
      tries++;
      Serial.print("\nWrong Password | Tries: ");
      Serial.println(tries);
      if (tries >= 3) {
        locked = true;
        Serial.println("SYSTEM LOCKED — Enter Reset Code.");
      }
    }
    input = "";
  }
}

// ─────────────────────────────────────────
//  IR REMOTE CONTROL
// ─────────────────────────────────────────
void remoteControl() {
  if (!RR.decode(&results)) return;

  unsigned long value = results.value;
  Serial.println(value, HEX);

  if (value == 0xFF02FD) {//OK
    automaticMode     = !automaticMode;
    lcdMode           = 0;
    Serial.println(automaticMode ? "AUTO MODE" : "MANUAL MODE");
  }

  if (!automaticMode) {
    if (value == 0xFF6897) { doorsStatus = "open";    openDoors(); }//1
    if (value == 0xFF9867) { doorsStatus = "closed";  closeDoors(); }//2
    if (value == 0xFFB04F) { windowsStatus = "open";  openWindows(); }//3
    if (value == 0xFF30CF) { windowsStatus = "closed"; closeWindows(); }//4
    if (value == 0xFF18E7) {  //5 - fan half speed toggle
      if (fanStatus == "on") { fanStatus = "off"; deactivateFan(); }
      else                   { fanStatus = "on";  activateFan(0); }
    }
    if (value == 0xFF7A85) {  //6 - fan full speed toggle
      if (fanStatus == "on") { fanStatus = "off"; deactivateFan(); }
      else                   { fanStatus = "on";  activateFanFull(); }
    }
    if (value == 0xFF10EF) {//7
      buzzerStatus = "on";
      digitalWrite(buzzer, HIGH);
      buzzerStart  = millis();
      buzzerActive = true;
    }
    if (value == 0xFF38C7) {//8
      buzzerStatus = "off";
      digitalWrite(buzzer, LOW);
      buzzerActive = false;
    }
    if (value == 0xFF5AA5) { waterPumpStatus = "on";  digitalWrite(waterPump, HIGH); }//9
    if (value == 0xFF4AB5) { waterPumpStatus = "off"; digitalWrite(waterPump, LOW); }//0
  }

  RR.resume();
}

// ─────────────────────────────────────────
//  LCD UPDATE
// ─────────────────────────────────────────
void updateLCD(int smokeValue, int flameValue, long distanceValue, int tempValue) {
  if (lcdMode == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(currentAlert);
    lcd.setCursor(0, 1);
    lcd.print("System Alert");
    if (millis() - lcdAlertTime > alertDuration) {
      lcdMode      = 0;
      currentAlert = "";
    }
    return;
  }

  lcd.setCursor(0, 0);
  lcd.print(automaticMode ? "AUTO" : "MAN ");
  lcd.print(" S:");
  lcd.print(smokeValue);
  lcd.print("   ");

  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(tempValue);
  lcd.print(" F:");
  lcd.print(flameValue);
  lcd.print(distanceValue < 25 ? " M:YES" : " M:NO ");
}

// ─────────────────────────────────────────
//  SYSTEM UPDATE (timers)
// ─────────────────────────────────────────
void systemUpdate() {
  if (redStart    != 0 && millis() - redStart    > ledDuration) { digitalWrite(ledRed,    LOW); redStart    = 0; }
  if (blueStart   != 0 && millis() - blueStart   > ledDuration) { digitalWrite(ledBlue,   LOW); blueStart   = 0; }
  if (greenStart  != 0 && millis() - greenStart  > ledDuration) { digitalWrite(ledGreen,  LOW); greenStart  = 0; }
  if (yellowStart != 0 && millis() - yellowStart > ledDuration) {
    digitalWrite(ledYellow, LOW);
    yellowStart    = 0;
    intrusionMode  = false;
    lastMotionTime = 0;
  }

  // Only auto-silence the buzzer if it isn't locked ON by Telegram
  if (buzzerStatus != "on" && buzzerActive && millis() - buzzerStart > ledDuration) {
    digitalWrite(buzzer, LOW);
    buzzerActive = false;
  }

  // Re-enforce locked states every cycle (guards against power glitches, etc.)
  if (fanStatus       == "on")     { if (!fanActive) activateFanFull(); }
  if (fanStatus       == "off")    { if (fanActive)  deactivateFan(); }
  if (waterPumpStatus == "on")     digitalWrite(waterPump, HIGH);
  if (waterPumpStatus == "off")    digitalWrite(waterPump, LOW);
  if (buzzerStatus    == "on")     { digitalWrite(buzzer, HIGH); buzzerActive = true; }
  if (buzzerStatus    == "off")    { digitalWrite(buzzer, LOW);  buzzerActive = false; }

  // keep fan on for 3 seconds after smoke disappears
  if (smokeFanRunning &&
      millis() - smokeFanStart > smokeFanDuration) {
    if (!smokeMode && fanStatus == "free") {
      deactivateFan();
    }
    smokeFanRunning = false;
  }

  if (redStart != 0 && millis() - redStart > ledDuration) {
    digitalWrite(ledRed, LOW);
    redStart  = 0;
    fireMode  = false;
  }
}
