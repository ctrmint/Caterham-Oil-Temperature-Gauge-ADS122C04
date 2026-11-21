#include "display/Layouts.h"
#include <cstdio>
#include <cstring>

using namespace layout;

namespace {
lv_obj_t* centred_line(lv_obj_t* parent, int w, int y, uint32_t colour) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(l, w, lines::kTextH);
  lv_obj_set_pos(l, lines::kCentre - w / 2, y);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
  return l;
}
} // namespace

void StatusLines::build(lv_obj_t* parent, int status_y) {
  status_ = centred_line(parent, lines::kStatusW, status_y, kAmber);
  lv_label_set_text_static(status_, "SENSOR CHECK");
  footer_ = centred_line(parent, lines::kStatusW, status_y + lines::kGap, kMute);
  std::snprintf(footer_text_, sizeof(footer_text_), "NOT CALIBRATED");
  lv_label_set_text_static(footer_, footer_text_);
  demo_ = centred_line(parent, lines::kDemoW, status_y + 2 * lines::kGap, kRed);
  lv_label_set_text_static(demo_, "DEMO ONLY - NOT REAL");
  lv_obj_add_flag(demo_, LV_OBJ_FLAG_HIDDEN);
  status_text_ = nullptr;
  status_colour_ = kAmber;
  demo_shown_ = false;
}

void StatusLines::render(const DisplayFrame& f) {
  if (!status_) return;
  if (f.status != status_text_) { status_text_ = f.status; lv_label_set_text_static(status_, f.status); }
  if (f.colour != status_colour_) {
    status_colour_ = f.colour;
    lv_obj_set_style_text_color(status_, lv_color_hex(f.colour), 0);
  }
  if (std::strncmp(footer_text_, f.footer, sizeof(footer_text_)) != 0) {
    std::snprintf(footer_text_, sizeof(footer_text_), "%s", f.footer);
    lv_label_set_text_static(footer_, footer_text_);
  }
  if (f.demo != demo_shown_) {
    demo_shown_ = f.demo;
    if (f.demo) lv_obj_remove_flag(demo_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(demo_, LV_OBJ_FLAG_HIDDEN);
  }
}
