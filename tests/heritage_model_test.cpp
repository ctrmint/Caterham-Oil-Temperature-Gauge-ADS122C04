#include "core/Calibration.h"
#include "core/HeritageModel.h"
#include "core/OemModel.h"
#include "core/SweepModel.h"
#include "core/Temperature.h"
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
  return gauge::heritageNeedle(make(s,true,t_c,alarm));
}
[[maybe_unused]] static bool near(float a, float b) { return std::fabs(a-b)<1e-5f; }
static const Status kAlarms[]={Status::Normal,Status::Amber,Status::Red};

int main() {
  // The scale: 50-130 degC, a tick every 5 degC, so 17 ticks from index 0 (50) to 16 (130).
  assert(gauge::kHeritageMinC==50.f && gauge::kHeritageMaxC==130.f && gauge::kHeritageTickC==5.f);
  assert(gauge::kHeritageTicks==17 && gauge::kHeritageNumbers==3);
  assert((gauge::kHeritageMaxC-gauge::kHeritageMinC)/gauge::kHeritageTickC==gauge::kHeritageTicks-1);
  // Long every 20 degC from 50, medium at the other tens, short at the fives.
  { int major=0, medium=0, minor=0;
    for(int i=0;i<gauge::kHeritageTicks;i++) {
      const int t=50+5*i;
      const TickSize want=(t-50)%20==0 ? TickSize::Major : (t%10==0 ? TickSize::Medium : TickSize::Minor);
      assert(gauge::heritageTick(i)==want);
      if(want==TickSize::Major) major++; else if(want==TickSize::Medium) medium++; else minor++;
    }
    assert(major==5 && medium==4 && minor==8); }
  assert(gauge::heritageTick(0)==TickSize::Major && gauge::heritageTick(1)==TickSize::Minor && gauge::heritageTick(2)==TickSize::Medium);
  assert(gauge::heritageTick(4)==TickSize::Major && gauge::heritageTick(6)==TickSize::Medium && gauge::heritageTick(8)==TickSize::Major);
  assert(gauge::heritageTick(15)==TickSize::Minor && gauge::heritageTick(16)==TickSize::Major);
  // Numbers at the start, the middle (90, at 12 o'clock) and the end, on long ticks, in degC and degF.
  { const int tick[]={0,8,16}, c[]={50,90,130}, f[]={122,194,266};
    for(int i=0;i<gauge::kHeritageNumbers;i++) {
      assert(gauge::heritageNumberTick(i)==tick[i]);
      assert(gauge::heritageTick(gauge::heritageNumberTick(i))==TickSize::Major);
      assert(gauge::heritageLabel(Unit::Celsius,i)==c[i]);
      assert(gauge::heritageLabel(Unit::Fahrenheit,i)==f[i]);
    }
    assert(gauge::heritageNumberTick(gauge::kHeritageNumbers-1)==gauge::kHeritageTicks-1);
    assert(2*gauge::heritageNumberTick(1)==gauge::kHeritageTicks-1); }

  // The demo build never shows the legend: the status lines and DEMO ONLY - NOT REAL are always there.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++)
    for(Status a : kAlarms) {
      assert(!gauge::heritageLegend(make(static_cast<Status>(i),true,80.f,a),true));
      assert(!gauge::heritageLegend(make(static_cast<Status>(i),false,0.f,a),true));
    }

#ifdef GAUGE_DEMO
  assert(gauge::calibrationValid());
  // The needle across the scale: the start, each tick of note, the middle (90 at 12 o'clock) and the end.
  { NeedleState n=at(50.f); assert(n.visible && n.fraction==0.f);
    n=at(70.f); assert(n.visible && near(n.fraction,0.25f));
    n=at(90.f); assert(n.visible && near(n.fraction,0.5f));
    n=at(110.f); assert(n.visible && near(n.fraction,0.75f));
    n=at(117.f,Status::Amber,Status::Amber); assert(n.visible && near(n.fraction,0.8375f));
    n=at(130.f,Status::Red,Status::Red); assert(n.visible && near(n.fraction,1.f));
    // In the table but off the scale: rests at the start, or stops at the end.
    n=at(25.f,Status::Cold); assert(n.visible && n.fraction==0.f);
    n=at(49.9f,Status::Cold); assert(n.visible && n.fraction==0.f);
    n=at(135.f,Status::Red,Status::Red); assert(n.visible && n.fraction==1.f);
    // Beyond the table: the start below it, the table maximum (capped at the end) above it.
    n=gauge::heritageNeedle(make(Status::BelowRange)); assert(n.visible && n.fraction==0.f);
    n=gauge::heritageNeedle(make(Status::BelowRange,false,0.f,Status::Amber)); assert(n.visible && n.fraction==0.f);
    n=gauge::heritageNeedle(make(Status::AboveRange,false,0.f,Status::Red));
    assert(n.visible && n.fraction==1.f && gauge::calibrationMaxC()>=gauge::kHeritageMaxC);
  }
  // Never outside 0..1 and never moving backwards as the temperature rises.
  { float last=-1.f;
    for(float t=-60.f;t<=220.f;t+=0.25f) {
      const NeedleState n=at(t); assert(n.visible && n.fraction>=0.f && n.fraction<=1.f && n.fraction>=last); last=n.fraction;
    } }
  // Hidden without a finite temperature, and on every fault, with or without a latched alarm; the same visibility as the
  // Needle dial and the same place as OEM's needle (the same 50-130 degC span).
  assert(!at(NAN).visible && !at(INFINITY).visible && !at(-INFINITY).visible);
  assert(!gauge::heritageNeedle(make(Status::Normal,false,80.f)).visible);
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Status a : kAlarms) for(bool has_t : {false,true}) for(float t : {30.f,100.f,125.f}) {
      const gauge::Reading r=make(s,has_t,t,a);
      const NeedleState n=gauge::heritageNeedle(r), o=gauge::oemNeedle(r);
      if(gauge::isFault(s)) assert(!n.visible);
      assert(n.visible==gauge::needleState(r,Unit::Celsius).visible);
      assert(n.visible==o.visible && (!n.visible || n.fraction==o.fraction));
    }
  }
  // The legend: only for NORMAL or OIL WARMING with a finite temperature, no alarm latched, not the demo build; exactly
  // when OEM shows CATERHAM.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Status a : kAlarms) for(bool has_t : {false,true}) for(float t : {25.f,80.f,NAN,INFINITY}) for(bool demo : {false,true}) {
      const gauge::Reading r=make(s,has_t,t,a);
      const bool quiet=!demo && a==Status::Normal && (s==Status::Normal || s==Status::Cold) && has_t && std::isfinite(t);
      assert(gauge::heritageLegend(r,demo)==quiet);
      assert(gauge::heritageLegend(r,demo)==gauge::oemWordmark(r,demo));
    }
  }
  assert(gauge::heritageLegend(make(Status::Normal,true,80.f),false));
  assert(!gauge::heritageLegend(make(Status::Normal,true,110.f,Status::Amber),false));  // still latched while cooling
  std::puts("Heritage model tests passed (demo)");
#else
  assert(!gauge::calibrationValid());
  // Uncalibrated: no needle, and never the legend (CALIBRATION REQUIRED shows), whatever arrives.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Status a : kAlarms) for(bool demo : {false,true}) {
      assert(!gauge::heritageNeedle(make(s,false,0.f,a)).visible);
      assert(!gauge::heritageNeedle(make(s,true,80.f,a)).visible);
      assert(!gauge::heritageLegend(make(s,true,80.f,a),demo));
      assert(!gauge::heritageLegend(make(s,false,0.f,a),demo));
    }
  }
  std::puts("Heritage model tests passed (uncalibrated)");
#endif
  return 0;
}
