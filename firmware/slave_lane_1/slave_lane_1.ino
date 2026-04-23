/*
 * Lane 1 slave — IR start/finish + LCD + I2C 0x08.
 * Speed on wire stays mm/s; LCD shows mph. No delay().
 */

#include <Wire.h>
#include <LiquidCrystal.h>
#include <stdio.h>
#include <string.h>

static const uint8_t I2C_ADDR = 0x08;

const int rs = 4, en = 5, d4 = 6, d5 = 7, d6 = 8, d7 = 9;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int startIR = 2;
const int finishIR = 12;

static const uint32_t DIST_MM = 450UL;

enum : uint8_t {
  ST_IDLE = 0,
  ST_ARMED = 1,
  ST_TO_FINISH = 2,
  ST_DONE = 3
};

static volatile uint8_t state = ST_IDLE;
static volatile uint32_t t_race_start_slave = 0;
static volatile uint8_t pendingArmLcd = 0;

static uint32_t reaction_ms = 0;
static uint32_t lap_ms = 0;
static uint32_t speed_mm_s = 0;

static uint32_t enteredFinishWaitMs = 0;

static uint8_t txPacket[14];

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

static void lcdLine(uint8_t row, char* buf) {
  pad16(buf);
  lcd.setCursor(0, row);
  lcd.print(buf);
}

static void lcdTwo(const char* r0, const char* r1) {
  char a[17], b[17];
  snprintf(a, sizeof a, "%.16s", r0);
  snprintf(b, sizeof b, "%.16s", r1);
  pad16(a);
  pad16(b);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(a);
  lcd.setCursor(0, 1);
  lcd.print(b);
}

static void buildTxPacket() {
  txPacket[0] = state;
  txPacket[1] = (uint8_t)(reaction_ms & 0xFF);
  txPacket[2] = (uint8_t)((reaction_ms >> 8) & 0xFF);
  txPacket[3] = (uint8_t)((reaction_ms >> 16) & 0xFF);
  txPacket[4] = (uint8_t)((reaction_ms >> 24) & 0xFF);
  txPacket[5] = (uint8_t)(lap_ms & 0xFF);
  txPacket[6] = (uint8_t)((lap_ms >> 8) & 0xFF);
  txPacket[7] = (uint8_t)((lap_ms >> 16) & 0xFF);
  txPacket[8] = (uint8_t)((lap_ms >> 24) & 0xFF);
  txPacket[9] = (uint8_t)(speed_mm_s & 0xFF);
  txPacket[10] = (uint8_t)((speed_mm_s >> 8) & 0xFF);
  txPacket[11] = (uint8_t)((speed_mm_s >> 16) & 0xFF);
  txPacket[12] = (uint8_t)((speed_mm_s >> 24) & 0xFF);
  txPacket[13] = 0;
}

static void onReceive(int numBytes) {
  if (numBytes < 5) return;
  uint8_t cmd = Wire.read();
  if (cmd != 's' && cmd != 'S') return;
  (void)Wire.read();
  (void)Wire.read();
  (void)Wire.read();
  (void)Wire.read();

  t_race_start_slave = millis();
  enteredFinishWaitMs = 0;

  reaction_ms = 0;
  lap_ms = 0;
  speed_mm_s = 0;
  state = ST_ARMED;
  buildTxPacket();
  pendingArmLcd = 1;
}

static void onRequest() {
  buildTxPacket();
  Wire.write(txPacket, sizeof(txPacket));
}

static void showDoneLcd() {
  char a[17], b[17];
  uint32_t mph10 = mmS_to_mph_tenths(speed_mm_s);
  snprintf(a, sizeof a, "Spd %lu.%lu mph",
           (unsigned long)(mph10 / 10u), (unsigned long)(mph10 % 10u));
  snprintf(b, sizeof b, "Lap %lu ms", (unsigned long)lap_ms);
  lcd.clear();
  lcdLine(0, a);
  lcdLine(1, b);
}

void setup() {
  lcd.begin(16, 2);
  pinMode(startIR, INPUT_PULLUP);
  pinMode(finishIR, INPUT_PULLUP);

  Wire.begin(I2C_ADDR);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);

  lcdTwo("Lane 1 I2C 0x08", "Spd 0.0 mph    ");
  buildTxPacket();
}

void loop() {
  static uint32_t lastUi = 0;
  static uint32_t armedAt = 0;

  if (pendingArmLcd) {
    pendingArmLcd = 0;
    lcdTwo("Start: pin D2  ", "Spd 0.0 mph    ");
  }

  if (state == ST_ARMED) {
    if (armedAt == 0) armedAt = millis();
    if (millis() - armedAt > 120000UL) {
      state = ST_IDLE;
      armedAt = 0;
      reaction_ms = lap_ms = speed_mm_s = 0;
      buildTxPacket();
      lcdTwo("Timed out       ", "Spd 0.0 mph    ");
    } else if (digitalRead(startIR) == LOW) {
      uint32_t t_start = millis();
      reaction_ms = (t_start >= t_race_start_slave) ? (t_start - t_race_start_slave) : 0;
      state = ST_TO_FINISH;
      armedAt = 0;
      enteredFinishWaitMs = millis();
      lcdTwo("Finish: pin D12", "break beam     ");
    }
  } else if (state == ST_TO_FINISH) {
    if (digitalRead(finishIR) == LOW) {
      uint32_t t_fin = millis();
      lap_ms = (t_fin > t_race_start_slave) ? (t_fin - t_race_start_slave) : 1;
      if (lap_ms == 0) lap_ms = 1;
      speed_mm_s = (DIST_MM * 1000UL) / lap_ms;
      state = ST_DONE;
      enteredFinishWaitMs = 0;
      showDoneLcd();
    } else if (enteredFinishWaitMs != 0 && (millis() - enteredFinishWaitMs > 120000UL)) {
      state = ST_IDLE;
      enteredFinishWaitMs = 0;
      reaction_ms = lap_ms = speed_mm_s = 0;
      buildTxPacket();
      lcdTwo("Timed out       ", "Spd 0.0 mph    ");
    }
  }

  if (state == ST_DONE && millis() - lastUi > 400) {
    lastUi = millis();
    showDoneLcd();
  }

  if (state == ST_ARMED && millis() - lastUi > 250) {
    lastUi = millis();
    lcdTwo("Start: pin D2  ", "Spd 0.0 mph    ");
  }
}
