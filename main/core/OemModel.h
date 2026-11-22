#pragma once
#include "DisplayText.h"
#include "SweepModel.h"
// OEM dial (#17): styled on the Caterham factory gauges. Pure logic, so the host tests cover it; the display only draws it.
namespace gauge {
// 50-130 degC in kOemSteps steps of 10 degC, a tick on each: the OEM face's 0-8 bar, one for one.
constexpr float kOemMinC = 50.f, kOemMaxC = 130.f;
constexpr int kOemSteps = 8;
// Numbers on every second tick after the start; the start has a tick but no number, like 0 bar on the OEM face.
constexpr int kOemNumbers = 4;
// The tick (0..kOemSteps) that number index sits on: 2, 4, 6, 8. index valid for 0 <= index < kOemNumbers.
int oemNumberStep(int index);
// 70, 90, 110, 130 degC, or 158, 194, 230, 266 degF.
int oemLabel(Unit unit, int index);
// The needle's place as a fraction 0..1 of the OEM span, with needleState()'s rules: hidden when uncalibrated or on a
// fault, at the start below the table, at the table maximum above it, and always clamped to the scale's ends. The same in
// either unit, since the scale's ends convert with the reading.
NeedleState oemNeedle(const Reading &r);
// CATERHAM shows only while allIsWell(): a calibrated build that isn't the demo, NORMAL or OIL WARMING with a finite
// temperature, and no alarm latched. Anything else puts the status lines in its place.
bool oemWordmark(const Reading &r, bool demo);
} // namespace gauge
