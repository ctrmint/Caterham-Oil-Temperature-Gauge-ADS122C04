#include "display/Layouts.h"

using namespace layout;

namespace {
// Classic geometry on the 466 px screen; y offsets are from the screen centre. The ring's inner radius is
// (kArcSize - 2 * kArcWidth) / 2 = 214 px. Digit block (ReadingDigits): kBlockW by kGlyphH, centred on the screen centre.
constexpr int kArcSize = 456, kArcWidth = 14;
constexpr int kInner = (kArcSize - 2 * kArcWidth) / 2;
using digits::kGlyphH;
using digits::kBlockW;
constexpr int kClear = kInner - 4;
static_assert((kBlockW / 2) * (kBlockW / 2) + (kGlyphH / 2) * (kGlyphH / 2) <= kClear * kClear,
              "reading block must fit inside the ring");
// Text boxes (width, height, top y from the centre). Each box is centred horizontally.
constexpr int kTitleW = 160, kTitleH = 28, kTitleTop = -kGlyphH / 2 - 6 - kTitleH;
constexpr int kDemoW = 220, kDemoH = 18, kDemoTop = kTitleTop - 4 - kDemoH;
constexpr int kStatusW = 300, kStatusH = 18, kStatusTop = kGlyphH / 2 + 6;
constexpr int kFooterW = 300, kFooterH = 18, kFooterTop = kStatusTop + kStatusH + 4;
// Worst corner of every text box is inside the ring with a 4 px margin.
constexpr bool boxInside(int w, int top, int h) {
  const int x = w / 2;
  const int y = (top < 0 ? -top : top) > ((top + h) < 0 ? -(top + h) : (top + h)) ? (top < 0 ? -top : top) : ((top + h) < 0 ? -(top + h) : (top + h));
  return x * x + y * y < kClear * kClear;
}
static_assert(boxInside(kTitleW, kTitleTop, kTitleH) && boxInside(kDemoW, kDemoTop, kDemoH) &&
              boxInside(kStatusW, kStatusTop, kStatusH) && boxInside(kFooterW, kFooterTop, kFooterH),
              "text boxes must fit inside the ring");
static_assert(kDemoTop + kDemoH < kTitleTop && kTitleTop + kTitleH < -kGlyphH / 2 && kGlyphH / 2 < kStatusTop &&
              kStatusTop + kStatusH < kFooterTop, "text boxes must not overlap");

void center_text(lv_obj_t* label, int w, int top, int h, const lv_font_t* font, uint32_t color) {
  lv_obj_set_size(label, w, h);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, top + h / 2);
}
} // namespace

void ClassicLayout::build(lv_obj_t* screen, gauge::Unit unit) {
  root_ = lv_obj_create(screen);
  lv_obj_set_size(root_, 466, 466);
  lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_remove_flag(root_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

  arc_ = lv_arc_create(root_);
  lv_obj_set_size(arc_, kArcSize, kArcSize);
  lv_obj_align(arc_, LV_ALIGN_CENTER, 0, 0);
  lv_arc_set_range(arc_, 0, 160);
  lv_arc_set_rotation(arc_, 135);
  lv_arc_set_bg_angles(arc_, 0, 270);
  lv_arc_set_value(arc_, 0);
  lv_obj_remove_flag(arc_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_style_arc_width(arc_, kArcWidth, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc_, kArcWidth, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc_, lv_color_hex(0x22313A), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc_, lv_color_hex(kGreen), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(arc_, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_remove_style(arc_, nullptr, LV_PART_KNOB);

  title_ = lv_label_create(root_);
  lv_label_set_text_static(title_, gauge::titleText(unit));
  center_text(title_, kTitleW, kTitleTop, kTitleH, &lv_font_montserrat_24, kWhite);
  digits_.build(root_);
  subtitle_ = lv_label_create(root_);
  lv_label_set_text(subtitle_, "SENSOR CHECK");
  center_text(subtitle_, kStatusW, kStatusTop, kStatusH, &lv_font_montserrat_16, kAmber);
  footer_ = lv_label_create(root_);
  lv_label_set_text(footer_, "NOT CALIBRATED");
  center_text(footer_, kFooterW, kFooterTop, kFooterH, &lv_font_montserrat_16, kMute);
  demo_ = lv_label_create(root_);
  lv_label_set_text(demo_, "DEMO ONLY - NOT REAL");
  center_text(demo_, kDemoW, kDemoTop, kDemoH, &lv_font_montserrat_16, kRed);
  lv_obj_add_flag(demo_, LV_OBJ_FLAG_HIDDEN);
  // Presses must reach the screen: no child may be clickable or scrollable.
  const uint32_t count = lv_obj_get_child_count(root_);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_remove_flag(lv_obj_get_child(root_, static_cast<int32_t>(i)),
                       static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  }
}

void ClassicLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  lv_arc_set_value(arc_, f.classic_arc);
  lv_obj_set_style_arc_color(arc_, lv_color_hex(f.colour), LV_PART_INDICATOR);
  lv_label_set_text_static(title_, f.title);
  digits_.render(f.cells, f.colour);
  lv_label_set_text_static(footer_, f.footer);
  lv_label_set_text_static(subtitle_, f.status);
  lv_obj_set_style_text_color(subtitle_, lv_color_hex(f.colour), 0);
  if (f.demo) lv_obj_remove_flag(demo_, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(demo_, LV_OBJ_FLAG_HIDDEN);
}
