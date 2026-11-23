#include "display/Layouts.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>

using namespace layout;

// OEM dial (#17), styled on the Caterham factory gauge. The face (rim, black face and ticks) is drawn by one object; the
// numbers and unit are labels; the needle and its counterweight are drawn by an object that moves with them, under a
// static hub; the oil-temperature symbol is the 6 o'clock LED, coloured as ledState() says. At the bottom, CATERHAM
// shows only while oemWordmark() says all is well; otherwise the status lines take its place.
//
// The draw callbacks read the layout's members in the LVGL task, which holds the display lock; render() and the timers
// write them under the same lock.
namespace {
constexpr int kC = 233;
constexpr float kPi = 3.14159265f;
constexpr float kStartDeg = 135.0f, kSweepDeg = 270.0f;  // as Needle and Sweep: start lower left, open at the bottom
constexpr int kRimW = 15, kFaceR = kC - kRimW;            // the dark rim, then the black face inside it
constexpr int kTickIn = 191, kTickOut = 211, kTickW = 6;  // a short, thick dash every 10 degC, no arc line
constexpr int kNumR = 140;                                // numbers (Montserrat 48) centred on this radius
constexpr int kNumInkHalfW = 46, kNumInkHalfH = 20;       // half the widest number's ink ("130", "266")
constexpr int kUnitCy = kC - kNumR + kNumInkHalfH + 22;   // the unit (Montserrat 24) under the 90 at 12 o'clock
constexpr int kUnitHalfH = 14;
constexpr int kHubR = 30;
constexpr int kNeedleR = kTickIn - 3;                     // the tip stops at the ticks' inner ends
constexpr int kNeedleBack = 14, kNeedleHalfW = 5;         // the base sits behind the pivot, under the hub
constexpr int kTailR = 42, kTailHalfW = 10;               // the counterweight: a round-ended bar opposite the needle
constexpr int kSymW = 72, kSymH = 44, kSymY = 292;        // the oil-temperature symbol, centred under the hub
constexpr int kTextY = 356;                               // the wordmark's top, and the status line's in its place
constexpr int kWordH = 28, kWordHalfW = 96, kWordSpace = 6;
constexpr int kTextBlockH = 2 * lines::kGap + lines::kTextH;  // status, footer and demo lines
constexpr int kSin45x1000 = 707;
constexpr uint32_t kTimerMs = 30;
constexpr float kEase = 0.3f;  // the share of the remaining distance moved each frame: it never overshoots
constexpr float kSettled = 0.0005f;
constexpr uint32_t kFace = 0x000000, kRim = 0x16191C, kRimSheen = 0x2C3136, kTick = 0xC9CDD1, kInk = 0xD3D6D9;
constexpr uint32_t kNeedle = 0xE9E7E0, kTail = 0x1C1F22, kHubTop = 0x181B1E, kHubBottom = 0x060708, kHubEdge = 0x2E3338;
constexpr uint32_t kWord = 0xB9BDC1, kSymbolOff = 0x30353A;

constexpr bool insideFace(int half_w, int y_top, int h) {
  const int dy = lines::iabs(y_top - kC) > lines::iabs(y_top + h - kC) ? lines::iabs(y_top - kC) : lines::iabs(y_top + h - kC);
  return half_w * half_w + dy * dy <= (kFaceR - 2) * (kFaceR - 2);
}
constexpr int sq(int v) { return v * v; }

static_assert(kTickOut < kFaceR, "ticks must sit inside the rim");
// Each number's ink stays inside the ticks and clear of the hub and the counterweight, wherever it points.
static_assert(sq(kTickIn - kNumR) >= sq(kNumInkHalfW) + sq(kNumInkHalfH), "OEM numbers must stay inside the ticks");
static_assert(sq(kNumR - kTailR - kTailHalfW) >= sq(kNumInkHalfW) + sq(kNumInkHalfH), "OEM numbers must clear the hub");
static_assert(kNeedleR < kTickIn && kNeedleR > kTailR + kTailHalfW, "the needle reaches from the hub to the ticks");
static_assert(sq(kNeedleBack) + sq(kNeedleHalfW) < sq(kHubR - 2), "the needle's base must hide under the hub");
static_assert(kTailR + kTailHalfW > kHubR, "the counterweight must show beyond the hub");
// The unit sits under the 90 and above the counterweight, which never points higher than 45 degrees above level.
static_assert(kUnitCy - kUnitHalfH > kC - kNumR + kNumInkHalfH, "unit must be below the top number");
static_assert(kUnitCy + kUnitHalfH < kC - (kTailR + kTailHalfW), "unit must be above the hub and counterweight");
// The symbol sits under the counterweight's lowest reach (pointing straight down), clear of the needle at either end of
// the scale (45 degrees below level), of the bottom number and of the text below it.
static_assert(kSymY > kC + kTailR + kTailHalfW + 4, "symbol must clear the counterweight");
static_assert(((kSymY - kC) - kSymW / 2) * kSin45x1000 / 1000 > kNeedleHalfW + 4, "symbol must clear the needle");
static_assert(kSymW / 2 < kNumR * kSin45x1000 / 1000 - kNumInkHalfW, "symbol must clear the bottom number");
static_assert(kSymY + kSymH < kTextY, "symbol must be above the wordmark and status lines");
// The wordmark and the status lines share the space below the bottom number, inside the face and clear of the end ticks.
static_assert(kTextY > kC + kNumR * kSin45x1000 / 1000 + kNumInkHalfH, "text must be below the bottom number");
static_assert(insideFace(kWordHalfW, kTextY, kWordH), "wordmark must fit inside the rim");
static_assert(insideFace(lines::kStatusW / 2, kTextY, lines::kTextH) &&
                  insideFace(lines::kStatusW / 2, kTextY + lines::kGap, lines::kTextH) &&
                  insideFace(lines::kDemoW / 2, kTextY + 2 * lines::kGap, lines::kTextH),
              "OEM status lines must fit inside the rim");
static_assert(lines::kStatusW / 2 < kTickIn * kSin45x1000 / 1000 - kTickW && kWordHalfW < lines::kStatusW / 2,
              "text must clear the ticks at the scale's ends");

float rad(float fraction) { return (kStartDeg + kSweepDeg * fraction) * kPi / 180.0f; }
lv_value_precise_t px(float v) { return static_cast<lv_value_precise_t>(lroundf(v)); }

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

void disc(lv_layer_t* layer, int cx, int cy, int r, lv_color_t c) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE;
  d.bg_color = c;
  d.bg_opa = LV_OPA_COVER;
  const lv_area_t a = {cx - r, cy - r, cx + r, cy + r};
  lv_draw_rect(layer, &d, &a);
}

void box(lv_layer_t* layer, int x0, int y0, int x1, int y1, int radius, lv_color_t c) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = radius;
  d.bg_color = c;
  d.bg_opa = LV_OPA_COVER;
  const lv_area_t a = {x0, y0, x1, y1};
  lv_draw_rect(layer, &d, &a);
}

void segment(lv_layer_t* layer, int x0, int y0, int x1, int y1, int w, lv_color_t c) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = c;
  d.width = w;
  d.p1 = {static_cast<lv_value_precise_t>(x0), static_cast<lv_value_precise_t>(y0)};
  d.p2 = {static_cast<lv_value_precise_t>(x1), static_cast<lv_value_precise_t>(y1)};
  lv_draw_line(layer, &d);
}
// The Wi-Fi panel (#34) sits above the status lines and LED, so it never hides an alarm.
static_assert(panel::kBottom < kTextY && panel::kBottom < kSymY,
              "the Wi-Fi panel must clear the status lines and LED");
} // namespace

void OemLayout::build(lv_obj_t* screen, gauge::Unit unit) {
  root_ = plain(screen, 0, 0, 466, 466);
  lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t* face = plain(root_, 0, 0, 466, 466);
  lv_obj_add_event_cb(face, &OemLayout::onDrawFace, LV_EVENT_DRAW_MAIN, nullptr);

  // Numbers on every second tick after the start, and the unit under the 90; hidden until calibrated.
  for (int i = 0; i < gauge::kOemNumbers; ++i) {
    const float a = rad(static_cast<float>(gauge::oemNumberStep(i)) / gauge::kOemSteps);
    std::snprintf(number_text_[i], sizeof(number_text_[i]), "%d", gauge::oemLabel(unit, i));
    numbers_[i] = centred_label(root_, &lv_font_montserrat_48, kInk, static_cast<int>(lroundf(kNumR * cosf(a))),
                                static_cast<int>(lroundf(kNumR * sinf(a))));
    lv_label_set_text_static(numbers_[i], number_text_[i]);
    lv_obj_add_flag(numbers_[i], LV_OBJ_FLAG_HIDDEN);
  }
  unit_ = centred_label(root_, &lv_font_montserrat_24, kInk, 0, kUnitCy - kC);
  unit_text_ = gauge::unitLabel(unit);
  lv_label_set_text_static(unit_, unit_text_);
  lv_obj_add_flag(unit_, LV_OBJ_FLAG_HIDDEN);
  labels_shown_ = false;

  // The symbol, which is the LED: dark grey until ledState() lights it.
  symbol_ = plain(root_, kC - kSymW / 2, kSymY, kSymW, kSymH);
  lv_obj_add_event_cb(symbol_, &OemLayout::onDrawSymbol, LV_EVENT_DRAW_MAIN, this);
  symbol_colour_ = kSymbolOff;
  symbol_opa_ = 255;
  symbol_pulse_ = gauge::LedPulse::None;
  pulse_timer_ = lv_timer_create(&OemLayout::onPulseTimer, led::kTickMs, this);
  lv_timer_pause(pulse_timer_);

  // The wordmark: plain text in the firmware's font, drawn twice a pixel apart for weight. Hidden until all is well.
  word_ = plain(root_, kC - kWordHalfW, kTextY, 2 * kWordHalfW, kWordH);
  for (int dx = 0; dx < 2; ++dx) {
    lv_obj_t* l = lv_label_create(word_);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(l, 2 * kWordHalfW - 1, kWordH);
    lv_obj_set_pos(l, dx, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_letter_space(l, kWordSpace, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(kWord), 0);
    lv_label_set_text_static(l, "CATERHAM");
  }
  lv_obj_add_flag(word_, LV_OBJ_FLAG_HIDDEN);

  // The status lines in the wordmark's place, shown from the start: nothing is known to be well yet.
  text_ = plain(root_, 0, kTextY, 466, kTextBlockH);
  lines_.build(text_, 0);
  word_shown_ = false;

  // The needle and counterweight, under the hub. Hidden until there's a temperature; no sweep at power-up.
  needle_ = plain(root_, 0, 0, 1, 1);
  lv_obj_add_event_cb(needle_, &OemLayout::onDrawNeedle, LV_EVENT_DRAW_MAIN, this);
  needle_key_ = -1;
  placeNeedle(0.f);
  lv_obj_add_flag(needle_, LV_OBJ_FLAG_HIDDEN);
  needle_shown_ = false;
  shown_ = target_ = 0.f;
  needle_timer_ = lv_timer_create(&OemLayout::onNeedleTimer, kTimerMs, this);
  lv_timer_pause(needle_timer_);

  lv_obj_t* hub = plain(root_, kC - kHubR, kC - kHubR, 2 * kHubR, 2 * kHubR);
  lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(hub, lv_color_hex(kHubTop), 0);
  lv_obj_set_style_bg_grad_color(hub, lv_color_hex(kHubBottom), 0);
  lv_obj_set_style_bg_grad_dir(hub, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(hub, 2, 0);
  lv_obj_set_style_border_color(hub, lv_color_hex(kHubEdge), 0);

  // Presses must reach the screen: nothing may be clickable or scrollable.
  const lv_obj_flag_t off = static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, off);
  const uint32_t count = lv_obj_get_child_count(root_);
  for (uint32_t i = 0; i < count; ++i) lv_obj_remove_flag(lv_obj_get_child(root_, static_cast<int32_t>(i)), off);
}

void OemLayout::onDrawFace(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t area;
  lv_obj_get_coords(lv_event_get_target_obj(e), &area);
  const int cx = area.x1 + kC, cy = area.y1 + kC;

  lv_draw_rect_dsc_t face;
  lv_draw_rect_dsc_init(&face);
  face.radius = LV_RADIUS_CIRCLE;
  face.bg_color = lv_color_hex(kFace);
  face.bg_opa = LV_OPA_COVER;
  face.border_color = lv_color_hex(kRim);
  face.border_width = kRimW;
  face.border_opa = LV_OPA_COVER;
  lv_draw_rect(layer, &face, &area);

  // A faint sheen along the rim's upper edge.
  lv_draw_arc_dsc_t sheen;
  lv_draw_arc_dsc_init(&sheen);
  sheen.color = lv_color_hex(kRimSheen);
  sheen.width = 3;
  sheen.center.x = cx;
  sheen.center.y = cy;
  sheen.radius = kC - 2;
  sheen.start_angle = 205;
  sheen.end_angle = 335;
  sheen.rounded = 1;
  lv_draw_arc(layer, &sheen);

  for (int i = 0; i <= gauge::kOemSteps; ++i) {
    const float a = rad(static_cast<float>(i) / gauge::kOemSteps), ux = cosf(a), uy = sinf(a);
    segment(layer, cx + static_cast<int>(lroundf(kTickIn * ux)), cy + static_cast<int>(lroundf(kTickIn * uy)),
            cx + static_cast<int>(lroundf(kTickOut * ux)), cy + static_cast<int>(lroundf(kTickOut * uy)), kTickW,
            lv_color_hex(kTick));
  }
}

// The object spans just the needle and counterweight at this angle, padded for anti-aliasing; moving or resizing it
// redraws the old and new areas, and the invalidate covers a turn that keeps the same box.
void OemLayout::placeNeedle(float fraction) {
  const float a = rad(fraction), ux = cosf(a), uy = sinf(a);
  const float tip_x = kC + kNeedleR * ux, tip_y = kC + kNeedleR * uy;
  const int key = static_cast<int>(lroundf(tip_x)) * 1000 + static_cast<int>(lroundf(tip_y));
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

void OemLayout::onDrawNeedle(lv_event_t* e) {
  const OemLayout* self = static_cast<const OemLayout*>(lv_event_get_user_data(e));
  if (!self) return;
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t area;
  lv_obj_get_coords(lv_event_get_target_obj(e), &area);
  auto at = [&](const lv_point_precise_t& p) {
    return lv_point_precise_t{p.x + area.x1, p.y + area.y1};
  };
  lv_draw_line_dsc_t tail;
  lv_draw_line_dsc_init(&tail);
  tail.color = lv_color_hex(kTail);
  tail.width = 2 * kTailHalfW;
  tail.round_start = 1;
  tail.round_end = 1;
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

// An oil can (body, handle, spout and a drip) with a thermometer standing in it, kept apart from the body by a thin
// black keyline. One colour, scaled towards the black face by the pulse's opacity.
void OemLayout::onDrawSymbol(lv_event_t* e) {
  const OemLayout* self = static_cast<const OemLayout*>(lv_event_get_user_data(e));
  if (!self) return;
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t area;
  lv_obj_get_coords(lv_event_get_target_obj(e), &area);
  const int x = area.x1, y = area.y1;
  const lv_color_t c = lv_color_mix(lv_color_hex(self->symbol_colour_), lv_color_hex(kFace), self->symbol_opa_);
  const lv_color_t key = lv_color_hex(kFace);

  box(layer, x + 12, y + 27, x + 48, y + 42, 4, c);  // body
  lv_draw_triangle_dsc_t spout;                       // from the body's upper right, up to the tip
  lv_draw_triangle_dsc_init(&spout);
  spout.color = c;
  spout.opa = LV_OPA_COVER;
  spout.p[0] = {static_cast<lv_value_precise_t>(x + 42), static_cast<lv_value_precise_t>(y + 28)};
  spout.p[1] = {static_cast<lv_value_precise_t>(x + 48), static_cast<lv_value_precise_t>(y + 39)};
  spout.p[2] = {static_cast<lv_value_precise_t>(x + 70), static_cast<lv_value_precise_t>(y + 18)};
  lv_draw_triangle(layer, &spout);
  disc(layer, x + 68, y + 29, 3, c);  // the drip
  lv_draw_arc_dsc_t handle;          // the left half of a ring
  lv_draw_arc_dsc_init(&handle);
  handle.color = c;
  handle.width = 3;
  handle.center.x = x + 13;
  handle.center.y = y + 34;
  handle.radius = 9;
  handle.start_angle = 90;
  handle.end_angle = 270;
  lv_draw_arc(layer, &handle);

  box(layer, x + 26, y + 24, x + 34, y + 31, 0, key);  // the keyline round the thermometer where it meets the body
  disc(layer, x + 30, y + 33, 7, key);
  box(layer, x + 28, y + 3, x + 32, y + 31, 2, c);  // stem
  disc(layer, x + 30, y + 33, 5, c);                // bulb
  for (int ty : {8, 13, 18}) segment(layer, x + 34, y + ty, x + 38, y + ty, 2, c);  // scale marks
}

void OemLayout::onNeedleTimer(lv_timer_t* t) {
  OemLayout* self = static_cast<OemLayout*>(lv_timer_get_user_data(t));
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

void OemLayout::onPulseTimer(lv_timer_t* t) {
  OemLayout* self = static_cast<OemLayout*>(lv_timer_get_user_data(t));
  if (!self || !self->symbol_) return;
  const uint8_t opa = gauge::ledOpacity(self->symbol_pulse_, lv_tick_get());
  if (opa == self->symbol_opa_) return;
  self->symbol_opa_ = opa;
  lv_obj_invalidate(self->symbol_);
}

// Lit in the reading's colour, or dark grey when ledState() says off; the timer animates the pulse.
void OemLayout::renderSymbol(const gauge::LedState& s) {
  const uint32_t colour = s.lit ? s.colour : kSymbolOff;
  const gauge::LedPulse pulse = s.lit ? s.pulse : gauge::LedPulse::None;
  if (colour != symbol_colour_) {
    symbol_colour_ = colour;
    lv_obj_invalidate(symbol_);
  }
  if (pulse != symbol_pulse_) {
    symbol_pulse_ = pulse;
    if (pulse == gauge::LedPulse::None) {
      lv_timer_pause(pulse_timer_);
      symbol_opa_ = 255;
      lv_obj_invalidate(symbol_);
    } else {
      lv_timer_resume(pulse_timer_);
    }
  }
}

void OemLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  lines_.render(f);
  if (f.wordmark != word_shown_) {
    word_shown_ = f.wordmark;
    show(word_, word_shown_);
    show(text_, !word_shown_);
  }
  if (f.unit != unit_text_) { unit_text_ = f.unit; lv_label_set_text_static(unit_, f.unit); }
  if (f.scale_visible != labels_shown_) {
    labels_shown_ = f.scale_visible;
    for (lv_obj_t* n : numbers_) show(n, labels_shown_);
    show(unit_, labels_shown_);
  }
  renderSymbol(f.led);
  if (!f.oem_needle.visible) {
    if (needle_shown_) {
      lv_obj_add_flag(needle_, LV_OBJ_FLAG_HIDDEN);
      lv_timer_pause(needle_timer_);
      needle_shown_ = false;
    }
    return;
  }
  target_ = f.oem_needle.fraction;
  if (!needle_shown_) {
    // Appearing (power-up, or after a fault): straight to the reading, with no sweep from anywhere else.
    shown_ = target_;
    placeNeedle(shown_);
    lv_obj_remove_flag(needle_, LV_OBJ_FLAG_HIDDEN);
    needle_shown_ = true;
  } else if (target_ != shown_) {
    lv_timer_resume(needle_timer_);
  }
}
