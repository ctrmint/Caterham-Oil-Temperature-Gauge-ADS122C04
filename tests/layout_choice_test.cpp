#include "core/LayoutChoice.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

int main() {
  using gauge::Layout; using gauge::LongPress;
  using gauge::kLongPressMs; using gauge::kLongPressSlopPx;
  assert(kLongPressMs==1000u && kLongPressSlopPx==40);

  // Cycle: Classic -> Sweep -> Needle -> Trace -> Tape -> OEM -> White -> Night -> Nixie -> Classic.
  assert(gauge::nextLayout(Layout::Classic)==Layout::Sweep);
  assert(gauge::nextLayout(Layout::Sweep)==Layout::Needle);
  assert(gauge::nextLayout(Layout::Needle)==Layout::Trace);
  assert(gauge::nextLayout(Layout::Trace)==Layout::Tape);
  assert(gauge::nextLayout(Layout::Tape)==Layout::Oem);
  assert(gauge::nextLayout(Layout::Oem)==Layout::White);
  assert(gauge::nextLayout(Layout::White)==Layout::Night);
  assert(gauge::nextLayout(Layout::Night)==Layout::Nixie);
  assert(gauge::nextLayout(Layout::Nixie)==Layout::Classic);
  { Layout l=Layout::Sweep; for(int i=0;i<9;i++) l=gauge::nextLayout(l); assert(l==Layout::Sweep); }
  { Layout l=Layout::Classic; for(int i=1;i<9;i++) { l=gauge::nextLayout(l); assert(l!=Layout::Classic); } }  // nine distinct

  // Stored value: anything missing or unknown is Classic. Existing values keep their meaning (0 Classic, 1 Sweep).
  assert(gauge::encodeLayout(Layout::Classic)==0 && gauge::encodeLayout(Layout::Sweep)==1);
  assert(gauge::encodeLayout(Layout::Needle)==2 && gauge::encodeLayout(Layout::Trace)==3 && gauge::encodeLayout(Layout::Tape)==4);
  assert(gauge::encodeLayout(Layout::Oem)==5 && gauge::encodeLayout(Layout::White)==6 && gauge::encodeLayout(Layout::Night)==7 && gauge::encodeLayout(Layout::Nixie)==8);
  assert(gauge::decodeLayout(true,0)==Layout::Classic);
  assert(gauge::decodeLayout(true,1)==Layout::Sweep);
  assert(gauge::decodeLayout(true,2)==Layout::Needle);
  assert(gauge::decodeLayout(true,3)==Layout::Trace);
  assert(gauge::decodeLayout(true,4)==Layout::Tape);
  assert(gauge::decodeLayout(true,5)==Layout::Oem);
  assert(gauge::decodeLayout(true,6)==Layout::White);
  assert(gauge::decodeLayout(true,7)==Layout::Night);
  assert(gauge::decodeLayout(true,8)==Layout::Nixie);
  assert(gauge::decodeLayout(true,9)==Layout::Classic);
  assert(gauge::decodeLayout(true,255)==Layout::Classic);
  for(uint8_t v : {0,1,2,3,4,5,6,7,8,9,255}) assert(gauge::decodeLayout(false,v)==Layout::Classic);
  for(Layout l : {Layout::Classic, Layout::Sweep, Layout::Needle, Layout::Trace, Layout::Tape, Layout::Oem, Layout::White, Layout::Night, Layout::Nixie})
    assert(gauge::decodeLayout(true,gauge::encodeLayout(l))==l);

  const uint32_t t0=5000;
  // No press yet.
  { LongPress lp; assert(!lp.hold(100,100,t0)); assert(!lp.hold(100,100,t0+5000)); }
  // Also at and near the zero-initialised origin, at or after the threshold.
  { LongPress lp;
    assert(!lp.hold(0,0,kLongPressMs)); assert(!lp.hold(0,0,t0+5000));
    assert(!lp.hold(10,-10,kLongPressMs)); assert(!lp.hold(kLongPressSlopPx,kLongPressSlopPx,t0+5000)); }

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
    assert(!lp.hold(100,100,t0+kLongPressMs)); assert(!lp.hold(100,100,t0+3000));
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

  // One detector, several presses: a fired press must not block the next one,
  // and the press point is that of the latest press.
  { LongPress lp; lp.press(100,100,t0);
    assert(lp.hold(100,100,t0+kLongPressMs));
    lp.release();
    const uint32_t t1=t0+3000;
    lp.press(300,300,t1);  // far from the first press point
    assert(!lp.hold(300,300,t1+kLongPressMs-1));
    assert(lp.hold(300,300,t1+kLongPressMs));
    assert(!lp.hold(300,300,t1+kLongPressMs+1));
    assert(!lp.hold(300,300,t1+5000)); }

  // Slow drag: each step within the slop, total beyond it, measured from the press point.
  { LongPress lp; lp.press(100,100,t0);
    assert(!lp.hold(130,100,t0+100));
    assert(!lp.hold(160,100,t0+200));
    assert(!lp.hold(190,100,t0+300));
    assert(!lp.hold(160,100,t0+400));
    assert(!lp.hold(130,100,t0+500));
    assert(!lp.hold(100,100,t0+600));
    assert(!lp.hold(100,100,t0+kLongPressMs-1));
    assert(!lp.hold(100,100,t0+kLongPressMs));
    assert(!lp.hold(100,100,t0+3000)); }

  // First sample beyond the slop arrives at the threshold: the slop wins over the time.
  { LongPress lp; lp.press(100,100,t0);
    assert(!lp.hold(100,100,t0+kLongPressMs-1));
    assert(!lp.hold(100+kLongPressSlopPx+1,100,t0+kLongPressMs));
    assert(!lp.hold(100,100,t0+kLongPressMs+1));
    assert(!lp.hold(100,100,t0+3000)); }

  // Press point off the diagonal (x != y): catches x/y swaps.
  { LongPress lp; lp.press(100,300,t0);
    assert(!lp.hold(100+kLongPressSlopPx,300-kLongPressSlopPx,t0+kLongPressMs-1));
    assert(lp.hold(100+kLongPressSlopPx,300-kLongPressSlopPx,t0+kLongPressMs)); }
  { LongPress lp; lp.press(100,300,t0);
    assert(!lp.hold(100+kLongPressSlopPx+1,300,t0+200));
    assert(!lp.hold(100,300,t0+kLongPressMs)); }
  { LongPress lp; lp.press(100,300,t0);
    assert(!lp.hold(100,300+kLongPressSlopPx+1,t0+200));
    assert(!lp.hold(100,300,t0+kLongPressMs)); }

  // press() again without release(): timer and point restart from the second press.
  { LongPress lp; lp.press(100,100,t0);
    assert(!lp.hold(100,100,t0+500));
    const uint32_t t1=t0+800;
    lp.press(300,200,t1);
    assert(!lp.hold(300,200,t1+kLongPressMs-1));
    assert(lp.hold(300,200,t1+kLongPressMs));
    assert(!lp.hold(300,200,t1+kLongPressMs+1)); }

  std::printf("layout choice tests passed\n");
  return 0;
}
