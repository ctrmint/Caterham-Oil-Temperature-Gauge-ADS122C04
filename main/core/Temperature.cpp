#include "Temperature.h"
#include "Calibration.h"
#include "Config.h"
#include <cmath>

namespace gauge {
namespace {
int rank(Status s) { return s==Status::Red ? 2 : s==Status::Amber ? 1 : 0; }
}

Reading convertSample(const Sample &s) {
  Reading r; r.pullup_volts=s.pullup_volts; r.sensor_volts=s.sender_volts; r.supply_volts=s.supply_volts;
  if (!s.adc_ok || !std::isfinite(s.pullup_volts) || !std::isfinite(s.sender_volts) ||
      !std::isfinite(s.supply_volts)) return r;
  if (s.supply_volts<config::kValidSupplyMinV || s.supply_volts>config::kValidSupplyMaxV) {
    r.status=Status::BadSupply;return r;
  }
  // Vp + Vs = A0 - A1, so the remainder of the rail is the tank ground offset.
  r.ground_offset_volts=s.supply_volts-(s.pullup_volts+s.sender_volts);
  if (r.ground_offset_volts<config::kGroundOffsetMinV || r.ground_offset_volts>config::kGroundOffsetMaxV) {
    r.status=Status::GroundFault;return r;
  }
  if (s.sender_volts<=0.0f) { r.status=Status::ShortCircuit;return r; }
  if (s.pullup_volts<=0.0f) { r.status=Status::OpenCircuit;return r; }
  r.resistance_ohms=config::kPullUpOhms*s.sender_volts/s.pullup_volts;
  if (!std::isfinite(r.resistance_ohms) || r.resistance_ohms>config::kOpenResistanceOhms) {
    r.status=Status::OpenCircuit;return r;
  }
  if (r.resistance_ohms<config::kShortResistanceOhms) {
    r.status=Status::ShortCircuit;return r;
  }
  if (!calibrationValid()) {r.status=Status::NeedsCalibration;return r;}
  const int side=calibrationSide(r.resistance_ohms);
  if (side<0) {r.status=Status::BelowRange;return r;}
  if (side>0) {r.status=Status::AboveRange;return r;}
  float c=0;
  if (!temperatureFromResistance(r.resistance_ohms,c)) {r.status=Status::AboveRange;return r;}
  r.temperature_c=c;r.has_temperature=true;
  r.status=(c<config::kWarmThresholdC) ? Status::Cold : Status::Normal;
  return r;
}
const char* statusText(Status s) {
  switch(s) {
    case Status::NoAdc: return "ADC UNAVAILABLE";
    case Status::BadSupply: return "SUPPLY FAULT";
    case Status::GroundFault: return "GROUND FAULT";
    case Status::ShortCircuit: return "SENSOR SHORT";
    case Status::OpenCircuit: return "SENSOR OPEN";
    case Status::NeedsCalibration: return "CALIBRATION REQUIRED";
    case Status::BelowRange: return "BELOW CAL RANGE";
    case Status::AboveRange: return "ABOVE CAL RANGE";
    case Status::Cold: return "OIL WARMING";
    case Status::Normal: return "NORMAL";
    case Status::Amber: return "HIGH TEMPERATURE";
    case Status::Red: return "CRITICAL TEMP";
  }
  return "UNKNOWN";
}
bool isFault(Status s) {
  return s==Status::NoAdc || s==Status::BadSupply || s==Status::GroundFault ||
         s==Status::ShortCircuit || s==Status::OpenCircuit || s==Status::NeedsCalibration;
}
void TemperatureState::reset() {
  filter_ready_=false;alarm_=Status::Normal;amber_timing_=red_timing_=false;below_hold_=false;
}
Status TemperatureState::targetFor(float t) const {
  // Hysteresis: an active alarm holds until the temperature falls below its threshold
  // minus the hysteresis band, and Red steps down through Amber, not straight to Normal.
  const float h=config::kAlarmResetHysteresisC;
  if (t>=config::kRedThresholdC || (alarm_==Status::Red && t>=config::kRedThresholdC-h))
    return Status::Red;
  if (t>=config::kAmberThresholdC || (alarm_!=Status::Normal && t>=config::kAmberThresholdC-h))
    return Status::Amber;
  return Status::Normal;
}
void TemperatureState::updateAlarm(Status target, uint32_t ms) {
  // Escalation needs the level to be held for the debounce time; de-escalation is immediate.
  if (rank(target)>=1) { if (!amber_timing_) {amber_timing_=true;amber_since_=ms;} }
  else amber_timing_=false;
  if (rank(target)>=2) { if (!red_timing_) {red_timing_=true;red_since_=ms;} }
  else red_timing_=false;
  Status confirmed=Status::Normal;
  if (amber_timing_ && uint32_t(ms-amber_since_)>=config::kAlarmDebounceMs) confirmed=Status::Amber;
  if (red_timing_ && uint32_t(ms-red_since_)>=config::kAlarmDebounceMs) confirmed=Status::Red;
  const Status lowered=rank(target)<rank(alarm_) ? target : alarm_;
  alarm_=rank(confirmed)>rank(lowered) ? confirmed : lowered;
}
Reading TemperatureState::update(const Sample &s, uint32_t ms) {
  Reading r=convertSample(s);
  if (isFault(r.status)) {
    // Brief faults (e.g. a vibrating connector) keep the filter and alarm state.
    if (!in_fault_) {in_fault_=true;fault_since_=ms;}
    if (uint32_t(ms-fault_since_)>=config::kFaultResetMs) reset();
    r.alarm=alarm_;
    return r;
  }
  in_fault_=false;
  // A latched alarm survives a cold raw sample (e.g. a sender wire open to a low value)
  // only for kBelowRangeHoldMs: the sample stays out of the filter, alarm and maximum,
  // then the state resets as for a sustained fault.
  if (r.status==Status::BelowRange && alarm_!=Status::Normal) {
    if (!below_hold_) {below_hold_=true;below_since_=ms;}
    if (uint32_t(ms-below_since_)<config::kBelowRangeHoldMs) {
      r.has_temperature=false;r.temperature_c=0;r.alarm=alarm_;
      return r;
    }
    reset();
  } else below_hold_=false;
  if (!filter_ready_) {filtered_resistance_=r.resistance_ohms;filter_ready_=true;}
  else filtered_resistance_ +=config::kFilterAlpha*(r.resistance_ohms-filtered_resistance_);
  r.has_temperature=false;r.temperature_c=0;
  const int side=calibrationSide(filtered_resistance_);
  float t=0;
  if (side<0) {
    r.status=Status::BelowRange;
    updateAlarm(Status::Normal,ms);
  } else if (side>0 || !temperatureFromResistance(filtered_resistance_,t)) {
    // Hotter than the verified table: the real temperature is unknown but at least the
    // top calibration point, so treat it as critical rather than as a sensor fault.
    r.status=Status::AboveRange;
    max_above_range_=true;
    updateAlarm(Status::Red,ms);
  } else {
    r.temperature_c=t;r.has_temperature=true;
    if (t>max_c_) max_c_=t;
    updateAlarm(targetFor(t),ms);
    if (alarm_==Status::Red || alarm_==Status::Amber) r.status=alarm_;
    else r.status=(t<config::kWarmThresholdC) ? Status::Cold : Status::Normal;
  }
  r.alarm=alarm_;
  return r;
}
} // namespace gauge
