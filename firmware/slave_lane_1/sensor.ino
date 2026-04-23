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

char signalData[4];
unsigned long signalTime = 0;

unsigned long reactionTime = 0;

unsigned long objectTime = 0;

int distance = 19; //MUST CHANGE

float speed = 0; 


void getSignal(){
  while(Wire.available()){
    char c = Wire.read();
    if(c == 's'){
      raceSignal = true;
    }
    else{
      if(Wire.available() >= sizeof(unsigned long)){
        Wire.readBytes((byte*)&signalTime, sizeof(unsigned long));
      }
    }
  }
}

void sendData(){
  //send data when it's ready
  if(dataReady){
    Wire.write((byte*)&speed, sizeof(speed));
    Wire.write((byte*)&objectTime, sizeof(objectTime));

    dataReady = false;
  }
}

void printData(){
  //print stuff on LCD

  char speedBuf[16];
  sprintf(speedBuf, "Speed: %f", speed);
  lcd.setCursor(0, 0);
  lcd.clear();
  lcd.print(speedBuf);

  char reactionBuf[16];
  sprintf(reactionBuf, "Reaction: %f", reactionTime);
  lcd.setCursor(0, 1);
  lcd.print(reactionBuf);

  //Put the time the car finished in the clock
}


void setup() {
  lcd.begin(16, 2); //Setup LCD

  Wire.begin(4);
  Wire.onReceive(getSignal);
  Wire.onRequest(sendData);
}

void loop() {
  if(raceSignal){

    lcd.setCursor(0, 0);
    lcd.print("Got signal");

    while(digitalRead(startIR) == HIGH);
    unsigned long startTime = millis(); //The time when the car starts to go

    while(digitalRead(finishIR) == HIGH);
    unsigned long finishTime = millis();
    reactionTime = startTime - signalTime;

    objectTime = finishTime - signalTime; //The time it took the object in total
    speed = distance / (objectTime / 1000);

    dataReady = true;

    printData();
    raceSignal = false;
  }
}

