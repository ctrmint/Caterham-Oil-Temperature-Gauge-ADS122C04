#pragma once
#include <stdint.h>
#include "core/Temperature.h"
#include "driver/i2c_master.h"

class Ads1115Source {
 public:
  bool begin();
  gauge::Sample read();
 private:
  i2c_master_bus_handle_t bus_ = nullptr;
  i2c_master_dev_handle_t device_ = nullptr;
  bool failing_ = false;
  bool convert(uint16_t mux, float& volts);
};
