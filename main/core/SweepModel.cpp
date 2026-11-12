#include "SweepModel.h"
#include "Calibration.h"
#include <cmath>
namespace gauge {
namespace {
constexpr float kScaleMinC = 40.f, kScaleMaxC = 150.f;
constexpr float kScaleMinF = 100.f, kScaleMaxF = 300.f;
constexpr int kLabelStep = 20;
} // namespace
SweepScale sweepScale(Unit unit) {
  return unit == Unit::Fahrenheit ? SweepScale{kScaleMinF, kScaleMaxF} : SweepScale{kScaleMinC, kScaleMaxC};
}
int scaleLabelCount(Unit unit) {
  const SweepScale s = sweepScale(unit);
  return static_cast<int>((s.max - s.min) / kLabelStep) + 1;
}
int scaleLabel(Unit unit, int index) {
  return static_cast<int>(sweepScale(unit).min) + kLabelStep * index;
}
bool scaleLabelsVisible() { return calibrationValid(); }
int litSegments(const Reading &r, Unit unit) {
  if (!calibrationValid() || isFault(r.status) || r.status == Status::BelowRange) return 0;
  float t_c;
  if (r.has_temperature) t_c = r.temperature_c;
  else if (r.status == Status::AboveRange) t_c = calibrationMaxC();
  else return 0;
  if (!std::isfinite(t_c)) return 0;
  const float t = toUnit(t_c, unit);
  if (!std::isfinite(t)) return 0;
  const SweepScale s = sweepScale(unit);
  const float n = std::floor((t - s.min) * kSweepSegments / (s.max - s.min));
  if (!(n > 0.f)) return 0;
  if (n >= static_cast<float>(kSweepSegments)) return kSweepSegments;
  return static_cast<int>(n);
}
uint8_t sevenSegmentMask(char c) {
  static const uint8_t kDigits[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  if (c >= '0' && c <= '9') return kDigits[c - '0'];
  if (c == '-') return 0x40;
  return 0x00;
}
} // namespace gauge
