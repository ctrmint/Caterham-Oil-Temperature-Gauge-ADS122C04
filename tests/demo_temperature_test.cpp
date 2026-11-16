#include "core/Calibration.h"
#include "core/Temperature.h"
#include "Config.h"
#include <cassert>
#include <cstdint>
#include <cmath>
#include <cstdio>

using gauge::Status;

static gauge::Sample forOhms(float r) {
  const float amps=config::kExcitationA;
  return {true,0.0f,amps*config::kReferenceOhms,amps*r,config::kGroundSenseBiasV,
          config::kGroundSenseBiasV+(amps+10e-6f)*r,3.30f};
}
static gauge::Sample at(float t) {
  const float r = gauge::resistanceFromTemperature(t);
  assert(std::isfinite(r));
  return forOhms(r);
}
// Open sender: no current flows, so there is no voltage across Rref.
static const gauge::Sample kOpen = {true,0.0f,0.0f,2.0f,config::kGroundSenseBiasV,2.048f,3.30f};

// Feed the same sample every 250 ms for duration_ms; returns the last reading.
static gauge::Reading hold(gauge::TemperatureState &s, const gauge::Sample &x, unsigned &ms, unsigned duration_ms) {
  gauge::Reading r;
  for (unsigned end=ms+duration_ms; ms<=end; ms+=250) r=s.update(x,ms);
  return r;
}

int main() {
  assert(gauge::calibrationValid());
  assert(gauge::calibrationMinC()==20.f && gauge::calibrationMaxC()==140.f);
  float t=0;
  assert(gauge::temperatureFromResistance(120.f,t));
  assert(std::fabs(t-100.f)<0.01f);
  assert(!gauge::temperatureFromResistance(1500.f,t)); // no extrapolation
  assert(gauge::calibrationSide(1500.f)<0 && gauge::calibrationSide(40.f)>0 && gauge::calibrationSide(120.f)==0);

  {
    gauge::TemperatureState sample;
    auto warm=sample.update(at(70),0);
    assert(warm.has_temperature && warm.status==Status::Normal);
    auto hot=sample.update(at(139),5000);
    // EMA prevents a sudden step from instantly triggering a red alarm.
    assert(hot.has_temperature && hot.status != Status::Red);
  }
  {
    // Sustained critical temperature latches Red after the debounce.
    gauge::TemperatureState s; unsigned ms=0;
    auto r=hold(s,at(139),ms,4000);
    assert(r.status==Status::Red && r.alarm==Status::Red);
    assert(s.maximumC()>135.f);
    // Hotter than the table: Red must stay, not drop to a sensor fault.
    r=hold(s,forOhms(40.f),ms,5000);
    assert(r.status==Status::AboveRange && r.alarm==Status::Red && !r.has_temperature);
    assert(s.maximumAboveRange());
  }
  {
    // Above the table from cold start: debounced to Red like any critical reading.
    gauge::TemperatureState s; unsigned ms=0;
    auto r=s.update(forOhms(40.f),ms);
    assert(r.status==Status::AboveRange && r.alarm==Status::Normal);
    r=hold(s,forOhms(40.f),ms,3000);
    assert(r.alarm==Status::Red);
  }
  {
    // Colder than the table: not a fault, no alarm.
    gauge::TemperatureState s; unsigned ms=0;
    auto r=hold(s,forOhms(1500.f),ms,1000);
    assert(r.status==Status::BelowRange && r.alarm==Status::Normal && !gauge::isFault(r.status));
  }
  {
    // An intermittent connector (every other sample open) must not block the alarm.
    gauge::TemperatureState s; gauge::Reading r;
    for (unsigned ms=0; ms<=6000; ms+=250) r=s.update((ms/250)%2 ? kOpen : at(139),ms);
    assert(r.alarm==Status::Red);
    // A brief fault keeps the latched alarm visible...
    r=s.update(kOpen,6250);
    assert(r.status==Status::OpenCircuit && r.alarm==Status::Red);
    // ...but a sustained fault resets it.
    unsigned ms=6500;
    r=hold(s,kOpen,ms,config::kFaultResetMs);
    assert(r.status==Status::OpenCircuit && r.alarm==Status::Normal);
  }
  {
    // Cooling from Red steps down through Amber (hysteresis), then clears.
    gauge::TemperatureState s; unsigned ms=0;
    auto r=hold(s,at(139),ms,4000);
    assert(r.alarm==Status::Red);
    r=hold(s,at(127),ms,10000);   // inside Red hysteresis band (125-130)
    assert(r.alarm==Status::Red);
    r=hold(s,at(112),ms,10000);   // below Red band, inside Amber band (110-115)
    assert(r.status==Status::Amber && r.alarm==Status::Amber);
    r=hold(s,at(105),ms,10000);
    assert(r.status==Status::Normal && r.alarm==Status::Normal);
  }
  {
    // A single filtered step from the Red band straight into the Amber band must land on
    // Amber, not skip to Normal.
    gauge::TemperatureState s; unsigned ms=0;
    auto r=hold(s,at(139),ms,4000);
    r=hold(s,at(127),ms,10000);
    assert(r.alarm==Status::Red);
    r=s.update(forOhms(204.f),ms);
    assert(r.has_temperature && r.temperature_c>110.f && r.temperature_c<115.f);
    assert(r.status==Status::Amber && r.alarm==Status::Amber);
  }
  {
    // AC-1: Amber escalates exactly at the debounce time, not before.
    gauge::TemperatureState s;
    for (unsigned ms=0; ms<=5000; ms+=250) {
      auto r=s.update(at(120),ms);
      if (ms<config::kAlarmDebounceMs) assert(r.alarm==Status::Normal);
      else assert(r.alarm==Status::Amber && r.status==Status::Amber);
    }
  }
  {
    // AC-2: Red escalates exactly at the debounce time, not before.
    gauge::TemperatureState s;
    for (unsigned ms=0; ms<=5000; ms+=250) {
      auto r=s.update(at(139),ms);
      if (ms<config::kAlarmDebounceMs) assert(r.alarm==Status::Normal);
      else assert(r.alarm==Status::Red);
    }
  }
  {
    // AC-3: with Amber latched, the Red timer starts only once the reported temperature
    // reaches the Red threshold, so Red follows t0 by the debounce time.
    gauge::TemperatureState s; unsigned ms=0;
    auto r=hold(s,at(120),ms,4000);
    assert(r.alarm==Status::Amber);
    bool found=false; unsigned t0=0;
    for (; ms<=20000; ms+=250) {
      r=s.update(at(139),ms);
      if (!found && r.has_temperature && r.temperature_c>=config::kRedThresholdC) {found=true;t0=ms;}
      if (!found) {assert(r.alarm==Status::Amber);continue;}
      if (ms-t0<config::kAlarmDebounceMs) assert(r.alarm==Status::Amber);
      else assert(r.alarm==Status::Red);
      if (ms-t0>=4000) break;
    }
    assert(found && ms-t0>=4000);
  }
  {
    // AC-4: debounce and fault reset are unchanged across uint32_t ms wraparound.
    // hold() cannot be used here: its end value wraps and the loop would not run.
    gauge::TemperatureState s;
    const uint32_t start=0xFFFFF000u;
    uint32_t ms=start;
    gauge::Reading r;
    for (int i=0; i<12; i++, ms+=250) { // start .. start+2750
      r=s.update(at(139),ms);
      if (ms-start<2500) assert(r.alarm==Status::Normal);
      else assert(r.alarm==Status::Red);
    }
    const uint32_t fault_start=ms; // start+3000
    assert(fault_start==start+3000u);
    assert(fault_start > fault_start+config::kFaultResetMs); // window crosses the wrap
    for (int i=0; i<=12; i++, ms+=250) { // fault_start .. fault_start+3000
      r=s.update(kOpen,ms);
      assert(r.status==Status::OpenCircuit);
      if (ms-fault_start<config::kFaultResetMs) assert(r.alarm==Status::Red);
      else assert(r.alarm==Status::Normal);
    }
  }
  {
    // AC-4 (b): the debounce interval itself spans the wrap.
    gauge::TemperatureState s;
    const uint32_t start=0xFFFFFC00u;
    assert(start+config::kAlarmDebounceMs<start);
    uint32_t ms=start;
    for (int i=0; i<=20; i++, ms+=250) { // elapsed 0 .. 5000
      auto r=s.update(at(139),ms);
      if (ms-start<config::kAlarmDebounceMs) assert(r.alarm==Status::Normal);
      else assert(r.alarm==Status::Red);
    }
  }
  {
    // AC-5: a fault shorter than kFaultResetMs keeps the latched Red alarm and the filter.
    gauge::TemperatureState s; unsigned ms=0;
    hold(s,at(139),ms,4000);
    const unsigned fault_start=ms;
    gauge::Reading r;
    for (; ms-fault_start<=config::kFaultResetMs-250; ms+=250) {
      r=s.update(kOpen,ms);
      assert(r.status==Status::OpenCircuit && r.alarm==Status::Red);
    }
    assert(ms==fault_start+config::kFaultResetMs);
    r=s.update(at(120),ms);
    assert(r.has_temperature);
    // Kept filter reads ~135.2; a re-seeded one would read 120.
    assert(r.temperature_c>config::kRedThresholdC && r.temperature_c<139.f);
    assert(r.alarm==Status::Red && r.status==Status::Red);
  }
  {
    // AC-6: below kWarmThresholdC the status is Cold, which is not a fault.
    gauge::TemperatureState s; unsigned ms=0;
    auto r=hold(s,at(30),ms,2000);
    assert(r.status==Status::Cold && r.has_temperature);
    assert(r.alarm==Status::Normal && !gauge::isFault(r.status));
  }
  {
    // AC-7: table endpoints are inclusive and on the table.
    float t=0;
    assert(gauge::temperatureFromResistance(1100.f,t) && std::fabs(t-20.f)<0.01f);
    assert(gauge::temperatureFromResistance(47.f,t) && std::fabs(t-140.f)<0.01f);
    assert(gauge::calibrationSide(1100.f)==0 && gauge::calibrationSide(47.f)==0);
  }
  {
    // AC-8: maximumC() rises with the temperature, never decreases, and equals the
    // highest reported temperature.
    gauge::TemperatureState s; unsigned ms=0;
    float prev=s.maximumC(), highest=-999.f;
    gauge::Reading r;
    auto phase=[&](float temp, unsigned duration_ms) {
      for (unsigned end=ms+duration_ms; ms<=end; ms+=250) {
        r=s.update(at(temp),ms);
        assert(s.maximumC()>=prev); prev=s.maximumC();
        if (r.has_temperature && r.temperature_c>highest) highest=r.temperature_c;
      }
    };
    phase(70,3000);
    const float max_at_70=s.maximumC();
    phase(139,10000);
    assert(s.maximumC()>max_at_70+1.f);
    phase(105,10000);
    assert(std::fabs(s.maximumC()-highest)<0.01f);
    assert(r.has_temperature && r.temperature_c<s.maximumC());
  }
  {
    // CR-2: below-range hold while an alarm is latched (REQ-T3).
    const gauge::Sample kCold = forOhms(1500.f);
    const uint32_t kHold = config::kBelowRangeHoldMs;
    // Cold samples inside the hold window: alarm kept, BelowRange, no temperature, maximum untouched.
    auto expectHeld = [&](gauge::TemperatureState &s, uint32_t &ms, uint32_t c0, Status alarm, float max_c, bool above) {
      while (uint32_t(ms-c0)<kHold) {
        auto r=s.update(kCold,ms);
        assert(r.alarm==alarm);
        assert(r.status==Status::BelowRange);
        assert(!r.has_temperature && !gauge::isFault(r.status));
        assert(s.maximumC()==max_c && s.maximumAboveRange()==above);
        ms+=250;
      }
    };
    // From the first update at or after the window: reset, BelowRange, Normal, for extra_ms more.
    auto expectReset = [&](gauge::TemperatureState &s, uint32_t &ms, uint32_t extra_ms) {
      for (uint32_t n=0; n<=extra_ms/250; ++n, ms+=250) {
        auto r=s.update(kCold,ms);
        assert(r.alarm==Status::Normal && r.status==Status::BelowRange && !r.has_temperature);
      }
    };
    {
      // AC-5: a real cool-down still steps Red -> Amber -> Normal.
      gauge::TemperatureState s; unsigned ms=0;
      auto r=hold(s,at(139),ms,4000);
      assert(r.alarm==Status::Red);
      Status prev=r.alarm; int changes=0;
      auto track=[&](const gauge::Reading &x) {
        if (x.alarm!=prev) {
          ++changes;
          assert((prev==Status::Red && x.alarm==Status::Amber) || (prev==Status::Amber && x.alarm==Status::Normal));
          prev=x.alarm;
        }
      };
      for (int i=0; i<=136; ++i, ms+=250) { r=s.update(at(139.f-0.25f*i),ms); track(r); }
      for (unsigned end=ms+2000; ms<=end; ms+=250) { r=s.update(at(105),ms); track(r); }
      assert(changes==2 && r.alarm==Status::Normal);
    }
    {
      // AC-6: cold with no alarm latched is unchanged.
      gauge::TemperatureState s; unsigned ms=0;
      for (unsigned end=kHold+2000; ms<=end; ms+=250) {
        auto r=s.update(kCold,ms);
        assert(r.status==Status::BelowRange && r.alarm==Status::Normal && !r.has_temperature);
      }
      // The cold samples went into the filter: one 70 C sample must not read as 70 C.
      gauge::TemperatureState f; unsigned m=0;
      hold(f,kCold,m,1000);
      auto r=f.update(at(70),m);
      assert(r.status==Status::BelowRange && !r.has_temperature);
    }
    {
      // AC-4: after the hold the state resets and stays reset.
      gauge::TemperatureState s; unsigned ms=0;
      assert(hold(s,at(139),ms,4000).alarm==Status::Red);
      uint32_t m=ms, c0=ms;
      while (uint32_t(m-c0)<kHold) s.update(kCold,m), m+=250;
      expectReset(s,m,5000);
    }
    {
      // AC-1: Red is held for the whole window.
      gauge::TemperatureState s; unsigned ms=0;
      assert(hold(s,at(139),ms,4000).alarm==Status::Red);
      uint32_t m=ms, c0=ms;
      const float max_c=s.maximumC(); const bool above=s.maximumAboveRange();
      expectHeld(s,m,c0,Status::Red,max_c,above);
    }
    {
      // AC-2: Amber is held for the whole window.
      gauge::TemperatureState s; unsigned ms=0;
      assert(hold(s,at(120),ms,4000).alarm==Status::Amber);
      uint32_t m=ms, c0=ms;
      const float max_c=s.maximumC(); const bool above=s.maximumAboveRange();
      expectHeld(s,m,c0,Status::Amber,max_c,above);
    }
    {
      // AC-3: held cold samples stay out of the filter; a hot sample at the end is read at once.
      gauge::TemperatureState s; unsigned ms=0;
      assert(hold(s,at(139),ms,4000).alarm==Status::Red);
      uint32_t m=ms, c0=ms;
      for (; uint32_t(m-c0)<=kHold-250; m+=250) s.update(kCold,m);
      assert(m-c0==kHold);
      auto r=s.update(at(139),m);
      assert(r.has_temperature && std::fabs(r.temperature_c-139.f)<=0.5f);
      assert(r.alarm==Status::Red && r.status==Status::Red);
    }
    {
      // AC-7: the window crosses the uint32_t millisecond wrap.
      gauge::TemperatureState s; uint32_t ms=0xFFFFE000u;
      gauge::Reading r;
      for (int i=0; i<17; ++i, ms+=250) r=s.update(at(139),ms);
      assert(r.alarm==Status::Red);
      const uint32_t c0=ms;
      assert(uint32_t(c0+kHold)<c0);
      const float max_c=s.maximumC(); const bool above=s.maximumAboveRange();
      expectHeld(s,ms,c0,Status::Red,max_c,above);
      expectReset(s,ms,2000);
    }
    {
      // AC-9: alternating cold and fault samples cannot hold the alarm for ever.
      gauge::TemperatureState s; unsigned ms=0;
      assert(hold(s,at(139),ms,4000).alarm==Status::Red);
      uint32_t m=ms, c0=ms;
      for (bool cold=true; ; cold=!cold, m+=250) {
        auto r=s.update(cold ? kCold : kOpen,m);
        if (uint32_t(m-c0)<kHold) { assert(r.alarm==Status::Red); continue; }
        if (cold) { assert(r.alarm==Status::Normal && r.status==Status::BelowRange); break; }
      }
    }
    {
      // CR-2 AC-1 re-arm: an in-table sample ends the cold episode, so a later one gets a fresh window.
      gauge::TemperatureState s; unsigned ms=0;
      assert(hold(s,at(139),ms,4000).alarm==Status::Red);
      uint32_t m=ms;
      for (uint32_t end=m+1000; m<=end; m+=250) assert(s.update(kCold,m).alarm==Status::Red);
      for (uint32_t end=m+kHold+1000; m<=end; m+=250) assert(s.update(at(139),m).alarm==Status::Red);
      const uint32_t c0=m;
      const float max_c=s.maximumC(); const bool above=s.maximumAboveRange();
      expectHeld(s,m,c0,Status::Red,max_c,above);
    }
  }
  std::puts("Demo calibration, interpolation and alarm test passed");
}
