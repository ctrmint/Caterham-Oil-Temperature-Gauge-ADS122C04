#include "display/Layouts.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

using namespace layout;

namespace {
constexpr uint32_t kDim = 0x22313A;    // unlit ring segment, same as the Classic track
constexpr uint32_t kGhost = 0x111A22;  // unlit digit segment: about half way from the black kBg to kDim
constexpr float kPi = 3.14159265f;
constexpr float kStartDeg = 135.0f, kSweepDeg = 270.0f;  // as the Classic arc: start lower left, open at the bottom
constexpr int kRingInner = 198, kRingOuter = 226, kSegWidth = 16;
constexpr int kLabelRadius = 168, kLabelW = 36, kLabelH = 16;
constexpr int kDigitLeft = sweep::kDigitCx - sweep::kDigitW / 2;
constexpr int kDigitRight = sweep::kDigitCx + sweep::kDigitW / 2;
constexpr int kDigitTop = sweep::kDigitCy - sweep::kDigitH / 2;
constexpr int kDigitBottom = sweep::kDigitCy + sweep::kDigitH / 2;
constexpr int kSignRight = kDigitLeft - sweep::kSignGap;
// Every scale label box (kLabelW x kLabelH, centred on kLabelRadius) lies outside kLabelClear of the centre, so a box
// whose corners are all inside it can never touch a label, whatever the label's angle.
constexpr int kLabelClear = kLabelRadius - 20;  // 20 >= half-diagonal of the 36 x 16 label box (19.7)
constexpr bool insideLabels(int x, int y) {
  const int dx = x - sweep::kCentre, dy = y - sweep::kCentre;
  return dx * dx + dy * dy < kLabelClear * kLabelClear;
}
static_assert(insideLabels(kDigitLeft, kDigitTop) && insideLabels(kDigitRight, kDigitTop) &&
              insideLabels(kDigitLeft, kDigitBottom) && insideLabels(kDigitRight, kDigitBottom),
              "Sweep digits must stay inside the scale labels");
static_assert(insideLabels(kSignRight - sweep::kSignW, sweep::kDigitCy - sweep::kSignInkH / 2) &&
              insideLabels(kSignRight - sweep::kSignW, sweep::kDigitCy + sweep::kSignInkH / 2),
              "Sweep sign must stay inside the scale labels");
// Small texts (16 px tall): unit and OIL TEMP under the digits' right edge, then status, footer and demo banner.
constexpr int kTextH = 16, kUnitY = kDigitBottom + 6, kTitleY = kUnitY + 18, kStatusY = 300, kFooterY = 320, kDemoY = 340;
// LED at 6 o'clock (#10): in the ring's open bottom, below the demo banner and inside the round screen, glow included.
constexpr int kLedCy = 410;
static_assert(kLedCy - led::kD / 2 - led::kGlow > kDemoY + kTextH, "LED must sit below the demo banner");
static_assert((kLedCy - sweep::kCentre) + led::kD / 2 + led::kGlow <= sweep::kCentre, "LED must stay on the round screen");
static_assert(led::kD / 2 + led::kGlow < kRingInner * 707 / 1000, "LED must stay inside the ring's open bottom");
static_assert(kTitleY + kTextH < kStatusY && kStatusY + kTextH < kFooterY && kFooterY + kTextH < kDemoY,
              "Sweep texts must not overlap");
static_assert(sweep::kGlyphW <= sweep::kCellW && 2 * sweep::kThick + 2 * sweep::kGap < sweep::kGlyphW,
              "seven-segment glyph must fit its cell");

// Build-once tables in static storage (one SweepLayout instance), so they stay off the 8 KB main-task stack.
// lv_line keeps the points pointer, so pts must outlive the lines.
lv_point_precise_t pts[gauge::kSweepSegments][2];
lv_obj_t* seg[gauge::kSweepSegments];
uint32_t seg_colour[gauge::kSweepSegments];
uint32_t seg_lit_colour[gauge::kSweepSegments];  // graduated colour at each segment's place on the scale
lv_obj_t* label[sweep::kMaxLabels];
char label_text[sweep::kMaxLabels][8];
char footer_text[gauge::kFooterTextSize];

// Seven-segment digits (geometry in sweep::), plus one '<' / '>' sign label.
using sweep::kCellW; using sweep::kGlyphW; using sweep::kThick; using sweep::kGap;
lv_obj_t* digit_seg[gauge::kSweepCells][7];
uint32_t digit_colour[gauge::kSweepCells][7];
lv_obj_t* sign_label;
char sign_ch;          // 0 = sign hidden
uint32_t sign_colour;

int px(float v) { return static_cast<int>(lroundf(v)); }

lv_obj_t* small_label(lv_obj_t* parent, int w, int x, int y, lv_text_align_t align, uint32_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(l, w, kTextH);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_style_text_align(l, align, 0);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  return l;
}
// The Wi-Fi panel (#34) sits above the status lines and LED, so it never hides an alarm.
static_assert(panel::kBottom < kStatusY && panel::kBottom < kDemoY && panel::kBottom < kLedCy - led::kD / 2 - led::kGlow,
              "the Wi-Fi panel must clear the status lines and LED");
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
    seg_lit_colour[i] = gauge::segmentColour(i, unit);
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

  // Seven-segment cells: segments a-g (bits 0-6 of sevenSegmentMask) as small rectangles, centred as a block of three.
  {
    const int mid = (sweep::kDigitH - kThick) / 2;
    const int vlen = mid - kThick - 2 * kGap;
    const int hlen = kGlyphW - 2 * kThick - 2 * kGap;
    // x, y, w, h relative to the glyph origin, in order a b c d e f g.
    const int geo[7][4] = {
      {kThick + kGap, 0, hlen, kThick},
      {kGlyphW - kThick, kThick + kGap, kThick, vlen},
      {kGlyphW - kThick, mid + kThick + kGap, kThick, vlen},
      {kThick + kGap, sweep::kDigitH - kThick, hlen, kThick},
      {0, mid + kThick + kGap, kThick, vlen},
      {0, kThick + kGap, kThick, vlen},
      {kThick + kGap, mid, hlen, kThick}};
    for (int i = 0; i < static_cast<int>(gauge::kSweepCells); ++i) {
      const int gx = kDigitLeft + i * kCellW + (kCellW - kGlyphW) / 2;
      for (int k = 0; k < 7; ++k) {
        lv_obj_t* o = lv_obj_create(root_);
        lv_obj_set_size(o, geo[k][2], geo[k][3]);
        lv_obj_set_pos(o, gx + geo[k][0], kDigitTop + geo[k][1]);
        lv_obj_set_style_radius(o, 3, 0);
        lv_obj_set_style_border_width(o, 0, 0);
        lv_obj_set_style_pad_all(o, 0, 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(o, lv_color_hex(kGhost), 0);
        digit_seg[i][k] = o;
        digit_colour[i][k] = kGhost;
      }
    }
    // The sign, right-aligned against the first cell, as wide as the wider of '<' and '>' (at most kSignW).
    int32_t sw = 0;
    for (uint32_t c : {static_cast<uint32_t>('<'), static_cast<uint32_t>('>')}) {
      const int32_t w = static_cast<int32_t>(lv_font_get_glyph_width(&lv_font_montserrat_48, c, 0));
      if (w > sw) sw = w;
    }
    if (sw > sweep::kSignW) sw = sweep::kSignW;
    const int lh = static_cast<int>(lv_font_get_line_height(&lv_font_montserrat_48));
    sign_label = lv_label_create(root_);
    lv_label_set_long_mode(sign_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(sign_label, sw, lh);
    lv_obj_set_pos(sign_label, kSignRight - sw, sweep::kDigitCy - lh / 2);
    lv_obj_set_style_text_align(sign_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(sign_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(sign_label, lv_color_hex(kWhite), 0);
    lv_label_set_text_static(sign_label, ">");
    lv_obj_add_flag(sign_label, LV_OBJ_FLAG_HIDDEN);
    sign_ch = 0;
    sign_colour = kWhite;
  }

  // Unit above OIL TEMP, lower right of the digit block.
  unit_ = small_label(root_, 70, kDigitRight - 70, kUnitY, LV_TEXT_ALIGN_RIGHT, kMute);
  title_ = small_label(root_, 90, kDigitRight - 90, kTitleY, LV_TEXT_ALIGN_RIGHT, kWhite);
  // Status, footer and demo banner in the open bottom of the ring.
  lv_label_set_text_static(unit_, gauge::unitLabel(unit));
  lv_label_set_text_static(title_, "OIL TEMP");
  status_ = small_label(root_, 220, sweep::kCentre - 110, kStatusY, LV_TEXT_ALIGN_CENTER, kAmber);
  lv_label_set_text_static(status_, "SENSOR CHECK");
  footer_ = small_label(root_, 220, sweep::kCentre - 110, kFooterY, LV_TEXT_ALIGN_CENTER, kMute);
  demo_ = small_label(root_, 180, sweep::kCentre - 90, kDemoY, LV_TEXT_ALIGN_CENTER, kRed);
  lv_label_set_text_static(demo_, "DEMO ONLY - NOT REAL");
  lv_obj_add_flag(demo_, LV_OBJ_FLAG_HIDDEN);
  std::snprintf(footer_text, sizeof(footer_text), "NOT CALIBRATED");
  lv_label_set_text_static(footer_, footer_text);
  status_colour_ = kAmber;

  // LED at 6 o'clock, hidden until there's a temperature.
  led_.build(root_, kLedCy);

  // Presses must reach the screen: nothing may be clickable or scrollable.
  const lv_obj_flag_t off = static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, off);
  const uint32_t count = lv_obj_get_child_count(root_);
  for (uint32_t i = 0; i < count; ++i) lv_obj_remove_flag(lv_obj_get_child(root_, static_cast<int32_t>(i)), off);
}

void SweepLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  // Fail closed: if any cell is not displayable, show the whole reading as " --" with no sign.
  char ch[gauge::kSweepCells];
  bool ok = true;
  for (size_t i = 0; i < gauge::kSweepCells; ++i) {
    ch[i] = f.sweep_cells[i];
    if (!(ch[i] == ' ' || (ch[i] != '\0' && gauge::sevenSegmentMask(ch[i]) != 0))) ok = false;
  }
  const char sign = (ok && (f.sweep_sign == '<' || f.sweep_sign == '>')) ? f.sweep_sign : 0;
  for (size_t i = 0; i < gauge::kSweepCells; ++i) {
    if (!ok) ch[i] = (i + 2 >= gauge::kSweepCells) ? '-' : ' ';
    const uint8_t mask = gauge::sevenSegmentMask(ch[i]);
    for (int k = 0; k < 7; ++k) {
      const uint32_t c = (mask >> k) & 1 ? f.colour : kGhost;
      if (c == digit_colour[i][k]) continue;
      lv_obj_set_style_bg_color(digit_seg[i][k], lv_color_hex(c), 0);
      digit_colour[i][k] = c;
    }
  }
  if (sign != sign_ch) {
    if (sign) {
      lv_label_set_text_static(sign_label, sign == '<' ? "<" : ">");
      if (!sign_ch) lv_obj_remove_flag(sign_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(sign_label, LV_OBJ_FLAG_HIDDEN);
    }
    sign_ch = sign;
  }
  if (sign && sign_colour != f.colour) {
    sign_colour = f.colour;
    lv_obj_set_style_text_color(sign_label, lv_color_hex(f.colour), 0);
  }
  for (int i = 0; i < gauge::kSweepSegments; ++i) {
    const uint32_t c = i < f.lit_segments ? seg_lit_colour[i] : kDim;
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
  led_.render(f.led);
}
