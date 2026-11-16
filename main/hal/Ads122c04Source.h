#pragma once
#include <stdint.h>
#include "core/Temperature.h"
#include "driver/i2c_master.h"

class Ads122c04Source {
 public:
  bool begin();
  gauge::Sample read();
 private:
  i2c_master_bus_handle_t bus_ = nullptr;
  i2c_master_dev_handle_t device_ = nullptr;
  bool failing_ = false;
  bool counter_known_ = false;
  uint8_t counter_ = 0;
  bool readRegister(uint8_t address, uint8_t& value);
  bool readResult(uint8_t& counter, int32_t& code);
  bool convert(uint8_t mux, bool burn_out, float& volts);
};
