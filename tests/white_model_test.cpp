#include "core/Calibration.h"
#include "core/OemModel.h"
#include "core/SweepModel.h"
#include "core/Temperature.h"
#include "core/WhiteModel.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

using gauge::NeedleState; using gauge::Status; using gauge::TickSize; using gauge::Unit;

static gauge::Reading make(Status s, bool has_t=false, float t_c=0, Status alarm=Status::Normal) {
  gauge::Reading r;
  r.status=s; r.alarm=alarm; r.has_temperature=has_t; r.temperature_c=t_c; r.resistance_ohms=123.4f;
  return r;
}
[[maybe_unused]] static NeedleState at(float t_c, Status s=Status::Normal, Status alarm=Status::Normal) {
  return gauge::whiteNeedle(make(s,true,t_c,alarm));
}
[[maybe_unused]] static bool near(float a, float b) { return std::fabs(a-b)<1e-5f; }
static const Status kAlarms[]={Status::Normal,Status::Amber,Status::Red};

int main() {
  // The scale: 40-140 degC, a tick every 2 degC, so 51 ticks from index 0 (40) to 50 (140).
  assert(gauge::kWhiteMinC==40.f && gauge::kWhiteMaxC==140.f && gauge::kWhiteTickC==2.f);
  assert(gauge::kWhiteTicks==51 && gauge::kWhiteNumbers==6);
  assert((gauge::kWhiteMaxC-gauge::kWhiteMinC)/gauge::kWhiteTickC==gauge::kWhiteTicks-1);
  // Long every 20 degC, medium at the other tens, short at the rest.
  { int major=0, medium=0, minor=0;
    for(int i=0;i<gauge::kWhiteTicks;i++) {
      const int t=40+2*i;
      const TickSize want=(t-40)%20==0 ? TickSize::Major : (t%10==0 ? TickSize::Medium : TickSize::Minor);
      assert(gauge::whiteTick(i)==want);
      if(want==TickSize::Major) major++; else if(want==TickSize::Medium) medium++; else minor++;
    }
    assert(major==6 && medium==5 && minor==40); }
  assert(gauge::whiteTick(0)==TickSize::Major && gauge::whiteTick(1)==TickSize::Minor && gauge::whiteTick(4)==TickSize::Minor);
  assert(gauge::whiteTick(5)==TickSize::Medium && gauge::whiteTick(10)==TickSize::Major && gauge::whiteTick(25)==TickSize::Medium);
  assert(gauge::whiteTick(49)==TickSize::Minor && gauge::whiteTick(50)==TickSize::Major);
  // A number on each long tick, the first at the start and the last at the end, in degC and degF.
  { const int tick[]={0,10,20,30,40,50}, c[]={40,60,80,100,120,140}, f[]={104,140,176,212,248,284};
    for(int i=0;i<gauge::kWhiteNumbers;i++) {
      assert(gauge::whiteNumberTick(i)==tick[i]);
      assert(gauge::whiteTick(gauge::whiteNumberTick(i))==TickSize::Major);
      assert(gauge::whiteLabel(Unit::Celsius,i)==c[i]);
      assert(gauge::whiteLabel(Unit::Fahrenheit,i)==f[i]);
    }
    assert(gauge::whiteNumberTick(gauge::kWhiteNumbers-1)==gauge::kWhiteTicks-1); }

  // The status window always shows in the demo build, whatever the reading.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++)
    for(Status a : kAlarms) {
      assert(gauge::whiteWindow(make(static_cast<Status>(i),true,80.f,a),true));
      assert(gauge::whiteWindow(make(static_cast<Status>(i),false,0.f,a),true));
    }

#ifdef GAUGE_DEMO
  assert(gauge::calibrationValid());
  // The needle across the scale: the start, the middle (90 at 12 o'clock), the end, and each number.
  { NeedleState n=at(40.f); assert(n.visible && n.fraction==0.f);
    n=at(60.f); assert(n.visible && near(n.fraction,0.2f));
    n=at(80.f); assert(n.visible && near(n.fraction,0.4f));
    n=at(90.f); assert(n.visible && near(n.fraction,0.5f));
    n=at(100.f); assert(n.visible && near(n.fraction,0.6f));
    n=at(117.f,Status::Amber,Status::Amber); assert(n.visible && near(n.fraction,0.77f));
    n=at(120.f,Status::Amber,Status::Amber); assert(n.visible && near(n.fraction,0.8f));
    n=at(140.f,Status::Red,Status::Red); assert(n.visible && near(n.fraction,1.f));
    // In the table but off the printed scale: rests at the start.
    n=at(25.f,Status::Cold); assert(n.visible && n.fraction==0.f);
    n=at(39.9f,Status::Cold); assert(n.visible && n.fraction==0.f);
    n=at(150.f,Status::Red,Status::Red); assert(n.visible && n.fraction==1.f);
    // Beyond the table: the start below it (also during the REQ-T3 hold), the table maximum above it.
    n=gauge::whiteNeedle(make(Status::BelowRange)); assert(n.visible && n.fraction==0.f);
    n=gauge::whiteNeedle(make(Status::BelowRange,false,0.f,Status::Amber)); assert(n.visible && n.fraction==0.f);
    n=gauge::whiteNeedle(make(Status::AboveRange,false,0.f,Status::Red));
    assert(n.visible && near(n.fraction,(gauge::calibrationMaxC()-gauge::kWhiteMinC)/(gauge::kWhiteMaxC-gauge::kWhiteMinC)));
  }
  // Never outside 0..1 and never moving backwards as the temperature rises.
  { float last=-1.f;
    for(float t=-60.f;t<=220.f;t+=0.25f) {
      const NeedleState n=at(t); assert(n.visible && n.fraction>=0.f && n.fraction<=1.f && n.fraction>=last); last=n.fraction;
    } }
  // Hidden without a finite temperature, and on every fault, with or without a latched alarm.
  assert(!at(NAN).visible && !at(INFINITY).visible && !at(-INFINITY).visible);
  assert(!gauge::whiteNeedle(make(Status::Normal,false,80.f)).visible);  // stale temperature, no flag
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Status a : kAlarms) for(bool has_t : {false,true}) {
      const gauge::Reading r=make(s,has_t,100.f,a);
      if(gauge::isFault(s)) assert(!gauge::whiteNeedle(r).visible);
      // Same visibility as the Needle dial's needle: the layouts never disagree about whether there's a reading.
      assert(gauge::whiteNeedle(r).visible==gauge::needleState(r,Unit::Celsius).visible);
    }
  }
  // The window: hidden only for NORMAL or OIL WARMING with a finite temperature, no alarm latched, not the demo build.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Status a : kAlarms) for(bool has_t : {false,true}) for(float t : {25.f,80.f,NAN,INFINITY}) for(bool demo : {false,true}) {
      const gauge::Reading r=make(s,has_t,t,a);
      const bool quiet=!demo && a==Status::Normal && (s==Status::Normal || s==Status::Cold) && has_t && std::isfinite(t);
      assert(gauge::whiteWindow(r,demo)==!quiet);
      assert(gauge::allIsWell(r,demo)==quiet);
      assert(gauge::whiteWindow(r,demo)==!gauge::oemWordmark(r,demo));  // the window shows exactly when CATERHAM doesn't
    }
  }
  assert(!gauge::whiteWindow(make(Status::Normal,true,80.f),false));
  assert(!gauge::whiteWindow(make(Status::Cold,true,25.f),false));
  assert(gauge::whiteWindow(make(Status::Normal,true,80.f,Status::Amber),false));  // still latched while cooling
  assert(gauge::whiteWindow(make(Status::BelowRange,false,0.f),false));
  std::puts("White model tests passed (demo)");
#else
  assert(!gauge::calibrationValid());
  // Uncalibrated: no needle, and the window always shows (CALIBRATION REQUIRED), whatever arrives.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Status a : kAlarms) for(bool demo : {false,true}) {
      assert(!gauge::whiteNeedle(make(s,false,0.f,a)).visible);
      assert(!gauge::whiteNeedle(make(s,true,80.f,a)).visible);
      assert(gauge::whiteWindow(make(s,true,80.f,a),demo));
      assert(gauge::whiteWindow(make(s,false,0.f,a),demo));
      assert(!gauge::allIsWell(make(s,true,80.f,a),demo));
    }
  }
  std::puts("White model tests passed (uncalibrated)");
#endif
  return 0;
}
