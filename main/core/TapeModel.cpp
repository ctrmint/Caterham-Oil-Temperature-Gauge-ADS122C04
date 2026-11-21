#include "TapeModel.h"
#include "Calibration.h"
#include <cmath>
#include <cstdio>
namespace gauge {
TapeState tapeState(const Reading &r) {
  if (!calibrationValid() || isFault(r.status)) return {false, 0.f};
  if (r.status == Status::BelowRange) return {true, calibrationMinC()};
  if (r.has_temperature && std::isfinite(r.temperature_c)) return {true, r.temperature_c};
  if (r.status == Status::AboveRange) return {true, calibrationMaxC()};
  return {false, 0.f};
}
float tapeOffsetPx(float value_c, float centre_c, float px_per_c) { return (centre_c - value_c) * px_per_c; }
Rate traceRate(const TraceHistory &h) {
  const Rate none = {false, 0.f};
  if (h.count() < kRateSamples) return none;
  const int first = h.count() - kRateSamples;
  float sum = 0.f;
  for (int i = 0; i < kRateSamples; ++i) {
    const TracePoint &p = h.point(first + i);
    if (!p.present || p.above_range || !std::isfinite(p.celsius)) return none;
    sum += p.celsius;
  }
  // Slope against the sample index, centred so the sums stay small: sum((x - xm)(y - ym)) / sum((x - xm)^2).
  const float xm = (kRateSamples - 1) / 2.f, ym = sum / kRateSamples;
  float num = 0.f, den = 0.f;
  for (int i = 0; i < kRateSamples; ++i) {
    const float dx = static_cast<float>(i) - xm;
    num += dx * (h.point(first + i).celsius - ym);
    den += dx * dx;
  }
  const float per_min = num / den * (60000.f / static_cast<float>(kTraceSampleMs));
  if (!std::isfinite(per_min)) return none;
  return {true, per_min};
}
RateTrend rateTrend(float c_per_min) {
  if (!(std::fabs(c_per_min) >= kRateSteadyCPerMin)) return RateTrend::Steady;
  return c_per_min > 0.f ? RateTrend::Rising : RateTrend::Falling;
}
void rateText(const Rate &rate, Unit unit, char *out, size_t size) {
  if (!out || size == 0) return;
  out[0] = '\0';
  if (!rate.available || !std::isfinite(rate.c_per_min)) return;
  if (rateTrend(rate.c_per_min) == RateTrend::Steady) { std::snprintf(out, size, "STEADY"); return; }
  const bool f = unit == Unit::Fahrenheit;
  const float v = std::fabs(rate.c_per_min) * (f ? 1.8f : 1.f);  // a rate converts without the 32 degF offset
  std::snprintf(out, size, "%.1f %s/min", static_cast<double>(v), f ? "\xC2\xB0""F" : "\xC2\xB0""C");
}
} // namespace gauge
