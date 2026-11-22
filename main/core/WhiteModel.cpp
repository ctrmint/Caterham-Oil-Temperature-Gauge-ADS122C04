#include "WhiteModel.h"
#include <cmath>
namespace gauge {
TickSize whiteTick(int index) {
  if (index % 10 == 0) return TickSize::Major;
  if (index % 5 == 0) return TickSize::Medium;
  return TickSize::Minor;
}
int whiteNumberTick(int index) { return 10 * index; }
int whiteLabel(Unit unit, int index) {
  const float c = kWhiteMinC + kWhiteTickC * static_cast<float>(whiteNumberTick(index));
  return static_cast<int>(std::lround(toUnit(c, unit)));
}
NeedleState whiteNeedle(const Reading &r) { return needleOnScale(r, Unit::Celsius, kWhiteMinC, kWhiteMaxC); }
bool whiteWindow(const Reading &r, bool demo) { return !allIsWell(r, demo); }
} // namespace gauge
