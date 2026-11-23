#include "NightModel.h"
namespace gauge {
bool nightQuiet(const Reading &r, bool demo) { return allIsWell(r, demo); }
uint8_t nightBeat(uint32_t frame) {
  constexpr uint32_t half = kNightBeatFrames / 2;
  static_assert(kNightBeatFrames % 2 == 0 && half > 0 && (0x100000000ULL % kNightBeatFrames) == 0,
                "an even period that divides 2^32, so the beat runs on across the frame counter's wrap");
  const uint32_t phase = frame % kNightBeatFrames;
  const uint32_t level = phase <= half ? phase : kNightBeatFrames - phase;  // 0 .. half .. 1
  return static_cast<uint8_t>(kNightBeatMinOpa + (kNightBeatMaxOpa - kNightBeatMinOpa) * level / half);
}
} // namespace gauge
