#include "display/Layouts.h"
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace layout;

namespace {
constexpr uint32_t kDim = 0x22313A;  // unlit segment, same as the Classic track
constexpr float kPi = 3.14159265f;
constexpr float kStartDeg = 135.0f, kSweepDeg = 270.0f;  // as the Classic arc: start lower left, open at the bottom
constexpr int kRingInner = 198, kRingOuter = 226, kSegWidth = 16;
constexpr int kLabelRadius = 168, kLabelW = 36, kLabelH = 16;
constexpr int kDigitRight = sweep::kDigitCx + sweep::kDigitW / 2;
constexpr int kDigitBottom = sweep::kDigitCy + sweep::kDigitH / 2;

// Build-once tables in static storage (one SweepLayout instance), so they stay off the 8 KB main-task stack.
// lv_line keeps the points pointer, so pts must outlive the lines.
lv_point_precise_t pts[gauge::kSweepSegments][2];
lv_obj_t* seg[gauge::kSweepSegments];
uint32_t seg_colour[gauge::kSweepSegments];
lv_obj_t* label[sweep::kMaxLabels];
char label_text[sweep::kMaxLabels][8];
char footer_text[gauge::kFooterTextSize];

int px(float v) { return static_cast<int>(lroundf(v)); }

lv_obj_t* small_label(lv_obj_t* parent, int w, int x, int y, lv_text_align_t align, uint32_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(l, w, 16);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_style_text_align(l, align, 0);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  return l;
}
} // namespace

void SweepLayout::build(lv_obj_t* screen, gauge::Unit unit) {
  root_ = lv_obj_create(screen);
  lv_obj_set_size(root_, 466, 466);
  lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);

  // Radial line per segment; each object's box is just its own segment.
  const float pitch = kSweepDeg / gauge::kSweepSegments;
  for (int i = 0; i < gauge::kSweepSegments; ++i) {
    const float a = (kStartDeg + (i + 0.5f) * pitch) * kPi / 180.0f;
    const int x0 = sweep::kCentre + px(kRingInner * cosf(a)), y0 = sweep::kCentre + px(kRingInner * sinf(a));
    const int x1 = sweep::kCentre + px(kRingOuter * cosf(a)), y1 = sweep::kCentre + px(kRingOuter * sinf(a));
    const int mx = x0 < x1 ? x0 : x1, my = y0 < y1 ? y0 : y1;
    pts[i][0] = {static_cast<lv_value_precise_t>(x0 - mx), static_cast<lv_value_precise_t>(y0 - my)};
    pts[i][1] = {static_cast<lv_value_precise_t>(x1 - mx), static_cast<lv_value_precise_t>(y1 - my)};
    lv_obj_t* s = lv_line_create(root_);
    lv_line_set_points(s, pts[i], 2);
    lv_obj_set_pos(s, mx, my);
    lv_obj_set_style_line_width(s, kSegWidth, 0);
    lv_obj_set_style_line_rounded(s, false, 0);
    lv_obj_set_style_line_color(s, lv_color_hex(kDim), 0);
    seg[i] = s;
    seg_colour[i] = kDim;
  }

  // Scale numbers inside the ring, at the angle of their value. Hidden until calibrated.
  const gauge::SweepScale sc = gauge::sweepScale(unit);
  label_count_ = gauge::scaleLabelCount(unit);
  if (label_count_ > sweep::kMaxLabels) label_count_ = sweep::kMaxLabels;
  labels_shown_ = gauge::scaleLabelsVisible();
  for (int i = 0; i < label_count_; ++i) {
    const float v = static_cast<float>(gauge::scaleLabel(unit, i));
    const float a = (kStartDeg + kSweepDeg * (v - sc.min) / (sc.max - sc.min)) * kPi / 180.0f;
    std::snprintf(label_text[i], sizeof(label_text[i]), "%d", gauge::scaleLabel(unit, i));
    const int cx = sweep::kCentre + px(kLabelRadius * cosf(a)), cy = sweep::kCentre + px(kLabelRadius * sinf(a));
    lv_obj_t* l = small_label(root_, kLabelW, cx - kLabelW / 2, cy - kLabelH / 2, LV_TEXT_ALIGN_CENTER, kMute);
    lv_label_set_text_static(l, label_text[i]);
    if (!labels_shown_) lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    label[i] = l;
  }

  // Unit above OIL TEMP, lower right of the reserved digit area (the digit area itself stays empty for T11).
  unit_ = small_label(root_, 70, kDigitRight - 70, kDigitBottom + 6, LV_TEXT_ALIGN_RIGHT, kMute);
  title_ = small_label(root_, 90, kDigitRight - 90, kDigitBottom + 24, LV_TEXT_ALIGN_RIGHT, kWhite);
  // Status, footer and demo banner in the open bottom of the ring.
  lv_label_set_text_static(unit_, gauge::unitLabel(unit));
  lv_label_set_text_static(title_, "OIL TEMP");
  status_ = small_label(root_, 220, sweep::kCentre - 110, 300, LV_TEXT_ALIGN_CENTER, kAmber);
  lv_label_set_text_static(status_, "SENSOR CHECK");
  footer_ = small_label(root_, 220, sweep::kCentre - 110, 320, LV_TEXT_ALIGN_CENTER, kMute);
  demo_ = small_label(root_, 180, sweep::kCentre - 90, 340, LV_TEXT_ALIGN_CENTER, kRed);
  lv_label_set_text_static(demo_, "DEMO ONLY - NOT REAL");
  lv_obj_add_flag(demo_, LV_OBJ_FLAG_HIDDEN);
  std::snprintf(footer_text, sizeof(footer_text), "NOT CALIBRATED");
  lv_label_set_text_static(footer_, footer_text);
  status_colour_ = kAmber;

  // Presses must reach the screen: nothing may be clickable or scrollable.
  const lv_obj_flag_t off = static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, off);
  const uint32_t count = lv_obj_get_child_count(root_);
  for (uint32_t i = 0; i < count; ++i) lv_obj_remove_flag(lv_obj_get_child(root_, static_cast<int32_t>(i)), off);
}

void SweepLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  for (int i = 0; i < gauge::kSweepSegments; ++i) {
    const uint32_t c = i < f.lit_segments ? f.colour : kDim;
    if (c == seg_colour[i]) continue;
    seg_colour[i] = c;
    lv_obj_set_style_line_color(seg[i], lv_color_hex(c), 0);
  }
  if (f.scale_visible != labels_shown_) {
    labels_shown_ = f.scale_visible;
    for (int i = 0; i < label_count_; ++i) {
      if (labels_shown_) lv_obj_remove_flag(label[i], LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(label[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (f.unit != unit_text_) { unit_text_ = f.unit; lv_label_set_text_static(unit_, f.unit); }
  if (f.status != status_text_) { status_text_ = f.status; lv_label_set_text_static(status_, f.status); }
  if (f.colour != status_colour_) {
    status_colour_ = f.colour;
    lv_obj_set_style_text_color(status_, lv_color_hex(f.colour), 0);
  }
  if (std::strncmp(footer_text, f.footer, sizeof(footer_text)) != 0) {
    std::snprintf(footer_text, sizeof(footer_text), "%s", f.footer);
    lv_label_set_text_static(footer_, footer_text);
  }
  if (f.demo != demo_shown_) {
    demo_shown_ = f.demo;
    if (f.demo) lv_obj_remove_flag(demo_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(demo_, LV_OBJ_FLAG_HIDDEN);
  }
}
