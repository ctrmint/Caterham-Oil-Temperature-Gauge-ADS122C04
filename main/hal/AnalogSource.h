#pragma once
#include "core/Temperature.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

class AnalogSource {
 public:
  bool begin();
  gauge::Sample read();
 private:
  adc_oneshot_unit_handle_t unit_ = nullptr;
  adc_cali_handle_t sensor_calibration_ = nullptr;
  adc_cali_handle_t supply_calibration_ = nullptr;
  bool available_ = false;
  bool readVolts(adc_channel_t channel, adc_cali_handle_t calibration, float& volts);
};
