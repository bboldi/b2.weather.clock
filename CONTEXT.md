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

**Weather notice**:
A forecast icon that another system sends to the clock. It replaces the real forecast icon for a set time, in the same way a Notice replaces a Row.
_Avoid_: icon override

**Status light**:
One of four segments of the first bottom-row digit (top, upper right, lower right, bottom). Another system, such as Home Assistant, sets a status light to a color to signal a state, for example that the heating is on. It stays lit while the temperature switches between inside and outside, until it is turned off or its time runs out. It shows only together with the temperature, so a Notice on the bottom row hides it. A restart turns all status lights off.
_Avoid_: segment, flag, indicator (the indicator is the inside / outside sign)

**Button gesture**:
A click, a double click or a long press on the clock's button. The clock reports every gesture to the owner's home automation. A click also switches between the inside and outside temperature, or dismisses the current Notice and Weather notice.
_Avoid_: button event, tap

**Transition**:
The short animation a digit plays when what it shows changes, for example when a minute passes or a Notice appears or ends. The owner chooses the style, or lets the clock pick one at random.
_Avoid_: effect, change animation

**Debug log**:
The recent events the clock remembers, so that the owner can see later what happened. It is only kept when the owner turns debugging on. The last few events survive a restart, but not a power loss.
_Avoid_: serial log, history

### Freshness

**Stale time**:
The time is stale when there was no successful network time sync for 3 hours. The clock then shows the dots slightly red.

**Stale weather**:
The weather is stale when there was no successful weather fetch for 2 hours. The clock then hides the forecast icon and the outside temperature.
_Avoid_: weather error
