#include "TrackModel.h"
#include "Calibration.h"
#include "Config.h"
#include <cmath>
#include <cstdio>
namespace gauge {
namespace {
bool knownTemperature(const Reading &r) { return r.has_temperature && std::isfinite(r.temperature_c); }
uint32_t addSat(uint32_t a, uint32_t b) { return a > UINT32_MAX - b ? UINT32_MAX : a + b; }
long rounded(float c, Unit unit) { long n = 0; roundHalfAway(toUnit(c, unit), n); return n; }
} // namespace

Band trackBand(const Reading &r) {
  if (!calibrationValid()) return Band::NoReading;
  switch (r.status) {
    case Status::Cold: return knownTemperature(r) ? Band::Warming : Band::NoReading;
    case Status::Normal: return knownTemperature(r) ? Band::Normal : Band::NoReading;
    case Status::Amber: return knownTemperature(r) ? Band::High : Band::NoReading;
    case Status::Red: return knownTemperature(r) ? Band::Critical : Band::NoReading;
    case Status::AboveRange: return Band::Critical;
    case Status::BelowRange: return calibrationMinC() < config::kWarmThresholdC ? Band::Warming : Band::NoReading;
    default: return Band::NoReading;
  }
}

void BandTimes::update(const Reading &r, uint32_t now_ms) {
  if (started_) {
    const uint32_t dt = now_ms - last_ms_;
    const Band b = dt > kBandGapMs ? Band::NoReading : last_band_;
    ms_[static_cast<int>(b)] = addSat(ms_[static_cast<int>(b)], dt);
  }
  started_ = true;
  last_ms_ = now_ms;
  last_band_ = trackBand(r);
}

uint32_t BandTimes::totalMs() const {
  uint32_t t = 0;
  for (uint32_t m : ms_) t = addSat(t, m);
  return t;
}

void bandBars(const BandTimes &t, int width_px, int (&out)[kBands]) {
  for (int &o : out) o = 0;
  double total = 0;
  for (int b = 0; b < kBands; ++b) total += t.ms(static_cast<Band>(b));
  if (total <= 0 || width_px <= 0) return;
  double rem[kBands];
  int used = 0;
  for (int b = 0; b < kBands; ++b) {
    const double exact = static_cast<double>(width_px) * t.ms(static_cast<Band>(b)) / total;
    out[b] = static_cast<int>(std::floor(exact));
    rem[b] = exact - out[b];
    used += out[b];
  }
  for (; used < width_px; ++used) {  // the largest remainders round up
    int best = 0;
    for (int b = 1; b < kBands; ++b) if (rem[b] > rem[best]) best = b;
    ++out[best];
    rem[best] = -1;
  }
  for (int b = 0; b < kBands; ++b) {  // any time at all shows, taken from the longest bar
    if (t.ms(static_cast<Band>(b)) == 0 || out[b] >= kMinBarPx) continue;
    int longest = 0;
    for (int k = 1; k < kBands; ++k) if (out[k] > out[longest]) longest = k;
    const int need = kMinBarPx - out[b];
    if (out[longest] - need < kMinBarPx) continue;  // too narrow to share
    out[longest] -= need;
    out[b] += need;
  }
}

void durationText(uint32_t ms, char *out, size_t size) {
  const uint32_t s = ms / 1000;
  if (s < 3600) std::snprintf(out, size, "%u:%02u", static_cast<unsigned>(s / 60), static_cast<unsigned>(s % 60));
  else if (s / 3600 <= 99) std::snprintf(out, size, "%uh%02u", static_cast<unsigned>(s / 3600), static_cast<unsigned>(s / 60 % 60));
  else std::snprintf(out, size, "99h+");
}

void bandLabel(Band b, Unit unit, char *out, size_t size) {
  const long warm = rounded(config::kWarmThresholdC, unit), amber = rounded(config::kAmberThresholdC, unit);
  const long red = rounded(config::kRedThresholdC, unit);
  switch (b) {
    case Band::Warming: std::snprintf(out, size, "<%ld", warm); break;
    case Band::Normal: std::snprintf(out, size, "%ld-%ld", warm, amber); break;
    case Band::High: std::snprintf(out, size, "%ld-%ld", amber, red); break;
    case Band::Critical: std::snprintf(out, size, "%ld+", red); break;
    default: std::snprintf(out, size, "--"); break;
  }
}

void maxText(float max_c, bool max_above_range, Unit unit, char *out, size_t size) {
  long n = 0;
  if (!calibrationValid()) std::snprintf(out, size, "%s", "");
  else if (max_above_range && roundHalfAway(toUnit(calibrationMaxC(), unit), n)) std::snprintf(out, size, "MAX >%ld", n);
  else if (!max_above_range && max_c > kNoMaxYetC && roundHalfAway(toUnit(max_c, unit), n)) std::snprintf(out, size, "MAX %ld", n);
  else std::snprintf(out, size, "%s", "");
}
} // namespace gauge
