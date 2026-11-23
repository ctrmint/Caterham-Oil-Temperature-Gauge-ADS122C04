#pragma once
#include "DisplayText.h"
#include "SweepModel.h"
// Heritage dial (#25): a 1960s-style short-sweep dial. Pure logic, so the host tests cover it; the display only draws it.
namespace gauge {
// 50-130 degC over a 90-degree sweep with a tick every kHeritageTickC: kHeritageTicks ticks, index 0 at 50 degC.
constexpr float kHeritageMinC = 50.f, kHeritageMaxC = 130.f, kHeritageTickC = 5.f;
constexpr int kHeritageTicks = 17;
// Numbers at 50, 90 and 130 degC: five would crowd a 90-degree sweep, as on period short-sweep dials.
constexpr int kHeritageNumbers = 3;
// Long every 20 degC (50, 70, 90, 110, 130), medium at the other tens, short at the fives. 0 <= index < kHeritageTicks.
TickSize heritageTick(int index);
// The tick that number index sits on: 0, 8, 16. 0 <= index < kHeritageNumbers.
int heritageNumberTick(int index);
// 50, 90, 130 degC, or 122, 194, 266 degF.
int heritageLabel(Unit unit, int index);
// The needle's place as a fraction 0..1 of the 50-130 degC span, with needleState()'s rules: hidden when uncalibrated or
// on a fault, at the start below the table, at the table maximum above it, and clamped to the scale's ends.
NeedleState heritageNeedle(const Reading &r);
// The legend (OIL TEMP and the unit) shows while all is well (allIsWell()); otherwise the status lines take its place, as
// on OEM, so never in the demo build or the uncalibrated build.
bool heritageLegend(const Reading &r, bool demo);
} // namespace gauge
