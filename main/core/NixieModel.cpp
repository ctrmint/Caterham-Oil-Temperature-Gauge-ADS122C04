#include "NixieModel.h"
#include <cmath>
namespace gauge {
namespace {
constexpr float kPi = 3.14159265f;
// The numerals in a 40 x 80 box, y down; arcs in degrees clockwise from 3 o'clock (270 is the top, 90 the bottom).
constexpr NixieStroke L(float x0, float y0, float x1, float y1) { return {false, x0, y0, x1, y1, 0.f, 0.f}; }
constexpr NixieStroke A(float cx, float cy, float rx, float ry, float a0, float a1) { return {true, cx, cy, rx, ry, a0, a1}; }
const NixieStroke k0[] = {A(20, 40, 16, 36, 0, 360)};
const NixieStroke k1[] = {L(20, 76, 20, 4), L(20, 4, 12, 13)};
const NixieStroke k2[] = {A(20, 20, 16, 16, 180, 400), L(32.26f, 30.28f, 4, 76), L(4, 76, 36, 76)};  // top bowl, to 40 deg
const NixieStroke k3[] = {L(6, 4, 34, 4), L(34, 4, 17.22f, 33.32f), A(20, 54, 16, 21, 260, 510)};
const NixieStroke k4[] = {L(28, 76, 28, 4), L(28, 4, 4, 56), L(4, 56, 36, 56)};
const NixieStroke k5[] = {L(34, 4, 9, 4), L(9, 4, 8.69f, 39.15f), A(20, 54, 16, 21, 225, 510)};
const NixieStroke k6[] = {A(20, 54, 16, 21, 0, 360), A(36, 54, 32, 48, 180, 250)};  // the loop, and the stem rising from it
const NixieStroke k7[] = {L(4, 4, 36, 4), L(36, 4, 14, 76)};
const NixieStroke k8[] = {A(20, 21, 13, 17, 90, 450), A(20, 57, 16, 19, 270, 630)};  // both loops start where they meet
const NixieStroke k9[] = {A(20, 26, 16, 21, 0, 360), A(4, 26, 32, 48, 0, 70)};  // the 6 turned half a turn
const NixieStroke kDash[] = {L(8, 42, 32, 42)};
const NixieStroke kLess[] = {L(32, 22, 8, 42), L(8, 42, 32, 62)};
const NixieStroke kMore[] = {L(8, 22, 32, 42), L(32, 42, 8, 62)};
template <int N> const NixieStroke *take(const NixieStroke (&s)[N], int &count) { count = N; return s; }
} // namespace

bool nixieShowable(char c) { return c == ' ' || (c >= '0' && c <= '9') || c == '-' || c == '<' || c == '>'; }

void nixieTubes(const char *cells, char (&out)[kNixieTubes]) {
  bool ok = cells != nullptr;
  for (int i = 0; ok && i < kNixieTubes; ++i) ok = nixieShowable(cells[i]);
  for (int i = 0; i < kNixieTubes; ++i) out[i] = ok ? cells[i] : (i + 2 >= kNixieTubes ? '-' : ' ');
}

const NixieStroke *nixieStrokes(char c, int &count) {
  switch (c) {
    case '0': return take(k0, count);
    case '1': return take(k1, count);
    case '2': return take(k2, count);
    case '3': return take(k3, count);
    case '4': return take(k4, count);
    case '5': return take(k5, count);
    case '6': return take(k6, count);
    case '7': return take(k7, count);
    case '8': return take(k8, count);
    case '9': return take(k9, count);
    case '-': return take(kDash, count);
    case '<': return take(kLess, count);
    case '>': return take(kMore, count);
    default: count = 0; return nullptr;
  }
}

int nixiePolyline(const NixieStroke &s, NixiePoint (&out)[kNixieMaxPoints]) {
  if (!s.arc) {
    out[0] = {s.x0, s.y0};
    out[1] = {s.x1, s.y1};
    return 2;
  }
  int steps = static_cast<int>(std::ceil((s.a1 - s.a0) / kNixieArcStepDeg - 1e-4f));
  if (steps < 1) steps = 1;
  if (steps > kNixieMaxPoints - 1) steps = kNixieMaxPoints - 1;
  for (int i = 0; i <= steps; ++i) {
    const float a = (s.a0 + (s.a1 - s.a0) * static_cast<float>(i) / static_cast<float>(steps)) * kPi / 180.f;
    out[i] = {s.x0 + s.x1 * std::cos(a), s.y0 + s.y1 * std::sin(a)};
  }
  return steps + 1;
}
} // namespace gauge
