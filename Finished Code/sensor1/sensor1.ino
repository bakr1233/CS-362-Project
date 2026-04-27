/*
  David Flores (dflor54), Bakr Bouhaya (bbouh), Levell Kensey (lkens)
  Who's The Fastest?
  
  Abstract:
  This project uses multiple Arduinos to implement a racing system. 
  There will be a central control center Arduino that controls the whole race, as well as other Arduinos that sense the objects being raced. 
  The Arduinos will use a wired connection to ensure fast, accurate communication. 
  The main component of the project, aside from the race, is calculating speeds, which will be ranked. As well as a reaction time ranking.

*/

#include <Wire.h>
#include <LiquidCrystal.h>
#include <TM1637Display.h>

//Pins for LCD
const int rs = 4, en = 5, d4 = 6, d5 = 7, d6 = 8, d7 = 9;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

int CLK = A0;
int DIO = A1;
TM1637Display display(CLK, DIO);

bool raceSignal = false; //Signal to start up the race
bool dataReady = false;

int finishIR = 12;
int startIR = 2;

unsigned long signalTime = 0;

volatile unsigned long reactionTime = 0;

volatile unsigned long objectTime = 0;

int distance = 19; //MUST CHANGE

volatile long speed = 0; 

int idle = 0;
int ready = 1;
int racing = 2;
int done = 3;

int state = idle;

bool lcdUpdate = false;

uint8_t packet[9];

void buildPacket(){
  //State
  packet[0] = state;

  //Time
  packet[1] = (uint8_t)(objectTime & 0xFF);
  packet[2] = (uint8_t)((objectTime >> 8) & 0xFF);
  packet[3] = (uint8_t)((objectTime >> 16) & 0xFF);
  packet[4] = (uint8_t)((objectTime >> 24) & 0xFF);
  
  // Speed (4 bytes)
  packet[5] = (uint8_t)(speed & 0xFF);
  packet[6] = (uint8_t)((speed >> 8) & 0xFF);
  packet[7] = (uint8_t)((speed >> 16) & 0xFF);
  packet[8] = (uint8_t)((speed >> 24) & 0xFF);

}

void getSignal(int bytes){
  if(bytes < 1){
    return;
  }

  char c = Wire.read();
  if(c != 's'){
    return;
  }
  state = ready;
  signalTime = millis();
  lcdUpdate = true;

}

void sendData(){
  //send data when it's ready
  buildPacket();
  Wire.write(packet, sizeof(packet));
}

void printData(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("S: ");
  lcd.print(speed / 100);
  lcd.print(".");
  int decimals = speed % 100;
  if (decimals < 10){
    lcd.print("0");
  } 
  lcd.print(decimals);
  lcd.print(" MPH    ");
  
  lcd.setCursor(0, 1);
  lcd.print("R: ");
  lcd.print(reactionTime / 1000);
  lcd.print(".");
  lcd.print(reactionTime % 1000);
  lcd.println(" seconds   ");

  int totalSeconds = objectTime / 1000;
  int centiSeconds = (objectTime % 1000) / 10;
  int displayValue = (totalSeconds * 100) + centiSeconds;

  display.showNumberDecEx(displayValue, 0x40, true);
}

void setup() {
  lcd.begin(16, 2); //Setup LCD

  pinMode(startIR, INPUT);
  pinMode(finishIR, INPUT);

  Wire.begin(4);
  Wire.onReceive(getSignal);
  Wire.onRequest(sendData);

  display.setBrightness(0x0f);
  display.clear();             

  lcd.setCursor(0, 0);
  lcd.print("Lane 1 is ready ");
}

void loop() {

  if(state == ready){
    //gotta add idle timeout
    if(lcdUpdate && state == ready){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ready to race");
    }
    

    if(digitalRead(startIR) == LOW){
      state = racing;
      unsigned long startTime = millis();
      reactionTime = startTime - signalTime;
      lcd.clear();
      lcd.print("Racing...");
    }
  }
  else if(state == racing){
    //gotta add idle timeout

    if(digitalRead(finishIR) == LOW){
      state = done;
      unsigned long finishTime = millis();
      objectTime = finishTime - signalTime;
      if (objectTime > 0) {
        speed = (distance * 2237L) / objectTime;
      }

      printData();

    }
  }

}
