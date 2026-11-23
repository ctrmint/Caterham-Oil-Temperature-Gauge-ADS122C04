#include "core/Calibration.h"
#include "core/TrackModel.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

using gauge::Band; using gauge::Status; using gauge::Unit;

static gauge::Reading make(Status s, bool has_t=false, float t_c=0) {
  gauge::Reading r;
  r.status=s; r.alarm=Status::Normal; r.has_temperature=has_t; r.temperature_c=t_c; r.resistance_ohms=123.4f;
  return r;
}
static bool text(void (*fn)(uint32_t, char*, size_t), uint32_t v, const char *want) {
  char out[gauge::kDurationTextSize]; fn(v, out, sizeof(out)); return std::strcmp(out, want) == 0;
}
static bool label(Band b, Unit u, const char *want) {
  char out[gauge::kBandLabelSize]; gauge::bandLabel(b, u, out, sizeof(out)); return std::strcmp(out, want) == 0;
}
static bool maxIs(float c, bool above, Unit u, const char *want) {
  char out[24]; gauge::maxText(c, above, u, out, sizeof(out)); return std::strcmp(out, want) == 0;
}
static int sum(const int (&b)[gauge::kBands]) { int s = 0; for (int v : b) s += v; return s; }

int main() {
  constexpr uint32_t H = 3600000u, M = 60000u;
  // The time text: m:ss, then hours and minutes, then capped.
  assert(text(gauge::durationText, 0, "0:00") && text(gauge::durationText, 999, "0:00") && text(gauge::durationText, 59999, "0:59"));
  assert(text(gauge::durationText, M, "1:00") && text(gauge::durationText, 125000, "2:05") && text(gauge::durationText, H - 1, "59:59"));
  assert(text(gauge::durationText, H, "1h00") && text(gauge::durationText, H + 5 * M + 59999, "1h05"));
  assert(text(gauge::durationText, 99 * H + 59 * M + 59999, "99h59") && text(gauge::durationText, 100 * H, "99h+"));
  assert(text(gauge::durationText, UINT32_MAX, "99h+"));
  { char small[4]; gauge::durationText(H - 1, small, sizeof(small)); assert(std::strlen(small) == 3); }  // truncates, never overruns

  // The labels come from the thresholds (60, 115, 130 degC), in the display unit.
  assert(label(Band::Warming, Unit::Celsius, "<60") && label(Band::Normal, Unit::Celsius, "60-115"));
  assert(label(Band::High, Unit::Celsius, "115-130") && label(Band::Critical, Unit::Celsius, "130+"));
  assert(label(Band::NoReading, Unit::Celsius, "--") && label(Band::NoReading, Unit::Fahrenheit, "--"));
  assert(label(Band::Warming, Unit::Fahrenheit, "<140") && label(Band::Normal, Unit::Fahrenheit, "140-239"));
  assert(label(Band::High, Unit::Fahrenheit, "239-266") && label(Band::Critical, Unit::Fahrenheit, "266+"));

  // Bars: nothing before any time; otherwise each band's share, adding up to the full width, with any time at all shown.
  { gauge::BandTimes t; int b[gauge::kBands] = {9, 9, 9, 9, 9};
    gauge::bandBars(t, 200, b); for (int v : b) assert(v == 0);
    t.update(make(Status::Normal, true, 80.f), 1000); gauge::bandBars(t, 200, b); for (int v : b) assert(v == 0); }
  // The timer: nothing before the second sample; each gap goes to the previous reading's band, or to No reading after a
  // stall; across the millisecond wrap; saturating, never wrapping.
  { gauge::BandTimes t;
    t.update(make(Status::Normal, true, 80.f), 5000); assert(t.totalMs() == 0);
    t.update(make(Status::OpenCircuit), 5250);          // the 250 ms before the fault go to the reading before it
    t.update(make(Status::OpenCircuit), 5500);
    t.update(make(Status::Normal, true, 80.f), 7500);   // exactly the gap limit: still the previous band
    t.update(make(Status::Normal, true, 80.f), 9501);   // one over: a stall, so No reading
    assert(t.ms(Band::Normal) == (gauge::calibrationValid() ? 250u : 0u));
    assert(t.ms(Band::NoReading) == (gauge::calibrationValid() ? 250u + 2000u + 2001u : 4501u));
    assert(t.totalMs() == 9501 - 5000); }
  { gauge::BandTimes t; uint32_t now = 0xFFFFFE00u;
    t.update(make(Status::Amber, true, 118.f), now);
    for (int i = 0; i < 8; ++i) t.update(make(Status::Amber, true, 118.f), now += 250);  // crosses the wrap
    assert(now < 0x1000u && t.totalMs() == 2000u);
    assert(t.ms(gauge::calibrationValid() ? Band::High : Band::NoReading) == 2000u); }
  { gauge::BandTimes t; uint32_t now = 0;
    t.update(make(Status::Normal, true, 80.f), now);
    for (uint32_t i = 0; i < 2200000u; ++i) t.update(make(Status::Normal, true, 80.f), now += 1999);  // past 2^32 ms in all
    const Band b = gauge::calibrationValid() ? Band::Normal : Band::NoReading;
    assert(t.ms(b) == UINT32_MAX && t.totalMs() == UINT32_MAX); }

#ifdef GAUGE_DEMO
  assert(gauge::calibrationValid() && gauge::calibrationMinC() < 60.f);
  // The band of every status: a known temperature for the temperature bands, the table's ends beyond it.
  assert(gauge::trackBand(make(Status::Cold, true, 30.f)) == Band::Warming);
  assert(gauge::trackBand(make(Status::Normal, true, 80.f)) == Band::Normal);
  assert(gauge::trackBand(make(Status::Amber, true, 118.f)) == Band::High);
  assert(gauge::trackBand(make(Status::Red, true, 135.f)) == Band::Critical);
  assert(gauge::trackBand(make(Status::AboveRange)) == Band::Critical);
  assert(gauge::trackBand(make(Status::BelowRange)) == Band::Warming);  // the demo table starts at 20 degC
  for (Status s : {Status::Cold, Status::Normal, Status::Amber, Status::Red}) {
    assert(gauge::trackBand(make(s, false, 80.f)) == Band::NoReading);
    assert(gauge::trackBand(make(s, true, NAN)) == Band::NoReading);
    assert(gauge::trackBand(make(s, true, INFINITY)) == Band::NoReading);
  }
  for (Status s : {Status::NoAdc, Status::BadSupply, Status::GroundFault, Status::ShortCircuit, Status::OpenCircuit, Status::NeedsCalibration}) {
    assert(gauge::trackBand(make(s)) == Band::NoReading && gauge::trackBand(make(s, true, 80.f)) == Band::NoReading);
  }
  { gauge::BandTimes t; uint32_t now = 0;
    const Status seq[] = {Status::Cold, Status::Normal, Status::Normal, Status::Normal, Status::Amber};
    t.update(make(Status::Cold, true, 30.f), now);
    for (Status s : seq) for (int i = 0; i < 400; ++i) { now += 250; t.update(make(s, true, s == Status::Cold ? 30.f : 100.f), now); }
    // 400 updates of 250 ms in each step; the first update of a step goes to the band of the reading before it.
    assert(t.ms(Band::Warming) == 100250u && t.ms(Band::Normal) == 300000u && t.ms(Band::High) == 99750u);
    assert(t.totalMs() == now);
    int b[gauge::kBands]; gauge::bandBars(t, 200, b);
    assert(sum(b) == 200 && b[0] == 40 && b[1] == 120 && b[2] == 40 && b[3] == 0 && b[4] == 0);  // 40.1, 120, 39.9
    gauge::bandBars(t, 0, b); for (int v : b) assert(v == 0); }
  { gauge::BandTimes t; uint32_t now = 1000;  // ten hours normal, five seconds critical: the critical bar still shows
    t.update(make(Status::Normal, true, 90.f), now);
    for (uint32_t i = 0; i < 10 * 3600 * 4; ++i) { now += 250; t.update(make(Status::Normal, true, 90.f), now); }
    t.update(make(Status::Red, true, 135.f), now += 250);
    for (int i = 0; i < 20; ++i) t.update(make(Status::Red, true, 135.f), now += 250);
    t.update(make(Status::Normal, true, 90.f), now += 250);
    assert(t.ms(Band::Critical) == 21 * 250u);
    int b[gauge::kBands]; gauge::bandBars(t, 200, b);
    assert(sum(b) == 200 && b[3] == gauge::kMinBarPx && b[1] == 200 - gauge::kMinBarPx && b[0] == 0 && b[2] == 0 && b[4] == 0); }
  { gauge::BandTimes t; uint32_t now = 0;  // three equal thirds of 100 px: the remainder goes to one bar, still adding up
    t.update(make(Status::Cold, true, 30.f), now);
    t.update(make(Status::Normal, true, 80.f), now += 1000);
    t.update(make(Status::Amber, true, 118.f), now += 1000);
    t.update(make(Status::Amber, true, 118.f), now += 1000);
    int b[gauge::kBands]; gauge::bandBars(t, 100, b);
    assert(sum(b) == 100 && b[0] >= 33 && b[0] <= 34 && b[1] >= 33 && b[1] <= 34 && b[2] >= 33 && b[2] <= 34); }

  // The maximum: whole degrees, ">max" above the table, nothing before the first reading.
  assert(maxIs(121.4f, false, Unit::Celsius, "MAX 121") && maxIs(121.5f, false, Unit::Celsius, "MAX 122"));
  assert(maxIs(121.4f, false, Unit::Fahrenheit, "MAX 251") && maxIs(0.f, true, Unit::Celsius, "MAX >140"));
  assert(maxIs(0.f, true, Unit::Fahrenheit, "MAX >284") && maxIs(gauge::kNoMaxYetC, false, Unit::Celsius, ""));
  assert(maxIs(NAN, false, Unit::Celsius, "") && maxIs(-20.f, false, Unit::Celsius, "MAX -20"));
  std::puts("Track model tests passed (demo)");
#else
  assert(!gauge::calibrationValid());
  // Uncalibrated: every reading is No reading, and there's no maximum.
  for (int i = static_cast<int>(Status::NoAdc); i <= static_cast<int>(Status::Red); i++)
    for (bool has_t : {false, true}) assert(gauge::trackBand(make(static_cast<Status>(i), has_t, 80.f)) == Band::NoReading);
  assert(maxIs(121.f, false, Unit::Celsius, "") && maxIs(0.f, true, Unit::Celsius, ""));
  { gauge::BandTimes t; uint32_t now = 0;  // all the time goes to No reading, so its bar is the whole width
    for (Status s : {Status::NeedsCalibration, Status::Normal, Status::OpenCircuit, Status::Amber}) t.update(make(s, true, 80.f), now += 250);
    int b[gauge::kBands]; gauge::bandBars(t, 200, b);
    assert(t.ms(Band::NoReading) == 750u && b[4] == 200 && sum(b) == 200); }
  std::puts("Track model tests passed (uncalibrated)");
#endif
  return 0;
}
