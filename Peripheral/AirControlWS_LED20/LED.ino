#include "FastLED.h"
#define NUM_LEDS 200
#define DATA_PIN 7
#define CLOCK_PIN 13
CRGB *leds = (CRGB *)buff;
//int whichLED=0;
void setupLED() {
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
}
void breathLED(){
  float vv=128*(1+sin(millis()*TWO_PI/1000));
  for (int i = 0; i < NUM_LEDS; i++) {
     leds[i] = CHSV(255/NUM_LEDS*i, 255, vv);
  }
}
void loopLED() {
  //  int avaliableLED=sizeof(buff)/sizeof(CRGB);
  //  DEBUG_println(avaliableLED);
  //  timerLED();
  led_value = valueApproach(led_value, 0, 0.01);
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(led_hue, 255, led_value);
  }
  //  FastLED.delay(33);
  FastLED.show();
}
