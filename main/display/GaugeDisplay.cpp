#include "display/GaugeDisplay.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_err.h"
#include <cmath>

using namespace layout;

bool GaugeDisplay::begin(gauge::Unit unit, gauge::Layout layout) {
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
  classic_.build(root, unit_);
  sweep_.build(root, unit_);
  classic_.setVisible(layout == gauge::Layout::Classic);
  sweep_.setVisible(layout == gauge::Layout::Sweep);
  built_ = true;
  bsp_display_unlock();
  return true;
}

void GaugeDisplay::render(const gauge::Reading& r, float max_temp_c, bool max_above_range, bool demo) {
  if (!built_ || bsp_display_lock(100) != ESP_OK) return;
  DisplayFrame f;
  f.colour = kGreen;
  if (gauge::isFault(r.status) || r.status == gauge::Status::AboveRange) f.colour = kAmber;
  else if (r.status == gauge::Status::BelowRange) f.colour = kMute;
  if (r.alarm == gauge::Status::Amber) f.colour = kAmber;
  if (r.alarm == gauge::Status::Red) f.colour = kRed;
  if (r.has_temperature) {
    f.classic_arc = static_cast<int>(fmaxf(0.0f,fminf(160.0f,r.temperature_c)));
  } else if (r.status == gauge::Status::AboveRange) {
    f.classic_arc = 160;
  } else {
    f.classic_arc = 0;
  }
  f.lit_segments = gauge::litSegments(r,unit_);
  f.scale_visible = gauge::scaleLabelsVisible();
  char text[gauge::kReadingTextSize];
  gauge::readingText(r,unit_,text,sizeof(text));
  gauge::readingCells(text,f.cells,sizeof(f.cells));
  gauge::footerText(r,max_temp_c,max_above_range,unit_,footer_text_,sizeof(footer_text_));
  f.title = gauge::titleText(unit_);
  f.unit = gauge::unitLabel(unit_);
  f.status = gauge::statusText(r.status);
  f.footer = footer_text_;
  f.demo = demo;
  classic_.render(f);
  sweep_.render(f);
  bsp_display_unlock();
}
