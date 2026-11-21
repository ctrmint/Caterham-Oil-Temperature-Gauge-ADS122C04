#include "core/Calibration.h"
#include "core/SweepModel.h"
#include "core/Temperature.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static gauge::Reading make(gauge::Status s, bool has_t=false, float t_c=0) {
  gauge::Reading r;
  r.status=s; r.resistance_ohms=123.4f; r.has_temperature=has_t; r.temperature_c=t_c;
  return r;
}
static int lit(float t_c, gauge::Unit u) { return gauge::litSegments(make(gauge::Status::Normal,true,t_c),u); }
static int channel(uint32_t rgb, int shift) { return static_cast<int>((rgb>>shift)&0xFF); }
// Half-way mix, rounded half away from zero per channel, as the gradient does at f = 0.5.
static uint32_t half(uint32_t a, uint32_t b) {
  uint32_t out=0;
  for(int s=16;s>=0;s-=8) out|=static_cast<uint32_t>((channel(a,s)+channel(b,s)+1)/2)<<s;
  return out;
}
static int maxChannelStep(uint32_t a, uint32_t b) {
  int m=0;
  for(int s=16;s>=0;s-=8) { const int d=std::abs(channel(a,s)-channel(b,s)); if(d>m) m=d; }
  return m;
}
static uint32_t colourOf(gauge::Status s, gauge::Status alarm, bool has_t=false, float t_c=0) {
  gauge::Reading r=make(s,has_t,t_c); r.alarm=alarm;
  return gauge::readingColour(r);
}
static gauge::LedState ledOf(gauge::Status s, gauge::Status alarm, bool has_t=false, float t_c=0) {
  gauge::Reading r=make(s,has_t,t_c); r.alarm=alarm;
  return gauge::ledState(r);
}
static gauge::NeedleState needleOf(gauge::Status s, bool has_t, float t_c, gauge::Unit u, gauge::Status alarm=gauge::Status::Normal) {
  gauge::Reading r=make(s,has_t,t_c); r.alarm=alarm;
  return gauge::needleState(r,u);
}
[[maybe_unused]] static bool near(float a, float b) { return std::fabs(a-b)<1e-5f; }

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
  assert(gauge::sweepScale(Unit::Celsius).min==30.f && gauge::sweepScale(Unit::Celsius).max==130.f);
  assert(gauge::sweepScale(Unit::Fahrenheit).min==86.f && gauge::sweepScale(Unit::Fahrenheit).max==266.f);
  assert(gauge::scaleLabelCount(Unit::Celsius)==6);
  assert(gauge::scaleLabelCount(Unit::Fahrenheit)==6);
  for(int i=0;i<6;i++) assert(gauge::scaleLabel(Unit::Celsius,i)==30+20*i);
  for(int i=0;i<6;i++) assert(gauge::scaleLabel(Unit::Fahrenheit,i)==86+36*i);
  assert(gauge::scaleLabel(Unit::Celsius,5)==130 && gauge::scaleLabel(Unit::Fahrenheit,5)==266);

  // Gradient: pure at each stop, a half-way mix at each band boundary, clamped at the ends.
  using gauge::gradientColour;
  assert(gradientColour(-40.f)==gauge::kColourDarkBlue && gradientColour(40.f)==gauge::kColourDarkBlue);
  assert(gradientColour(-INFINITY)==gauge::kColourDarkBlue);
  assert(gradientColour(60.f)==gauge::kColourLightBlue);
  assert(gradientColour(75.f)==gauge::kColourGreen && gradientColour(82.f)==gauge::kColourGreen);
  assert(gradientColour(90.f)==gauge::kColourGreen);
  assert(gradientColour(100.f)==gauge::kColourYellow);
  assert(gradientColour(112.f)==gauge::kColourAmber);
  assert(gradientColour(120.f)==gauge::kColourRed && gradientColour(200.f)==gauge::kColourRed);
  assert(gradientColour(INFINITY)==gauge::kColourRed);
  assert(gradientColour(NAN)==gauge::kColourAmber);
  assert(gradientColour(50.f)==half(gauge::kColourDarkBlue,gauge::kColourLightBlue));   // cold | cool
  assert(gradientColour(95.f)==half(gauge::kColourGreen,gauge::kColourYellow));         // optimum | slightly warm
  assert(gradientColour(106.f)==half(gauge::kColourYellow,gauge::kColourAmber));        // slightly warm | hot
  assert(gradientColour(116.f)==half(gauge::kColourAmber,gauge::kColourRed));           // inside hot, towards danger
  assert(gradientColour(119.f)!=gauge::kColourRed);   // still blending 1 degC below the red stop
  assert(gradientColour(41.f)!=gauge::kColourDarkBlue && gradientColour(111.f)!=gauge::kColourAmber);
  // No hard steps: 0.1 degC never moves a channel by more than 4.
  for(int i=0;i<2000;i++) {
    const float t=i*0.1f;
    assert(maxChannelStep(gradientColour(t),gradientColour(t+0.1f))<=4);
  }
  // Segment colours: the gradient at each segment's centre, the same in both units.
  assert(gauge::segmentColour(0,Unit::Celsius)==gauge::kColourDarkBlue);    // 31.25 degC
  assert(gauge::segmentColour(18,Unit::Celsius)==gauge::kColourGreen);      // 76.25 degC
  assert(gauge::segmentColour(28,Unit::Celsius)==gradientColour(101.25f));
  assert(gauge::segmentColour(39,Unit::Celsius)==gauge::kColourRed);        // 128.75 degC
  for(int i=0;i<gauge::kSweepSegments;i++) {
    assert(gauge::segmentColour(i,Unit::Celsius)==gradientColour(30.f+(i+0.5f)*2.5f));
    assert(maxChannelStep(gauge::segmentColour(i,Unit::Celsius),gauge::segmentColour(i,Unit::Fahrenheit))<=1);
  }

  // Reading colour in every build: a Red alarm is red, a fault is amber (red with a Red alarm).
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::NeedsCalibration);i++) {
    const Status s=static_cast<Status>(i);
    assert(colourOf(s,Status::Normal)==gauge::kColourAmber);
    assert(colourOf(s,Status::Amber)==gauge::kColourAmber);
    assert(colourOf(s,Status::Red)==gauge::kColourRed);
    assert(colourOf(s,Status::Normal,true,80.f)==gauge::kColourAmber);  // a fault never takes a temperature colour
  }

  // Seven-segment table: bits 0-6 are segments a-g.
  const char digits[]="0123456789- ";
  const uint8_t masks[]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,0x40,0x00};
  for(int i=0;i<12;i++) assert(gauge::sevenSegmentMask(digits[i])==masks[i]);
  assert(gauge::sevenSegmentMask('<')==0 && gauge::sevenSegmentMask('>')==0);
  assert(gauge::sevenSegmentMask('A')==0 && gauge::sevenSegmentMask('\0')==0);

  // Sweep's three cells: the sign is split off, the rest right-aligned; what does not fit is " --" with no sign.
  assert(gauge::kSweepCells==3);
  const struct { const char *in; char sign; const char *cells; } split[]={
    {"123",0,"123"}, {"85",0," 85"}, {"5",0,"  5"}, {"-5",0," -5"}, {"-15",0,"-15"}, {"--",0," --"},
    {">140",'>',"140"}, {"<20",'<'," 20"}, {">284",'>',"284"}, {"<68",'<'," 68"}, {"<-5",'<'," -5"},
    {"1234",0," --"}, {"4999",0," --"}, {">1000",0," --"}, {">",0," --"}, {"<",0," --"}, {"",0," --"}};
  for(const auto &k:split) {
    char sign='x'; char cells[gauge::kSweepCells+1];
    gauge::sweepReading(k.in,sign,cells,sizeof(cells));
    assert(sign==k.sign);
    assert(std::strcmp(cells,k.cells)==0);
  }
  {
    char sign='x'; char cells[gauge::kSweepCells+1];
    gauge::sweepReading(nullptr,sign,cells,sizeof(cells));
    assert(sign==0 && std::strcmp(cells," --")==0);
    char tiny[2]={'q','q'};
    gauge::sweepReading(">140",sign,tiny,sizeof(tiny));  // too small: truncated, never overrun
    assert(tiny[1]=='\0');
  }

  // Nothing lit without a temperature, in any build.
  for(Unit u:units) {
    assert(gauge::litSegments(make(Status::BelowRange),u)==0);
    gauge::Reading below=make(Status::BelowRange); below.alarm=Status::Red;
    assert(gauge::litSegments(below,u)==0);
    for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::NeedsCalibration);i++)
      assert(gauge::litSegments(make(static_cast<Status>(i)),u)==0);
    assert(gauge::litSegments(make(Status::Normal,true,NAN),u)==0);
    assert(gauge::litSegments(make(Status::Normal,true,INFINITY),u)==0);
    assert(gauge::litSegments(make(Status::Normal,true,-INFINITY),u)==0);
    assert(gauge::litSegments(make(Status::Normal,false,100.f),u)==0);  // stale temperature, no flag
  }

  // Needle dial numbers (#11): five over Sweep's span. Sweep keeps its own numbers every 20 degC (tested above).
  assert(gauge::kNeedleLabels==5);
  { const int c[]={30,55,80,105,130}, f[]={86,131,176,221,266};
    for(int i=0;i<gauge::kNeedleLabels;i++) {
      assert(gauge::needleLabel(Unit::Celsius,i)==c[i]);
      assert(gauge::needleLabel(Unit::Fahrenheit,i)==f[i]);
    }
    assert(gauge::needleLabel(Unit::Celsius,0)==static_cast<int>(gauge::sweepScale(Unit::Celsius).min));
    assert(gauge::needleLabel(Unit::Celsius,4)==static_cast<int>(gauge::sweepScale(Unit::Celsius).max)); }

  // LED pulse (#10): full brightness when steady; a cosine from 255 down to kLedMinOpa and back; Red twice as fast.
  { using gauge::LedPulse; using gauge::ledOpacity;
    assert(gauge::kLedFastPeriodMs*2==gauge::kLedSlowPeriodMs && gauge::kLedMinOpa>0);
    for(uint32_t ms : {0u,1u,250u,499u,999u,123456u,0xFFFFFFFFu}) assert(ledOpacity(LedPulse::None,ms)==255);
    assert(ledOpacity(LedPulse::Slow,0)==255 && ledOpacity(LedPulse::Slow,gauge::kLedSlowPeriodMs)==255);
    assert(ledOpacity(LedPulse::Fast,0)==255 && ledOpacity(LedPulse::Fast,gauge::kLedFastPeriodMs)==255);
    assert(ledOpacity(LedPulse::Slow,gauge::kLedSlowPeriodMs/2)==gauge::kLedMinOpa);
    assert(ledOpacity(LedPulse::Fast,gauge::kLedFastPeriodMs/2)==gauge::kLedMinOpa);
    for(uint32_t ms=0; ms<4000; ms+=3) {
      const uint8_t s=ledOpacity(LedPulse::Slow,ms), f=ledOpacity(LedPulse::Fast,ms);
      assert(s>=gauge::kLedMinOpa && f>=gauge::kLedMinOpa);
      assert(f==ledOpacity(LedPulse::Slow,2*ms));
      const int mirror=ledOpacity(LedPulse::Slow,gauge::kLedSlowPeriodMs-(ms%gauge::kLedSlowPeriodMs));
      assert(std::abs(static_cast<int>(s)-mirror)<=1);  // symmetric about the half-period
    }
    for(uint32_t ms=1; ms<=gauge::kLedSlowPeriodMs/2; ms++)  // dims steadily, then (by symmetry) brightens
      assert(ledOpacity(LedPulse::Slow,ms)<=ledOpacity(LedPulse::Slow,ms-1)); }

#ifdef GAUGE_DEMO
  assert(gauge::scaleLabelsVisible());
  // Lit count: floor((T-min)/(max-min)*40), clamped.
  assert(lit(30.f,Unit::Celsius)==0);
  assert(lit(130.f,Unit::Celsius)==40);
  assert(lit(67.5f,Unit::Celsius)==15);   // exact boundary: 37.5/100 = 0.375
  assert(lit(67.49f,Unit::Celsius)==14);
  assert(lit(67.51f,Unit::Celsius)==15);
  assert(lit(20.f,Unit::Celsius)==0);
  assert(lit(140.f,Unit::Celsius)==40);
  assert(lit(60.f,Unit::Fahrenheit)==12); // 60 degC = 140 degF, exact boundary: 54/180 = 0.3
  assert(lit(59.99f,Unit::Fahrenheit)==11);
  assert(lit(60.01f,Unit::Fahrenheit)==12);
  assert(lit(25.f,Unit::Fahrenheit)==0);
  assert(lit(140.f,Unit::Fahrenheit)==40);
  // Statuses the ring really gets: Cold, Amber and Red carry a temperature too.
  const struct { Status s; float t_c; int c; int f; } carry[]={
    {Status::Cold,51.f,8,8}, {Status::Amber,117.f,34,34}, {Status::Red,131.f,40,40}};
  for(const auto &k:carry) {
    assert(gauge::litSegments(make(k.s,true,k.t_c),Unit::Celsius)==k.c);
    assert(gauge::litSegments(make(k.s,true,k.t_c),Unit::Fahrenheit)==k.f);
  }
  // Faults and BelowRange light nothing even with a temperature set or an alarm latched.
  for(Unit u:units) {
    for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::BelowRange);i++) {
      assert(gauge::litSegments(make(static_cast<Status>(i),true,100.f),u)==0);
      for(Status a:{Status::Amber,Status::Red}) {
        gauge::Reading r=make(static_cast<Status>(i)); r.alarm=a;
        assert(gauge::litSegments(r,u)==0);
        r.has_temperature=true; r.temperature_c=100.f;
        assert(gauge::litSegments(r,u)==0);
      }
    }
  }
  // Above range lights up to the table maximum (140 degC, beyond the 130 degC scale end, so the full ring).
  for(Unit u:units) {
    assert(gauge::litSegments(make(Status::AboveRange),u)==lit(gauge::calibrationMaxC(),u));
    assert(gauge::litSegments(make(Status::AboveRange),u)==40);
  }

  // Reading colour follows the temperature with no alarm latched.
  assert(colourOf(Status::Normal,Status::Normal,true,80.f)==gauge::kColourGreen);
  assert(colourOf(Status::Cold,Status::Normal,true,45.f)==gradientColour(45.f));
  assert(colourOf(Status::Normal,Status::Normal,true,106.f)==gradientColour(106.f));
  assert(colourOf(Status::Normal,Status::Normal,true,116.f)==gradientColour(116.f));  // before the Amber debounce
  assert(colourOf(Status::Normal,Status::Normal,true,125.f)==gauge::kColourRed);     // red band before the Red alarm
  assert(colourOf(Status::Normal,Status::Normal,true,NAN)==gauge::kColourAmber);
  assert(colourOf(Status::Normal,Status::Normal,true,INFINITY)==gauge::kColourAmber);   // not a temperature
  assert(colourOf(Status::Normal,Status::Normal,true,-INFINITY)==gauge::kColourAmber);
  assert(colourOf(Status::Normal,Status::Normal)==gauge::kColourAmber);              // no temperature flagged
  // Never less severe than the alarm: an Amber alarm gives at least amber, a Red alarm gives red.
  assert(colourOf(Status::Normal,Status::Amber,true,111.f)==gauge::kColourAmber);    // in the 5 degC hysteresis
  assert(colourOf(Status::Normal,Status::Amber,true,60.f)==gauge::kColourAmber);
  assert(colourOf(Status::Normal,Status::Amber,true,112.f)==gauge::kColourAmber);
  assert(colourOf(Status::Amber,Status::Amber,true,118.f)==gradientColour(118.f));  // warmer than amber stays warmer
  assert(colourOf(Status::Normal,Status::Red,true,126.f)==gauge::kColourRed);
  assert(colourOf(Status::Normal,Status::Red,true,80.f)==gauge::kColourRed);
  // Beyond the table: the colour of the table limit shown, unless an alarm demands more.
  assert(colourOf(Status::AboveRange,Status::Normal)==gradientColour(gauge::calibrationMaxC()));
  assert(colourOf(Status::AboveRange,Status::Normal)==gauge::kColourRed);
  assert(colourOf(Status::BelowRange,Status::Normal)==gradientColour(gauge::calibrationMinC()));
  assert(colourOf(Status::BelowRange,Status::Normal)==gauge::kColourDarkBlue);
  assert(colourOf(Status::BelowRange,Status::Amber)==gauge::kColourAmber);   // REQ-T3 hold keeps the alarm colour
  assert(colourOf(Status::BelowRange,Status::Red)==gauge::kColourRed);

  // LED (#10).
  { using gauge::LedPulse;
    gauge::LedState l=ledOf(Status::Cold,Status::Normal,true,40.f);
    assert(l.lit && l.pulse==LedPulse::None && l.colour==gauge::kColourDarkBlue);
    l=ledOf(Status::Normal,Status::Normal,true,80.f);
    assert(l.lit && l.pulse==LedPulse::None && l.colour==colourOf(Status::Normal,Status::Normal,true,80.f));
    // Over 115 degC but still inside the debounce: steady. It pulses only once the alarm latches.
    l=ledOf(Status::Normal,Status::Normal,true,117.f);
    assert(l.lit && l.pulse==LedPulse::None);
    l=ledOf(Status::Amber,Status::Amber,true,117.f);
    assert(l.lit && l.pulse==LedPulse::Slow && l.colour==colourOf(Status::Amber,Status::Amber,true,117.f));
    l=ledOf(Status::Red,Status::Red,true,131.f);
    assert(l.lit && l.pulse==LedPulse::Fast && l.colour==gauge::kColourRed);
    // Beyond the table: lit in the table limit's colour, pulsing once the alarm latches.
    l=ledOf(Status::AboveRange,Status::Normal);
    assert(l.lit && l.pulse==LedPulse::None && l.colour==gradientColour(gauge::calibrationMaxC()));
    l=ledOf(Status::AboveRange,Status::Red);
    assert(l.lit && l.pulse==LedPulse::Fast && l.colour==gauge::kColourRed);
    l=ledOf(Status::BelowRange,Status::Normal);
    assert(l.lit && l.pulse==LedPulse::None && l.colour==gauge::kColourDarkBlue);
    l=ledOf(Status::BelowRange,Status::Amber);  // REQ-T3 hold
    assert(l.lit && l.pulse==LedPulse::Slow && l.colour==gauge::kColourAmber);
    for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
      const Status s=static_cast<Status>(i);
      // Faults: off, unless an alarm is still latched through a brief fault; then it keeps pulsing in the alarm's colour.
      if(gauge::isFault(s)) {
        assert(!ledOf(s,Status::Normal).lit && !ledOf(s,Status::Normal,true,80.f).lit);
        l=ledOf(s,Status::Amber); assert(l.lit && l.pulse==LedPulse::Slow && l.colour==gauge::kColourAmber);
        l=ledOf(s,Status::Red); assert(l.lit && l.pulse==LedPulse::Fast && l.colour==gauge::kColourRed);
      }
      // Whenever lit it shows the reading's colour, never a calmer one; it pulses exactly while an alarm is latched.
      for(Status a : {Status::Normal,Status::Amber,Status::Red}) {
        for(float t : {40.f,80.f,117.f,135.f}) {
          for(bool has_t : {false,true}) {
            l=ledOf(s,a,has_t,t);
            if(a!=Status::Normal) assert(l.lit);
            if(l.lit) assert(l.colour==colourOf(s,a,has_t,t));
            assert((l.pulse==LedPulse::None)==(a==Status::Normal));
            assert(l.pulse!=LedPulse::Fast || a==Status::Red);
          }
        }
      }
    } }

  // Needle (#11): follows the Sweep segments, clamped to the scale.
  for(Unit u:units) {
    gauge::NeedleState n=needleOf(Status::Normal,true,30.f,u); assert(n.visible && near(n.fraction,0.f));
    n=needleOf(Status::Normal,true,55.f,u); assert(n.visible && near(n.fraction,0.25f));
    n=needleOf(Status::Normal,true,80.f,u); assert(n.visible && near(n.fraction,0.5f));
    n=needleOf(Status::Amber,true,105.f,u,Status::Amber); assert(n.visible && near(n.fraction,0.75f));
    n=needleOf(Status::Red,true,130.f,u,Status::Red); assert(n.visible && near(n.fraction,1.f));
    n=needleOf(Status::Cold,true,20.f,u); assert(n.visible && n.fraction==0.f);   // in the table, below the scale
    n=needleOf(Status::Red,true,135.f,u,Status::Red); assert(n.visible && n.fraction==1.f);
    n=needleOf(Status::AboveRange,false,0.f,u); assert(n.visible && n.fraction==1.f);  // table max 140, capped
    n=needleOf(Status::BelowRange,false,0.f,u); assert(n.visible && n.fraction==0.f);
    n=needleOf(Status::BelowRange,false,0.f,u,Status::Amber); assert(n.visible && n.fraction==0.f);  // REQ-T3 hold
    for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
      const Status s=static_cast<Status>(i);
      if(!gauge::isFault(s)) continue;
      for(Status a : {Status::Normal,Status::Amber,Status::Red}) {
        assert(!needleOf(s,false,0.f,u,a).visible);
        assert(!needleOf(s,true,80.f,u,a).visible);
      }
    }
    assert(!needleOf(Status::Normal,true,NAN,u).visible);
    assert(!needleOf(Status::Normal,true,INFINITY,u).visible);
    assert(!needleOf(Status::Normal,true,-INFINITY,u).visible);
    assert(!needleOf(Status::Normal,false,80.f,u).visible);  // stale temperature, no flag
    // The needle and the Sweep segments agree: the lit count is the needle's place in whole segments.
    for(float t=30.f; t<=130.f; t+=0.37f) {
      n=needleOf(Status::Normal,true,t,u);
      const float segs=n.fraction*gauge::kSweepSegments;
      assert(n.visible && n.fraction>=0.f && n.fraction<=1.f);
      assert(std::fabs(static_cast<float>(lit(t,u))-std::floor(segs))<=1.f && static_cast<float>(lit(t,u))<=segs+1e-3f);
    }
  }
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
  // Fail closed: uncalibrated, nothing takes a temperature colour; only a Red alarm (never latched here) would be red.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    assert(colourOf(s,Status::Normal)==gauge::kColourAmber);
    assert(colourOf(s,Status::Normal,true,80.f)==gauge::kColourAmber);
    assert(colourOf(s,Status::Amber,true,40.f)==gauge::kColourAmber);
    assert(colourOf(s,Status::Red,true,40.f)==gauge::kColourRed);
    // No temperature, so no LED and no needle, whatever the status or alarm.
    for(Status a : {Status::Normal,Status::Amber,Status::Red}) {
      for(bool has_t : {false,true}) {
        assert(!ledOf(s,a,has_t,100.f).lit);
        for(Unit u:units) assert(!needleOf(s,has_t,100.f,u,a).visible);
      }
    }
  }
  std::puts("Sweep model tests passed (uncalibrated)");
#endif
}
