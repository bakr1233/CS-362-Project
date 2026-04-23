/*
 * CS-362 — Master, lane 1 only (I2C 0x08).
 * LCD 5,6,7–10 | LEDs 2,3,4 | piezo + on A0 (tone) | D11 race | D12 last run
 */

#include <Wire.h>
#include <LiquidCrystal.h>

static const uint8_t LANE1 = 0x08;

const int rs = 5, en = 6, d4 = 7, d5 = 8, d6 = 9, d7 = 10;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int led1 = 2, led2 = 3, led3 = 4;
const int buzzer = A0;
const int btnRace = 11;
const int btnLast = 12;

static uint32_t speed_mm_s = 0;
static uint32_t lap_ms = 0;
static uint32_t reaction_ms = 0;

static unsigned long db1 = 0, db2 = 0;
static int st1 = HIGH, lst1 = HIGH;
static int st2 = HIGH, lst2 = HIGH;

static void zeros() {
  speed_mm_s = 0;
  lap_ms = 0;
  reaction_ms = 0;
}

static void lcdRow0(const char* s) {
  lcd.setCursor(0, 0);
  lcd.print(s);
}

static void lcdRow1(const char* s) {
  lcd.setCursor(0, 1);
  lcd.print(s);
}

static void drawHome() {
  zeros();
  lcd.clear();
  lcdRow0("Spd 0 mm/s     ");
  lcdRow1("Lap 0 ms       ");
}

static void drawLastRun() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Spd ");
  lcd.print(speed_mm_s);
  lcd.print(" mm/s");
  lcd.setCursor(0, 1);
  lcd.print("Lap ");
  lcd.print(lap_ms);
  lcd.print(" ms");
}

static void allLedsOff() {
  digitalWrite(led1, LOW);
  digitalWrite(led2, LOW);
  digitalWrite(led3, LOW);
}

static void countdown() {
  noTone(buzzer);
  allLedsOff();
  digitalWrite(led1, HIGH);
  delay(800);
  digitalWrite(led1, LOW);
  digitalWrite(led2, HIGH);
  tone(buzzer, 880, 120);
  delay(800);
  digitalWrite(led2, LOW);
  digitalWrite(led3, HIGH);
  tone(buzzer, 990, 120);
  delay(800);
  digitalWrite(led3, LOW);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);
  tone(buzzer, 1320, 200);
  delay(450);
  noTone(buzzer);
  allLedsOff();
}

static void beepOk() {
  tone(buzzer, 1568, 90);
  delay(100);
  noTone(buzzer);
}

static void beepBad() {
  tone(buzzer, 220, 200);
  delay(220);
  noTone(buzzer);
}

static bool i2cPing(uint8_t a) {
  Wire.beginTransmission(a);
  return Wire.endTransmission() == 0;
}

static bool sendGo(uint32_t t0) {
  Wire.beginTransmission(LANE1);
  Wire.write((uint8_t)'s');
  Wire.write((uint8_t)(t0 & 0xFF));
  Wire.write((uint8_t)((t0 >> 8) & 0xFF));
  Wire.write((uint8_t)((t0 >> 16) & 0xFF));
  Wire.write((uint8_t)((t0 >> 24) & 0xFF));
  return Wire.endTransmission() == 0;
}

static bool read14(uint8_t* st, uint32_t* rx, uint32_t* lap, uint32_t* spd) {
  uint8_t n = Wire.requestFrom((int)LANE1, 14);
  if (n < 14) return false;
  *st = Wire.read();
  uint32_t r = Wire.read();
  r |= (uint32_t)Wire.read() << 8;
  r |= (uint32_t)Wire.read() << 16;
  r |= (uint32_t)Wire.read() << 24;
  uint32_t l = Wire.read();
  l |= (uint32_t)Wire.read() << 8;
  l |= (uint32_t)Wire.read() << 16;
  l |= (uint32_t)Wire.read() << 24;
  uint32_t s = Wire.read();
  s |= (uint32_t)Wire.read() << 8;
  s |= (uint32_t)Wire.read() << 16;
  s |= (uint32_t)Wire.read() << 24;
  (void)Wire.read();
  *rx = r;
  *lap = l;
  *spd = s;
  return true;
}

static void waitLane1() {
  zeros();
  lcd.clear();
  lcdRow0("Race on        ");
  lcdRow1("Spd 0 Lap 0    ");

  uint32_t t0 = millis();
  while (millis() - t0 < 120000UL) {
    delay(45);
    uint8_t st = 0;
    uint32_t r = 0, l = 0, s = 0;
    if (!read14(&st, &r, &l, &s)) {
      lcdRow1("Wire check A4A5");
      continue;
    }
    if (st == 0) {
      lcdRow1("Lane idle      ");
    } else if (st == 1) {
      lcdRow1("At start line  ");
    } else if (st == 2) {
      lcdRow1("At finish line ");
    } else if (st == 3) {
      reaction_ms = r;
      lap_ms = l;
      speed_mm_s = s;
      beepOk();
      drawLastRun();
      return;
    }
    lcdRow0("Race on        ");
  }
  zeros();
  beepBad();
  lcd.clear();
  lcdRow0("No finish      ");
  lcdRow1("Spd 0 Lap 0    ");
  delay(2000);
  drawHome();
}

static void runRace() {
  countdown();
  uint32_t go = millis();
  if (!sendGo(go)) {
    zeros();
    beepBad();
    lcd.clear();
    lcdRow0("Lane1 offline  ");
    lcdRow1("Spd 0 Lap 0    ");
    delay(2000);
    drawHome();
    return;
  }
  waitLane1();
}

static void debounceRace() {
  int r = digitalRead(btnRace);
  if (r != lst1) db1 = millis();
  if (millis() - db1 > 50U) {
    if (r != st1) {
      st1 = r;
      if (st1 == LOW) runRace();
    }
  }
  lst1 = r;
}

static void debounceLast() {
  int r = digitalRead(btnLast);
  if (r != lst2) db2 = millis();
  if (millis() - db2 > 50U) {
    if (r != st2) {
      st2 = r;
      if (st2 == LOW) {
        if (speed_mm_s == 0 && lap_ms == 0) {
          lcd.clear();
          lcdRow0("No run yet     ");
          lcdRow1("Spd 0 Lap 0    ");
          delay(1200);
          drawHome();
        } else {
          drawLastRun();
          delay(2000);
          drawHome();
        }
      }
    }
  }
  lst2 = r;
}

void setup() {
  lcd.begin(16, 2);
  Wire.begin();
  zeros();

  pinMode(btnRace, INPUT_PULLUP);
  pinMode(btnLast, INPUT_PULLUP);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  pinMode(buzzer, OUTPUT);
  noTone(buzzer);
  allLedsOff();

  lcd.clear();
  lcdRow0("CS362 lane 1   ");
  lcdRow1(i2cPing(LANE1) ? "Lane1 OK       " : "No lane1 I2C   ");
  delay(1600);
  drawHome();
}

void loop() {
  debounceRace();
  debounceLast();
}
