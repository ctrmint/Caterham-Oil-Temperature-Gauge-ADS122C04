#include "display/GaugeDisplay.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "Config.h"
#include "core/Calibration.h"
#include "esp_err.h"
#include <cstdio>
#include <cstring>
#include <cmath>

namespace {
constexpr uint32_t kBg = 0x080E15;
constexpr uint32_t kWhite = 0xF2F6F8;
constexpr uint32_t kMute = 0x8597A3;
constexpr uint32_t kGreen = 0x24CA89;
constexpr uint32_t kAmber = 0xF4B246;
constexpr uint32_t kRed = 0xF04B4B;

void center_text(lv_obj_t* label, int y, const lv_font_t* font, uint32_t color) {
  lv_obj_set_width(label, 360);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, y);
}

// Static one-character strings, so the cell labels never allocate. Index i is kCellChars[i].
constexpr char kCellChars[] = " 0123456789-<>";
const char* const kCellText[] = {" ", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "-", "<", ">"};
const char* cellText(char ch) {
  const char* p = strchr(kCellChars, ch);
  return (ch != '\0' && p) ? kCellText[p - kCellChars] : kCellText[0];
}
} // namespace

bool GaugeDisplay::begin(gauge::Unit unit) {
  unit_ = unit;
  // Vendor BSP initializes QSPI CO5300 display, CST9217 touch and LVGL task.
  // BSP display_start is the exact API used by Waveshare's IDF v9 demo.
  if (bsp_display_start() == nullptr) return false;
  // Waveshare BSP 3.x reports esp_err_t, not a bool: ESP_OK == 0.
  if (bsp_display_lock(2000) != ESP_OK) return false;
  lv_obj_t* root = lv_screen_active();
  lv_obj_set_style_bg_color(root, lv_color_hex(kBg), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

  arc_ = lv_arc_create(root);
  lv_obj_set_size(arc_, 430, 430);
  lv_obj_align(arc_, LV_ALIGN_CENTER, 0, 0);
  lv_arc_set_range(arc_, 0, 160);
  lv_arc_set_rotation(arc_, 135);
  lv_arc_set_bg_angles(arc_, 0, 270);
  lv_arc_set_value(arc_, 0);
  lv_obj_remove_flag(arc_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc_, 18, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc_, 18, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc_, lv_color_hex(0x22313A), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc_, lv_color_hex(kGreen), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(arc_, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_remove_style(arc_, nullptr, LV_PART_KNOB);

  label_ = lv_label_create(root);
  lv_label_set_text_static(label_, gauge::titleText(unit_));
  center_text(label_, -105, &lv_font_montserrat_24, kWhite);
  // Equal-width cells: the widest digit-font glyph of the reading characters.
  const lv_font_t* digit_font = &lv_font_montserrat_48;
  int32_t cell_w = 0;
  for (const char* p = kCellChars + 1; *p; ++p) {
    const int32_t w = static_cast<int32_t>(lv_font_get_glyph_width(digit_font, static_cast<uint32_t>(*p), 0));
    if (w > cell_w) cell_w = w;
  }
  const int32_t n = static_cast<int32_t>(gauge::kReadingCells);
  for (int32_t i = 0; i < n; ++i) {
    lv_obj_t* cell = lv_label_create(root);
    lv_label_set_long_mode(cell, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(cell, cell_w);
    lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(cell, digit_font, 0);
    lv_obj_set_style_text_color(cell, lv_color_hex(kWhite), 0);
    lv_obj_align(cell, LV_ALIGN_CENTER, (2 * i - (n - 1)) * cell_w / 2, -26);
    lv_label_set_text_static(cell, kCellText[0]);
    cells_[i] = cell;
  }
  subtitle_ = lv_label_create(root);
  lv_label_set_text(subtitle_, "SENSOR CHECK");
  center_text(subtitle_, 44, &lv_font_montserrat_16, kAmber);
  footer_ = lv_label_create(root);
  lv_label_set_text(footer_, "NOT CALIBRATED");
  center_text(footer_, 110, &lv_font_montserrat_16, kMute);
  demo_ = lv_label_create(root);
  lv_label_set_text(demo_, "DEMO ONLY - NOT REAL");
  center_text(demo_, -165, &lv_font_montserrat_16, kRed);
  lv_obj_add_flag(demo_, LV_OBJ_FLAG_HIDDEN);
  bsp_display_unlock();
  return true;
}

void GaugeDisplay::render(const gauge::Reading& r, float max_temp_c, bool max_above_range, bool demo) {
  if (!cells_[0] || bsp_display_lock(100) != ESP_OK) return;
  // Built-in Montserrat fonts have the degree sign but no Greek letters, so write "ohm".
  uint32_t c = kGreen;
  if (gauge::isFault(r.status) || r.status == gauge::Status::AboveRange) c = kAmber;
  else if (r.status == gauge::Status::BelowRange) c = kMute;
  if (r.alarm == gauge::Status::Amber) c = kAmber;
  if (r.alarm == gauge::Status::Red) c = kRed;
  if (r.has_temperature) {
    lv_arc_set_value(arc_,static_cast<int>(fmaxf(0.0f,fminf(160.0f,r.temperature_c))));
  } else if (r.status == gauge::Status::AboveRange) {
    lv_arc_set_value(arc_,160);
  } else {
    lv_arc_set_value(arc_,0);
  }
  char text[gauge::kReadingTextSize];
  char cells[gauge::kReadingCells + 1];
  gauge::readingText(r,unit_,text,sizeof(text));
  gauge::readingCells(text,cells,sizeof(cells));
  for (size_t i = 0; i < gauge::kReadingCells; ++i) {
    lv_label_set_text_static(cells_[i],cellText(cells[i]));
    lv_obj_set_style_text_color(cells_[i],lv_color_hex(c),0);
  }
  gauge::footerText(r,max_temp_c,max_above_range,unit_,footer_text_,sizeof(footer_text_));
  lv_label_set_text_static(footer_,footer_text_);
  lv_obj_set_style_arc_color(arc_,lv_color_hex(c),LV_PART_INDICATOR);
  lv_label_set_text(subtitle_,gauge::statusText(r.status));
  lv_obj_set_style_text_color(subtitle_,lv_color_hex(c),0);
  if (demo) lv_obj_remove_flag(demo_,LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(demo_,LV_OBJ_FLAG_HIDDEN);
  bsp_display_unlock();
}
