#include "core/Calibration.h"
#include "core/TapeModel.h"
#include "core/Temperature.h"
#include "core/TraceModel.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>

using gauge::Rate; using gauge::RateTrend; using gauge::Status; using gauge::TraceHistory; using gauge::Unit;

static gauge::Reading make(Status s, bool has_t=false, float t_c=0, Status alarm=Status::Normal) {
  gauge::Reading r;
  r.status=s; r.alarm=alarm; r.has_temperature=has_t; r.temperature_c=t_c; r.resistance_ohms=123.4f;
  return r;
}
static gauge::Reading at(float t_c) { return make(Status::Normal,true,t_c); }
[[maybe_unused]] static bool near(float a, float b, float tol=1e-3f) { return std::fabs(a-b)<tol; }
static std::string textOf(const Rate &r, Unit u) { char b[gauge::kRateTextSize]; gauge::rateText(r,u,b,sizeof(b)); return b; }

int main() {
  assert(gauge::kTapeSpanC==40.f && gauge::kRateSamples==30 && gauge::kRateSteadyCPerMin==0.5f);
  const uint32_t P=gauge::kTraceSampleMs;

  // Tape offset: hotter is higher (negative offset), linear in the difference.
  assert(gauge::tapeOffsetPx(80.f,80.f,6.f)==0.f);
  assert(gauge::tapeOffsetPx(90.f,80.f,6.f)==-60.f && gauge::tapeOffsetPx(70.f,80.f,6.f)==60.f);
  assert(gauge::tapeOffsetPx(100.f,80.f,6.f)==-120.f);  // the visible edge: half of kTapeSpanC at 6 px/degC is 120 px

  // Trend and text, independent of calibration.
  assert(gauge::rateTrend(0.f)==RateTrend::Steady && gauge::rateTrend(0.49f)==RateTrend::Steady);
  assert(gauge::rateTrend(-0.49f)==RateTrend::Steady && gauge::rateTrend(NAN)==RateTrend::Steady);
  assert(gauge::rateTrend(0.5f)==RateTrend::Rising && gauge::rateTrend(-0.5f)==RateTrend::Falling);
  assert(gauge::rateTrend(INFINITY)==RateTrend::Rising && gauge::rateTrend(-INFINITY)==RateTrend::Falling);
  assert(textOf({false,6.f},Unit::Celsius).empty());
  assert(textOf({true,NAN},Unit::Celsius).empty());
  assert(textOf({true,0.2f},Unit::Celsius)=="STEADY" && textOf({true,-0.2f},Unit::Fahrenheit)=="STEADY");
  assert(textOf({true,2.4f},Unit::Celsius)=="2.4 \xC2\xB0""C/min");
  assert(textOf({true,-1.1f},Unit::Celsius)=="1.1 \xC2\xB0""C/min");                 // magnitude; the arrow gives the sign
  assert(textOf({true,5.f},Unit::Fahrenheit)=="9.0 \xC2\xB0""F/min");                // x 9/5, no 32 offset
  assert(textOf({true,0.5f},Unit::Celsius)=="0.5 \xC2\xB0""C/min");
  { char tiny[4]; gauge::rateText({true,2.4f},Unit::Celsius,tiny,sizeof(tiny)); assert(std::strlen(tiny)<sizeof(tiny)); }
  gauge::rateText({true,2.4f},Unit::Celsius,nullptr,8);  // no crash

  // Rate needs kRateSamples points: fewer is unavailable, whatever they say.
  { TraceHistory h; uint32_t ms=0;
    for(int i=0;i<gauge::kRateSamples-1;i++,ms+=P) h.update(at(50.f+i),ms);
    assert(!gauge::traceRate(h).available); }

#ifdef GAUGE_DEMO
  assert(gauge::calibrationValid());
  // Tape state: the reading; the table limits beyond it; hidden on any fault, even with an alarm latched.
  { gauge::TapeState t=gauge::tapeState(at(80.f)); assert(t.visible && t.centre_c==80.f);
    t=gauge::tapeState(make(Status::Red,true,131.f,Status::Red)); assert(t.visible && t.centre_c==131.f);
    t=gauge::tapeState(make(Status::AboveRange,false,0.f,Status::Red)); assert(t.visible && t.centre_c==gauge::calibrationMaxC());
    t=gauge::tapeState(make(Status::BelowRange)); assert(t.visible && t.centre_c==gauge::calibrationMinC());
    t=gauge::tapeState(make(Status::BelowRange,false,0.f,Status::Amber)); assert(t.visible && t.centre_c==gauge::calibrationMinC());
    assert(!gauge::tapeState(make(Status::Normal,true,NAN)).visible);
    assert(!gauge::tapeState(make(Status::Normal,false,80.f)).visible);  // stale temperature, no flag
    for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
      const Status s=static_cast<Status>(i);
      if(!gauge::isFault(s)) continue;
      for(Status a : {Status::Normal,Status::Amber,Status::Red}) assert(!gauge::tapeState(make(s,true,80.f,a)).visible);
    } }
  // Rate: known ramps, per minute, from the newest kRateSamples points.
  for(float slope_per_s : {0.1f, -0.05f, 0.f, 0.004f, 1.f}) {
    TraceHistory h; uint32_t ms=0;
    for(int i=0;i<gauge::kRateSamples+10;i++,ms+=P) h.update(at(80.f+slope_per_s*static_cast<float>(i)),ms);
    const Rate r=gauge::traceRate(h);
    assert(r.available && near(r.c_per_min,slope_per_s*60.f,1e-2f));
  }
  // A noisy ramp: least squares recovers the slope.
  { TraceHistory h; uint32_t ms=0;
    for(int i=0;i<gauge::kRateSamples;i++,ms+=P) h.update(at(90.f+0.05f*i+((i%2)?0.2f:-0.2f)),ms);
    const Rate r=gauge::traceRate(h); assert(r.available && std::fabs(r.c_per_min-3.f)<0.5f); }
  // Only the newest kRateSamples count: an old jump doesn't affect the rate.
  { TraceHistory h; uint32_t ms=0;
    for(int i=0;i<10;i++,ms+=P) h.update(at(40.f),ms);
    for(int i=0;i<gauge::kRateSamples;i++,ms+=P) h.update(at(100.f),ms);
    const Rate r=gauge::traceRate(h); assert(r.available && r.c_per_min==0.f && textOf(r,Unit::Celsius)=="STEADY"); }
  // Any gap, fault, below-range or above-range point among the newest kRateSamples: no rate. Older ones don't matter.
  for(const gauge::Reading &bad : {make(Status::OpenCircuit), make(Status::BelowRange),
                                   make(Status::AboveRange,false,0.f,Status::Red), make(Status::GroundFault,true,80.f,Status::Amber)}) {
    for(int pos : {0, gauge::kRateSamples/2, gauge::kRateSamples-1}) {
      TraceHistory h; uint32_t ms=0;
      for(int i=0;i<gauge::kRateSamples;i++,ms+=P) h.update(i==pos ? bad : at(80.f+0.1f*i),ms);
      assert(!gauge::traceRate(h).available);
      // Once kRateSamples clean points follow, the rate comes back.
      for(int i=0;i<gauge::kRateSamples;i++,ms+=P) h.update(at(90.f),ms);
      assert(gauge::traceRate(h).available);
    }
  }
  std::puts("Tape model tests passed (demo)");
#else
  assert(!gauge::calibrationValid());
  // Uncalibrated: no tape and no rate, whatever arrives.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Status a : {Status::Normal,Status::Amber,Status::Red}) {
      assert(!gauge::tapeState(make(s,false,0.f,a)).visible);
      assert(!gauge::tapeState(make(s,true,80.f,a)).visible);
    }
  }
  { TraceHistory h; uint32_t ms=0;
    for(int i=0;i<gauge::kRateSamples*3;i++,ms+=P) h.update(at(80.f+0.1f*i),ms);
    assert(!gauge::traceRate(h).available); }
  std::puts("Tape model tests passed (uncalibrated)");
#endif
  return 0;
}
