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

// Sweep's LED at 6 o'clock (#10). Lit in readingColour() and pulsing while an alarm is latched: slowly at Amber, faster at
// Red. Off without a temperature (a fault, or an uncalibrated build), except that an alarm kept through a brief fault
// keeps it pulsing, so it never looks calmer than the digits.
enum class LedPulse : uint8_t { None, Slow, Fast };
struct LedState { bool lit; uint32_t colour; LedPulse pulse; };
LedState ledState(const Reading &r);
// One bright-dim-bright cycle; the brightness never drops below kLedMinOpa (of 255).
constexpr uint32_t kLedSlowPeriodMs = 1000, kLedFastPeriodMs = 500;
constexpr uint8_t kLedMinOpa = 77;  // about 30 %
// LED opacity at a millisecond tick: 255 when not pulsing, else a cosine from 255 (at each whole period) down to
// kLedMinOpa (half-way). The phase jumps once at the 49.7-day tick wrap; that's cosmetic.
uint8_t ledOpacity(LedPulse pulse, uint32_t ms);

// Needle dial (#11): the same span as Sweep (sweepScale), with five numbers.
constexpr int kNeedleLabels = 5;
// 30, 55, 80, 105, 130 degC, or 86, 131, 176, 221, 266 degF; index valid for 0 <= index < kNeedleLabels.
int needleLabel(Unit unit, int index);
// The needle's place as a fraction 0..1 along the scale. It follows litSegments(): hidden when uncalibrated or on a fault,
// at the start below the table, at the table maximum above it, and always clamped to the scale's ends.
struct NeedleState { bool visible; float fraction; };
NeedleState needleState(const Reading &r, Unit unit);
// The same rules on any scale from min_u to max_u in the given unit (max_u > min_u); the OEM dial (#17) uses its own span.
NeedleState needleOnScale(const Reading &r, Unit unit, float min_u, float max_u);
} // namespace gauge
