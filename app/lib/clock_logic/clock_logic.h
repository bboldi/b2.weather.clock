// Hardware independent logic of the clock, unit tested on the host (pio test -e native)
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace clocklogic
{

struct Rgb
{
  uint8_t r, g, b;
};

struct ColorStop
{
  int value;
  Rgb color;
};

/**
 * Color for a value on a gradient, stops must be sorted by value.
 * Below the first / above the last stop the end colors are used.
 */
inline Rgb gradientColor(int value, const ColorStop *stops, size_t count)
{
  if (value <= stops[0].value)
    return stops[0].color;

  for (size_t i = 1; i < count; i++)
  {
    if (value <= stops[i].value)
    {
      const ColorStop &a = stops[i - 1];
      const ColorStop &b = stops[i];
      int span = b.value - a.value;
      int pos = value - a.value;
      return {
          (uint8_t)(a.color.r + (b.color.r - a.color.r) * pos / span),
          (uint8_t)(a.color.g + (b.color.g - a.color.g) * pos / span),
          (uint8_t)(a.color.b + (b.color.b - a.color.b) * pos / span)};
    }
  }

  return stops[count - 1].color;
}

/**
 * Parse "+01:00" / "-05:30" into seconds, false if the format is wrong
 */
inline bool parseUtcOffset(const char *text, long &seconds)
{
  if (!text || (text[0] != '+' && text[0] != '-'))
    return false;

  char *end;
  long hours = strtol(text + 1, &end, 10);
  if (end == text + 1 || *end != ':')
    return false;

  char *minutesStart = end + 1;
  long minutes = strtol(minutesStart, &end, 10);
  if (end == minutesStart || *end != '\0' || hours > 14 || minutes > 59)
    return false;

  seconds = (text[0] == '-' ? -1 : 1) * (hours * 3600 + minutes * 60);
  return true;
}

/**
 * Seconds since local midnight
 */
inline long secondsOfDay(time_t utc, long offset)
{
  long s = (long)((utc + offset) % 86400);
  return s < 0 ? s + 86400 : s;
}

/**
 * True if the hour is in [start, end), works over midnight too (e.g. 23 -> 7)
 */
inline bool isNight(int hour, int start, int end)
{
  return start <= end ? (hour >= start && hour < end) : (hour >= start || hour < end);
}

/**
 * True if at least interval ms passed since last, millis() overflow safe
 */
inline bool elapsed(uint32_t now, uint32_t last, uint32_t interval)
{
  return (uint32_t)(now - last) >= interval;
}

/**
 * Triangle wave 0..255..0 over period ms, used to fade the dots
 */
inline uint8_t fadeLevel(unsigned long now, unsigned long period)
{
  unsigned long pos = now % period;
  unsigned long half = period / 2;
  unsigned long level = pos < half ? pos * 255 / half : (period - pos) * 255 / (period - half);
  return (uint8_t)level;
}

/**
 * Button press detection without blocking delay
 */
struct Debouncer
{
  unsigned long debounceMs;
  bool state;
  unsigned long changedAt;

  explicit Debouncer(unsigned long ms) : debounceMs(ms), state(false), changedAt(0) {}

  // true once per press (released -> pressed)
  bool pressed(bool raw, unsigned long now)
  {
    if (raw == state || !elapsed(now, changedAt, debounceMs))
      return false;

    state = raw;
    changedAt = now;
    return raw;
  }
};

/**
 * Click, double click and long press from the debounced button state
 */
enum Gesture : uint8_t
{
  G_NONE, G_CLICK, G_DOUBLE_CLICK, G_LONG_PRESS
};

struct Gestures
{
  uint32_t longMs, doubleMs;
  bool down = false, longFired = false;
  uint8_t clicks = 0;
  uint32_t pressAt = 0, releaseAt = 0;

  Gestures(uint32_t longPressMs, uint32_t doubleClickMs) : longMs(longPressMs), doubleMs(doubleClickMs) {}

  Gesture update(bool isDown, uint32_t now)
  {
    if (isDown && !down)
    {
      down = true;
      pressAt = now;
      longFired = false;
    }
    else if (!isDown && down)
    {
      down = false;
      if (!longFired && ++clicks >= 2)
      {
        clicks = 0;
        return G_DOUBLE_CLICK;
      }
      releaseAt = now;
    }

    if (down && !longFired && elapsed(now, pressAt, longMs))
    {
      longFired = true;
      clicks = 0;
      return G_LONG_PRESS;
    }
    // a single click is only sure when no second one came in time
    if (!down && clicks == 1 && elapsed(now, releaseAt, doubleMs))
    {
      clicks = 0;
      return G_CLICK;
    }
    return G_NONE;
  }
};

// ---- Notice: text sent from outside, shown on a row for a while ----

// segment bits: a=top, b=upper right, c=lower right, d=bottom, e=lower left, f=upper left, g=middle
enum : uint8_t
{
  SEG_A = 1, SEG_B = 2, SEG_C = 4, SEG_D = 8, SEG_E = 16, SEG_F = 32, SEG_G = 64
};

/**
 * 7 segment glyph of a character, -1 if it can not be shown.
 * Letters have one form, whichever reads best (A, b, C, d, h ...), case does not matter.
 * K, M, V, W, X are approximations.
 */
inline int glyph(char c)
{
  static const uint8_t DIGITS[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  // A..Z
  static const uint8_t LETTERS[] = {
      0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x3D, 0x74, 0x30, 0x1E, 0x76, 0x38, 0x55,
      0x54, 0x3F, 0x73, 0x67, 0x50, 0x6D, 0x78, 0x3E, 0x62, 0x7E, 0x49, 0x6E, 0x5B};

  if (c >= '0' && c <= '9')
    return DIGITS[c - '0'];
  if (c >= 'a' && c <= 'z')
    c -= 'a' - 'A';
  if (c >= 'A' && c <= 'Z')
    return LETTERS[c - 'A'];

  switch (c)
  {
  case ' ': return 0;
  case '-': return SEG_G;
  case '_': return SEG_D;
  case '=': return SEG_G | SEG_D;
  case '*': return SEG_A | SEG_B | SEG_F | SEG_G; // degree sign
  case '[': return SEG_A | SEG_D | SEG_E | SEG_F;
  case ']': return SEG_A | SEG_B | SEG_C | SEG_D;
  case '?': return SEG_A | SEG_B | SEG_E | SEG_G;
  default: return -1;
  }
}

// dots between the top row digits
enum : uint8_t
{
  DOT_NONE = 0, DOT_LOWER = 1, DOT_UPPER = 2
};

inline uint8_t separatorDots(char c)
{
  return c == '.' ? DOT_LOWER : c == '\'' ? DOT_UPPER : c == ':' ? (DOT_LOWER | DOT_UPPER) : DOT_NONE;
}

struct RowLayout
{
  uint8_t segments[4]; // glyph per digit, left to right
  uint8_t dots;        // DOT_* bits, only the top row has dots
};

// first max glyphs of text (unknown characters dropped) right aligned into out[0..max)
inline void placeGlyphs(const char *text, uint8_t *out, size_t max)
{
  uint8_t found[4];
  size_t n = 0;
  for (const char *p = text; *p && n < max; p++)
  {
    int g = glyph(*p);
    if (g >= 0)
      found[n++] = (uint8_t)g;
  }
  for (size_t i = 0; i < max; i++)
    out[i] = i < max - n ? 0 : found[i - (max - n)];
}

/**
 * Lay out a row text. Characters that can not be shown are dropped, at most 4 are kept, right aligned.
 * Top row: if the 3rd kept character is . : or ' it sets the dots and splits the row 2 + 2.
 */
inline RowLayout layoutRow(const char *text, bool top)
{
  RowLayout row = {{0, 0, 0, 0}, DOT_NONE};
  if (!text)
    return row;

  if (top)
  {
    // keep glyphs and separators, then look at the 3rd one
    char kept[6];
    size_t n = 0;
    const char *rest = nullptr;
    for (const char *p = text; *p && n < 3; p++)
    {
      if (glyph(*p) >= 0 || separatorDots(*p))
      {
        kept[n++] = *p;
        rest = p + 1;
      }
    }
    if (n == 3 && separatorDots(kept[2]))
    {
      char left[3] = {kept[0], kept[1], 0};
      placeGlyphs(left, row.segments, 2);
      placeGlyphs(rest, row.segments + 2, 2);
      row.dots = separatorDots(kept[2]);
      return row;
    }
  }

  placeGlyphs(text, row.segments, 4);
  return row;
}

enum NoticeStyle : uint8_t
{
  STYLE_SIMPLE, STYLE_FADE, STYLE_SLOW_BLINK, STYLE_FAST_BLINK
};

inline NoticeStyle parseStyle(const char *s)
{
  if (!s)
    return STYLE_SIMPLE;
  if (!strcmp(s, "fade_in_out"))
    return STYLE_FADE;
  if (!strcmp(s, "slow_blink"))
    return STYLE_SLOW_BLINK;
  if (!strcmp(s, "fast_blink"))
    return STYLE_FAST_BLINK;
  return STYLE_SIMPLE;
}

// brightness 0..255 of a row with the given style at time now (ms)
inline uint8_t styleLevel(NoticeStyle style, unsigned long now)
{
  switch (style)
  {
  case STYLE_FADE: return fadeLevel(now, 2000);
  case STYLE_SLOW_BLINK: return (now % 2000) < 1000 ? 255 : 0;
  case STYLE_FAST_BLINK: return (now % 500) < 250 ? 255 : 0;
  default: return 255;
  }
}

/**
 * Notice duration in ms from the "delay" seconds: missing -> 60 s, capped at 1 hour, 0 means clear
 */
// delay -1: the notice stays until the button, a newer notice or delay 0
constexpr uint32_t NOTICE_FOREVER = UINT32_MAX;

inline uint32_t noticeDurationMs(bool given, long seconds)
{
  if (!given)
    return 60000;
  if (seconds == -1)
    return NOTICE_FOREVER;
  if (seconds <= 0)
    return 0;
  return (uint32_t)(seconds > 3600 ? 3600 : seconds) * 1000;
}

// ---- Debug log ----

/**
 * Lines in a fixed size ring, the oldest are overwritten. Each line ends with '\n'.
 */
template <size_t N>
struct LogRing
{
  char data[N];
  size_t head = 0;      // next write position
  bool wrapped = false; // data was overwritten at least once

  void append(const char *line)
  {
    for (const char *p = line; *p; p++)
      put(*p);
    put('\n');
  }

  /**
   * Content in order as up to 2 pieces; after wrapping the cut first line is skipped
   */
  void pieces(const char *&a, size_t &aLen, const char *&b, size_t &bLen) const
  {
    if (!wrapped)
    {
      a = data, aLen = head, b = data, bLen = 0;
      return;
    }
    size_t start = head;
    while (start < N && data[start] != '\n')
      start++;
    if (start < N)
    {
      a = data + start + 1, aLen = N - start - 1, b = data, bLen = head;
      return;
    }
    // the cut line runs into the beginning
    size_t i = 0;
    while (i < head && data[i] != '\n')
      i++;
    a = data + (i < head ? i + 1 : head), aLen = i < head ? head - i - 1 : 0, b = data, bLen = 0;
  }

  /**
   * Last whole lines that fit into max - 1 bytes, null terminated, returns the length
   */
  size_t tail(char *out, size_t max) const
  {
    const char *a, *b;
    size_t aLen, bLen;
    pieces(a, aLen, b, bLen);
    size_t total = aLen + bLen;
    size_t skip = total > max - 1 ? total - (max - 1) : 0;

    size_t n = 0;
    bool lineStart = skip == 0;
    for (size_t i = skip; i < total; i++)
    {
      char c = i < aLen ? a[i] : b[i - aLen];
      if (!lineStart)
      {
        lineStart = c == '\n';
        continue;
      }
      out[n++] = c;
    }
    out[n] = 0;
    return n;
  }

private:
  void put(char c)
  {
    data[head++] = c;
    if (head == N)
      head = 0, wrapped = true;
  }
};

/**
 * Hide the value of key (e.g. "appid=") up to the next '&', in place
 */
inline void redactParam(char *text, size_t size, const char *key)
{
  char *p = strstr(text, key);
  if (!p)
    return;
  char *value = p + strlen(key);
  char *end = value;
  while (*end && *end != '&' && *end != ' ')
    end++;
  if (end - value <= 3)
    return;
  memcpy(value, "***", 3);
  memmove(value + 3, end, strlen(end) + 1);
  (void)size;
}

/**
 * Line prefix: local "HH:MM:SS" once the time is known, before that the uptime "+12.3s"
 */
inline void formatStamp(char *out, size_t size, bool timeKnown, long secondsOfDay, unsigned long uptimeMs)
{
  if (timeKnown)
    snprintf(out, size, "%02ld:%02ld:%02ld", secondsOfDay / 3600, (secondsOfDay / 60) % 60, secondsOfDay % 60);
  else
    snprintf(out, size, "+%lu.%lus", uptimeMs / 1000, (uptimeMs / 100) % 10);
}

/**
 * Lets the same text through once per window, so retries every minute don't flood the Debug log
 */
template <size_t SLOTS>
struct RepeatFilter
{
  uint32_t hashes[SLOTS] = {};
  uint32_t times[SLOTS] = {};
  size_t next = 0;

  bool allow(const char *text, uint32_t now, uint32_t window)
  {
    uint32_t h = 2166136261u; // FNV-1a
    for (const char *p = text; *p; p++)
      h = (h ^ (uint8_t)*p) * 16777619u;

    for (size_t i = 0; i < SLOTS; i++)
    {
      if (hashes[i] == h && times[i] != 0)
      {
        if (!elapsed(now, times[i], window))
          return false;
        times[i] = now | 1;
        return true;
      }
    }
    hashes[next] = h;
    times[next] = now | 1; // 0 means empty slot
    next = (next + 1) % SLOTS;
    return true;
  }
};

// ---- Transition: animation of a digit when its segments change ----

// segment index inside a digit, the order of the leds on the strip (same as DIGIT in main.ino)
enum : uint8_t
{
  IX_E = 0, IX_F = 1, IX_D = 2, IX_G = 3, IX_A = 4, IX_C = 5, IX_B = 6
};

enum TransitionStyle : uint8_t
{
  TR_NONE, TR_SLIDE_UP, TR_SPIRAL, TR_GLITCH, TR_FADE, TR_RANDOM
};

inline TransitionStyle parseTransition(const char *s)
{
  if (!s || !strcmp(s, "none"))
    return TR_NONE;
  if (!strcmp(s, "slide_up"))
    return TR_SLIDE_UP;
  if (!strcmp(s, "spiral"))
    return TR_SPIRAL;
  if (!strcmp(s, "glitch"))
    return TR_GLITCH;
  if (!strcmp(s, "fade"))
    return TR_FADE;
  return TR_RANDOM;
}

const uint32_t SLIDE_STEP_MS = 120;
const uint32_t SPIRAL_STEP_MS = 60;
const uint32_t SPIRAL_FADE_MS = 300;
const uint32_t GLITCH_MS = 400;
const uint32_t GLITCH_STEP_MS = 50;
const uint32_t FADE_MS = 400;
const uint32_t STAGGER_MS = 120;

inline bool isLit(const Rgb &c) { return c.r || c.g || c.b; }

// bit i set if segment index i is lit
inline uint8_t litMask(const Rgb seg[7])
{
  uint8_t m = 0;
  for (int i = 0; i < 7; i++)
    if (isLit(seg[i]))
      m |= 1 << i;
  return m;
}

inline Rgb scaleRgb(Rgb c, uint8_t level)
{
  return {(uint8_t)(c.r * level / 255), (uint8_t)(c.g * level / 255), (uint8_t)(c.b * level / 255)};
}

inline Rgb blendRgb(Rgb a, Rgb b, uint8_t amount)
{
  return {(uint8_t)(a.r + (b.r - a.r) * amount / 255), (uint8_t)(a.g + (b.g - a.g) * amount / 255),
          (uint8_t)(a.b + (b.b - a.b) * amount / 255)};
}

// every segment one level up: bottom -> middle -> top, lower verticals -> upper ones, the top leaves
inline void shiftUp(const Rgb in[7], Rgb out[7])
{
  for (int i = 0; i < 7; i++)
    out[i] = {0, 0, 0};
  out[IX_A] = in[IX_G];
  out[IX_G] = in[IX_D];
  out[IX_F] = in[IX_E];
  out[IX_B] = in[IX_C];
}

// every segment one level down, the bottom leaves
inline void shiftDown(const Rgb in[7], Rgb out[7])
{
  for (int i = 0; i < 7; i++)
    out[i] = {0, 0, 0};
  out[IX_G] = in[IX_A];
  out[IX_D] = in[IX_G];
  out[IX_E] = in[IX_F];
  out[IX_C] = in[IX_B];
}

/**
 * One frame of a digit transition, t ms after its start (negative: not started yet).
 * Returns true when finished, out is then the new digit.
 */
inline bool transitionFrame(TransitionStyle style, const Rgb from[7], const Rgb to[7], long t, uint32_t seed, Rgb out[7])
{
  if (t < 0)
  {
    for (int i = 0; i < 7; i++)
      out[i] = from[i];
    return false;
  }

  switch (style)
  {
  case TR_SLIDE_UP:
  {
    // old one scrolls out at the top while the new one scrolls in from the bottom
    long step = t / SLIDE_STEP_MS; // 0: 1 level, 1: 2 levels, then done
    if (step >= 2)
      break;
    Rgb oldPart[7], newPart[7], tmp[7];
    shiftUp(from, oldPart);
    shiftDown(to, tmp);
    if (step == 0)
      shiftDown(tmp, newPart);
    else
    {
      for (int i = 0; i < 7; i++)
        newPart[i] = tmp[i];
      shiftUp(oldPart, tmp);
      for (int i = 0; i < 7; i++)
        oldPart[i] = tmp[i];
    }
    for (int i = 0; i < 7; i++)
      out[i] = isLit(newPart[i]) ? newPart[i] : oldPart[i];
    return false;
  }

  case TR_SPIRAL:
  {
    // segments go dark one by one: middle, upper left, top, upper right, lower right, bottom, lower left
    static const uint8_t ORDER[] = {IX_G, IX_F, IX_A, IX_B, IX_C, IX_D, IX_E};
    long off = t / SPIRAL_STEP_MS + 1;
    if (off <= 7)
    {
      for (int i = 0; i < 7; i++)
        out[i] = from[i];
      for (long i = 0; i < off; i++)
        out[ORDER[i]] = {0, 0, 0};
      return false;
    }
    long fadeT = t - 7 * (long)SPIRAL_STEP_MS;
    if (fadeT >= (long)SPIRAL_FADE_MS)
      break;
    for (int i = 0; i < 7; i++)
      out[i] = scaleRgb(to[i], (uint8_t)(fadeT * 255 / SPIRAL_FADE_MS));
    return false;
  }

  case TR_GLITCH:
  {
    if (t >= (long)GLITCH_MS)
      break;
    // random segments in the new digit's color (the old one's if the new is empty)
    Rgb color = {0, 0, 0};
    for (int i = 0; i < 7 && !isLit(color); i++)
      color = isLit(to[i]) ? to[i] : color;
    for (int i = 0; i < 7 && !isLit(color); i++)
      color = isLit(from[i]) ? from[i] : color;
    uint32_t x = seed * 2654435761u + (uint32_t)(t / GLITCH_STEP_MS) * 40503u + 1;
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    for (int i = 0; i < 7; i++)
      out[i] = (x >> (i * 3)) & 1 ? color : Rgb{0, 0, 0};
    return false;
  }

  case TR_FADE:
    if (t >= (long)FADE_MS)
      break;
    for (int i = 0; i < 7; i++)
      out[i] = blendRgb(from[i], to[i], (uint8_t)(t * 255 / FADE_MS));
    return false;

  default:
    break;
  }

  for (int i = 0; i < 7; i++)
    out[i] = to[i];
  return true;
}

} // namespace clocklogic
