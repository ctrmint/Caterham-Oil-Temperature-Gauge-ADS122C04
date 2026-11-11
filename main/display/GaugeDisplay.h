#pragma once
#include "core/DisplayText.h"
#include "core/Temperature.h"
#include "lvgl.h"

class GaugeDisplay {
 public:
  bool begin(gauge::Unit unit = gauge::Unit::Celsius);
  void render(const gauge::Reading& r, float max_temp_c, bool max_above_range, bool demo);
 private:
  lv_obj_t* arc_ = nullptr;
  lv_obj_t* cells_[gauge::kReadingCells] = {};
  gauge::Unit unit_ = gauge::Unit::Celsius;
  char footer_text_[gauge::kFooterTextSize] = {};
  lv_obj_t* label_ = nullptr;
  lv_obj_t* subtitle_ = nullptr;
  lv_obj_t* footer_ = nullptr;
  lv_obj_t* demo_ = nullptr;
};
