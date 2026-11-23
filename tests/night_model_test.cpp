#include "core/Calibration.h"
#include "core/NightModel.h"
#include "core/OemModel.h"
#include "core/SweepModel.h"
#include "core/WhiteModel.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

using gauge::Status;

static gauge::Reading make(Status s, bool has_t=false, float t_c=0, Status alarm=Status::Normal) {
  gauge::Reading r;
  r.status=s; r.alarm=alarm; r.has_temperature=has_t; r.temperature_c=t_c; r.resistance_ohms=123.4f;
  return r;
}
static const Status kAlarms[]={Status::Normal,Status::Amber,Status::Red};

int main() {
  // The heartbeat: 8 updates a beat (2 s at the 250 ms display interval), between about 10 % and 40 %.
  assert(gauge::kNightBeatFrames==8u && gauge::kNightBeatMinOpa==26 && gauge::kNightBeatMaxOpa==102);
  { const uint8_t beat[]={26,45,64,83,102,83,64,45};  // up in four even steps, then down again
    for(uint32_t f=0;f<16;f++) assert(gauge::nightBeat(f)==beat[f%8]); }
  // The same for every frame count: periodic, inside the range, lowest at the start of a beat and highest half-way, and
  // never the same on two updates in a row, so a frozen screen would show.
  for(uint32_t base : {0u,1000u,0x7FFFFFF0u,0xFFFFFF00u}) {
    for(uint32_t k=0;k<64;k++) {
      const uint32_t f=base+k;
      const uint8_t b=gauge::nightBeat(f);
      assert(b>=gauge::kNightBeatMinOpa && b<=gauge::kNightBeatMaxOpa);
      assert(b==gauge::nightBeat(f+gauge::kNightBeatFrames));
      assert(b!=gauge::nightBeat(f+1));
      if(f%8==0) assert(b==gauge::kNightBeatMinOpa);
      if(f%8==4) assert(b==gauge::kNightBeatMaxOpa);
      if(f%8<4) assert(gauge::nightBeat(f+1)>b);   // rising
      else assert(gauge::nightBeat(f+1)<b);        // falling
      assert(b==gauge::nightBeat(f-f%8+(8-f%8)%8));  // symmetric about the peak
    }
  }
  // Across the counter's wrap the beat runs on without a jump.
  assert(gauge::nightBeat(0xFFFFFFFFu)==45 && gauge::nightBeat(0u)==26 && gauge::nightBeat(1u)==45);
  assert(gauge::nightBeat(0xFFFFFFFCu)==102 && gauge::nightBeat(0xFFFFFFFDu)==83);

  // The demo build is never quiet, whatever the reading, so DEMO ONLY - NOT REAL always shows.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++)
    for(Status a : kAlarms) {
      assert(!gauge::nightQuiet(make(static_cast<Status>(i),true,80.f,a),true));
      assert(!gauge::nightQuiet(make(static_cast<Status>(i),false,0.f,a),true));
    }

#ifdef GAUGE_DEMO
  assert(gauge::calibrationValid());
  // Quiet only for NORMAL or OIL WARMING with a finite temperature, no alarm latched, not the demo build: exactly when OEM
  // shows CATERHAM and White hides its window.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Status a : kAlarms) for(bool has_t : {false,true}) for(float t : {25.f,80.f,NAN,INFINITY,-INFINITY}) for(bool demo : {false,true}) {
      const gauge::Reading r=make(s,has_t,t,a);
      const bool quiet=!demo && a==Status::Normal && (s==Status::Normal || s==Status::Cold) && has_t && std::isfinite(t);
      assert(gauge::nightQuiet(r,demo)==quiet);
      assert(gauge::nightQuiet(r,demo)==gauge::allIsWell(r,demo));
      assert(gauge::nightQuiet(r,demo)==gauge::oemWordmark(r,demo));
      assert(gauge::nightQuiet(r,demo)==!gauge::whiteWindow(r,demo));
    }
  }
  assert(gauge::nightQuiet(make(Status::Normal,true,80.f),false));
  assert(gauge::nightQuiet(make(Status::Cold,true,25.f),false));
  assert(!gauge::nightQuiet(make(Status::Amber,true,117.f,Status::Normal),false));  // lights at the threshold, before the alarm
  assert(!gauge::nightQuiet(make(Status::Normal,true,110.f,Status::Amber),false));  // still latched while cooling
  assert(!gauge::nightQuiet(make(Status::BelowRange,false,0.f),false));
  assert(!gauge::nightQuiet(make(Status::AboveRange,false,0.f,Status::Red),false));
  assert(!gauge::nightQuiet(make(Status::OpenCircuit,false,0.f),false));
  std::puts("Night model tests passed (demo)");
#else
  assert(!gauge::calibrationValid());
  // Uncalibrated: never quiet, so CALIBRATION REQUIRED always shows, whatever arrives.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Status a : kAlarms) for(bool has_t : {false,true}) for(bool demo : {false,true})
      assert(!gauge::nightQuiet(make(s,has_t,80.f,a),demo));
  }
  std::puts("Night model tests passed (uncalibrated)");
#endif
  return 0;
}
