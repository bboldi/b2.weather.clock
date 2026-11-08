#include <unity.h>
#include <clock_logic.h>
#include <string>

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
  TEST_ASSERT_EQUAL_UINT32(NOTICE_FOREVER, noticeDurationMs(true, -1));
  TEST_ASSERT_EQUAL_UINT32(0, noticeDurationMs(true, -5));
}

void test_status_light_duration()
{
  TEST_ASSERT_EQUAL_UINT32(NOTICE_FOREVER, statusLightDurationMs(false, 0));
  TEST_ASSERT_EQUAL_UINT32(NOTICE_FOREVER, statusLightDurationMs(true, -1));
  TEST_ASSERT_EQUAL_UINT32(0, statusLightDurationMs(true, 0));
  TEST_ASSERT_EQUAL_UINT32(0, statusLightDurationMs(true, -5));
  TEST_ASSERT_EQUAL_UINT32(86400000, statusLightDurationMs(true, 86400));
  TEST_ASSERT_EQUAL_UINT32(172800000, statusLightDurationMs(true, 999999));
}

static std::string joined(const LogRing<16> &r)
{
  const char *a, *b;
  size_t aLen, bLen;
  r.pieces(a, aLen, b, bLen);
  return std::string(a, aLen) + std::string(b, bLen);
}

void test_log_ring_keeps_lines_in_order()
{
  LogRing<16> r;
  r.append("one");
  r.append("two");
  TEST_ASSERT_EQUAL_STRING("one\ntwo\n", joined(r).c_str());
}

void test_log_ring_drops_oldest_and_cut_line()
{
  LogRing<16> r;
  r.append("aaaa");
  r.append("bbbb");
  r.append("cccc");
  r.append("dddd"); // 20 bytes into 16, "aaaa" is cut
  TEST_ASSERT_EQUAL_STRING("bbbb\ncccc\ndddd\n", joined(r).c_str());

  r.append("eeeeeeeeeeeeeee"); // longer than the ring: nothing whole is left
  TEST_ASSERT_EQUAL_STRING("", joined(r).c_str());
}

void test_log_ring_tail()
{
  LogRing<64> r;
  r.append("first line");
  r.append("second");
  r.append("third");
  char out[16];
  r.tail(out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("second\nthird\n", out);
  r.tail(out, 4);
  TEST_ASSERT_EQUAL_STRING("", out);
}

void test_redact_param()
{
  char url[] = "GET http://x/weather?lang=en&appid=0123456789abcdef&lat=1 -> 200";
  redactParam(url, sizeof(url), "appid=");
  TEST_ASSERT_EQUAL_STRING("GET http://x/weather?lang=en&appid=***&lat=1 -> 200", url);

  char none[] = "no key here";
  redactParam(none, sizeof(none), "appid=");
  TEST_ASSERT_EQUAL_STRING("no key here", none);
}

void test_format_stamp()
{
  char s[16];
  formatStamp(s, sizeof(s), true, 13 * 3600 + 42 * 60 + 7, 0);
  TEST_ASSERT_EQUAL_STRING("13:42:07", s);
  formatStamp(s, sizeof(s), false, 0, 12345);
  TEST_ASSERT_EQUAL_STRING("+12.3s", s);
}

void test_repeat_filter()
{
  RepeatFilter<2> f;
  TEST_ASSERT_TRUE(f.allow("err a", 1000, 600000));
  TEST_ASSERT_FALSE(f.allow("err a", 61000, 600000)); // retry a minute later
  TEST_ASSERT_TRUE(f.allow("err b", 61000, 600000));
  TEST_ASSERT_TRUE(f.allow("err a", 602000, 600000)); // window passed
  TEST_ASSERT_TRUE(f.allow("err c", 603000, 600000)); // evicts the oldest slot
}

// digit from the user's numbering: 1 top, 2 upper left, 3 upper right, 4 middle, 5 lower left, 6 lower right, 7 bottom
static const uint8_t USER_TO_IX[] = {0, IX_A, IX_F, IX_B, IX_G, IX_E, IX_C, IX_D};

static uint8_t userMask(const char *segments)
{
  uint8_t m = 0;
  for (const char *p = segments; *p; p++)
    m |= 1 << USER_TO_IX[*p - '0'];
  return m;
}

static void digitOf(const char *segments, Rgb out[7], Rgb color = {255, 255, 255})
{
  uint8_t m = userMask(segments);
  for (int i = 0; i < 7; i++)
    out[i] = (m >> i) & 1 ? color : Rgb{0, 0, 0};
}

void test_slide_up_two_to_three()
{
  Rgb two[7], three[7], out[7];
  digitOf("13457", two);
  digitOf("13467", three);

  transitionFrame(TR_SLIDE_UP, two, three, 0, 0, out);
  TEST_ASSERT_EQUAL_HEX8(userMask("1247"), litMask(out)); // 2 moved up once + top of 3 at the bottom
  transitionFrame(TR_SLIDE_UP, two, three, SLIDE_STEP_MS, 0, out);
  TEST_ASSERT_EQUAL_HEX8(userMask("1467"), litMask(out)); // top of 2 + 3 moved in twice
  TEST_ASSERT_TRUE(transitionFrame(TR_SLIDE_UP, two, three, 2 * SLIDE_STEP_MS, 0, out));
  TEST_ASSERT_EQUAL_HEX8(userMask("13467"), litMask(out));
}

void test_spiral_order_then_fade_in()
{
  Rgb eight[7], one[7], out[7];
  digitOf("1234567", eight);
  digitOf("36", one);

  transitionFrame(TR_SPIRAL, eight, one, 0, 0, out);
  TEST_ASSERT_EQUAL_HEX8(userMask("123567"), litMask(out)); // middle first
  transitionFrame(TR_SPIRAL, eight, one, 3 * SPIRAL_STEP_MS, 0, out);
  TEST_ASSERT_EQUAL_HEX8(userMask("567"), litMask(out)); // 4,2,1,3 gone
  transitionFrame(TR_SPIRAL, eight, one, 6 * SPIRAL_STEP_MS, 0, out);
  TEST_ASSERT_EQUAL_HEX8(0, litMask(out));
  transitionFrame(TR_SPIRAL, eight, one, 7 * SPIRAL_STEP_MS + SPIRAL_FADE_MS / 2, 0, out);
  TEST_ASSERT_EQUAL_HEX8(userMask("36"), litMask(out));
  TEST_ASSERT_UINT8_WITHIN(2, 127, out[IX_B].r); // half way faded in
  TEST_ASSERT_TRUE(transitionFrame(TR_SPIRAL, eight, one, 7 * SPIRAL_STEP_MS + SPIRAL_FADE_MS, 0, out));
}

void test_fade_and_glitch()
{
  Rgb from[7], to[7], out[7];
  digitOf("1", from, {200, 0, 0});
  digitOf("7", to, {0, 0, 200});

  transitionFrame(TR_FADE, from, to, FADE_MS / 2, 0, out);
  TEST_ASSERT_UINT8_WITHIN(2, 100, out[IX_A].r);
  TEST_ASSERT_UINT8_WITHIN(2, 100, out[IX_D].b);
  TEST_ASSERT_TRUE(transitionFrame(TR_FADE, from, to, FADE_MS, 0, out));

  // glitch only uses the new digit's color, and finishes on the new digit
  for (long t = 0; t < (long)GLITCH_MS; t += GLITCH_STEP_MS)
  {
    TEST_ASSERT_FALSE(transitionFrame(TR_GLITCH, from, to, t, 7, out));
    for (int i = 0; i < 7; i++)
      TEST_ASSERT_TRUE(!isLit(out[i]) || out[i].b == 200);
  }
  TEST_ASSERT_TRUE(transitionFrame(TR_GLITCH, from, to, GLITCH_MS, 7, out));
  TEST_ASSERT_EQUAL_HEX8(userMask("7"), litMask(out));
}

void test_transition_not_started_and_none()
{
  Rgb from[7], to[7], out[7];
  digitOf("1", from);
  digitOf("7", to);
  TEST_ASSERT_FALSE(transitionFrame(TR_FADE, from, to, -50, 0, out));
  TEST_ASSERT_EQUAL_HEX8(userMask("1"), litMask(out));
  TEST_ASSERT_TRUE(transitionFrame(TR_NONE, from, to, 0, 0, out));
  TEST_ASSERT_EQUAL_HEX8(userMask("7"), litMask(out));
  TEST_ASSERT_EQUAL(TR_RANDOM, parseTransition("random"));
  TEST_ASSERT_EQUAL(TR_SPIRAL, parseTransition("spiral"));
}

static Gesture feed(Gestures &g, bool down, uint32_t from, uint32_t to)
{
  Gesture result = G_NONE;
  for (uint32_t t = from; t <= to; t += 10)
  {
    Gesture e = g.update(down, t);
    if (e != G_NONE)
      result = e;
  }
  return result;
}

void test_gestures()
{
  Gestures g(1000, 400);
  // click: press, release, then nothing for the double click window
  TEST_ASSERT_EQUAL(G_NONE, feed(g, true, 0, 100));
  TEST_ASSERT_EQUAL(G_NONE, feed(g, false, 110, 400));
  TEST_ASSERT_EQUAL(G_CLICK, feed(g, false, 410, 600));

  // double click
  TEST_ASSERT_EQUAL(G_NONE, feed(g, true, 1000, 1100));
  TEST_ASSERT_EQUAL(G_NONE, feed(g, false, 1110, 1200));
  TEST_ASSERT_EQUAL(G_NONE, feed(g, true, 1210, 1300));
  TEST_ASSERT_EQUAL(G_DOUBLE_CLICK, feed(g, false, 1310, 1320));
  TEST_ASSERT_EQUAL(G_NONE, feed(g, false, 1330, 2000)); // no extra click after it

  // long press fires while held, the release is not a click
  TEST_ASSERT_EQUAL(G_LONG_PRESS, feed(g, true, 3000, 4100));
  TEST_ASSERT_EQUAL(G_NONE, feed(g, false, 4110, 5000));
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_gestures);
  RUN_TEST(test_slide_up_two_to_three);
  RUN_TEST(test_spiral_order_then_fade_in);
  RUN_TEST(test_fade_and_glitch);
  RUN_TEST(test_transition_not_started_and_none);
  RUN_TEST(test_repeat_filter);
  RUN_TEST(test_log_ring_keeps_lines_in_order);
  RUN_TEST(test_log_ring_drops_oldest_and_cut_line);
  RUN_TEST(test_log_ring_tail);
  RUN_TEST(test_redact_param);
  RUN_TEST(test_format_stamp);
  RUN_TEST(test_glyphs);
  RUN_TEST(test_bottom_row_drops_unknown_and_keeps_four);
  RUN_TEST(test_bottom_row_right_aligned);
  RUN_TEST(test_top_row_separator);
  RUN_TEST(test_top_row_without_separator);
  RUN_TEST(test_styles);
  RUN_TEST(test_notice_duration);
  RUN_TEST(test_status_light_duration);
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
