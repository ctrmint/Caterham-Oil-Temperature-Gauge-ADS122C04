// Track layout (#24): a data page for track days. Labels and plain bars only; each is updated only when its text or length
// changes.
#include "display/Layouts.h"
#include <cstdio>
#include <cstring>

using namespace layout;

namespace {
constexpr int kC = 233;
// Top to bottom: the title, the reading, MAX and the rate, five band rows, the status lines and the LED.
constexpr int kTitleY = 58, kTitleW = 160, kTitleH = 18;            // Montserrat 16
constexpr int kValueY = 80, kValueW = 220, kValueH = 52;            // Montserrat 48
constexpr int kInfoY = 140, kInfoW = 150, kInfoH = 18, kInfoGap = 10;  // MAX to the left of the centre, the rate to the right
constexpr int kRowY0 = 170, kRowPitch = 24, kRowH = 18;             // Montserrat 16 rows
constexpr int kLabelX = 64, kLabelW = 72;                           // right-aligned up to the bar
constexpr int kBarX = kLabelX + kLabelW + 8, kBarW = 190, kBarH = 10, kBarR = 3;
constexpr int kTimeX = kBarX + kBarW + 8, kTimeW = 60;              // left-aligned after the bar
constexpr int kRowsEnd = kRowY0 + (gauge::kBands - 1) * kRowPitch + kRowH;
constexpr int kLinesY = kRowsEnd + 14;
constexpr int kLinesEnd = kLinesY + 2 * lines::kGap + lines::kTextH;
constexpr int kLedReach = led::kD / 2 + led::kGlow;
constexpr int kLedCy = kLinesEnd + 10 + kLedReach;
constexpr uint32_t kTrack = 0x1A242C;  // the empty bar behind each band's bar
constexpr uint32_t kBandColour[gauge::kBands] = {gauge::kColourDarkBlue, gauge::kColourGreen, gauge::kColourAmber,
                                                 gauge::kColourRed, kMute};

constexpr bool on(int x, int y) { return lines::onScreen(x, y); }
static_assert(on(kC - kTitleW / 2, kTitleY) && kTitleY + kTitleH <= kValueY, "the title must fit above the reading");
static_assert(on(kC - kValueW / 2, kValueY) && kValueY + kValueH <= kInfoY, "the reading must fit above MAX and the rate");
static_assert(on(kC - kInfoGap / 2 - kInfoW, kInfoY) && on(kC + kInfoGap / 2 + kInfoW, kInfoY) && kInfoY + kInfoH < kRowY0,
              "MAX and the rate must fit above the rows");
static_assert(on(kLabelX, kRowY0) && on(kTimeX + kTimeW, kRowY0) && on(kLabelX, kRowsEnd) && on(kTimeX + kTimeW, kRowsEnd),
              "the band rows must fit on the screen");
static_assert(kRowH <= kRowPitch && kBarH <= kRowH, "rows must not overlap");
static_assert(lines::blockFits(kLinesY), "Track status lines must fit on the screen");
static_assert(kLedCy - kLedReach > kLinesEnd && on(kC + kLedReach, kLedCy + kLedReach), "the LED must fit below the lines");

lv_obj_t* text(lv_obj_t* parent, int x, int y, int w, int h, const lv_font_t* font, lv_text_align_t align, uint32_t colour) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(l, w, h);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_style_text_align(l, align, 0);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
  return l;
}

lv_obj_t* bar(lv_obj_t* parent, int x, int y, int w, uint32_t colour) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_size(o, w, kBarH);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_radius(o, kBarR, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(colour), 0);
  return o;
}

void setText(lv_obj_t* label, char* buf, size_t size, const char* want) {
  if (std::strncmp(buf, want, size) == 0) return;
  std::snprintf(buf, size, "%s", want);
  lv_label_set_text_static(label, buf);
}

void passPresses(lv_obj_t* obj) {
  const uint32_t count = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_t* child = lv_obj_get_child(obj, static_cast<int32_t>(i));
    lv_obj_remove_flag(child, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    passPresses(child);
  }
}

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_size(o, 466, 466);
  lv_obj_set_pos(o, 0, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  return o;
}
} // namespace

void TrackLayout::build(lv_obj_t* screen, gauge::Unit unit) {
  unit_ = unit;
  root_ = plain(screen);
  lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);

  title_text_ = gauge::titleText(unit);
  title_ = text(root_, kC - kTitleW / 2, kTitleY, kTitleW, kTitleH, &lv_font_montserrat_16, LV_TEXT_ALIGN_CENTER, kMute);
  lv_label_set_text_static(title_, title_text_);
  value_ = text(root_, kC - kValueW / 2, kValueY, kValueW, kValueH, &lv_font_montserrat_48, LV_TEXT_ALIGN_CENTER, kWhite);
  lv_label_set_text_static(value_, value_text_);
  max_ = text(root_, kC - kInfoGap / 2 - kInfoW, kInfoY, kInfoW, kInfoH, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT, kMute);
  lv_label_set_text_static(max_, max_text_);
  rate_ = text(root_, kC + kInfoGap / 2, kInfoY, kInfoW, kInfoH, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT, kWhite);
  lv_label_set_text_static(rate_, rate_text_);

  // A row per band: its label, the empty track, its bar over the track, and its time. Hidden until calibrated.
  rows_ = plain(root_);
  for (int b = 0; b < gauge::kBands; ++b) {
    const int y = kRowY0 + b * kRowPitch;
    gauge::bandLabel(static_cast<gauge::Band>(b), unit, label_text_[b], sizeof(label_text_[b]));
    lv_obj_t* l = text(rows_, kLabelX, y, kLabelW, kRowH, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT, kMute);
    lv_label_set_text_static(l, label_text_[b]);
    bar(rows_, kBarX, y + (kRowH - kBarH) / 2, kBarW, kTrack);
    bars_[b] = bar(rows_, kBarX, y + (kRowH - kBarH) / 2, kBarH, kBandColour[b]);
    lv_obj_add_flag(bars_[b], LV_OBJ_FLAG_HIDDEN);
    bar_px_[b] = 0;
    gauge::durationText(0, time_text_[b], sizeof(time_text_[b]));
    times_[b] = text(rows_, kTimeX, y, kTimeW, kRowH, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT, kWhite);
    lv_label_set_text_static(times_[b], time_text_[b]);
  }
  lv_obj_add_flag(rows_, LV_OBJ_FLAG_HIDDEN);
  rows_shown_ = false;

  lines_.build(root_, kLinesY);
  led_.build(root_, kLedCy);
  lv_obj_remove_flag(root_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  passPresses(root_);
}

void TrackLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  if (f.title != title_text_) { title_text_ = f.title; lv_label_set_text_static(title_, title_text_); }
  // The reading: Classic's text, without its leading blanks, in its colour.
  const char* v = f.cells;
  while (*v == ' ') ++v;
  setText(value_, value_text_, sizeof(value_text_), v);
  if (f.colour != value_colour_) {
    value_colour_ = f.colour;
    lv_obj_set_style_text_color(value_, lv_color_hex(f.colour), 0);
  }
  setText(max_, max_text_, sizeof(max_text_), f.max_text);
  // The rate, with an arrow for its direction, as on Tape; empty when there's no clean 30 s of samples.
  char rate[gauge::kRateTextSize];
  gauge::rateText(f.rate, unit_, rate, sizeof(rate));
  char shown[sizeof(rate_text_)];
  const gauge::RateTrend trend = gauge::rateTrend(f.rate.c_per_min);
  if (rate[0] && trend != gauge::RateTrend::Steady)
    std::snprintf(shown, sizeof(shown), "%s %s", trend == gauge::RateTrend::Rising ? LV_SYMBOL_UP : LV_SYMBOL_DOWN, rate);
  else
    std::snprintf(shown, sizeof(shown), "%s", rate);
  setText(rate_, rate_text_, sizeof(rate_text_), shown);

  // The band rows: only with a calibration, since their labels are temperatures.
  const bool rows = f.scale_visible && f.bands;
  if (rows != rows_shown_) {
    rows_shown_ = rows;
    if (rows) lv_obj_remove_flag(rows_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(rows_, LV_OBJ_FLAG_HIDDEN);
  }
  if (rows) {
    int px[gauge::kBands];
    gauge::bandBars(*f.bands, kBarW, px);
    for (int b = 0; b < gauge::kBands; ++b) {
      if (px[b] != bar_px_[b]) {
        if (px[b] > 0) {
          lv_obj_set_width(bars_[b], px[b]);
          if (bar_px_[b] == 0) lv_obj_remove_flag(bars_[b], LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_obj_add_flag(bars_[b], LV_OBJ_FLAG_HIDDEN);
        }
        bar_px_[b] = px[b];
      }
      char t[gauge::kDurationTextSize];
      gauge::durationText(f.bands->ms(static_cast<gauge::Band>(b)), t, sizeof(t));
      setText(times_[b], time_text_[b], sizeof(time_text_[b]), t);
    }
  }
  lines_.render(f);
  led_.render(f.led);
}
