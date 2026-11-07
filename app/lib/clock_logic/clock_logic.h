// Hardware independent logic of the clock, unit tested on the host (pio test -e native)
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
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
inline uint32_t noticeDurationMs(bool given, long seconds)
{
  if (!given)
    return 60000;
  if (seconds <= 0)
    return 0;
  return (uint32_t)(seconds > 3600 ? 3600 : seconds) * 1000;
}

} // namespace clocklogic
