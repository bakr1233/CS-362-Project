#include <Wire.h>
#include <LiquidCrystal.h>

const int rs = 5, en = 6, d4 = 7, d5 = 8, d6 = 9, d7 = 10;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


// Pin definitions
const int led1 = 2;
const int led2 = 3;
const int led3 = 4;
const int buzzer = A0;



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
        
      }
      else{
        return;
      }
    }
  }
  lastButton1State = reading; //Record the last state of the button
}


//Button 2
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
        if(backwards){
          backwards = false;
        }
        else{
          backwards = true;
        }
      }
      else{
        if(backwards){
          backwards = true;
        }
        else{
          backwards = false;
        }
      }
    }
  }
  lastButton2State = reading; //Record the last state of the button
}


void race(){
  Wire.beginTransmission(4);
  Wire.write('s');
  Wire.endTransmission()

  Wire.beginTransmission(5);
  Wire.write('s');
  Wire.endTransmission()



  

}




void setup(){
  lcd.begin(16, 2); //Setup LCD

  Wire.begin();

  pinMode(button1, INPUT);
  pinMode(button2, INPUT);

  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);

  pinMode(buzzer, OUTPUT);
  
}


void loop(){
  button1Check();
  button2Check();

  
}