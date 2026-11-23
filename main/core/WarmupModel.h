#pragma once
#include "DisplayText.h"
#include "SweepModel.h"
#include <stddef.h>
#include <stdint.h>
// Warm-up (#27): progress to operating temperature and how long it took. Pure logic, so the host tests cover it; the
// display only draws it.
namespace gauge {
// The ring's fill as a fraction: 0 at 0 degC, 1 at the warm threshold (kWarmThresholdC), with needleState()'s rules:
// hidden when uncalibrated or on a fault (an empty ring would look like cold oil), empty below the table, full above it,
// and clamped to the ends.
NeedleState warmupRing(const Reading &r);
// The warm-up clock, counted from the first sample whichever layout is showing. Warm is the first reading at or above the
// warm threshold (NORMAL, HIGH TEMPERATURE, CRITICAL TEMP with a finite temperature, or ABOVE CAL RANGE) with a verified
// calibration; the time to it is recorded once and kept for the power cycle, even if the oil cools again. It was warm at
// start if no cold reading (OIL WARMING with a finite temperature, or BELOW CAL RANGE) came before. Faults never count
// either way and don't stop the clock. The elapsed time adds each gap (unsigned millisecond subtraction) and saturates.
class WarmupClock {
 public:
  void update(const Reading &r, uint32_t now_ms);
  uint32_t elapsedMs() const { return elapsed_ms_; }
  bool warm() const { return warm_; }
  bool warmAtStart() const { return warm_ && !seen_cold_; }
  uint32_t warmMs() const { return warm_ms_; }
 private:
  bool started_ = false;
  uint32_t last_ms_ = 0;
  uint32_t elapsed_ms_ = 0;
  bool seen_cold_ = false;
  bool warm_ = false;
  uint32_t warm_ms_ = 0;
};
// "2:35" (the time since power-up) while warming, "WARM IN 6:12" once warm, "WARM AT START", or "" when uncalibrated.
constexpr size_t kWarmupTextSize = 24;
void warmupText(const WarmupClock &c, char *out, size_t size);
} // namespace gauge
