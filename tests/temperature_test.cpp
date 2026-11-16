#include "core/Calibration.h"
#include "core/Temperature.h"
#include "Config.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

// ADS122C04 readings for a sender of the given resistance: the excitation current through
// Rref and the sender, the tank ground ground_offset volts above the gauge GND (read with the
// burn-out bias added), and an ADC offset added to every differential reading.
static gauge::Sample forOhms(float resistance, float ground_offset=0.0f, float offset=0.0f,
                             float amps=config::kExcitationA, float supply=3.3f) {
  const float ground=ground_offset+config::kGroundSenseBiasV;
  const float burn_out=10e-6f;  // also raises the sender node during the tap check
  return {true,offset,amps*config::kReferenceOhms+offset,amps*resistance+offset,
          ground,ground+(amps+burn_out)*resistance,supply};
}
static gauge::Status statusOf(const gauge::Sample &s) { return gauge::convertSample(s).status; }
int main() {
  using gauge::Status;
  auto v=gauge::convertSample(forOhms(120));
  assert(fabsf(v.resistance_ohms-120.f)<0.02f);
  assert(v.status==Status::NeedsCalibration); // default fail closed
  assert(!v.has_temperature);
  // Ground offset inside the window doesn't enter the resistance.
  v=gauge::convertSample(forOhms(120,0.15f));
  assert(fabsf(v.resistance_ohms-120.f)<0.02f && fabsf(v.ground_offset_volts-0.15f)<0.001f);
  assert(statusOf(forOhms(120,0.05f))==Status::NeedsCalibration);
  assert(statusOf(forOhms(120,-0.099f))==Status::NeedsCalibration);
  assert(statusOf(forOhms(120,-0.101f))==Status::GroundFault);
  assert(statusOf(forOhms(120,0.199f))==Status::NeedsCalibration);
  assert(statusOf(forOhms(120,0.201f))==Status::GroundFault);
  assert(statusOf(forOhms(120,-0.2f))==Status::GroundFault);
  assert(statusOf(forOhms(120,0.3f))==Status::GroundFault);
  // Broken ground-sense wire: the burn-out source pulls A2 to the top of the ADC's range.
  gauge::Sample broken=forOhms(120); broken.ground_volts=2.048f;
  assert(statusOf(broken)==Status::GroundFault);
  // Open A1 tap: the burn-out source pulls A1 to the top of the ADC's range; at the open
  // threshold an intact tap is still well inside the limit.
  gauge::Sample tap=forOhms(120); tap.tap_volts=2.048f;
  assert(statusOf(tap)==Status::OpenCircuit);
  tap=forOhms(120); tap.tap_volts+=config::kSenderTapMaxV-0.01f;
  assert(statusOf(tap)==Status::NeedsCalibration);
  tap=forOhms(120); tap.tap_volts+=config::kSenderTapMaxV+0.01f;
  assert(statusOf(tap)==Status::OpenCircuit);
  assert(statusOf(forOhms(4990,0.199f))==Status::NeedsCalibration);
  // The ADC offset is subtracted from Vref and Vs before the ratio...
  v=gauge::convertSample(forOhms(120,0,0.0002f));
  assert(v.status==Status::NeedsCalibration && fabsf(v.resistance_ohms-120.f)<0.02f);
  v=gauge::convertSample(forOhms(120,0,-0.0002f));
  assert(v.status==Status::NeedsCalibration && fabsf(v.resistance_ohms-120.f)<0.02f);
  // ...and an offset beyond the self-test limit means the ADC is suspect.
  assert(statusOf(forOhms(120,0,0.0003f))==Status::NoAdc);
  assert(statusOf(forOhms(120,0,-0.0003f))==Status::NoAdc);
  // Any failed or non-finite reading is NoAdc.
  gauge::Sample bad=forOhms(120); bad.adc_ok=false;
  assert(statusOf(bad)==Status::NoAdc);
  float gauge::Sample::*fields[]={&gauge::Sample::offset_volts,&gauge::Sample::reference_volts,
      &gauge::Sample::sender_volts,&gauge::Sample::ground_volts,&gauge::Sample::tap_volts,
      &gauge::Sample::supply_volts};
  const float non_finite[]={NAN,INFINITY,-INFINITY};
  for (auto f : fields) for (float x : non_finite) {
    gauge::Sample s=forOhms(120); s.*f=x;
    assert(statusOf(s)==Status::NoAdc);
  }
  // Supply window.
  assert(statusOf(forOhms(120,0,0,config::kExcitationA,2.3f))==Status::BadSupply);
  assert(statusOf(forOhms(120,0,0,config::kExcitationA,2.89f))==Status::BadSupply);
  assert(statusOf(forOhms(120,0,0,config::kExcitationA,2.91f))==Status::NeedsCalibration);
  assert(statusOf(forOhms(120,0,0,config::kExcitationA,3.49f))==Status::NeedsCalibration);
  assert(statusOf(forOhms(120,0,0,config::kExcitationA,3.51f))==Status::BadSupply);
  // Excitation window: the measured current must match the source's setting.
  assert(statusOf(forOhms(120,0,0,0.000215f))==Status::NeedsCalibration);
  assert(statusOf(forOhms(120,0,0,0.000285f))==Status::NeedsCalibration);
  assert(statusOf(forOhms(120,0,0,0.000205f))==Status::BadSupply);
  assert(statusOf(forOhms(120,0,0,0.000295f))==Status::BadSupply);
  // A 2.4 kOhm resistor fitted where config expects 2.0 kOhm: Vref is 20 % high.
  gauge::Sample wrong_rref=forOhms(120); wrong_rref.reference_volts*=1.2f;
  assert(statusOf(wrong_rref)==Status::BadSupply);
  // Short and open sender.
  gauge::Sample shorted=forOhms(120); shorted.sender_volts=0;
  assert(statusOf(shorted)==Status::ShortCircuit);
  assert(statusOf(forOhms(1))==Status::ShortCircuit);
  assert(statusOf(forOhms(19.9f))==Status::ShortCircuit);
  assert(statusOf(forOhms(20.1f))==Status::NeedsCalibration);
  // Open sender: the current stops, so Vref collapses.
  gauge::Sample open=forOhms(120); open.reference_volts=0; open.sender_volts=2.0f;
  assert(statusOf(open)==Status::OpenCircuit);
  open.reference_volts=0.0001f;
  assert(statusOf(open)==Status::OpenCircuit);
  assert(statusOf(forOhms(4990))==Status::NeedsCalibration);
  assert(statusOf(forOhms(5010))==Status::OpenCircuit);
  assert(statusOf(forOhms(100000))==Status::OpenCircuit);
  assert(gauge::isFault(Status::GroundFault));
  assert(!gauge::isFault(Status::AboveRange) && !gauge::isFault(Status::BelowRange));
  // Every Status needs its own text. Update this count and statusText() when adding a Status.
  assert(static_cast<int>(Status::Red)==11);
  for(int i=0;i<=static_cast<int>(Status::Red);i++) {
    const char* a=gauge::statusText(static_cast<Status>(i));
    assert(a && a[0] && std::strcmp(a,"UNKNOWN")!=0);
    for(int j=0;j<i;j++) assert(std::strcmp(a,gauge::statusText(static_cast<Status>(j)))!=0);
  }
  std::puts("Temperature conversion and fault tests passed");
}
