#pragma once
#include "core/Temperature.h"
#include "lvgl.h"

class GaugeDisplay {
 public:
  bool begin();
  void render(const gauge::Reading& r, float max_temp_c, bool demo);
 private:
  lv_obj_t* arc_ = nullptr;
  lv_obj_t* temp_ = nullptr;
  lv_obj_t* label_ = nullptr;
  lv_obj_t* subtitle_ = nullptr;
  lv_obj_t* footer_ = nullptr;
  lv_obj_t* demo_ = nullptr;
};
