#include "WarmupModel.h"
#include "Calibration.h"
#include "Config.h"
#include "TrackModel.h"
#include <cmath>
#include <cstdio>
namespace gauge {
namespace {
bool knownTemperature(const Reading &r) { return r.has_temperature && std::isfinite(r.temperature_c); }
bool warmReading(const Reading &r) {
  if (r.status == Status::AboveRange) return true;
  return (r.status == Status::Normal || r.status == Status::Amber || r.status == Status::Red) && knownTemperature(r);
}
bool coldReading(const Reading &r) {
  return r.status == Status::BelowRange || (r.status == Status::Cold && knownTemperature(r));
}
} // namespace

NeedleState warmupRing(const Reading &r) { return needleOnScale(r, Unit::Celsius, 0.f, config::kWarmThresholdC); }

void WarmupClock::update(const Reading &r, uint32_t now_ms) {
  if (started_) {
    const uint32_t dt = now_ms - last_ms_;
    elapsed_ms_ = elapsed_ms_ > UINT32_MAX - dt ? UINT32_MAX : elapsed_ms_ + dt;
  }
  started_ = true;
  last_ms_ = now_ms;
  if (warm_ || !calibrationValid()) return;
  if (warmReading(r)) {
    warm_ = true;
    warm_ms_ = elapsed_ms_;
  } else if (coldReading(r)) {
    seen_cold_ = true;
  }
}

void warmupText(const WarmupClock &c, char *out, size_t size) {
  char t[kDurationTextSize];
  if (!calibrationValid()) {
    std::snprintf(out, size, "%s", "");
  } else if (c.warmAtStart()) {
    std::snprintf(out, size, "WARM AT START");
  } else if (c.warm()) {
    durationText(c.warmMs(), t, sizeof(t));
    std::snprintf(out, size, "WARM IN %s", t);
  } else {
    durationText(c.elapsedMs(), t, sizeof(t));
    std::snprintf(out, size, "%s", t);
  }
}
} // namespace gauge
