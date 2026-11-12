#include "SweepModel.h"
#include "Calibration.h"
#include <cmath>
#include <cstdio>
#include <cstring>
namespace gauge {
namespace {
constexpr float kScaleMinC = 30.f, kScaleMaxC = 130.f;
constexpr float kScaleMinF = 86.f, kScaleMaxF = 266.f;  // the degC scale converted
constexpr int kLabelStepC = 20, kLabelStepF = 36;        // 20 degC = 36 degF, so labels sit at the same angles
struct Stop { float c; uint32_t rgb; };
constexpr Stop kStops[] = {
  {40.f, kColourDarkBlue}, {60.f, kColourLightBlue}, {75.f, kColourGreen}, {90.f, kColourGreen},
  {100.f, kColourYellow}, {kColourAmberStopC, kColourAmber}, {kColourRedStopC, kColourRed}};
constexpr int kStopCount = static_cast<int>(sizeof(kStops) / sizeof(kStops[0]));
uint32_t mix(uint32_t a, uint32_t b, float f) {
  uint32_t out = 0;
  for (int shift = 16; shift >= 0; shift -= 8) {
    const float ca = static_cast<float>((a >> shift) & 0xFF), cb = static_cast<float>((b >> shift) & 0xFF);
    out |= static_cast<uint32_t>(std::lround(ca + (cb - ca) * f)) << shift;
  }
  return out;
}
} // namespace
SweepScale sweepScale(Unit unit) {
  return unit == Unit::Fahrenheit ? SweepScale{kScaleMinF, kScaleMaxF} : SweepScale{kScaleMinC, kScaleMaxC};
}
int scaleLabelCount(Unit unit) {
  const SweepScale s = sweepScale(unit);
  const int step = unit == Unit::Fahrenheit ? kLabelStepF : kLabelStepC;
  return static_cast<int>((s.max - s.min) / static_cast<float>(step)) + 1;
}
int scaleLabel(Unit unit, int index) {
  const int step = unit == Unit::Fahrenheit ? kLabelStepF : kLabelStepC;
  return static_cast<int>(sweepScale(unit).min) + step * index;
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
void sweepReading(const char *text, char &sign, char *out, size_t size) {
  sign = 0;
  if (!out || size == 0) return;
  const char *rest = text ? text : "";
  const char s = (*rest == '<' || *rest == '>') ? *rest : 0;
  if (s) ++rest;
  const size_t len = std::strlen(rest);
  if (len == 0 || len > kSweepCells) {
    std::snprintf(out, size, "%*s", static_cast<int>(kSweepCells), "--");
    return;
  }
  std::snprintf(out, size, "%*s", static_cast<int>(kSweepCells), rest);
  sign = s;
}
uint32_t gradientColour(float celsius) {
  if (std::isnan(celsius)) return kColourAmber;
  if (celsius <= kStops[0].c) return kStops[0].rgb;
  for (int i = 1; i < kStopCount; ++i) {
    if (celsius < kStops[i].c) {
      const Stop &lo = kStops[i - 1], &hi = kStops[i];
      return mix(lo.rgb, hi.rgb, (celsius - lo.c) / (hi.c - lo.c));
    }
  }
  return kStops[kStopCount - 1].rgb;
}
uint32_t readingColour(const Reading &r) {
  if (r.alarm == Status::Red) return kColourRed;
  if (!calibrationValid() || isFault(r.status)) return kColourAmber;
  float t_c;
  if (r.has_temperature && std::isfinite(r.temperature_c)) t_c = r.temperature_c;
  else if (r.status == Status::AboveRange) t_c = calibrationMaxC();
  else if (r.status == Status::BelowRange) t_c = calibrationMinC();
  else return kColourAmber;
  if (r.alarm == Status::Amber && t_c < kColourAmberStopC) t_c = kColourAmberStopC;
  return gradientColour(t_c);
}
uint32_t segmentColour(int index, Unit unit) {
  const SweepScale s = sweepScale(unit);
  const float t = s.min + (static_cast<float>(index) + 0.5f) * (s.max - s.min) / kSweepSegments;
  return gradientColour(unit == Unit::Fahrenheit ? (t - 32.f) * 5.f / 9.f : t);
}
} // namespace gauge
