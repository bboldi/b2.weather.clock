# b2.weather.clock

An LED wall clock that shows local time, the weather forecast and the outside and inside temperatures.

## Language

### Place and time

**Location**:
The place whose weather the clock shows, given as coordinates.
_Avoid_: city, position

**Auto location**:
A Location derived from the public IP address of the clock's network.
_Avoid_: geolocation, detected location

**Default location**:
The Location the owner configured. It is used when Auto location is off or not available yet.
_Avoid_: fallback location, manual location

**UTC offset**:
The difference between local time and UTC, including daylight saving time.
_Avoid_: timezone, raw offset

**Time server**:
A service the owner hosts. It reports the current UTC offset and the current time. It is the primary source of the UTC offset, and the backup source of the time when network time sync fails.
_Avoid_: time API, worldtimeapi

**Fallback time zone**:
A configured time zone rule. The clock uses it to work out the UTC offset when the Time server is not available.
_Avoid_: TZ constant

### Display

**Temperature unit**:
The one unit, Celsius or Fahrenheit, used for every temperature the clock shows and for the color thresholds.
_Avoid_: scale

**Night mode**:
The configured hours when the clock shows everything at reduced brightness.
_Avoid_: dimming, sleep mode

**Notice**:
A short text that another system, such as Home Assistant, sends to the clock. It replaces the top row, the bottom row or both for a set time, or until it is dismissed (a Permanent notice), then the normal display comes back. A newer Notice replaces the current one.
_Avoid_: message, overlay, error display

**Row**:
One of the two four-character lines of the display. The top row normally shows the time, the bottom row the temperature.
_Avoid_: line, half

### Freshness

**Stale time**:
The time is stale when there was no successful network time sync for 3 hours. The clock then shows the dots slightly red.

**Stale weather**:
The weather is stale when there was no successful weather fetch for 2 hours. The clock then hides the forecast icon and the outside temperature.
_Avoid_: weather error
