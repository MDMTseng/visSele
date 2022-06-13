/**
   The MIT License (MIT)

   Copyright (c) 2018 by ThingPulse, Daniel Eichhorn
   Copyright (c) 2018 by Fabrice Weinberg

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

   ThingPulse invests considerable time and money to develop these open source libraries.
   Please support us by buying our products (and not the clones) from
   https://thingpulse.com

*/
#include <Arduino.h> 
#include <string> 
// Include the correct display library
// For a connection via I2C using Wire include
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SH1106Wire.h" // legacy include: `#include "SSD1306.h"`
int READPin=5;
// Initialize the OLED display using brzo_i2c
// D3 -> SDA
// D5 -> SCL
// SSD1306Brzo display(0x3c, D3, D5);
// or
// SH1106Brzo  display(0x3c, D3, D5);

// Initialize the OLED display using Wire library
SH1106Wire display(0x3c, SDA, SCL);   // ADDRESS, SDA, SCL  -  SDA and SCL usually populate automatically based on your board's pins_arduino.h e.g. https://github.com/esp8266/Arduino/blob/master/variants/nodemcu/pins_arduino.h
// SH1106Wire display(0x3c, SDA, SCL);

void setup() {
  display.init();

  // display.flipScreenVertically();

  display.setContrast(250);
  pinMode(READPin, INPUT_PULLUP);
}


int counter=0;
unsigned long T_start=0;
int checkPIN()
{
  static int pinPreState=0;
  int curSt=digitalRead(READPin);
  if(curSt!=pinPreState)
  {
    if(pinPreState==0)
    {
      counter++;
    }
    pinPreState=curSt;
  }
  return counter;
}

void loop() { 

  static int preCounter=-999;
  static unsigned long preUpdateTime=0;

  
  static int window_start_count=0;
  static unsigned long window_start_ms=0;

  int counter = checkPIN();

  unsigned long curms= millis()-T_start;
  static float pre_window_speed=0;
  static float pre_window_speed_LP=0;
  if(curms-window_start_ms>1000)
  {
    pre_window_speed=(float)1000*(counter-window_start_count)/(curms-window_start_ms);
    pre_window_speed_LP+=(pre_window_speed-pre_window_speed_LP)*0.5;
    window_start_ms+=1000;
    window_start_count=counter;
  }





  unsigned long timDiff=curms-preUpdateTime;
  bool isSkip=false;
  if(timDiff<30)isSkip=true;//too fast
  if(preCounter==counter)//unchanged
  {
    isSkip=true;
  }
  if(timDiff>(unsigned long)1000)// too long no update
  {
    isSkip=false;
  }
  if(isSkip)return;


  preUpdateTime=curms;
  display.clear();
  
  
  char buffer[100];
  sprintf(buffer,"%05d pcs",counter);
  display.drawString(0,0, buffer);
  
  sprintf(buffer,"%3.1f pcs/s",(float)1000*counter/curms);
  display.drawString(0,15, buffer);

  
  sprintf(buffer,"%3.1f pcs/s  *RECENT",pre_window_speed);
  display.drawString(0,30, buffer);
  sprintf(buffer,"%3.1f pcs/s  *SMOOTH",pre_window_speed_LP);
  display.drawString(0,45, buffer);


  display.display();
  
  
  preCounter=counter;
}
