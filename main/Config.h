#pragma once
#include <stdint.h>

namespace config {
// Measured pull-up resistance in ohms; replace with your actual fitted resistor reading.
constexpr float kPullUpOhms = 470.0f;
// 3.3V sense on GPIO17 via two separate precision resistors.
constexpr float kReferenceUpperOhms = 10000.0f;
constexpr float kReferenceLowerOhms = 10000.0f;
constexpr float kValidSupplyMinV = 2.9f;
constexpr float kValidSupplyMaxV = 3.5f;
// GPIO16 input approaching supply is considered sender open. The ESP32-S3 ADC is
// unreliable at its upper range; deliberately flag before ADC saturation.
constexpr float kOpenVoltageRatio = 0.87f;
constexpr float kShortResistanceOhms = 20.0f;
constexpr float kOpenResistanceOhms = 20000.0f;
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
}  // namespace config
