#pragma once
#include <stdint.h>
namespace gauge {
enum class Status : uint8_t {
  NoAdc, BadSupply, ShortCircuit, OpenCircuit, OutOfCalibration,
  NeedsCalibration, Cold, Normal, Amber, Red
};
struct Reading {
  Status status = Status::NoAdc;
  float sensor_volts = 0;
  float supply_volts = 0;
  float resistance_ohms = 0;
  float temperature_c = 0;
  bool has_temperature = false;
};
struct Sample { bool adc_ok; float sender_volts; float supply_volts; };
Reading convertSample(const Sample &sample);
const char* statusText(Status value);
bool isFault(Status value);
class TemperatureState {
 public:
  Reading update(const Sample &s, uint32_t ms);
  float maximumC() const { return max_c_; }
 private:
  float filtered_resistance_=0;
  bool filter_ready_=false;
  Status alarm_=Status::Normal;
  Status pending_=Status::Normal;
  uint32_t pending_since_=0;
  float max_c_=-999;
};
} // namespace gauge
