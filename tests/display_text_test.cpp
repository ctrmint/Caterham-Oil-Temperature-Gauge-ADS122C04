#include "core/Calibration.h"
#include "core/DisplayText.h"
#include "core/Temperature.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

static const char* kDeg="\xC2\xB0";

static gauge::Reading make(gauge::Status s, float ohms=123.4f, bool has_t=false, float t_c=0) {
  gauge::Reading r;
  r.status=s; r.resistance_ohms=ohms; r.has_temperature=has_t; r.temperature_c=t_c;
  return r;
}
static bool eq(const char* a,const char* b) { return std::strcmp(a,b)==0; }
static bool reads(const gauge::Reading &r, gauge::Unit u, const char* want) {
  char buf[gauge::kReadingTextSize];
  gauge::readingText(r,u,buf,sizeof buf);
  return eq(buf,want);
}
static bool foots(const gauge::Reading &r, float max_c, bool above, gauge::Unit u, const char* want) {
  char buf[gauge::kFooterTextSize];
  gauge::footerText(r,max_c,above,u,buf,sizeof buf);
  return eq(buf,want);
}
static bool cells(const char* in, const char* want) {
  char buf[gauge::kReadingCells+1];
  std::memset(buf,'x',sizeof buf);
  gauge::readingCells(in,buf,sizeof buf);
  return buf[gauge::kReadingCells]=='\0' && std::strlen(buf)==gauge::kReadingCells && eq(buf,want);
}

int main() {
  using gauge::Status; using gauge::Unit;
  const Unit units[]={Unit::Celsius,Unit::Fahrenheit};
#ifdef GAUGE_DEMO
  assert(gauge::calibrationValid());  // build guard: never pass vacuously
#else
  assert(!gauge::calibrationValid());
#endif

  // Rounding: nearest, halves away from zero.
  long n=0;
  assert(gauge::roundHalfAway(112.4f,n) && n==112);
  assert(gauge::roundHalfAway(112.5f,n) && n==113);
  assert(gauge::roundHalfAway(-0.5f,n) && n==-1);
  assert(gauge::roundHalfAway(-0.4f,n) && n==0);
  assert(gauge::roundHalfAway(4999.6f,n) && n==5000);
  assert(!gauge::roundHalfAway(NAN,n));
  assert(!gauge::roundHalfAway(INFINITY,n));
  assert(!gauge::roundHalfAway(-INFINITY,n));
  assert(!gauge::roundHalfAway(1e6f,n) && !gauge::roundHalfAway(-1e6f,n));
  assert(gauge::roundHalfAway(999999.f,n) && n==999999);

  // Conversion.
  assert(gauge::toUnit(100.f,Unit::Celsius)==100.f);
  assert(std::fabs(gauge::toUnit(100.f,Unit::Fahrenheit)-212.f)<0.001f);
  assert(std::fabs(gauge::toUnit(-40.f,Unit::Fahrenheit)+40.f)<0.001f);

  // Unit label and title.
  for(Unit u:units) assert(eq(gauge::titleText(u),(std::string("OIL ")+gauge::unitLabel(u)).c_str()));
#ifdef GAUGE_DEMO
  assert(eq(gauge::unitLabel(Unit::Celsius),"\xC2\xB0""C") && eq(gauge::titleText(Unit::Celsius),"OIL \xC2\xB0""C"));
  assert(eq(gauge::unitLabel(Unit::Fahrenheit),"\xC2\xB0""F") && eq(gauge::titleText(Unit::Fahrenheit),"OIL \xC2\xB0""F"));
#else
  for(Unit u:units) assert(eq(gauge::unitLabel(u),"ohm") && eq(gauge::titleText(u),"OIL ohm"));
#endif

#ifdef GAUGE_DEMO
  // Reading: values, with the conversion before the rounding.
  assert(reads(make(Status::Normal,123.4f,true,112.4f),Unit::Celsius,"112"));
  assert(reads(make(Status::Normal,123.4f,true,112.5f),Unit::Celsius,"113"));
  assert(reads(make(Status::Normal,123.4f,true,-0.5f),Unit::Celsius,"-1"));
  assert(reads(make(Status::Normal,123.4f,true,115.0f),Unit::Fahrenheit,"239"));
  assert(reads(make(Status::Normal,123.4f,true,130.0f),Unit::Fahrenheit,"266"));
  assert(reads(make(Status::Normal,123.4f,true,114.7f),Unit::Fahrenheit,"238"));
  assert(reads(make(Status::AboveRange),Unit::Celsius,">140"));
  assert(reads(make(Status::AboveRange),Unit::Fahrenheit,">284"));
  assert(reads(make(Status::BelowRange),Unit::Celsius,"<20"));
  assert(reads(make(Status::BelowRange),Unit::Fahrenheit,"<68"));
  for(Unit u:units) {
    assert(reads(make(Status::NeedsCalibration,4999.6f),u,"5000"));
    assert(reads(make(Status::Normal,123.4f,true,NAN),u,"--"));
    for(Status s:{Status::NoAdc,Status::BadSupply,Status::GroundFault,Status::ShortCircuit,Status::OpenCircuit})
      assert(reads(make(s),u,"--"));
  }
#else
  for(Unit u:units) {
    assert(reads(make(Status::NeedsCalibration,4999.6f),u,"5000"));
    assert(reads(make(Status::NeedsCalibration,123.4f),u,"123"));
    assert(reads(make(Status::Normal,123.4f,true,112.4f),u,"--"));
    assert(reads(make(Status::AboveRange),u,"--"));
    assert(reads(make(Status::BelowRange),u,"--"));
  }
#endif

  // Ohms: an exact half rounds away from zero; bad resistances show nothing (both builds).
  for(Unit u:units) {
    assert(reads(make(Status::NeedsCalibration,122.5f),u,"123"));
    assert(reads(make(Status::NeedsCalibration,NAN),u,"--"));
    assert(reads(make(Status::NeedsCalibration,INFINITY),u,"--"));
    assert(reads(make(Status::NeedsCalibration,0.f),u,"--"));
    assert(reads(make(Status::NeedsCalibration,-5.f),u,"--"));
  }

  // No unit in the reading, for every Status; no degree sign anywhere when uncalibrated.
  for(int i=0;i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Unit u:units) {
      for(bool has_t:{false,true}) {
        char buf[gauge::kReadingTextSize];
        gauge::readingText(make(s,123.4f,has_t,112.4f),u,buf,sizeof buf);
        assert(!std::strstr(buf,kDeg) && !std::strchr(buf,'C') && !std::strchr(buf,'F') && !std::strstr(buf,"ohm"));
#ifndef GAUGE_DEMO
        char foot[gauge::kFooterTextSize];
        gauge::footerText(make(s,123.4f,has_t,112.4f),112.4f,false,u,foot,sizeof foot);
        assert(!std::strstr(buf,kDeg) && !std::strstr(foot,kDeg) && !std::strstr(foot,"MAX"));
        assert(!std::strstr(gauge::unitLabel(u),kDeg) && !std::strstr(gauge::titleText(u),kDeg));
#endif
      }
    }
  }

  // Cells: fixed width, right-aligned.
  assert(cells("9","   9") && cells("99","  99") && cells("100"," 100") && cells("188"," 188"));
  assert(cells("--","  --") && cells(">140",">140") && cells("4999","4999"));
  assert(cells("12345","  --") && cells("<1234","  --"));

#ifdef GAUGE_DEMO
  // MAX line.
  const gauge::Reading norm=make(Status::Normal,123.4f,true,112.4f);
  assert(foots(norm,112.4f,false,Unit::Celsius,"MAX 112  |  123 ohm"));
  assert(foots(norm,112.5f,false,Unit::Fahrenheit,"MAX 235  |  123 ohm"));
  assert(foots(norm,114.7f,false,Unit::Fahrenheit,"MAX 238  |  123 ohm"));
  assert(foots(make(Status::Normal,122.5f,true,112.4f),112.4f,false,Unit::Celsius,"MAX 112  |  123 ohm"));
  assert(foots(make(Status::Normal,122.5f),-999.f,false,Unit::Celsius,"123 ohm"));
  for(float m:{112.4f,-999.f}) {
    assert(foots(norm,m,true,Unit::Celsius,"MAX >140  |  123 ohm"));
    assert(foots(norm,m,true,Unit::Fahrenheit,"MAX >284  |  123 ohm"));
  }
  assert(foots(norm,-999.f,false,Unit::Celsius,"123 ohm"));
  assert(foots(make(Status::BelowRange,123.4f),112.4f,false,Unit::Celsius,"MAX 112  |  123 ohm"));
  for(Unit u:units) {
    assert(foots(make(Status::NeedsCalibration),112.4f,false,u,"TEMPERATURE NOT VERIFIED"));
    for(Status s:{Status::NoAdc,Status::BadSupply,Status::GroundFault,Status::ShortCircuit,Status::OpenCircuit})
      assert(foots(make(s),112.4f,false,u,"CHECK SENSOR AND WIRING"));
    for(int i=0;i<=static_cast<int>(Status::Red);i++) {
      char foot[gauge::kFooterTextSize];
      gauge::footerText(make(static_cast<Status>(i)),112.4f,false,u,foot,sizeof foot);
      assert(!std::strstr(foot,kDeg));
    }
  }
  std::puts("Display text tests passed (demo)");
#else
  // Uncalibrated footer: never a MAX line.
  for(Unit u:units) {
    assert(foots(make(Status::NeedsCalibration),112.4f,false,u,"TEMPERATURE NOT VERIFIED"));
    for(int i=0;i<=static_cast<int>(Status::Red);i++) {
      if(static_cast<Status>(i)==Status::NeedsCalibration) continue;
      assert(foots(make(static_cast<Status>(i),123.4f,true,112.4f),112.4f,false,u,"CHECK SENSOR AND WIRING"));
    }
  }
  std::puts("Display text tests passed (uncalibrated)");
#endif
}
