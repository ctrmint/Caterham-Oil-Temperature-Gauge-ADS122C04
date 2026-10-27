#include "core/Calibration.h"
#include "core/Temperature.h"
#include "Config.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

// ADS1115 readings for a sender of the given resistance, with the tank ground sitting
// ground_offset volts above the gauge GND.
static gauge::Sample forOhms(float resistance, float ground_offset=0.0f) {
  const float amps=(3.3f-ground_offset)/(config::kPullUpOhms+resistance);
  return {true,amps*config::kPullUpOhms,amps*resistance,3.3f};
}
int main() {
  auto v=gauge::convertSample(forOhms(120));
  assert(fabsf(v.resistance_ohms-120.f)<0.02f);
  assert(v.status==gauge::Status::NeedsCalibration); // default fail closed
  assert(!v.has_temperature);
  // Ground offset inside the window cancels out of the resistance.
  v=gauge::convertSample(forOhms(120,0.15f));
  assert(fabsf(v.resistance_ohms-120.f)<0.02f && fabsf(v.ground_offset_volts-0.15f)<0.001f);
  assert(gauge::convertSample(forOhms(120,0.05f)).status==gauge::Status::NeedsCalibration);
  assert(gauge::convertSample(forOhms(120,-0.2f)).status==gauge::Status::GroundFault);
  assert(gauge::convertSample(forOhms(120,0.3f)).status==gauge::Status::GroundFault);
  // Broken ground-sense wire: 100k bias pulls A1 to 3V3, so Vp + Vs collapses.
  assert(gauge::convertSample({true,0.02f,-0.02f,3.3f}).status==gauge::Status::GroundFault);
  assert(gauge::convertSample({false,1,1,3.3}).status==gauge::Status::NoAdc);
  assert(gauge::convertSample({true,NAN,1,3.3}).status==gauge::Status::NoAdc);
  assert(gauge::convertSample({true,1,1,2.3}).status==gauge::Status::BadSupply);
  assert(gauge::convertSample({true,3.3,0,3.3}).status==gauge::Status::ShortCircuit);
  assert(gauge::convertSample(forOhms(1)).status==gauge::Status::ShortCircuit);
  assert(gauge::convertSample({true,0,3.3,3.3}).status==gauge::Status::OpenCircuit);
  assert(gauge::convertSample(forOhms(100000)).status==gauge::Status::OpenCircuit);
  assert(gauge::isFault(gauge::Status::GroundFault));
  assert(!gauge::isFault(gauge::Status::AboveRange) && !gauge::isFault(gauge::Status::BelowRange));
  // Every Status needs its own text. Update this count and statusText() when adding a Status.
  assert(static_cast<int>(gauge::Status::Red)==11);
  for(int i=0;i<=static_cast<int>(gauge::Status::Red);i++) {
    const char* a=gauge::statusText(static_cast<gauge::Status>(i));
    assert(a && a[0] && std::strcmp(a,"UNKNOWN")!=0);
    for(int j=0;j<i;j++) assert(std::strcmp(a,gauge::statusText(static_cast<gauge::Status>(j)))!=0);
  }
  std::puts("Temperature conversion and fault tests passed");
}
