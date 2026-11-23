#pragma once
#include "DisplayText.h"
#include <stddef.h>
#include <stdint.h>
// Track (#24): a data page for track days. The time spent in each temperature band since power-up, the session maximum
// and the rate of change. Pure logic, so the host tests cover it; the display only draws it.
namespace gauge {
// The bands, hottest last, then the time with no temperature known.
enum class Band : uint8_t { Warming, Normal, High, Critical, NoReading };
constexpr int kBands = 5;
// The band a reading counts towards. Fail closed: only a known temperature counts towards a temperature band.
//  - Warming: OIL WARMING, and BELOW CAL RANGE when the table starts below the warm threshold (else it can't be placed).
//  - Normal: NORMAL. High: HIGH TEMPERATURE. Critical: CRITICAL TEMP, and ABOVE CAL RANGE (which drives the Red alarm).
//  - No reading: everything else (faults, CALIBRATION REQUIRED), any status without a finite temperature where one is
//    expected, and every reading in an uncalibrated build.
Band trackBand(const Reading &r);
// A sample gap longer than this (a stall) counts as No reading: time the gauge didn't see is never credited to a
// temperature band.
constexpr uint32_t kBandGapMs = 2000;
// The time per band since the first sample. Each update adds the time since the previous one (unsigned millisecond
// subtraction, so safe across the counter's wrap) to the previous reading's band, or to No reading after a gap. The
// totals saturate instead of wrapping (after about 49 days).
class BandTimes {
 public:
  void update(const Reading &r, uint32_t now_ms);
  uint32_t ms(Band b) const { return ms_[static_cast<int>(b)]; }
  uint32_t totalMs() const;
 private:
  bool started_ = false;
  uint32_t last_ms_ = 0;
  Band last_band_ = Band::NoReading;
  uint32_t ms_[kBands] = {};
};
// Bar lengths in whole pixels for a full bar width_px wide: each band's share of the total, adding up to width_px exactly
// (largest remainders round up), and at least kMinBarPx for any band with time in it, taken from the longest, so a short
// spell in a band always shows. All zero before any time is counted.
constexpr int kMinBarPx = 2;
void bandBars(const BandTimes &t, int width_px, int (&out)[kBands]);
// m:ss up to 59:59, then 1h00 .. 99h59, then "99h+".
constexpr size_t kDurationTextSize = 8;
void durationText(uint32_t ms, char *out, size_t size);
// The band's label in the display unit, from the thresholds in Config.h: "<60", "60-115", "115-130", "130+", "--"
// (degF: "<140", "140-239", "239-266", "266+").
constexpr size_t kBandLabelSize = 12;
void bandLabel(Band b, Unit unit, char *out, size_t size);
// "MAX 121", "MAX >130" once the maximum went above the table, or "" when there's no maximum yet or no calibration.
void maxText(float max_c, bool max_above_range, Unit unit, char *out, size_t size);
} // namespace gauge
