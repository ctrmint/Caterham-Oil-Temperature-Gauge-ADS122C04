#include "LayoutChoice.h"
#include <cstdlib>
namespace gauge {
Layout nextLayout(Layout current) {
  switch (current) {
    case Layout::Classic: return Layout::Sweep;
    case Layout::Sweep: return Layout::Needle;
    case Layout::Needle: return Layout::Trace;
    default: return Layout::Classic;
  }
}
Layout decodeLayout(bool found, uint8_t stored) {
  if (!found) return Layout::Classic;
  switch (stored) {
    case 1: return Layout::Sweep;
    case 2: return Layout::Needle;
    case 3: return Layout::Trace;
    default: return Layout::Classic;
  }
}
uint8_t encodeLayout(Layout layout) { return static_cast<uint8_t>(layout); }
void LongPress::press(int32_t x, int32_t y, uint32_t ms) { armed_ = true; x_ = x; y_ = y; since_ = ms; }
bool LongPress::hold(int32_t x, int32_t y, uint32_t ms) {
  if (!armed_) return false;
  if (std::abs(x - x_) > kLongPressSlopPx || std::abs(y - y_) > kLongPressSlopPx) { armed_ = false; return false; }
  if (uint32_t(ms - since_) < kLongPressMs) return false;
  armed_ = false;
  return true;
}
void LongPress::release() { armed_ = false; }
} // namespace gauge
