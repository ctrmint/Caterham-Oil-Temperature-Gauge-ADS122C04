#include "OemModel.h"
#include <cmath>
namespace gauge {
int oemNumberStep(int index) { return 2 * (index + 1); }
int oemLabel(Unit unit, int index) {
  const float c = kOemMinC + (kOemMaxC - kOemMinC) * static_cast<float>(oemNumberStep(index)) / kOemSteps;
  return static_cast<int>(std::lround(toUnit(c, unit)));
}
NeedleState oemNeedle(const Reading &r) { return needleOnScale(r, Unit::Celsius, kOemMinC, kOemMaxC); }
bool oemWordmark(const Reading &r, bool demo) { return allIsWell(r, demo); }
} // namespace gauge
