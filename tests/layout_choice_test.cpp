#include "core/LayoutChoice.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

int main() {
  using gauge::Layout; using gauge::LongPress;
  using gauge::kLongPressMs; using gauge::kLongPressSlopPx;
  assert(kLongPressMs==1000u && kLongPressSlopPx==40);

  // Cycle.
  assert(gauge::nextLayout(Layout::Classic)==Layout::Sweep);
  assert(gauge::nextLayout(Layout::Sweep)==Layout::Classic);

  // Stored value: anything missing or unknown is Classic.
  assert(gauge::encodeLayout(Layout::Classic)==0 && gauge::encodeLayout(Layout::Sweep)==1);
  assert(gauge::decodeLayout(true,0)==Layout::Classic);
  assert(gauge::decodeLayout(true,1)==Layout::Sweep);
  assert(gauge::decodeLayout(true,2)==Layout::Classic);
  assert(gauge::decodeLayout(true,255)==Layout::Classic);
  assert(gauge::decodeLayout(false,0)==Layout::Classic);
  assert(gauge::decodeLayout(false,1)==Layout::Classic);
  for(Layout l : {Layout::Classic, Layout::Sweep}) assert(gauge::decodeLayout(true,gauge::encodeLayout(l))==l);

  const uint32_t t0=5000;
  // No press yet.
  { LongPress lp; assert(!lp.hold(100,100,t0)); assert(!lp.hold(100,100,t0+5000)); }

  // Long press fires exactly once, at the threshold.
  { LongPress lp; lp.press(100,100,t0);
    assert(!lp.hold(100,100,t0));
    assert(!lp.hold(100,100,t0+kLongPressMs-1));
    assert(lp.hold(100,100,t0+kLongPressMs));
    assert(!lp.hold(100,100,t0+kLongPressMs+1));
    assert(!lp.hold(100,100,t0+5000)); }

  // Tap: released early, then nothing; a new press restarts the timer.
  { LongPress lp; lp.press(100,100,t0);
    for(uint32_t d=0; d<=900; d+=100) assert(!lp.hold(100,100,t0+d));
    lp.release();
    assert(!lp.hold(100,100,t0+1000)); assert(!lp.hold(100,100,t0+3000));
    const uint32_t t1=t0+4000;
    lp.press(100,100,t1);
    assert(!lp.hold(100,100,t1+kLongPressMs-1));
    assert(lp.hold(100,100,t1+kLongPressMs)); }

  // Swipe beyond slop (x or y) cancels, even if the finger returns.
  { LongPress lp; lp.press(100,100,t0);
    assert(!lp.hold(100+kLongPressSlopPx+1,100,t0+200));
    assert(!lp.hold(100,100,t0+2000)); assert(!lp.hold(100,100,t0+5000)); }
  { LongPress lp; lp.press(100,100,t0);
    assert(!lp.hold(100,100+kLongPressSlopPx+1,t0+200));
    assert(!lp.hold(100,100,t0+2000)); }
  { LongPress lp; lp.press(100,100,t0);  // negative direction
    assert(!lp.hold(100-kLongPressSlopPx-1,100,t0+200));
    assert(!lp.hold(100,100,t0+2000)); }
  { LongPress lp; lp.press(100,100,t0);
    assert(!lp.hold(100,100-kLongPressSlopPx-1,t0+200));
    assert(!lp.hold(100,100,t0+2000)); }
  // A cancelled press stays cancelled until release; a new press works again.
  { LongPress lp; lp.press(100,100,t0);
    assert(!lp.hold(200,100,t0+100));
    lp.release();
    lp.press(100,100,t0+3000);
    assert(lp.hold(100,100,t0+3000+kLongPressMs)); }

  // Jitter exactly at the slop in both axes still counts.
  { LongPress lp; lp.press(100,100,t0);
    assert(!lp.hold(100+kLongPressSlopPx,100-kLongPressSlopPx,t0+500));
    assert(lp.hold(100+kLongPressSlopPx,100-kLongPressSlopPx,t0+kLongPressMs)); }
  { LongPress lp; lp.press(100,100,t0);
    assert(lp.hold(100-kLongPressSlopPx,100+kLongPressSlopPx,t0+kLongPressMs)); }

  // Millisecond wraparound: press 512 ms before the wrap.
  { const uint32_t p=0xFFFFFE00u; LongPress lp; lp.press(100,100,p);
    assert(!lp.hold(100,100,p));
    assert(!lp.hold(100,100,uint32_t(p+100)));   // before the wrap
    assert(!lp.hold(100,100,uint32_t(p+511)));
    assert(!lp.hold(100,100,uint32_t(p+512)));   // wrapped to 0
    assert(!lp.hold(100,100,uint32_t(p+600)));
    assert(!lp.hold(100,100,uint32_t(p+kLongPressMs-1)));
    assert(lp.hold(100,100,uint32_t(p+kLongPressMs)));
    assert(!lp.hold(100,100,uint32_t(p+kLongPressMs+1))); }

  // Same across the signed boundary (2^31), to catch signed comparisons.
  { const uint32_t p=0x7FFFFE00u; LongPress lp; lp.press(100,100,p);
    assert(!lp.hold(100,100,uint32_t(p+100)));
    assert(!lp.hold(100,100,uint32_t(p+512)));
    assert(!lp.hold(100,100,uint32_t(p+kLongPressMs-1)));
    assert(lp.hold(100,100,uint32_t(p+kLongPressMs))); }

  std::printf("layout choice tests passed\n");
  return 0;
}
