#pragma once
#include "core/DisplayText.h"
#include "core/Temperature.h"
#include "display/Layouts.h"
#include "lvgl.h"

class GaugeDisplay {
 public:
  bool begin(gauge::Unit unit = gauge::Unit::Celsius);
  void render(const gauge::Reading& r, float max_temp_c, bool max_above_range, bool demo);
 private:
  ClassicLayout classic_;
  bool built_ = false;
  gauge::Unit unit_ = gauge::Unit::Celsius;
  char footer_text_[gauge::kFooterTextSize] = {};
};
