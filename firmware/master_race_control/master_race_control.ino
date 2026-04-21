/*
 * CS-362 "Who is the Fastest?" — Master (race control + leaderboard)
 * I2C master. Slaves: 0x08 (lane 1), 0x09 (lane 2).
 * Uno/Nano: SDA=A4, SCL=A5. Install "LiquidCrystal I2C" by Frank de Brabander if using I2C LCD.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static const uint8_t SLAVE_LANE1 = 0x08;
static const uint8_t SLAVE_LANE2 = 0x09;

LiquidCrystal_I2C lcd(0x27, 16, 2);

static const uint8_t PIN_START_BTN = 2;
static const uint8_t PIN_LEADER_BTN = 3;
static const uint8_t PIN_LED_COUNT_3 = 4;
static const uint8_t PIN_LED_COUNT_2 = 5;
static const uint8_t PIN_LED_COUNT_1 = 6;
static const uint8_t PIN_LED_GO = 7;
static const uint8_t PIN_BUZZER = 8;

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

static bool readLanePacket(uint8_t addr, LanePacket* out) {
  Wire.requestFrom(addr, PACKET_BYTES);
  if (Wire.available() < (int)PACKET_BYTES) return false;
  out->lane = Wire.read();
  out->speed_mm_s = (uint16_t)Wire.read() | ((uint16_t)Wire.read() << 8);
  out->reaction_ms = (uint16_t)Wire.read() | ((uint16_t)Wire.read() << 8);
  out->finished = Wire.read();
  return true;
}

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

static void runCountdown() {
  prevFin1 = prevFin2 = 0;
  digitalWrite(PIN_LED_COUNT_3, HIGH);
  delay(800);
  digitalWrite(PIN_LED_COUNT_3, LOW);
  digitalWrite(PIN_LED_COUNT_2, HIGH);
  tone(PIN_BUZZER, 880, 120);
  delay(800);
  digitalWrite(PIN_LED_COUNT_2, LOW);
  digitalWrite(PIN_LED_COUNT_1, HIGH);
  tone(PIN_BUZZER, 990, 120);
  delay(800);
  digitalWrite(PIN_LED_COUNT_1, LOW);
  digitalWrite(PIN_LED_GO, HIGH);
  tone(PIN_BUZZER, 1320, 200);
  uint32_t t = millis();
  sendRaceStartToSlaves(t);
  raceActive = true;
  delay(400);
  digitalWrite(PIN_LED_GO, LOW);
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
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Who is fastest?");
  lcd.setCursor(0, 1);
  lcd.print("Ready.");

  pinMode(PIN_START_BTN, INPUT_PULLUP);
  pinMode(PIN_LEADER_BTN, INPUT_PULLUP);
  pinMode(PIN_LED_COUNT_3, OUTPUT);
  pinMode(PIN_LED_COUNT_2, OUTPUT);
  pinMode(PIN_LED_COUNT_1, OUTPUT);
  pinMode(PIN_LED_GO, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
}

void loop() {
  static uint32_t lastPoll = 0;
  static uint32_t raceDeadline = 0;

  if (digitalRead(PIN_START_BTN) == LOW) {
    delay(30);
    while (digitalRead(PIN_START_BTN) == LOW) {}
    runCountdown();
    raceDeadline = millis() + 60000UL;
  }
  if (digitalRead(PIN_LEADER_BTN) == LOW) {
    delay(30);
    while (digitalRead(PIN_LEADER_BTN) == LOW) {}
    displayLeaderboard();
    delay(2500);
    lcd.clear();
    lcd.print("Who is fastest?");
    lcd.setCursor(0, 1);
    lcd.print("Ready.");
  }

  if (raceActive && millis() - lastPoll > 40) {
    lastPoll = millis();
    LanePacket p1, p2;
    if (readLanePacket(SLAVE_LANE1, &p1)) {
      if (p1.finished && !prevFin1 && p1.speed_mm_s > 0) {
        addToLeaderboard(p1.lane, p1.speed_mm_s);
        lcdClearLine(0);
        lcd.setCursor(0, 0);
        lcd.print("L");
        lcd.print(p1.lane);
        lcd.print(" ");
        lcd.print(p1.speed_mm_s);
        lcd.print("mm/s R");
        lcd.print(p1.reaction_ms);
      }
      prevFin1 = p1.finished;
    }
    if (readLanePacket(SLAVE_LANE2, &p2)) {
      if (p2.finished && !prevFin2 && p2.speed_mm_s > 0) {
        addToLeaderboard(p2.lane, p2.speed_mm_s);
        lcdClearLine(1);
        lcd.setCursor(0, 1);
        lcd.print("L");
        lcd.print(p2.lane);
        lcd.print(" ");
        lcd.print(p2.speed_mm_s);
        lcd.print(" R");
        lcd.print(p2.reaction_ms);
      }
      prevFin2 = p2.finished;
    }
    if (prevFin1 && prevFin2) raceActive = false;
    if (raceActive && raceDeadline != 0 && millis() > raceDeadline) raceActive = false;
  }
}
