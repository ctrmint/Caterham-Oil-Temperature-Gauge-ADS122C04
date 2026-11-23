#include "WebLog.h"
#include "Calibration.h"
#include "Config.h"
#include "SweepModel.h"
#include "TrackModel.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
namespace gauge {
static_assert(kBands == 5, "WebStatus::band_ms holds Track's five bands");
namespace {
// Appends printf-style text at out + *used; false (and *used unchanged) when it doesn't fit.
bool add(char *out, size_t size, size_t *used, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
bool add(char *out, size_t size, size_t *used, const char *fmt, ...) {
  if (*used >= size) return false;
  va_list args;
  va_start(args, fmt);
  const int n = std::vsnprintf(out + *used, size - *used, fmt, args);
  va_end(args);
  if (n < 0 || static_cast<size_t>(n) >= size - *used) { out[*used] = '\0'; return false; }
  *used += static_cast<size_t>(n);
  return true;
}
// A number with fixed decimals, or null when it isn't finite.
bool num(char *out, size_t size, size_t *used, float v, int decimals) {
  if (!std::isfinite(v)) return add(out, size, used, "null");
  return add(out, size, used, "%.*f", decimals, static_cast<double>(v));
}
const char *unitText(Unit unit) { return unit == Unit::Fahrenheit ? "F" : "C"; }
} // namespace

LogRow makeLogRow(const Reading &r, uint32_t seq, uint32_t uptime_ms) {
  LogRow row;
  row.seq = seq;
  row.uptime_ms = uptime_ms;
  row.status = static_cast<uint8_t>(r.status);
  row.alarm = static_cast<uint8_t>(r.alarm);
  const bool has_t = calibrationValid() && r.has_temperature && std::isfinite(r.temperature_c);
  row.temp_c = has_t ? r.temperature_c : NAN;
  row.ohms = r.resistance_ohms;
  row.ref_v = r.reference_volts;
  row.sender_v = r.sensor_volts;
  row.supply_v = r.supply_volts;
  row.ground_v = r.ground_offset_volts;
  return row;
}

void LogBuffer::append(const Reading &r, uint32_t uptime_ms) {
  if (!rows_ || capacity_ == 0) return;
  rows_[next_ % capacity_] = makeLogRow(r, next_, uptime_ms);
  ++next_;
  if (count_ < capacity_) ++count_;
}

uint32_t LogBuffer::copy(uint32_t since, LogRow *out, uint32_t max) const {
  if (!rows_ || !out) return 0;
  uint32_t from = since < first() ? first() : since;
  uint32_t n = 0;
  for (; from < next_ && n < max; ++from, ++n) out[n] = rows_[from % capacity_];
  return n;
}

uint32_t parseSince(const char *query) {
  if (!query) return 0;
  const char *p = std::strstr(query, "since=");
  if (!p || (p != query && p[-1] != '&' && p[-1] != '?')) return 0;
  p += 6;
  uint64_t v = 0;
  int digits = 0;
  for (; *p >= '0' && *p <= '9'; ++p, ++digits) {
    v = v * 10 + static_cast<uint64_t>(*p - '0');
    if (v > 0xFFFFFFFFull || digits > 10) return 0;
  }
  if (digits == 0 || (*p != '\0' && *p != '&')) return 0;
  return static_cast<uint32_t>(v);
}

size_t webLogHead(const WebStatus &s, uint32_t first, uint32_t next, char *out, size_t size) {
  size_t used = 0;
  const Reading &r = s.reading;
  const bool calibrated = calibrationValid();
  const bool has_t = calibrated && r.has_temperature && std::isfinite(r.temperature_c);
  const bool above = calibrated && s.max_above_range;
  const bool has_max = above || (calibrated && std::isfinite(s.max_c) && s.max_c > kNoMaxYetC);
  bool ok = add(out, size, &used, "{\"v\":1,\"session\":\"%08lx\",\"first\":%lu,\"next\":%lu,\"demo\":%s,\"calibrated\":%s,"
                "\"now\":{\"uptime\":%lu,\"status\":%u,\"alarm\":%u,\"temp\":",
                static_cast<unsigned long>(s.session), static_cast<unsigned long>(first), static_cast<unsigned long>(next),
                s.demo ? "true" : "false", calibrated ? "true" : "false", static_cast<unsigned long>(s.uptime_ms),
                static_cast<unsigned>(r.status), static_cast<unsigned>(r.alarm));
  ok = ok && num(out, size, &used, has_t ? r.temperature_c : NAN, 1);
  ok = ok && add(out, size, &used, ",\"ohms\":") && num(out, size, &used, r.resistance_ohms, 1);
  ok = ok && add(out, size, &used, ",\"ref\":") && num(out, size, &used, r.reference_volts, 4);
  ok = ok && add(out, size, &used, ",\"sender\":") && num(out, size, &used, r.sensor_volts, 4);
  ok = ok && add(out, size, &used, ",\"supply\":") && num(out, size, &used, r.supply_volts, 4);
  ok = ok && add(out, size, &used, ",\"ground\":") && num(out, size, &used, r.ground_offset_volts, 4);
  // Sweep's LED, as the gauge decides it: off, steady, or pulsing slowly (Amber) or fast (Red) while an alarm is latched.
  const LedState led = ledState(r);
  const char *led_text = !led.lit ? "off" : led.pulse == LedPulse::Fast ? "fast" : led.pulse == LedPulse::Slow ? "slow" : "steady";
  ok = ok && add(out, size, &used, ",\"colour\":\"#%06lX\",\"led\":\"%s\",\"lit\":%d,\"max\":",
                 static_cast<unsigned long>(s.colour & 0xFFFFFFu), led_text, litSegments(r, s.unit));
  ok = ok && num(out, size, &used, above ? calibrationMaxC() : (has_max ? s.max_c : NAN), 1);
  // The warm-up text is the gauge's own, from a fixed set of characters: digits, letters, ':' and spaces.
  char warm[sizeof(s.warmup)];
  size_t w = 0;
  for (size_t i = 0; i < sizeof(s.warmup) && s.warmup[i]; ++i) {
    const char ch = s.warmup[i];
    if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == ':' || ch == ' ') warm[w++] = ch;
  }
  warm[w < sizeof(warm) ? w : sizeof(warm) - 1] = '\0';
  ok = ok && add(out, size, &used, ",\"maxAbove\":%s,\"warmup\":\"%s\"},\"bands\":[%lu,%lu,%lu,%lu,%lu],\"rows\":[",
                 above ? "true" : "false", warm, static_cast<unsigned long>(s.band_ms[0]),
                 static_cast<unsigned long>(s.band_ms[1]), static_cast<unsigned long>(s.band_ms[2]),
                 static_cast<unsigned long>(s.band_ms[3]), static_cast<unsigned long>(s.band_ms[4]));
  if (!ok) { if (size) out[0] = '\0'; return 0; }
  return used;
}

size_t webLogRow(const LogRow &row, bool comma, char *out, size_t size) {
  size_t used = 0;
  bool ok = add(out, size, &used, "%s[%lu,%lu,%u,%u,", comma ? "," : "", static_cast<unsigned long>(row.seq),
                static_cast<unsigned long>(row.uptime_ms), static_cast<unsigned>(row.status), static_cast<unsigned>(row.alarm));
  ok = ok && num(out, size, &used, row.temp_c, 1) && add(out, size, &used, ",");
  ok = ok && num(out, size, &used, row.ohms, 1) && add(out, size, &used, ",");
  ok = ok && num(out, size, &used, row.ref_v, 4) && add(out, size, &used, ",");
  ok = ok && num(out, size, &used, row.sender_v, 4) && add(out, size, &used, ",");
  ok = ok && num(out, size, &used, row.supply_v, 4) && add(out, size, &used, ",");
  ok = ok && num(out, size, &used, row.ground_v, 4) && add(out, size, &used, "]");
  if (!ok) { if (size) out[0] = '\0'; return 0; }
  return used;
}

size_t webScale(Unit unit, char *out, size_t size) {
  size_t used = 0;
  const SweepScale s = sweepScale(unit);
  bool ok = add(out, size, &used, "{\"v\":1,\"unit\":\"%s\",\"warm\":%.1f,\"amber\":%.1f,\"red\":%.1f,\"dial\":[%.1f,%.1f],",
                unitText(unit), static_cast<double>(config::kWarmThresholdC),
                static_cast<double>(config::kAmberThresholdC), static_cast<double>(config::kRedThresholdC),
                static_cast<double>(s.min), static_cast<double>(s.max));
  // Sweep's scale labels, only where the gauge shows them (calibrated).
  ok = ok && add(out, size, &used, "\"labels\":[");
  const int labels = scaleLabelsVisible() ? scaleLabelCount(unit) : 0;
  for (int i = 0; ok && i < labels; ++i) ok = add(out, size, &used, "%s%d", i ? "," : "", scaleLabel(unit, i));
  ok = ok && add(out, size, &used, "],\"segments\":[");
  for (int i = 0; ok && i < kSweepSegments; ++i)
    ok = add(out, size, &used, "%s\"#%06lX\"", i ? "," : "", static_cast<unsigned long>(segmentColour(i, unit) & 0xFFFFFFu));
  ok = ok && add(out, size, &used, "],\"gradient\":[");
  for (int c = 0; ok && c <= 160; c += 2)
    ok = add(out, size, &used, "%s\"#%06lX\"", c ? "," : "", static_cast<unsigned long>(gradientColour(static_cast<float>(c)) & 0xFFFFFFu));
  ok = ok && add(out, size, &used, "]}");
  if (!ok) { if (size) out[0] = '\0'; return 0; }
  return used;
}
} // namespace gauge
