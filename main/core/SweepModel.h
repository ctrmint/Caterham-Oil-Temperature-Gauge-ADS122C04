#pragma once
#include "DisplayText.h"
#include <stdint.h>
namespace gauge {
constexpr int kSweepSegments = 40;
struct SweepScale { float min; float max; };
// Scale ends in the selected unit: 30-130 degC, or the same span in degF (86-266).
SweepScale sweepScale(Unit unit);
// Scale numbers at a fixed step from the scale minimum (20 degC / 36 degF); index valid for 0 <= index < count.
int scaleLabelCount(Unit unit);
int scaleLabel(Unit unit, int index);
// Scale numbers are shown only with a verified calibration.
bool scaleLabelsVisible();
// Lit segments 0..kSweepSegments. Zero when uncalibrated, on a fault or below range; AboveRange lights up to the table maximum.
int litSegments(const Reading &r, Unit unit);
// Seven-segment pattern, bits 0-6 are segments a-g. Characters without a pattern give 0.
uint8_t sevenSegmentMask(char c);
// Sweep shows three seven-segment cells, with '<' or '>' drawn as a separate sign to their left.
constexpr size_t kSweepCells = 3;
// Splits a readingText() string: a leading '<' or '>' goes to sign (otherwise sign is 0), and the rest is right-aligned in
// kSweepCells characters. Anything that does not fit (a 4-digit resistance, an empty or missing text) becomes " --" with
// no sign. size >= kSweepCells + 1.
void sweepReading(const char *text, char &sign, char *out, size_t size);

// Graduated colours, 0xRRGGBB. The bands are dark blue below 50 degC, light blue 50-69, green 70-95, yellow 96-105,
// amber 106-119 and red from 120. Each colour is pure at its stop and blends linearly to the next stop, so a band
// boundary is a half-way mix.
constexpr uint32_t kColourDarkBlue = 0x2F5BEA;
constexpr uint32_t kColourLightBlue = 0x4FC3F7;
constexpr uint32_t kColourGreen = 0x2ED158;
constexpr uint32_t kColourYellow = 0xF5E13B;
constexpr uint32_t kColourAmber = 0xFF9A1F;
constexpr uint32_t kColourRed = 0xF0383B;
// Stops in degC: dark blue at or below 40, light blue 60, green 75-90, yellow 100, amber 112, red at or above 120.
constexpr float kColourAmberStopC = 112.f;
constexpr float kColourRedStopC = 120.f;
// Colour at a temperature in degC. -inf gives dark blue, +inf red, NaN amber.
uint32_t gradientColour(float celsius);
// Colour of the reading digits, the status text and the Classic arc. It is the gradient at the reading's temperature
// (the table limit beyond the table), but never less severe than the latched alarm: an Amber alarm gives at least
// amber, a Red alarm gives red. A fault, an uncalibrated build or a missing temperature gives amber (red with a Red alarm).
uint32_t readingColour(const Reading &r);
// Colour of lit ring segment index (0..kSweepSegments-1): the gradient at the segment's centre on the scale.
uint32_t segmentColour(int index, Unit unit);
} // namespace gauge
