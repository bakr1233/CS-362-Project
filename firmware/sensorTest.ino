//This is the code for the sensor stuff

#include <Wire.h>
#include <LiquidCrystal.h>

//Pins for LCD
const int rs = 4, en = 5, d4 = 6, d5 = 7, d6 = 8, d7 = 9;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

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

uint8_t packet[9];

void buildPacket(){
  // 1 state + 4 time + 4 speed + 7 padding

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

}

void sendData(){
  //send data when it's ready
  buildPacket();
  Wire.write(packet, sizeof(packet));
}

void printData(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Speed: ");
  lcd.print(speed);
  
  lcd.setCursor(0, 1);
  lcd.print("Time: ");
  lcd.print(objectTime);

  //Put the time the car finished in the clock
}

void setup() {
  lcd.begin(16, 2); //Setup LCD

  pinMode(startIR, INPUT);
  pinMode(finishIR, INPUT);

  Wire.begin(4);
  Wire.onReceive(getSignal);
  Wire.onRequest(sendData);
}

void loop() {

  if(state == ready){
    //gotta add idle timeout
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ready to race");

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
        speed = (distance * 1000) / objectTime;
      }

      printData();

    }
  }

}
