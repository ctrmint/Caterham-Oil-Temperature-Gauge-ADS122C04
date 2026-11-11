#pragma once
#include "DisplayText.h"
#include <stdint.h>
namespace gauge {
constexpr int kSweepSegments = 40;
struct SweepScale { float min; float max; };
// Scale ends in the selected unit.
SweepScale sweepScale(Unit unit);
// Scale numbers at a fixed step from the scale minimum; index valid for 0 <= index < count.
int scaleLabelCount(Unit unit);
int scaleLabel(Unit unit, int index);
// Scale numbers are shown only with a verified calibration.
bool scaleLabelsVisible();
// Lit segments 0..kSweepSegments. Zero when uncalibrated, on a fault or below range; AboveRange lights up to the table maximum.
int litSegments(const Reading &r, Unit unit);
// Seven-segment pattern, bits 0-6 are segments a-g. Characters without a pattern give 0.
uint8_t sevenSegmentMask(char c);
} // namespace gauge
