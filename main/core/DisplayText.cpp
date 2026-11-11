#include "DisplayText.h"
#include "Calibration.h"
#include <cmath>
#include <cstdio>
#include <cstring>
namespace gauge {
namespace {
constexpr float kRoundLimit = 1e6f;
constexpr float kNoMaxYetC = -200.f;  // max_c at or below this means no maximum recorded yet
// Writes the rounded value, or "--" when it cannot be shown.
void writeRounded(float value, const char *prefix, char *out, size_t size) {
  long n;
  if (!roundHalfAway(value, n)) std::snprintf(out, size, "--");
  else std::snprintf(out, size, "%s%ld", prefix, n);
}
bool ohmsShowable(const Reading &r) { return std::isfinite(r.resistance_ohms) && r.resistance_ohms > 0.0f; }
} // namespace
float toUnit(float celsius, Unit unit) {
  return unit == Unit::Fahrenheit ? celsius * 9.0f / 5.0f + 32.0f : celsius;
}
bool roundHalfAway(float value, long &out) {
  if (!std::isfinite(value) || std::fabs(value) >= kRoundLimit) return false;
  out = std::lround(value);
  return true;
}
const char* unitLabel(Unit unit) {
  if (!calibrationValid()) return "ohm";
  return unit == Unit::Fahrenheit ? "\xC2\xB0""F" : "\xC2\xB0""C";
}
const char* titleText(Unit unit) {
  if (!calibrationValid()) return "OIL ohm";
  return unit == Unit::Fahrenheit ? "OIL \xC2\xB0""F" : "OIL \xC2\xB0""C";
}
void readingText(const Reading &r, Unit unit, char *out, size_t size) {
  if (!calibrationValid()) {
    if (r.status == Status::NeedsCalibration && ohmsShowable(r)) writeRounded(r.resistance_ohms, "", out, size);
    else std::snprintf(out, size, "--");
  } else if (r.has_temperature) writeRounded(toUnit(r.temperature_c, unit), "", out, size);
  else if (r.status == Status::AboveRange) writeRounded(toUnit(calibrationMaxC(), unit), ">", out, size);
  else if (r.status == Status::BelowRange) writeRounded(toUnit(calibrationMinC(), unit), "<", out, size);
  else if (r.status == Status::NeedsCalibration && ohmsShowable(r)) writeRounded(r.resistance_ohms, "", out, size);
  else std::snprintf(out, size, "--");
}
void readingCells(const char *text, char *out, size_t size) {
  const size_t len = std::strlen(text);
  if (len > kReadingCells) std::snprintf(out, size, "%*s", static_cast<int>(kReadingCells), "--");
  else std::snprintf(out, size, "%*s", static_cast<int>(kReadingCells), text);
}
void footerText(const Reading &r, float max_c, bool max_above_range, Unit unit, char *out, size_t size) {
  const bool beyond_table = r.status == Status::AboveRange || r.status == Status::BelowRange;
  if (calibrationValid() && (r.has_temperature || beyond_table)) {
    char ohms[kReadingTextSize];
    writeRounded(r.resistance_ohms, "", ohms, sizeof(ohms));
    char max[kReadingTextSize];
    if (max_above_range) writeRounded(toUnit(calibrationMaxC(), unit), ">", max, sizeof(max));
    else if (max_c > kNoMaxYetC) writeRounded(toUnit(max_c, unit), "", max, sizeof(max));
    else max[0] = '\0';
    if (max[0]) std::snprintf(out, size, "MAX %s  |  %s ohm", max, ohms);
    else std::snprintf(out, size, "%s ohm", ohms);
  } else if (r.status == Status::NeedsCalibration) {
    std::snprintf(out, size, "TEMPERATURE NOT VERIFIED");
  } else {
    std::snprintf(out, size, "CHECK SENSOR AND WIRING");
  }
}
} // namespace gauge
