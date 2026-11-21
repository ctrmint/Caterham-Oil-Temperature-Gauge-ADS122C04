#pragma once
#include "DisplayText.h"
#include "TraceModel.h"
#include <stddef.h>
#include <stdint.h>
// Tape layout (#15): a scale that scrolls past a fixed pointer, and the rate of change from the trace history. Pure
// logic, so the host tests cover it; the display only draws it.
namespace gauge {
constexpr float kTapeSpanC = 40.f;          // visible: 20 degC either side of the reading
constexpr int kRateSamples = 30;            // the rate's window: the newest 30 trace points (30 s at one a second)
constexpr float kRateSteadyCPerMin = 0.5f;  // slower than this either way is STEADY
constexpr size_t kRateTextSize = 24;
// Where the tape is centred, in degC: the reading; the table maximum above the table; the table minimum below it.
// Hidden on a fault or when uncalibrated.
struct TapeState { bool visible; float centre_c; };
TapeState tapeState(const Reading &r);
// Pixels from the tape's centre line, positive downwards, for value_c on a tape centred on centre_c with px_per_c
// pixels per degC: hotter is higher.
float tapeOffsetPx(float value_c, float centre_c, float px_per_c);
// Least-squares slope of the newest kRateSamples trace points, in degC per minute. Available only when that many points
// exist and every one is present and inside the table (not above_range): a gap or a fault never gives a rate.
struct Rate { bool available; float c_per_min; };
Rate traceRate(const TraceHistory &h);
enum class RateTrend : uint8_t { Steady, Rising, Falling };
// Steady when |c_per_min| < kRateSteadyCPerMin (and for NaN).
RateTrend rateTrend(float c_per_min);
// The rate's magnitude ("2.4 °C/min"; the trend gives the direction), "STEADY", or "" when unavailable. degF: degF/min.
void rateText(const Rate &rate, Unit unit, char *out, size_t size);
} // namespace gauge
