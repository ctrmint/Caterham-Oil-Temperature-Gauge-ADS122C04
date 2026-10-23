#include "core/Calibration.h"
#include "core/Temperature.h"
#include "Config.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static gauge::Sample forOhms(float resistance) {
  return {true, 3.3f*resistance/(config::kPullUpOhms+resistance),3.3f};
}
int main() {
  auto v=gauge::convertSample(forOhms(120));
  assert(fabsf(v.resistance_ohms-120.f)<0.02f);
  assert(v.status==gauge::Status::NeedsCalibration); // default fail closed
  assert(!v.has_temperature);
  assert(gauge::convertSample({false,1,3.3}).status==gauge::Status::NoAdc);
  assert(gauge::convertSample({true,0,3.3}).status==gauge::Status::ShortCircuit);
  assert(gauge::convertSample({true,3.29,3.3}).status==gauge::Status::OpenCircuit);
  assert(gauge::convertSample({true,1,2.3}).status==gauge::Status::BadSupply);
  assert(gauge::convertSample(forOhms(1)).status==gauge::Status::ShortCircuit);
  assert(gauge::convertSample(forOhms(100000)).status==gauge::Status::OpenCircuit);
  std::puts("Temperature conversion and fault tests passed");
}
