#include "display/Layouts.h"
#include "Config.h"
#include "core/Calibration.h"
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace layout;

// Moving tape (#15): a scale that scrolls past a fixed pointer, like an aircraft's speed tape. 40 degC are visible, with
// ticks every degree, numbers every 10, a strip down the right edge in each temperature's graduated colour, amber and
// red bars at the alarm thresholds, a hatched region beyond the calibration table, and a marker at the session maximum.
// The reading sits in a window beside the pointer, with the rate of change under it.
//
// One object draws the tape in a single pass. Its state (the centre, the maximum) is written by render() and the easing
// timer, both under the display lock, and read by the draw callback in the LVGL task, which holds the same lock.
namespace {
constexpr int kTapeX = 92, kTapeY = 113, kTapeW = 110, kTapeH = 240;
constexpr int kCentreY = kTapeY + kTapeH / 2;  // the pointer's line
constexpr int kStripW = 10;                     // the colour strip down the tape's right edge
constexpr int kStripX = kTapeX + kTapeW - kStripW;
constexpr int kTickMinor = 6, kTickMid = 11, kTickMajor = 17, kTickW = 2;
constexpr int kNumRight = kStripX - kTickMajor - 5, kNumH = 28;  // tape numbers, right-aligned, Montserrat 24
constexpr int kThresholdH = 4, kMaxMarkW = 10, kMaxMarkHalfH = 6;
constexpr int kPointerX0 = kTapeX + kTapeW + 2, kPointerX1 = kPointerX0 + 18, kPointerHalfH = 10;  // points left
constexpr int kObjW = kPointerX1 - kTapeX + 1;  // the drawing object spans the tape and the pointer
constexpr int kWinX = kPointerX1, kWinW = 150, kWinH = 72, kWinY = kCentreY - kWinH / 2;
constexpr int kValueH = 56;
constexpr int kRateX = kWinX - 20, kRateW = 190, kRateY = kWinY + kWinH + 10, kRateH = 28;
constexpr int kTitleY = 64, kTitleW = 160;
constexpr int kStatusY = 380;
constexpr int kMaxNumbers = 64;  // numbers every 10 display units across the calibration table
constexpr uint32_t kTimerMs = 30;
constexpr float kEase = 0.3f;    // the share of the remaining distance moved each frame: it never overshoots
constexpr float kSettledC = 0.01f;
constexpr uint32_t kTapeBg = kBg, kTick = 0xC9D3DA, kHatch = 0x26323C, kWinBorder = 0x5A6872;

static_assert(lines::onScreen(kTapeX, kTapeY) && lines::onScreen(kTapeX, kTapeY + kTapeH), "tape must fit on the screen");
static_assert(lines::onScreen(kWinX + kWinW, kWinY) && lines::onScreen(kWinX + kWinW, kWinY + kWinH),
              "window must fit on the screen");
static_assert(lines::onScreen(kRateX + kRateW, kRateY + kRateH), "rate line must fit on the screen");
static_assert(kNumRight > kTapeX + 40, "tape numbers need room for three digits and a sign");
static_assert(lines::fits(kTitleW, kTitleY) && kTitleY + lines::kTextH < kTapeY, "title must fit above the tape");
static_assert(kTapeY + kTapeH < kStatusY && kRateY + kRateH < kStatusY, "tape and rate must sit above the status lines");
static_assert(lines::blockFits(kStatusY), "Tape status lines must fit on the round screen");

// Shared with the draw callback; all under the display lock.
bool tape_on = false;
float shown_c = 0.f, target_c = 0.f;  // the tape's centre as drawn, and where it's easing to
bool max_on = false;
float max_c = 0.f;
gauge::Unit tape_unit = gauge::Unit::Celsius;
bool table_known = false;
float table_lo_u = 0.f, table_hi_u = 0.f;  // the calibration table's ends, in display units
char number_text[kMaxNumbers][8];          // numbers at multiples of 10 display units, from number_base
int number_base = 0, number_count = 0;

float toU(float c) { return gauge::toUnit(c, tape_unit); }
float spanU() { return tape_unit == gauge::Unit::Fahrenheit ? gauge::kTapeSpanC * 1.8f : gauge::kTapeSpanC; }
int floorDiv10(int v) { return v >= 0 ? v / 10 : -((-v + 9) / 10); }

void fill(lv_layer_t* layer, int x0, int y0, int x1, int y1, uint32_t colour) {
  if (x1 < x0 || y1 < y0) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = lv_color_hex(colour);
  d.bg_opa = LV_OPA_COVER;
  const lv_area_t a = {x0, y0, x1, y1};
  lv_draw_rect(layer, &d, &a);
}

void line(lv_layer_t* layer, int x0, int y0, int x1, int y1, int w, uint32_t colour) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = lv_color_hex(colour);
  d.width = w;
  d.p1 = {static_cast<lv_value_precise_t>(x0), static_cast<lv_value_precise_t>(y0)};
  d.p2 = {static_cast<lv_value_precise_t>(x1), static_cast<lv_value_precise_t>(y1)};
  lv_draw_line(layer, &d);
}

void triangle(lv_layer_t* layer, int ax, int ay, int bx, int by, int cx, int cy, uint32_t colour) {
  lv_draw_triangle_dsc_t d;
  lv_draw_triangle_dsc_init(&d);
  d.color = lv_color_hex(colour);
  d.opa = LV_OPA_COVER;
  d.p[0] = {static_cast<lv_value_precise_t>(ax), static_cast<lv_value_precise_t>(ay)};
  d.p[1] = {static_cast<lv_value_precise_t>(bx), static_cast<lv_value_precise_t>(by)};
  d.p[2] = {static_cast<lv_value_precise_t>(cx), static_cast<lv_value_precise_t>(cy)};
  lv_draw_triangle(layer, &d);
}

// 45-degree hatching between rows y0 and y1 of the tape: no calibration there.
void beyondTable(lv_layer_t* layer, int ox, int oy, int y0, int y1) {
  if (y0 < kTapeY) y0 = kTapeY;
  if (y1 > kTapeY + kTapeH - 1) y1 = kTapeY + kTapeH - 1;
  if (y1 < y0) return;
  // Lines x - y = k, cut to the region.
  for (int k = kTapeX - y1; k <= kTapeX + kTapeW - y0; k += 10) {
    const int xs = (kTapeX > y0 + k) ? kTapeX : y0 + k;
    const int xe = (kTapeX + kTapeW - 1 < y1 + k) ? kTapeX + kTapeW - 1 : y1 + k;
    if (xs < xe) line(layer, xs + ox, xs - k + oy, xe + ox, xe - k + oy, 2, kHatch);
  }
}

void onDrawTape(lv_event_t* e) {
  if (!tape_on) return;
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t area;
  lv_obj_get_coords(lv_event_get_target_obj(e), &area);
  const int ox = area.x1 - kTapeX, oy = area.y1 - kTapeY;  // layout pixels to screen pixels
  const float span = spanU(), pxu = kTapeH / span, centre = toU(shown_c);
  auto yOf = [&](float u) { return static_cast<int>(lroundf(kCentreY + gauge::tapeOffsetPx(u, centre, pxu))); };

  fill(layer, kTapeX + ox, kTapeY + oy, kTapeX + kTapeW - 1 + ox, kTapeY + kTapeH - 1 + oy, kTapeBg);
  if (table_known) {
    beyondTable(layer, ox, oy, kTapeY, yOf(table_hi_u) - 1);
    beyondTable(layer, ox, oy, yOf(table_lo_u) + 1, kTapeY + kTapeH - 1);
  }

  // The colour strip, one band per display unit, inside the table only.
  const int lo = static_cast<int>(std::floor(centre - span / 2)) - 1, hi = static_cast<int>(std::ceil(centre + span / 2)) + 1;
  for (int u = lo; u <= hi; ++u) {
    if (!table_known || u < table_lo_u || u + 1 > table_hi_u + 0.001f) continue;
    int y0 = yOf(static_cast<float>(u + 1)), y1 = yOf(static_cast<float>(u)) - 1;
    if (y0 < kTapeY) y0 = kTapeY;
    if (y1 > kTapeY + kTapeH - 1) y1 = kTapeY + kTapeH - 1;
    const float mid = static_cast<float>(u) + 0.5f;
    const float c = tape_unit == gauge::Unit::Fahrenheit ? (mid - 32.f) / 1.8f : mid;
    fill(layer, kStripX + ox, y0 + oy, kTapeX + kTapeW - 1 + ox, y1 + oy, gauge::gradientColour(c));
  }

  // Threshold bars across the tape, from the alarm settings.
  const float thresholds[2] = {config::kAmberThresholdC, config::kRedThresholdC};
  const uint32_t colours[2] = {kAmber, kRed};
  for (int i = 0; i < 2; ++i) {
    const int y = yOf(toU(thresholds[i]));
    if (y < kTapeY || y > kTapeY + kTapeH - 1) continue;
    fill(layer, kTapeX + ox, y - kThresholdH / 2 + oy, kStripX - 1 + ox, y + kThresholdH / 2 - 1 + oy, colours[i]);
  }

  // Ticks every display unit and numbers every 10, inside the table only.
  lv_draw_label_dsc_t num;
  lv_draw_label_dsc_init(&num);
  num.font = &lv_font_montserrat_24;
  num.color = lv_color_hex(kWhite);
  num.align = LV_TEXT_ALIGN_RIGHT;
  for (int u = lo + 1; u < hi; ++u) {
    if (!table_known || u < table_lo_u || u > table_hi_u) continue;
    const int y = yOf(static_cast<float>(u));
    if (y < kTapeY - kNumH || y > kTapeY + kTapeH + kNumH) continue;
    const int m10 = ((u % 10) + 10) % 10, m5 = ((u % 5) + 5) % 5;
    const int len = m10 == 0 ? kTickMajor : (m5 == 0 ? kTickMid : kTickMinor);
    line(layer, kStripX - len + ox, y + oy, kStripX - 1 + ox, y + oy, kTickW, kTick);
    const int idx = m10 == 0 ? floorDiv10(u) - number_base : -1;
    if (idx >= 0 && idx < number_count) {
      num.text = number_text[idx];
      const lv_area_t a = {kTapeX + 4 + ox, y - kNumH / 2 + oy, kNumRight + ox, y + kNumH / 2 + oy};
      lv_draw_label(layer, &num, &a);
    }
  }

  // The session maximum: a small marker on the tape's left edge, pointing in.
  if (max_on) {
    const int y = yOf(toU(max_c));
    if (y >= kTapeY && y <= kTapeY + kTapeH - 1)
      triangle(layer, kTapeX + ox, y - kMaxMarkHalfH + oy, kTapeX + ox, y + kMaxMarkHalfH + oy, kTapeX + kMaxMarkW + ox, y + oy,
               kWhite);
  }

  // The fixed pointer: a line across the strip and a triangle pointing at the tape.
  line(layer, kStripX + ox, kCentreY + oy, kTapeX + kTapeW - 1 + ox, kCentreY + oy, 2, kWhite);
  triangle(layer, kPointerX0 + ox, kCentreY + oy, kPointerX1 + ox, kCentreY - kPointerHalfH + oy, kPointerX1 + ox,
           kCentreY + kPointerHalfH + oy, kWhite);
}

lv_obj_t* label(lv_obj_t* parent, int x, int y, int w, int h, const lv_font_t* font, lv_text_align_t align, uint32_t colour) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(l, w, h);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_align(l, align, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
  return l;
}
// The Wi-Fi panel (#34) sits above the status lines and LED, so it never hides an alarm.
static_assert(panel::kBottom < kStatusY,
              "the Wi-Fi panel must clear the status lines and LED");
} // namespace

void TapeLayout::build(lv_obj_t* screen, gauge::Unit unit) {
  unit_ = tape_unit = unit;
  // The table's ends, and the tape's numbers (multiples of 10 display units inside the table), worked out once.
  table_known = gauge::calibrationValid();
  number_base = number_count = 0;
  if (table_known) {
    table_lo_u = toU(gauge::calibrationMinC());
    table_hi_u = toU(gauge::calibrationMaxC());
    number_base = floorDiv10(static_cast<int>(std::ceil(table_lo_u)) + 9);
    for (int v = number_base * 10; v <= table_hi_u && number_count < kMaxNumbers; v += 10)
      std::snprintf(number_text[number_count++], sizeof(number_text[0]), "%d", v);
  }

  root_ = lv_obj_create(screen);
  lv_obj_set_size(root_, 466, 466);
  lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);

  title_ = label(root_, lines::kCentre - kTitleW / 2, kTitleY, kTitleW, lines::kTextH, &lv_font_montserrat_16,
                 LV_TEXT_ALIGN_CENTER, kWhite);
  title_text_ = gauge::titleText(unit);
  lv_label_set_text_static(title_, title_text_);

  tape_ = lv_obj_create(root_);
  lv_obj_set_size(tape_, kObjW, kTapeH);
  lv_obj_set_pos(tape_, kTapeX, kTapeY);
  lv_obj_set_style_bg_opa(tape_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tape_, 0, 0);
  lv_obj_set_style_pad_all(tape_, 0, 0);
  lv_obj_set_style_radius(tape_, 0, 0);
  lv_obj_add_event_cb(tape_, &onDrawTape, LV_EVENT_DRAW_MAIN, nullptr);
  lv_obj_add_flag(tape_, LV_OBJ_FLAG_HIDDEN);
  tape_on = tape_shown_ = false;
  max_on = false;

  lv_obj_t* window = lv_obj_create(root_);
  lv_obj_set_size(window, kWinW, kWinH);
  lv_obj_set_pos(window, kWinX, kWinY);
  lv_obj_set_style_bg_color(window, lv_color_hex(kTapeBg), 0);
  lv_obj_set_style_bg_opa(window, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(window, 2, 0);
  lv_obj_set_style_border_color(window, lv_color_hex(kWinBorder), 0);
  lv_obj_set_style_radius(window, 8, 0);
  lv_obj_set_style_pad_all(window, 0, 0);
  value_ = label(root_, kWinX + 8, kCentreY - kValueH / 2, kWinW - 20, kValueH, &lv_font_montserrat_48,
                 LV_TEXT_ALIGN_RIGHT, kAmber);
  std::snprintf(value_text_, sizeof(value_text_), "--");
  lv_label_set_text_static(value_, value_text_);
  value_colour_ = kAmber;

  rate_ = label(root_, kRateX, kRateY, kRateW, kRateH, &lv_font_montserrat_24, LV_TEXT_ALIGN_CENTER, kWhite);
  rate_text_[0] = '\0';
  lv_label_set_text_static(rate_, rate_text_);
  lv_obj_add_flag(rate_, LV_OBJ_FLAG_HIDDEN);

  lines_.build(root_, kStatusY);
  timer_ = lv_timer_create(&TapeLayout::onTimer, kTimerMs, this);
  lv_timer_pause(timer_);

  // Presses must reach the screen: nothing may be clickable or scrollable.
  const lv_obj_flag_t off = static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, off);
  const uint32_t count = lv_obj_get_child_count(root_);
  for (uint32_t i = 0; i < count; ++i) lv_obj_remove_flag(lv_obj_get_child(root_, static_cast<int32_t>(i)), off);
}

void TapeLayout::onTimer(lv_timer_t* t) {
  TapeLayout* self = static_cast<TapeLayout*>(lv_timer_get_user_data(t));
  if (!self || !self->tape_) return;
  const float d = target_c - shown_c;
  if (std::fabs(d) < kSettledC) {
    shown_c = target_c;
    lv_timer_pause(t);
  } else {
    shown_c += d * kEase;
  }
  lv_obj_invalidate(self->tape_);
}

void TapeLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  lines_.render(f);
  if (f.title != title_text_) { title_text_ = f.title; lv_label_set_text_static(title_, f.title); }

  // The window: the same reading text as Classic (right-aligned, "--" on a fault, ohms when uncalibrated).
  if (std::strncmp(value_text_, f.cells, sizeof(value_text_)) != 0) {
    std::snprintf(value_text_, sizeof(value_text_), "%s", f.cells);
    lv_label_set_text_static(value_, value_text_);
  }
  if (f.colour != value_colour_) {
    value_colour_ = f.colour;
    lv_obj_set_style_text_color(value_, lv_color_hex(f.colour), 0);
  }

  // The rate, with an arrow for its direction; hidden when there's no clean 30 s of samples.
  char text[gauge::kRateTextSize];
  gauge::rateText(f.rate, unit_, text, sizeof(text));
  char shown[sizeof(rate_text_)];
  const gauge::RateTrend trend = gauge::rateTrend(f.rate.c_per_min);
  if (text[0] && trend != gauge::RateTrend::Steady)
    std::snprintf(shown, sizeof(shown), "%s %s", trend == gauge::RateTrend::Rising ? LV_SYMBOL_UP : LV_SYMBOL_DOWN, text);
  else
    std::snprintf(shown, sizeof(shown), "%s", text);
  if (std::strncmp(rate_text_, shown, sizeof(rate_text_)) != 0) {
    const bool was = rate_text_[0] != '\0';
    std::snprintf(rate_text_, sizeof(rate_text_), "%s", shown);
    lv_label_set_text_static(rate_, rate_text_);
    if (was != (rate_text_[0] != '\0')) {
      if (rate_text_[0]) lv_obj_remove_flag(rate_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(rate_, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // The session maximum's marker.
  if (f.max_shown != max_on || (f.max_shown && f.max_c != max_c)) {
    max_on = f.max_shown;
    max_c = f.max_c;
    if (tape_shown_) lv_obj_invalidate(tape_);
  }

  // The tape: hidden without a temperature; on appearing it goes straight to the reading, with no scroll.
  if (!f.tape.visible) {
    if (tape_shown_) {
      lv_obj_add_flag(tape_, LV_OBJ_FLAG_HIDDEN);
      lv_timer_pause(timer_);
      tape_on = tape_shown_ = false;
    }
    return;
  }
  target_c = f.tape.centre_c;
  if (!tape_shown_) {
    shown_c = target_c;
    tape_on = tape_shown_ = true;
    lv_obj_remove_flag(tape_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(tape_);
  } else if (target_c != shown_c) {
    lv_timer_resume(timer_);
  }
}
