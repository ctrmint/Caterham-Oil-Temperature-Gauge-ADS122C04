#include "Calibration.h"
#include <cmath>

namespace gauge {
namespace {
#if defined(CONFIG_OIL_GAUGE_DEMO) || defined(GAUGE_DEMO)
// Synthetic data used ONLY in explicitly named demo build. Not the Caterham curve.
static constexpr CalibrationPoint kDemoPoints[] = {
  {20.0f, 1100.f}, {40.0f, 650.f}, {60.0f, 350.f},
  {80.0f, 200.f}, {100.0f, 120.f}, {120.0f, 75.f}, {140.0f, 47.f}
};
static constexpr const CalibrationPoint* kPoints = kDemoPoints;
static constexpr size_t kCount = sizeof(kDemoPoints) / sizeof(kDemoPoints[0]);
static constexpr bool kReady = true;
#else
static constexpr const CalibrationPoint* kPoints = kCalibrationPoints;
static constexpr size_t kCount = sizeof(kCalibrationPoints) / sizeof(kPoints[0]);
static constexpr bool kReady = kCalibrationVerified;
#endif
}
bool calibrationValid() {
  if (!kReady || kCount < 2) return false;
  for (size_t i = 0; i < kCount; ++i) {
    if (!std::isfinite(kPoints[i].temperature_c) ||
        !std::isfinite(kPoints[i].resistance_ohms) ||
        kPoints[i].resistance_ohms <= 0) return false;
    if (i && (kPoints[i].temperature_c <= kPoints[i-1].temperature_c ||
              kPoints[i].resistance_ohms >= kPoints[i-1].resistance_ohms)) return false;
  }
  return true;
}
bool temperatureFromResistance(float r, float &out) {
  if (!calibrationValid() || !std::isfinite(r) || r <= 0) return false;
  // No extrapolation beyond measured range.
  if (r > kPoints[0].resistance_ohms || r < kPoints[kCount-1].resistance_ohms) return false;
  for (size_t i=1;i<kCount;i++) {
    auto high=kPoints[i-1], low=kPoints[i];
    if (r<=high.resistance_ohms && r>=low.resistance_ohms) {
      float u=(high.resistance_ohms-r)/(high.resistance_ohms-low.resistance_ohms);
      out=high.temperature_c + u*(low.temperature_c-high.temperature_c);
      return true;
    }
  }
  return false;
}
float resistanceFromTemperature(float t) {
  if (!calibrationValid() || t < kPoints[0].temperature_c ||
      t > kPoints[kCount-1].temperature_c) return NAN;
  for (size_t i=1; i<kCount;i++) {
    auto a=kPoints[i-1],b=kPoints[i];
    if (t >= a.temperature_c && t <= b.temperature_c) {
      float u=(t-a.temperature_c)/(b.temperature_c-a.temperature_c);
      return a.resistance_ohms + u*(b.resistance_ohms-a.resistance_ohms);
    }
  }
  return NAN;
}
} // namespace gauge
