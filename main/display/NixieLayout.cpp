// Nixie layout (#26): the reading in four fixed glass tubes. Each tube is its own object, redrawn only when its character
// changes, so a change in the ones digit redraws one tube. The draw callback reads the layout's shown_ array in the LVGL
// task, which holds the display lock; render() writes it under the same lock.
#include "display/Layouts.h"
#include <cmath>

using namespace layout;

namespace {
constexpr int kC = 233;  // the screen's centre and radius
// Four tubes in a row, centred on the screen, each a capsule of glass.
constexpr int kTubeW = 84, kTubeH = 150, kTubeGap = 8, kTubeR = 40, kGlassW = 2;
constexpr int kTubesW = gauge::kNixieTubes * kTubeW + (gauge::kNixieTubes - 1) * kTubeGap;
constexpr int kTubeX0 = kC - kTubesW / 2, kTubeY = kC - kTubeH / 2;
// The numeral's box (gauge::kNixieW x kNixieH units) scaled into the middle of its tube.
constexpr float kScale = 110.f / gauge::kNixieH;
constexpr float kNumOx = (kTubeW - gauge::kNixieW * kScale) / 2.f, kNumOy = (kTubeH - gauge::kNixieH * kScale) / 2.f;
// Stroke widths: two glow strokes under the bright core, and the unlit numerals' thin wire.
constexpr int kGlowOuterW = 15, kGlowInnerW = 8, kCoreW = 3, kStackW = 2;
// The anode mesh: two sets of 45-degree lines kMeshStep apart, inside a rectangle inset kMeshInset from the tube's sides.
constexpr int kMeshInset = 14, kMeshTop = 26, kMeshStep = 12;
// Title above the tubes; the status lines and then the LED below them.
constexpr int kTitleW = 160, kTitleH = 28, kTitleCy = kTubeY - 10 - kTitleH / 2;
constexpr int kLinesY = kTubeY + kTubeH + 12;
constexpr int kLinesEnd = kLinesY + 2 * lines::kGap + lines::kTextH;
constexpr int kLedReach = led::kD / 2 + led::kGlow;
constexpr int kLedCy = kLinesEnd + 10 + kLedReach;
// Colours on black: the glow strokes are the orange already blended with black, so overlapping strokes don't build up.
constexpr uint32_t kCore = 0xFFA052, kGlowInner = 0x662406, kGlowOuter = 0x290E03;
constexpr uint32_t kStack = 0x231E1B, kMesh = 0x161C21, kGlass = 0x22303A;

constexpr int sq(int v) { return v * v; }
constexpr bool onScreen(int x, int y, int margin) { return sq(x - kC) + sq(y - kC) <= sq(kC - margin); }
static_assert(onScreen(kTubeX0, kTubeY, 8) && onScreen(kTubeX0 + kTubesW, kTubeY + kTubeH, 8), "the tubes must fit on the screen");
// The numeral, with its widest glow, stays inside the glass: its box's corner, inset by the margin, against the capsule's
// corner circle.
constexpr float kInX = kNumOx + gauge::kNixieMargin * kScale, kInY = kNumOy + gauge::kNixieMargin * kScale;
static_assert((kTubeR - kInX) * (kTubeR - kInX) + (kTubeR - kInY) * (kTubeR - kInY) <=
              (kTubeR - kGlassW - kGlowOuterW / 2.f) * (kTubeR - kGlassW - kGlowOuterW / 2.f), "numerals must sit inside the glass");
static_assert(kNumOx >= kGlassW + kGlowOuterW / 2.f && kNumOy >= kGlassW + kGlowOuterW / 2.f, "numerals must sit inside the glass");
static_assert(sq(kTubeR - kMeshInset) + sq(kTubeR - kMeshTop) < sq(kTubeR - kGlassW - 1), "the mesh must sit inside the glass");
static_assert(kTitleCy + kTitleH / 2 < kTubeY && onScreen(kC - kTitleW / 2, kTitleCy - kTitleH / 2, 8), "the title must fit above the tubes");
static_assert(kLinesY > kTubeY + kTubeH && lines::blockFits(kLinesY), "the status lines must fit below the tubes");
static_assert(kLedCy - kLedReach > kLinesEnd && onScreen(kC + kLedReach, kLedCy + kLedReach, 4), "the LED must fit below the lines");

void line(lv_layer_t* layer, float x0, float y0, float x1, float y1, int w, uint32_t colour) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = lv_color_hex(colour);
  d.width = w;
  d.round_start = 1;
  d.round_end = 1;
  d.p1 = {static_cast<lv_value_precise_t>(lroundf(x0)), static_cast<lv_value_precise_t>(lroundf(y0))};
  d.p2 = {static_cast<lv_value_precise_t>(lroundf(x1)), static_cast<lv_value_precise_t>(lroundf(y1))};
  lv_draw_line(layer, &d);
}

// A character's numeral, from the tube's top-left corner (x, y).
void numeral(lv_layer_t* layer, int x, int y, char c, int w, uint32_t colour) {
  int count = 0;
  const gauge::NixieStroke* s = gauge::nixieStrokes(c, count);
  gauge::NixiePoint p[gauge::kNixieMaxPoints];
  for (int k = 0; k < count; ++k) {
    const int n = gauge::nixiePolyline(s[k], p);
    for (int i = 1; i < n; ++i)
      line(layer, x + kNumOx + p[i - 1].x * kScale, y + kNumOy + p[i - 1].y * kScale, x + kNumOx + p[i].x * kScale,
           y + kNumOy + p[i].y * kScale, w, colour);
  }
}

// The mesh's lines x - y = k and x + y = k (tube coordinates), cut to its rectangle.
void mesh(lv_layer_t* layer, int x, int y) {
  const int x0 = kMeshInset, x1 = kTubeW - kMeshInset, y0 = kMeshTop, y1 = kTubeH - kMeshTop;
  for (int k = x0 - y1 + kMeshStep / 2; k < x1 - y0; k += kMeshStep) {  // down to the right
    const int xs = x0 > y0 + k ? x0 : y0 + k, xe = x1 < y1 + k ? x1 : y1 + k;
    if (xs < xe) line(layer, x + xs, y + xs - k, x + xe, y + xe - k, 1, kMesh);
  }
  for (int k = x0 + y0 + kMeshStep / 2; k < x1 + y1; k += kMeshStep) {  // down to the left
    const int xs = x0 > k - y1 ? x0 : k - y1, xe = x1 < k - y0 ? x1 : k - y0;
    if (xs < xe) line(layer, x + xs, y + k - xs, x + xe, y + k - xe, 1, kMesh);
  }
}

lv_obj_t* plain(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_remove_flag(o, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  return o;
}
// The Wi-Fi panel (#34) sits above the status lines and LED, so it never hides an alarm.
static_assert(panel::kBottom < kLinesY && panel::kBottom < kLedCy - kLedReach,
              "the Wi-Fi panel must clear the status lines and LED");
} // namespace

void NixieLayout::build(lv_obj_t* screen, gauge::Unit unit) {
  root_ = plain(screen, 0, 0, 466, 466);
  lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
  for (int i = 0; i < gauge::kNixieTubes; ++i) {
    tubes_[i] = plain(root_, kTubeX0 + i * (kTubeW + kTubeGap), kTubeY, kTubeW, kTubeH);
    lv_obj_add_event_cb(tubes_[i], &NixieLayout::onDrawTube, LV_EVENT_DRAW_MAIN, &shown_[i]);
  }
  title_text_ = gauge::titleText(unit);
  title_ = lv_label_create(root_);
  lv_label_set_long_mode(title_, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(title_, kTitleW, kTitleH);
  lv_obj_set_pos(title_, kC - kTitleW / 2, kTitleCy - kTitleH / 2);
  lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(title_, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title_, lv_color_hex(kMute), 0);
  lv_label_set_text_static(title_, title_text_);
  lines_.build(root_, kLinesY);
  led_.build(root_, kLedCy);
  // Presses must reach the screen: no child may be clickable or scrollable.
  const uint32_t count = lv_obj_get_child_count(root_);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_remove_flag(lv_obj_get_child(root_, static_cast<int32_t>(i)),
                       static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  }
}

void NixieLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  char t[gauge::kNixieTubes];
  gauge::nixieTubes(f.cells, t);  // Classic's cells; fails closed to "  --"
  for (int i = 0; i < gauge::kNixieTubes; ++i) {
    if (t[i] != shown_[i]) { shown_[i] = t[i]; lv_obj_invalidate(tubes_[i]); }
  }
  if (f.title != title_text_) { title_text_ = f.title; lv_label_set_text_static(title_, title_text_); }
  lines_.render(f);
  led_.render(f.led);
}

void NixieLayout::onDrawTube(lv_event_t* e) {
  const char* c = static_cast<const char*>(lv_event_get_user_data(e));
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(lv_event_get_target_obj(e), &a);
  const int x = a.x1, y = a.y1;

  lv_draw_rect_dsc_t glass;
  lv_draw_rect_dsc_init(&glass);
  glass.radius = kTubeR;
  glass.bg_opa = LV_OPA_TRANSP;
  glass.border_color = lv_color_hex(kGlass);
  glass.border_width = kGlassW;
  glass.border_opa = LV_OPA_COVER;
  lv_draw_rect(layer, &glass, &a);
  mesh(layer, x, y);
  for (char d = '0'; d <= '9'; ++d) numeral(layer, x, y, d, kStackW, kStack);  // the unlit cathodes, stacked
  if (!c || *c == ' ') return;
  numeral(layer, x, y, *c, kGlowOuterW, kGlowOuter);
  numeral(layer, x, y, *c, kGlowInnerW, kGlowInner);
  numeral(layer, x, y, *c, kCoreW, kCore);
}
