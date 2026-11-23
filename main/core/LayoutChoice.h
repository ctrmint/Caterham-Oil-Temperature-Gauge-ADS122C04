#pragma once
#include <stdint.h>
namespace gauge {
// The stored value is the enum's value, so existing values never change meaning.
enum class Layout : uint8_t { Classic = 0, Sweep = 1, Needle = 2, Trace = 3, Tape = 4, Oem = 5, White = 6, Night = 7, Nixie = 8, Heritage = 9, Track = 10 };
constexpr uint32_t kLongPressMs = 1000;
constexpr int32_t kLongPressSlopPx = 40;
// Classic -> Sweep -> Needle -> Trace -> Tape -> OEM -> White -> Night -> Nixie -> Heritage -> Track -> Classic.
Layout nextLayout(Layout current);
// Stored value; anything missing or unknown is Classic.
Layout decodeLayout(bool found, uint8_t stored);
uint8_t encodeLayout(Layout layout);
// Long-press detector. hold() is true once per press, when the finger has stayed within the slop of the press point for kLongPressMs.
// A larger move cancels the press until release().
class LongPress {
 public:
  void press(int32_t x, int32_t y, uint32_t ms);
  bool hold(int32_t x, int32_t y, uint32_t ms);
  void release();
 private:
  bool armed_ = false;
  int32_t x_ = 0, y_ = 0;
  uint32_t since_ = 0;
};
} // namespace gauge
