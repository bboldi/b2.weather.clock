#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <FastLED.h>
#include <ArduinoJson.h>

#include <FS.h>

// only added so I don't have to push my passwords to git repo
#include <env_variables.h>

// pin consts

#define BUTTON_PIN 14
#define LED_PIN 12

// fastled consts

#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS 64

// time related

#define TIME_API_URL "http://worldtimeapi.org/api/ip"
#define LOCATION_URL "http://ip-api.com/json"
#define OPENWEATHER_URL_FORECAST "http://api.openweathermap.org/data/2.5/forecast?units=metric&cnt=2&appid=" _OPENWEATHER_API_KEY_
#define OPENWEATHER_URL_CURRENT "http://api.openweathermap.org/data/2.5/weather?units=metric&appid=" _OPENWEATHER_API_KEY_

// weather related

#define TEMP_FREEZING _TEMP_FREEZING_
#define TEMP_COLD _TEMP_COLD_
#define TEMP_NORMAL _TEMP_NORMAL_
#define TEMP_HOT _TEMP_HOT_

// timer/animation related

#define UPDATE_FORECAST 900000 // update data every N mins
#define DOT_CHANGE_TIME 500    // animate dot after N millisec

// misc

#define LED_BRIGHTNESS 255
#define RETRY_AFTER_ERROR 10000
#define CONFIG_AP_PASS _CONFIG_AP_PASSWORD_
#define CONFIG_AP_NAME "b2.weather.clock"
#define CONFIG_AP_TIMEOUT 180

#define TEMP_SUFFIX ",54,51,53," // "c"
// #define TEMP_SUFFIX ",51,52,54,55," // "F"

// variables

// misc

CRGB leds[NUM_LEDS];
HTTPClient http;
WiFiManager wifiManager;

// animation

unsigned long dotAnimationTime = DOT_CHANGE_TIME;
bool dotVisible = false;

unsigned long forecastUpdated = 0;

// data we fetched from time server

int unixTime;

// when did we fetch the time exactly

unsigned long timeFetchedDiff;

// display time values

int displayH = 0;
int displayM = 0;
int displayS = 0;

// location

String lat = "";
String lng = "";

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

CRGB clockColor = CRGB::White;
CRGB errorColor = CRGB(255, 0, 0);
CRGB messageColor = CRGB(0, 255, 0);

CRGB temperatureColorFreezing = CRGB::Blue;
CRGB temperatureColorCold = CRGB::DodgerBlue;
CRGB temperatureColorNormal = CRGB::Gold;
CRGB temperatureColorHot = CRGB::Red;

CRGB backgroundColor = CRGB(0, 0, 0);

// code

/**
 * Turn on leds by index
 * pins - list of pins, before and after each ther must be a "," example ",1,2,3,4,"
 * color - color of the pins
 */
void turnOnLeds(String pins, CRGB color, boolean doClear = true )
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = (pins.indexOf("," + String(i) + ",") > -1 ? color : ( doClear ? backgroundColor : leds[i]) );
  }
}


/**
 * set error code
 */
void setErrorCode(byte code)
{
  errorCode = errorCode | code;
}

/**
 * Code to connect to wifi
 */
bool connectToWiFi(bool init = false)
{
  if(init)
  {
    // try to connect
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.setConfigPortalTimeout(CONFIG_AP_TIMEOUT);

    // if there's a button press when restarting, go to AP
    if (digitalRead(BUTTON_PIN) == 1) {
      wifiManager.startConfigPortal(CONFIG_AP_NAME, CONFIG_AP_PASS);
      return true;
    }
  }

  wifiManager.autoConnect(CONFIG_AP_NAME, CONFIG_AP_PASS);

}

/**
 * Config mode display
 */
void configModeCallback(WiFiManager *_wifiMan)
{
  Serial.println("AP config mode");
  FastLED.clear();
  displayMessage(5, messageColor);
}

/**
 * Fetch weather
 */
void fetchLocation()
{
  // fetch location
  delay(500);

  http.begin(LOCATION_URL);
  int responseCode = http.GET();

  Serial.println("Fetch location");

  if (responseCode != 200)
  {
    Serial.println("Error fetching location");
    // @todo set error code

    setErrorCode(0b1);
    forecastUpdated = millis() + UPDATE_FORECAST;
  }

  String data = http.getString();

  StaticJsonDocument<500> doc;
  DeserializationError error = deserializeJson(doc, data);
  if(error.code() != DeserializationError::Ok) { Serial.println(error.c_str()); }

  String _lat = doc["lat"];
  String _lng = doc["lon"];

  lat = _lat;
  lng = _lng;

  Serial.print("\nlat: ");
  Serial.print(lat);
  Serial.print(" lng: ");
  Serial.print(lng);
}

String forecast = "";
String temperature = "";

/**
 * Fetch weather
 */
void fetchWeather()
{
  String url = OPENWEATHER_URL_FORECAST;
  url += "&lat=" + lat + "&lon=" + lng;

  Serial.println(url);

  // fetch weather
  http.begin(url);
  int responseCode = http.GET();

  Serial.println("Fetch forecast");
  Serial.println(responseCode);

  if (responseCode != 200)
  {
    Serial.println("\nError fetching weather forecast");
    // @todo set error code
    // @todo set update time to 5 sec from now to try and update it
    // maybe we should do this for the rest

    setErrorCode(0b10);

    // update it in 5 seconds
    // if(millis() - forecastUpdated > UPDATE_FORECAST)
    forecastUpdated = millis() - (UPDATE_FORECAST - 5000);
  }

  String data = http.getString();
  Serial.print(data);

  StaticJsonDocument<500> doc;
  DeserializationError error = deserializeJson(doc, data);
  if(error.code() != DeserializationError::Ok) { Serial.println(error.c_str()); }

  // get forecast for the next 3 hour
  String _forecast = doc["list"][0]["weather"][0]["icon"];

  Serial.println("\n---\n");
  Serial.println(_forecast);

  forecast = _forecast;

  fetchTemperature();
}

// fetch temperaturek
void fetchTemperature()
{
  String url = OPENWEATHER_URL_CURRENT;
  url += "&lat=" + lat + "&lon=" + lng;

  Serial.println(url);

  // fetch weather
  http.begin(url);
  int responseCode = http.GET();

  Serial.println("Fetch forecast");
  Serial.println(responseCode);

  if (responseCode != 200)
  {
    Serial.println("\nError fetching weather current");
    // @todo set error code
    // @todo set update time to 5 sec from now to try and update it
    // maybe we should do this for the rest

    setErrorCode(0b10);

    // update it in 5 seconds
    // if(millis() - forecastUpdated > UPDATE_FORECAST)
    forecastUpdated = millis() - (UPDATE_FORECAST - 5000);
  }

  String data = http.getString();
  Serial.print(data);

  StaticJsonDocument<500> doc;
  DeserializationError error = deserializeJson(doc, data);
  if(error.code() != DeserializationError::Ok) { Serial.println(error.c_str()); }

  // get current temperature
  String _temperature = doc["main"]["temp"];

  Serial.println("\n---\n");
  Serial.println(_temperature);
  Serial.println(_temperature.toInt());

  temperature = _temperature;
}

/**
 * Display forecast data using the leds
 */
void displayForecast(CRGB cloudColor1, CRGB cloudColor2, CRGB sunColor,
                     CRGB rainColor, CRGB snowColor, CRGB lightningColor)
{
  leds[58] = rainColor;
  leds[59] = snowColor;
  leds[60] = lightningColor;
  leds[61] = sunColor;
  leds[62] = cloudColor2;
  leds[63] = cloudColor1;
}

/**
 * Fetch time from api server and set values
 */
void fetchTime()
{
  // get time TODO

  int responseCode = 0;

  http.begin(TIME_API_URL);
  responseCode = http.GET();

  Serial.println("Fetch time");

  if (responseCode != 200)
  {
    Serial.print("\nCannot connect to tiem server; Response code: ");
    Serial.print(responseCode);

    setErrorCode(0b100);

    // @todo maybe set a refresh time here as well
  }

  String data = http.getString();
  Serial.print("Data: ");
  Serial.print(data);

  // parse data, fetch current time

  StaticJsonDocument<500> doc;
  DeserializationError error = deserializeJson(doc, data);
  if(error.code() != DeserializationError::Ok) { Serial.println(error.c_str()); }

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

  switch (index)
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
    turnOnLeds(",30,31,32,37,39,40,42,47,44,46,51,52,54,55,56,57,", color);
    break;

  case 3:
    // Weather forecast
    turnOnLeds(",30,31,33,34,37,39,40,42,44,47,51,52,53,54,55,", color);
    break;

  case 4:
    // refresh
    turnOnLeds(",30,33,38,37,41,40,39,44,45,48,47,51,54,", color);
    break;

  case 5:
    // ap config
    turnOnLeds(",0,1,3,4,5,6,7,8,10,11,13,30,31,32,34,37,40,42,39,44,47,49,51,52,54,55,", color);
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
  FastLED.setBrightness(LED_BRIGHTNESS);

  FastLED.clear();
  FastLED.show();

  // connect to wifi

  delay(500);
  displayMessage(0, messageColor);
  delay(200);
  connectToWiFi(true);

  // fetch time

  displayMessage(1, messageColor);
  fetchTime();

  // fetch location
  displayMessage(2, messageColor);
  fetchLocation();

  // fetch weather
  displayMessage(3, messageColor);
  fetchWeather();

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
  if (dotAnimationTime > millis())
  {
    dotAnimationTime = 0;
  }
  if (millis() - dotAnimationTime >= DOT_CHANGE_TIME)
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
 * Helper function to display error
 */
void displayError(int code)
{
  // clear
  FastLED.clear();

  // display Er:
  turnOnLeds(",0,1,2,3,4,7,10,14,", errorColor);

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

void setTemperature(int temp)
{
  CRGB _color = temperatureColorHot;

  if (temp <= TEMP_FREEZING)
  {
    _color = temperatureColorFreezing;
  }
  else if (temp <= TEMP_COLD)
  {
    _color = temperatureColorCold;
  }
  else if (temp <= TEMP_HOT)
  {
    _color = temperatureColorNormal;
  }

  // reset temp numbers
  setNumber(4, 8, backgroundColor);
  setNumber(5, 8, backgroundColor);
  setNumber(6, 8, backgroundColor);
  setNumber(7, 8, backgroundColor);

  // set prefix

  byte _minusLed = 0;
  boolean _showMinus = false;

  if (temp < 0)
  {
    _showMinus = true;
  }
  temp = abs(temp);

  if (temp > 99)
  {
    _minusLed = 33;
    setNumber(4, temp / 100, _color);
    setNumber(5, (temp % 100) / 10, _color);
    setNumber(6, temp % 10, _color);
  }
  else if (temp > 9)
  {
    _minusLed = 33;
    setNumber(5, temp / 10, _color);
    setNumber(6, temp % 10, _color);
  }
  else
  {
    _minusLed = 40;
    setNumber(6, temp, _color);
  }

  if (_showMinus)
  {
    leds[_minusLed] = _color;
  }

  turnOnLeds(TEMP_SUFFIX, _color, false);
}

void setForecast(String icon)
{
  if (icon == "01d")
  {
    // clear sky
    displayForecast(CRGB::Black, CRGB::Black, CRGB::Yellow, CRGB::Black, CRGB::Black, CRGB::Black);
  }
  else if (icon == "01n")
  {
    // clear sky night
    displayForecast(CRGB::Black, CRGB::Black, CRGB::White, CRGB::Black, CRGB::Black, CRGB::Black);
  }
  else if (icon == "02d")
  {
    // few clouds
    displayForecast(CRGB::LightBlue, CRGB::Blue, CRGB::Yellow, CRGB::Black, CRGB::Black, CRGB::Black);
  }
  else if (icon == "02n")
  {
    // few clouds night
    displayForecast(CRGB::LightBlue, CRGB::Blue, CRGB::White, CRGB::Black, CRGB::Black, CRGB::Black);
  }
  else if (icon == "03d" || icon == "03n")
  {
    // scattered clouds
    displayForecast(CRGB::LightBlue, CRGB::LightBlue, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black);
  }
  else if (icon == "04d" || icon == "04n")
  {
    // broken clouds
    displayForecast(CRGB::Blue, CRGB::Gray, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black);
  }
  else if (icon == "09d" || icon == "09n")
  {
    // shower rain
    displayForecast(CRGB::LightBlue, CRGB::Blue, CRGB::Black, CRGB::Blue, CRGB::Black, CRGB::Black);
  }
  else if (icon == "10d")
  {
    // rain
    displayForecast(CRGB::Blue, CRGB::DarkSlateBlue, CRGB::Gold, CRGB::DeepSkyBlue, CRGB::Black, CRGB::Black);
  }
  else if (icon == "10n")
  {
    // rain night
    displayForecast(CRGB::Indigo, CRGB::DarkSlateBlue, CRGB::Silver, CRGB::DeepSkyBlue, CRGB::Black, CRGB::Black);
  }
  else if (icon == "11d" || icon == "11n")
  {
    // thunderstorm
    displayForecast(CRGB::Gray, CRGB::DarkSlateBlue, CRGB::Black, CRGB::DeepSkyBlue, CRGB::Black, CRGB::Yellow);
  }
  else if (icon == "13d" || icon == "13n")
  {
    // snow
    displayForecast(CRGB::White, CRGB::LightBlue, CRGB::Black, CRGB::Black, CRGB::White, CRGB::Black);
  }
  else if (icon == "50d" || icon == "50n")
  {
    // mist
    displayForecast(CRGB::Gray, CRGB::DarkGray, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black);
  }
  else
  {
    // default
    displayForecast(CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black);
  }
}

/**
 * check if we need to refresh data
 */
bool doUpdateForecast()
{
  if (millis() < forecastUpdated)
  {
    forecastUpdated = 0;
  }

  if (millis() - forecastUpdated > UPDATE_FORECAST)
  {
    forecastUpdated = millis();
    return true;
  }

  return false;
}

int cnt = 0;

/**
 * main loop
 */
void loop()
{
  if (errorCode != 0)
  {
    displayError(errorCode);
    FastLED.show();
    delay(RETRY_AFTER_ERROR);
  }
  else
  {
    // calculate and display time

    calculateTime();
    displayTime(displayH, displayM, displayS);

    // animate dot

    animateDot();
    displayDot(dotVisible, clockColor);

    // set temperature

    setTemperature(temperature.toInt());

    // set forecast

		String icons[] = {
			"01d",
			"02d",
			"03d",
			"04d",
			"09d",
			"10d",
			"11d",
			"13d",
			"50d",
			"01n",
			"02n",
			"03n",
			"04n",
			"09n",
			"10n",
			"11n",
			"13n",
			"50n"
		};

    setForecast(forecast);

    // @todo move this to interrupt
    if (digitalRead(BUTTON_PIN) == 1)
    {
      Serial.print("!");
      cnt++;
      // cnt = (cnt > sizeof(icons) / sizeof(icons[0])) ? cnt = 0 : cnt;
      delay(300);

      temperature = String(random(-99,200));
      forecast = icons[random(0, sizeof(icons) / sizeof(icons[0]))];
    }

    // update display
    FastLED.show();
  }

  // do update forecast?
  if (doUpdateForecast() || errorCode > 0)
  {
    if (errorCode > 0)
    {
      displayMessage(4, errorColor);
    }

    Serial.println("Refresh data");
    setErrorCode(0);

    connectToWiFi();
    fetchTime();
    fetchLocation();
    fetchWeather();
  }
}
