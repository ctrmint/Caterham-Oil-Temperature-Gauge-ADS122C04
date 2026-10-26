#pragma once
#include <stdint.h>
namespace gauge {
enum class Status : uint8_t {
  NoAdc, BadSupply, GroundFault, ShortCircuit, OpenCircuit, NeedsCalibration,
  BelowRange, AboveRange, Cold, Normal, Amber, Red
};
struct Reading {
  Status status = Status::NoAdc;
  Status alarm = Status::Normal;   // latched alarm level: Normal, Amber or Red
  float pullup_volts = 0;
  float sensor_volts = 0;
  float supply_volts = 0;
  float ground_offset_volts = 0;
  float resistance_ohms = 0;
  float temperature_c = 0;
  bool has_temperature = false;
};
// ADS1115 readings: Vp = A0 - A3 across the pull-up, Vs = A3 - A1 across the sender,
// supply = A0 single-ended.
struct Sample { bool adc_ok; float pullup_volts; float sender_volts; float supply_volts; };
Reading convertSample(const Sample &sample);
const char* statusText(Status value);
bool isFault(Status value);
class TemperatureState {
 public:
  Reading update(const Sample &s, uint32_t ms);
  float maximumC() const { return max_c_; }
  bool maximumAboveRange() const { return max_above_range_; }
 private:
  Status targetFor(float t) const;
  void updateAlarm(Status target, uint32_t ms);
  void reset();
  float filtered_resistance_=0;
  bool filter_ready_=false;
  Status alarm_=Status::Normal;
  bool amber_timing_=false;
  bool red_timing_=false;
  uint32_t amber_since_=0;
  uint32_t red_since_=0;
  bool in_fault_=false;
  uint32_t fault_since_=0;
  float max_c_=-999;
  bool max_above_range_=false;
};
} // namespace gauge
