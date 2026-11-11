#include "LayoutChoice.h"
#include <cstdlib>
namespace gauge {
Layout nextLayout(Layout current) { return current == Layout::Classic ? Layout::Sweep : Layout::Classic; }
Layout decodeLayout(bool found, uint8_t stored) {
  return (found && stored == 1) ? Layout::Sweep : Layout::Classic;
}
uint8_t encodeLayout(Layout layout) { return layout == Layout::Sweep ? 1 : 0; }
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
