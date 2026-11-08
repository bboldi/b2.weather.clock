# b2.weather.clock

An ESP8266 (D1 mini) LED wall clock. It shows the local time, the weather forecast, and the outside and inside temperatures. Other systems, such as Home Assistant, can also send it short texts to show.

Tutorial Video: https://youtu.be/0P77SvR9b40

Thingiverse: https://www.thingiverse.com/thing:3883019

## Setup

1. Copy `app/src/env_variables.h.example` to `app/src/env_variables.h`. The copy is gitignored, so your keys stay local.
2. Set your openweathermap.org API key (https://openweathermap.org/appid).
3. Check the other settings in the same file. Each one is explained there:
   - location: auto or a default
   - time server
   - fallback time zone
   - temperature unit and colors
   - night mode
   - temperature upload
   - notice token
4. Build and upload: `cd app && pio run -t upload`
5. On the first boot, or while the button is held during boot, the clock opens the `b2.weather.clock` Wi-Fi access point. Connect to it and choose your network.

Unit tests of the hardware independent logic (`app/lib/clock_logic`): `cd app && pio test -e native`

## How it behaves when something fails

The clock never stops to show an error screen. It shows what it still knows and retries in the background.

- **Time:** the time comes from NTP. If NTP fails, the optional time server is the backup. Until the first sync, dashes are shown. If there was no sync for 3 hours, the dots turn slightly red.
- **UTC offset:** the UTC offset comes from the time server, which is fetched hourly. If the time server is not set, or its offset is older than 3 hours, the fallback time zone rule is used.
- **Weather:** the weather is fetched every 15 minutes, and every minute after a failure. If there is no fresh weather for 2 hours, the forecast icon and the outside temperature are hidden.
- **Location:** if auto location fails, the default location is used, and auto location is tried again later.
- **Wi-Fi:** if Wi-Fi fails, the clock keeps running offline. The config access point opens only when no network is saved, or when the button is held during boot.
- **Night mode:** from 23:00 to 07:00 the brightness is reduced. This is configurable.

## Time server

The time server is optional. It must answer a GET request with JSON like `{"utc_offset":"+01:00","unixtime":1794044207}`. You can host your own with https://github.com/bboldi/b2.ipTotim.

## Notice

Other systems, such as Home Assistant, can show a short text on the clock for a while:

```
curl -X POST http://<clock-ip>/notice \
  -H 'Authorization: Bearer <token>' \
  -d '{"top":"Er:PL","bottom":" 523","delay":30,"top_color":[255,0,0],"bottom_color":[255,255,255],"top_style":"fast_blink","bottom_style":"simple","night_dim":true}'
```

- **Fields:** every field is optional.
  - A missing row keeps its normal content.
  - A missing color uses the normal color.
- **Token:** the `Authorization` header is needed only when `_NOTICE_TOKEN_` is set.
- **Characters:**
  - Each letter has one form, whichever reads best, so upper and lower case look the same.
  - Characters that 7 segments cannot show are dropped.
  - At most 4 characters are kept, right aligned.
  - The supported characters are digits, letters, space, `-`, `_`, `=`, `*` (degree), `[`, `]` and `?`.
- **Dots:** on the top row, a `.`, `:` or `'` as the 3rd character lights the lower dot, both dots or the upper dot.
- **`delay`:**
  - The value is in seconds. The default is 60 and the maximum is 3600.
  - `0` clears the current notice.
  - `-1` keeps the notice until the button, a newer notice or `0` removes it. It is lost when the clock restarts.
  - A newer notice replaces the current one.
  - The button dismisses the current notice.
- **Styles:** `simple`, `fade_in_out`, `slow_blink` and `fast_blink`, set per row.
- **Night mode:** `"night_dim": false` keeps full brightness during Night mode.
- **Responses:** 200 on success, 400 for invalid JSON or color, 401 for a wrong token.

## Debug log

With `#define _DEBUG_ true` in `env_variables.h`, the clock keeps a Debug log of its recent events. The log costs about 7 KB RAM.

```
curl http://<clock-ip>/debug                         # or ?token=<token> / Bearer header if _NOTICE_TOKEN_ is set
```

- **Status:** the response starts with the current status: firmware build, uptime, reset reason, free heap, Wi-Fi RSSI, time sync, UTC offset, weather age, the inside sensor and the active notice.
- **Log:** after the status comes the log, oldest line first. The lines have a local time stamp, or the uptime (`+12.3s`) before the first time sync.
- **What is logged:**
  - every HTTP fetch with its result
  - time syncs
  - Wi-Fi lost and back
  - notices and button presses
  - the reset reason at boot
  - a status line every 10 minutes
- **What is left out:**
  - The API key is hidden as `appid=***`.
  - The same line is kept at most once per 10 minutes, so retries do not flood the 6 KB buffer.
  - Successful temperature uploads are left out.
- **After a restart:** the last lines are kept in RTC memory and are shown as `--- previous boot ---`. They survive a reset or a crash, but not a power loss.
- **Serial:** the serial output (115200) has everything, also with `_DEBUG_ false`.

## Weather notice

Other systems can also replace the forecast icon for a while. Each element of the icon gets its own color, and an element that is left out stays off:

```
curl -X POST http://<clock-ip>/weather \
  -d '{"cloud":[120,120,255],"sun":[255,200,0],"rain":[0,0,255],"snow":[255,255,255],"lightning":[255,255,0],"style":"slow_blink","delay":10}'
```

- **Elements:** `cloud` (both cloud LEDs), `sun`, `rain`, `snow`, `lightning`. Each takes an `[r,g,b]` color with values 0-255.
- **`style`:** works as for a notice (`simple`, `fade_in_out`, `slow_blink`, `fast_blink`).
- **`delay`:** works as for a notice: 60 s by default, `0` clears, `-1` keeps the icon until the button is clicked.
- **Icon code:** an OpenWeather icon code, `{"icon":"10d"}`, also works and uses the clock's own colors. When colors are given, they win.
- **Independence:** the Weather notice is separate from the text notice. A click clears both.

## Status lights

The left digit of the bottom row shows whether the temperature is inside (lower left segment) or outside (upper left segment). Its other four segments are **status lights**: other systems can set each of them to a color to signal a state, for example orange upper right = the heating is on, red lower right = take the bin out today.

```
curl -X POST http://<clock-ip>/status_light \
  -H 'Authorization: Bearer <token>' \
  -d '{"light":"upper_right","color":[255,100,0],"style":"slow_blink","delay":-1}'
```

- **`light`:** `top`, `upper_right`, `lower_right` or `bottom`. One request sets one light; the others do not change.
- **`color`:** `[r,g,b]`, 0-255. A missing color or `[0,0,0]` turns the light off.
- **`style`:** works as for a notice (`simple`, `fade_in_out`, `slow_blink`, `fast_blink`).
- **`delay`:**
  - In seconds, at most 172800 (48 hours). After it, the light turns off by itself.
  - Missing or `-1` keeps the light on until it is turned off.
  - `0` turns it off.
- **When they show:** together with the temperature, inside and outside alike. A notice on the bottom row hides them until it ends. The button does not clear them.
- **Night mode:** dimmed like everything else.
- **Restart:** the lights are kept in RAM only, so a restart or an OTA update turns them all off. Send the state again when it changes and, for example, when Home Assistant starts.
- **Token and responses:** as for a notice. 400 for an unknown light, invalid JSON or color. `GET /debug` lists the lights that are on.

## Button

- **Click:** switches between the inside and outside temperature, or dismisses the active notices.
- **Gestures:** every click, double click (two clicks within 400 ms) and long press (1 s) is POSTed to `_BUTTON_EVENT_URL_` as `{"event":"click|double_click|long_press","device":"b2-weather-clock"}`, for example to a Home Assistant webhook. Leave the URL empty to send nothing.
- **Click delay:** a click acts 400 ms after the release, because the clock waits to see whether a second click comes.

### Home Assistant example

The owner's setup (https://github.com/bboldi/iot.refactor, `wall_clock.yaml`) uses a webhook with `local_only: true` and the POST method only:

- **double click:** shows `Tmrw` / `Wthr` and tomorrow's forecast as a Weather notice for 10 s.
- **long press:** shows the water temperature in the boiler on the top row and the radiator circuit on the bottom row (`69*` / `50*`) for 10 s.
- **After either view,** HA sends the active alert Notice again.

## Transitions

When the segments of a digit change, the digit plays a short animation. The style is set with `_TRANSITION_STYLE_`:

- `slide_up`: the old digit scrolls out at the top while the new one scrolls in from the bottom.
- `spiral`: the old segments go dark one by one (middle, upper left, top, upper right, lower right, bottom, lower left), then the new digit fades in.
- `glitch`: random segments flicker in the new digit's color, then the new digit shows.
- `fade`: the old digit fades into the new one.
- `random`: the default. One of the four styles above is picked for each change.
- `none`: no animation.

When several digits change at once, they start one after the other, from right to left. A change that only affects the color, such as the temperature color gradient, applies at once without animation. The `_TRANSITION_ON_*_` switches choose which changes animate:

- `_TRANSITION_ON_TIME_`: a minute or hour change.
- `_TRANSITION_ON_START_`: the change from the dashes to the first time shown.
- `_TRANSITION_ON_NOTICE_`: a notice appears or ends.
- `_TRANSITION_ON_TEMP_`: the inside/outside switch, or a temperature change.

## OTA update

After the first USB upload, the firmware can be updated over Wi-Fi. Set a password in `_OTA_PASSWORD_` in `env_variables.h`, because an empty password turns OTA off. Then set `upload_port` in the `d1_mini_ota` env of `platformio.ini` to the clock's IP and upload:

```
cd app && OTA_PASSWORD=<the same password> pio run -e d1_mini_ota -t upload
```

During the upload, the display shows `OtA` and the progress in percent, then `donE`, or `Err` if the upload fails. A wrong password is rejected.
