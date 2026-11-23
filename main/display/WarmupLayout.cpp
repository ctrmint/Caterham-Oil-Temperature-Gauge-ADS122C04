// Warm-up layout (#27): progress to operating temperature. The ring is an lv_arc like Classic's; the reading is Classic's
// digits (ReadingDigits); the warm-up line comes from warmupText(), with Tape's rate after it while warming.
#include "display/Layouts.h"
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace layout;

namespace {
constexpr int kC = 233;
// The ring, as Classic's arc: 456 px across, 14 px wide, so its inner radius is 214 px; open at the bottom.
constexpr int kArcSize = 456, kArcWidth = 14, kArcRange = 1000;
constexpr int kInner = (kArcSize - 2 * kArcWidth) / 2, kClear = kInner - 4;
constexpr uint32_t kTrack = 0x22313A;  // the ring's dim track, as Classic's
// Top to bottom inside the ring: the title, the digits (centred), the status lines and the warm-up line.
constexpr int kTitleW = 160, kTitleH = 28, kTitleY = kC - digits::kGlyphH / 2 - 6 - kTitleH;
constexpr int kLinesY = kC + digits::kGlyphH / 2 + 8;
constexpr int kLineY = kLinesY + 3 * lines::kGap, kLineW = 260, kLineH = 18;  // after the status, footer and demo lines

constexpr int iabs(int v) { return v < 0 ? -v : v; }
// A box w wide centred across the screen, from y0 to y1, lies inside the ring (its corners are the farthest points).
constexpr bool inside(int w, int y0, int y1) {
  const int dy = iabs(y0 - kC) > iabs(y1 - kC) ? iabs(y0 - kC) : iabs(y1 - kC);
  return (w / 2) * (w / 2) + dy * dy < kClear * kClear;
}
static_assert(inside(digits::kBlockW, kC - digits::kGlyphH / 2, kC + digits::kGlyphH / 2), "the digits must fit in the ring");
static_assert(inside(kTitleW, kTitleY, kTitleY + kTitleH) && kTitleY + kTitleH < kC - digits::kGlyphH / 2,
              "the title must fit above the digits, inside the ring");
static_assert(kLinesY > kC + digits::kGlyphH / 2 && inside(lines::kStatusW, kLinesY, kLinesY + 2 * lines::kGap + lines::kTextH),
              "the status lines must fit below the digits, inside the ring");
static_assert(kLineY >= kLinesY + 2 * lines::kGap + lines::kTextH && inside(kLineW, kLineY, kLineY + kLineH),
              "the warm-up line must fit below the status lines, inside the ring");

lv_obj_t* label(lv_obj_t* parent, int w, int y, int h, const lv_font_t* font, uint32_t colour) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(l, w, h);
  lv_obj_set_pos(l, kC - w / 2, y);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
  return l;
}
} // namespace

void WarmupLayout::build(lv_obj_t* screen, gauge::Unit unit) {
  unit_ = unit;
  root_ = lv_obj_create(screen);
  lv_obj_set_size(root_, 466, 466);
  lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);

  arc_ = lv_arc_create(root_);
  lv_obj_set_size(arc_, kArcSize, kArcSize);
  lv_obj_align(arc_, LV_ALIGN_CENTER, 0, 0);
  lv_arc_set_range(arc_, 0, kArcRange);
  lv_arc_set_rotation(arc_, 135);
  lv_arc_set_bg_angles(arc_, 0, 270);
  lv_arc_set_value(arc_, 0);
  lv_obj_set_style_arc_width(arc_, kArcWidth, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc_, kArcWidth, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc_, lv_color_hex(kTrack), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc_, lv_color_hex(kGreen), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(arc_, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_remove_style(arc_, nullptr, LV_PART_KNOB);
  // Hidden until a reading puts it somewhere: no ring is never mistaken for cold oil.
  lv_obj_add_flag(arc_, LV_OBJ_FLAG_HIDDEN);
  ring_shown_ = false;
  arc_value_ = -1;

  title_text_ = gauge::titleText(unit);
  title_ = label(root_, kTitleW, kTitleY, kTitleH, &lv_font_montserrat_24, kWhite);
  lv_label_set_text_static(title_, title_text_);
  digits_.build(root_);
  lines_.build(root_, kLinesY);
  line_ = label(root_, kLineW, kLineY, kLineH, &lv_font_montserrat_16, kWhite);
  lv_label_set_text_static(line_, line_text_);

  // Presses must reach the screen: no child may be clickable or scrollable.
  const lv_obj_flag_t off = static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, off);
  const uint32_t count = lv_obj_get_child_count(root_);
  for (uint32_t i = 0; i < count; ++i) lv_obj_remove_flag(lv_obj_get_child(root_, static_cast<int32_t>(i)), off);
}

void WarmupLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  // The ring: hidden (not empty) without a reading to place it.
  if (f.warmup_ring.visible != ring_shown_) {
    ring_shown_ = f.warmup_ring.visible;
    if (ring_shown_) lv_obj_remove_flag(arc_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(arc_, LV_OBJ_FLAG_HIDDEN);
  }
  if (ring_shown_) {
    const int value = static_cast<int>(lroundf(f.warmup_ring.fraction * kArcRange));
    if (value != arc_value_) { arc_value_ = value; lv_arc_set_value(arc_, value); }
    if (f.colour != arc_colour_) {
      arc_colour_ = f.colour;
      lv_obj_set_style_arc_color(arc_, lv_color_hex(f.colour), LV_PART_INDICATOR);
    }
  }
  if (f.title != title_text_) { title_text_ = f.title; lv_label_set_text_static(title_, title_text_); }
  digits_.render(f.cells, f.colour);
  lines_.render(f);

  // The warm-up line: the time, then the rate (with an arrow, as on Tape) while still warming.
  char line[sizeof(line_text_)];
  char rate[gauge::kRateTextSize];
  gauge::rateText(f.rate, unit_, rate, sizeof(rate));
  const gauge::RateTrend trend = gauge::rateTrend(f.rate.c_per_min);
  if (!f.warm && f.warmup_text[0] && rate[0] && trend != gauge::RateTrend::Steady)
    std::snprintf(line, sizeof(line), "%s    %s %s", f.warmup_text, trend == gauge::RateTrend::Rising ? LV_SYMBOL_UP : LV_SYMBOL_DOWN, rate);
  else if (!f.warm && f.warmup_text[0] && rate[0])
    std::snprintf(line, sizeof(line), "%s    %s", f.warmup_text, rate);
  else
    std::snprintf(line, sizeof(line), "%s", f.warmup_text);
  if (std::strncmp(line_text_, line, sizeof(line_text_)) != 0) {
    std::snprintf(line_text_, sizeof(line_text_), "%s", line);
    lv_label_set_text_static(line_, line_text_);
  }
}
