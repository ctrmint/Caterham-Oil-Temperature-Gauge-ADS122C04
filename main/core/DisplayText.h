#pragma once
#include "Temperature.h"
#include <stddef.h>
#include <stdint.h>
namespace gauge {
enum class Unit : uint8_t { Celsius, Fahrenheit };
constexpr size_t kReadingCells = 4;
constexpr size_t kReadingTextSize = 12;
constexpr size_t kFooterTextSize = 48;
// degC -> display unit: F = C * 9/5 + 32.
float toUnit(float celsius, Unit unit);
// Nearest whole number, halves away from zero. False when not finite or |value| >= 1e6.
bool roundHalfAway(float value, long &out);
// Static strings. Uncalibrated firmware has no degrees: the label is "ohm".
const char* unitLabel(Unit unit);
const char* titleText(Unit unit);
// Unitless reading: whole degrees, "<"/">" beyond the table, whole ohms when uncalibrated, or "--".
void readingText(const Reading &r, Unit unit, char *out, size_t size);
// Right-aligns text in kReadingCells characters; longer text becomes "  --". size >= kReadingCells + 1.
void readingCells(const char *text, char *out, size_t size);
// Footer: MAX and ohms, or the fault / calibration fallback text.
void footerText(const Reading &r, float max_c, bool max_above_range, Unit unit, char *out, size_t size);
} // namespace gauge
