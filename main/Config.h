#pragma once
#include <stdint.h>

namespace config {
// Measured pull-up resistance in ohms; replace with your actual fitted resistor reading.
// With the ratiometric ADS1115 circuit this resistor is the measurement reference.
constexpr float kPullUpOhms = 470.0f;
// 3.3V excitation rail, read single-ended on ADS1115 A0.
constexpr float kValidSupplyMinV = 2.9f;
constexpr float kValidSupplyMaxV = 3.5f;
// Tank ground relative to gauge GND, derived as supply - (Vp + Vs). A broken ground-sense
// wire is pulled to 3V3 by its 100k bias resistor and lands far outside this window.
constexpr float kGroundOffsetMinV = -0.10f;
constexpr float kGroundOffsetMaxV = 0.20f;
constexpr float kShortResistanceOhms = 20.0f;
// Well above the ~1158 ohm cold sender; an open sender gives Vp ~ 0 and a huge resistance.
constexpr float kOpenResistanceOhms = 5000.0f;
constexpr uint32_t kSampleIntervalMs = 250;
constexpr uint32_t kDisplayIntervalMs = 250;
constexpr uint32_t kLogIntervalMs = 1000;
constexpr float kFilterAlpha = 0.2f;
// Provisional operator-configurable UI settings, NOT Caterham limits.
constexpr float kWarmThresholdC = 60.0f;
constexpr float kAmberThresholdC = 115.0f;
constexpr float kRedThresholdC = 130.0f;
constexpr float kAlarmResetHysteresisC = 5.0f;
constexpr uint32_t kAlarmDebounceMs = 2500;
// A fault shorter than this keeps the filter and alarm state, so an intermittent
// connector cannot keep restarting the alarm debounce.
constexpr uint32_t kFaultResetMs = 3000;
}  // namespace config
