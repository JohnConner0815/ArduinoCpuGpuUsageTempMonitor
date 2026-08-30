#include <FastLED.h>
#include <EEPROM.h>
#include <IRremote.h> 

// --- PIN DEFINITIONS ---
// Usage Group
#define DATA_PIN_GPU_USAGE 7  
#define DATA_PIN_CPU_USAGE 6  

// Temperature Group
#define CPU_METER_PIN 9      // PWM capable
#define GPU_METER_PIN 10     // PWM capable
#define LED_DATA_PIN_TEMP 11 

// Controls Group
#define IR_RECEIVER_PIN 2 
#define BTN1_PIN 3          
#define BTN2_PIN 4          
#define BTN3_PIN 12         

// --- HARDWARE CONFIGURATION ---
#define NUM_LEDS_USAGE 21
#define NUM_LEDS_TEMP 9
#define BRIGHTNESS_STEPS 10 
#define DEBOUNCE_DELAY 80
#define IR_DEBOUNCE 300 
#define IDLE_TIMEOUT 5000   // 5 seconds

// --- EEPROM ADDRESSES ---
#define EEPROM_ADDR_PROFILE 0
#define EEPROM_ADDR_BRIGHTNESS 1

// --- IR REMOTE CONFIGURATION ---
#define IR_CODE_POWER   0x0FFA25D
#define IR_CODE_EQ      0x0FF9867
#define IR_CODE_PREV    0x0FF22DD
#define IR_CODE_NEXT    0x0FFC23D
#define IR_CODE_UP      0x0FF906F
#define IR_CODE_DOWN    0x0FFE01F
#define IR_CODE_REPEAT  0xFFFFFFFF

#define IR_CODE_0       0x0FF6897
#define IR_CODE_1       0x0FF30CF
#define IR_CODE_2       0x0FF18E7
#define IR_CODE_3       0x0FF7A85
#define IR_CODE_4       0x0FF10EF
#define IR_CODE_5       0x0FF38C7
#define IR_CODE_6       0x0FF5AA5
#define IR_CODE_7       0x0FF42BD
#define IR_CODE_8       0x0FF4AB5
#define IR_CODE_9       0x0FF52AD

// --- INITIALIZATION ---
CRGB ledsGpu[NUM_LEDS_USAGE];
CRGB ledsCpu[NUM_LEDS_USAGE];
CRGB ledsTemp[NUM_LEDS_TEMP];

uint8_t brightnessLevels[BRIGHTNESS_STEPS] = {4, 8, 20, 33, 57, 81, 118, 155, 205, 255};

// State variables
int cpu = 0;
int gpu = 0;
int cpuTemp = 0;
int gpuTemp = 0;
int activeLedsCpu = 0;
int activeLedsGpu = 0;
int currentProfile = 1;
int brightnessIndex = 9; 
bool ledsNeedUpdate = true; 
bool isPoweredOn = true;  
bool isIdleMode = false;

// Idle timing variables
unsigned long lastSerialReceiveTime = 0;
unsigned long idleStartTime = 0;

// Normal button debouncing variables
int btn1State = HIGH;
int lastBtn1State = HIGH;
unsigned long lastBtn1DebounceTime = 0;

int btn2State = HIGH;
int lastBtn2State = HIGH;
unsigned long lastBtn2DebounceTime = 0;

// Test button variables
int btn3State = HIGH;
int lastBtn3State = HIGH;
unsigned long btn3PressStartTime = 0;
bool testModeTriggered = false;
bool inTestMode = false;

// IR Remote Variables
unsigned long lastIrActionTime = 0; 
unsigned long irBtnEqPressStartTime = 0;
bool irBtnEqHolding = false;

// Temp LED blink state variables
unsigned long previousBlinkTime = 0;  
bool blinkState = false;              

IRrecv irrecv(IR_RECEIVER_PIN);
decode_results results;

void setup() {
  Serial.begin(19200);
  
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  pinMode(CPU_METER_PIN, OUTPUT);
  pinMode(GPU_METER_PIN, OUTPUT);

  currentProfile = EEPROM.read(EEPROM_ADDR_PROFILE);
  if (currentProfile < 1 || currentProfile > 10) currentProfile = 1;

  brightnessIndex = EEPROM.read(EEPROM_ADDR_BRIGHTNESS);
  if (brightnessIndex >= BRIGHTNESS_STEPS) brightnessIndex = 9; 

  FastLED.addLeds<WS2812B, DATA_PIN_GPU_USAGE, GRB>(ledsGpu, NUM_LEDS_USAGE);
  FastLED.addLeds<WS2812B, DATA_PIN_CPU_USAGE, GRB>(ledsCpu, NUM_LEDS_USAGE);
  FastLED.addLeds<WS2812B, LED_DATA_PIN_TEMP, GRB>(ledsTemp, NUM_LEDS_TEMP).setCorrection(TypicalLEDStrip);
  
  FastLED.setBrightness(brightnessLevels[brightnessIndex]); 
  FastLED.clear();
  FastLED.show();

  irrecv.enableIRIn(); 

  // --- STARTUP SEQUENCE (Dynamic for both Usage and Temp) ---
  unsigned long totalUsageTime = (unsigned long)NUM_LEDS_USAGE * 75; 
  int totalTempSteps = NUM_LEDS_TEMP / 2; 
  unsigned long tempStepDelay = (totalTempSteps > 0) ? (totalUsageTime / totalTempSteps) : 0;
  int currentTempStep = 0;

  for (int i = 0; i < NUM_LEDS_USAGE; i++) {
    ledsGpu[i] = CRGB::White;
    ledsCpu[i] = CRGB::White;

    if (totalTempSteps > 0 && currentTempStep < totalTempSteps) {
      unsigned long currentTime = (unsigned long)i * 75;
      if (currentTime >= (unsigned long)currentTempStep * tempStepDelay) {
        ledsTemp[currentTempStep] = CRGB::White; 
        ledsTemp[NUM_LEDS_TEMP - 1 - currentTempStep] = CRGB::White; 
        currentTempStep++;
      }
    }

    FastLED.show();
    delay(75);
  }
  
  delay(500);
  FastLED.clear();
  FastLED.show();
  
  analogWrite(CPU_METER_PIN, 0);
  analogWrite(GPU_METER_PIN, 0);
  
  ledsNeedUpdate = false; 
  lastSerialReceiveTime = millis(); // Initialize serial timer
}

void loop() {
  // 0. HANDLE IR REMOTE SIGNALS
  if (irrecv.decode(&results)) {
    bool handled = true;

    if (results.value == IR_CODE_POWER) {
      isPoweredOn = !isPoweredOn;
      if (!isPoweredOn) {
        FastLED.clear(); FastLED.show(); 
        isIdleMode = false; // Turn off idle if powered off
      } else {
        ledsNeedUpdate = true; 
        lastSerialReceiveTime = millis(); // Reset idle timer on wake
      }
    } 
    else if (results.value == IR_CODE_PREV) {
      if (isPoweredOn && !isIdleMode && (millis() - lastIrActionTime > IR_DEBOUNCE)) {
        currentProfile--;
        if (currentProfile < 1) currentProfile = 10;
        EEPROM.write(EEPROM_ADDR_PROFILE, currentProfile);
        ledsNeedUpdate = true;
        lastIrActionTime = millis();
      }
    }
    else if (results.value == IR_CODE_NEXT) {
      if (isPoweredOn && !isIdleMode && (millis() - lastIrActionTime > IR_DEBOUNCE)) {
        currentProfile++;
        if (currentProfile > 10) currentProfile = 1;
        EEPROM.write(EEPROM_ADDR_PROFILE, currentProfile);
        ledsNeedUpdate = true;
        lastIrActionTime = millis();
      }
    }
    else if (results.value == IR_CODE_UP) {
      if (isPoweredOn && !isIdleMode && (millis() - lastIrActionTime > IR_DEBOUNCE)) {
        if (currentProfile != 2 && currentProfile != 4 && currentProfile != 6) {
          if (brightnessIndex < BRIGHTNESS_STEPS - 1) { 
            brightnessIndex++;
            FastLED.setBrightness(brightnessLevels[brightnessIndex]);
            EEPROM.write(EEPROM_ADDR_BRIGHTNESS, brightnessIndex);
            ledsNeedUpdate = true;
          }
        }
        lastIrActionTime = millis();
      }
    }
    else if (results.value == IR_CODE_DOWN) {
      if (isPoweredOn && !isIdleMode && (millis() - lastIrActionTime > IR_DEBOUNCE)) {
        if (currentProfile != 2 && currentProfile != 4 && currentProfile != 6) {
          if (brightnessIndex > 0) { 
            brightnessIndex--;
            FastLED.setBrightness(brightnessLevels[brightnessIndex]);
            EEPROM.write(EEPROM_ADDR_BRIGHTNESS, brightnessIndex);
            ledsNeedUpdate = true;
          }
        }
        lastIrActionTime = millis();
      }
    }
    else if (results.value == IR_CODE_EQ) {
      if (isPoweredOn) {
        irBtnEqPressStartTime = millis();
        irBtnEqHolding = true;
      }
    }
    else if (results.value == IR_CODE_REPEAT && irBtnEqHolding) {
      if (millis() - irBtnEqPressStartTime >= 1000) {
        inTestMode = true;
        irBtnEqHolding = false; 
        runTestSequence();
        lastIrActionTime = millis();
      }
    }
    // Number Buttons (Direct Profile Select) - Enabled in Idle Mode
    else if (results.value == IR_CODE_1) { if (isPoweredOn) changeProfileDirect(1); }
    else if (results.value == IR_CODE_2) { if (isPoweredOn) changeProfileDirect(2); }
    else if (results.value == IR_CODE_3) { if (isPoweredOn) changeProfileDirect(3); }
    else if (results.value == IR_CODE_4) { if (isPoweredOn) changeProfileDirect(4); }
    else if (results.value == IR_CODE_5) { if (isPoweredOn) changeProfileDirect(5); }
    else if (results.value == IR_CODE_6) { if (isPoweredOn) changeProfileDirect(6); }
    else if (results.value == IR_CODE_7) { if (isPoweredOn) changeProfileDirect(7); }
    else if (results.value == IR_CODE_8) { if (isPoweredOn) changeProfileDirect(8); }
    else if (results.value == IR_CODE_9) { if (isPoweredOn) changeProfileDirect(9); }
    else if (results.value == IR_CODE_0) { if (isPoweredOn) changeProfileDirect(10); }
    else {
      handled = false; 
      irBtnEqHolding = false; 
    }

    if (handled) lastIrActionTime = millis(); 
    irrecv.resume(); 
  }

  // 1. Handle Physical Buttons
  handlePhysicalButton1(); // Disabled in idle mode (except wake)
  handlePhysicalButton2(); // Disabled in idle mode (except wake)
  handlePhysicalButton3(); // Always active

  if (inTestMode) return;

  // 2. Read Data from PC tool
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    int c1 = input.indexOf(',');
    int c2 = input.indexOf(',', c1 + 1);
    int c3 = input.indexOf(',', c2 + 1);
    
    if (c1 > 0 && c2 > c1 && c3 > c2) {
      cpu = input.substring(0, c1).toInt();
      gpu = input.substring(c1 + 1, c2).toInt();
      cpuTemp = input.substring(c2 + 1, c3).toInt();
      gpuTemp = input.substring(c3 + 1).toInt();
      
      cpu = constrain(cpu, 0, 100);
      gpu = constrain(gpu, 0, 100);
      cpuTemp = constrain(cpuTemp, 0, 100);
      gpuTemp = constrain(gpuTemp, 0, 100);
      
      lastSerialReceiveTime = millis(); // Reset timeout timer
      
      if (isIdleMode) {
        isIdleMode = false; // Exit idle mode
      }
      
      ledsNeedUpdate = true; 
    }
  }

    // 3. Handle IDLE MODE Logic
  if (!isIdleMode && !inTestMode && (millis() - lastSerialReceiveTime > IDLE_TIMEOUT)) {
    isIdleMode = true;
    idleStartTime = millis();
  }

  if (isIdleMode) {
    unsigned long elapsed = millis() - idleStartTime;
    if (elapsed > 20000) {
      idleStartTime = millis();
      elapsed = 0;
    }
    
    int fakeLoad;
    if (elapsed <= 10000) {
      fakeLoad = map(elapsed, 0, 10000, 0, 100);
    } else {
      fakeLoad = map(elapsed, 10000, 20000, 100, 0);
    }
    
    fakeLoad = constrain(fakeLoad, 0, 100);
    
    // ONLY update if the value actually changes to prevent IR receiver lockout
    if (cpu != fakeLoad) {
      cpu = fakeLoad;
      gpu = fakeLoad;
      cpuTemp = fakeLoad;
      gpuTemp = fakeLoad;
      
      activeLedsCpu = map(cpu, 0, 100, 0, NUM_LEDS_USAGE);
      activeLedsGpu = map(gpu, 0, 100, 0, NUM_LEDS_USAGE);
      
      // Force VU meters to 0
      analogWrite(CPU_METER_PIN, 0);
      analogWrite(GPU_METER_PIN, 0);
      
      ledsNeedUpdate = true; 
    }
  } 
  else {
    // Normal active calculation
    activeLedsCpu = map(cpu, 0, 100, 0, NUM_LEDS_USAGE);
    activeLedsGpu = map(gpu, 0, 100, 0, NUM_LEDS_USAGE);

    // Update VU Meters (ALWAYS active, even when powered off)
    updateMeters();
  } 

  // 5. Update LED colors ONLY when a change occurred
  if (ledsNeedUpdate) {
    FastLED.clear(); // Clear all 3 strips

    if (isPoweredOn) {
      renderUsageLEDs();
      renderTempLEDs();
    }
    
    FastLED.show();
    ledsNeedUpdate = false; 
  }
}

// ==========================================
// RENDERING FUNCTIONS
// ==========================================
void renderUsageLEDs() {
  bool isBrightnessProfile = (currentProfile == 2 || currentProfile == 4 || currentProfile == 6);

  if (isBrightnessProfile) {
    uint8_t cpuBright = getLoadBrightness(cpu);
    uint8_t gpuBright = getLoadBrightness(gpu);
    CRGB cpuColor = getProfileColor(0, cpu);
    CRGB gpuColor = getProfileColor(0, gpu);

    for (int i = 0; i < NUM_LEDS_USAGE; i++) {
      ledsCpu[i] = cpuColor;
      ledsCpu[i].nscale8_video(cpuBright);
      
      ledsGpu[i] = gpuColor;
      ledsGpu[i].nscale8_video(gpuBright);
    }
  } 
  else {
    if (currentProfile == 9) {
      CRGB cpuColor9 = getProfileColor(0, cpu);
      CRGB gpuColor9 = getProfileColor(0, gpu);
      fill_solid(ledsCpu, NUM_LEDS_USAGE, cpuColor9);
      fill_solid(ledsGpu, NUM_LEDS_USAGE, gpuColor9);
    } 
    else if (currentProfile != 10) {
      for (int i = 0; i < activeLedsCpu; i++) ledsCpu[i] = getProfileColor(i, cpu);
      for (int i = 0; i < activeLedsGpu; i++) ledsGpu[i] = getProfileColor(i, gpu);
    }
  }
}

void renderTempLEDs() {
  bool cpuBlink = (cpuTemp >= 96);
  bool gpuBlink = (gpuTemp >= 81);
  
  if (cpuBlink || gpuBlink) {
    if (millis() - previousBlinkTime >= 200) { // 200ms blink interval
      blinkState = !blinkState; 
      previousBlinkTime = millis();
    }
    
    if (cpuBlink) {
      fill_solid(&ledsTemp[5], 4, blinkState ? CRGB::Red : CRGB::Black);
    } else {
      fill_solid(&ledsTemp[5], 4, getCpuColor(cpuTemp));
    }

    if (gpuBlink) {
      fill_solid(ledsTemp, 4, blinkState ? CRGB::Red : CRGB::Black);
    } else {
      fill_solid(ledsTemp, 4, getGpuColor(gpuTemp));
    }
    
    ledsTemp[4] = CRGB::Black;
  } 
  else {
    CRGB cpuColor = getCpuColor(cpuTemp);
    CRGB gpuColor = getGpuColor(gpuTemp);

    fill_solid(ledsTemp, 4, gpuColor);
    ledsTemp[4] = CRGB::Black;
    fill_solid(&ledsTemp[5], 4, cpuColor);
  }
}

void updateMeters() {
  analogWrite(CPU_METER_PIN, map(cpuTemp, 0, 100, 0, 242));
  analogWrite(GPU_METER_PIN, map(gpuTemp, 0, 100, 0, 229));
}

// ==========================================
// PHYSICAL BUTTON HANDLERS
// ==========================================
void handlePhysicalButton1() {
  int btnReading = digitalRead(BTN1_PIN);
  if (btnReading != lastBtn1State) { lastBtn1DebounceTime = millis(); }
  if ((millis() - lastBtn1DebounceTime) > DEBOUNCE_DELAY) {
    if (btnReading != btn1State) {
      btn1State = btnReading;
      if (btn1State == LOW) {
        if (!isPoweredOn) {
          isPoweredOn = true;
          lastSerialReceiveTime = millis(); // Prevent instant idle
          ledsNeedUpdate = true;
        } else if (!isIdleMode) { // Disabled in idle mode
          currentProfile++;
          if (currentProfile > 10) currentProfile = 1;
          EEPROM.write(EEPROM_ADDR_PROFILE, currentProfile);
          ledsNeedUpdate = true; 
        }
      }
    }
  }
  lastBtn1State = btnReading;
}

void handlePhysicalButton2() {
  int btnReading = digitalRead(BTN2_PIN);
  if (btnReading != lastBtn2State) { lastBtn2DebounceTime = millis(); }
  if ((millis() - lastBtn2DebounceTime) > DEBOUNCE_DELAY) {
    if (btnReading != btn2State) {
      btn2State = btnReading;
      if (btn2State == LOW) {
        if (!isPoweredOn) {
          isPoweredOn = true;
          lastSerialReceiveTime = millis(); // Prevent instant idle
          ledsNeedUpdate = true;
        } else if (!isIdleMode) { // Disabled in idle mode
          if (currentProfile != 2 && currentProfile != 4 && currentProfile != 6) {
            brightnessIndex++;
            if (brightnessIndex >= BRIGHTNESS_STEPS) brightnessIndex = 0;
            FastLED.setBrightness(brightnessLevels[brightnessIndex]);
            EEPROM.write(EEPROM_ADDR_BRIGHTNESS, brightnessIndex);
            ledsNeedUpdate = true; 
          }
        }
      }
    }
  }
  lastBtn2State = btnReading;
}

void handlePhysicalButton3() {
  int btnReading = digitalRead(BTN3_PIN);
  if (btnReading != lastBtn3State) {
    if (btnReading == LOW) {
      btn3PressStartTime = millis();
      testModeTriggered = false;
    } else {
      if (!testModeTriggered && (millis() - btn3PressStartTime >= 1000)) {
        testModeTriggered = true;
        if (!isPoweredOn) {
          isPoweredOn = true;
          lastSerialReceiveTime = millis();
          ledsNeedUpdate = true;
        } else {
          inTestMode = true;
          runTestSequence();
        }
      }
    }
  }
  lastBtn3State = btnReading;
}

// ==========================================
// TEST SEQUENCE LOGIC
// ==========================================
void runTestSequence() {
  int savedProfile = currentProfile;
  int savedBrightness = brightnessIndex;

  FastLED.setBrightness(brightnessLevels[3]); 
  FastLED.clear();
  FastLED.show();

  unsigned long testStartTime = millis();

  for (int p = 1; p <= 9; p++) {
    bool isBrightnessProfileTest = (p == 2 || p == 4 || p == 6);
    FastLED.clear();

    for (int i = 0; i < NUM_LEDS_USAGE; i++) {
      float fakeLoad = (float)i / (NUM_LEDS_USAGE - 1); 
      int fakeLoadPercent = map(i, 0, NUM_LEDS_USAGE - 1, 0, 100);
      
      CRGB currentColor;
      switch (p) {
        case 1: currentColor = CRGB::White; break;
        case 2: currentColor = CRGB::White; break;
        case 3: currentColor = CRGB::Blue; break;
        case 4: currentColor = CRGB::Blue; break;
        case 5: currentColor = CRGB::Cyan; break;
        case 6: currentColor = CRGB::Cyan; break;
        case 7: currentColor = getLoadColor(fakeLoad); break;
        case 8: currentColor = getLoadColor(fakeLoad); break;
        case 9: currentColor = getLoadColor(fakeLoad); break;
      }

      if (isBrightnessProfileTest) {
        uint8_t dynamicBright = getLoadBrightness(fakeLoadPercent);
        for (int j = 0; j < NUM_LEDS_USAGE; j++) {
          ledsCpu[j] = currentColor; ledsCpu[j].nscale8_video(dynamicBright);
          ledsGpu[j] = currentColor; ledsGpu[j].nscale8_video(dynamicBright);
        }
      } else if (p == 8) {
        for (int j = 0; j <= i; j++) { ledsCpu[j] = currentColor; ledsGpu[j] = currentColor; }
      } else if (p == 9) {
        fill_solid(ledsCpu, NUM_LEDS_USAGE, currentColor);
        fill_solid(ledsGpu, NUM_LEDS_USAGE, currentColor);
      } else {
        ledsCpu[i] = currentColor;
        ledsGpu[i] = currentColor;
      }

      unsigned long elapsed = millis() - testStartTime;
      int tempVal = 0;
      if (elapsed < 5000) {
        tempVal = map(elapsed, 0, 5000, 0, 100); 
      } else if (elapsed < 6000) {
        tempVal = 100; 
      } else if (elapsed < 11000) {
        tempVal = map(elapsed, 6000, 11000, 100, 0); 
      } else {
        tempVal = 0; 
      }

      CRGB cpuTempColor = getCpuColor(tempVal);
      CRGB gpuTempColor = getGpuColor(tempVal);
      fill_solid(ledsTemp, 4, gpuTempColor);
      ledsTemp[4] = CRGB::Black;
      fill_solid(&ledsTemp[5], 4, cpuTempColor);

      analogWrite(CPU_METER_PIN, map(tempVal, 0, 100, 0, 242));
      analogWrite(GPU_METER_PIN, map(tempVal, 0, 100, 0, 229));

      FastLED.show();
      delay(75); 
    }
    delay(500); 
    FastLED.clear();
    FastLED.show();
  }

  analogWrite(CPU_METER_PIN, 0);
  analogWrite(GPU_METER_PIN, 0);
  FastLED.clear();

  for (int f = 0; f < 3; f++) {
    fill_solid(ledsCpu, NUM_LEDS_USAGE, CRGB::White);
    fill_solid(ledsGpu, NUM_LEDS_USAGE, CRGB::White);
    fill_solid(ledsTemp, NUM_LEDS_TEMP, CRGB::White);
    FastLED.show();
    delay(300); 
    
    FastLED.clear();
    FastLED.show();
    delay(300); 
  }

  currentProfile = savedProfile;
  brightnessIndex = savedBrightness;
  FastLED.setBrightness(brightnessLevels[brightnessIndex]);
  FastLED.clear();
  FastLED.show();
  
  // If we were in idle mode before the test, seamlessly resume it
  if (isIdleMode) {
    idleStartTime = millis(); 
  }
  
  inTestMode = false; 
  ledsNeedUpdate = false; 
}

// ==========================================
// HELPER FUNCTIONS
// ==========================================
void changeProfileDirect(int targetProfile) {
  if (currentProfile != targetProfile) {
    currentProfile = targetProfile;
    EEPROM.write(EEPROM_ADDR_PROFILE, currentProfile);
    ledsNeedUpdate = true;
  }
}

uint8_t getLoadBrightness(int loadPercent) {
  loadPercent = constrain(loadPercent, 0, 100);
  if (loadPercent <= 10) return map(loadPercent, 0, 10, 0, 4);
  else if (loadPercent <= 20) return map(loadPercent, 10, 20, 4, 8);
  else if (loadPercent <= 30) return map(loadPercent, 20, 30, 8, 20);
  else if (loadPercent <= 40) return map(loadPercent, 30, 40, 20, 33);
  else if (loadPercent <= 50) return map(loadPercent, 40, 50, 33, 57);
  else if (loadPercent <= 60) return map(loadPercent, 50, 60, 57, 81);
  else if (loadPercent <= 70) return map(loadPercent, 60, 70, 81, 118);
  else if (loadPercent <= 80) return map(loadPercent, 70, 80, 118, 155);
  else if (loadPercent <= 90) return map(loadPercent, 80, 90, 155, 205);
  else return map(loadPercent, 90, 100, 205, 255);
}

CRGB getProfileColor(int ledIndex, int loadPercent) {
  switch (currentProfile) {
    case 1: return CRGB::White;
    case 2: return CRGB::White; 
    case 3: return CRGB::Blue;
    case 4: return CRGB::Blue;  
    case 5: return CRGB::Cyan;
    case 6: return CRGB::Cyan;  
    case 7: return getLoadColor((float)ledIndex / (NUM_LEDS_USAGE - 1)); 
    case 8: return getLoadColor((float)loadPercent / 100.0); 
    case 9: return getLoadColor((float)loadPercent / 100.0); 
    default: return CRGB::Black; 
  }
}

CRGB getLoadColor(float percent) {
  percent = constrain(percent, 0.0, 1.0);
  if (percent <= 0.50) return CRGB(0, 255, 0); 
  else if (percent <= 0.60) return CRGB((uint8_t)(255 * ((percent - 0.50) / 0.10)), 255, 0);
  else if (percent <= 0.70) return CRGB(255, 255, 0); 
  else if (percent <= 0.75) return CRGB(255, 255 - (uint8_t)(90 * ((percent - 0.70) / 0.05)), 0);
  else if (percent <= 0.80) return CRGB(255, 165, 0); 
  else if (percent <= 0.90) return CRGB(255, 165 - (uint8_t)(165 * ((percent - 0.80) / 0.10)), 0);
  else return CRGB(255, 0, 0); 
}

CRGB getCpuColor(int temp) {
  if (temp <= 30) return CRGB::Cyan;
  if (temp <= 35) return blend(CRGB::Cyan, CRGB::Green, map(temp, 30, 35, 0, 255));
  if (temp <= 65) return CRGB::Green; 
  if (temp <= 70) return blend(CRGB::Green, CRGB::Yellow, map(temp, 65, 70, 0, 255));
  if (temp <= 75) return CRGB::Yellow;
  if (temp <= 80) return blend(CRGB::Yellow, CRGB::Orange, map(temp, 75, 80, 0, 255));
  if (temp <= 85) return CRGB::Orange;
  if (temp <= 90) return blend(CRGB::Orange, CRGB::Red, map(temp, 85, 90, 0, 255));
  return CRGB::Red; 
}

CRGB getGpuColor(int temp) {
  if (temp <= 30) return CRGB::Cyan;
  if (temp <= 35) return blend(CRGB::Cyan, CRGB::Green, map(temp, 30, 35, 0, 255));
  if (temp <= 50) return CRGB::Green;
  if (temp <= 55) return blend(CRGB::Green, CRGB::Yellow, map(temp, 50, 55, 0, 255));
  if (temp <= 60) return CRGB::Yellow;
  if (temp <= 65) return blend(CRGB::Yellow, CRGB::Orange, map(temp, 60, 65, 0, 255));
  if (temp <= 70) return CRGB::Orange;
  if (temp <= 75) return blend(CRGB::Orange, CRGB::Red, map(temp, 70, 75, 0, 255));
  return CRGB::Red; 
}
