#pragma once
#include <stdint.h>

namespace config {
// Measured value of the reference resistor Rref (nominal 2.00 kOhm, 0.1 %, <=25 ppm/C);
// replace with your fitted resistor's reading. Rs = Rref * Vs / Vref, so this resistor is
// the measurement reference.
constexpr float kReferenceOhms = 2000.0f;
// AVDD of the ADS122C04 (the display's 3.3 V), read through the chip's own supply monitor.
constexpr float kValidSupplyMinV = 2.9f;
constexpr float kValidSupplyMaxV = 3.5f;
// Excitation current: the driver sets the ADC's current source to 250 uA (+/-6 %). The
// measured current, Vref / Rref, must stay inside the window; outside it the source or Rref
// is wrong, so no reading is trusted (SUPPLY FAULT).
constexpr float kExcitationA = 0.000250f;
constexpr float kExcitationMinA = 0.000210f;
constexpr float kExcitationMaxA = 0.000290f;
// Shorted-input reading, subtracted from Vref and Vs. Beyond this the ADC itself is suspect.
constexpr float kAdcOffsetMaxV = 0.00025f;
// The ground reading is A2 with the ADC's 10 uA burn-out source on; this is that current's
// drop across A2's 2.2 kOhm series resistor. A broken ground-sense wire is pulled towards
// AVDD by the source and lands far outside the window below.
constexpr float kGroundSenseBiasV = 0.022f;
// Tank ground relative to gauge GND.
constexpr float kGroundOffsetMinV = -0.10f;
constexpr float kGroundOffsetMaxV = 0.20f;
// Sender tap check: A1 read with the burn-out source on, minus the ground reading and Vs.
// An intact tap gives about 10 uA x Rs (under 0.06 V up to the open threshold); an open A1
// tap is pulled towards AVDD, which would otherwise give a plausible but wrong resistance.
constexpr float kSenderTapMaxV = 0.25f;
constexpr float kShortResistanceOhms = 20.0f;
// Well above the ~1158 ohm sender at 40 C; an open sender stops the excitation current,
// so Vref ~ 0 and the resistance is huge.
constexpr float kOpenResistanceOhms = 5000.0f;
constexpr uint32_t kSampleIntervalMs = 250;
constexpr uint32_t kDisplayIntervalMs = 250;
constexpr uint32_t kLogIntervalMs = 1000;
// The web page's log (#32): one row per log interval, the last 8 hours, held in PSRAM.
constexpr uint32_t kWebLogRows = 8u * 60u * 60u * 1000u / kLogIntervalMs;
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
// While an alarm is latched, raw samples colder than the table are held for this long,
// then the state resets as for a sustained fault.
constexpr uint32_t kBelowRangeHoldMs = 10000;
}  // namespace config
