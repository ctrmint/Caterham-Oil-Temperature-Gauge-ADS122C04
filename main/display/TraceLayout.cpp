#include "display/Layouts.h"
#include "Config.h"
#include <cmath>
#include <cstdio>

using namespace layout;

// Time-based trace (#12): the last 60 s, one point every 5 s, newest on the right, on a 30 degC window. A thick white
// line, the newest point as a dot in the reading's colour (a ring when it's beyond the table), amber and red threshold
// lines when they're in the window, and grid lines every 10 degC. No digital readout.
namespace {
constexpr int kPlotX = 96, kPlotY = 112, kPlotW = 288, kPlotH = 180;
constexpr int kStepX = kPlotW / (gauge::kTracePoints - 1);
constexpr int kLineW = 6, kDotD = 16, kDotRing = 3, kThresholdH = 2;
constexpr int kGridLabelX = 46, kGridLabelW = 42, kGridLabelH = 16;  // right-aligned, 8 px left of the plot
constexpr int kGridLines = 4;  // multiples of 10 degC in a 30 degC window
constexpr int kTitleY = 76, kTitleW = 160, kTimeY = kPlotY + kPlotH + 8, kTimeW = 60;
constexpr int kStatusY = 330;
constexpr uint32_t kGrid = 0x1C2A33;

static_assert(kStepX * (gauge::kTracePoints - 1) == kPlotW, "points must be evenly spaced across the plot");
static_assert(kGridLabelX + kGridLabelW < kPlotX, "grid labels must sit left of the plot");
// Everything lies on the round screen: the grid labels' outer corners, the plot's corners with the dot, the title.
static_assert(lines::onScreen(kGridLabelX, kPlotY - kGridLabelH / 2) &&
              lines::onScreen(kGridLabelX, kPlotY + kPlotH + kGridLabelH / 2), "grid labels must fit on the screen");
static_assert(lines::onScreen(kPlotX + kPlotW + kDotD / 2, kPlotY - kDotD / 2) &&
              lines::onScreen(kPlotX + kPlotW + kDotD / 2, kPlotY + kPlotH + kDotD / 2), "plot must fit on the screen");
static_assert(lines::fits(kTitleW, kTitleY) && kTitleY + lines::kTextH < kPlotY - kGridLabelH / 2, "title must fit above the plot");
static_assert(lines::fits(kPlotW, kTimeY) && kTimeY + lines::kTextH < kStatusY, "time labels must fit under the plot");
static_assert(lines::blockFits(kStatusY), "Trace status lines must fit on the round screen");

lv_obj_t* grid[kGridLines];
lv_obj_t* grid_label[kGridLines];
char grid_text[kGridLines][8];
lv_obj_t* threshold[2];  // amber, red
lv_obj_t* seg[gauge::kTracePoints - 1];
lv_point_precise_t seg_pts[gauge::kTracePoints - 1][2];

lv_obj_t* label(lv_obj_t* parent, int w, int x, int y, lv_text_align_t align, uint32_t colour, const lv_font_t* font) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(l, w, lines::kTextH);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_style_text_align(l, align, 0);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
  return l;
}

lv_obj_t* bar(lv_obj_t* parent, int h, uint32_t colour) {
  lv_obj_t* b = lv_obj_create(parent);
  lv_obj_set_size(b, kPlotW, h);
  lv_obj_set_pos(b, kPlotX, kPlotY);
  lv_obj_set_style_radius(b, 0, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(colour), 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
  return b;
}

void show(lv_obj_t* o, bool visible) {
  if (visible) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

// The y pixel of a temperature in the window, clamped to the plot's edges.
int yOf(const gauge::TraceWindow& w, float celsius) {
  const float v = celsius < w.lo_c ? w.lo_c : (celsius > w.hi_c ? w.hi_c : celsius);
  return kPlotY + static_cast<int>(lroundf((w.hi_c - v) / (w.hi_c - w.lo_c) * kPlotH));
}
} // namespace

void TraceLayout::build(lv_obj_t* screen, gauge::Unit unit) {
  unit_ = unit;
  root_ = lv_obj_create(screen);
  lv_obj_set_size(root_, 466, 466);
  lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);

  title_ = label(root_, kTitleW, lines::kCentre - kTitleW / 2, kTitleY, LV_TEXT_ALIGN_CENTER, kWhite, &lv_font_montserrat_16);
  title_text_ = gauge::titleText(unit);
  lv_label_set_text_static(title_, title_text_);

  for (int i = 0; i < kGridLines; ++i) {
    grid[i] = bar(root_, 1, kGrid);
    grid_label[i] = label(root_, kGridLabelW, kGridLabelX, kPlotY, LV_TEXT_ALIGN_RIGHT, kMute, &lv_font_montserrat_14);
    grid_text[i][0] = '\0';
    lv_label_set_text_static(grid_label[i], grid_text[i]);
    lv_obj_add_flag(grid_label[i], LV_OBJ_FLAG_HIDDEN);
  }
  threshold[0] = bar(root_, kThresholdH, kAmber);
  threshold[1] = bar(root_, kThresholdH, kRed);

  for (int i = 0; i < gauge::kTracePoints - 1; ++i) {
    seg[i] = lv_line_create(root_);
    lv_obj_set_style_line_width(seg[i], kLineW, 0);
    lv_obj_set_style_line_color(seg[i], lv_color_hex(kWhite), 0);
    lv_obj_set_style_line_rounded(seg[i], true, 0);
    lv_obj_add_flag(seg[i], LV_OBJ_FLAG_HIDDEN);
  }

  dot_ = lv_obj_create(root_);
  lv_obj_set_size(dot_, kDotD, kDotD);
  lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_all(dot_, 0, 0);
  lv_obj_add_flag(dot_, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* older = label(root_, kTimeW, kPlotX, kTimeY, LV_TEXT_ALIGN_LEFT, kMute, &lv_font_montserrat_14);
  lv_label_set_text_static(older, "-60 s");
  lv_obj_t* now = label(root_, kTimeW, kPlotX + kPlotW - kTimeW, kTimeY, LV_TEXT_ALIGN_RIGHT, kMute, &lv_font_montserrat_14);
  lv_label_set_text_static(now, "now");

  lines_.build(root_, kStatusY);
  drawn_ = false;

  // Presses must reach the screen: nothing may be clickable or scrollable.
  const lv_obj_flag_t off = static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, off);
  const uint32_t count = lv_obj_get_child_count(root_);
  for (uint32_t i = 0; i < count; ++i) lv_obj_remove_flag(lv_obj_get_child(root_, static_cast<int32_t>(i)), off);
}

void TraceLayout::drawTrace(const gauge::TraceHistory& h, bool scale_visible) {
  const gauge::TraceWindow w = h.window();

  // Grid lines at the multiples of 10 degC inside the window, labelled in the display unit once calibrated.
  int g = 0;
  for (float v = std::ceil(w.lo_c / 10.f) * 10.f; v <= w.hi_c + 0.001f && g < kGridLines; v += 10.f, ++g) {
    const int y = yOf(w, v);
    lv_obj_set_y(grid[g], y);
    show(grid[g], true);
    long n = 0;
    if (scale_visible && gauge::roundHalfAway(gauge::toUnit(v, unit_), n)) {
      std::snprintf(grid_text[g], sizeof(grid_text[g]), "%ld", n);
      lv_label_set_text_static(grid_label[g], grid_text[g]);
      lv_obj_set_y(grid_label[g], y - kGridLabelH / 2);
      show(grid_label[g], true);
    } else {
      show(grid_label[g], false);
    }
  }
  for (; g < kGridLines; ++g) { show(grid[g], false); show(grid_label[g], false); }

  // Threshold lines, from the alarm settings, when they fall inside the window.
  const float thresholds[2] = {config::kAmberThresholdC, config::kRedThresholdC};
  for (int i = 0; i < 2; ++i) {
    const bool in = scale_visible && gauge::traceShows(w, thresholds[i]);
    if (in) lv_obj_set_y(threshold[i], yOf(w, thresholds[i]) - kThresholdH / 2);
    show(threshold[i], in);
  }

  // The line: one segment between each pair of neighbouring points, none across a gap. The newest point is on the right.
  const int first_slot = gauge::kTracePoints - h.count();
  for (int s = 0; s < gauge::kTracePoints - 1; ++s) {
    const gauge::TracePoint& a = h.point(s - first_slot);
    const gauge::TracePoint& b = h.point(s + 1 - first_slot);
    if (!a.present || !b.present) { show(seg[s], false); continue; }
    const int x0 = kPlotX + s * kStepX, x1 = x0 + kStepX;
    const int y0 = yOf(w, a.celsius), y1 = yOf(w, b.celsius);
    // Padded by the line width, so a flat segment never has a zero-height box.
    const int mx = x0 - kLineW, my = (y0 < y1 ? y0 : y1) - kLineW;
    seg_pts[s][0] = {static_cast<lv_value_precise_t>(x0 - mx), static_cast<lv_value_precise_t>(y0 - my)};
    seg_pts[s][1] = {static_cast<lv_value_precise_t>(x1 - mx), static_cast<lv_value_precise_t>(y1 - my)};
    lv_obj_set_pos(seg[s], mx, my);
    lv_line_set_points(seg[s], seg_pts[s], 2);
    show(seg[s], true);
  }

  // The newest point: a dot in its reading's colour, or a ring when it's beyond the table (shown at the table maximum).
  const gauge::TracePoint& p = h.point(h.count() - 1);
  if (h.count() > 0 && p.present) {
    lv_obj_set_pos(dot_, kPlotX + kPlotW - kDotD / 2, yOf(w, p.celsius) - kDotD / 2);
    lv_obj_set_style_bg_color(dot_, lv_color_hex(p.colour), 0);
    lv_obj_set_style_bg_opa(dot_, p.above_range ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dot_, lv_color_hex(p.colour), 0);
    lv_obj_set_style_border_width(dot_, p.above_range ? kDotRing : 0, 0);
    show(dot_, true);
  } else {
    show(dot_, false);
  }
}

void TraceLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  lines_.render(f);
  if (f.title != title_text_) { title_text_ = f.title; lv_label_set_text_static(title_, f.title); }
  if (!f.trace) return;
  if (!drawn_ || f.trace->version() != version_ || f.scale_visible != scale_shown_) {
    drawTrace(*f.trace, f.scale_visible);
    version_ = f.trace->version();
    scale_shown_ = f.scale_visible;
    drawn_ = true;
  }
}
