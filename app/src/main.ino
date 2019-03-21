#include <Arduino.h>
#include <FastLED.h>

// pin consts

#define BUTTON_PIN 14
#define LED_PIN 12

// fastled consts

#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS 64

// digit definitions

// variables

CRGB leds[NUM_LEDS];

bool DIGIT[][7] = {
  { 1,1,1,0,1,1,1 },
  { 0,0,0,0,0,1,1 },
  { 1,0,1,1,1,0,1 },
  { 0,0,1,1,1,1,1 },
  { 0,1,0,1,0,1,1 },
  { 0,1,1,1,1,1,0 },
  { 1,1,1,1,1,1,0 },
  { 0,0,0,0,1,1,1 },
  { 1,1,1,1,1,1,1 },
  { 0,1,1,1,1,1,1 }
};

void setup()
{
  // randomize
  randomSeed(analogRead(0));

  // setup serial
  Serial.begin(115200);

  // pin definitions
  pinMode(BUTTON_PIN, INPUT);

  // fastled setup
	FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
	FastLED.clear();
	FastLED.setBrightness(100);
}

void setNumber(byte position, byte number, CRGB color)
{
  byte numbers[] = { 0,7,16,23,30,37,44,51 };
  // see if the position is valid
  if((position>=0 && position<sizeof(numbers)) && (number>=0 && number<10))
  {
    Serial.print("!");
    for(int i=0;i<7;i++) { leds[numbers[position]+i] = (DIGIT[number][i] ? color : CRGB(0,0,0)); }
  }
}

byte cnt=0;

void loop()
{
  /*
  for(int i=0;i<8;i++) {
    setNumber(i,cnt,CRGB(200,255,200));
  }
  */

  setNumber(1,cnt,CRGB(200,255,200));

  cnt++;
  cnt = (cnt>9) ? 0 : cnt;

  Serial.print(cnt);
	FastLED.show();
  delay(500);
}
