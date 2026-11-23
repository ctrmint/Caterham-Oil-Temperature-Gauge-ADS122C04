#include "display/Layouts.h"
#include <cmath>
#include <cstdio>

using namespace layout;

// White dial (#19), styled on a white-faced aftermarket oil-temperature gauge. The face (black ring, white face and 51
// ticks) is drawn by one object; the numbers and legend are labels; the needle and its counterweight are drawn by an
// object that moves with them, under a static hub. Below the hub, in the 90-degree wedge the needle never sweeps: the
// legend, the 6 o'clock LED (AlarmLed, unchanged) in a black socket, and a black status window that shows only while
// whiteWindow() says something needs attention. The window runs down into the ring, so its lines are always on black.
//
// The draw callbacks read the layout's members in the LVGL task, which holds the display lock; render() and the timers
// write them under the same lock.
namespace {
constexpr int kC = 233;
constexpr float kPi = 3.14159265f;
constexpr float kStartDeg = 135.0f, kSweepDeg = 270.0f;  // as Needle, Sweep and OEM: start lower left, open at the bottom
constexpr int kRingW = 14, kFaceR = kC - kRingW;          // the black ring, then the white face inside it
constexpr int kTickOut = 214;                             // every tick runs in from here
constexpr int kMajorIn = 188, kMajorW = 5;                // long and thick at the numbers (every 20 degC)
constexpr int kMediumIn = 196, kMediumW = 3;              // the other tens
constexpr int kMinorIn = 203, kMinorW = 2;                // every other 2 degC
constexpr int kNumR = 136;                                // numbers (Montserrat 48) centred on this radius
constexpr int kNumInkHalfW = 46, kNumInkHalfH = 20;       // half the widest number's ink ("284" is 91 x 34)
constexpr int kHubR = 26, kCapR = 9;
constexpr int kNeedleR = kMajorIn - 2;                    // the tip stops at the long ticks' inner ends
constexpr int kNeedleBack = 12, kNeedleHalfW = 5;         // the base sits behind the pivot, under the hub
constexpr int kTailR = 40, kTailHalfW = 7, kTailReach = 41;  // the counterweight: a square-ended bar opposite the needle
constexpr int kOilCy = 289, kOilInkHalfW = 22, kOilInkHalfH = 9;    // "OIL" (Montserrat 24, ink 42 x 17)
constexpr int kUnitCy = 307, kUnitInkHalfW = 10, kUnitInkHalfH = 7;  // the unit (Montserrat 16, ink 18 x 12)
constexpr int kLedCy = 347, kSocketR = 30;                // the LED's black socket, just wider than its glow
constexpr int kWinY = 383, kWinHalfW = 120, kWinR = 12;   // the status window's top edge; it runs down off the screen
constexpr int kTextPad = 5;                               // from the window's top to the status line
constexpr int kSin45x1000 = 707;
constexpr uint32_t kTimerMs = 30;
constexpr float kEase = 0.3f;  // the share of the remaining distance moved each frame: it never overshoots
constexpr float kSettled = 0.0005f;
constexpr uint32_t kFace = 0xF2F2F2, kRing = 0x000000, kRingSheen = 0x2C3136, kInk = 0x111111;
constexpr uint32_t kNeedle = 0xE53B2C, kTail = 0x141618, kHubTop = 0x2A2D31, kHubBottom = 0x0B0C0D, kHubEdge = 0x3A3F45;
constexpr uint32_t kCap = 0x050505, kSocket = 0x000000, kLens = 0x30353A, kWindow = 0x000000;

constexpr int sq(int v) { return v * v; }
// A box half_w either side of the centre line, with its top at y, stays clear of the needle at either end of the scale
// (45 degrees below level): the perpendicular distance from its top corners to the needle's line.
constexpr bool clearOfNeedle(int half_w, int y) { return (y - kC - half_w) * kSin45x1000 / 1000 > kNeedleHalfW + 2; }

static_assert(kTickOut < kFaceR, "ticks must sit inside the ring");
static_assert(kMajorIn < kMediumIn && kMediumIn < kMinorIn && kMinorIn < kTickOut, "ticks: long, medium, short");
// Each number's ink stays inside the long ticks and clear of the hub and counterweight, wherever it points.
static_assert(sq(kMajorIn - kNumR) >= sq(kNumInkHalfW) + sq(kNumInkHalfH), "White numbers must stay inside the ticks");
static_assert(sq(kNumR - kTailReach) >= sq(kNumInkHalfW) + sq(kNumInkHalfH), "White numbers must clear the hub");
static_assert(kNeedleR < kMajorIn && kNeedleR > kTailReach, "the needle reaches from the hub to the ticks");
static_assert(sq(kNeedleBack) + sq(kNeedleHalfW) < sq(kHubR - 2), "the needle's base must hide under the hub");
static_assert(kTailR > kHubR && sq(kTailR) + sq(kTailHalfW) <= sq(kTailReach), "the counterweight shows beyond the hub");
// Top to bottom under the hub: the counterweight's lowest reach (pointing straight down, at 90), OIL, the unit, the LED's
// socket and the window, each clear of the needle at the scale's ends.
static_assert(kOilCy - kOilInkHalfH > kC + kTailReach + 2, "OIL must clear the counterweight");
static_assert(clearOfNeedle(kOilInkHalfW, kOilCy - kOilInkHalfH), "OIL must clear the needle");
static_assert(kUnitCy - kUnitInkHalfH > kOilCy + kOilInkHalfH, "the unit must sit below OIL");
static_assert(clearOfNeedle(kUnitInkHalfW, kUnitCy - kUnitInkHalfH), "the unit must clear the needle");
static_assert(kSocketR >= led::kD / 2 + led::kGlow, "the socket must hold the LED's glow");
static_assert(kLedCy - kSocketR > kUnitCy + kUnitInkHalfH, "the LED must sit below the unit");
static_assert((kLedCy - kC) * kSin45x1000 / 1000 > kSocketR + kNeedleHalfW + 2, "the LED must clear the needle");
static_assert(kSocketR < kNumR * kSin45x1000 / 1000 - kNumInkHalfW, "the LED must clear the bottom numbers");
static_assert(kWinY > kLedCy + kSocketR, "the window must sit below the LED");
static_assert(kWinY > kC + kNumR * kSin45x1000 / 1000 + kNumInkHalfH, "the window must sit below the bottom numbers");
static_assert(clearOfNeedle(kWinHalfW, kWinY), "the window must clear the needle");
static_assert(kWinHalfW < kMajorIn * kSin45x1000 / 1000 - kMajorW, "the window must clear the ticks at the scale's ends");
// The status lines sit inside the window, which runs down into the black ring, and on the screen.
static_assert(lines::kStatusW / 2 <= kWinHalfW - 4 && lines::kDemoW <= lines::kStatusW, "lines must fit the window");
static_assert(lines::blockFits(kWinY + kTextPad), "White status lines must fit on the screen");
static_assert(kTextPad + 2 * lines::kGap + lines::kTextH <= 466 - kWinY, "the lines must fit in the window's panel");

float rad(float fraction) { return (kStartDeg + kSweepDeg * fraction) * kPi / 180.0f; }
lv_value_precise_t px(float v) { return static_cast<lv_value_precise_t>(lroundf(v)); }
int ipx(float v) { return static_cast<int>(lroundf(v)); }

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

lv_obj_t* filled(lv_obj_t* parent, int x, int y, int w, int h, int radius, uint32_t colour) {
  lv_obj_t* o = plain(parent, x, y, w, h);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(colour), 0);
  return o;
}

lv_obj_t* centred_label(lv_obj_t* parent, const lv_font_t* font, uint32_t colour, int dx, int dy) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
  lv_obj_align(l, LV_ALIGN_CENTER, dx, dy);
  return l;
}

void show(lv_obj_t* o, bool on) {
  if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// A tick, skipped when it lies outside the area being redrawn: lv_draw_line() allocates a draw task either way, and a
// moving needle redraws only small parts of the face.
void tick(lv_layer_t* layer, int x0, int y0, int x1, int y1, int w, lv_color_t c) {
  const lv_area_t& clip = layer->_clip_area;
  if ((x0 < x1 ? x0 : x1) - w > clip.x2 || (x0 > x1 ? x0 : x1) + w < clip.x1 ||
      (y0 < y1 ? y0 : y1) - w > clip.y2 || (y0 > y1 ? y0 : y1) + w < clip.y1) return;
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = c;
  d.width = w;
  d.p1 = {static_cast<lv_value_precise_t>(x0), static_cast<lv_value_precise_t>(y0)};
  d.p2 = {static_cast<lv_value_precise_t>(x1), static_cast<lv_value_precise_t>(y1)};
  lv_draw_line(layer, &d);
}
} // namespace

void WhiteLayout::build(lv_obj_t* screen, gauge::Unit unit) {
  root_ = plain(screen, 0, 0, 466, 466);
  lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t* face = plain(root_, 0, 0, 466, 466);
  lv_obj_add_event_cb(face, &WhiteLayout::onDrawFace, LV_EVENT_DRAW_MAIN, nullptr);

  // A number on each long tick, and the unit under OIL; hidden until calibrated. OIL itself always shows.
  for (int i = 0; i < gauge::kWhiteNumbers; ++i) {
    const float a = rad(static_cast<float>(gauge::whiteNumberTick(i)) / (gauge::kWhiteTicks - 1));
    std::snprintf(number_text_[i], sizeof(number_text_[i]), "%d", gauge::whiteLabel(unit, i));
    numbers_[i] = centred_label(root_, &lv_font_montserrat_48, kInk, ipx(kNumR * cosf(a)), ipx(kNumR * sinf(a)));
    lv_label_set_text_static(numbers_[i], number_text_[i]);
    lv_obj_add_flag(numbers_[i], LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_t* oil = centred_label(root_, &lv_font_montserrat_24, kInk, 0, kOilCy - kC);
  lv_label_set_text_static(oil, "OIL");
  unit_ = centred_label(root_, &lv_font_montserrat_16, kInk, 0, kUnitCy - kC);
  unit_text_ = gauge::unitLabel(unit);
  lv_label_set_text_static(unit_, unit_text_);
  lv_obj_add_flag(unit_, LV_OBJ_FLAG_HIDDEN);
  labels_shown_ = false;

  // The LED in a black socket; the dark-grey lens shows through while ledState() says off.
  filled(root_, kC - kSocketR, kLedCy - kSocketR, 2 * kSocketR, 2 * kSocketR, LV_RADIUS_CIRCLE, kSocket);
  filled(root_, kC - led::kD / 2, kLedCy - led::kD / 2, led::kD, led::kD, LV_RADIUS_CIRCLE, kLens);
  led_.build(root_, kLedCy);

  // The status window: a black panel from kWinY down into the ring, with the status lines in it. Shown from the start:
  // nothing is known to be well yet.
  window_ = plain(root_, 0, kWinY, 466, 466 - kWinY);
  filled(window_, kC - kWinHalfW, 0, 2 * kWinHalfW, 466 - kWinY + kWinR, kWinR, kWindow);  // only the top corners show
  lines_.build(window_, kTextPad);
  window_shown_ = true;

  // The needle and counterweight, under the hub. Hidden until there's a temperature; no sweep at power-up.
  needle_ = plain(root_, 0, 0, 1, 1);
  lv_obj_add_event_cb(needle_, &WhiteLayout::onDrawNeedle, LV_EVENT_DRAW_MAIN, this);
  needle_key_ = -1;
  placeNeedle(0.f);
  lv_obj_add_flag(needle_, LV_OBJ_FLAG_HIDDEN);
  needle_shown_ = false;
  shown_ = target_ = 0.f;
  timer_ = lv_timer_create(&WhiteLayout::onTimer, kTimerMs, this);
  lv_timer_pause(timer_);

  lv_obj_t* hub = filled(root_, kC - kHubR, kC - kHubR, 2 * kHubR, 2 * kHubR, LV_RADIUS_CIRCLE, kHubTop);
  lv_obj_set_style_bg_grad_color(hub, lv_color_hex(kHubBottom), 0);
  lv_obj_set_style_bg_grad_dir(hub, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(hub, 2, 0);
  lv_obj_set_style_border_color(hub, lv_color_hex(kHubEdge), 0);
  lv_obj_t* cap = filled(root_, kC - kCapR, kC - kCapR, 2 * kCapR, 2 * kCapR, LV_RADIUS_CIRCLE, kCap);
  lv_obj_set_style_border_width(cap, 1, 0);
  lv_obj_set_style_border_color(cap, lv_color_hex(kHubTop), 0);

  // Presses must reach the screen: nothing may be clickable or scrollable.
  const lv_obj_flag_t off = static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, off);
  const uint32_t count = lv_obj_get_child_count(root_);
  for (uint32_t i = 0; i < count; ++i) lv_obj_remove_flag(lv_obj_get_child(root_, static_cast<int32_t>(i)), off);
  const uint32_t in_window = lv_obj_get_child_count(window_);
  for (uint32_t i = 0; i < in_window; ++i) lv_obj_remove_flag(lv_obj_get_child(window_, static_cast<int32_t>(i)), off);
}

void WhiteLayout::onDrawFace(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t area;
  lv_obj_get_coords(lv_event_get_target_obj(e), &area);
  const int cx = area.x1 + kC, cy = area.y1 + kC;

  lv_draw_rect_dsc_t face;
  lv_draw_rect_dsc_init(&face);
  face.radius = LV_RADIUS_CIRCLE;
  face.bg_color = lv_color_hex(kFace);
  face.bg_opa = LV_OPA_COVER;
  face.border_color = lv_color_hex(kRing);
  face.border_width = kRingW;
  face.border_opa = LV_OPA_COVER;
  lv_draw_rect(layer, &face, &area);

  // A faint sheen along the ring's upper edge.
  lv_draw_arc_dsc_t sheen;
  lv_draw_arc_dsc_init(&sheen);
  sheen.color = lv_color_hex(kRingSheen);
  sheen.width = 3;
  sheen.center.x = cx;
  sheen.center.y = cy;
  sheen.radius = kC - 2;
  sheen.start_angle = 205;
  sheen.end_angle = 335;
  sheen.rounded = 1;
  lv_draw_arc(layer, &sheen);

  const lv_color_t ink = lv_color_hex(kInk);
  for (int i = 0; i < gauge::kWhiteTicks; ++i) {
    const float a = rad(static_cast<float>(i) / (gauge::kWhiteTicks - 1)), ux = cosf(a), uy = sinf(a);
    const gauge::TickSize size = gauge::whiteTick(i);
    const int in = size == gauge::TickSize::Major ? kMajorIn : (size == gauge::TickSize::Medium ? kMediumIn : kMinorIn);
    const int w = size == gauge::TickSize::Major ? kMajorW : (size == gauge::TickSize::Medium ? kMediumW : kMinorW);
    tick(layer, cx + ipx(in * ux), cy + ipx(in * uy), cx + ipx(kTickOut * ux), cy + ipx(kTickOut * uy), w, ink);
  }
}

// The object spans just the needle and counterweight at this angle, padded for anti-aliasing; moving or resizing it
// redraws the old and new areas, and the invalidate covers a turn that keeps the same box.
void WhiteLayout::placeNeedle(float fraction) {
  const float a = rad(fraction), ux = cosf(a), uy = sinf(a);
  const float tip_x = kC + kNeedleR * ux, tip_y = kC + kNeedleR * uy;
  const int key = ipx(tip_x) * 1000 + ipx(tip_y);
  if (key == needle_key_) return;
  needle_key_ = key;
  const float bx = kC - kNeedleBack * ux, by = kC - kNeedleBack * uy;
  const float xs[3] = {bx - kNeedleHalfW * uy, bx + kNeedleHalfW * uy, tip_x};
  const float ys[3] = {by + kNeedleHalfW * ux, by - kNeedleHalfW * ux, tip_y};
  const float tail_x = kC - kTailR * ux, tail_y = kC - kTailR * uy;
  float x0 = fminf(kC, tail_x) - kTailHalfW, x1 = fmaxf(kC, tail_x) + kTailHalfW;
  float y0 = fminf(kC, tail_y) - kTailHalfW, y1 = fmaxf(kC, tail_y) + kTailHalfW;
  for (int i = 0; i < 3; ++i) {
    x0 = fminf(x0, xs[i]); x1 = fmaxf(x1, xs[i]);
    y0 = fminf(y0, ys[i]); y1 = fmaxf(y1, ys[i]);
  }
  constexpr int kPad = 3;
  const int mx = static_cast<int>(floorf(x0)) - kPad, my = static_cast<int>(floorf(y0)) - kPad;
  const int w = static_cast<int>(ceilf(x1)) + kPad - mx + 1, h = static_cast<int>(ceilf(y1)) + kPad - my + 1;
  for (int i = 0; i < 3; ++i) needle_pts_[i] = {px(xs[i] - mx), px(ys[i] - my)};
  tail_pts_[0] = {px(static_cast<float>(kC - mx)), px(static_cast<float>(kC - my))};
  tail_pts_[1] = {px(tail_x - mx), px(tail_y - my)};
  lv_obj_set_pos(needle_, mx, my);
  lv_obj_set_size(needle_, w, h);
  lv_obj_invalidate(needle_);
}

void WhiteLayout::onDrawNeedle(lv_event_t* e) {
  const WhiteLayout* self = static_cast<const WhiteLayout*>(lv_event_get_user_data(e));
  if (!self) return;
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t area;
  lv_obj_get_coords(lv_event_get_target_obj(e), &area);
  auto at = [&](const lv_point_precise_t& p) {
    return lv_point_precise_t{p.x + area.x1, p.y + area.y1};
  };
  lv_draw_line_dsc_t tail;  // square ends, as in the reference; the inner end is under the hub
  lv_draw_line_dsc_init(&tail);
  tail.color = lv_color_hex(kTail);
  tail.width = 2 * kTailHalfW;
  tail.p1 = at(self->tail_pts_[0]);
  tail.p2 = at(self->tail_pts_[1]);
  lv_draw_line(layer, &tail);

  lv_draw_triangle_dsc_t tri;
  lv_draw_triangle_dsc_init(&tri);
  tri.color = lv_color_hex(kNeedle);
  tri.opa = LV_OPA_COVER;
  for (int i = 0; i < 3; ++i) tri.p[i] = at(self->needle_pts_[i]);
  lv_draw_triangle(layer, &tri);
}

void WhiteLayout::onTimer(lv_timer_t* t) {
  WhiteLayout* self = static_cast<WhiteLayout*>(lv_timer_get_user_data(t));
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

void WhiteLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  lines_.render(f);
  if (f.status_window != window_shown_) {
    window_shown_ = f.status_window;
    show(window_, window_shown_);
  }
  if (f.unit != unit_text_) { unit_text_ = f.unit; lv_label_set_text_static(unit_, f.unit); }
  if (f.scale_visible != labels_shown_) {
    labels_shown_ = f.scale_visible;
    for (lv_obj_t* n : numbers_) show(n, labels_shown_);
    show(unit_, labels_shown_);
  }
  led_.render(f.led);
  if (!f.white_needle.visible) {
    if (needle_shown_) {
      lv_obj_add_flag(needle_, LV_OBJ_FLAG_HIDDEN);
      lv_timer_pause(timer_);
      needle_shown_ = false;
    }
    return;
  }
  target_ = f.white_needle.fraction;
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
