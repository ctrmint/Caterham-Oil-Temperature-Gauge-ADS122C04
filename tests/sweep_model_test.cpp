#include "core/Calibration.h"
#include "core/SweepModel.h"
#include "core/Temperature.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static gauge::Reading make(gauge::Status s, bool has_t=false, float t_c=0) {
  gauge::Reading r;
  r.status=s; r.resistance_ohms=123.4f; r.has_temperature=has_t; r.temperature_c=t_c;
  return r;
}
static int lit(float t_c, gauge::Unit u) { return gauge::litSegments(make(gauge::Status::Normal,true,t_c),u); }

int main() {
  using gauge::Status; using gauge::Unit;
  const Unit units[]={Unit::Celsius,Unit::Fahrenheit};
#ifdef GAUGE_DEMO
  assert(gauge::calibrationValid());  // build guard: never pass vacuously
#else
  assert(!gauge::calibrationValid());
#endif

  // Scale.
  assert(gauge::kSweepSegments==40);
  assert(gauge::sweepScale(Unit::Celsius).min==40.f && gauge::sweepScale(Unit::Celsius).max==150.f);
  assert(gauge::sweepScale(Unit::Fahrenheit).min==100.f && gauge::sweepScale(Unit::Fahrenheit).max==300.f);
  assert(gauge::scaleLabelCount(Unit::Celsius)==6);
  assert(gauge::scaleLabelCount(Unit::Fahrenheit)==11);
  for(int i=0;i<6;i++) assert(gauge::scaleLabel(Unit::Celsius,i)==40+20*i);
  for(int i=0;i<11;i++) assert(gauge::scaleLabel(Unit::Fahrenheit,i)==100+20*i);

  // Seven-segment table: bits 0-6 are segments a-g.
  const char digits[]="0123456789- ";
  const uint8_t masks[]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,0x40,0x00};
  for(int i=0;i<12;i++) assert(gauge::sevenSegmentMask(digits[i])==masks[i]);
  assert(gauge::sevenSegmentMask('<')==0 && gauge::sevenSegmentMask('>')==0);
  assert(gauge::sevenSegmentMask('A')==0 && gauge::sevenSegmentMask('\0')==0);

  // Nothing lit without a temperature, in any build.
  for(Unit u:units) {
    assert(gauge::litSegments(make(Status::BelowRange),u)==0);
    gauge::Reading below=make(Status::BelowRange); below.alarm=Status::Red;
    assert(gauge::litSegments(below,u)==0);
    for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::NeedsCalibration);i++)
      assert(gauge::litSegments(make(static_cast<Status>(i)),u)==0);
    assert(gauge::litSegments(make(Status::Normal,true,NAN),u)==0);
    assert(gauge::litSegments(make(Status::Normal,false,100.f),u)==0);  // stale temperature, no flag
  }

#ifdef GAUGE_DEMO
  assert(gauge::scaleLabelsVisible());
  // Lit count: floor((T-min)/(max-min)*40), clamped.
  assert(lit(40.f,Unit::Celsius)==0);
  assert(lit(150.f,Unit::Celsius)==40);
  assert(lit(67.5f,Unit::Celsius)==10);   // exact boundary: 27.5/110 = 0.25
  assert(lit(67.49f,Unit::Celsius)==9);
  assert(lit(67.51f,Unit::Celsius)==10);
  assert(lit(30.f,Unit::Celsius)==0);
  assert(lit(160.f,Unit::Celsius)==40);
  assert(lit(60.f,Unit::Fahrenheit)==8);  // 60 degC = 140 degF, exact boundary
  assert(lit(59.99f,Unit::Fahrenheit)==7);
  assert(lit(60.01f,Unit::Fahrenheit)==8);
  assert(lit(37.f,Unit::Fahrenheit)==0);
  assert(lit(150.f,Unit::Fahrenheit)==40);
  // Above range lights only up to the table maximum, never the full scale.
  for(Unit u:units) assert(gauge::litSegments(make(Status::AboveRange),u)==36);
  std::puts("Sweep model tests passed (demo)");
#else
  assert(!gauge::scaleLabelsVisible());
  for(Unit u:units) {
    for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
      assert(gauge::litSegments(make(static_cast<Status>(i)),u)==0);
      assert(gauge::litSegments(make(static_cast<Status>(i),true,100.f),u)==0);
    }
    assert(lit(100.f,u)==0);
    assert(lit(150.f,u)==0);
  }
  std::puts("Sweep model tests passed (uncalibrated)");
#endif
}
