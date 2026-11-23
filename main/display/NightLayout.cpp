// Night layout (#21): dark while all is well, fully lit otherwise. Its own opaque root in kBg (pure black), so the pixels
// around the reading stay off.
#include "display/Layouts.h"
#include <cstdio>
#include <cstring>

using namespace layout;

namespace {
constexpr int kC = 233;  // the screen's centre and radius
constexpr int iabs(int v) { return v < 0 ? -v : v; }
// A centred box w by h around (kC, cy) lies on the round screen, inset by margin (its corners are the farthest points).
constexpr bool boxOnScreen(int w, int cy, int h, int margin) {
  const int dy = iabs(cy - kC) + h / 2 + 1;
  return (w / 2 + 1) * (w / 2 + 1) + dy * dy <= (kC - margin) * (kC - margin);
}

// Quiet: the reading at the centre, the unit under it and the heartbeat dot under that, all dim. Montserrat 48's line is
// 52 px with the digits' ink (about 34 px) centred in it; Montserrat 16's is 18 px.
constexpr int kReadingCy = kC, kReadingW = 160, kReadingH = 52;
constexpr int kUnitCy = kReadingCy + 38, kUnitW = 60, kUnitH = 18;
constexpr int kBeatCy = kUnitCy + 26, kBeatD = 8;
constexpr lv_opa_t kQuietOpa = LV_OPA_50;  // the reading and unit at half brightness
static_assert(kReadingCy + kReadingH / 2 < kUnitCy - kUnitH / 2 && kUnitCy + kUnitH / 2 < kBeatCy - kBeatD / 2,
              "quiet: the reading, the unit and the dot must not overlap");
static_assert(boxOnScreen(kReadingW, kReadingCy, kReadingH, 8) && boxOnScreen(kUnitW, kUnitCy, kUnitH, 8) &&
              boxOnScreen(kBeatD, kBeatCy, kBeatD, 8), "quiet: everything must lie on the screen");

// Alert: Classic's digits centred on the screen, OIL and the unit above them, then the status lines and the LED below.
constexpr int kTitleW = 160, kTitleH = 28, kTitleCy = kC - digits::kGlyphH / 2 - 6 - kTitleH / 2;
constexpr int kLinesY = kC + digits::kGlyphH / 2 + 8;
constexpr int kLinesEnd = kLinesY + 2 * lines::kGap + lines::kTextH;  // the bottom of the demo line
constexpr int kLedReach = led::kD / 2 + led::kGlow;                  // the glow's reach from the LED's centre
constexpr int kLedCy = kLinesEnd + 14 + kLedReach;
static_assert(boxOnScreen(digits::kBlockW, kC, digits::kGlyphH, 8), "alert: the digits must lie on the screen");
static_assert(kTitleCy + kTitleH / 2 < kC - digits::kGlyphH / 2 && boxOnScreen(kTitleW, kTitleCy, kTitleH, 8),
              "alert: the title must sit above the digits, on the screen");
static_assert(kLinesY > kC + digits::kGlyphH / 2 && lines::blockFits(kLinesY),
              "alert: the status lines must sit below the digits, on the screen");
static_assert(kLedCy - kLedReach > kLinesEnd && boxOnScreen(2 * kLedReach, kLedCy, 2 * kLedReach, 4),
              "alert: the LED and its glow must sit below the status lines, on the screen");

lv_obj_t* group(lv_obj_t* parent) {
  lv_obj_t* g = lv_obj_create(parent);
  lv_obj_set_size(g, 466, 466);
  lv_obj_set_pos(g, 0, 0);
  lv_obj_set_style_bg_opa(g, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g, 0, 0);
  lv_obj_set_style_pad_all(g, 0, 0);
  return g;
}

lv_obj_t* label(lv_obj_t* parent, int w, int cy, int h, const lv_font_t* font, uint32_t colour) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(l, w, h);
  lv_obj_set_pos(l, kC - w / 2, cy - h / 2);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
  return l;
}

// Presses must reach the screen: nothing under root may be clickable or scrollable.
void passPresses(lv_obj_t* obj) {
  const uint32_t count = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_t* child = lv_obj_get_child(obj, static_cast<int32_t>(i));
    lv_obj_remove_flag(child, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    passPresses(child);
  }
}
// The Wi-Fi panel (#34) sits above the status lines and LED, so it never hides an alarm.
static_assert(panel::kBottom < kLinesY && panel::kBottom < kLedCy - kLedReach,
              "the Wi-Fi panel must clear the status lines and LED");
} // namespace

void NightLayout::build(lv_obj_t* screen, gauge::Unit unit) {
  root_ = lv_obj_create(screen);
  lv_obj_set_size(root_, 466, 466);
  lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(root_, lv_color_hex(kBg), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_radius(root_, 0, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_remove_flag(root_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

  quiet_ = group(root_);
  reading_ = label(quiet_, kReadingW, kReadingCy, kReadingH, &lv_font_montserrat_48, kMute);
  lv_obj_set_style_text_opa(reading_, kQuietOpa, 0);
  lv_label_set_text_static(reading_, reading_text_);
  unit_text_ = gauge::unitLabel(unit);
  unit_ = label(quiet_, kUnitW, kUnitCy, kUnitH, &lv_font_montserrat_16, kMute);
  lv_obj_set_style_text_opa(unit_, kQuietOpa, 0);
  lv_label_set_text_static(unit_, unit_text_);
  beat_ = lv_obj_create(quiet_);
  lv_obj_set_size(beat_, kBeatD, kBeatD);
  lv_obj_set_pos(beat_, kC - kBeatD / 2, kBeatCy - kBeatD / 2);
  lv_obj_set_style_radius(beat_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(beat_, 0, 0);
  lv_obj_set_style_pad_all(beat_, 0, 0);
  lv_obj_set_style_bg_color(beat_, lv_color_hex(kWhite), 0);
  lv_obj_set_style_bg_opa(beat_, gauge::nightBeat(0), 0);

  alert_ = group(root_);
  title_text_ = gauge::titleText(unit);
  title_ = label(alert_, kTitleW, kTitleCy, kTitleH, &lv_font_montserrat_24, kWhite);
  lv_label_set_text_static(title_, title_text_);
  digits_.build(alert_);
  lines_.build(alert_, kLinesY);
  led_.build(alert_, kLedCy);

  // Start lit, as the alert state, until the first update says otherwise.
  quiet_shown_ = false;
  lv_obj_add_flag(quiet_, LV_OBJ_FLAG_HIDDEN);
  passPresses(root_);
}

// Both states are kept current, so switching never shows a stale value.
void NightLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  if (f.night_quiet != quiet_shown_) {
    quiet_shown_ = f.night_quiet;
    lv_obj_add_flag(quiet_shown_ ? alert_ : quiet_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(quiet_shown_ ? quiet_ : alert_, LV_OBJ_FLAG_HIDDEN);
  }
  // Quiet: the reading without its leading blanks, in its colour; the heartbeat steps once per update.
  const char* text = f.cells;
  while (*text == ' ') ++text;
  if (std::strncmp(reading_text_, text, sizeof(reading_text_)) != 0) {
    std::snprintf(reading_text_, sizeof(reading_text_), "%s", text);
    lv_label_set_text_static(reading_, reading_text_);
  }
  if (f.colour != colour_) { colour_ = f.colour; lv_obj_set_style_text_color(reading_, lv_color_hex(colour_), 0); }
  if (f.unit != unit_text_) { unit_text_ = f.unit; lv_label_set_text_static(unit_, unit_text_); }
  ++frame_;
  lv_obj_set_style_bg_opa(beat_, gauge::nightBeat(frame_), 0);
  // Alert: as Classic, with the shared status lines and LED.
  if (f.title != title_text_) { title_text_ = f.title; lv_label_set_text_static(title_, title_text_); }
  digits_.render(f.cells, f.colour);
  lines_.render(f);
  led_.render(f.led);
}
