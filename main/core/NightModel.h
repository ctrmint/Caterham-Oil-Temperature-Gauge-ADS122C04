#pragma once
#include "SweepModel.h"
#include <stdint.h>
// Night (#21): a dark layout, quiet while all is well and fully lit otherwise. Pure logic, so the host tests cover it; the
// display only draws it.
namespace gauge {
// Quiet exactly when all is well (allIsWell()): just a small, dim reading and the heartbeat. Anything else lights the
// screen, so never in the demo build or the uncalibrated build. The same rule as OEM's wordmark and White's window.
bool nightQuiet(const Reading &r, bool demo);
// The quiet state's heartbeat: a dim dot whose brightness steps once per display update, rising for half of
// kNightBeatFrames updates and falling for the other half, so it stops if the updates stop. Two consecutive updates never
// give the same brightness, across the frame counter's wrap too.
constexpr uint32_t kNightBeatFrames = 8;                          // 2 s at the 250 ms display interval
constexpr uint8_t kNightBeatMinOpa = 26, kNightBeatMaxOpa = 102;  // about 10 % and 40 % (of 255)
uint8_t nightBeat(uint32_t frame);
} // namespace gauge
