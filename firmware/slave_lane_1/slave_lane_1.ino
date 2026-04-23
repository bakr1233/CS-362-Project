/*
 * Lane 1 slave — IR start/finish + parallel LCD + I2C (0x08).
 *
 * IMPORTANT: master millis() and slave millis() are NOT the same clock.
 * All lap/reaction math uses t_race_start_slave = millis() on this board when GO is received.
 *
 * Pins: LCD 4-9, start D2, finish D12 (beam broken = LOW + INPUT_PULLUP), I2C A4/A5.
 */

#include <Wire.h>
#include <LiquidCrystal.h>

static const uint8_t I2C_ADDR = 0x08;

const int rs = 4, en = 5, d4 = 6, d5 = 7, d6 = 8, d7 = 9;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int startIR = 2;
const int finishIR = 12;

static const uint32_t DIST_MM = 1900UL;

enum : uint8_t {
  ST_IDLE = 0,
  ST_ARMED = 1,
  ST_TO_FINISH = 2,
  ST_DONE = 3
};

static volatile uint8_t state = ST_IDLE;
static volatile uint32_t t_race_start_slave = 0;
static volatile uint32_t t_master_echo = 0;
static volatile uint8_t pendingArmLcd = 0;

static uint32_t reaction_ms = 0;
static uint32_t lap_ms = 0;
static uint32_t speed_mm_s = 0;

static uint32_t enteredFinishWaitMs = 0;

static uint8_t txPacket[14];

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
  uint32_t t = (uint32_t)Wire.read();
  t |= (uint32_t)Wire.read() << 8;
  t |= (uint32_t)Wire.read() << 16;
  t |= (uint32_t)Wire.read() << 24;

  t_master_echo = t;
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
  lcd.setCursor(0, 0);
  lcd.print("Lap:");
  lcd.print(lap_ms);
  lcd.print("ms    ");
  lcd.setCursor(0, 1);
  lcd.print("V:");
  lcd.print(speed_mm_s);
  lcd.print("mm/s ");
}

void setup() {
  lcd.begin(16, 2);
  pinMode(startIR, INPUT_PULLUP);
  pinMode(finishIR, INPUT_PULLUP);

  Wire.begin(I2C_ADDR);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);

  lcd.clear();
  lcd.print("Lane1 addr 0x08");
  lcd.setCursor(0, 1);
  lcd.print("press RACE mstr ");
  buildTxPacket();
}

void loop() {
  static uint32_t lastUi = 0;
  static uint32_t armedAt = 0;

  if (pendingArmLcd) {
    pendingArmLcd = 0;
    lcd.clear();
    lcd.print("GO: brk START");
    lcd.setCursor(0, 1);
    lcd.print("D2 LOW=break    ");
  }

  if (state == ST_ARMED) {
    if (armedAt == 0) armedAt = millis();
    if (millis() - armedAt > 120000UL) {
      state = ST_IDLE;
      armedAt = 0;
      lcd.clear();
      lcd.print("timeout idle");
    } else if (digitalRead(startIR) == LOW) {
      uint32_t t_start = millis();
      reaction_ms = (t_start >= t_race_start_slave) ? (t_start - t_race_start_slave) : 0;
      state = ST_TO_FINISH;
      armedAt = 0;
      enteredFinishWaitMs = millis();
      lcd.clear();
      lcd.print("brk FINISH D12");
      lcd.setCursor(0, 1);
      lcd.print("LOW=break       ");
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
      lcd.clear();
      lcd.print("timeout idle");
    }
  }

  if (state == ST_DONE && millis() - lastUi > 400) {
    lastUi = millis();
    showDoneLcd();
  }

  if (state == ST_ARMED && millis() - lastUi > 250) {
    lastUi = millis();
    lcd.setCursor(0, 0);
    lcd.print("Break START IR");
    lcd.setCursor(0, 1);
    lcd.print("t0m=");
    lcd.print(t_master_echo % 100000);
    lcd.print("    ");
  }

  delay(2);
}
