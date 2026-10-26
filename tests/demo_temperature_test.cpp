#include "core/Calibration.h"
#include "core/Temperature.h"
#include "Config.h"
#include <cassert>
#include <cmath>
#include <cstdio>

using gauge::Status;

static gauge::Sample forOhms(float r) {
  const float amps=3.30f/(config::kPullUpOhms+r);
  return {true,amps*config::kPullUpOhms,amps*r,3.30f};
}
static gauge::Sample at(float t) {
  const float r = gauge::resistanceFromTemperature(t);
  assert(std::isfinite(r));
  return forOhms(r);
}
static const gauge::Sample kOpen = {true,0.0f,3.30f,3.30f};

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
  std::puts("Demo calibration, interpolation and alarm test passed");
}
