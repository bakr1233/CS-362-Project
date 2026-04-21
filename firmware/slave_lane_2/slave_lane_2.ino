/*
 * CS-362 — Lane 2 sensor slave (I2C 0x09)
 * Same as lane 1 with different I2C address and lane id.
 */

#include <Wire.h>

static const uint8_t I2C_ADDR = 0x09;
static const uint8_t LANE_ID = 2;

static const uint8_t PIN_TRIG_FINISH = 9;
static const uint8_t PIN_ECHO_FINISH = 10;
static const uint8_t PIN_TRIG_START = 11;
static const uint8_t PIN_ECHO_START = 12;
static const uint8_t PIN_STATUS_LED = 13;

static const uint16_t TRACK_LENGTH_MM = 1000;
static const uint32_t DIST_THRESHOLD_CM = 8;

static uint32_t raceStartMs = 0;
static bool armed = false;
static bool finishLatched = false;
static uint16_t speedMmS = 0;
static uint16_t reactionMs = 0;

static uint8_t txBuf[6];

static uint32_t readDistanceCm(uint8_t trigPin, uint8_t echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  uint32_t dur = pulseIn(echoPin, HIGH, 30000UL);
  if (dur == 0) return 999;
  return dur / 58;
}

static void onReceive(int numBytes) {
  if (numBytes < 5) return;
  if (Wire.read() != 'S') return;
  uint32_t t = (uint32_t)Wire.read();
  t |= (uint32_t)Wire.read() << 8;
  t |= (uint32_t)Wire.read() << 16;
  t |= (uint32_t)Wire.read() << 24;
  raceStartMs = t;
  armed = true;
  finishLatched = false;
  speedMmS = 0;
  reactionMs = 0;
  digitalWrite(PIN_STATUS_LED, LOW);
}

static void onRequest() {
  txBuf[0] = LANE_ID;
  txBuf[1] = (uint8_t)(speedMmS & 0xFF);
  txBuf[2] = (uint8_t)((speedMmS >> 8) & 0xFF);
  txBuf[3] = (uint8_t)(reactionMs & 0xFF);
  txBuf[4] = (uint8_t)((reactionMs >> 8) & 0xFF);
  txBuf[5] = finishLatched ? 1 : 0;
  Wire.write(txBuf, 6);
}

void setup() {
  pinMode(PIN_TRIG_FINISH, OUTPUT);
  pinMode(PIN_TRIG_START, OUTPUT);
  pinMode(PIN_ECHO_FINISH, INPUT);
  pinMode(PIN_ECHO_START, INPUT);
  pinMode(PIN_STATUS_LED, OUTPUT);
  Wire.begin(I2C_ADDR);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
}

void loop() {
  if (!armed) {
    delay(20);
    return;
  }

  uint32_t dStart = readDistanceCm(PIN_TRIG_START, PIN_ECHO_START);
  if (reactionMs == 0 && dStart < DIST_THRESHOLD_CM) {
    reactionMs = (uint16_t)constrain((int32_t)millis() - (int32_t)raceStartMs, 0, 60000);
  }

  if (!finishLatched) {
    uint32_t dFin = readDistanceCm(PIN_TRIG_FINISH, PIN_ECHO_FINISH);
    if (dFin < DIST_THRESHOLD_CM) {
      uint32_t elapsed = millis() - raceStartMs;
      if (elapsed > 0) {
        speedMmS = (uint16_t)min(65535UL, (TRACK_LENGTH_MM * 1000UL) / elapsed);
      }
      finishLatched = true;
      digitalWrite(PIN_STATUS_LED, HIGH);
    }
  }
  delay(5);
}
