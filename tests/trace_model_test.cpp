#include "core/Calibration.h"
#include "core/SweepModel.h"
#include "core/Temperature.h"
#include "core/TraceModel.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

using gauge::Status; using gauge::TraceHistory; using gauge::TracePoint; using gauge::TraceWindow;

static gauge::Reading make(Status s, bool has_t=false, float t_c=0, Status alarm=Status::Normal) {
  gauge::Reading r;
  r.status=s; r.alarm=alarm; r.has_temperature=has_t; r.temperature_c=t_c; r.resistance_ohms=123.4f;
  return r;
}
static gauge::Reading at(float t_c) { return make(Status::Normal,true,t_c); }
static bool multipleOfStep(float v) { return std::fabs(v/gauge::kTraceStepC-std::round(v/gauge::kTraceStepC))<1e-6f; }
// The window rule's promises after any point: 30 degC tall, ends on 5 degC steps, the point inside with the margin.
static void checkWindow(const TraceWindow &w, float latest) {
  assert(std::fabs((w.hi_c-w.lo_c)-gauge::kTraceSpanC)<1e-6f);
  assert(multipleOfStep(w.lo_c) && multipleOfStep(w.hi_c));
  assert(latest>=w.lo_c+gauge::kTraceMarginC && latest<=w.hi_c-gauge::kTraceMarginC);
}

int main() {
  assert(gauge::kTraceSampleMs==5000u && gauge::kTracePoints==13);  // 60 s on screen
  assert(gauge::kTraceSpanC==30.f && gauge::kTraceStepC==5.f && gauge::kTraceMarginC==3.f);

  // Window rule: centred on the first point, kept while the point stays inside the margin, re-centred otherwise.
  { TraceWindow w=gauge::traceWindow(gauge::kTraceDefaultWindow,false,82.f);
    assert(w.lo_c==65.f && w.hi_c==95.f); checkWindow(w,82.f);
    TraceWindow k=gauge::traceWindow(w,true,68.f); assert(k.lo_c==65.f);   // exactly at the margin: kept
    k=gauge::traceWindow(w,true,92.f); assert(k.lo_c==65.f);
    k=gauge::traceWindow(w,true,92.5f); assert(k.lo_c==80.f && k.hi_c==110.f); checkWindow(k,92.5f);
    k=gauge::traceWindow(w,true,67.9f); assert(k.lo_c==55.f); checkWindow(k,67.9f);
    k=gauge::traceWindow(w,true,NAN); assert(k.lo_c==65.f && k.hi_c==95.f);  // a non-finite point changes nothing
    k=gauge::traceWindow(w,true,INFINITY); assert(k.lo_c==65.f);
    // Without a window yet, even a point inside the default is centred.
    k=gauge::traceWindow(gauge::kTraceDefaultWindow,false,47.f); assert(k.lo_c==30.f && k.hi_c==60.f); checkWindow(k,47.f);
    k=gauge::traceWindow(gauge::kTraceDefaultWindow,false,38.f); assert(k.lo_c==25.f); checkWindow(k,38.f); }
  // A slow climb and fall: the promises always hold, and the window moves only near an edge.
  { TraceWindow w=gauge::kTraceDefaultWindow; bool have=false; int moves=0;
    for(float t=15.f; t<=145.f; t+=0.7f) {
      const TraceWindow n=gauge::traceWindow(w,have,t);
      if(have && (n.lo_c!=w.lo_c)) { ++moves; assert(t<w.lo_c+gauge::kTraceMarginC || t>w.hi_c-gauge::kTraceMarginC); }
      w=n; have=true; checkWindow(w,t);
    }
    for(float t=145.f; t>=15.f; t-=0.7f) { w=gauge::traceWindow(w,true,t); checkWindow(w,t); }
    assert(moves>0 && moves<20); }
  // Threshold lines show only inside the window, ends included.
  { const TraceWindow w={100.f,130.f};
    assert(gauge::traceShows(w,115.f) && gauge::traceShows(w,130.f) && gauge::traceShows(w,100.f));
    assert(!gauge::traceShows(w,130.01f) && !gauge::traceShows(w,99.99f)); }

  // Timing: one sample at the first call, then every 5 s, from 250 ms updates; across the millisecond wrap too.
  for(uint32_t t0 : {0u, 1000u, 0xFFFFFFFFu-7000u, 0xFFFFFFFFu-4999u, 0xFFFFFFFFu}) {
    TraceHistory h; int taken=0; uint32_t last=0;
    for(uint32_t d=0; d<=70000; d+=250) {
      const uint32_t ms=t0+d;
      if(h.update(at(80.f),ms)) {
        if(taken) assert(uint32_t(ms-last)==gauge::kTraceSampleMs);
        else assert(d==0);
        last=ms; ++taken;
      }
    }
    assert(taken==15 && h.count()==gauge::kTracePoints && h.version()==15u);
  }
  // A late loop keeps the 5 s schedule; a stall of two periods or more restarts it, with no catch-up samples.
  { TraceHistory h;
    assert(h.update(at(80.f),0));
    assert(!h.update(at(80.f),4999));
    assert(h.update(at(80.f),5300));    // late: the next is still due at 10000
    assert(!h.update(at(80.f),9999));
    assert(h.update(at(80.f),10000));
    assert(h.update(at(80.f),60000));   // stalled: one sample, then 5 s from here
    assert(!h.update(at(80.f),64999));
    assert(h.update(at(80.f),65000));
    assert(h.count()==5); }

  // Order and capacity: newest last, the oldest dropped once 13 are held; out of range is a gap.
  { TraceHistory h; uint32_t ms=0;
    assert(h.count()==0 && !h.point(0).present && h.version()==0u);
    for(int i=0;i<20;i++,ms+=gauge::kTraceSampleMs) h.update(make(Status::Normal,true,50.f+i),ms);
    assert(h.count()==gauge::kTracePoints);
#ifdef GAUGE_DEMO
    for(int i=0;i<gauge::kTracePoints;i++) assert(h.point(i).present && h.point(i).celsius==50.f+7+i);
#endif
    assert(!h.point(-1).present && !h.point(gauge::kTracePoints).present); }

#ifdef GAUGE_DEMO
  assert(gauge::calibrationValid());
  // Points: the temperature with its reading colour; the table maximum (marked) above it; a gap otherwise.
  { TracePoint p=gauge::tracePoint(at(80.f));
    assert(p.present && !p.above_range && p.celsius==80.f && p.colour==gauge::readingColour(at(80.f)));
    p=gauge::tracePoint(make(Status::Amber,true,117.f,Status::Amber));
    assert(p.present && p.colour==gauge::readingColour(make(Status::Amber,true,117.f,Status::Amber)));
    p=gauge::tracePoint(make(Status::AboveRange,false,0.f,Status::Red));
    assert(p.present && p.above_range && p.celsius==gauge::calibrationMaxC() && p.colour==gauge::kColourRed);
    assert(!gauge::tracePoint(make(Status::BelowRange)).present);
    assert(!gauge::tracePoint(make(Status::BelowRange,false,0.f,Status::Amber)).present);  // REQ-T3 hold: no number
    assert(!gauge::tracePoint(make(Status::Normal,true,NAN)).present);
    assert(!gauge::tracePoint(make(Status::Normal,false,80.f)).present);  // stale temperature, no flag
    for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
      const Status s=static_cast<Status>(i);
      if(!gauge::isFault(s)) continue;
      // A fault is a gap, even while an alarm is latched through it: nothing is carried over.
      for(Status a : {Status::Normal,Status::Amber,Status::Red}) assert(!gauge::tracePoint(make(s,true,80.f,a)).present);
    } }
  // History: gaps break the line and leave the window alone; points move it.
  { TraceHistory h; uint32_t ms=0;
    h.update(at(82.f),ms);                       ms+=gauge::kTraceSampleMs;
    assert(h.window().lo_c==65.f);
    h.update(make(Status::OpenCircuit),ms);      ms+=gauge::kTraceSampleMs;
    h.update(make(Status::BelowRange),ms);       ms+=gauge::kTraceSampleMs;
    assert(h.window().lo_c==65.f && h.count()==3);
    assert(h.point(0).present && !h.point(1).present && !h.point(2).present);
    h.update(at(120.f),ms);                      ms+=gauge::kTraceSampleMs;
    assert(h.window().lo_c==105.f && h.window().hi_c==135.f);
    h.update(make(Status::AboveRange,false,0.f,Status::Red),ms);
    assert(h.point(4).above_range && h.point(4).celsius==140.f);
    checkWindow(h.window(),140.f); }
  std::puts("Trace model tests passed (demo)");
#else
  assert(!gauge::calibrationValid());
  // Uncalibrated: no temperature, so every sample is a gap and the window never leaves its default.
  for(int i=static_cast<int>(Status::NoAdc);i<=static_cast<int>(Status::Red);i++) {
    const Status s=static_cast<Status>(i);
    for(Status a : {Status::Normal,Status::Amber,Status::Red}) {
      assert(!gauge::tracePoint(make(s,false,0.f,a)).present);
      assert(!gauge::tracePoint(make(s,true,80.f,a)).present);
    }
  }
  { TraceHistory h;
    for(uint32_t ms=0; ms<=70000; ms+=250) h.update(make(Status::NeedsCalibration),ms);
    assert(h.count()==gauge::kTracePoints);
    for(int i=0;i<h.count();i++) assert(!h.point(i).present);
    assert(h.window().lo_c==gauge::kTraceDefaultWindow.lo_c && h.window().hi_c==gauge::kTraceDefaultWindow.hi_c); }
  std::puts("Trace model tests passed (uncalibrated)");
#endif
  return 0;
}
