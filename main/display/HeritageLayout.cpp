#include "display/Layouts.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>

using namespace layout;

// Heritage dial (#25), in the spirit of a 1960s instrument. The face (a chrome-look bezel, a faint glass reflection, the
// scale line and 17 ticks) is drawn by one object; the numbers and legend are labels; the needle and its counterweight are
// drawn by an object that moves with them, under a black boss with a chrome cap. The scale is a 90-degree sweep across the
// top about a pivot kPivotDy below the screen's centre, so the needle never points below the pivot: under it sit the
// legend (or the status lines in its place) and the 6 o'clock LED (AlarmLed, unchanged) in a chrome surround.
//
// The draw callbacks read the layout's members in the LVGL task, which holds the display lock; render() and the timer
// write them under the same lock.
namespace {
constexpr int kC = 233;
constexpr float kPi = 3.14159265f;
constexpr int kBezelW = 16, kFaceR = kC - kBezelW;  // the chrome bezel at the edge, then the black face inside it
constexpr int kPivotDy = 60, kPivotY = kC + kPivotDy;
constexpr float kStartDeg = 225.0f, kSweepDeg = 90.0f;  // 50 degC at the upper left, 90 at 12 o'clock, 130 at the upper right
constexpr int kTickOut = 244;                           // from the pivot: every tick runs in from here, under the scale line
constexpr int kMajorIn = 222, kMajorW = 4;              // long at 50, 70, 90, 110 and 130
constexpr int kMediumIn = 229, kMediumW = 3;            // the other tens
constexpr int kMinorIn = 235, kMinorW = 2;              // the fives
constexpr int kScaleLineW = 2;                          // a thin arc along the ticks' outer ends
constexpr int kNumR = 170;                              // numbers (Montserrat 48) centred on this radius from the pivot
constexpr int kNumInkHalfW = 46, kNumInkHalfH = 17;     // half the widest number's ink ("266", about 91 x 34)
constexpr int kBossR = 16, kCapR = 6;
constexpr int kNeedleR = kMajorIn - 4;                  // the tip stops short of the long ticks
constexpr int kNeedleBack = 10, kNeedleHalfW = 4;       // the base sits behind the pivot, under the boss
constexpr int kTailR = 24, kTailHalfW = 4, kTailReach = 25;  // the counterweight: a short bar opposite the needle
constexpr int kLinesY = 326;                            // the status lines, or the legend in their place
constexpr int kLegendCy = 337, kUnitCy = 359, kLegendInkHalfH = 8;
constexpr int kLampCy = 414, kRingR = 24, kWellR = 18, kLensR = 14;  // the LED in a chrome surround
constexpr int kLedReach = led::kD / 2 + led::kGlow;
constexpr int kGlassR = 198, kGlassW = 14;              // a faint reflection across the upper left of the face
constexpr int kSin45x1000 = 707;
constexpr uint32_t kTimerMs = 30;
constexpr float kEase = 0.3f;  // the share of the remaining distance moved each frame: it never overshoots
constexpr float kSettled = 0.0005f;
constexpr uint32_t kInk = 0xEDE3C8;  // cream, like aged luminous paint
constexpr uint32_t kChromeLight = 0xB4B9BE, kChromeDark = 0x34383C, kBezelEdge = 0x1C1F22, kBezelLip = 0x111315;
constexpr uint32_t kStreak = 0xD5D9DD, kTail = 0xB8B09C, kBoss = 0x0A0B0C, kBossEdge = 0x2E3236;
constexpr uint32_t kWell = 0x050505, kLens = 0x30353A;

constexpr int sq(int v) { return v * v; }
static_assert(kMajorIn < kMediumIn && kMediumIn < kMinorIn && kMinorIn < kTickOut, "ticks: long, medium, short");
// The scale's ends (45 degrees above level from the pivot) are its farthest points from the screen's centre.
static_assert(sq((kTickOut + 1) * kSin45x1000 / 1000) + sq((kTickOut + 1) * kSin45x1000 / 1000 - kPivotDy) <= sq(kFaceR - 6) &&
              kTickOut + 1 - kPivotDy <= kFaceR - 6, "the scale must sit inside the bezel");
static_assert(sq(kMajorIn - kNumR) >= sq(kNumInkHalfW) + sq(kNumInkHalfH), "numbers must stay inside the ticks");
static_assert(sq(kNumR * kSin45x1000 / 1000 + kNumInkHalfW) + sq(kNumR * kSin45x1000 / 1000 - kPivotDy + kNumInkHalfH) <=
              sq(kFaceR - 8), "the end numbers must sit inside the bezel");
static_assert(kNumR - kNumInkHalfW - kNumInkHalfH > kBossR + 8, "numbers must clear the boss");
static_assert(kNeedleR < kMajorIn && kNeedleR > kTailReach, "the needle reaches from the boss to the ticks");
static_assert(sq(kNeedleBack) + sq(kNeedleHalfW) < sq(kBossR - 2), "the needle's base must hide under the boss");
static_assert(kTailR > kBossR && sq(kTailR) + sq(kTailHalfW) <= sq(kTailReach), "the counterweight shows beyond the boss");
// Under the pivot: the counterweight's lowest reach, then the legend or status lines, then the lamp, on the face.
static_assert(kLinesY > kPivotY + kTailReach + 2 && kLinesY > kPivotY + kBossR, "the text must clear the counterweight");
static_assert(lines::blockFits(kLinesY), "Heritage status lines must fit on the screen");
static_assert(kLegendCy - kLegendInkHalfH >= kLinesY && kUnitCy + kLegendInkHalfH < kLampCy - kRingR, "the legend sits above the lamp");
static_assert(kLampCy - kLedReach > kLinesY + 2 * lines::kGap + lines::kTextH, "the lamp's glow must clear the status lines");
static_assert(kLensR >= led::kD / 2 && kWellR > kLensR && kRingR > kWellR, "the lamp: chrome ring, well, then lens");
static_assert(kLampCy + kLedReach - kC <= kFaceR - 4 && kLampCy + kRingR - kC <= kFaceR - 4, "the lamp must sit inside the bezel");
static_assert(kGlassR <= kFaceR - 6, "the reflection must sit inside the bezel");

float rad(float fraction) { return (kStartDeg + kSweepDeg * fraction) * kPi / 180.0f; }
lv_value_precise_t px(float v) { return static_cast<lv_value_precise_t>(lroundf(v)); }
int ipx(float v) { return static_cast<int>(lroundf(v)); }

uint32_t mix(uint32_t a, uint32_t b, float t) {  // a at t = 0, b at t = 1
  uint32_t out = 0;
  for (int sh = 0; sh <= 16; sh += 8) {
    const float ca = static_cast<float>((a >> sh) & 0xFF), cb = static_cast<float>((b >> sh) & 0xFF);
    out |= static_cast<uint32_t>(lroundf(ca + (cb - ca) * t)) << sh;
  }
  return out;
}
// Polished chrome lit from the upper left: brightest there, darkest at the lower right.
uint32_t chrome(float deg) {
  const float t = 0.5f * (1.f + cosf((deg - 225.f) * kPi / 180.f));
  return mix(kChromeDark, kChromeLight, t * t);
}

bool outside(const lv_area_t& clip, int x0, int y0, int x1, int y1) {
  return x0 > clip.x2 || x1 < clip.x1 || y0 > clip.y2 || y1 < clip.y1;
}

// An arc about (cx, cy), skipped when it lies outside the area being redrawn: lv_draw_arc() allocates a draw task either
// way, and a moving needle redraws only small parts of the face. The box is the sector's corners, padded; the arcs drawn
// here span at most 90 degrees, so it's checked against the box of the corners and the arc's middle.
void arc(lv_layer_t* layer, int cx, int cy, int r, int w, float a0, float a1, uint32_t colour, lv_opa_t opa) {
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  for (float a : {a0, (a0 + a1) / 2.f, a1})
    for (int rr : {r, r - w}) {
      const float x = cx + rr * cosf(a * kPi / 180.f), y = cy + rr * sinf(a * kPi / 180.f);
      x0 = fminf(x0, x); x1 = fmaxf(x1, x); y0 = fminf(y0, y); y1 = fmaxf(y1, y);
    }
  const int pad = 2 + r / 20;  // the arc bulges past its chord
  if (outside(layer->_clip_area, ipx(x0) - pad, ipx(y0) - pad, ipx(x1) + pad, ipx(y1) + pad)) return;
  lv_draw_arc_dsc_t d;
  lv_draw_arc_dsc_init(&d);
  d.color = lv_color_hex(colour);
  d.opa = opa;
  d.width = w;
  d.center.x = cx;
  d.center.y = cy;
  d.radius = static_cast<uint16_t>(r);
  d.start_angle = a0;
  d.end_angle = a1;
  lv_draw_arc(layer, &d);
}

void tick(lv_layer_t* layer, int x0, int y0, int x1, int y1, int w, lv_color_t c) {
  if (outside(layer->_clip_area, (x0 < x1 ? x0 : x1) - w, (y0 < y1 ? y0 : y1) - w, (x0 > x1 ? x0 : x1) + w,
              (y0 > y1 ? y0 : y1) + w)) return;
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = c;
  d.width = w;
  d.p1 = {static_cast<lv_value_precise_t>(x0), static_cast<lv_value_precise_t>(y0)};
  d.p2 = {static_cast<lv_value_precise_t>(x1), static_cast<lv_value_precise_t>(y1)};
  lv_draw_line(layer, &d);
}

lv_obj_t* plain(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  return o;
}

lv_obj_t* disc(lv_obj_t* parent, int cx, int cy, int r, uint32_t top, uint32_t bottom, uint32_t edge) {
  lv_obj_t* o = plain(parent, cx - r, cy - r, 2 * r, 2 * r);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(top), 0);
  if (bottom != top) {
    lv_obj_set_style_bg_grad_color(o, lv_color_hex(bottom), 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
  }
  if (edge != top) {
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(edge), 0);
  }
  return o;
}

lv_obj_t* centred_label(lv_obj_t* parent, const lv_font_t* font, int dx, int dy) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(kInk), 0);
  lv_obj_align(l, LV_ALIGN_CENTER, dx, dy);
  return l;
}

void show(lv_obj_t* o, bool on) {
  if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

void passPresses(lv_obj_t* obj) {
  const uint32_t count = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_t* child = lv_obj_get_child(obj, static_cast<int32_t>(i));
    lv_obj_remove_flag(child, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    passPresses(child);
  }
}
// The Wi-Fi panel (#34) sits above the status lines and LED, so it never hides an alarm.
static_assert(panel::kBottom < kLinesY && panel::kBottom < kLampCy - kRingR,
              "the Wi-Fi panel must clear the status lines and LED");
} // namespace

void HeritageLayout::build(lv_obj_t* screen, gauge::Unit unit) {
  root_ = plain(screen, 0, 0, 466, 466);
  lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t* face = plain(root_, 0, 0, 466, 466);
  lv_obj_add_event_cb(face, &HeritageLayout::onDrawFace, LV_EVENT_DRAW_MAIN, nullptr);

  // Numbers at 50, 90 and 130; hidden until calibrated.
  for (int i = 0; i < gauge::kHeritageNumbers; ++i) {
    const float a = rad(static_cast<float>(gauge::heritageNumberTick(i)) / (gauge::kHeritageTicks - 1));
    std::snprintf(number_text_[i], sizeof(number_text_[i]), "%d", gauge::heritageLabel(unit, i));
    numbers_[i] = centred_label(root_, &lv_font_montserrat_48, ipx(kNumR * cosf(a)), kPivotDy + ipx(kNumR * sinf(a)));
    lv_label_set_text_static(numbers_[i], number_text_[i]);
    lv_obj_add_flag(numbers_[i], LV_OBJ_FLAG_HIDDEN);
  }
  labels_shown_ = false;

  // The legend, and the status lines that take its place; the status lines show from the start: nothing is known to be
  // well yet.
  legend_ = plain(root_, 0, 0, 466, 466);
  lv_obj_t* oil = centred_label(legend_, &lv_font_montserrat_16, 0, kLegendCy - kC);
  lv_obj_set_style_text_letter_space(oil, 3, 0);
  lv_label_set_text_static(oil, "OIL TEMP");
  unit_ = centred_label(legend_, &lv_font_montserrat_16, 0, kUnitCy - kC);
  unit_text_ = gauge::unitLabel(unit);
  lv_label_set_text_static(unit_, unit_text_);
  lv_obj_add_flag(unit_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(legend_, LV_OBJ_FLAG_HIDDEN);
  legend_shown_ = false;
  text_ = plain(root_, 0, 0, 466, 466);
  lines_.build(text_, kLinesY);

  // The lamp: the LED in a chrome ring around a dark well, with a dark lens that shows while ledState() says off.
  disc(root_, kC, kLampCy, kRingR, kChromeLight, kChromeDark, kBezelEdge);
  disc(root_, kC, kLampCy, kWellR, kWell, kWell, kWell);
  disc(root_, kC, kLampCy, kLensR, kLens, kLens, kLens);
  led_.build(root_, kLampCy);

  // The needle and counterweight, under the boss. Hidden until there's a temperature; no sweep at power-up.
  needle_ = plain(root_, 0, 0, 1, 1);
  lv_obj_add_event_cb(needle_, &HeritageLayout::onDrawNeedle, LV_EVENT_DRAW_MAIN, this);
  needle_key_ = -1;
  placeNeedle(0.f);
  lv_obj_add_flag(needle_, LV_OBJ_FLAG_HIDDEN);
  needle_shown_ = false;
  shown_ = target_ = 0.f;
  timer_ = lv_timer_create(&HeritageLayout::onTimer, kTimerMs, this);
  lv_timer_pause(timer_);

  disc(root_, kC, kPivotY, kBossR, kBoss, kBoss, kBossEdge);
  disc(root_, kC, kPivotY, kCapR, kChromeLight, kChromeDark, kBezelEdge);

  // Presses must reach the screen: nothing may be clickable or scrollable.
  lv_obj_remove_flag(root_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  passPresses(root_);
}

void HeritageLayout::onDrawFace(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t area;
  lv_obj_get_coords(lv_event_get_target_obj(e), &area);
  const int cx = area.x1 + kC, cy = area.y1 + kC, py = area.y1 + kPivotY;

  // The bezel: a dark outer edge, a chrome band in 5-degree steps of light (each overlapping the next by a degree, so no
  // seam shows), a bright streak where the light catches it, and a dark lip where it meets the glass.
  arc(layer, cx, cy, kC, 3, 0.f, 90.f, kBezelEdge, LV_OPA_COVER);
  arc(layer, cx, cy, kC, 3, 90.f, 180.f, kBezelEdge, LV_OPA_COVER);
  arc(layer, cx, cy, kC, 3, 180.f, 270.f, kBezelEdge, LV_OPA_COVER);
  arc(layer, cx, cy, kC, 3, 270.f, 360.f, kBezelEdge, LV_OPA_COVER);
  for (int i = 0; i < 72; ++i) {
    const float a0 = 5.f * static_cast<float>(i), a1 = a0 + (i == 71 ? 5.f : 6.f);
    arc(layer, cx, cy, kC - 3, kBezelW - 5, a0, a1, chrome(a0 + 2.5f), LV_OPA_COVER);
  }
  arc(layer, cx, cy, kC - 6, 2, 200.f, 250.f, kStreak, LV_OPA_COVER);
  arc(layer, cx, cy, kFaceR + 2, 2, 0.f, 90.f, kBezelLip, LV_OPA_COVER);
  arc(layer, cx, cy, kFaceR + 2, 2, 90.f, 180.f, kBezelLip, LV_OPA_COVER);
  arc(layer, cx, cy, kFaceR + 2, 2, 180.f, 270.f, kBezelLip, LV_OPA_COVER);
  arc(layer, cx, cy, kFaceR + 2, 2, 270.f, 360.f, kBezelLip, LV_OPA_COVER);

  // The glass: a faint reflection, very low brightness so it lights almost nothing.
  arc(layer, cx, cy, kGlassR, kGlassW, 200.f, 245.f, 0xFFFFFF, 10);

  // The scale line and the ticks, about the pivot.
  arc(layer, cx, py, kTickOut + 1, kScaleLineW, kStartDeg, kStartDeg + kSweepDeg, kInk, LV_OPA_COVER);
  const lv_color_t ink = lv_color_hex(kInk);
  for (int i = 0; i < gauge::kHeritageTicks; ++i) {
    const float a = rad(static_cast<float>(i) / (gauge::kHeritageTicks - 1)), ux = cosf(a), uy = sinf(a);
    const gauge::TickSize size = gauge::heritageTick(i);
    const int in = size == gauge::TickSize::Major ? kMajorIn : (size == gauge::TickSize::Medium ? kMediumIn : kMinorIn);
    const int w = size == gauge::TickSize::Major ? kMajorW : (size == gauge::TickSize::Medium ? kMediumW : kMinorW);
    tick(layer, cx + ipx(in * ux), py + ipx(in * uy), cx + ipx(kTickOut * ux), py + ipx(kTickOut * uy), w, ink);
  }
}

// The object spans just the needle and counterweight at this angle, padded for anti-aliasing; moving or resizing it
// redraws the old and new areas, and the invalidate covers a turn that keeps the same box.
void HeritageLayout::placeNeedle(float fraction) {
  const float a = rad(fraction), ux = cosf(a), uy = sinf(a);
  const float tip_x = kC + kNeedleR * ux, tip_y = kPivotY + kNeedleR * uy;
  const int key = ipx(tip_x) * 1000 + ipx(tip_y);
  if (key == needle_key_) return;
  needle_key_ = key;
  const float bx = kC - kNeedleBack * ux, by = kPivotY - kNeedleBack * uy;
  const float xs[3] = {bx - kNeedleHalfW * uy, bx + kNeedleHalfW * uy, tip_x};
  const float ys[3] = {by + kNeedleHalfW * ux, by - kNeedleHalfW * ux, tip_y};
  const float tail_x = kC - kTailR * ux, tail_y = kPivotY - kTailR * uy;
  float x0 = fminf(kC, tail_x) - kTailHalfW, x1 = fmaxf(kC, tail_x) + kTailHalfW;
  float y0 = fminf(kPivotY, tail_y) - kTailHalfW, y1 = fmaxf(kPivotY, tail_y) + kTailHalfW;
  for (int i = 0; i < 3; ++i) {
    x0 = fminf(x0, xs[i]); x1 = fmaxf(x1, xs[i]);
    y0 = fminf(y0, ys[i]); y1 = fmaxf(y1, ys[i]);
  }
  constexpr int kPad = 3;
  const int mx = static_cast<int>(floorf(x0)) - kPad, my = static_cast<int>(floorf(y0)) - kPad;
  const int w = static_cast<int>(ceilf(x1)) + kPad - mx + 1, h = static_cast<int>(ceilf(y1)) + kPad - my + 1;
  for (int i = 0; i < 3; ++i) needle_pts_[i] = {px(xs[i] - mx), px(ys[i] - my)};
  tail_pts_[0] = {px(static_cast<float>(kC - mx)), px(static_cast<float>(kPivotY - my))};
  tail_pts_[1] = {px(tail_x - mx), px(tail_y - my)};
  lv_obj_set_pos(needle_, mx, my);
  lv_obj_set_size(needle_, w, h);
  lv_obj_invalidate(needle_);
}

void HeritageLayout::onDrawNeedle(lv_event_t* e) {
  const HeritageLayout* self = static_cast<const HeritageLayout*>(lv_event_get_user_data(e));
  if (!self) return;
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t area;
  lv_obj_get_coords(lv_event_get_target_obj(e), &area);
  auto at = [&](const lv_point_precise_t& p) {
    return lv_point_precise_t{p.x + area.x1, p.y + area.y1};
  };
  lv_draw_line_dsc_t tail;  // the inner end is under the boss
  lv_draw_line_dsc_init(&tail);
  tail.color = lv_color_hex(kTail);
  tail.width = 2 * kTailHalfW;
  tail.p1 = at(self->tail_pts_[0]);
  tail.p2 = at(self->tail_pts_[1]);
  lv_draw_line(layer, &tail);

  lv_draw_triangle_dsc_t tri;
  lv_draw_triangle_dsc_init(&tri);
  tri.color = lv_color_hex(kInk);
  tri.opa = LV_OPA_COVER;
  for (int i = 0; i < 3; ++i) tri.p[i] = at(self->needle_pts_[i]);
  lv_draw_triangle(layer, &tri);
}

void HeritageLayout::onTimer(lv_timer_t* t) {
  HeritageLayout* self = static_cast<HeritageLayout*>(lv_timer_get_user_data(t));
  if (!self || !self->needle_) return;
  const float d = self->target_ - self->shown_;
  if (std::fabs(d) < kSettled) {
    self->shown_ = self->target_;
    lv_timer_pause(t);
  } else {
    self->shown_ += d * kEase;
  }
  self->placeNeedle(self->shown_);
}

void HeritageLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  lines_.render(f);
  if (f.heritage_legend != legend_shown_) {
    legend_shown_ = f.heritage_legend;
    show(legend_, legend_shown_);
    show(text_, !legend_shown_);
  }
  if (f.unit != unit_text_) { unit_text_ = f.unit; lv_label_set_text_static(unit_, f.unit); }
  if (f.scale_visible != labels_shown_) {
    labels_shown_ = f.scale_visible;
    for (lv_obj_t* n : numbers_) show(n, labels_shown_);
    show(unit_, labels_shown_);
  }
  led_.render(f.led);
  if (!f.heritage_needle.visible) {
    if (needle_shown_) {
      lv_obj_add_flag(needle_, LV_OBJ_FLAG_HIDDEN);
      lv_timer_pause(timer_);
      needle_shown_ = false;
    }
    return;
  }
  target_ = f.heritage_needle.fraction;
  if (!needle_shown_) {
    // Appearing (power-up, or after a fault): straight to the reading, with no sweep from anywhere else.
    shown_ = target_;
    placeNeedle(shown_);
    lv_obj_remove_flag(needle_, LV_OBJ_FLAG_HIDDEN);
    needle_shown_ = true;
  } else if (target_ != shown_) {
    lv_timer_resume(timer_);
  }
}
