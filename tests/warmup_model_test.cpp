#include "core/Calibration.h"
#include "core/SweepModel.h"
#include "core/Temperature.h"
#include "core/WarmupModel.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

using gauge::NeedleState; using gauge::Status; using gauge::Unit;

static gauge::Reading make(Status s, bool has_t=false, float t_c=0, Status alarm=Status::Normal) {
  gauge::Reading r;
  r.status=s; r.alarm=alarm; r.has_temperature=has_t; r.temperature_c=t_c; r.resistance_ohms=123.4f;
  return r;
}
[[maybe_unused]] static bool near(float a, float b) { return std::fabs(a-b)<1e-5f; }
static bool says(const gauge::WarmupClock &c, const char *want) {
  char out[gauge::kWarmupTextSize]; gauge::warmupText(c, out, sizeof(out)); return std::strcmp(out, want) == 0;
}
static const Status kAlarms[]={Status::Normal,Status::Amber,Status::Red};

int main() {
  // The clock: from the first sample, each gap added, across the millisecond wrap, saturating.
  { gauge::WarmupClock c; assert(c.elapsedMs()==0 && !c.warm());
    c.update(make(Status::OpenCircuit), 0xFFFFFF00u); assert(c.elapsedMs()==0);
    for(int i=0;i<4;i++) c.update(make(Status::OpenCircuit), 0xFFFFFF00u + 250u*(i+1));  // crosses the wrap
    assert(c.elapsedMs()==1000 && !c.warm());
    c.update(make(Status::OpenCircuit), 0xFFFFFF00u + 1000u + 155000u);  // a long gap still counts: it's time since power-up
    assert(c.elapsedMs()==156000); }
  { gauge::WarmupClock c; uint32_t now=0;
    c.update(make(Status::OpenCircuit), now);
    for(int i=0;i<3;i++) c.update(make(Status::OpenCircuit), now+=0x7FFFFFFFu);
    assert(c.elapsedMs()==UINT32_MAX); }
  // Faults never count as warm or cold, in any build.
  { gauge::WarmupClock c; uint32_t now=1000;
    for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::NeedsCalibration);i++)
      for(Status a : kAlarms) c.update(make(static_cast<Status>(i),true,80.f,a), now+=250);
    assert(!c.warm() && !c.warmAtStart()); }

#ifdef GAUGE_DEMO
  assert(gauge::calibrationValid());
  // The ring: 0 at 0 degC, half at 30, full at the warm threshold (60) and above; empty below the table and below 0.
  { NeedleState n=gauge::warmupRing(make(Status::Cold,true,0.f)); assert(n.visible && n.fraction==0.f);
    n=gauge::warmupRing(make(Status::Cold,true,30.f)); assert(n.visible && near(n.fraction,0.5f));
    n=gauge::warmupRing(make(Status::Cold,true,45.f)); assert(n.visible && near(n.fraction,0.75f));
    n=gauge::warmupRing(make(Status::Normal,true,60.f)); assert(n.visible && n.fraction==1.f);
    n=gauge::warmupRing(make(Status::Amber,true,118.f,Status::Amber)); assert(n.visible && n.fraction==1.f);
    n=gauge::warmupRing(make(Status::Cold,true,-10.f)); assert(n.visible && n.fraction==0.f);
    n=gauge::warmupRing(make(Status::BelowRange)); assert(n.visible && n.fraction==0.f);
    n=gauge::warmupRing(make(Status::AboveRange,false,0.f,Status::Red)); assert(n.visible && n.fraction==1.f); }
  { float last=-1.f;
    for(float t=-40.f;t<=150.f;t+=0.25f) {
      const NeedleState n=gauge::warmupRing(make(Status::Normal,true,t)); assert(n.visible && n.fraction>=last && n.fraction<=1.f); last=n.fraction;
    } }
  // Hidden (not empty) on every fault and without a finite temperature: an empty ring would look like cold oil.
  assert(!gauge::warmupRing(make(Status::Cold,true,NAN)).visible && !gauge::warmupRing(make(Status::Normal,false,80.f)).visible);
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++)
    for(Status a : kAlarms) for(bool has_t : {false,true}) {
      const gauge::Reading r=make(static_cast<Status>(i),has_t,40.f,a);
      if(gauge::isFault(r.status)) assert(!gauge::warmupRing(r).visible);
      assert(gauge::warmupRing(r).visible==gauge::needleState(r,Unit::Celsius).visible);
    }

  // A warm-up from cold: the time since power-up while warming, then the time to the first warm reading, kept.
  { gauge::WarmupClock c; uint32_t now=5000;
    c.update(make(Status::Cold,true,15.f), now); assert(says(c, "0:00"));
    for(int i=0;i<620;i++) c.update(make(Status::Cold,true,15.f+i*0.07f), now+=250);  // 155 s
    assert(!c.warm() && says(c, "2:35"));
    for(int i=0;i<868;i++) c.update(make(Status::Cold,true,59.f), now+=250);  // to 372 s
    c.update(make(Status::Normal,true,60.f), now+=250);                        // 372.25 s: warm
    assert(c.warm() && !c.warmAtStart() && c.warmMs()==372250 && says(c, "WARM IN 6:12"));
    c.update(make(Status::Cold,true,55.f), now+=60000);  // cools again: still recorded
    c.update(make(Status::OpenCircuit), now+=250);       // a fault: still recorded
    c.update(make(Status::Normal,true,90.f), now+=250);
    assert(c.warmMs()==372250 && says(c, "WARM IN 6:12") && c.elapsedMs()==372250+60500); }
  // HIGH TEMPERATURE, CRITICAL TEMP and ABOVE CAL RANGE are warm too; OIL WARMING without a temperature is neither.
  for(Status s : {Status::Amber, Status::Red, Status::AboveRange}) {
    gauge::WarmupClock c; c.update(make(Status::Cold,true,30.f), 0); c.update(make(s,s!=Status::AboveRange,120.f,Status::Amber), 4000);
    assert(c.warm() && c.warmMs()==4000 && says(c, "WARM IN 0:04"));
  }
  { gauge::WarmupClock c; c.update(make(Status::Cold,false,30.f), 0); c.update(make(Status::Normal,true,NAN), 250);
    c.update(make(Status::Normal,false,80.f), 500); assert(!c.warm());
    c.update(make(Status::Normal,true,80.f), 750); assert(c.warm() && c.warmAtStart() && says(c, "WARM AT START")); }
  // Warm at start: the first reading with a temperature is already warm, even after faults; BELOW CAL RANGE first isn't.
  { gauge::WarmupClock c; c.update(make(Status::Normal,true,85.f), 100); assert(c.warmAtStart() && c.warmMs()==0 && says(c, "WARM AT START")); }
  { gauge::WarmupClock c; c.update(make(Status::OpenCircuit), 100); c.update(make(Status::OpenCircuit), 2100);
    c.update(make(Status::Normal,true,85.f), 2350); assert(c.warmAtStart() && c.warmMs()==2250 && says(c, "WARM AT START")); }
  { gauge::WarmupClock c; c.update(make(Status::BelowRange), 100); c.update(make(Status::Normal,true,61.f), 30100);
    assert(c.warm() && !c.warmAtStart() && says(c, "WARM IN 0:30")); }
  std::puts("Warm-up model tests passed (demo)");
#else
  assert(!gauge::calibrationValid());
  // Uncalibrated: no ring, never warm, and no text, whatever arrives.
  { gauge::WarmupClock c; uint32_t now=0;
    for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++)
      for(Status a : kAlarms) for(bool has_t : {false,true}) {
        const gauge::Reading r=make(static_cast<Status>(i),has_t,80.f,a);
        assert(!gauge::warmupRing(r).visible);
        c.update(r, now+=250);
      }
    assert(!c.warm() && says(c, "")); }
  std::puts("Warm-up model tests passed (uncalibrated)");
#endif
  return 0;
}
