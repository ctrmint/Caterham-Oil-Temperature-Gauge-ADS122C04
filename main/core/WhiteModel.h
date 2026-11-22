#pragma once
#include "DisplayText.h"
#include "SweepModel.h"
#include <stdint.h>
// White dial (#19): a white face styled on an aftermarket oil-temperature gauge. Pure logic, so the host tests cover it;
// the display only draws it.
namespace gauge {
// 40-140 degC with a tick every kWhiteTickC: kWhiteTicks ticks, index 0 at 40 degC and the last at 140.
constexpr float kWhiteMinC = 40.f, kWhiteMaxC = 140.f, kWhiteTickC = 2.f;
constexpr int kWhiteTicks = 51;
// Numbers every 20 degC, on every tenth tick: 40, 60 ... 140.
constexpr int kWhiteNumbers = 6;
enum class TickSize : uint8_t { Minor, Medium, Major };
// Long at the numbers, medium at the other tens (50, 70 ... 130), short at the rest. index valid for 0 <= index < kWhiteTicks.
TickSize whiteTick(int index);
// The tick that number index sits on: 0, 10, 20, 30, 40, 50. index valid for 0 <= index < kWhiteNumbers.
int whiteNumberTick(int index);
// 40, 60, 80, 100, 120, 140 degC, or 104, 140, 176, 212, 248, 284 degF.
int whiteLabel(Unit unit, int index);
// The needle's place as a fraction 0..1 of the 40-140 degC span, with needleState()'s rules: hidden when uncalibrated or on
// a fault, at the start below the table, at the table maximum above it, and always clamped to the scale's ends.
NeedleState whiteNeedle(const Reading &r);
// The status window shows whenever all is not well (allIsWell()), so always in the demo build and the uncalibrated build.
bool whiteWindow(const Reading &r, bool demo);
} // namespace gauge
