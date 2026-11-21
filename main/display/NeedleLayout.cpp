#include "display/Layouts.h"
#include <cmath>
#include <cstdio>

using namespace layout;

// Analogue dial (#11): a white scale arc open at the bottom, a minor tick between each pair of numbers, a triangular
// pointer at each of the five numbers, a red needle under a dark hub, and OIL above the unit. No digital readout.
namespace {
constexpr int kC = 233;
constexpr float kPi = 3.14159265f;
constexpr float kStartDeg = 135.0f, kSweepDeg = 270.0f;  // as Sweep: start lower left, open at the bottom
constexpr int kArcR = 216, kArcW = 4;                     // the scale's arc: outer radius and width
constexpr int kArcIn = kArcR - kArcW;                     // ticks and pointers stand on the arc's inner edge
constexpr int kTickLen = 14, kTickW = 3;
constexpr int kPointerH = 16, kPointerHalfW = 8;          // apex towards the centre
constexpr int kApexR = kArcIn - kPointerH;
constexpr int kNumR = 135;                                // numbers (Montserrat 48) centred on this radius
constexpr int kNumInkHalfW = 46, kNumInkHalfH = 20;       // half the widest number's ink ("130", "266")
constexpr int kHubR = 36;
constexpr int kNeedleR = 190, kNeedleW = 7;               // tip radius and thickness
constexpr int kLegendY = 282, kLegendH = 28, kLegendHalfW = 30;  // OIL, then the unit (Montserrat 24)
constexpr int kStatusY = 384;
constexpr int kSin45x1000 = 707;
constexpr uint32_t kTimerMs = 30;
constexpr float kEase = 0.3f;     // the share of the remaining distance moved each frame: it never overshoots
constexpr float kSettled = 0.0005f;
constexpr uint32_t kHubTop = 0x2A2D31, kHubBottom = 0x101214, kHubEdge = 0x3A3F45;
constexpr uint32_t kNeedle = 0xE8312F;

// Each number's ink stays clear of the pointers and of the hub.
static_assert((kApexR - kNumR) * (kApexR - kNumR) >= kNumInkHalfW * kNumInkHalfW + kNumInkHalfH * kNumInkHalfH,
              "Needle numbers must stay inside the pointers");
static_assert((kNumR - kHubR) * (kNumR - kHubR) >= kNumInkHalfW * kNumInkHalfW + kNumInkHalfH * kNumInkHalfH,
              "Needle numbers must stay outside the hub");
static_assert(kNeedleR < kApexR && kNeedleR > kHubR, "the needle reaches from the hub to just inside the pointers");
// The legend sits below the hub, between the two bottom numbers and above the status lines.
static_assert(kLegendY > kC + kHubR, "legend must be below the hub");
static_assert(kLegendHalfW < kNumR * kSin45x1000 / 1000 - kNumInkHalfW, "legend must clear the bottom numbers");
static_assert(kLegendY + 2 * kLegendH <= kStatusY, "legend must be above the status line");
static_assert(kC + kNumR * kSin45x1000 / 1000 + kNumInkHalfH < kStatusY, "bottom numbers must be above the status line");
// The status lines lie on the screen, clear of the scale's ends and their pointers.
static_assert(lines::blockFits(kStatusY), "Needle status lines must fit on the round screen");
static_assert(kC - lines::kStatusW / 2 > kC - kApexR * kSin45x1000 / 1000 + kPointerHalfW,
              "status lines must clear the scale's end pointers");

lv_obj_t* num_label[gauge::kNeedleLabels];
char num_text[gauge::kNeedleLabels][8];
lv_point_precise_t needle_pts[2];
int needle_x1 = -1, needle_y1 = -1;  // the tip as drawn, to skip redundant redraws

float rad(float fraction) { return (kStartDeg + kSweepDeg * fraction) * kPi / 180.0f; }
lv_value_precise_t px(float v) { return static_cast<lv_value_precise_t>(lroundf(v)); }
lv_point_precise_t pt(float x, float y) { return {px(x), px(y)}; }

lv_obj_t* centred_label(lv_obj_t* parent, const lv_font_t* font, uint32_t colour, int dx, int dy) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
  lv_obj_align(l, LV_ALIGN_CENTER, dx, dy);
  return l;
}
} // namespace

void NeedleLayout::build(lv_obj_t* screen, gauge::Unit unit) {
  root_ = lv_obj_create(screen);
  lv_obj_set_size(root_, 466, 466);
  lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);

  // The scale (arc, ticks and pointers) is drawn by one object, so it's a single draw pass with no per-tick objects.
  lv_obj_t* face = lv_obj_create(root_);
  lv_obj_set_size(face, 466, 466);
  lv_obj_set_pos(face, 0, 0);
  lv_obj_set_style_bg_opa(face, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(face, 0, 0);
  lv_obj_set_style_pad_all(face, 0, 0);
  lv_obj_add_event_cb(face, &NeedleLayout::onDrawFace, LV_EVENT_DRAW_MAIN, nullptr);

  // Numbers at their angles, hidden until calibrated.
  for (int i = 0; i < gauge::kNeedleLabels; ++i) {
    const float a = rad(static_cast<float>(i) / (gauge::kNeedleLabels - 1));
    std::snprintf(num_text[i], sizeof(num_text[i]), "%d", gauge::needleLabel(unit, i));
    lv_obj_t* l = centred_label(root_, &lv_font_montserrat_48, kWhite, static_cast<int>(lroundf(kNumR * cosf(a))),
                                static_cast<int>(lroundf(kNumR * sinf(a))));
    lv_label_set_text_static(l, num_text[i]);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    num_label[i] = l;
  }
  labels_shown_ = false;

  lv_obj_t* oil = centred_label(root_, &lv_font_montserrat_24, kWhite, 0, kLegendY + kLegendH / 2 - kC);
  lv_label_set_text_static(oil, "OIL");
  unit_ = centred_label(root_, &lv_font_montserrat_24, kWhite, 0, kLegendY + kLegendH + kLegendH / 2 - kC);
  unit_text_ = gauge::unitLabel(unit);
  lv_label_set_text_static(unit_, unit_text_);

  // The needle, under the hub. Hidden until there's a temperature; it never sweeps at power-up.
  needle_ = lv_line_create(root_);
  lv_obj_set_style_line_width(needle_, kNeedleW, 0);
  lv_obj_set_style_line_color(needle_, lv_color_hex(kNeedle), 0);
  lv_obj_set_style_line_rounded(needle_, true, 0);
  needle_x1 = needle_y1 = -1;
  placeNeedle(0.f);
  lv_obj_add_flag(needle_, LV_OBJ_FLAG_HIDDEN);
  needle_shown_ = false;
  shown_ = target_ = 0.f;

  lv_obj_t* hub = lv_obj_create(root_);
  lv_obj_set_size(hub, 2 * kHubR, 2 * kHubR);
  lv_obj_set_pos(hub, kC - kHubR, kC - kHubR);
  lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(hub, lv_color_hex(kHubTop), 0);
  lv_obj_set_style_bg_grad_color(hub, lv_color_hex(kHubBottom), 0);
  lv_obj_set_style_bg_grad_dir(hub, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(hub, 2, 0);
  lv_obj_set_style_border_color(hub, lv_color_hex(kHubEdge), 0);
  lv_obj_set_style_pad_all(hub, 0, 0);

  lines_.build(root_, kStatusY);
  timer_ = lv_timer_create(&NeedleLayout::onTimer, kTimerMs, this);
  lv_timer_pause(timer_);

  // Presses must reach the screen: nothing may be clickable or scrollable.
  const lv_obj_flag_t off = static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, off);
  const uint32_t count = lv_obj_get_child_count(root_);
  for (uint32_t i = 0; i < count; ++i) lv_obj_remove_flag(lv_obj_get_child(root_, static_cast<int32_t>(i)), off);
}

void NeedleLayout::onDrawFace(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t area;
  lv_obj_get_coords(lv_event_get_target_obj(e), &area);
  const float cx = static_cast<float>(area.x1 + kC), cy = static_cast<float>(area.y1 + kC);

  lv_draw_arc_dsc_t arc;
  lv_draw_arc_dsc_init(&arc);
  arc.color = lv_color_hex(kWhite);
  arc.width = kArcW;
  arc.center.x = area.x1 + kC;
  arc.center.y = area.y1 + kC;
  arc.radius = kArcR;
  arc.start_angle = static_cast<lv_value_precise_t>(kStartDeg);
  arc.end_angle = static_cast<lv_value_precise_t>(kStartDeg + kSweepDeg - 360.0f);
  lv_draw_arc(layer, &arc);

  lv_draw_line_dsc_t tick;
  lv_draw_line_dsc_init(&tick);
  tick.color = lv_color_hex(kWhite);
  tick.width = kTickW;
  for (int i = 0; i < gauge::kNeedleLabels - 1; ++i) {
    const float a = rad((static_cast<float>(i) + 0.5f) / (gauge::kNeedleLabels - 1));
    tick.p1 = pt(cx + (kArcIn - kTickLen) * cosf(a), cy + (kArcIn - kTickLen) * sinf(a));
    tick.p2 = pt(cx + kArcIn * cosf(a), cy + kArcIn * sinf(a));
    lv_draw_line(layer, &tick);
  }

  lv_draw_triangle_dsc_t tri;
  lv_draw_triangle_dsc_init(&tri);
  tri.color = lv_color_hex(kWhite);
  tri.opa = LV_OPA_COVER;
  for (int i = 0; i < gauge::kNeedleLabels; ++i) {
    const float a = rad(static_cast<float>(i) / (gauge::kNeedleLabels - 1));
    const float ux = cosf(a), uy = sinf(a);
    const float base = static_cast<float>(kArcIn + 1);  // overlap the arc by a pixel, so there's no seam
    tri.p[0] = pt(cx + kApexR * ux, cy + kApexR * uy);
    tri.p[1] = pt(cx + base * ux - kPointerHalfW * uy, cy + base * uy + kPointerHalfW * ux);
    tri.p[2] = pt(cx + base * ux + kPointerHalfW * uy, cy + base * uy - kPointerHalfW * ux);
    lv_draw_triangle(layer, &tri);
  }
}

// The line object spans just the needle, padded by its width so it never has a zero-size box (pointing straight up).
void NeedleLayout::placeNeedle(float fraction) {
  const float a = rad(fraction);
  const int x1 = static_cast<int>(lroundf(kC + kNeedleR * cosf(a))), y1 = static_cast<int>(lroundf(kC + kNeedleR * sinf(a)));
  if (x1 == needle_x1 && y1 == needle_y1) return;
  needle_x1 = x1;
  needle_y1 = y1;
  const int mx = (x1 < kC ? x1 : kC) - kNeedleW, my = (y1 < kC ? y1 : kC) - kNeedleW;
  needle_pts[0] = {static_cast<lv_value_precise_t>(kC - mx), static_cast<lv_value_precise_t>(kC - my)};
  needle_pts[1] = {static_cast<lv_value_precise_t>(x1 - mx), static_cast<lv_value_precise_t>(y1 - my)};
  lv_obj_set_pos(needle_, mx, my);
  lv_line_set_points(needle_, needle_pts, 2);
}

void NeedleLayout::onTimer(lv_timer_t* t) {
  NeedleLayout* self = static_cast<NeedleLayout*>(lv_timer_get_user_data(t));
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

void NeedleLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  lines_.render(f);
  if (f.unit != unit_text_) { unit_text_ = f.unit; lv_label_set_text_static(unit_, f.unit); }
  if (f.scale_visible != labels_shown_) {
    labels_shown_ = f.scale_visible;
    for (int i = 0; i < gauge::kNeedleLabels; ++i) {
      if (labels_shown_) lv_obj_remove_flag(num_label[i], LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(num_label[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (!f.needle.visible) {
    if (needle_shown_) {
      lv_obj_add_flag(needle_, LV_OBJ_FLAG_HIDDEN);
      lv_timer_pause(timer_);
      needle_shown_ = false;
    }
    return;
  }
  target_ = f.needle.fraction;
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
