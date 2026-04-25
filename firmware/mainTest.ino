#include <Wire.h>
#include <LiquidCrystal.h>

const int rs = 5, en = 6, d4 = 7, d5 = 8, d6 = 9, d7 = 10;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Pin definitions
const int red = 2;
const int yellow = 3;
const int green = 4;
const int buzzer = A0;

long speed1 = 1;

unsigned long carTime1 = 0;

unsigned long transLastTime = 0;

unsigned long countdownLastTime = 0;


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


//Button 2
// int button2 = 12; //The pin to the button

// int button2State;            // the current reading from the button
// int lastButton2State = LOW;  // the previous reading from the button


// //These variables are used for the debouncing the button
// unsigned long lastDebounceTime2 = 0;  // the last time the button was toggled
// unsigned long debounceDelay2 = 50;    // the debounce time; increase if the output flickers

// void button2Check(){
//    int reading = digitalRead(button2); // read the state of the button

//   // If the state changed at all
//   if (reading != lastButton2State) {
//     // reset the debouncing timer
//     lastDebounceTime2 = millis();
//   }

//   //If it's been longer than the debounce state, then take the current state of the button
//   if ((millis() - lastDebounceTime2) > debounceDelay2) {

//     // if the button state has changed:
//     if (reading != button2State) {
//       button2State = reading;
//       //If the button is pressed, reset the timer
//       if(button2State == HIGH){
//         leaderboard();
//       }
//       else{
//         return;
//       }
//     }
//   }
//   lastButton2State = reading; //Record the last state of the button
// }


bool getData(int* state, unsigned long* time, unsigned long* speed){
  byte bytesToRequest = 9;
  Wire.requestFrom(4, (int)bytesToRequest);
  
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
  noTone(buzzer);

  Wire.beginTransmission(4);
  Wire.write('s');
  int error1 = Wire.endTransmission();
  if (error1 != 0) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Start signal fail");
    return;
  }


  // Wire.beginTransmission(5);
  // Wire.write('s');
  // int error2 = Wire.endTransmission()
  // if (error2 != 0) {
  //   lcd.clear();
  //   lcd.print("Start signal fail");
  //   return;
  // }

  int car1State = 0; 
  bool raceDone = false;

  unsigned long pollingLastTime = 0;
  while(!raceDone){
    if(millis() - pollingLastTime > 50){

      if(getData(&car1State, &carTime1, &speed1)){
        lcd.clear();
        lcd.setCursor(0,0);
        if(car1State == 1){
          lcd.print("Ready to race");
        }
        else if(car1State == 2){
          lcd.print("Car racing");
        }
        else if(car1State == 3){
          lcd.clear();

          lcd.setCursor(0,0);
          lcd.print("Car speed: ");
          lcd.print(speed1);

          lcd.setCursor(0, 1);
          unsigned long seconds = carTime1 / 1000;
          unsigned long milliseconds = carTime1 % 1000;
          lcd.print(seconds);
          lcd.print(".");
          lcd.print(milliseconds);
          lcd.println(" seconds");

          raceDone = true;
          tone(buzzer, 1568, 90);
        }
      }
      
      pollingLastTime = millis();
    }
   
  }
  
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
          ledState = 0;
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

// void leaderboard(){
//   return;
// }

void setup(){
  lcd.begin(16, 2); //Setup LCD

  Wire.begin();

  pinMode(button1, INPUT);
  //pinMode(button2, INPUT);

  pinMode(red, OUTPUT);
  pinMode(yellow, OUTPUT);
  pinMode(green, OUTPUT);

  pinMode(buzzer, OUTPUT);
  
}


void loop(){
  button1Check();
  // button2Check();

}