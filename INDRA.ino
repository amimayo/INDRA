// INDRA - Intelligent Note Detector And Rupee Authenticator
// ESP32 firmware — verifies Indian Rupee notes via RGB, UV, and IR sensors

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

enum State {
  STATE_IDLE,
  STATE_TRIGGERED,
  STATE_COLOR_PROFILING,
  STATE_UV_TEST,
  STATE_IR_TEST,
  STATE_DECISION,
  STATE_OUTPUT
};

volatile State currentState = STATE_IDLE;
unsigned long stateEntryTime = 0;
volatile bool noteDetected = false;

unsigned long redFreq  = 0;
unsigned long greenFreq = 0;
unsigned long blueFreq  = 0;
int uvReading = 0;
int irReading = 0;

bool isGenuine = false;
String denomination = "Unknown";

void IRAM_ATTR onNoteTrigger();
unsigned long readTCS3200(uint8_t s2, uint8_t s3);
int averagedAnalogRead(uint8_t pin, int samples);
void identifyDenomination();
void displayText(const char* line1, const char* line2 = nullptr);
void resetToIdle();

void setup() {
  Serial.begin(115200);
  Serial.println("\n[INDRA] Booting...");

  // GPIO34 is input-only, no internal pull-up
  pinMode(PIN_TRIGGER, INPUT);

  pinMode(PIN_TCS_S2, OUTPUT);
  pinMode(PIN_TCS_S3, OUTPUT);
  pinMode(PIN_TCS_OUT, INPUT);
  pinMode(PIN_TCS_LED, OUTPUT);
  digitalWrite(PIN_TCS_LED, LOW);

  pinMode(PIN_UV_EMITTER, OUTPUT);
  digitalWrite(PIN_UV_EMITTER, LOW);

  pinMode(PIN_IR_EMITTER, OUTPUT);
  digitalWrite(PIN_IR_EMITTER, LOW);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_LED_GREEN, OUTPUT);
  digitalWrite(PIN_LED_GREEN, LOW);
  pinMode(PIN_LED_RED, OUTPUT);
  digitalWrite(PIN_LED_RED, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[INDRA] OLED init FAILED!");
    while (true);
  }
  display.clearDisplay();
  display.display();

  attachInterrupt(digitalPinToInterrupt(PIN_TRIGGER), onNoteTrigger, FALLING);

  displayText("INDRA Ready.", "Insert Note.");
  Serial.println("[INDRA] System ready. Waiting for note...");
}

void loop() {
  switch (currentState) {

    case STATE_IDLE:
      if (noteDetected) {
        noteDetected = false;
        currentState = STATE_TRIGGERED;
        Serial.println("[INDRA] Note detected! Starting pipeline...");
      }
      break;

    case STATE_TRIGGERED:
      delay(50); // debounce
      if (digitalRead(PIN_TRIGGER) == LOW) {
        currentState = STATE_COLOR_PROFILING;
      } else {
        Serial.println("[INDRA] False trigger, returning to idle.");
        currentState = STATE_IDLE;
      }
      break;

    case STATE_COLOR_PROFILING:
      displayText("Scanning...", "Color Profile");
      digitalWrite(PIN_TCS_LED, HIGH);
      delay(50);

      redFreq   = readTCS3200(LOW, LOW);    // Red
      greenFreq = readTCS3200(HIGH, HIGH);  // Green
      blueFreq  = readTCS3200(LOW, HIGH);   // Blue

      digitalWrite(PIN_TCS_LED, LOW);
      Serial.printf("[COLOR] R=%lu  G=%lu  B=%lu\n", redFreq, greenFreq, blueFreq);

      identifyDenomination();
      currentState = STATE_UV_TEST;
      break;

    case STATE_UV_TEST:
      displayText("Scanning...", "UV Test");
      digitalWrite(PIN_UV_EMITTER, HIGH);
      delay(100);

      uvReading = averagedAnalogRead(PIN_UV_RECEIVER, 10);
      digitalWrite(PIN_UV_EMITTER, LOW);
      Serial.printf("[UV] ADC = %d  (threshold = %d)\n", uvReading, UV_THRESHOLD);

      currentState = STATE_IR_TEST;
      break;

    case STATE_IR_TEST:
      displayText("Scanning...", "IR Test");
      digitalWrite(PIN_IR_EMITTER, HIGH);
      delay(100);

      irReading = averagedAnalogRead(PIN_IR_RECEIVER, 10);
      digitalWrite(PIN_IR_EMITTER, LOW);
      Serial.printf("[IR] ADC = %d  (threshold = %d)\n", irReading, IR_THRESHOLD);

      currentState = STATE_DECISION;
      break;

    case STATE_DECISION: {
      bool colorOK = (denomination != "Unknown");
      bool uvOK    = (uvReading < UV_THRESHOLD);  // genuine absorbs UV -> low ADC
      bool irOK    = (irReading < IR_THRESHOLD);   // genuine IR ink -> low ADC

      Serial.printf("[DECISION] color=%s  uv=%s  ir=%s\n",
                     colorOK ? "PASS" : "FAIL",
                     uvOK    ? "PASS" : "FAIL",
                     irOK    ? "PASS" : "FAIL");

      isGenuine = colorOK && uvOK && irOK;
      currentState = STATE_OUTPUT;
      stateEntryTime = millis();
      break;
    }

    case STATE_OUTPUT:
      if (stateEntryTime != 0) {
        if (isGenuine) {
          digitalWrite(PIN_LED_GREEN, HIGH);
          String msg = "Genuine " + denomination;
          displayText("VERIFIED", msg.c_str());
          Serial.printf("[RESULT] GENUINE — %s\n", denomination.c_str());
        } else {
          digitalWrite(PIN_LED_RED, HIGH);
          tone(PIN_BUZZER, 2000);
          displayText("WARNING!", "COUNTERFEIT");
          Serial.println("[RESULT] COUNTERFEIT DETECTED");
        }
        stateEntryTime = millis();
      }

      if (millis() - stateEntryTime >= RESULT_HOLD_MS) {
        resetToIdle();
      }
      break;
  }
}

void IRAM_ATTR onNoteTrigger() {
  if (currentState == STATE_IDLE) {
    noteDetected = true;
  }
}

// Returns pulse width in microseconds for selected color filter
unsigned long readTCS3200(uint8_t s2, uint8_t s3) {
  digitalWrite(PIN_TCS_S2, s2);
  digitalWrite(PIN_TCS_S3, s3);
  delay(20);

  unsigned long duration = pulseIn(PIN_TCS_OUT, LOW, 100000);
  if (duration == 0) {
    Serial.println("[TCS3200] Timeout — no pulse detected");
  }
  return duration;
}

int averagedAnalogRead(uint8_t pin, int samples) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(5);
  }
  return (int)(sum / samples);
}

// PLACEHOLDER thresholds — calibrate with real notes via Serial Monitor
// Lower pulse width = more of that color detected
void identifyDenomination() {
  denomination = "Unknown";

  // Rs.500 — Stone Grey
  if (redFreq > 40 && redFreq < 120 &&
      greenFreq > 50 && greenFreq < 130 &&
      blueFreq > 60 && blueFreq < 140) {
    denomination = "Rs.500";
  }
  // Rs.200 — Bright Orange
  else if (redFreq > 20 && redFreq < 80 &&
           greenFreq > 60 && greenFreq < 150 &&
           blueFreq > 80 && blueFreq < 180) {
    denomination = "Rs.200";
  }
  // Rs.100 — Lavender
  else if (redFreq > 50 && redFreq < 130 &&
           greenFreq > 40 && greenFreq < 120 &&
           blueFreq > 30 && blueFreq < 100) {
    denomination = "Rs.100";
  }
  // Rs.50 — Fluorescent Blue
  else if (redFreq > 70 && redFreq < 160 &&
           greenFreq > 50 && greenFreq < 140 &&
           blueFreq > 20 && blueFreq < 90) {
    denomination = "Rs.50";
  }
  // Rs.20 — Greenish Yellow
  else if (redFreq > 40 && redFreq < 110 &&
           greenFreq > 30 && greenFreq < 100 &&
           blueFreq > 70 && blueFreq < 160) {
    denomination = "Rs.20";
  }
  // Rs.10 — Chocolate Brown
  else if (redFreq > 30 && redFreq < 90 &&
           greenFreq > 60 && greenFreq < 150 &&
           blueFreq > 70 && blueFreq < 160) {
    denomination = "Rs.10";
  }

  Serial.printf("[DENOM] Identified: %s\n", denomination.c_str());
}

void displayText(const char* line1, const char* line2) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  if (line2) {
    display.setCursor(0, 8);
    display.println(line1);
    display.setTextSize(1);
    display.setCursor(0, 35);
    display.println(line2);
  } else {
    display.setCursor(0, 24);
    display.println(line1);
  }

  display.display();
}

void resetToIdle() {
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED, LOW);
  noTone(PIN_BUZZER);
  digitalWrite(PIN_BUZZER, LOW);

  redFreq = greenFreq = blueFreq = 0;
  uvReading = irReading = 0;
  isGenuine = false;
  denomination = "Unknown";
  stateEntryTime = 0;

  currentState = STATE_IDLE;
  displayText("INDRA Ready.", "Insert Note.");
  Serial.println("[INDRA] Reset. Waiting for note...\n");
}
