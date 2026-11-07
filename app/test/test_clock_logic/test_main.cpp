#include <unity.h>
#include <clock_logic.h>

using namespace clocklogic;

void setUp() {}
void tearDown() {}

static const ColorStop STOPS[] = {
    {20, {255, 255, 255}},
    {26, {255, 0, 0}},
};

void test_gradient_ends_are_clamped()
{
  Rgb low = gradientColor(-10, STOPS, 2);
  Rgb high = gradientColor(40, STOPS, 2);
  TEST_ASSERT_EQUAL_UINT8(255, low.g);
  TEST_ASSERT_EQUAL_UINT8(0, high.g);
}

void test_gradient_interpolates_between_stops()
{
  Rgb mid = gradientColor(23, STOPS, 2);
  TEST_ASSERT_EQUAL_UINT8(255, mid.r);
  TEST_ASSERT_EQUAL_UINT8(128, mid.g);
  TEST_ASSERT_EQUAL_UINT8(128, mid.b);

  Rgb exact = gradientColor(26, STOPS, 2);
  TEST_ASSERT_EQUAL_UINT8(0, exact.g);
}

void test_gradient_multiple_stops()
{
  const ColorStop stops[] = {{0, {0, 0, 255}}, {10, {0, 255, 0}}, {20, {255, 0, 0}}};
  Rgb c = gradientColor(15, stops, 3);
  TEST_ASSERT_EQUAL_UINT8(127, c.r);
  TEST_ASSERT_EQUAL_UINT8(128, c.g);
  TEST_ASSERT_EQUAL_UINT8(0, c.b);
}

void test_parse_utc_offset()
{
  long s = 0;
  TEST_ASSERT_TRUE(parseUtcOffset("+01:00", s));
  TEST_ASSERT_EQUAL(3600, s);
  TEST_ASSERT_TRUE(parseUtcOffset("-05:30", s));
  TEST_ASSERT_EQUAL(-19800, s);
  TEST_ASSERT_TRUE(parseUtcOffset("+00:00", s));
  TEST_ASSERT_EQUAL(0, s);
}

void test_parse_utc_offset_rejects_garbage()
{
  long s = 42;
  TEST_ASSERT_FALSE(parseUtcOffset(nullptr, s));
  TEST_ASSERT_FALSE(parseUtcOffset("", s));
  TEST_ASSERT_FALSE(parseUtcOffset("01:00", s));
  TEST_ASSERT_FALSE(parseUtcOffset("+1", s));
  TEST_ASSERT_FALSE(parseUtcOffset("+01:00x", s));
  TEST_ASSERT_FALSE(parseUtcOffset("+25:00", s));
  TEST_ASSERT_EQUAL(42, s);
}

void test_seconds_of_day()
{
  // 2026-11-07T09:36:47Z
  TEST_ASSERT_EQUAL(10 * 3600 + 36 * 60 + 47, secondsOfDay(1794044207, 3600));
  // negative offset wrapping to the previous day
  TEST_ASSERT_EQUAL(23 * 3600, secondsOfDay(86400 * 10 + 3600, -7200));
}

void test_is_night()
{
  TEST_ASSERT_TRUE(isNight(23, 23, 7));
  TEST_ASSERT_TRUE(isNight(0, 23, 7));
  TEST_ASSERT_TRUE(isNight(6, 23, 7));
  TEST_ASSERT_FALSE(isNight(7, 23, 7));
  TEST_ASSERT_FALSE(isNight(22, 23, 7));
  TEST_ASSERT_TRUE(isNight(2, 1, 5));
  TEST_ASSERT_FALSE(isNight(5, 1, 5));
}

void test_elapsed_survives_millis_overflow()
{
  TEST_ASSERT_TRUE(elapsed(1000, 0, 1000));
  TEST_ASSERT_FALSE(elapsed(999, 0, 1000));
  TEST_ASSERT_TRUE(elapsed(500, 0xFFFFFFFFUL - 600, 1000));
  TEST_ASSERT_FALSE(elapsed(100, 0xFFFFFFFFUL - 600, 1000));
}

void test_fade_level_triangle()
{
  TEST_ASSERT_EQUAL_UINT8(0, fadeLevel(0, 2000));
  TEST_ASSERT_EQUAL_UINT8(127, fadeLevel(500, 2000));
  TEST_ASSERT_EQUAL_UINT8(255, fadeLevel(1000, 2000));
  TEST_ASSERT_EQUAL_UINT8(127, fadeLevel(1500, 2000));
  TEST_ASSERT_EQUAL_UINT8(0, fadeLevel(2000, 2000));
}

void test_debouncer_one_event_per_press()
{
  Debouncer b(50);
  TEST_ASSERT_TRUE(b.pressed(true, 100));
  TEST_ASSERT_FALSE(b.pressed(true, 120));  // still held
  TEST_ASSERT_FALSE(b.pressed(false, 130)); // bounce, too early
  TEST_ASSERT_FALSE(b.pressed(true, 140));
  TEST_ASSERT_FALSE(b.pressed(false, 200)); // release
  TEST_ASSERT_TRUE(b.pressed(true, 260));   // next press
}

void test_glyphs()
{
  TEST_ASSERT_EQUAL(0x3F, glyph('0'));
  TEST_ASSERT_EQUAL(0x79, glyph('E'));
  TEST_ASSERT_EQUAL(0x50, glyph('r'));
  TEST_ASSERT_EQUAL(0x50, glyph('R'));
  TEST_ASSERT_EQUAL(glyph('O'), glyph('o'));     // case does not matter
  TEST_ASSERT_EQUAL(0x74, glyph('H'));           // lower case h form
  TEST_ASSERT_EQUAL(0x76, glyph('k'));           // K looks like H
  TEST_ASSERT_EQUAL(0, glyph(' '));
  TEST_ASSERT_EQUAL(-1, glyph('#'));
  TEST_ASSERT_EQUAL(-1, glyph('.'));
  for (char c = 'A'; c <= 'Z'; c++)
    TEST_ASSERT_TRUE(glyph(c) > 0);
}

void test_bottom_row_drops_unknown_and_keeps_four()
{
  RowLayout r = layoutRow("w#@%d..z..t", false);
  TEST_ASSERT_EQUAL(glyph('w'), r.segments[0]);
  TEST_ASSERT_EQUAL(glyph('d'), r.segments[1]);
  TEST_ASSERT_EQUAL(glyph('z'), r.segments[2]);
  TEST_ASSERT_EQUAL(glyph('t'), r.segments[3]);
  TEST_ASSERT_EQUAL(DOT_NONE, r.dots);

  r = layoutRow("Er.PL", false); // no dot on the bottom row
  TEST_ASSERT_EQUAL(glyph('E'), r.segments[0]);
  TEST_ASSERT_EQUAL(glyph('L'), r.segments[3]);

  r = layoutRow("ABCDEF", false);
  TEST_ASSERT_EQUAL(glyph('D'), r.segments[3]);
}

void test_bottom_row_right_aligned()
{
  RowLayout r = layoutRow("523", false);
  TEST_ASSERT_EQUAL(0, r.segments[0]);
  TEST_ASSERT_EQUAL(glyph('5'), r.segments[1]);
  TEST_ASSERT_EQUAL(glyph('3'), r.segments[3]);

  r = layoutRow("", false);
  TEST_ASSERT_EQUAL(0, r.segments[3]);
}

void test_top_row_separator()
{
  RowLayout r = layoutRow("Er:PL", true);
  TEST_ASSERT_EQUAL(glyph('E'), r.segments[0]);
  TEST_ASSERT_EQUAL(glyph('r'), r.segments[1]);
  TEST_ASSERT_EQUAL(glyph('P'), r.segments[2]);
  TEST_ASSERT_EQUAL(glyph('L'), r.segments[3]);
  TEST_ASSERT_EQUAL(DOT_LOWER | DOT_UPPER, r.dots);

  TEST_ASSERT_EQUAL(DOT_LOWER, layoutRow("12.34", true).dots);
  TEST_ASSERT_EQUAL(DOT_UPPER, layoutRow("12'34", true).dots);

  // unknown characters are dropped before the 3rd position is checked
  r = layoutRow("E#r.P", true);
  TEST_ASSERT_EQUAL(DOT_LOWER, r.dots);
  TEST_ASSERT_EQUAL(0, r.segments[2]); // right half right aligned
  TEST_ASSERT_EQUAL(glyph('P'), r.segments[3]);
}

void test_top_row_without_separator()
{
  RowLayout r = layoutRow("1234", true);
  TEST_ASSERT_EQUAL(DOT_NONE, r.dots);
  TEST_ASSERT_EQUAL(glyph('3'), r.segments[2]);

  r = layoutRow("1:5", true); // ':' is the 2nd character, so it is just dropped
  TEST_ASSERT_EQUAL(DOT_NONE, r.dots);
  TEST_ASSERT_EQUAL(glyph('1'), r.segments[2]);
  TEST_ASSERT_EQUAL(glyph('5'), r.segments[3]);
}

void test_styles()
{
  TEST_ASSERT_EQUAL(STYLE_SIMPLE, parseStyle(nullptr));
  TEST_ASSERT_EQUAL(STYLE_SIMPLE, parseStyle("bogus"));
  TEST_ASSERT_EQUAL(STYLE_FAST_BLINK, parseStyle("fast_blink"));
  TEST_ASSERT_EQUAL_UINT8(255, styleLevel(STYLE_SIMPLE, 1234));
  TEST_ASSERT_EQUAL_UINT8(255, styleLevel(STYLE_SLOW_BLINK, 999));
  TEST_ASSERT_EQUAL_UINT8(0, styleLevel(STYLE_SLOW_BLINK, 1000));
  TEST_ASSERT_EQUAL_UINT8(0, styleLevel(STYLE_FAST_BLINK, 250));
  TEST_ASSERT_EQUAL_UINT8(255, styleLevel(STYLE_FADE, 1000));
}

void test_notice_duration()
{
  TEST_ASSERT_EQUAL_UINT32(60000, noticeDurationMs(false, 0));
  TEST_ASSERT_EQUAL_UINT32(0, noticeDurationMs(true, 0));
  TEST_ASSERT_EQUAL_UINT32(30000, noticeDurationMs(true, 30));
  TEST_ASSERT_EQUAL_UINT32(3600000, noticeDurationMs(true, 99999));
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_glyphs);
  RUN_TEST(test_bottom_row_drops_unknown_and_keeps_four);
  RUN_TEST(test_bottom_row_right_aligned);
  RUN_TEST(test_top_row_separator);
  RUN_TEST(test_top_row_without_separator);
  RUN_TEST(test_styles);
  RUN_TEST(test_notice_duration);
  RUN_TEST(test_gradient_ends_are_clamped);
  RUN_TEST(test_gradient_interpolates_between_stops);
  RUN_TEST(test_gradient_multiple_stops);
  RUN_TEST(test_parse_utc_offset);
  RUN_TEST(test_parse_utc_offset_rejects_garbage);
  RUN_TEST(test_seconds_of_day);
  RUN_TEST(test_is_night);
  RUN_TEST(test_elapsed_survives_millis_overflow);
  RUN_TEST(test_fade_level_triangle);
  RUN_TEST(test_debouncer_one_event_per_press);
  return UNITY_END();
}
