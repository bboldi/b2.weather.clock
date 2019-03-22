#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <FastLED.h>
#include <ArduinoJson.h>

// only added so I don't have to push my passwords to git repo
#include <env_variables.h>

// pin consts

#define BUTTON_PIN 14
#define LED_PIN 12

// fastled consts

#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS 64

// wifi connection

#define WIFI_AP _WIFI_AP_
#define WIFI_PASS _WIFI_PASS_

// time related

#define TIME_API_URL "http://worldtimeapi.org/api/ip"
#define LOCATION_URL "http://ip-api.com/json"
#define OPENWEATHER_URL "http://ip-api.com/json"

// weather related

// timer/animation related

#define UPDATE_TIME 10000  // update time after N millisec
#define DOT_CHANGE_TIME 500  // animate dot after N millisec

// variables

// misc

CRGB leds[NUM_LEDS];
HTTPClient http;

// animation

unsigned long dotAnimationTime = DOT_CHANGE_TIME;
bool dotVisible = false;

// data we fetched from time server

String timeZone;
String timeString;
int unixTime;

// when did we fetch the time exactly

unsigned long timeFetchedDiff;

// display time values

int displayH = 0;
int displayM = 0;
int displayS = 0;

// error container

int errorCode = 0;

// digit definitions

bool DIGIT[][7] = {
    {1, 1, 1, 0, 1, 1, 1},
    {0, 0, 0, 0, 0, 1, 1},
    {1, 0, 1, 1, 1, 0, 1},
    {0, 0, 1, 1, 1, 1, 1},
    {0, 1, 0, 1, 0, 1, 1},
    {0, 1, 1, 1, 1, 1, 0},
    {1, 1, 1, 1, 1, 1, 0},
    {0, 0, 0, 0, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1}};

CRGB clockColor = CRGB(255, 0, 0);
CRGB errorColor = CRGB(255, 0, 0);
CRGB messageColor = CRGB(0, 255, 0);
CRGB backgroundColor = CRGB(0, 0, 0);

// code

/**
 * Code to connect to wifi
 */
void connectToWiFi()
{
  WiFi.begin(WIFI_AP, WIFI_PASS);
  Serial.println("\nConnecting to WiFi");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nConnected to WiFi! IP:");
  Serial.println(WiFi.localIP());
}

/**
 * Fetch weather
 */
void fetchWeather()
{
  // fetch location

}

/**
 * Fetch time from api server and set values
 */
void fetchTime()
{
  // get time TODO

  int responseCode = 0;

  do
  {

    http.begin(TIME_API_URL);
    responseCode = http.GET();

    if (responseCode != 200)
    {
      Serial.print("\nCannot connect to tiem server; Response code: ");
      Serial.print(responseCode);
      delay(2000);
    }

  } while (responseCode != 200);

  String data = http.getString();
  Serial.print("Data: ");
  Serial.print(data);

  // parse data, fetch current time

  StaticJsonDocument<500> doc;
  DeserializationError error = deserializeJson(doc, data);

  if (error)
  {
    Serial.print("Cannot read time data from:");
    Serial.println(error.c_str());
    Serial.print(data);
    // @todo set time on error to 12:00 or something like that
  }

  String utime_str = doc["unixtime"];
  String dtime_str = doc["datetime"];

  int time = utime_str.toInt();

  int hour = dtime_str.substring(11, 13).toInt();
  int minute = dtime_str.substring(14, 16).toInt();
  int second = dtime_str.substring(17, 19).toInt();

  unixTime = hour * 60 * 60 + minute * 60 + second;
  timeFetchedDiff = millis();

  Serial.println("\nTime: ");
  Serial.println(hour);
  Serial.println(minute);
  Serial.println(second);

}


/**
 * Display different messages via index
 */
void displayMessage(int index, CRGB color)
{
  FastLED.clear();

  switch(index)
  {
    case 0:
      // Connecting
      turnOnLeds(",30,31,32,34,37,39,40,42,44,47,49,51,54,56,", color);
    break;

    case 1:
      // Sync time
      turnOnLeds(",31,32,33,34,35,37,44,47,49,51,53,54,38,40,43,", color);
    break;

    case 2:
      // Location
      turnOnLeds(",30,31,32,37,39,40,42,47,44,46,49,", color);
    break;

    case 3:
      // Weather forecast
      turnOnLeds(",30,31,33,34,37,39,40,42,44,47,51,52,53,54,55,", color);
    break;

    default:
      // -------
      turnOnLeds(",3,10,19,26,33,40,47,54,", color);
    break;
  }

  FastLED.show();
}

/**
 * setup loop
 */
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
  FastLED.setBrightness(20);
  FastLED.clear();
  FastLED.show();

  // connect to wifi

  delay(100);
  displayMessage(0, messageColor);
  connectToWiFi();

  // fetch time

  displayMessage(1, messageColor);
  fetchTime();

  // clear screen
  FastLED.clear();
  FastLED.show();
}

/**
 * calculate current time from initial and millis()
 */
void calculateTime()
{
  // millis overflow, fetch time
  if (millis() < timeFetchedDiff)
  {
    fetchTime();
  }

  int _time = unixTime + ((millis() - timeFetchedDiff) / 1000);
  displayS = _time % 60;
  displayM = (_time / 60) % 60;
  displayH = (_time / 3600) % 24;
}

void displayTime(int h, int m, int s)
{
  // dispaly hour

  if (h > 9)
  {
    setNumber(0, h / 10, clockColor);
    setNumber(1, h % 10, clockColor);
  }
  else
  {
    setNumber(0, 0, backgroundColor);
    setNumber(1, h, clockColor);
  }

  // display minute

  if (m > 9)
  {
    setNumber(2, m / 10, clockColor);
    setNumber(3, m % 10, clockColor);
  }
  else
  {
    setNumber(2, 0, clockColor);
    setNumber(3, m, clockColor);
  }
}

/**
 * function to animate blinking dot
 */
void animateDot()
{
  // prepare it for millis reset
  if(dotAnimationTime>millis()) { dotAnimationTime = 0; }
  if(millis() - dotAnimationTime >= DOT_CHANGE_TIME)
  {
    dotAnimationTime = millis();
    dotVisible = !dotVisible;
  }
}

/**
 * display the dot
 */
void displayDot(bool isVisible, CRGB color)
{
  leds[14] = leds[15] = isVisible ? color : backgroundColor;
}

/***
 * helper function to display numbers on 7 segment display
 * position - position of the number
 * number - value to display
 * color - CRGB value, color to display
 */
void setNumber(byte position, byte number, CRGB color)
{
  byte numbers[] = {0, 7, 16, 23, 30, 37, 44, 51};
  // see if the position is valid
  if ((position >= 0 && position < sizeof(numbers)) && (number >= 0 && number < 10))
  {
    for (int i = 0; i < 7; i++)
    {
      leds[numbers[position] + i] = (DIGIT[number][i] ? color : backgroundColor);
    }
  }
}

/**
 * Turn on leds by index
 * pins - list of pins, before and after each ther must be a "," example ",1,2,3,4,"
 * color - color of the pins
 */
void turnOnLeds(String pins, CRGB color)
{
  for(int i=0;i<NUM_LEDS;i++)
  {
    leds[i] = (pins.indexOf("," + String(i) + ",")>-1 ? color : backgroundColor);
  }
}

/**
 * Helper function to display error
 */
void displayError(int code)
{
  // clear
  FastLED.clear();

  // display Er:
  turnOnLeds(",0,1,2,3,4,7,10,14,15,", errorColor);
  // leds[0] = leds[1] = leds[2] = leds[3] = leds[4] = leds[7] = leds[10] = leds[14] = leds[15] = errorColor;

  if (code > 9)
  {
    setNumber(2, code / 10, errorColor);
    setNumber(3, code % 10, errorColor);
  }
  else
  {
    setNumber(2, 0, errorColor);
    setNumber(3, code, errorColor);
  }
}

/**
 * main loop
 */
void loop()
{
  if(errorCode!=0)
  {
    displayError(errorCode);
    FastLED.show();

    // wait and reset
    delay(10000);
    ESP.restart();
    delay(10000);

  }
  else
  {
    // calculate and display time

    calculateTime();
    displayTime(displayH, displayM, displayS);

    // animate dot

    animateDot();
    displayDot(dotVisible, clockColor);

    // @todo move this to interrupt
    if (digitalRead(BUTTON_PIN) == 1)
    {
      Serial.print("!");
    }

    // update display
    FastLED.show();
  }

}
