/*
 * CS-362 — Master, lane 1 only (I2C 0x08).
 * No delay(): timing uses millis() + a small state machine.
 */

#include <Wire.h>
#include <LiquidCrystal.h>
#include <stdio.h>
#include <string.h>

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

enum : uint8_t {
  PH_BOOT,
  PH_HOME,
  PH_COUNTDOWN,
  PH_RACE_POLL,
  PH_HOLD
};

static uint8_t phase = PH_BOOT;
static uint32_t phaseMarkMs = 0;
static uint32_t holdUntilMs = 0;
static uint8_t cdStep = 0;
static uint32_t raceDeadlineMs = 0;
static uint32_t lastPollMs = 0;
static uint32_t mmS_to_mph_tenths(uint32_t mm_s) {
  if (mm_s == 0) return 0;
  return (uint32_t)(((uint64_t)mm_s * 223694ULL + 5000000ULL) / 10000000ULL);
}

static void pad16(char* s) {
  size_t n = strlen(s);
  if (n > 16) n = 16;
  s[n] = '\0';
  while (n < 16) s[n++] = ' ';
  s[16] = '\0';
}

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
  char a[17], b[17];
  zeros();
  strcpy(a, "Spd 0.0 mph");
  strcpy(b, "Lap 0 ms");
  pad16(a);
  pad16(b);
  lcd.clear();
  lcdRow0(a);
  lcdRow1(b);
}

static void drawLastRun() {
  char a[17], b[17];
  uint32_t mph10 = mmS_to_mph_tenths(speed_mm_s);
  snprintf(a, sizeof a, "Spd %lu.%lu mph",
           (unsigned long)(mph10 / 10u), (unsigned long)(mph10 % 10u));
  snprintf(b, sizeof b, "Lap %lu ms", (unsigned long)lap_ms);
  pad16(a);
  pad16(b);
  lcd.clear();
  lcdRow0(a);
  lcdRow1(b);
}

static void allLedsOff() {
  digitalWrite(led1, LOW);
  digitalWrite(led2, LOW);
  digitalWrite(led3, LOW);
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

static void beginHold(uint32_t holdMs) {
  holdUntilMs = millis() + holdMs;
  phase = PH_HOLD;
}

static bool tickCountdown() {
  uint32_t dt = millis() - phaseMarkMs;
  switch (cdStep) {
    case 0:
      digitalWrite(led1, HIGH);
      if (dt >= 800) {
        digitalWrite(led1, LOW);
        digitalWrite(led2, HIGH);
        tone(buzzer, 880, 120);
        cdStep = 1;
        phaseMarkMs = millis();
      }
      break;
    case 1:
      if (dt >= 800) {
        digitalWrite(led2, LOW);
        digitalWrite(led3, HIGH);
        tone(buzzer, 990, 120);
        cdStep = 2;
        phaseMarkMs = millis();
      }
      break;
    case 2:
      if (dt >= 800) {
        digitalWrite(led3, LOW);
        digitalWrite(led1, HIGH);
        digitalWrite(led2, HIGH);
        digitalWrite(led3, HIGH);
        tone(buzzer, 1320, 200);
        cdStep = 3;
        phaseMarkMs = millis();
      }
      break;
    case 3:
      if (dt >= 450) {
        noTone(buzzer);
        allLedsOff();
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}

static void startRaceFlow() {
  noTone(buzzer);
  allLedsOff();
  phase = PH_COUNTDOWN;
  cdStep = 0;
  phaseMarkMs = millis();
}

static void tickRacePoll() {
  uint32_t now = millis();
  if ((uint32_t)(now - lastPollMs) < 45U) return;
  lastPollMs = now;

  uint8_t st = 0;
  uint32_t r = 0, l = 0, s = 0;
  if (!read14(&st, &r, &l, &s)) {
    lcdRow1("Wire check A4A5");
    return;
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
    tone(buzzer, 1568, 90);
    drawLastRun();
    phase = PH_HOME;
    return;
  }
  lcdRow0("Race on        ");

  if ((uint32_t)(now - raceDeadlineMs) >= 120000UL) {
    zeros();
    tone(buzzer, 220, 200);
    lcd.clear();
    lcdRow0("No finish      ");
    lcdRow1("Spd 0.0 mph    ");
    beginHold(2000);
  }
}

static void tickMaster() {
  uint32_t now = millis();

  if (phase == PH_BOOT) {
    if ((uint32_t)(now - phaseMarkMs) >= 1600U) {
      drawHome();
      phase = PH_HOME;
    }
    return;
  }

  if (phase == PH_HOLD) {
    if ((int32_t)(now - holdUntilMs) >= 0) {
      drawHome();
      phase = PH_HOME;
    }
    return;
  }

  if (phase == PH_COUNTDOWN) {
    if (tickCountdown()) {
      uint32_t go = millis();
      if (!sendGo(go)) {
        zeros();
        tone(buzzer, 220, 200);
        lcd.clear();
        lcdRow0("Lane1 offline  ");
        lcdRow1("Spd 0.0 mph    ");
        beginHold(2000);
        return;
      }
      zeros();
      lcd.clear();
      lcdRow0("Race on        ");
      lcdRow1("Spd 0.0 mph    ");
      raceDeadlineMs = millis();
      lastPollMs = millis() - 50UL;
      phase = PH_RACE_POLL;
    }
    return;
  }

  if (phase == PH_RACE_POLL) {
    tickRacePoll();
  }
}

static void debounceRace() {
  if (phase != PH_HOME) return;
  int r = digitalRead(btnRace);
  if (r != lst1) db1 = millis();
  if (millis() - db1 > 50U) {
    if (r != st1) {
      st1 = r;
      if (st1 == LOW) startRaceFlow();
    }
  }
  lst1 = r;
}

static void debounceLast() {
  if (phase != PH_HOME) return;
  int r = digitalRead(btnLast);
  if (r != lst2) db2 = millis();
  if (millis() - db2 > 50U) {
    if (r != st2) {
      st2 = r;
      if (st2 == LOW) {
        if (speed_mm_s == 0 && lap_ms == 0) {
          lcd.clear();
          lcdRow0("No run yet     ");
          lcdRow1("Spd 0.0 mph    ");
          beginHold(1200);
        } else {
          drawLastRun();
          beginHold(2000);
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
  phase = PH_BOOT;
  phaseMarkMs = millis();
}

void loop() {
  tickMaster();
  debounceRace();
  debounceLast();
}
