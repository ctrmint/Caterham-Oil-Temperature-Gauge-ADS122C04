#include "TraceModel.h"
#include "Calibration.h"
#include "SweepModel.h"
#include <cmath>
namespace gauge {
TracePoint tracePoint(const Reading &r) {
  TracePoint p;
  if (!calibrationValid() || isFault(r.status) || r.status == Status::BelowRange) return p;
  if (r.has_temperature && std::isfinite(r.temperature_c)) {
    p.celsius = r.temperature_c;
  } else if (r.status == Status::AboveRange) {
    p.celsius = calibrationMaxC();
    p.above_range = true;
  } else {
    return p;
  }
  p.present = true;
  p.colour = readingColour(r);
  return p;
}
TraceWindow traceWindow(const TraceWindow &current, bool have, float latest_c) {
  if (!std::isfinite(latest_c)) return current;
  if (have && latest_c >= current.lo_c + kTraceMarginC && latest_c <= current.hi_c - kTraceMarginC) return current;
  const float lo = std::round((latest_c - kTraceSpanC / 2) / kTraceStepC) * kTraceStepC;
  return {lo, lo + kTraceSpanC};
}
bool traceShows(const TraceWindow &w, float celsius) { return celsius >= w.lo_c && celsius <= w.hi_c; }
bool TraceHistory::update(const Reading &r, uint32_t ms) {
  if (started_) {
    const uint32_t since = uint32_t(ms - last_ms_);
    if (since < kTraceSampleMs) return false;
    last_ms_ = since < 2 * kTraceSampleMs ? last_ms_ + kTraceSampleMs : ms;
  } else {
    started_ = true;
    last_ms_ = ms;
  }
  const TracePoint p = tracePoint(r);
  points_[head_] = p;
  head_ = (head_ + 1) % kTracePoints;
  if (count_ < kTracePoints) ++count_;
  if (p.present) {
    window_ = traceWindow(window_, have_window_, p.celsius);
    have_window_ = true;
  }
  ++version_;
  return true;
}
const TracePoint &TraceHistory::point(int i) const {
  static const TracePoint kGap;
  if (i < 0 || i >= count_) return kGap;
  return points_[(head_ - count_ + i + kTracePoints) % kTracePoints];
}
} // namespace gauge
