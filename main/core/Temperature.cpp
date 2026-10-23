#include "Temperature.h"
#include "Calibration.h"
#include "Config.h"
#include <cmath>

namespace gauge {
Reading convertSample(const Sample &s) {
  Reading r; r.sensor_volts=s.sender_volts; r.supply_volts=s.supply_volts;
  if (!s.adc_ok || !std::isfinite(s.sender_volts) || !std::isfinite(s.supply_volts)) return r;
  if (s.supply_volts<config::kValidSupplyMinV || s.supply_volts>config::kValidSupplyMaxV) {
    r.status=Status::BadSupply;return r;
  }
  if (s.sender_volts < -0.05f || s.sender_volts >= s.supply_volts*config::kOpenVoltageRatio) {
    r.status=Status::OpenCircuit;return r;
  }
  if (s.sender_volts<=0.0f) { r.status=Status::ShortCircuit;return r; }
  r.resistance_ohms=config::kPullUpOhms*s.sender_volts/(s.supply_volts-s.sender_volts);
  if (!std::isfinite(r.resistance_ohms) || r.resistance_ohms>config::kOpenResistanceOhms) {
    r.status=Status::OpenCircuit;return r;
  }
  if (r.resistance_ohms<config::kShortResistanceOhms) {
    r.status=Status::ShortCircuit;return r;
  }
  if (!calibrationValid()) {r.status=Status::NeedsCalibration;return r;}
  float c=0;
  if (!temperatureFromResistance(r.resistance_ohms,c)) {
    r.status=Status::OutOfCalibration;return r;
  }
  r.temperature_c=c;r.has_temperature=true;
  r.status=(c<config::kWarmThresholdC) ? Status::Cold : Status::Normal;
  return r;
}
const char* statusText(Status s) {
  switch(s) {
    case Status::NoAdc: return "ADC UNAVAILABLE";
    case Status::BadSupply: return "SUPPLY FAULT";
    case Status::ShortCircuit: return "SENSOR SHORT";
    case Status::OpenCircuit: return "SENSOR OPEN";
    case Status::OutOfCalibration: return "OUT OF CAL RANGE";
    case Status::NeedsCalibration: return "CALIBRATION REQUIRED";
    case Status::Cold: return "OIL WARMING";
    case Status::Normal: return "NORMAL";
    case Status::Amber: return "HIGH TEMPERATURE";
    case Status::Red: return "CRITICAL TEMP";
  }
  return "UNKNOWN";
}
bool isFault(Status s) {
  return s==Status::NoAdc || s==Status::BadSupply || s==Status::ShortCircuit ||
         s==Status::OpenCircuit || s==Status::OutOfCalibration || s==Status::NeedsCalibration;
}
Reading TemperatureState::update(const Sample &s, uint32_t ms) {
  Reading r=convertSample(s);
  if (r.status==Status::ShortCircuit || r.status==Status::OpenCircuit || r.status==Status::NoAdc ||
      r.status==Status::BadSupply || r.status==Status::NeedsCalibration || r.status==Status::OutOfCalibration) {
    filter_ready_=false;alarm_=pending_=Status::Normal;return r;
  }
  if (!filter_ready_) {filtered_resistance_=r.resistance_ohms;filter_ready_=true;}
  else filtered_resistance_ +=config::kFilterAlpha*(r.resistance_ohms-filtered_resistance_);
  float t=0;
  if (!temperatureFromResistance(filtered_resistance_, t)) {
    r.status=Status::OutOfCalibration;r.has_temperature=false;return r;
  }
  r.temperature_c=t;r.has_temperature=true;
  if (t>max_c_) max_c_=t;
  // Alarms use a debounce for escalation and hysteresis for de-escalation.
  Status target=(t>=config::kRedThresholdC)?Status::Red:
      (t>=config::kAmberThresholdC)?Status::Amber:Status::Normal;
  if (alarm_==Status::Red && t>=config::kRedThresholdC-config::kAlarmResetHysteresisC) target=Status::Red;
  else if (alarm_==Status::Amber && t>=config::kAmberThresholdC-config::kAlarmResetHysteresisC && target==Status::Normal) target=Status::Amber;
  if (target!=pending_) {pending_=target;pending_since_=ms;}
  if (target==Status::Red || (alarm_==Status::Normal && target==Status::Amber)) {
    if (uint32_t(ms-pending_since_)>=config::kAlarmDebounceMs) alarm_=target;
  } else alarm_=target; // falling threshold may clear immediately after hysteresis
  if (alarm_==Status::Red || alarm_==Status::Amber) r.status=alarm_;
  else if (t<config::kWarmThresholdC) r.status=Status::Cold;
  else r.status=Status::Normal;
  return r;
}
} // namespace gauge
