#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include <coredecls.h>
#include <WiFiManager.h>

#include <FastLED.h>
#include <ArduinoJson.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <clock_logic.h>

// only added so I don't have to push my passwords to git repo
#include <env_variables.h>

using clocklogic::elapsed;

// pin consts

#define BUTTON_PIN 14
#define LED_PIN 12
#define DS18B20_PIN 2

// fastled consts

#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS 64

// network related

#define LOCATION_URL "http://ip-api.com/json?fields=status,lat,lon"
#define OPENWEATHER_URL "http://api.openweathermap.org/data/2.5/"
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"
#define HTTP_TIMEOUT 3000 // ms, keeps the display from freezing long on a bad network

// temperature related

const char *OPENWEATHER_UNITS = _USE_FAHRENHEIT_ ? "imperial" : "metric";
const char *TEMP_SUFFIX = _USE_FAHRENHEIT_ ? ",51,52,54,55," /* "F" */ : ",54,51,53," /* "c" */;

#define TEMP_FREEZING _TEMP_FREEZING_
#define TEMP_COLD _TEMP_COLD_
#define TEMP_NORMAL _TEMP_NORMAL_
#define TEMP_HOT _TEMP_HOT_

// timer/animation related

#define UPDATE_WEATHER 900000      // fetch weather every 15 min
#define RETRY_WEATHER 60000        // retry after 1 min if the last fetch failed
#define WEATHER_MAX_AGE 7200000    // hide weather if there is no fresh data for 2 hours
#define UPDATE_TIME_SERVER 3600000 // fetch utc offset every hour
#define RETRY_TIME_SERVER 60000    // retry after 1 min if the last fetch failed
#define OFFSET_MAX_AGE 10800000    // use the fallback time zone if the time server offset is older than 3 hours
#define NTP_MISSED 5400000         // SNTP syncs hourly, after 1.5 hours without sync take the time server's time
#define TIME_MAX_AGE 10800000      // tint the dots if there was no time sync for 3 hours
#define DOT_CHANGE_TIME 500        // blink dot after N millisec
#define DOT_FADE_PERIOD 2000       // one fade in + fade out of the dot
#define TEMP_CHANGE 15000          // how often to change temp display
#define BUTTON_DEBOUNCE 50

// misc

#define LED_BRIGHTNESS 255
#define NIGHT_BRIGHTNESS _NIGHT_BRIGHTNESS_
#define CONFIG_AP_PASS _CONFIG_AP_PASSWORD_
#define CONFIG_AP_NAME "b2.weather.clock"
#define CONFIG_AP_TIMEOUT 180

// sizeof("") == 1, so this checks for a non empty string define
#define IS_SET(define) (sizeof(define) > 1)

// variables

// misc

CRGB leds[NUM_LEDS];

WiFiManager wifiManager;

clocklogic::Debouncer button(BUTTON_DEBOUNCE);

// animation

unsigned long dotAnimationTime = 0;
bool dotVisible = false;

unsigned long changeTempDisplay = 0;
bool showForecast = true;

// time: UTC comes from NTP (or the time server), the offset from the time server or the fallback TZ

bool timeSynced = false;
unsigned long lastTimeSync = 0;

bool timeServerOk = false;
unsigned long timeServerAttempt = 0;
bool hasServerOffset = false;
long serverOffset = 0;
unsigned long serverOffsetAt = 0;

// location, the default is used until the auto location works

bool autoLocationOk = false;
float lat = _DEFAULT_LAT_;
float lng = _DEFAULT_LON_;

// weather

String forecast = "";
int temperature = 0;
bool hasWeather = false;
bool weatherFetchOk = false;
unsigned long weatherUpdated = 0; // last successful fetch
unsigned long weatherAttempt = 0; // last fetch attempt

// notice: text sent from outside (Home Assistant), shown on a row for a while

struct Notice
{
  bool active = false;
  unsigned long start = 0;
  uint32_t duration = 0;
  bool hasTop = false, hasBottom = false;
  clocklogic::RowLayout top, bottom;
  CRGB topColor, bottomColor;
  clocklogic::NoticeStyle topStyle = clocklogic::STYLE_SIMPLE, bottomStyle = clocklogic::STYLE_SIMPLE;
  bool nightDim = true;
} notice;

ESP8266WebServer server(80);

// ponytail: guessed from the wiring, verify with a "12'34" / "12.34" notice
#define DOT_UPPER_LED 14
#define DOT_LOWER_LED 15

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
CRGB staleDotColor = CRGB(255, 120, 120); // dots when the time could not be synced for a while
CRGB messageColor = CRGB(0, 255, 0);
CRGB indicatorColorOut = CRGB::Green;
CRGB indicatorColorIn = CRGB::Green;

// temperature colors fade linearly between these points
const clocklogic::ColorStop temperatureColors[] = {
    {TEMP_FREEZING, {0, 0, 255}},  // Blue
    {TEMP_COLD, {30, 144, 255}},   // DodgerBlue
    {TEMP_NORMAL, {255, 215, 0}},  // Gold
    {TEMP_HOT, {255, 0, 0}},       // Red
};

CRGB backgroundColor = CRGB(0, 0, 0);

// onewire

OneWire oneWire(DS18B20_PIN);
DallasTemperature DS18B20(&oneWire);
bool insideOk = false;
float insideTempC = DEVICE_DISCONNECTED_C;

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
 * Code to connect to wifi, only at startup. Later the ESP reconnects by itself.
 */
void connectToWiFi()
{
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setConfigPortalTimeout(CONFIG_AP_TIMEOUT);

  // AP only on button press at restart or when no network is saved yet,
  // a failed connection (e.g. router still booting) must not block the clock
  if (digitalRead(BUTTON_PIN) == 1 || !wifiManager.getWiFiIsSaved())
  {
    wifiManager.startConfigPortal(CONFIG_AP_NAME, CONFIG_AP_PASS);
  }

  // connect to the saved network, the ESP keeps retrying in the background if this fails
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  WiFi.waitForConnectResult(15000);
  Serial.printf("WiFi '%s' status %d ip %s\n", WiFi.SSID().c_str(), WiFi.status(), WiFi.localIP().toString().c_str());
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
 * GET an url and parse it as json, false on any error
 */
bool getJson(const String &url, JsonDocument &doc)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.printf("No wifi (status %d), skip %s\n", WiFi.status(), url.c_str());
    return false;
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT);
  http.begin(client, url);
  int responseCode = http.GET();
  Serial.printf("GET %s -> %d\n", url.c_str(), responseCode);

  bool ok = false;
  if (responseCode == HTTP_CODE_OK)
  {
    DeserializationError error = deserializeJson(doc, http.getString());
    if (error) { Serial.println(error.c_str()); }
    ok = !error;
  }
  http.end();
  return ok;
}

/**
 * Fetch utc offset from the time server, and the time too if NTP is not working
 */
bool fetchTimeServer()
{
  JsonDocument doc;
  long offset;
  if (!getJson(_TIME_SERVER_URL_, doc) || !clocklogic::parseUtcOffset(doc["utc_offset"] | "", offset))
  {
    Serial.println("Error fetching time server");
    return false;
  }

  serverOffset = offset;
  serverOffsetAt = millis();
  hasServerOffset = true;

  long unixtime = doc["unixtime"] | 0L;
  if (unixtime > 0 && (!timeSynced || elapsed(millis(), lastTimeSync, NTP_MISSED)))
  {
    Serial.println("Set time from time server");
    timeval tv = {unixtime, 0};
    settimeofday(&tv, nullptr);
  }

  Serial.printf("utc offset: %ld\n", serverOffset);
  return true;
}

void updateTimeServer()
{
  if (!IS_SET(_TIME_SERVER_URL_))
  {
    return;
  }
  timeServerAttempt = millis();
  timeServerOk = fetchTimeServer();
}

/**
 * Seconds since local midnight, offset from the time server if fresh, otherwise the fallback TZ rule
 */
long localSecondsOfDay()
{
  time_t now = time(nullptr);
  if (hasServerOffset && !elapsed(millis(), serverOffsetAt, OFFSET_MAX_AGE))
  {
    return clocklogic::secondsOfDay(now, serverOffset);
  }

  struct tm local;
  localtime_r(&now, &local);
  return local.tm_hour * 3600L + local.tm_min * 60 + local.tm_sec;
}

/**
 * Fetch location based on our ip
 */
bool fetchLocation()
{
  JsonDocument doc;
  if (!getJson(LOCATION_URL, doc) || doc["status"] != "success")
  {
    Serial.println("Error fetching location, using the default");
    return false;
  }

  lat = doc["lat"];
  lng = doc["lon"];

  Serial.printf("lat: %f lng: %f\n", lat, lng);
  return true;
}

void updateLocation()
{
  if (_AUTO_LOCATION_ && !autoLocationOk)
  {
    autoLocationOk = fetchLocation();
  }
}

/**
 * Fetch forecast icon and current temperature
 */
bool fetchWeather()
{
  String params = String("&units=") + OPENWEATHER_UNITS + "&appid=" _OPENWEATHER_API_KEY_
                  "&lat=" + String(lat, 4) + "&lon=" + String(lng, 4);
  JsonDocument doc;

  // get forecast for the next 3 hour
  if (!getJson(OPENWEATHER_URL "forecast?cnt=2" + params, doc))
  {
    Serial.println("Error fetching weather forecast");
    return false;
  }
  String icon = doc["list"][0]["weather"][0]["icon"] | "";

  // get current temperature
  if (!getJson(OPENWEATHER_URL "weather?lang=en" + params, doc) || icon == "" || !doc["main"]["temp"].is<float>())
  {
    Serial.println("Error fetching weather current");
    return false;
  }

  forecast = icon;
  temperature = (int)doc["main"]["temp"].as<float>();

  Serial.printf("forecast: %s temp: %d\n", forecast.c_str(), temperature);
  return true;
}

/**
 * Refresh weather data, measure and send inside temperature
 */
void updateWeather()
{
  updateLocation();

  weatherAttempt = millis();
  weatherFetchOk = fetchWeather();
  if (weatherFetchOk)
  {
    weatherUpdated = millis();
    hasWeather = true;
  }

  // measure and send temperature no matter if we have data from net
  // because this might be important for other applications
  readInsideTemp();
  sendTemperature();
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
 * Read the result of the last conversion and start a new one, does not block
 */
void readInsideTemp()
{
  // by index, so a sensor plugged in after boot is found too
  insideTempC = DS18B20.getTempCByIndex(0);
  insideOk = insideTempC != DEVICE_DISCONNECTED_C;
  DS18B20.requestTemperatures();
}

int insideTemp()
{
  return (int)(_USE_FAHRENHEIT_ ? DallasTemperature::toFahrenheit(insideTempC) : insideTempC);
}

/**
 * Send temperature (celsius) to an url
 */
void sendTemperature()
{
  if (IS_SET(_SEND_TEMP_URI) && insideOk && WiFi.status() == WL_CONNECTED)
  {
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT);
    http.begin(client, _SEND_TEMP_URI + String(insideTempC));
    if (IS_SET(_SEND_TEMP_USER))
    {
      http.setAuthorization(_SEND_TEMP_USER, _SEND_TEMP_PASS);
    }
    int responseCode = http.GET();
    Serial.println("send temperature");
    Serial.println(responseCode);
    Serial.println(http.getString());
    http.end();
  }
}

/**
 * Setup DS sensor
 */
void DS18B20Setup()
{
  DS18B20.begin();
  // first conversion blocking, so we don't show the 85C power-on value
  DS18B20.requestTemperatures();
  DS18B20.setWaitForConversion(false);
  readInsideTemp();

  Serial.println("TEMP_C:");
  Serial.println(insideTempC);
}

/**
 * setup loop
 */
void setup()
{
  // setup serial
  Serial.begin(115200);

  // pin definitions
  pinMode(BUTTON_PIN, INPUT);

  // temp sensor

  DS18B20Setup();

  // fastled setup
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);

  FastLED.clear();
  FastLED.show();

  // connect to wifi

  delay(500);
  displayMessage(0, messageColor);
  delay(200);
  connectToWiFi();

  // time sync runs in the background (SNTP, hourly), also called when the time server sets the time
  settimeofday_cb([]() {
    lastTimeSync = millis();
    timeSynced = true;
  });
  configTime(_FALLBACK_TZ_, NTP_SERVER_1, NTP_SERVER_2);

  // notice endpoint for Home Assistant
  server.collectHeaders("Authorization");
  server.on("/notice", HTTP_POST, handleNotice);
  server.begin();

  displayMessage(1, messageColor);
  updateTimeServer();

  // fetch location and weather
  displayMessage(3, messageColor);
  updateWeather();

  // clear screen
  FastLED.clear();
  FastLED.show();
}

/**
 * show dashes instead of the clock digits
 */
void displayNoTime()
{
  for (int i = 0; i < 4; i++)
  {
    setNumber(i, 8, backgroundColor);
  }
  turnOnLeds(",3,10,19,26,", clockColor, false);
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
 * change display of inside / ourside temperature
 */
void tempDisplaySwitch()
{
  if (elapsed(millis(), changeTempDisplay, TEMP_CHANGE))
  {
    changeTempDisplay = millis();
    showForecast = !showForecast;

    Serial.println("switch temp display");
    Serial.println(showForecast ? "F" : "I");

    // refresh temperature if needed
    if(!showForecast) {
      readInsideTemp();
      // send temperature to the http link if required
      sendTemperature();
    }
  }
}

/**
 * function to animate blinking dot
 */
void animateDot()
{
  if (elapsed(millis(), dotAnimationTime, DOT_CHANGE_TIME))
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

CRGB temperatureColor(int temp)
{
  clocklogic::Rgb c = clocklogic::gradientColor(temp, temperatureColors, sizeof(temperatureColors) / sizeof(temperatureColors[0]));
  return CRGB(c.r, c.g, c.b);
}

void setTemperature(int temp, bool outside)
{
  CRGB _color = temperatureColor(temp);

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

  // show indicator if possible
  if(temp<99)
  {
    turnOnLeds( outside ? ",31," : ",30,", outside ? indicatorColorOut : indicatorColorIn, false);
  }
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
    displayForecast(CRGB::Blue, CRGB::Gray, CRGB::Gold, CRGB::Blue, CRGB::Black, CRGB::Black);
  }
  else if (icon == "10n")
  {
    // rain night
    displayForecast(CRGB::Blue, CRGB::Blue, CRGB::Silver, CRGB::Blue, CRGB::Black, CRGB::Black);
  }
  else if (icon == "11d" || icon == "11n")
  {
    // thunderstorm
    displayForecast(CRGB::Gray, CRGB::Blue, CRGB::Black, CRGB::Blue, CRGB::Black, CRGB::Yellow);
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
 * main loop
 */
void loop()
{
  // display time, or dashes until we know it

  long secs = localSecondsOfDay();
  if (timeSynced)
  {
    displayTime(secs / 3600, (secs / 60) % 60, secs % 60);
  }
  else
  {
    displayNoTime();
  }

  // notice ends by itself after its duration
  if (notice.active && elapsed(millis(), notice.start, notice.duration))
  {
    notice.active = false;
  }

  // night mode, only if we know what time it is, a notice can ask for full brightness
  bool night = timeSynced && clocklogic::isNight(secs / 3600, _NIGHT_START_HOUR_, _NIGHT_END_HOUR_);
  bool dim = night && !(notice.active && !notice.nightDim);
  FastLED.setBrightness(dim ? NIGHT_BRIGHTNESS : LED_BRIGHTNESS);

  // animate dot, slightly red if the time is not synced for a while

  CRGB dotColor = (timeSynced && !elapsed(millis(), lastTimeSync, TIME_MAX_AGE)) ? clockColor : staleDotColor;

  if(_DO_FADE)
  {
    displayDot(true, dotColor.nscale8(clocklogic::fadeLevel(millis(), DOT_FADE_PERIOD)));
  }
  else
  {
    animateDot();
    displayDot(dotVisible, dotColor);
  }

  tempDisplaySwitch();

  // only show what we have: outside needs fresh weather, inside needs the sensor

  bool weatherOk = hasWeather && !elapsed(millis(), weatherUpdated, WEATHER_MAX_AGE);
  bool showOutside = weatherOk && (showForecast || !insideOk);

  if (showOutside || insideOk)
  {
    setTemperature(showOutside ? temperature : insideTemp(), showOutside);
  }
  else
  {
    // clear temperature digits, suffix and indicator
    fill_solid(leds + 30, 28, backgroundColor);
  }

  // set forecast, empty icon turns it off
  setForecast(weatherOk ? forecast : "");

  // notice rows go over the normal display
  if (notice.active)
  {
    displayNotice();
  }

  // button dismisses the notice, otherwise switches inside / outside temperature
  if (button.pressed(digitalRead(BUTTON_PIN) == 1, millis()))
  {
    if (notice.active)
    {
      notice.active = false;
    }
    else
    {
      showForecast = !showForecast;
      changeTempDisplay = millis();
    }
  }

  // update display
  FastLED.show();

  server.handleClient();

  // refresh data, sooner if the last attempt failed
  if (elapsed(millis(), weatherAttempt, weatherFetchOk ? UPDATE_WEATHER : RETRY_WEATHER))
  {
    Serial.println("Refresh weather");
    updateWeather();
  }

  if (IS_SET(_TIME_SERVER_URL_) && elapsed(millis(), timeServerAttempt, timeServerOk ? UPDATE_TIME_SERVER : RETRY_TIME_SERVER))
  {
    Serial.println("Refresh time server");
    updateTimeServer();
  }
}

/**
 * show a 7 segment glyph (clocklogic::SEG_* bits) on a digit
 */
void setGlyph(byte position, uint8_t segments, CRGB color)
{
  // led order inside a digit, same as in DIGIT
  static const uint8_t SEG_ORDER[] = {clocklogic::SEG_E, clocklogic::SEG_F, clocklogic::SEG_D, clocklogic::SEG_G,
                                      clocklogic::SEG_A, clocklogic::SEG_C, clocklogic::SEG_B};
  byte numbers[] = {0, 7, 16, 23, 30, 37, 44, 51};
  for (int i = 0; i < 7; i++)
  {
    leds[numbers[position] + i] = (segments & SEG_ORDER[i]) ? color : backgroundColor;
  }
}

void displayNoticeRow(const clocklogic::RowLayout &row, byte firstDigit, CRGB color, clocklogic::NoticeStyle style)
{
  color.nscale8(clocklogic::styleLevel(style, millis() - notice.start));
  for (byte i = 0; i < 4; i++)
  {
    setGlyph(firstDigit + i, row.segments[i], color);
  }
}

void displayNotice()
{
  if (notice.hasTop)
  {
    displayNoticeRow(notice.top, 0, notice.topColor, notice.topStyle);
    CRGB dotColor = notice.topColor;
    dotColor.nscale8(clocklogic::styleLevel(notice.topStyle, millis() - notice.start));
    leds[DOT_UPPER_LED] = (notice.top.dots & clocklogic::DOT_UPPER) ? dotColor : backgroundColor;
    leds[DOT_LOWER_LED] = (notice.top.dots & clocklogic::DOT_LOWER) ? dotColor : backgroundColor;
  }
  if (notice.hasBottom)
  {
    displayNoticeRow(notice.bottom, 4, notice.bottomColor, notice.bottomStyle);
  }
}

/**
 * [r,g,b] from json, keeps the default if missing, false if it is malformed
 */
bool readColor(JsonVariant value, CRGB &color)
{
  if (value.isNull())
    return true;

  JsonArray rgb = value.as<JsonArray>();
  if (rgb.isNull() || rgb.size() != 3)
    return false;

  for (int i = 0; i < 3; i++)
  {
    if (!rgb[i].is<int>() || rgb[i].as<int>() < 0 || rgb[i].as<int>() > 255)
      return false;
  }
  color = CRGB(rgb[0].as<int>(), rgb[1].as<int>(), rgb[2].as<int>());
  return true;
}

/**
 * POST /notice {"top":"Er:PL","bottom":" 523","delay":30,"top_color":[255,0,0],"bottom_color":[255,255,255],
 *               "top_style":"fast_blink","bottom_style":"simple","night_dim":true}
 */
void handleNotice()
{
  if (IS_SET(_NOTICE_TOKEN_) && server.header("Authorization") != String("Bearer ") + _NOTICE_TOKEN_)
  {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>())
  {
    server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }

  Notice next;
  next.topColor = clockColor;
  next.bottomColor = CRGB::White;
  if (!readColor(doc["top_color"], next.topColor) || !readColor(doc["bottom_color"], next.bottomColor))
  {
    server.send(400, "application/json", "{\"error\":\"color must be [r,g,b], 0-255\"}");
    return;
  }

  next.duration = clocklogic::noticeDurationMs(doc["delay"].is<long>(), doc["delay"] | 0L);
  next.hasTop = doc["top"].is<const char *>();
  next.hasBottom = doc["bottom"].is<const char *>();
  next.top = clocklogic::layoutRow(doc["top"] | "", true);
  next.bottom = clocklogic::layoutRow(doc["bottom"] | "", false);
  next.topStyle = clocklogic::parseStyle(doc["top_style"] | "simple");
  next.bottomStyle = clocklogic::parseStyle(doc["bottom_style"] | "simple");
  next.nightDim = doc["night_dim"] | true;
  next.start = millis();
  next.active = next.duration > 0 && (next.hasTop || next.hasBottom);

  // a newer notice replaces the current one, delay 0 clears it
  notice = next;
  Serial.printf("notice %s for %lu ms\n", notice.active ? "shown" : "cleared", (unsigned long)notice.duration);
  server.send(200, "application/json", notice.active ? "{\"status\":\"shown\"}" : "{\"status\":\"cleared\"}");
}
