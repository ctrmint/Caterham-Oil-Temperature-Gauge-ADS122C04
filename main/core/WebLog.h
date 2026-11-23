#pragma once
#include "DisplayText.h"
#include "WarmupModel.h"
#include "Temperature.h"
#include <stddef.h>
#include <stdint.h>
// The web page's log (#32): one row a second with the serial CSV log's values (REQ-D2), numbered from 0 at power-up, in a
// ring the caller provides (8 hours in PSRAM on the gauge), and the JSON the page polls for. Pure logic, so the host tests
// cover it; main/net/WebServer only serves it.
namespace gauge {
struct LogRow {
  uint32_t seq;
  uint32_t uptime_ms;
  uint8_t status, alarm;    // the frozen status codes (docs/UDP_STREAM.md)
  float temp_c;             // NaN unless the reading has a finite temperature and a verified calibration
  float ohms, ref_v, sender_v, supply_v, ground_v;
};
LogRow makeLogRow(const Reading &r, uint32_t seq, uint32_t uptime_ms);

// A ring of capacity rows. Row numbers never repeat within a power cycle; once full, the oldest go.
class LogBuffer {
 public:
  LogBuffer(LogRow *storage, uint32_t capacity) : rows_(storage), capacity_(capacity) {}
  void append(const Reading &r, uint32_t uptime_ms);
  uint32_t first() const { return next_ - count_; }  // the oldest row still held
  uint32_t next() const { return next_; }            // the number the next row will get
  // Copies up to max rows from row since (or the oldest held, if since is older) into out; returns how many.
  uint32_t copy(uint32_t since, LogRow *out, uint32_t max) const;
 private:
  LogRow *rows_;
  uint32_t capacity_;
  uint32_t next_ = 0;
  uint32_t count_ = 0;
};

// The page asks GET /log?since=N. Anything but a plain decimal number that fits in 32 bits means 0 (from the start).
uint32_t parseSince(const char *query);
constexpr uint32_t kLogBatchRows = 600;  // at most this many rows per answer; the page asks again for the rest

// What the page shows besides the rows, as the gauge has it now.
struct WebStatus {
  uint32_t session;        // random per power-up, as the UDP stream's
  uint32_t uptime_ms;
  Reading reading;
  uint32_t colour;         // readingColour(): the gauge's own colour for the reading
  float max_c;             // the session maximum, at or below kNoMaxYetC for none
  bool max_above_range;
  bool demo;
  uint32_t band_ms[5];     // Track's time per band (TrackModel's Band order)
  char warmup[kWarmupTextSize];  // warmupText(): "2:35", "WARM IN 6:12", "WARM AT START" or ""
  Unit unit;                     // the display unit, for Sweep's lit segments
};
// The JSON object's opening, up to and including "rows":[ ; then rows, comma-separated; then "]}". Each returns the
// characters written, or 0 (writing nothing usable) when out is too small. Non-finite numbers are written as null.
size_t webLogHead(const WebStatus &s, uint32_t first, uint32_t next, char *out, size_t size);
size_t webLogRow(const LogRow &row, bool comma, char *out, size_t size);
// GET /scale, fetched once: the alarm thresholds, the display unit, the dial's span and its 40 segment colours (as on
// Sweep), and the gauge's colour at every 2 degC from 0 to 160, so the page uses the gauge's own colours.
size_t webScale(Unit unit, char *out, size_t size);
} // namespace gauge
