#pragma once
#include "Temperature.h"
#include <stdint.h>
// Time-based trace (#12): one sample every kTraceSampleMs, the last 60 s, on a kTraceSpanC window that follows the latest
// point. Pure logic, so the host tests cover it; the display only draws it.
namespace gauge {
constexpr uint32_t kTraceSampleMs = 1000;
constexpr int kTracePoints = 61;      // now and 60 one-second intervals back: 60 s
constexpr float kTraceSpanC = 30.f;   // the window's height
constexpr float kTraceStepC = 5.f;    // the window's ends sit on multiples of this
constexpr float kTraceMarginC = 3.f;  // re-centre when the latest point comes this close to an edge
struct TracePoint {
  bool present = false;      // false: a gap (fault, below the table, uncalibrated). Nothing is carried over.
  bool above_range = false;  // beyond the table: shown at the table maximum
  float celsius = 0;
  uint32_t colour = 0;       // readingColour() of the reading it came from
};
struct TraceWindow { float lo_c; float hi_c; };
constexpr TraceWindow kTraceDefaultWindow = {30.f, 30.f + kTraceSpanC};  // until the first point
// The point a reading gives: its temperature, the table maximum above the table, otherwise a gap.
TracePoint tracePoint(const Reading &r);
// The window after a new point at latest_c: unchanged while the point is at least kTraceMarginC inside it, otherwise
// kTraceSpanC centred on the point, ends rounded to kTraceStepC. have = false: no window yet. A non-finite point changes
// nothing.
TraceWindow traceWindow(const TraceWindow &current, bool have, float latest_c);
// Whether a value (a threshold line) falls inside the window, ends included.
bool traceShows(const TraceWindow &w, float celsius);
class TraceHistory {
 public:
  // Call with every new reading. The first call takes a sample, then one every kTraceSampleMs (wrap-safe). After a stall
  // of two periods or more the schedule restarts from now, with no catch-up samples. True when a sample was taken.
  bool update(const Reading &r, uint32_t ms);
  int count() const { return count_; }
  // i = 0 is the oldest, count() - 1 the newest. Out of range gives a gap.
  const TracePoint &point(int i) const;
  TraceWindow window() const { return window_; }
  // Changes every time a sample is taken.
  uint32_t version() const { return version_; }
 private:
  TracePoint points_[kTracePoints];
  int head_ = 0;  // where the next sample goes
  int count_ = 0;
  bool started_ = false;
  uint32_t last_ms_ = 0;
  bool have_window_ = false;
  TraceWindow window_ = kTraceDefaultWindow;
  uint32_t version_ = 0;
};
} // namespace gauge
