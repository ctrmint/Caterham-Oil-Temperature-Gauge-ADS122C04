#include "HeritageModel.h"
#include <cmath>
namespace gauge {
TickSize heritageTick(int index) {
  if (index % 4 == 0) return TickSize::Major;
  if (index % 2 == 0) return TickSize::Medium;
  return TickSize::Minor;
}
int heritageNumberTick(int index) { return 8 * index; }
int heritageLabel(Unit unit, int index) {
  const float c = kHeritageMinC + kHeritageTickC * static_cast<float>(heritageNumberTick(index));
  return static_cast<int>(std::lround(toUnit(c, unit)));
}
NeedleState heritageNeedle(const Reading &r) { return needleOnScale(r, Unit::Celsius, kHeritageMinC, kHeritageMaxC); }
bool heritageLegend(const Reading &r, bool demo) { return allIsWell(r, demo); }
} // namespace gauge
