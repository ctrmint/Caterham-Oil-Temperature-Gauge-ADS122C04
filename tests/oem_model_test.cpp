#include "core/Calibration.h"
#include "core/OemModel.h"
#include "core/SweepModel.h"
#include "core/Temperature.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

using gauge::NeedleState; using gauge::Status; using gauge::Unit;

static gauge::Reading make(Status s, bool has_t=false, float t_c=0, Status alarm=Status::Normal) {
  gauge::Reading r;
  r.status=s; r.alarm=alarm; r.has_temperature=has_t; r.temperature_c=t_c; r.resistance_ohms=123.4f;
  return r;
}
[[maybe_unused]] static NeedleState at(float t_c, Status s=Status::Normal, Status alarm=Status::Normal) {
  return gauge::oemNeedle(make(s,true,t_c,alarm));
}
[[maybe_unused]] static bool near(float a, float b) { return std::fabs(a-b)<1e-5f; }
static const Status kAlarms[]={Status::Normal,Status::Amber,Status::Red};

int main() {
  // The scale: 50-130 degC in 8 steps of 10, numbers on steps 2, 4, 6 and 8.
  assert(gauge::kOemMinC==50.f && gauge::kOemMaxC==130.f && gauge::kOemSteps==8 && gauge::kOemNumbers==4);
  assert((gauge::kOemMaxC-gauge::kOemMinC)/gauge::kOemSteps==10.f);
  { const int step[]={2,4,6,8}, c[]={70,90,110,130}, f[]={158,194,230,266};
    for(int i=0;i<gauge::kOemNumbers;i++) {
      assert(gauge::oemNumberStep(i)==step[i]);
      assert(gauge::oemLabel(Unit::Celsius,i)==c[i]);
      assert(gauge::oemLabel(Unit::Fahrenheit,i)==f[i]);
    }
    assert(gauge::oemNumberStep(gauge::kOemNumbers-1)==gauge::kOemSteps); }  // the last number is the scale's end

  // The wordmark never shows in the demo build, whatever the reading.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++)
    for(Status a : kAlarms) {
      assert(!gauge::oemWordmark(make(static_cast<Status>(i),true,80.f,a),true));
      assert(!gauge::oemWordmark(make(static_cast<Status>(i),false,0.f,a),true));
    }

#ifdef GAUGE_DEMO
  assert(gauge::calibrationValid());
  // The needle across the scale: the start, the middle (90 at 12 o'clock), the end, and each number.
  { NeedleState n=at(50.f); assert(n.visible && n.fraction==0.f);
    n=at(70.f); assert(n.visible && near(n.fraction,0.25f));
    n=at(90.f); assert(n.visible && near(n.fraction,0.5f));
    n=at(110.f); assert(n.visible && near(n.fraction,0.75f));
    n=at(130.f,Status::Red,Status::Red); assert(n.visible && near(n.fraction,1.f));
    n=at(117.f,Status::Amber,Status::Amber); assert(n.visible && near(n.fraction,67.f/80.f));
    // In the table but off the printed scale: rests at the nearer end.
    n=at(25.f,Status::Cold); assert(n.visible && n.fraction==0.f);
    n=at(49.9f,Status::Cold); assert(n.visible && n.fraction==0.f);
    n=at(135.f,Status::Red,Status::Red); assert(n.visible && n.fraction==1.f);
    // Beyond the table: the start below it (also during the REQ-T3 hold), the table maximum (capped) above it.
    n=gauge::oemNeedle(make(Status::BelowRange)); assert(n.visible && n.fraction==0.f);
    n=gauge::oemNeedle(make(Status::BelowRange,false,0.f,Status::Amber)); assert(n.visible && n.fraction==0.f);
    n=gauge::oemNeedle(make(Status::AboveRange,false,0.f,Status::Red)); assert(n.visible && n.fraction==1.f);
    assert(gauge::calibrationMaxC()>gauge::kOemMaxC);  // so the cap above is exercised
  }
  // Never outside 0..1 and never moving backwards as the temperature rises.
  { float last=-1.f;
    for(float t=-60.f;t<=220.f;t+=0.25f) {
      const NeedleState n=at(t); assert(n.visible && n.fraction>=0.f && n.fraction<=1.f && n.fraction>=last); last=n.fraction;
    } }
  // Hidden without a finite temperature, and on every fault, with or without a latched alarm.
  assert(!at(NAN).visible && !at(INFINITY).visible && !at(-INFINITY).visible);
  assert(!gauge::oemNeedle(make(Status::Normal,false,80.f)).visible);  // stale temperature, no flag
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Status a : kAlarms) for(bool has_t : {false,true}) {
      const gauge::Reading r=make(s,has_t,100.f,a);
      if(gauge::isFault(s)) assert(!gauge::oemNeedle(r).visible);
      // Same visibility as the Needle dial's needle: the two layouts never disagree about whether there's a reading.
      assert(gauge::oemNeedle(r).visible==gauge::needleState(r,Unit::Celsius).visible);
    }
  }
  // The wordmark: only NORMAL or OIL WARMING with a finite temperature, no alarm latched, not the demo build.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Status a : kAlarms) for(bool has_t : {false,true}) for(float t : {25.f,80.f,NAN,INFINITY}) {
      const bool want=a==Status::Normal && (s==Status::Normal || s==Status::Cold) && has_t && std::isfinite(t);
      assert(gauge::oemWordmark(make(s,has_t,t,a),false)==want);
    }
  }
  assert(gauge::oemWordmark(make(Status::Normal,true,80.f),false));
  assert(gauge::oemWordmark(make(Status::Cold,true,25.f),false));
  assert(!gauge::oemWordmark(make(Status::Normal,true,80.f,Status::Amber),false));  // still latched while cooling
  std::puts("OEM model tests passed (demo)");
#else
  assert(!gauge::calibrationValid());
  // Uncalibrated: no needle and no wordmark, whatever arrives.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Status a : kAlarms) for(bool demo : {false,true}) {
      assert(!gauge::oemNeedle(make(s,false,0.f,a)).visible);
      assert(!gauge::oemNeedle(make(s,true,80.f,a)).visible);
      assert(!gauge::oemWordmark(make(s,true,80.f,a),demo));
      assert(!gauge::oemWordmark(make(s,false,0.f,a),demo));
    }
  }
  std::puts("OEM model tests passed (uncalibrated)");
#endif
  return 0;
}
