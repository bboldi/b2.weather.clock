#include <Arduino.h>
#include <FastLED.h>

// pin consts

#define BUTTON_PIN 14
#define LED_PIN 12

// fastled consts

#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS 64

// variables

CRGB leds[NUM_LEDS];

void setup()
{
  // randomize
  randomSeed(analogRead(0));

  // setup serial
  Serial.begin(115200);

  // pin definitions
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);

  // fastled setup
	FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
	FastLED.clear();
	FastLED.setBrightness(50);
}

bool x = false;

void loop()
{
  for(int i=0;i<7;i++)
  {
    leds[i].red = (x ? 255 : 0);
    leds[i].green = 0;
    leds[i].blue = 0;
  }

  x=!x;

  Serial.print(".");
	FastLED.show();
  delay(500);
}
