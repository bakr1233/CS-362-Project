/*
 * CS-362 "Who is the Fastest?" — Master (race control + leaderboard)
 * Parallel LCD 16x2. I2C only for lane slaves: SDA=A4, SCL=A5.
 *
 * Bench test without slaves: MASTER_SOLO_TEST = 1 (fake lane data after GO).
 */
#define MASTER_SOLO_TEST 1

#include <Wire.h>
#include <LiquidCrystal.h>

static const uint8_t SLAVE_LANE1 = 0x08;
static const uint8_t SLAVE_LANE2 = 0x09;

const int rs = 5, en = 6, d4 = 7, d5 = 8, d6 = 9, d7 = 10;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int ledPins[3] = {2, 3, 4};
const int button1 = 11;
const int button2 = 12;
const int buzzer = A0;

struct LanePacket {
  uint8_t lane;
  uint16_t speed_mm_s;
  uint16_t reaction_ms;
  uint8_t finished;
};

static const uint8_t PACKET_BYTES = 6;

struct LeaderEntry {
  uint8_t lane;
  uint16_t speed_mm_s;
};

static LeaderEntry leaderboard[5];
static uint8_t leaderboardCount = 0;

static bool raceActive = false;
static uint8_t prevFin1 = 0;
static uint8_t prevFin2 = 0;

#if MASTER_SOLO_TEST
static uint32_t soloRaceStartMs = 0;
#endif

static void allLedsOff() {
  for (uint8_t i = 0; i < 3; i++) {
    digitalWrite(ledPins[i], LOW);
  }
}

static void lcdClearLine(uint8_t row) {
  lcd.setCursor(0, row);
  lcd.print("                ");
}

static void addToLeaderboard(uint8_t lane, uint16_t speed) {
  if (speed == 0) return;
  LeaderEntry e = {lane, speed};
  if (leaderboardCount < 5) {
    leaderboard[leaderboardCount++] = e;
  } else {
    uint8_t minI = 0;
    for (uint8_t i = 1; i < 5; i++) {
      if (leaderboard[i].speed_mm_s < leaderboard[minI].speed_mm_s) minI = i;
    }
    if (speed <= leaderboard[minI].speed_mm_s) return;
    leaderboard[minI] = e;
  }
  for (uint8_t i = 0; i + 1 < leaderboardCount; i++) {
    for (uint8_t j = 0; j + 1 < leaderboardCount - i; j++) {
      if (leaderboard[j].speed_mm_s < leaderboard[j + 1].speed_mm_s) {
        LeaderEntry t = leaderboard[j];
        leaderboard[j] = leaderboard[j + 1];
        leaderboard[j + 1] = t;
      }
    }
  }
}

static void announceLane(uint8_t lane, uint16_t speed_mm_s, uint16_t reaction_ms, uint8_t lcdRow) {
  addToLeaderboard(lane, speed_mm_s);
  lcdClearLine(lcdRow);
  lcd.setCursor(0, lcdRow);
  lcd.print("L");
  lcd.print(lane);
  lcd.print(" ");
  lcd.print(speed_mm_s);
  lcd.print("mm/s R");
  lcd.print(reaction_ms);
}

static bool readLanePacket(uint8_t addr, LanePacket* out) {
  Wire.requestFrom(addr, PACKET_BYTES);
  if (Wire.available() < (int)PACKET_BYTES) return false;
  out->lane = Wire.read();
  out->speed_mm_s = (uint16_t)Wire.read() | ((uint16_t)Wire.read() << 8);
  out->reaction_ms = (uint16_t)Wire.read() | ((uint16_t)Wire.read() << 8);
  out->finished = Wire.read();
  return true;
}

#if !MASTER_SOLO_TEST
static void sendRaceStartToSlaves(uint32_t t) {
  uint8_t buf[5];
  buf[0] = 'S';
  buf[1] = (uint8_t)(t & 0xFF);
  buf[2] = (uint8_t)((t >> 8) & 0xFF);
  buf[3] = (uint8_t)((t >> 16) & 0xFF);
  buf[4] = (uint8_t)((t >> 24) & 0xFF);
  Wire.beginTransmission(SLAVE_LANE1);
  for (uint8_t i = 0; i < 5; i++) Wire.write(buf[i]);
  Wire.endTransmission();
  Wire.beginTransmission(SLAVE_LANE2);
  for (uint8_t i = 0; i < 5; i++) Wire.write(buf[i]);
  Wire.endTransmission();
}
#endif

static void runCountdown() {
  prevFin1 = prevFin2 = 0;
  allLedsOff();

  digitalWrite(ledPins[0], HIGH);
  delay(800);
  digitalWrite(ledPins[0], LOW);
  digitalWrite(ledPins[1], HIGH);
  tone(buzzer, 880, 120);
  delay(800);
  digitalWrite(ledPins[1], LOW);
  digitalWrite(ledPins[2], HIGH);
  tone(buzzer, 990, 120);
  delay(800);
  digitalWrite(ledPins[2], LOW);

  for (uint8_t i = 0; i < 3; i++) {
    digitalWrite(ledPins[i], HIGH);
  }
  tone(buzzer, 1320, 200);
  uint32_t t = millis();
#if MASTER_SOLO_TEST
  soloRaceStartMs = t;
  Serial.println(F("solo: GO (fake lanes ~2s / ~3.8s)"));
#else
  sendRaceStartToSlaves(t);
#endif
  raceActive = true;
  delay(400);
  allLedsOff();
}

static void displayLeaderboard() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TOP SPEEDS mm/s");
  if (leaderboardCount == 0) {
    lcd.setCursor(0, 1);
    lcd.print("no data yet");
    return;
  }
  lcd.setCursor(0, 1);
  lcd.print("1:L");
  lcd.print(leaderboard[0].lane);
  lcd.print(" ");
  lcd.print(leaderboard[0].speed_mm_s);
}

void setup() {
  Serial.begin(9600);
#if MASTER_SOLO_TEST
  Serial.println(F("MASTER_SOLO_TEST=1: no lane I2C"));
#else
  Wire.begin();
#endif
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Who is fastest?");
  lcd.setCursor(0, 1);
  lcd.print("Ready.");

  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
}

void loop() {
  static uint32_t lastPoll = 0;
  static uint32_t raceDeadline = 0;

  if (digitalRead(button1) == LOW) {
    delay(30);
    while (digitalRead(button1) == LOW) {}
    runCountdown();
    raceDeadline = millis() + 60000UL;
  }
  if (digitalRead(button2) == LOW) {
    delay(30);
    while (digitalRead(button2) == LOW) {}
    displayLeaderboard();
    delay(2500);
    lcd.clear();
    lcd.print("Who is fastest?");
    lcd.setCursor(0, 1);
    lcd.print("Ready.");
  }

  if (raceActive && millis() - lastPoll > 40) {
    lastPoll = millis();
#if MASTER_SOLO_TEST
    uint32_t dt = millis() - soloRaceStartMs;
    if (!prevFin1 && dt >= 2000UL) {
      announceLane(1, 500, 120, 0);
      prevFin1 = 1;
      Serial.println(F("solo: fake lane 1"));
    }
    if (!prevFin2 && dt >= 3800UL) {
      announceLane(2, 450, 200, 1);
      prevFin2 = 1;
      Serial.println(F("solo: fake lane 2"));
    }
#else
    LanePacket p1, p2;
    if (readLanePacket(SLAVE_LANE1, &p1)) {
      if (p1.finished && !prevFin1 && p1.speed_mm_s > 0) {
        announceLane(p1.lane, p1.speed_mm_s, p1.reaction_ms, 0);
      }
      prevFin1 = p1.finished;
    }
    if (readLanePacket(SLAVE_LANE2, &p2)) {
      if (p2.finished && !prevFin2 && p2.speed_mm_s > 0) {
        announceLane(p2.lane, p2.speed_mm_s, p2.reaction_ms, 1);
      }
      prevFin2 = p2.finished;
    }
#endif
    if (prevFin1 && prevFin2) raceActive = false;
    if (raceActive && raceDeadline != 0 && millis() > raceDeadline) raceActive = false;
  }
}
