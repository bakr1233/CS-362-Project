/*
  David Flores (dflor54), Bakr Bouhaya (bbouh), Levell Kensey (lkens)
  Who's The Fastest?
  
  Abstract:
  This project uses multiple Arduinos to implement a racing system. 
  There will be a central control center Arduino that controls the whole race, as well as other Arduinos that sense the objects being raced. 
  The Arduinos will use a wired connection to ensure fast, accurate communication. 
  The main component of the project, aside from the race, is calculating speeds, which will be ranked. As well as a reaction time ranking.

*/

#include <Wire.h> //For I2C communication
#include <LiquidCrystal.h> //For LCD
#include <EEPROM.h> //For leaderboard memory storage

//Pins for LCD
const int rs = 5, en = 6, d4 = 7, d5 = 8, d6 = 9, d7 = 10;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Pins for LEDs and buzzer
const int red = 2;
const int yellow = 3;
const int green = 4;
const int buzzer = A0;

//Variables for car speed and time
long speed1 = 0;
unsigned long carTime1 = 0;

long speed2 = 0;
unsigned long carTime2 = 0;

//these are useless
// unsigned long transLastTime = 0;
// unsigned long countdownLastTime = 0;

//Variables for leaderboard
const int boardSize = 5;
const int EEPROM_START_ADDR = 0; // Where in memory to start saving

struct Leaderboard{
  long speed;
};


//--------------------------Race Signal Button------------------------------
int button1 = 11; //The pin to the button

int button1State;            // the current reading from the button
int lastButton1State = LOW;  // the previous reading from the button


//These variables are used for the debouncing the button
unsigned long lastDebounceTime1 = 0;  // the last time the button was toggled
unsigned long debounceDelay1 = 50;    // the debounce time; increase if the output flickers

void button1Check(){
   int reading = digitalRead(button1); // read the state of the button

  // If the state changed at all
  if (reading != lastButton1State) {
    // reset the debouncing timer
    lastDebounceTime1 = millis();
  }

  //If it's been longer than the debounce state, then take the current state of the button
  if ((millis() - lastDebounceTime1) > debounceDelay1) {

    // if the button state has changed:
    if (reading != button1State) {
      button1State = reading;
      //If the button is pressed, reset the timer
      if(button1State == HIGH){
        race();
      }
      else{
        return;
      }
    }
  }
  lastButton1State = reading; //Record the last state of the button
}


// -----------------Button 2----------------
int button2 = 12; //The pin to the button

int button2State;            // the current reading from the button
int lastButton2State = LOW;  // the previous reading from the button


//These variables are used for the debouncing the button
unsigned long lastDebounceTime2 = 0;  // the last time the button was toggled
unsigned long debounceDelay2 = 50;    // the debounce time; increase if the output flickers

void button2Check(){
   int reading = digitalRead(button2); // read the state of the button

  // If the state changed at all
  if (reading != lastButton2State) {
    // reset the debouncing timer
    lastDebounceTime2 = millis();
  }

  //If it's been longer than the debounce state, then take the current state of the button
  if ((millis() - lastDebounceTime2) > debounceDelay2) {

    // if the button state has changed:
    if (reading != button2State) {
      button2State = reading;
      //If the button is pressed, reset the timer
      if(button2State == HIGH){
        leaderboard();
        lcd.clear();
        lcd.print("Racing time!!   ");
      }
      else{
        return;
      }
    }
  }
  lastButton2State = reading; //Record the last state of the button
}


bool getData(int address, int* state, unsigned long* time, unsigned long* speed){
  byte bytesToRequest = 9;
  Wire.requestFrom(address, (int)bytesToRequest);
  
  if (Wire.available() < bytesToRequest) return false;

  //State read
  *state = Wire.read();

   // Read lap time (4 bytes)
  *time = Wire.read();
  *time |= (unsigned long)Wire.read() << 8;
  *time |= (unsigned long)Wire.read() << 16;
  *time |= (unsigned long)Wire.read() << 24;
  
  // Read speed (4 bytes)
  *speed = Wire.read();
  *speed |= (unsigned long)Wire.read() << 8;
  *speed |= (unsigned long)Wire.read() << 16;
  *speed |= (unsigned long)Wire.read() << 24;

  return true;
}

void race(){
  countdown();
  ledsOff();
  // noTone(buzzer);

  Wire.beginTransmission(4);
  Wire.write('s');
  int error1 = Wire.endTransmission();
  if (error1 != 0) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("L1 signal fail");
    return;
  }

  Wire.beginTransmission(5);
  Wire.write('s');
  int error2 = Wire.endTransmission();
  if (error2 != 0) {
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("L2 signal fail");
    return;
  }

  int car1State = 0; 
  int car2State = 0;
  bool raceDone = false;

  unsigned long pollingLastTime = 0;
  while(!raceDone){
    if(millis() - pollingLastTime > 75){

      bool lane1True = getData(4, &car1State, &carTime1, &speed1);
      bool lane2True = getData(5, &car2State, &carTime2, &speed2);

      if(lane1True && lane2True){
        lcd.clear();
        /*-----Lane 1------*/
        lcd.setCursor(0,0);
        if(car1State == 1){
          lcd.print("L1: ready       ");
        }
        else if(car1State == 2){
          lcd.print("L1: Car racing  ");
        }
        else if(car1State == 3){
          lcd.print("L1: ");
          unsigned long seconds = carTime1 / 1000;
          unsigned long milliseconds = carTime1 % 1000;
          lcd.print(seconds);
          lcd.print(".");
          lcd.print(milliseconds);
          lcd.println(" s   ");
        }

        /*-------Lane 2-----------*/
        lcd.setCursor(0, 1);
        if(car2State == 1){
          lcd.print("L2: ready       ");
        }
        else if(car2State == 2){
          lcd.print("L1: Car racing ");
        }
        else if(car2State == 3){
          lcd.print("L2: ");
          unsigned long seconds = carTime2 / 1000;
          unsigned long milliseconds = carTime2 % 1000;
          lcd.print(seconds);
          lcd.print(".");
          lcd.print(milliseconds);
          lcd.println(" s   ");
        }

        /*------Finish Condition--------------*/
        if(car1State == 3 && car2State == 3){
          raceDone = true;
          tone(buzzer, 1568, 90);
        }

      }

      pollingLastTime = millis();
    }
   
  }

  winner();
  
}

void winner(){
  lcd.clear();
  lcd.setCursor(0, 0);
  if (carTime1 < carTime2) {
    lcd.print("WINNER: LANE 1!");
  } else if (carTime1 > carTime2) {
    lcd.print("WINNER: LANE 2!");
  } else {
    lcd.print("ITS A TIE!");
  }

  unsigned long lastTime = 0;
  bool timerDone = false;
  int timeState = 0;
  while(!timerDone){
    if(millis() - lastTime > 1000){
      switch(timeState){
        case 0:
          timeState++;
          break;
        case 1:
          timeState++;
          break;
        case 2:
          timeState++;
          break;
        case 3:
          timerDone = true;
          break;
        default:
          return;
      }
      lastTime = millis();
    }
  }

  updateLeaderboard(speed1);
  updateLeaderboard(speed2);
}

void countdown(){
  //do countdown stuff
  unsigned long countLastTime = 0;
  int ledState = 0;
  while(true){
    if(millis() - countLastTime > 1000){
      countLastTime = millis();
      switch(ledState){
        case 0:
          digitalWrite(red, HIGH);
          digitalWrite(yellow, LOW);
          digitalWrite(green, LOW);
          tone(buzzer, 770, 120);
          //buzzer noise
          ledState++;
          break;
        case 1:
          digitalWrite(red, LOW);
          digitalWrite(yellow, HIGH);
          digitalWrite(green, LOW);
          tone(buzzer, 880, 120);
          ledState++;
          break;
        case 2:
          digitalWrite(red, LOW);
          digitalWrite(yellow, LOW);
          digitalWrite(green, HIGH);
          tone(buzzer, 990, 120);
          ledState++;
          break;
        case 3:
          digitalWrite(red, HIGH);
          digitalWrite(yellow, HIGH);
          digitalWrite(green, HIGH);
          tone(buzzer, 1320, 200);
          ledState++;
          break;
        case 4:
          return;
        default:
          return;
      }
    }
  }
}

void ledsOff(){
  digitalWrite(red, LOW);
  digitalWrite(yellow, LOW);
  digitalWrite(green, LOW);
}

void updateLeaderboard(long newSpeed) {
  if (newSpeed <= 0) return; // Ignore zero or negative

  Leaderboard leaderboard[boardSize]; // Ensure boardSize is defined globally

  // 1. Read from EEPROM
  for (int i = 0; i < boardSize; i++) {
    EEPROM.get(EEPROM_START_ADDR + (i * sizeof(Leaderboard)), leaderboard[i]);
    if (leaderboard[i].speed == 0xFFFFFFFF) {
      leaderboard[i].speed = 0;
    }
  }

  // 2. Find the spot and shift
  for (int i = 0; i < boardSize; i++) {
    if (newSpeed > leaderboard[i].speed) {
      
      // Shift lower ranks down
      for (int j = boardSize - 1; j > i; j--) {
        leaderboard[j] = leaderboard[j - 1];
      }
      
      // Insert new record
      leaderboard[i].speed = newSpeed;
      
      // 3. Save the updated board back to EEPROM
      for (int k = 0; k < boardSize; k++) {
        // Corrected variable name to 'leaderboard'
        EEPROM.put(EEPROM_START_ADDR + (k * sizeof(Leaderboard)), leaderboard[k]);
      }
      break; // Exit once the update is done
    }
  }
}

void leaderboard(){
  lcd.clear();
  lcd.print("--Leaderboard-- ");

  unsigned long waitLastTime = millis();
  bool waitDone = false;

  while(!waitDone){
    if(millis() - waitLastTime > 1000){
      waitLastTime = millis();
      waitDone = true;
    }
  }


  for(int i = 0; i < boardSize; i++){
    Leaderboard x;
    EEPROM.get(EEPROM_START_ADDR + (i * sizeof(Leaderboard)), x);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Rank ");
    lcd.print(i + 1);

    lcd.setCursor(0, 1);
    // If the speed is 0 or factory default
    if (x.speed == 0 || x.speed == 0xFFFFFFFF) {
      lcd.print("---");
    } 
    else {
      lcd.print(x.speed / 100);
      lcd.print(".");
      int decimals = x.speed % 100;
      if (decimals < 10){
        lcd.print("0");
      } 
      lcd.print(decimals);
      lcd.print(" MPH    ");
    }

    unsigned long delayLastTime = millis();
    bool delayDone = false;

    while(!delayDone){
      if(millis() - delayLastTime > 2000){
        delayLastTime = millis();
        delayDone = true;
      }
    }
  }
  
  lcd.clear();
  lcd.print("Racing time!!   ");

}

void setup(){
  lcd.begin(16, 2); //Setup LCD

  Wire.begin();

  pinMode(button1, INPUT);
  //pinMode(button2, INPUT);

  pinMode(red, OUTPUT);
  pinMode(yellow, OUTPUT);
  pinMode(green, OUTPUT);

  pinMode(buzzer, OUTPUT);

  lcd.print("Racing time!!   ");
  
}

void loop(){
  button1Check();
  button2Check();

}