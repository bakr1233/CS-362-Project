/*
 * Master test — parallel LCD, LEDs 2-3-4, buzzer A0, buttons 11-12 (INPUT_PULLUP, LOW=pressed).
 * Talks to lane slave at I2C 0x08 (use 8 in Wire.beginTransmission / requestFrom).
 *
 * Protocol matches slave_lane_1.ino: send 's' + uint32_t t0_ms (LE), then poll 14-byte status packet.
 */

#include <Wire.h>
#include <LiquidCrystal.h>

static const uint8_t SLAVE_ADDR = 0x08;

const int rs = 5, en = 6, d4 = 7, d5 = 8, d6 = 9, d7 = 10;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int led1 = 2;
const int led2 = 3;
const int led3 = 4;
const int buzzer = A0;

const int button1 = 11;
const int button2 = 12;

uint32_t reaction_ms = 0;
uint32_t lap_ms = 0;
uint32_t speed_mm_s = 0;

unsigned long lastDebounceTime1 = 0;
unsigned long debounceDelay1 = 50;
int button1State = HIGH;
int lastButton1State = HIGH;

unsigned long lastDebounceTime2 = 0;
unsigned long debounceDelay2 = 50;
int button2State = HIGH;
int lastButton2State = HIGH;

static void allLedsOff() {
  digitalWrite(led1, LOW);
  digitalWrite(led2, LOW);
  digitalWrite(led3, LOW);
}

static void countdown() {
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
  for (int i = 0; i < 3; i++) {
    digitalWrite(led1 + i, HIGH);
  }
  tone(buzzer, 1320, 200);
  delay(450);
  allLedsOff();
}

static void sendRaceStart(uint32_t t0) {
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write((uint8_t)'s');
  Wire.write((uint8_t)(t0 & 0xFF));
  Wire.write((uint8_t)((t0 >> 8) & 0xFF));
  Wire.write((uint8_t)((t0 >> 16) & 0xFF));
  Wire.write((uint8_t)((t0 >> 24) & 0xFF));
  uint8_t err = Wire.endTransmission();
  if (err != 0) {
    lcd.clear();
    lcd.print("I2C tx err ");
    lcd.print(err);
    delay(2000);
  }
}

static bool readSlavePacket(uint8_t* statusOut, uint32_t* reactOut, uint32_t* lapOut, uint32_t* spdOut) {
  const uint8_t N = 14;
  uint8_t got = Wire.requestFrom((int)SLAVE_ADDR, (int)N);
  if (got < N) return false;
  uint8_t st = Wire.read();
  uint32_t r = (uint32_t)Wire.read();
  r |= (uint32_t)Wire.read() << 8;
  r |= (uint32_t)Wire.read() << 16;
  r |= (uint32_t)Wire.read() << 24;
  uint32_t l = (uint32_t)Wire.read();
  l |= (uint32_t)Wire.read() << 8;
  l |= (uint32_t)Wire.read() << 16;
  l |= (uint32_t)Wire.read() << 24;
  uint32_t s = (uint32_t)Wire.read();
  s |= (uint32_t)Wire.read() << 8;
  s |= (uint32_t)Wire.read() << 16;
  s |= (uint32_t)Wire.read() << 24;
  (void)Wire.read();
  *statusOut = st;
  *reactOut = r;
  *lapOut = l;
  *spdOut = s;
  return true;
}

static void getDataBlocking() {
  uint32_t t0 = millis();
  reaction_ms = lap_ms = speed_mm_s = 0;
  lcd.clear();
  lcd.print("Waiting slave..");

  while (millis() - t0 < 120000UL) {
    delay(40);
    uint8_t st = 0;
    uint32_t r = 0, l = 0, s = 0;
    if (!readSlavePacket(&st, &r, &l, &s)) continue;
    if (st == 3) {
      reaction_ms = r;
      lap_ms = l;
      speed_mm_s = s;
      return;
    }
  }
  lcd.clear();
  lcd.print("Slave timeout");
}

static void showResults() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("mm/s:");
  lcd.print(speed_mm_s);
  lcd.print(" R:");
  lcd.print(reaction_ms);

  lcd.setCursor(0, 1);
  lcd.print("lap ms:");
  lcd.print(lap_ms);
  lcd.print("      ");
}

void race() {
  countdown();

  uint32_t signalTime = millis();
  sendRaceStart(signalTime);

  getDataBlocking();
  showResults();
}

void leaderboard() {
  lcd.clear();
  lcd.print("LB: last speed");
  lcd.setCursor(0, 1);
  lcd.print(speed_mm_s);
  lcd.print(" mm/s");
  delay(1500);
}

void button1Check() {
  int reading = digitalRead(button1);
  if (reading != lastButton1State) lastDebounceTime1 = millis();
  if ((millis() - lastDebounceTime1) > debounceDelay1) {
    if (reading != button1State) {
      button1State = reading;
      if (button1State == LOW) race();
    }
  }
  lastButton1State = reading;
}

void button2Check() {
  int reading = digitalRead(button2);
  if (reading != lastButton2State) lastDebounceTime2 = millis();
  if ((millis() - lastDebounceTime2) > debounceDelay2) {
    if (reading != button2State) {
      button2State = reading;
      if (button2State == LOW) leaderboard();
    }
  }
  lastButton2State = reading;
}

void setup() {
  lcd.begin(16, 2);
  Wire.begin();

  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);

  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  pinMode(buzzer, OUTPUT);
  allLedsOff();

  lcd.clear();
  lcd.print("mainTest master");
  lcd.setCursor(0, 1);
  lcd.print("btn11=race 12=LB");
}

void loop() {
  button1Check();
  button2Check();
}
